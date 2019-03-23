#ifndef __FACE_H__
#define __FACE_H__

#include <opencv2/opencv.hpp>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include <list>
#include <iostream>
#include <fstream>

#include "face.hpp" 

#define LBPH_MODE 0
#define FISH_MODE 1
#define EIGEN_FACE 2
#define FACE_REC_MODE FISH_MODE
 
#define FACE_SIZE_MIN 20
#define SCALE_MODE 0
#define FACE_SAMPLE_W 80
#define FACE_SAMPLE_H 80

struct Facerec_result {
	int predicted_label;
    double predicted_confidence;
};

using namespace cv;
using namespace std;
using namespace face;

vector <Rect> faceDectAndRecognize(CascadeClassifier face_cascade, 
								   CascadeClassifier eyeCascade1,
                                   CascadeClassifier eyeCascade2, 
								   Ptr<FaceRecognizer> model,
								   Mat srcImg,
								   Facerec_result* facerec_result);

Mat getPreprocessedFace(CascadeClassifier cascade, CascadeClassifier eyeCascade1,
						CascadeClassifier eyeCascade2, 
						Mat srcImg, 
						int desiredFaceWidth, int desiredFaceHeight, 
						bool doLeftAndRightSeparately, 
						Rect *storeFaceRect, 
						Point *storeLeftEye, Point *storeRightEye, 
						Rect *searchedLeftEye, Rect *searchedRightEye);
#endif