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
 * These demonstration programs do the following:
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

#include <sys/types.h>
//#include <unistd.h>

#include "owl-pwm.h"
#include "owl-comms.h"
#include "owl-cv.h"

#include <iostream> // for standard I/O
#include <string>   // for strings

#include <stdlib.h>
#include <cmath>

using namespace std;
using namespace cv;



int main(int argc, char *argv[])
{
    char receivedStr[1024];
    ostringstream CMDstream; // string packet
    string CMD;
    int N;
    int dValue;
    int mValue;
    int aValue;
    int randValue;
    int desLXVal;
    int desLYVal;
    int desRXVal;
    int desRYVal;
    int ChangeRX;
    int ChangeRY;
    int ChangeLX;
    int ChangeLY;
    Rx = RxLm; Lx = LxLm;
    Ry = RyC; Ly = LyC;
    Neck= NeckC;

    string source ="http://10.0.0.10:8080/stream/video.mjpeg"; // was argv[1];           // the source file name
    string PiADDR = "10.0.0.10";

    //SETUP TCP COMMS
    int PORT=12345;
    SOCKET u_sock = OwlCommsInit ( PORT, PiADDR);

    bool humanEye = false;
    bool chameleonEye = false;
    bool eyeMoveLeft = false;
    bool eyeRoll = false;
    bool eyeFear = false;
    bool neckMovement = false;
    bool neckMoveLeft = false;

    enum stepValue {step1, step2, step3, step4, step5, step6};

    /***********************
 * LOOP continuously for testing
 */
    // RyC=RyC-40; LyC=LyC+40; // offset for cross on card
    Rx = RxC; Lx = LxC;
    Ry = RyC; Ly = LyC;
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
            rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
            imshow("Left",Left);imshow("Right", RightCopy);
            waitKey(10); // display the images

            int key = waitKey(100); // this is a pause long enough to allow a stable photo to be taken.

            int change = 15;
            switch (key){
            case'w': //up arrow
                Ry=Ry+change;Ly=Ly-change;
                break;
            case's': //down arrow
                Ry=Ry-change;Ly=Ly+change;
                break;
            case'a': //left arrow
                Rx=Rx-change;Lx=Lx-change;
                break;
            case'd': // right arrow
                Rx=Rx+change;Lx=Lx+change;
                break;
            case'j': //anticlockwise neck movement
                dValue = Neck/90;
                mValue = 17 - dValue;
                if (mValue < 0){
                    mValue = -mValue;
                }
                aValue = 10 - mValue;
                if (aValue < 0){
                    aValue = -aValue;
                }
                Neck = Neck + (aValue * 2);
                cout << Neck << " " << (aValue * 2) << " ";
                break;
            case'l': //clockwise neck movement
                dValue = Neck/90;
                mValue = 17 - dValue;
                if (mValue < 0){
                    mValue = -mValue;
                }
                aValue = 10 - mValue;
                if (aValue < 0){
                    aValue = -aValue;
                }
                Neck = Neck - (aValue * 2);
                cout << Neck << " " << (aValue * 2) << " ";
                break;
            case 'c': // lowercase 'c'
                OWLtempl= Right(target);
                imshow("templ",OWLtempl);
                waitKey(1);
                inLOOP=false; // quit loop and start tracking target
                break; // left
            case '1':
                // Human eye start
                // Steady, side to side eye movement
                Rx = RxC;
                Lx = LxC;
                Ry = RyC;
                Ly = LyC;

                humanEye = true;
                break;
            case '2':
                // Chameleon eye start
                // Completely random movement
                Rx = RxC;
                Lx = LxC;
                Ry = RyC;
                Ly = LyC;

                chameleonEye = true;
                break;
            case '3':
                // Eye roll start
                // Action of human rolling eyes around
                Rx = RxC;
                Lx = LxC;
                Ry = RyC;
                Ly = LyC;

                eyeRoll = true;
                break;
            case '4':
                // Eye fear start
                // Rapid, side to side eye movement
                Rx = RxC;
                Lx = LxC;
                Ry = RyC;
                Ly = LyC;
                Neck = NeckC;

                eyeMoveLeft = true;
                eyeFear = true;
                break;
            case '5':
                // Sinusoidal neck start
                neckMovement = true;
                break;
            default:
                key=key;
                //nothing at present
            }

            // Automated human eye movement (horizontal)
            while(humanEye){

                // Movement speed
                int speed = 15;

                // Move the eyes left or right
                // based on their current position
                if (Rx < 1250 && Lx < 1250){
                    eyeMoveLeft = true;
                }
                if (Rx > 1800 && Lx > 1800){
                    eyeMoveLeft = false;
                }

                if (Rx < 1250 && Lx < 1250){
                    Rx = Rx + speed;
                    Lx = Lx + speed;
                }
                if (Rx > 1800 && Lx > 1800){
                    Rx = Rx - speed;
                    Lx = Lx - speed;
                }

                CMDstream.str("");
                CMDstream.clear();
                CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
                CMD = CMDstream.str();
                RxPacket= OwlSendPacket (u_sock, CMD.c_str());

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
                rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
                imshow("Left",Left);imshow("Right", RightCopy);
                waitKey(10); // display the images

                int key = waitKey(100);
                if (key == '1'){
                    // Turn off automation
                    humanEye = false;
                }
            }

            // Automated chameleon eye movement (random)
            while(chameleonEye == true){

                randValue = rand() % 40;

                // Move a random eye to a random position
                if (randValue == 0){
                    // Move left eye

                    Lx = rand() % 670 + 1180;
                    Ly = rand() % 820 + 1180;
                }
                if (randValue == 1){
                    // Move right eye

                    Rx = rand() % 690 + 1200;
                    Ry = rand() % 880 + 1120;
                }
                if (randValue == 2){
                    // Move both eyes

                    Lx = rand() % 670 + 1180;
                    Ly = rand() % 820 + 1180;
                    Rx = rand() % 690 + 1200;
                    Ry = rand() % 880 + 1120;
                }

                cout << "Left eye Values = " << Lx << " " << Ly;
                cout << "Right eye Values = " << Rx << " " << Ry;

                CMDstream.str("");
                CMDstream.clear();
                CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
                CMD = CMDstream.str();
                RxPacket= OwlSendPacket (u_sock, CMD.c_str());

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
                rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
                imshow("Left",Left);imshow("Right", RightCopy);
                waitKey(10); // display the images

                //randValue = rand() % 1000 + 500;
                int key = waitKey(100);
                if (key == '2'){
                    chameleonEye = false;
                }
            }

            stepValue Val = step1;

            // Automated eye roll movement
            while(eyeRoll == true){

                // Each case sets destination
                // coordinates for the eyes
                // to move to
                switch(Val){
                    case step1:
                        desLXVal = LxLm + 150;
                        desRXVal = RxLm + 150;
                        desLYVal = LyC;
                        desRYVal = RyC;
                        Val = step2;
                        cout << "step 1 ";
                        break;
                    case step2:
                        desLXVal = LxLm + 200;
                        desRXVal = RxLm + 200;
                        desLYVal = LyTm - 200;
                        desRYVal = RyTm - 200;
                        Val = step3;
                        cout << "step 2 ";
                        break;
                    case step3:
                        desLXVal = LxC;
                        desRXVal = RxC;
                        desLYVal = LyTm - 200;
                        desRYVal = RyTm - 200;
                        Val = step4;
                        cout << "step 3 ";
                        break;
                    case step4:
                        desLXVal = LxRm - 300;
                        desRXVal = RxRm - 200;
                        desLYVal = LyTm - 250;
                        desRYVal = RyTm - 200;
                        Val = step5;
                        cout << "step 4 ";
                        break;
                    case step5:
                        desLXVal = LxRm - 150;
                        desRXVal = RxRm - 150;
                        desLYVal = LyC;
                        desRYVal = RyC;
                        Val = step6;
                        cout << "step 5 ";
                        break;
                    case step6:
                        desLXVal = LxC;
                        desRXVal = RxC;
                        desLYVal = LyC;
                        desRYVal = RyC;
                        eyeRoll = false;

                        cout << "step 6 ";
                        break;
                    default:
                        break;
                }

                // Progressively move eyes
                // to destination coordinates
                while((Rx >= desRXVal + 25 || Rx <= desRXVal - 25) || (Ry >= desRYVal + 25 || Ry <= desRYVal - 25) ||
                       (Lx >= desLXVal + 25 || Lx <= desLXVal - 25) || (Ly >= desLYVal + 25 || Ly <= desLYVal - 25)){

                    ChangeRX = 0;
                    ChangeRY = 0;
                    ChangeLX = 0;
                    ChangeLY = 0;

                    // Movement speed
                    int changeVal = 20;

                    // Each coordinate is checked against the
                    // destination coordinate and progressively
                    // moved towards the target point if required
                    if (Ry > desRYVal){
                        ChangeRY = -changeVal;
                    }
                    if (Rx > desRXVal){
                        ChangeRX = -changeVal;
                    }
                    if (Ly > desLYVal){
                        ChangeLY = -changeVal;
                    }
                    if (Lx > desLXVal){
                        ChangeLX = -changeVal;
                    }

                    if (Ry < desRYVal){
                        ChangeRY = changeVal;
                    }
                    if (Rx < desRXVal){
                        ChangeRX = changeVal;
                    }
                    if (Ly < desLYVal){
                        ChangeLY = changeVal;
                    }
                    if (Lx < desLXVal){
                        ChangeLX = changeVal;
                    }

                    // Set the adjustment for each eye coordinate
                    Rx = Rx + ChangeRX;
                    Lx = Lx + ChangeLX;
                    Ry = Ry + ChangeRY;
                    Ly = Ly + ChangeLY;


                    CMDstream.str("");
                    CMDstream.clear();
                    CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
                    CMD = CMDstream.str();
                    RxPacket= OwlSendPacket (u_sock, CMD.c_str());

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
                    rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
                    imshow("Left",Left);imshow("Right", RightCopy);
                    waitKey(10); // display the images

                    //randValue = rand() % 1000 + 500;
                    int key = waitKey(10);
                    if (key == '3'){
                        eyeRoll = false;
                    }
                }
            }

            // Automated eye fear movement
            while(eyeFear == true){

                // Random value (1 - 40)
                randValue = rand() % 40 + 1;

                // If 'randValue' is a multiple of 10 (10, 20, 30, 40)
                if (randValue % 10 == 0){

                    int verticalMovement = 0;

                    // Randomly decide whether to move the eye vertically
                    if (randValue % 15 == 0){
                        // Random value (-10 and 10)
                        verticalMovement = rand() % 21 - 10;

                        Ry = RyC + verticalMovement;
                        Ly = LyC + verticalMovement;
                    }

                    // Alternate which direction the eyes snap to
                    if (eyeMoveLeft){

                        // Adjsut eyes
                        Rx = RxLm + 100;
                        Lx = LxLm + 100;

                        // Adjust neck in same direction
                        Neck = NeckC + 100;

                    }else{

                        // Adjsut eyes
                        Rx = RxRm - 100;
                        Lx = LxRm - 100;

                        // Adjust neck in same direction
                        Neck = NeckC - 100;

                    }

                    // Toggle which side the eyes are moving to
                    eyeMoveLeft = !eyeMoveLeft;

                }

                CMDstream.str("");
                CMDstream.clear();
                CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
                CMD = CMDstream.str();
                RxPacket= OwlSendPacket (u_sock, CMD.c_str());

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
                rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
                imshow("Left",Left);imshow("Right", RightCopy);
                waitKey(10); // display the images

                //randValue = rand() % 1000 + 500;
                int key = waitKey(100);
                if (key == '4'){
                    eyeFear = false;
                }
            }

            // Automated neck movement
            while(neckMovement == true){
                //if statement controls the direction the neck moves in
                //depending on how close it is to the neck max range values
                if (Neck <= NeckR + 100){

                    neckMoveLeft = true;

                }else if (Neck >= NeckL - 100){

                    neckMoveLeft = false;

                }

                if (neckMoveLeft == true){
                    //dValue reduces the neck value to manageable values for calculations
                    dValue = Neck/90;
                    //the mValue represents how close the Neck position is to its center position
                    mValue = 17 - dValue;
                    //if statement confirms all values become positive
                    if (mValue < 0){
                        mValue = -mValue;
                    }
                    //aValue is a computed value of how fast the neck should move depending on how
                    //close it is to the center value
                    aValue = 10 - mValue;
                    //values are made positive again
                    if (aValue < 0){
                        aValue = -aValue;
                    }
                    //the neck position is changed
                    Neck = Neck + (aValue * 3);

                }else{

                    dValue = Neck/90;

                    mValue = 17 - dValue;
                    if (mValue < 0){
                        mValue = -mValue;
                    }

                    aValue = 10 - mValue;
                    if (aValue < 0){
                        aValue = -aValue;
                    }

                    Neck = Neck - (aValue * 3);

                }

                CMDstream.str("");
                CMDstream.clear();
                CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
                CMD = CMDstream.str();
                RxPacket= OwlSendPacket (u_sock, CMD.c_str());

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
                rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 ); // draw white rect
                imshow("Left",Left);imshow("Right", RightCopy);
                waitKey(10); // display the images

                //randValue = rand() % 1000 + 500;
                int key = waitKey(100);
                if (key == '5'){
                    neckMovement = false;
                }
            }

            CMDstream.str("");
            CMDstream.clear();
            CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
            CMD = CMDstream.str();
            RxPacket= OwlSendPacket (u_sock, CMD.c_str());

            if (0) {
                for (int i=0;i<10;i++){
                    Rx=Rx-50; Lx=Lx-50;
                    CMDstream.str("");
                    CMDstream.clear();
                    CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
                    CMD = CMDstream.str();
                    RxPacket= OwlSendPacket (u_sock, CMD.c_str());
                    //waitKey(100); // cut the pause for a smooth persuit camera motion
                }
            }

        } // END cursor control loop
        // close windows down

        destroyAllWindows();



        // just a ZMCC
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
            /// Show me what you got
            Mat RightCopy;
            Right.copyTo(RightCopy);
            rectangle( RightCopy, target, Scalar::all(255), 2, 8, 0 );
            rectangle( Left, OWL.Match, Point( OWL.Match.x + OWLtempl.cols , OWL.Match.y + OWLtempl.rows), Scalar::all(255), 2, 8, 0 );
            rectangle( OWL.Result, OWL.Match, Point( OWL.Match.x + OWLtempl.cols , OWL.Match.y + OWLtempl.rows), Scalar::all(255), 2, 8, 0 );

            imshow("Owl-L", Left);
            imshow("Owl-R", RightCopy);
            imshow("Correl",OWL.Result );
            if (waitKey(10)== 27) inLOOP=false;
// P control
            double KPx=0.1; // track rate X
            double KPy=0.1; // track rate Y
            double LxScaleV = LxRangeV/(double)640; //PWM range /pixel range
            double Xoff= 320-(OWL.Match.x + OWLtempl.cols)/LxScaleV ; // compare to centre of image
            int LxOld=Lx;

            Lx=LxOld-Xoff*KPx; // roughly 300 servo offset = 320 [pixel offset


            double LyScaleV = LyRangeV/(double)480; //PWM range /pixel range
            double Yoff= (250+(OWL.Match.y + OWLtempl.rows)/LyScaleV)*KPy ; // compare to centre of image
            int LyOld=Ly;
            Ly=LyOld-Yoff; // roughly 300 servo offset = 320 [pixel offset

            cout << Lx << " " << Xoff << " " << LxOld << endl;
            cout << Ly << " " << Yoff << " " << LyOld << endl;
            //Atrous

            //Maxima

            // Align cameras

            // ZMCC disparity map

            // ACTION

            // move to get minimise distance from centre of both images, ie verge in to targe
            // move servos to position
            CMDstream.str("");
            CMDstream.clear();
            CMDstream << Rx << " " << Ry << " " << Lx << " " << Ly << " " << Neck;
            CMD = CMDstream.str();
            RxPacket= OwlSendPacket (u_sock, CMD.c_str());


        } // end if ZMCC
    } // end while outer loop
#ifdef _WIN32
        closesocket(u_sock);
#else
        close(clientSock);
#endif
        exit(0); // exit here for servo testing only
    }
