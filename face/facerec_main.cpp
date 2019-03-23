#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "face.h"
#include <iostream>

using namespace std;
using namespace cv;
using namespace face;

// Preprocess left & right sides of the face separately, in case there is stronger light on one side.
const bool preprocessLeftAndRightSeparately = true;

vector <Rect> faceDectAndRecognize(CascadeClassifier face_cascade, 
								   CascadeClassifier eyeCascade1,
                                   CascadeClassifier eyeCascade2, 
								   Ptr<FaceRecognizer> model,
								   Mat srcImg,
								   Facerec_result* facerec_result)
{
	vector <Rect> faces;
    Rect faceRect;
    Rect searchedLeftEye, searchedRightEye;
    Point leftEye, rightEye;
    Mat face_img = getPreprocessedFace(face_cascade, eyeCascade1, eyeCascade2,
                                   srcImg,
                                   FACE_SAMPLE_W, FACE_SAMPLE_H,
                                   preprocessLeftAndRightSeparately,
                                   &faceRect,
                                   &leftEye, &rightEye,
                                   &searchedLeftEye, &searchedRightEye);

    if (face_img.cols>0) {
        int predicted_label = -1;
        double predicted_confidence = 0.0;
        double similarity = 0;

        model->predict(face_img, predicted_label, predicted_confidence);
		
        if (FACE_REC_MODE == EIGEN_FACE || FACE_REC_MODE == FISH_MODE) {
            similarity = (double)predicted_confidence/((double)(face_img.rows*face_img.cols));
            similarity = 1.0 - min(max(similarity, 0.0), 1.0);
            predicted_confidence = (double)(similarity*100);
        }

        //printf("label : %d, confidence : %f%s.\n", predicted_label, predicted_confidence, "%");
        if (predicted_confidence<=100&&predicted_confidence>0) {
			facerec_result->predicted_label = predicted_label;
			facerec_result->predicted_confidence = predicted_confidence;
			faces.push_back(faceRect);    
		}
    }
	
	return faces;
}