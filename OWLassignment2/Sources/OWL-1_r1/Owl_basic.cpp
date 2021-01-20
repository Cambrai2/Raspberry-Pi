// owl.cpp : Defines the entry point for the console application.
/* Phil Culverhouse Oct 2016 (c) Plymouth UNiversity
 *
 * Uses IP sockets to communicate to the owl robot (see owl-comms.h)
 * Uses OpenCV to perform normalised cross correlation to find a match to a template
 * (see owl-cv.h).
 * PWM definitions for the owl servos are held in owl-pwm.h
 * includes bounds check definitions
 * requires setting for specific robot
 *
 * This demosntration programs does the following:
 * a) loop 1 - take picture, check arrow keys
 *             move servos +5 pwm units for each loop
 *             draw 64x64 pixel square overlaid on Right image
 *             if 'c' is pressed copy patch into a template for matching with left
 *              exit loop 1;
 * b) loop 2 - perform Normalised Cross Correlation between template and left image
 *             move Left eye to centre on best match with template
 *             (treats Right eye are dominate in this example).
 *             loop
 *             on exit by ESC key
 *                  go back to loop 1
 *
 * First start communcations on Pi by running 'python PFCpacket.py'
 * Then run this program. The Pi server prints out [Rx Ry Lx Ly] pwm values and loops
 *
 * NOTE: this program is just a demonstrator, the right eye does not track, just the left.
 */

#include <iostream>
#include <fstream>
#include <stdio.h>

#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "owl-pwm.h"
#include "owl-comms.h"
#include "owl-cv.h"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/opencv.hpp"

#include <iostream> // for standard I/O
#include <string>   // for strings
#include <math.h>

using namespace std;
using namespace cv;

int main(int argc, char *argv[])
{
    int asdf = 0;
    char receivedStr[1024];
    ostringstream CMDstream; // string packet
    string CMD;

    double LangleX;

    double RangleX;

    double VergeAngle;

    double const baseLine = 65;

    double distance;
    double distanceL;
    double distanceR;

    Rx = RxLm; Lx = LxLm;
    Ry = RyC; Ly = LyC;
    Neck= NeckC;

    string source ="http://10.0.0.10:8080/stream/video.mjpeg"; // was argv[1];           // the source file name
    string PiADDR = "10.0.0.10";

    //SETUP TCP COMMS
    int PORT=12345;
    SOCKET u_sock = OwlCommsInit ( PORT, PiADDR);

    /***********************
 * LOOP continuously for testing
 */
    // RyC=RyC-40; LyC=LyC+40; // offset for cross on card
    //WE HAVE SET THESE VALUES TO WHAT WE BELIEVE
    //IS THE CENTER POSITIONS FOR THE EYES
    Rx = 1490; Lx = 1425;
    Ry = 1400; Ly = 1555;
    Neck= NeckC;

    const Mat OWLresult;// correlation result passed back from matchtemplate
    cv::Mat Frame;
    Mat Left, Right; // images
    bool inLOOP=true; // run through cursor control first, capture a target then exit loop

    while (inLOOP){
        // move servos to centre of field
        CMDstream.str("");
        CMDstream.clear();
        CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
        CMD = CMDstream.str();
        string RxPacket= OwlSendPacket (u_sock, CMD.c_str());

        VideoCapture cap (source);              // Open input
        if (!cap.isOpened())
        {
            cout  << "Could not open the input video: " << source << endl;
            return -1;
        }
        //Rect region_of_interest = Rect(x, y, w, h);
        while (inLOOP){
            if (!cap.read(Frame))
            {
                cout  << "Could not open the input video: " << source << endl;
                //         break;
            }
            Mat FrameFlpd; cv::flip(Frame,FrameFlpd,1); // Note that Left/Right are reversed now
            //Mat Gray; cv::cvtColor(Frame, Gray, cv::COLOR_BGR2GRAY);
            // Split into LEFT and RIGHT images from the stereo pair sent as one MJPEG iamge
            Left= FrameFlpd( Rect(0, 0, 640, 480)); // using a rectangle
            Right=FrameFlpd( Rect(640, 0, 640, 480)); // using a rectangle
            Mat RightCopy;
            Right.copyTo(RightCopy);
            //rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
            //circle( Left, Point(320, 240), 10, Scalar::all(255), 2, 8, 0);
            //circle( RightCopy, Point(320, 240), 10, Scalar::all(255), 2, 8, 0);
            imshow("Left",Left);imshow("Right", RightCopy);

            int key = waitKey(30); // this is a pause long enough to allow a stable photo to be taken.
            printf("%d",key);//mrs added 01/02/2017 to diagnose arrow keys returned code ***************************************************
            switch (key){
            case 'w': //up
                Ry=Ry+5; // was Ly=+5 Changed BILL
                cout << "     " << Lx;
                break;
            case 's'://down
                Ry=Ry-5; // was Ly=-5 BILL
                cout << "     " << Lx;
                break;
            case 'i': //up
                Ly=Ly-5; // was Ly=+5 Changed BILL
                cout << "     " << Lx;
                break;
            case 'k'://down
                Ly=Ly+5; // was Ly=-5 BILL
                cout << "     " << Lx;
                break;
            case 'a'://left
                Rx=Rx-5;

                cout << "     " << Rx;

                break;
            case 'd'://right
                Rx=Rx+5;
                cout << "     " << Rx;

//                LangleX = Lx - LxC;
//                LangleXin = 90 - (LangleX * 0.113);
//                cout << "     " << LangleXin << "     ";

                break;
            case 'c': // lowercase 'c'
                OWLtempl= Right(target);
                imshow("templ",OWLtempl);
                waitKey(1);
                inLOOP=false; // quit loop and start tracking target
                break; // left
            case 'j':
                Lx=Lx-5;
                cout << "      " << Lx;
                break;
            case 'l':
                Lx=Lx+5;
                cout << "      " << Lx;
                break;
            case ' ': //Space
                asdf++;
                cv::imwrite("C:/Users/sjones20/Desktop/OWLHOOT/Data/CalTest/right" + std::to_string(asdf) + ".jpg", RightCopy);
                cv::imwrite("C:/Users/sjones20/Desktop/OWLHOOT/Data/CalTest/left" + std::to_string(asdf) + ".jpg", Left);
                break;
            case 'g':
                cout << "Lx: " << Lx << " Rx: " << Rx << " Ly: " << Ly << " Ry: " << Ry << "     ";
                break;
            default:
                key=key;
                //nothing at present
            }

            CMDstream.str("");
            CMDstream.clear();
            CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
            CMD = CMDstream.str();
            RxPacket= OwlSendPacket (u_sock, CMD.c_str());

        } // END cursor control loop

        // close windows down
        destroyAllWindows();

        //============= Normalised Cross Correlation ==========================
        // right is the template, just captured manually
        inLOOP=true; // run through the loop until decided to exit
        while (inLOOP) {
            if (!cap.read(Frame))
            {
                cout  << "Could not open the input video: " << source << endl;
                break;
            }
            Mat FrameFlpd; cv::flip(Frame,FrameFlpd,1); // Note that Left/Right are reversed now
            //Mat Gray; cv::cvtColor(Frame, Gray, cv::COLOR_BGR2GRAY);
            // Split into LEFT and RIGHT images from the stereo pair sent as one MJPEG iamge
            Left= FrameFlpd( Rect(0, 0, 640, 480)); // using a rectangle
            Right=FrameFlpd( Rect(640, 0, 640, 480)); // using a rectangle

            //Rect target= Rect(320-32, 240-32, 64, 64); //defined in owl-cv.h
            //Mat OWLtempl(Right, target);
            OwlCorrel OWL;
            OWL = Owl_matchTemplate( Right,  Left, OWLtempl, target);

            //OwlCorrelRight OWLright;
            //OWLright = Owl_matchTemplateRight(Right, Left, OWLtempl, target);

            /// Show me what you got
            Mat RightCopy;
            Right.copyTo(RightCopy);

            //rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 );
            //rectangle( Left, OWL.MatchLeft, Point( OWL.MatchLeft.x + OWLtempl.cols , OWL.MatchLeft.y + OWLtempl.rows), Scalar::all(255), 2, 8, 0 );
            //rectangle( RightCopy, OWL.MatchRight, Point( OWL.MatchRight.x + OWLtempl.cols , OWL.MatchRight.y + OWLtempl.rows), Scalar::all(255), 2, 8, 0 );

            //rectangle( OWL.ResultLeft, OWL.MatchLeft, Point( OWL.MatchLeft.x + OWLtempl.cols , OWL.MatchLeft.y + OWLtempl.rows), Scalar::all(255), 2, 8, 0 );
            //rectangle( OWL.ResultRight, OWL.MatchRight, Point( OWL.MatchRight.x + OWLtempl.cols , OWL.MatchRight.y + OWLtempl.rows), Scalar::all(255), 2, 8, 0 );

            imshow("Owl-L", Left);
            imshow("Owl-R", RightCopy);
            imshow("Correl",OWL.ResultLeft );
            imshow("correlR", OWL.ResultRight );
            if (waitKey(10)== 27) inLOOP=false;
            //// P control

            // Only for left eye at the moment
            //** P control set track rate to 10% of destination PWMs to avoid ringing in eye servo
            //======== try altering KPx & KPy to see the settling time/overshoot
            double KPx=0.05; // track rate X
            double KPy=0.05; // track rate Y

            double LxScaleV = LxRangeV/static_cast<double>(640); //PWM range /pixel range
            double Xoff= 320-(OWL.MatchLeft.x + OWLtempl.cols/2)/LxScaleV ; // compare to centre of image
            double LxOld=Lx;
            Lx=static_cast<int>(LxOld-Xoff*KPx); // roughly 300 servo offset = 320 [pixel offset]

            double LyScaleV = LyRangeV/static_cast<double>(480); //PWM range /pixel range
            double Yoff= (240+(OWL.MatchLeft.y + OWLtempl.rows/2)/LyScaleV)*KPy ; // compare to centre of image
            double LyOld=Ly;
            Ly=static_cast<int>(LyOld-Yoff); // roughly 300 servo offset = 320 [pixel offset]

            double RxScaleV = RxRangeV/static_cast<double>(640); //PWM range /pixel range
            Xoff= 320-(OWL.MatchRight.x + OWLtempl.cols/2)/RxScaleV ; // compare to centre of image
            double RxOld=Rx;
            Rx=static_cast<int>(RxOld-Xoff*KPx); // roughly 300 servo offset = 320 [pixel offset]

            double RyScaleV = (-RyRangeV)/static_cast<double>(480); //PWM range /pixel range
            Yoff= (240+(OWL.MatchRight.y + OWLtempl.rows/2)/RyScaleV)*KPy ; // compare to centre of image
            double RyOld=Ry;
            Ry=static_cast<int>(RyOld+Yoff); // roughly 300 servo offset = 320 [pixel offset]

            //cout << Lx << Ly << Rx <<
            Ry;


            double const pwmScale = 0.08;
            double const degtorad = 3.14159/180;
            //lxc = 1470
            LangleX = LxC - Lx;
//            if (LangleX < 0){
                LangleX = -LangleX;
//            }
//            LangleXin = 90 - (LangleX * pwmScale);

            //rxc = 1445
            RangleX = RxC - Rx;
//            if (RangleX < 0){
                RangleX = -RangleX;
//            }
//            RangleXin = 90 - (RangleX * pwmScale);

            //VergeAngle = 180 - RangleXin - LangleXin;

            RangleX = RangleX  * pwmScale;
            LangleX = LangleX  * pwmScale;

            cout << LangleX << "     " << RangleX << "     ";

            RangleX = RangleX  * degtorad;
            LangleX = LangleX  * degtorad;

            //above is correct

            distanceL = ((baseLine * cos(RangleX))/sin(RangleX + LangleX));
            distanceR = ((baseLine * cos(LangleX))/sin(RangleX + LangleX));
            distance = sqrt((distanceL * distanceL) + ((65 * 65)/4) - (distanceL * 65 * sin(LangleX)));
            cout << Lx << "     " << Rx << "     " << LangleX << "      " << RangleX << "   " << VergeAngle << "     " << distanceL << "    " << distanceR << "    " << distance << "     ";
//            double distance1 = (sin(RangleXin)* distanceR);
//            double distance2 = (sin(LangleXin)* distanceL);
//            distance = (distance1 + distance2)/2;
//            cout << distance;

            // move to get minimise distance from centre of both images, ie verge in to targe
            // move servos to position
            CMDstream.str("");
            CMDstream.clear();
            CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
            CMD = CMDstream.str();
#ifdef _WIN32
            RxPacket= OwlSendPacket (u_sock, CMD.c_str());
#else
            OwlSendPacket (clientSock, CMD.c_str());
#endif


        } // end if ZMCC
    } // end while outer loop
#ifdef _WIN32
    closesocket(u_sock);
#else
    close(clientSock);
#endif

    exit(0); // exit here for servo testing only

}
