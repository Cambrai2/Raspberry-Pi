// owl.cpp : Defines the entry point for the console application.
/* Phil Culverhouse, James Rogers Oct 2016 (c) Plymouth University
 *
 * Uses IP sockets to communicate to the owl robot (see owl-comms.h)
 * Uses OpenCV to perform normalised cross correlation to find a match to a template
 * (see owl-cv.h).
 * PWM definitions for the owl servos are held in owl-pwm.h
 * includes bounds check definitions
 * requires setting for specific robot
 *
 * This demonstration programs does the following:
 * Implements a version of Itti & Kochs saliency model of saccadic stereo vision
 * 1. camera calibration is read from previous calibration data
 * 2. Main Loop - get stereo pair from camera stream
 * 3.   - correct for distorations using REMAP()
 * 4.   -
 *
 * First start communcations on Pi by running './OWLsocket'
 * Then run this program. The Pi server prints out [Rx Ry Lx Ly] pwm values and loops
 *
 *
 */

#define PI 3.14159265

#include <iostream>
#include <fstream>
#include <math.h>
#include <string>
#include <numeric>

#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "owl-pwm.h"
#include "owl-comms.h"
#include "owl-cv.h"

#include "opencv2/calib3d.hpp"
#include "opencv2/calib3d/calib3d.hpp"

#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <stdio.h>


#include <iostream> // for standard I/O
#include <string>   // for strings

#define PX2DEG  0.0768
#define DEG2PWM 10.730
#define IPD 58.3

#define xOffset  -0.3815
#define yOffset -54.6682
#define xScale   26.4842
#define yScale   91.4178

using namespace std;
using namespace cv;

// PFC/JR messy decls, but works under openCV 3.4
string ServoAbs( double DEGRx,double DEGRy,double DEGLx,double DEGLy,double DEGNeck);
string ServoRel(double DEGRx,double DEGRy,double DEGLx,double DEGLy,double DEGNeck);
string TrackCorrelTarget (OwlCorrel OWL);
int OwlCalCapture(cv::VideoCapture &cap, string Folder);


//void ServoAbs(float DEGRx,float DEGRy,float DEGLx,float DEGLy,float DEGNeck);
//void ServoRel(float DEGRx,float DEGRy,float DEGLx,float DEGLy,float DEGNeck);
Mat DoGFilter(Mat src, int k, int g);

static string PiADDR = "10.0.0.10";
static int PORT=12345;
static SOCKET u_sock = OwlCommsInit ( PORT, PiADDR);
static ostringstream CMDstream; // string packet
static string CMD;

static int Ry,Rx,Ly,Lx,Neck; // calculate values for position

//Default feature map weights
static int DoGLowWeight = 30 ; //DoG edge detection
//Custom feature map weights
static int SobelWeight = 10;
static int CannyWeight = 10;
static int HSLWeight = 10;
static int FamiliarWeight = 5  ; //Familiarity of the target, how much has the owl focused on this before
static int foveaWeight    = 50 ; //Distance from fovea (center)
static int CannyThreshhold = 35;
//
//
//
Size img_size = {640,480} ;
Mat map11, map12, map21, map22;
cv::Mat disp, disp8;

enum { STEREO_BM=0, STEREO_SGBM=1, STEREO_HH=2, STEREO_VAR=3, STEREO_3WAY=4 };
static void print_help()
{
    printf("\nDemo stereo matching converting L and R images into disparity and point clouds\n");
    printf("\nUsage: stereo_match <left_image> <right_image> [--algorithm=bm|sgbm|hh|sgbm3way] [--blocksize=<block_size>]\n"
           "[--max-disparity=<max_disparity>] [--scale=scale_factor>] [-i=<intrinsic_filename>] [-e=<extrinsic_filename>]\n"
           "[--no-display] [-o=<disparity_image>] [-p=<point_cloud_file>]\n");
}

static void saveXYZ(const char* filename, const Mat& mat)
{
    const double max_z = 1.0e4;
    FILE* fp = fopen(filename, "wt");
    for(int y = 0; y < mat.rows; y++)
    {
        for(int x = 0; x < mat.cols; x++)
        {
            Vec3f point = mat.at<Vec3f>(y, x);
            if(fabs(point[2] - max_z) < FLT_EPSILON || fabs(point[2]) > max_z) continue;
            fprintf(fp, "%f %f %f\n", point[0], point[1], point[2]);
        }
    }
    fclose(fp);
}

//
//Custom method to calculate the distance using Vergence calculations
double DistanceByVergence(){

    //the default distance between each camera (mm)
    double const baseLine = 65;

    //The scale between the motor servos and Degree angles in world space
    double const pwmScale = 0.08;
    //used to convert degrees to radians ( Pi / 180 )
    double const degtorad = 3.14159/180;

    //Get the current X position (servo value)
    double LangleX = LxC - Lx;
    LangleX = -LangleX;

    //Get the current Y position (servo value)
    double RangleX = RxC - Rx;
    RangleX = -RangleX;

    //Convert servo positions to world angles
    RangleX = RangleX  * pwmScale;
    LangleX = LangleX  * pwmScale;

    //Convert servo positions to world angles
    RangleX = RangleX  * degtorad;
    LangleX = LangleX  * degtorad;

    //get the distance to the target from the left camera
    double distanceL = ((baseLine * cos(RangleX))/sin(RangleX + LangleX));
    //Get the distance to the target from the right camera
    //double distanceR = ((baseLine * cos(LangleX))/sin(RangleX + LangleX));

    //Using trigonometry, a distance and angle for one camera and the base distance between
    //the cameras, we can calculate the distance from the center of the robots vision
    return sqrt((distanceL * distanceL) + ((65 * 65)/4) - (distanceL * 65 * sin(LangleX)));
}

//
//Custom method to calculate the distance from a target
//using disparity to calculate the average distance
//within a certain area on an image
float AvgCalculatedDistance(Mat image, Rect area){

    //creates an array of values to hold the pixel values
    vector<float> totalValues;

    //embedded foreach loops placing pixel values within the aformentioned array
    for (int x = area.x; x < area.x + area.width; x++){
        for (int y = area.y; y < area.y + area.height; y++){

            short pixelValue = image.at<short>(y,x);
            float disparityValue = pixelValue / 16.0f;

            if (pixelValue > 0)
                totalValues.push_back(disparityValue);

        }
    }

    //resets the average value between disparity calculations
    float average = 0;

    //makes sure the array is not empty before distance calculations
    if (totalValues.size() > 0)
        average = std::accumulate(totalValues.begin(), totalValues.end(), 0.0f) / totalValues.size();

    //multiplies the average by the rectified error value
    float calcDisparity = average * 0.005793f;

    //transposes distance from disparity value
    float calcDistance = 234/calcDisparity;

    //returns the calculated distance
    return calcDistance;
}


int main(int argc, char *argv[])
{

    //==========================================Initialize Variables=================================
    //Frame Size
    Size imageSize;
    imageSize.width=640;
    imageSize.height=480;

    //Local Feature Map  - implements FOVEA as a bias to the saliency map to central targets, rather than peripheral targets
    // eg. for a primate vision system
    Mat fovea=Mat(480, 640, CV_8U, double(0));
    fovea=Mat(480, 640, CV_8U, double(0));
    circle(fovea, Point(320,240), 150, 255, -1);
    cv::blur(fovea, fovea, Size(301,301));
    fovea.convertTo(fovea, CV_32FC1);
    fovea*=foveaWeight;

    //Initilize Mats
    const Mat OWLresult;// correlation result passed back from matchtemplate
    Mat Frame;
    Mat Left, Right, OWLtempl; // images
    Mat PanView=Mat(1600, 2500, CV_8UC3, Scalar(0,0,0)); //init as black - ie. no data
    Mat familiar=Mat(1600, 2500, CV_8U, double(255));

    //Video stream source
    string source ="http://10.0.0.10:8080/stream/video.mjpeg"; // was argv[1];           // the source file name

    //Set center neck positions
    //WE HAVE SET THESE VALUES TO WHAT WE BELIEVE
    //IS THE CENTER POSITIONS FOR THE EYES
    Rx = 1490;/*RxC*/ Lx = 1425;/*LxC*/
    Ry = 1400;/*RyC*/ Ly = 1555;/*LyC*/
    Neck= NeckC;

    //Variable declorations
    double minVal; double maxVal; Point minLoc; Point maxLoc;
    Point minLocTarget; Point maxLocTarget;


    //===================================Read left calibration data===================================
    const string LeftCalibrationFile = argc > 1 ? argv[1] : "../../Data/LeftCalibration.xml";
    FileStorage LeftFile(LeftCalibrationFile, FileStorage::READ); // Read the settings
    if (!LeftFile.isOpened())
    {
        cout << "Cannot open LeftCalibration.xml: \"" << LeftCalibrationFile << "\"" << endl;
        return -1;
    }
    Mat cameraMatrixL, distCoeffsL;
    LeftFile["Camera_Matrix"] >> cameraMatrixL;
    LeftFile["Distortion_Coefficients"] >> distCoeffsL;
    LeftFile.release();

    Mat map1L, map2L;
    initUndistortRectifyMap(cameraMatrixL, distCoeffsL, Mat(),
    cv::getOptimalNewCameraMatrix(cameraMatrixL, distCoeffsL, imageSize, 1, imageSize, nullptr),
    imageSize, CV_16SC2, map1L, map2L);

    //===================================Read Right calibration data===================================
    const string RightCalibrationFile = argc > 1 ? argv[1] : "../../Data/RightCalibration.xml";
    FileStorage RightFile(RightCalibrationFile, FileStorage::READ); // Read the settings
    if (!RightFile.isOpened())
    {
        cout << "Cannot open RightCalibration.xml: \"" << RightCalibrationFile << "\"" << endl;
        return -1;
    }
    Mat cameraMatrixR, distCoeffsR;
    RightFile["Camera_Matrix"] >> cameraMatrixR;
    RightFile["Distortion_Coefficients"] >> distCoeffsR;
    RightFile.release();

    Mat map1R, map2R;
    initUndistortRectifyMap(cameraMatrixR, distCoeffsR, Mat(),
    getOptimalNewCameraMatrix(cameraMatrixR, distCoeffsR, imageSize, 1, imageSize, nullptr),
    imageSize, CV_16SC2, map1R, map2R);

    //========================================Initialize Servos========================================
    CMDstream.str("");
    CMDstream.clear();
    CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
    CMD = CMDstream.str();
    string RxPacket= OwlSendPacket (u_sock, CMD.c_str());

    //========================================Open Video Steam========================================
    VideoCapture cap (source);
    if (!cap.isOpened()){
        cout  << "Could not open the input video: " << source << endl;
        return -1;
    }

    while (1){//Main processing loop
        //cout<<"Capture Frame"<<endl;
        //==========================================Capture Frame============================================
        for(int f=0;f<7;++f){
            if (!cap.grab()){
                cout  << "Could not open the input video: " << source << endl;
            }
        }
cap.retrieve(Frame);
        Mat FrameFlpd; cv::flip(Frame,FrameFlpd,1);     // Note that Left/Right are reversed now
        // Split into LEFT and RIGHT images from the stereo pair sent as one MJPEG iamge
        Left= FrameFlpd( Rect(0, 0, 640, 480));         // using a rectangle
        remap(Left, Left, map1L, map2L, INTER_LINEAR);  //Apply camera calibration
        Right=FrameFlpd( Rect(640, 0, 640, 480));       // using a rectangle
        remap(Right, Right, map1R, map2R, INTER_LINEAR);//Apply camera calibration
        Mat LeftGrey;                                   //Make a grey copy of Left
        cvtColor(Left, LeftGrey, COLOR_BGR2GRAY);


        // ======================================CALCULATE FEATURE MAPS ====================================
        //===========================DoG low bandpass Map========================================
        Mat DoGLow = DoGFilter(LeftGrey,5,127);
        Mat DoGLow8;
        normalize(DoGLow, DoGLow8, 0, 255, CV_MINMAX, CV_8U);
        imshow("DoG Low", DoGLow8);

        //===========================Sobel filter================================================
        //Initialised required materials for Sobel calculations
        Mat SobelBase, grad_x, grad_y, abs_grad_x, abs_grad_y, Sobel1;

        //Reduce noise of greyscale image
        GaussianBlur(LeftGrey, SobelBase, Size(3,3), 0, 0, BORDER_DEFAULT);

        //Calculates the derivates in the x and y directions
        Sobel(SobelBase, grad_x, CV_16S, 1, 0, 3, 1, 0, BORDER_DEFAULT);
        Sobel(SobelBase, grad_y, CV_16S, 0, 1, 3, 1, 0, BORDER_DEFAULT);

        //Convert partial results back to CV_8U
        convertScaleAbs( grad_x, abs_grad_x);
        convertScaleAbs( grad_y, abs_grad_y);

        //Combined both directional gradients into one material and displayed end result
        addWeighted( abs_grad_x, 0.5, abs_grad_y, 0.5, 0, Sobel1);
        imshow("Sobel Filter", Sobel1);

        //============================Canny filter==================================================

        //Initialised required materials for Canny calculations
        Mat dst, detected_edges;

        //define the ratio and kernel size
        int ratio = 3;
        int kernel_size = 3;


        dst.create( Left.size(), Left.type() );

        // Reduce noise with a kernel 3x3
        GaussianBlur(LeftGrey, detected_edges, Size(3,3), 0, 0, BORDER_DEFAULT);

        // Canny detector
        Canny( detected_edges, detected_edges, CannyThreshhold, CannyThreshhold*ratio, kernel_size );

        // Using Canny's output as a mask
        dst = Scalar::all(0);

        //copy over end result overlay and display result with detected edges
        LeftGrey.copyTo( dst, detected_edges);
        imshow( "Canny Filter", dst );

        //=================================HSL map==============================================

        // NOTE : HSL and HLS are the same, however openCV method COLOR_BGR2HLS is acronymed incorrectly.

        //Initialised required materials for HSL filter map
        Mat hsl_channels[3], SalienceHSL, finalHSL;

        //Convert camera image to HLS format
        cvtColor(Left, SalienceHSL, COLOR_BGR2HLS);

        //Split the converted image in to the individual channels
        cv::split( SalienceHSL, hsl_channels);

        //Copy the first channel (Hue) to a new variable to display
        hsl_channels[0].copyTo(finalHSL);
        imshow("Hue Saturation Lightness", finalHSL);



        //=====================================Initialise Global Position====================================
        //cout<<"Globe Pos"<<endl;
        Point GlobalPos;    // Position of camera view within the range of movement of the OWL
        GlobalPos.x=static_cast<int>(900+((-(Neck-NeckC)+(Lx-LxC))/DEG2PWM)/PX2DEG);
        GlobalPos.y=static_cast<int>(500+((Ly-LyC)/DEG2PWM)/PX2DEG);

        Mat familiarLocal=familiar(Rect(GlobalPos.x,GlobalPos.y,Left.cols,Left.rows));
        //imshow("familiarLocal",familiarLocal);

        //====================================Combine maps into saliency map=====================================
        //cout<<"Salience"<<endl;


        //Convert all 8-bit Mat's to 32bit floating point's
        DoGLow.convertTo(DoGLow, CV_32FC1);
        DoGLow*=DoGLowWeight;

        dst.convertTo(dst, CV_32FC1);
        dst*=CannyWeight;

        Sobel1.convertTo(Sobel1, CV_32FC1);
        Sobel1*=SobelWeight;

//        finalHSL.convertTo(finalHSL, CV_32FC1);
//        finalHSL*=HSLWeight;


        familiarLocal.convertTo(familiarLocal, CV_32FC1);

        // Linear combination of feature maps to create a salience map
        Mat Salience=cv::Mat(Left.size(),CV_32FC1,0.0); // init map

        //Add each of the feature maps to the salience map image
        add(Salience,DoGLow,Salience);
        add(Salience,fovea,Salience);
        //add(Salience,finalHSL,Salience);
        add(Salience,Sobel1,Salience);
        add(Salience,dst,Salience);

        Salience=Salience.mul(familiarLocal);
        normalize(Salience, Salience, 0, 255, CV_MINMAX, CV_32FC1);

        //imshow("SalienceNew",Salience);

        //=====================================Find & Move to Most Salient Target=========================================

        minMaxLoc(Salience, &minVal, &maxVal, &minLoc, &maxLoc, Mat() );
        // Calculate relative servo correction and magnitude of correction
        double xDifference = static_cast<double>((maxLoc.x-320)*PX2DEG);
        double yDifference = static_cast<double>((maxLoc.y-240)*PX2DEG);

        rectangle(Left,Point(maxLoc.x-32,maxLoc.y-32),Point(maxLoc.x+32,maxLoc.y+32),Scalar::all(255),2,8,0); //draw rectangle on most salient area
        // Move left eye based on salience, move right eye to be parallel with left eye
        ServoRel(((Lx-LxC+RxC-Rx)/DEG2PWM)+xDifference*1,-((LyC-Ly+RyC-Ry)/DEG2PWM)+yDifference*1,xDifference*1,yDifference*1,(Lx-LxC)/100);

        double distance = DistanceByVergence();
        //double disparityDistance = DistanceByDisparityInLoop(bm, sgbm, cap, Frame, source, Left, Right, map11, map12, map21, map22, numberOfDisparities, SADWindowSize, roi1, roi2, alg, disp);
        cout << "  Vergence Distance = " << distance;

        // Update Familarity Map //
        // Familiar map to inhibit salient targets once observed (this is a global map)
        double longitude=(((Ly-LyC)/DEG2PWM)+maxLoc.y*PX2DEG);//calculate longitude as the global map is a spherical projection
        // ensure dwell time at perimeter of map is similar to that at centre.
        longitude*=2.5; //amplify the projection correction
        if(longitude>70){
            longitude=70;
        }

        Mat familiarNew=familiar.clone();
        circle(familiarNew, GlobalPos+maxLoc, static_cast<int>(60/cos(longitude*PI/180)), 0, -1);
        cv::blur(familiarNew, familiarNew, Size(151,151)); //Blur used to save on processing
        normalize(familiarNew, familiarNew, 0, 255, CV_MINMAX, CV_8U);
        addWeighted(familiarNew, (static_cast<double>(FamiliarWeight)/100), familiar, (100-static_cast<double>(FamiliarWeight))/100, 0, familiar);

        Mat familiarSmall;
        resize(familiar,familiarSmall,familiar.size()/4);
        imshow("Familiar",familiarSmall);
        //imshow("Right",Right);

        //=================================Convert Saliency into Heat Map=====================================

        Mat SalienceHSVnorm;
        Salience.convertTo(Salience, CV_8UC1);
        normalize(Salience, SalienceHSVnorm, 130, 255, CV_MINMAX, CV_8U);
        normalize(Salience, Salience, 0, 255, CV_MINMAX, CV_8U);
        Mat SalienceHSV;
        cvtColor(Left, SalienceHSV, COLOR_BGR2HSV);

        for(int y=0;y<480;y++){
            for(int x=0; x<640;x++){
                SalienceHSV.at<Vec3b>(y,x)=Vec3b(255-SalienceHSVnorm.at<uchar>(y,x),255,255);
            }
        }
        cvtColor(SalienceHSV, SalienceHSV, COLOR_HSV2BGR);

        //=======================================Update Global View===========================================
        //cout<<"Global View"<<endl;
        if(GlobalPos!=Point(0,0)){
            Mat LeftCrop = Left(Rect(220,140,200,200));//image cropped to minimize image stitching artifacts
            LeftCrop.copyTo(PanView(Rect(GlobalPos.x,GlobalPos.y,LeftCrop.cols,LeftCrop.rows)));
            Mat PanViewSmall;
            resize(PanView,PanViewSmall,PanView.size()/2);
            imshow("PanView",PanViewSmall);
        }

        resize(Left,Left,Left.size()/2);

        //Display the distance by Vergence calculation on the original camera image
        cv::putText(Left,("Distance in Square (mm): " + to_string((int)distance)), cvPoint(30,30), FONT_HERSHEY_COMPLEX_SMALL, 0.7, cv::Scalar(255,255,255), 1, LINE_8, false);
        imshow("Left",Left);

        resize(SalienceHSV,SalienceHSV,SalienceHSV.size()/2);
        imshow("SalienceHSV",SalienceHSV);

        //=========================================Control Window for feature weights =============================================
        //cout<<"Control Window"<<endl;

        //Set up a 'Control' window
        namedWindow("Control", CV_WINDOW_AUTOSIZE);

        //Add a track bar to control the level of input from
        //each of the feature maps to the salience map
        cvCreateTrackbar("LowFreq", "Control",&DoGLowWeight,100);
        cvCreateTrackbar("Familiar", "Control",&FamiliarWeight,100);
        cvCreateTrackbar("fovea", "Control",&foveaWeight,100);
        cvCreateTrackbar("HSL","Control", &HSLWeight,100);
        cvCreateTrackbar("Sobel","Control", &SobelWeight,100);
        cvCreateTrackbar("Canny-W","Control", &CannyWeight,100);
        cvCreateTrackbar("Canny-T", "Control", &CannyThreshhold, 100);

        resizeWindow("Control", 400,100);
        waitKey(10);
    }
}

//====================================================================================//
// SERVO FUNCTIONS

string ServoAbs( double DEGRx,double DEGRy,double DEGLx,double DEGLy,double DEGNeck){
    int Rx,Ry,Lx,Ly, Neck;
    //    Rx=static_cast<int>(DEGRx*DEG2PWM)+RxC;
    //    Ry=static_cast<int>(-DEGRy*DEG2PWM)+RyC;
    //    Lx=static_cast<int>(DEGLx*DEG2PWM)+LxC;
    //    Ly=static_cast<int>(DEGLy*DEG2PWM)+LyC;
    //    Neck=static_cast<int>(-DEGNeck*DEG2PWM)+NeckC;
    Rx=static_cast<int>(DEGRx*DEG2PWM);
    Ry=static_cast<int>(DEGRy*DEG2PWM);
    Lx=static_cast<int>(DEGLx*DEG2PWM);
    Ly=static_cast<int>(DEGLy*DEG2PWM);
    Neck=static_cast<int>(DEGNeck*DEG2PWM);

    if(Rx>RxRm){
        Rx=RxRm;
    }else if(Rx<RxLm){
        Rx=RxLm;
    }

    if(Lx>LxRm){
        Lx=LxRm;
    }else if(Lx<LxLm){
        Lx=LxLm;
    }

    if(Ry>RyTm){
        Ry=RyTm;
    }else if(Ry<RyBm){
        Ry=RyBm;
    }

    if(Ly>LyBm){
        Ly=LyBm;
    }else if(Ly<LyTm){
        Ly=LyTm;
    }

    if(Neck>NeckL){
        Neck=NeckL;
    }else if(Neck<NeckR){
        Neck=NeckR;
    }

    CMDstream.str("");
    CMDstream.clear();
    CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
    string CMD = CMDstream.str();
    string retSTR = OwlSendPacket (u_sock, CMD.c_str());
    return (retSTR);
}

string ServoRel(double DEGRx,double DEGRy,double DEGLx,double DEGLy,double DEGNeck){
    //int Rx,Ry,Lx,Ly, Neck;
    Rx=static_cast<int>(DEGRx*DEG2PWM)+Rx;
    Ry=static_cast<int>(-DEGRy*DEG2PWM)+Ry;
    Lx=static_cast<int>(DEGLx*DEG2PWM)+Lx;
    Ly=static_cast<int>(DEGLy*DEG2PWM)+Ly;
    Neck=static_cast<int>(-DEGNeck*DEG2PWM)+Neck;

    if(Rx>RxRm - 50){
        Rx=RxRm - 50;
    }else if(Rx<RxLm + 50){
        Rx=RxLm + 50;
    }

    if(Lx>LxRm - 50){
        Lx=LxRm - 50;
    }else if(Lx<LxLm + 50){
        Lx=LxLm + 50;
    }

    if(Ry>RyTm - 50){
        Ry=RyTm - 50;
    }else if(Ry<RyBm + 50){
        Ry=RyBm + 50;
    }

    if(Ly>LyBm - 50){
        Ly=LyBm - 50;
    }else if(Ly<LyTm + 50){
        Ly=LyTm + 50;
    }

    if(Neck>NeckL - 50){
        Neck=NeckL - 50;
    }else if(Neck<NeckR + 50){
        Neck=NeckR + 50;
    }

    CMDstream.str("");
    CMDstream.clear();
    CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
    string CMD = CMDstream.str();
    string retSTR=OwlSendPacket (u_sock, CMD.c_str());
    return(retSTR);
}

string TrackCorrelTarget (OwlCorrel OWL){
    ostringstream CMDstream;
    string CMD, RxPacket;

    // Only for left eye at the moment
    //** P control set track rate to 10% of destination PWMs to avoid ringing in eye servo
    double KPx=0.1; // track rate X
    double KPy=0.1; // track rate Y
    double LxScaleV = LxRangeV/static_cast<double>(640); //PWM range /pixel range
    double Xoff= 320-(OWL.Match.x + OWLtempl.cols/2)/LxScaleV ; // compare to centre of image
    double LxOld=Lx;
    Lx=static_cast<int>(LxOld-Xoff*KPx); // roughly 300 servo offset = 320 [pixel offset]

    double LyScaleV = LyRangeV/static_cast<double>(480); //PWM range /pixel range
    double Yoff= (240+(OWL.Match.y + OWLtempl.rows/2)/LyScaleV)*KPy ; // compare to centre of image
    double LyOld=Ly;
    Ly=static_cast<int>(Yoff-LyOld); // roughly 300 servo offset = 320 [pixel offset]

    //cout << owl::Lx << " " << Xoff << " " << LxOld << endl; // DEBUG PFC
    //cout << owl::Ly << " " << Yoff << " " << LyOld << endl;

    //** ACTION
    // move to get minimise distance from centre of both images, ie verge in to target
    // move servos to position
    string retSTR=ServoAbs( ((RxC-Rx)/DEG2PWM), //Rx
             ((RyC-Ry)/DEG2PWM), // Ry -- the right eye Y servo has inverted direction compared to left.
             ((LxC-Lx)/DEG2PWM),// Lx
             ((LyC-Ly)/DEG2PWM), // Ly
             NeckC/DEG2PWM); // NECK .. no neck motion as yet
    return(retSTR);

}



// create DoG bandpass filter, with g being odd always and above 91 for low pass, and >9 for high pass
// k is normally 3 or 5
Mat DoGFilter(Mat src, int k, int g){
    Mat srcC;
    src.convertTo(srcC,CV_32FC1);
    Mat g1, g2;
    GaussianBlur(srcC, g1, Size(g,g), 0);
    GaussianBlur(srcC, g2, Size(g*k,g*k), 0);
    srcC = (g1 - g2)*2;
    return srcC;

}


















