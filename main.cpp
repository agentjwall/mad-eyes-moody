#include <iostream>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <fstream>
#include "opencv2/imgproc/imgproc.hpp"
#include "constants.h"

using namespace std;
using namespace cv;

#define DEBUG 0

typedef struct {
    Point CenterPointOfEyes;
    Point OffsetFromEyeCenter;
    int eyeLeftMax=0;//13;
    int eyeRightMax=0;//13;
    int eyeTopMax=0;//11;
    int eyeBottomMax=0;//11;
    int count = 0;
} EyeSettingsSt;
EyeSettingsSt EyeSettings;

void scale(const Mat &src,Mat &dst) {
    cv::resize(src, dst, cv::Size(kFastEyeWidth,(((float)kFastEyeWidth)/src.cols) * src.rows));
}

Point unscale_point(Point p, Rect origSize) {
    float ratio = (((float)kFastEyeWidth)/origSize.width);
    int x = round(p.x / ratio);
    int y = round(p.y / ratio);
    return Point(x,y);
}

Mat matrix_magnitude(Mat mat_x, Mat mat_y) {
    Mat mag(mat_x.rows, mat_x.cols, CV_64F);

    for (int y = 0; y < mat_x.rows; y++) {
        const double *x_row = mat_x.ptr<double>(y), *y_row = mat_y.ptr<double>(y);
        double *mag_row = mag.ptr<double>(y);
        for (int x = 0; x < mat_x.cols; x++) {
            double gx = x_row[x], gy = y_row[x];
            double magnitude = sqrt((gx * gx) + (gy * gy));
            mag_row[x] = magnitude;
        }
    }
    return mag;
}


/*
 * Find possible center in gradient location
 * doesn't use the postprocessing weight of color (section 2.1)
 */
void possible_centers(int x, int y, const Mat &blurred, double gx, double gy, Mat &output) {

    for (int cy = 0; cy < output.rows; cy++) {
        double *output_row = output.ptr<double>(cy);
        for (int cx = 0; cx < output.cols; cx++) {
            if (x == cx && y == cy) {
                continue;
            }
            // equation (2)
            double dx = x - cx;
            double dy = y - cy;
            double magnitude = sqrt((dx * dx) + (dy * dy));
            dx = dx / magnitude;
            dy = dy / magnitude;

            double dotProduct = (dx*gx + dy*gy);
            // ignores vectors pointing in opposite direction/negative dot products
            dotProduct = max(0.0, dotProduct);

            // summation
            output_row[cx] += dotProduct * dotProduct;
        }
    }
}

/*
 * Imitate Matlab gradiant function, to better match results from paper
 */
Mat computeMatXGradient(const Mat &mat) {
    Mat out(mat.rows,mat.cols,CV_64F);

    for (int y = 0; y < mat.rows; ++y) {
        const uchar *Mr = mat.ptr<uchar>(y);
        double *Or = out.ptr<double>(y);

        Or[0] = Mr[1] - Mr[0];
        for (int x = 1; x < mat.cols - 1; ++x) {
            Or[x] = (Mr[x+1] - Mr[x-1])/2.0;
        }
        Or[mat.cols-1] = Mr[mat.cols-1] - Mr[mat.cols-2];
    }

    return out;
}

/*
 * Finds the pupils within the given eye region
 * returns points of where pupil is calculated to be
 *
 * face_image: image of face region from frame
 * eye_region: dimensions of eye region
 * window_name: display window name
 */
Point find_centers(Mat face_image, Rect eye_region) {

    Mat eye_unscaled = face_image(eye_region);

    // scale and grey image
    Mat eye_scaled_gray;
    scale(eye_unscaled, eye_scaled_gray);
    cvtColor(eye_scaled_gray, eye_scaled_gray, COLOR_BGRA2GRAY);

    // get the gradient of eye regions
    Mat gradient_x, gradient_y;
    gradient_x = computeMatXGradient(eye_scaled_gray);
    //Sobel(eye_scaled_gray, gradient_x, CV_64F, 1, 0, 5);
    gradient_y = computeMatXGradient(eye_scaled_gray.t()).t();
    //Sobel(eye_scaled_gray, gradient_y, CV_64F, 0, 1, 5);

    //Mat magnitude = matrix_magnitude(gradient_x, gradient_y);

    // normalized displacement vectors
    normalize(gradient_x, gradient_x);
    normalize(gradient_y, gradient_y);

    // blur and invert the image
    Mat blurred;
    GaussianBlur(eye_scaled_gray, blurred, Size(5, 5), 0, 0);
    bitwise_not(blurred, blurred);
    //increase contrast and decrease brightness
    for( int y = 0; y < blurred.rows; y++ )
    {
        for( int x = 0; x < blurred.cols; x++ )
        {
            for( int c = 0; c < 3; c++ )
            {
                blurred.at<Vec3b>(y,x)[c] = saturate_cast<uchar>(1.01*( blurred.at<Vec3b>(y,x)[c] ) - 10 );
            }
        }
    }

    //imshow("window", blurred);

    Mat outSum = Mat::zeros(eye_scaled_gray.rows, eye_scaled_gray.cols, CV_64F);

    for (int y = 0; y < blurred.rows; y++) {
        const double *x_row = gradient_x.ptr<double>(y), *y_row = gradient_y.ptr<double>(y);
        for (int x = 0; x < blurred.cols; x++) {
            double gx = x_row[x], gy = y_row[x];
            if (gx == 0.0 && gy == 0.0) {
                continue;
            }
            possible_centers(x, y, blurred, gx, gy, outSum);
        }
    }

    double numGradients = (blurred.rows*blurred.cols);
    Mat out;
    outSum.convertTo(out, CV_32F, 1.0/numGradients);

    Point max_point;
    double max_value;
    minMaxLoc(out, NULL, &max_value, NULL, &max_point);

    Point pupil = unscale_point(max_point, eye_region);
    return pupil;
}

/*
 * returns an array of points of the pupils
 * [left pupil, right pupil]
 *
 * color_image: image of the whole frame
 * face: dimensions of face in color_image
 */
void find_eyes(Mat color_image, Rect face, Point &left_pupil_dst, Point &right_pupil_dst, Rect &left_eye_region_dst, Rect &right_eye_region_dst) {
    // image of face
    Mat face_image = color_image(face);

    int eye_width = face.width * (kEyePercentWidth/100.0);
    int eye_height = face.height * (kEyePercentHeight/100.0);
    int eye_top = face.height * (kEyePercentTop/100.0);
    int eye_side = face.width * (kEyePercentSide/100.0);
    int right_eye_x = face.width - eye_width -  eye_side;

    // eye regions
    Rect left_eye_region(eye_side, eye_top, eye_width, eye_height);
    Rect right_eye_region(right_eye_x, eye_top, eye_width, eye_height);

    // get points of pupils within eye region
    Point left_pupil = find_centers(face_image, left_eye_region);
    Point right_pupil = find_centers(face_image, right_eye_region);

    // convert points to fit on frame image
    right_pupil.x += right_eye_region.x;
    right_pupil.y += right_eye_region.y;
    left_pupil.x += left_eye_region.x;
    left_pupil.y += left_eye_region.y;


    left_pupil_dst = left_pupil;
    right_pupil_dst = right_pupil;
    left_eye_region_dst = left_eye_region;
    right_eye_region_dst = right_eye_region;
}

void display_eyes(Mat color_image, Rect face, Point left_pupil, Point right_pupil, Rect left_eye_region, Rect right_eye_region) {
    Mat face_image = color_image(face);

    // draw eye regions
    rectangle(face_image, left_eye_region, Scalar(0, 0, 255));
    rectangle(face_image, right_eye_region, Scalar(0, 0, 255));

    //find eye center
    Point center;
    center.x = (right_pupil.x - left_pupil.x)/2 + left_pupil.x;
    center.y = (right_pupil.y + left_pupil.y)/2;

    // draw pupils
    circle(face_image, right_pupil, 3, Scalar(0, 255, 0));
    circle(face_image, left_pupil, 3, Scalar(0, 255, 0));
    //circle(face_image, center, 3, Scalar(255, 0, 0));

    std::string left_eye_region_width = std::to_string(left_eye_region.x + (left_eye_region.width/2));
    std::string left_eye_region_height = std::to_string(left_eye_region.y + (left_eye_region.height/2));

    std::string right_eye_region_width = std::to_string(right_eye_region.x + (right_eye_region.width/2));
    std::string right_eye_region_height = std::to_string(right_eye_region.y + (right_eye_region.height/2));

    std::string xleft_pupil_string = std::to_string(left_pupil.x);
    std::string yleft_pupil_string = std::to_string(left_pupil.y);

    std::string xright_pupil_string = std::to_string(right_pupil.x);
    std::string yright_pupil_string = std::to_string(right_pupil.y);

    String text1 = "Pupil(L,R): ([" + xleft_pupil_string + "," + yleft_pupil_string + "],[" + xright_pupil_string + "," + yright_pupil_string + "])";
    String text2 = "Center(L,R): ([" + left_eye_region_width + ", " + left_eye_region_height +"]," + "[" + right_eye_region_width + ", " + right_eye_region_height +"])";


    //add data
    putText (color_image, text1 + " " + text2, cvPoint(20,700), FONT_HERSHEY_SIMPLEX, double(1), Scalar(255,0,0));

    //display calibration points
    if(EyeSettings.eyeTopMax) {
        circle(color_image, Point(color_image.cols / 2, 0), 5, Scalar(0, 255,0), -1);
    }else{
        circle(color_image, Point(color_image.cols / 2, 0), 5, Scalar(0, 0, 255), -1);
    }
    if(EyeSettings.eyeRightMax) {
        circle(color_image, Point(color_image.cols, color_image.rows / 2), 5, Scalar(0, 255,0), -1);
    }else{
        circle(color_image, Point(color_image.cols, color_image.rows / 2), 5, Scalar(0, 0,255), -1);
    }
    if(EyeSettings.eyeBottomMax) {
        circle(color_image, Point(color_image.cols / 2, color_image.rows), 5, Scalar(0, 255,0), -1);
    }else{
        circle(color_image, Point(color_image.cols / 2, color_image.rows), 5, Scalar(0, 0,255), -1);
    }
    if(EyeSettings.eyeLeftMax) {
        circle(color_image, Point(0, color_image.rows / 2), 5, Scalar(0, 255,0), -1);
    }else{
        circle(color_image, Point(0, color_image.rows / 2), 5, Scalar(0, 0,255), -1);
    }
}

void display_shapes_on_screen(Mat background, vector<Point> shapes, Point guess) {
    int best_dist = -1;
    Point best_point;

    for (Point s: shapes) {
        int dist = pow(s.x - guess.x, 2) + pow(s.y - guess.y, 2);
        if (best_dist == -1 or dist < best_dist) {
            best_dist = dist;
            best_point = s;
        }
    }

    for (Point s : shapes) {
        if (s == best_point) {
            circle(background, s, 20, Scalar(0,255,0), -1);
            circle(background, s, 20, Scalar(0,0,0), 2);
        } else {
            circle(background, s, 20, Scalar(0,0,255), -1);
            circle(background, s, 20, Scalar(0,0,0), 2);
        }
    }
    circle(background, guess, 5, Scalar(0,0,0), -1);
}

void ListenForCalibrate(int wait_key) {
    //left calibration 97
    //right calibration 100
    //bottom calibration 115
    //top calibration 119
    switch (wait_key) {
        case 97:
            EyeSettings.eyeLeftMax = abs(EyeSettings.OffsetFromEyeCenter.x);
            #if DEBUG
            imwrite("test/calib-left.png", frame);
            #endif
            break;
        case 100:
            EyeSettings.eyeRightMax = abs(EyeSettings.OffsetFromEyeCenter.x);
            #if DEBUG
            imwrite("test/calib-right.png", frame);
            #endif
            break;
        case 115:
            EyeSettings.eyeBottomMax = abs(EyeSettings.OffsetFromEyeCenter.y);
            #if DEBUG
            imwrite("test/calib-bot.png", frame);
            #endif
            break;
        case 119:
            EyeSettings.eyeTopMax = abs(EyeSettings.OffsetFromEyeCenter.y);
            #if DEBUG
            imwrite("test/calib-top.png", frame);
            #endif
            break;
    }
}

int main(int argc, char* argv[]) {
    bool doImport = false;
    bool doExport = false;
    bool doCalibrate = true;
    bool doTest = false;
    bool doTrain = false;
    bool showCam = false;
    bool hasFile = false;
    int shapes_x = -1;
    int shapes_y = -1;
    ofstream file;

    for(int i = 1; i < argc; i++) {
        if (string("-").compare(string(argv[i]).substr(0,1)) == 0) {
            if (string("--import").compare(argv[i]) == 0 || string("-i").compare(argv[i]) == 0) {
                doImport = true;
            } else if (string("--export").compare(argv[i]) == 0 || string("-e").compare(argv[i]) == 0) {
                doExport = true;
            } else if (string("--calibrate").compare(argv[i]) == 0 || string("-c").compare(argv[i]) == 0) {
                doCalibrate = true;
            } else if (string("--train").compare(argv[i]) == 0 || string("-T").compare(argv[i]) == 0) {
                doTrain = true;
            } else if (string("--test").compare(argv[i]) == 0 || string("-t").compare(argv[i]) == 0) {
                doTest = true;
            } else if (string("--show-cam").compare(argv[i]) == 0 || string("-w").compare(argv[i]) == 0) {
                doTrain = true;
            } else if (string("--file-name").compare(argv[i]) == 0 || string("-f").compare(argv[i]) == 0) {
                if (i+1 < argc) {
                    file.open(argv[i + 1]);
                    if (!file) {
                        cerr << "Failed to open <" << argv[i] << ">!";
                        exit(1);
                    } else {
                        hasFile = true;
                    }
                } else {
                    cerr << "ERROR: please enter a file name!";
                    exit(1);
                }
            } else {
                cerr << "ERROR: No argument <" << argv[i] << "> exists!";
                exit(1);
            }
        } else if (i+2 == argc){
            shapes_x = atoi(argv[i]);
            shapes_y = atoi(argv[i+1]);
            break;
        } else {
            cerr << "ERROR: Incorrect number of arguments!\n" <<
                    "Syntax main [--import|-i] [--export|-e] [--calibrate|-c] [--test| -T] [--train|-t] " <<
                    "[--show-cam|-w] [--filename|-f CALIBRATION_FILE] [SHAPES_X SHAPES_Y]";
            exit(1);
        }
    }

    if (showCam + doTrain + doTest > 1) {
        cerr << "You cannot show the camera or train or test at the same time! (Mutually exclusive)";
        exit(1);
    }

    if ((doImport || doExport) && !hasFile) {
        cerr << "You must define a file! -f <FILENAME>";
    }



    const int height = 900;
    const int width = 1440;

    //define font
    CvFont font;
    double hScale=1.0;
    double vScale=1.0;
    int    lineWidth=6;
    cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);

    CascadeClassifier face_cascade;
    face_cascade.load("haar_data/haarcascade_frontalface_alt.xml");

    VideoCapture cap(0);
    if (!cap.isOpened()) {
        return -1;
    }

    namedWindow("window");
    Mat frame, shape_grey, shape_screen;
    shape_grey = Mat(height,width, CV_8UC1);
    cap >> frame;
    vector<Point> shapes{Point(100,100), Point(100,200), Point(200,200)};
    cvtColor(shape_grey, shape_screen, COLOR_GRAY2BGR);

    while (1) {
        Mat gray_image;
        vector<Rect> faces;
        cvtColor(frame, gray_image, COLOR_BGRA2GRAY);
        shape_screen.setTo(cv::Scalar(255,255,255));

        face_cascade.detectMultiScale(gray_image, faces, 1.7, 2, 0|CV_HAAR_SCALE_IMAGE|CV_HAAR_FIND_BIGGEST_OBJECT);

        Point left_pupil, right_pupil;
        Rect left_eye, right_eye;
        if (faces.size() > 0) {
            find_eyes(frame, faces[0], left_pupil, right_pupil, left_eye, right_eye);
            display_eyes(frame, faces[0], left_pupil, right_pupil, left_eye, right_eye);
        }

        // if 'q' is tapped, exit
        int wait_key = waitKey(8);
        if (wait_key == 113) {
            break;
        }

        EyeSettings.CenterPointOfEyes.x = ((right_eye.x + right_eye.width/2) + (left_eye.x + left_eye.width/2))/2;
        EyeSettings.CenterPointOfEyes.y = ((right_eye.y + right_eye.height/2) + (left_eye.y + left_eye.height/2))/2;

        EyeSettings.OffsetFromEyeCenter.x = EyeSettings.CenterPointOfEyes.x - (right_pupil.x + left_pupil.x)/2;
        EyeSettings.OffsetFromEyeCenter.y = EyeSettings.CenterPointOfEyes.y - (right_pupil.y + left_pupil.y)/2;

        ListenForCalibrate(wait_key);

        //space for test
        if(wait_key == 32)
        {
            doCalibrate = !doCalibrate;
        }

        if (!doCalibrate) {
            double pupilOffsetfromLeft = EyeSettings.OffsetFromEyeCenter.x+EyeSettings.eyeLeftMax;
            double pupilOffsetfromBottom = EyeSettings.OffsetFromEyeCenter.y+EyeSettings.eyeBottomMax;

            double percentageWidth = pupilOffsetfromLeft / (double)(EyeSettings.eyeLeftMax + EyeSettings.eyeRightMax);
            if(percentageWidth < 0){
                percentageWidth = 0;
            }else if(percentageWidth > 1){
                percentageWidth = 1;
            }
            double percentageHeight = pupilOffsetfromBottom / (double)(EyeSettings.eyeTopMax + EyeSettings.eyeBottomMax);
            if(percentageHeight < 0){
                percentageHeight = 0;
            }else if(percentageHeight > 1){
                percentageHeight = 1;
            }

            #if DEBUG
            cout << "xmax: " << (EyeSettings.eyeLeftMax + EyeSettings.eyeRightMax) << " cur: " << pupilOffsetfromLeft << " = "<< percentageWidth << " , "
                 << "ymax: " << (EyeSettings.eyeTopMax + EyeSettings.eyeBottomMax) << " cur: " << pupilOffsetfromBottom << " = "<< percentageHeight << endl;
            //draw expected position on screen from pupils
            circle(frame, Point(
                           (frame.cols*(percentageWidth)),
                           (frame.rows*(1-percentageHeight))),
                   5, Scalar(255, 255, 0), -1);

            Point pupilCenter = Point((right_pupil.x + left_pupil.x)/2, (right_pupil.y + left_pupil.y)/2);
            //draw pupil position
            circle(frame, Point(
                           pupilCenter.x + faces[0].x,
                           pupilCenter.y + faces[0].y),
                   3, Scalar(255, 0, 0), -1);
            //draw pupil bounding box from config
            rectangle(frame,
                      Rect(
                              EyeSettings.CenterPointOfEyes.x - EyeSettings.eyeRightMax + faces[0].x,
                              EyeSettings.CenterPointOfEyes.y - EyeSettings.eyeBottomMax + faces[0].y,
                              (EyeSettings.eyeLeftMax + EyeSettings.eyeRightMax),
                              (EyeSettings.eyeTopMax + EyeSettings.eyeBottomMax)
                      ),Scalar(255,255,0), 1);
            //draw eye center
            Point drawEyeCenter = Point(EyeSettings.CenterPointOfEyes.x + faces[0].x,
                                        EyeSettings.CenterPointOfEyes.y + faces[0].y);
            circle(frame, drawEyeCenter, 3, Scalar(0, 0, 255));

            //imwrite(("test/test"+std::to_string(EyeSettings.count)+".png"), shape_screen);
            imwrite(("test/testcolor"+std::to_string(EyeSettings.count)+".png"), frame);
            EyeSettings.count++;
            imshow("window", frame);
            #else
            display_shapes_on_screen(shape_screen, shapes, Point(frame.cols*percentageWidth, frame.rows*(1-percentageHeight)));
            imshow("window", shape_screen);
            #endif
        }

        if(doCalibrate) {
            imshow("window", frame);
        }

        cap >> frame;
    }

    return 0;
}