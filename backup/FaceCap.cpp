#include <opencv2/opencv.hpp>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include <iostream>
#include <fstream>
#include <list>
#include "Camera.h"
#include "VoiceText_IPC.h"

#define VIDEO_USING_IPC 0

using namespace cv;
using namespace std;


camera_t* viedoOpen(char* deviceName, int cam_w, int cam_h)
{
    camera_t* camera = camera_open(deviceName, cam_w, cam_h);
    camera_init(camera);
    return camera;
}

vector <Rect> videoCaptureFindFaces(camera_t* camera, int cam_w, int cam_h, CascadeClassifier face_cascade)
{
	vector <Rect> faces;
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    camera_start(camera);
    for (int i = 0; i < 5; i++) camera_frame(camera, timeout);/* skip 5 frames for booting a cam */
    camera_frame(camera, timeout);
	
	unsigned char* rgb =  yuyv2rgb(camera->head.start, camera->width, camera->height);
    Mat src =  Mat(cam_w, cam_h, CV_8UC4, rgb);
    face_cascade.detectMultiScale(src, faces, 2, 1,
                                  CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_SCALE_IMAGE,
                                  Size(30, 30), Size(900, 900));
	free(rgb);
	return faces;
}

void videoRelease(camera_t* camera)
{
    camera_stop(camera);
    camera_finish(camera);
    camera_close(camera);
}

Rect Maxfac;

BOOL maxFacesRefresh(vector <Rect> faces)
{
	BOOL maxChange = FALSE;
	if (faces.size() > 0) {
        int index;
        Rect face;
        for (index = 0; index < faces.size(); index++) {
            face = faces[index];
            cout << "face found " <<" x : "<<face.x<<", y : "<<face.y<<", width : "<<face.width<<", height : "<<face.height<< endl;
            if(index==0&&Maxfac.width==0) {
                Maxfac = face;
                maxChange = TRUE;
            }
            else if(face.width>Maxfac.width) {
                Maxfac = face;
                maxChange = TRUE;
            }
        }
    }
	return maxChange;
}

int IPC_Put_videoInfo(VoiceText_IPC *ipc, int videoId, vector <Rect> faces, int maxFacesPut)
{
	UINT8 status = 0;
	for(int faceIdx = 0; faceIdx <faces.size()&&faceIdx<maxFacesPut; faceIdx++)
	{	
		char xtag[20], ytag[20], atag[20];
		char xValue[10], yValue[10], aValue[10];
		
		sprintf(xtag, "video%d_%d_x", videoId, faceIdx);
		sprintf(xValue, "%d", faces[faceIdx].x);
		sprintf(ytag, "video%d_%d_y", videoId, faceIdx);
		sprintf(yValue, "%d", faces[faceIdx].y);
		sprintf(atag, "video%d_%d_area", videoId, faceIdx);
		sprintf(aValue, "%d", faces[faceIdx].width);
		
		status += ipc->IPC_Put_TAG_String(xtag, xValue);
		status += ipc->IPC_Put_TAG_String(ytag, yValue);
		status += ipc->IPC_Put_TAG_String(atag, aValue);
	}
	return status;
}

struct video {
	camera_t* camera;
	int index;
    string devName;
};

int main(int argc, char *argv[])
{
	int CAMERA_NUM;
    int cam_h = 480, cam_w = 640, idx = 1;
	int runCount;
	list<video> vList;
	list<string> nList;
	list<string>::iterator devName_it;
	list<video>::iterator video_it;
	
	if(argc<2){
		fprintf(stderr, "parameter miss(video number) !!\n");
		exit(1);
	}else if(atoi(argv[1])!=(argc-2)){
		fprintf(stderr, "CAMERA_NUM isn't match witch video count!!\n");
		exit(1);
	}else CAMERA_NUM = atoi(argv[1]);

	for(int i = 0; i<CAMERA_NUM; i++) {
		char tmp[20];
		sprintf(tmp, "/dev/video%s", argv[2+i]);
		nList.push_back(tmp);
	}
	
	for(devName_it = nList.begin(); devName_it!=nList.end(); devName_it++, idx++) {
		video v = {NULL, idx, *devName_it};
		vList.push_back(v);
	}
	
	for(video_it = vList.begin(); video_it!=vList.end(); video_it++) 
	{
		cout << "vedio add! : " <<(char*)video_it->devName.c_str()<< endl;
		video_it->camera = viedoOpen("/dev/video11", cam_w, cam_h);						
		cout << "vedio add!! : " <<(char*)video_it->devName.c_str()<< endl;
	}
#if VIDEO_USING_IPC
	VoiceText_IPC *ipc = VoiceText_IPC::VoiceText_IPC_GetInstance();
	if(ipc == NULL){
		fprintf(stderr, "IPC Init failly!!\n");
		exit(1);
	}
#endif
	
    //Load haarcascade xml file***********************************************************************>>>
    string stdFileName("./haarcascade_frontalface_alt.xml");
    CascadeClassifier face_cascade;
    if (!face_cascade.load(stdFileName)) {
        cout << "cascade loaded fail " << endl;
        return 0;
    }
    //Load haarcascade xml file***********************************************************************<<<

	runCount = 0;
    while (runCount<10) 
	{	
		int videoIdx = 0;
		for(video_it = vList.begin(); video_it!=vList.end(); video_it++){
			vector <Rect> faces = videoCaptureFindFaces(video_it->camera, cam_w, cam_h, face_cascade);
			camera_stop(video_it->camera);
			cout << "["<<video_it->devName<<"] face number : " << faces.size() <<" "<< endl;
			if(maxFacesRefresh(faces)) videoIdx = video_it->index;
		}
		if(videoIdx!=0){
            cout << "Max face at [" <<videoIdx-1<<"] : "<<" x : "<<Maxfac.x<<", y : "<<Maxfac.y<<", w : "<<Maxfac.width<<", h : "<<Maxfac.height<< endl;
        }
		
#if VIDEO_USING_IPC	
		int facePutNum = 2;
		IPC_Put_videoInfo(ipc, 0, face0, facePutNum);
		IPC_Put_videoInfo(ipc, 1, face1, facePutNum);
		IPC_Put_videoInfo(ipc, 2, face2, facePutNum);
		IPC_Put_videoInfo(ipc, 3, face3, facePutNum);
		IPC_Put_videoInfo(ipc, 4, face4, facePutNum);
		IPC_Put_videoInfo(ipc, 5, face5, facePutNum);
#endif
		sleep(1);
        cout<<"run "<<runCount<<endl;
        runCount++;
	}

	for(video_it = vList.begin(); video_it!=vList.end(); video_it++) 
		videoRelease(video_it->camera);
   
    return 0;
}