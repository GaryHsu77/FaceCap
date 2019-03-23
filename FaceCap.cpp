#include <opencv2/opencv.hpp>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include <iostream>
#include <fstream>
#include <list>
#include "camera/CAMERA_DEV.hpp"
#include "ttyacm/TTYACM_DEV.hpp"
#include "face/face.h"
#include "IPC/VoiceText_IPC.h"

#define VIDEO_USING_IPC 1
#define LED_CTRL 0
#define FACES_IPC_PUT 2 //how many faces info put to IPC.
#define FACE_RECOGNIZER 1

using namespace cv;
using namespace std;
using namespace face;

//Globl Variable
static char *ttyACM_name = NULL;
static int ttyACM_fd = -1;
static char video_name[20];
volatile int ProcessExitFlag = 0;
static int camera_exist = 0;

//rotater variable
struct Rotate_Direction_Struct
{
	const int dir;
	const char *RMS;
	int RMS_value;
};

struct Rotate_Direction_Struct Rotate_Directions [] = 
{
	{0,"rms_0",0},
	{1,"rms_1",0},
	{2,"rms_2",0},
	{3,"rms_3",0},
};
const int Rotate_Directions_size = sizeof(Rotate_Directions)/sizeof(Rotate_Directions[0]);

static void ProcessShutdownHook(int dummy)
{
    //info main to exit.
    ProcessExitFlag = 1;
}

int ManeaSteperWriteByte(TTYACM_DEV &arduino, unsigned char wbyte)
{
	char tmp[10];
	sprintf(tmp, "!%c$", wbyte);
	//printf("%s, (%d)\n", tmp, (int)wbyte);
	arduino.WriteBytes(tmp, 3);
}

int ManeaSteperReadBytes(TTYACM_DEV &arduino)
{
	char result[64];
	int read_len = 0;
	while(1){
		char tmp[64];
		int tmp_len = 0;
		tmp_len = arduino.ReadBytes(tmp, 64);
		if(tmp[0]==10) break;
		read_len = tmp_len;
		memcpy(result, tmp, read_len);
	}
	//printf("[FaceCap uart read] len : %d, buf : %s\n", read_len, result);
	return read_len;
}

void mat_capture(Mat &img, const unsigned char* yuyv_data, int cam_w, int cam_h)
{
    Mat yuyv(cam_h, cam_w, CV_8UC2);
    memcpy( yuyv.data, yuyv_data, sizeof(unsigned char) * (cam_w*2*cam_h) );
    cvtColor(yuyv, img, CV_YUV2BGR_YUYV);
    yuyv.release();
}

INT32 FaceDirDectionFromSoundDir(vector <Rect> faces)
{
    int maxIdx = 0, maxWidth = 0;

    for (int faceIdx = 0; faceIdx <faces.size(); faceIdx++)
    {
        if (maxWidth==0||(faces[faceIdx].width>maxWidth)) {
            maxIdx = faceIdx;
            maxWidth = faces[faceIdx].width;
        }
    }

    if (maxWidth!=0) return faces[maxIdx].x;
    
    return 0;
}

vector <Rect> facesDect(CvHaarClassifierCascade* classifier, Mat &img, int cam_w, int cam_h)
{
    vector <Rect> faces;

    IplImage copy = img;
    IplImage* imageRGB = &copy;

    //transfer image to gray
    IplImage* grayImg = cvCreateImage(cvGetSize(imageRGB), IPL_DEPTH_8U, 1);
    cvCvtColor(imageRGB, grayImg, CV_BGR2GRAY);

    CvMemStorage* facesMemStorage=cvCreateMemStorage(0);
    cvClearMemStorage(facesMemStorage);
    CvSeq* facesSeq = cvHaarDetectObjects(grayImg, classifier, facesMemStorage, 1.1, 3, CV_HAAR_DO_CANNY_PRUNING, cvSize(70, 70));

    if (facesSeq) {
        for (int i=0; i<facesSeq->total; ++i) {
            CvRect* rectangle = (CvRect*)cvGetSeqElem(facesSeq, i);
            Rect* face = (Rect*)malloc(sizeof(Rect));
            face->x = rectangle->x;
            face->y = rectangle->y;
            face->width = rectangle->width;
            face->height = rectangle->height;
            faces.push_back((Rect)*face);
            free(face);//GDB release
        }
    }

    cvReleaseMemStorage(&facesMemStorage);//GDB release
    cvReleaseImage(&grayImg);//GDB release
    img.release();//GDB release
    return faces;
}

int Face_xPoint_GetbyCamera(CAMERA_DEV &camera, CvHaarClassifierCascade* classifier, int cam_w, int cam_h)
{
	int face_x = 0;
	vector<Rect> faces;
	Mat img(cam_h, cam_w, CV_8UC4);
	mat_capture(img, camera.Capture(), cam_w, cam_h);
	faces = facesDect(classifier, img, cam_w, cam_h);
	
	face_x = FaceDirDectionFromSoundDir(faces);
	//printf("face_num : %d, face_x : %d\n", (int)faces.size(), face_x);
	return face_x;
}

int main(int argc, char *argv[])
{
	CvHaarClassifierCascade* classifier;
    int cam_h = 480, cam_w = 640;
    int runCount = 0, DBGR = 1;
    BOOL RUN_UNLIMIT = FALSE;
	BOOL ROTATER_RUN = FALSE;
	
    //register ctrl+c for release camera resource.
    signal(SIGINT, ProcessShutdownHook);

    //parameter(arg) process.
    if(argc<3){
        fprintf(stderr, "parameter miss\n   arg[1] : run loop count.(F for unlimit)\n   arg[2...] : dev/videoxx index number.\n");
        exit(1);
    }else{
        sprintf(video_name, "/dev/video%s", argv[2]);
		printf("FaceCap : video dev name :  %s.\n", video_name);
		
        if(strcmp(argv[1], "F")==0)
			RUN_UNLIMIT = TRUE;
    }

#if VIDEO_USING_IPC
    INT32 resolution_mode = 0;
	
    VoiceText_IPC *ipc = VoiceText_IPC::VoiceText_IPC_GetInstance();
    if (ipc == NULL) {
        fprintf(stderr, "FaceCap : IPC Init failly!!\n");
        exit(1);
    }

    DBGR = ipc->IPC_Get_TAG_INT32((char*)"face_dbg");
    resolution_mode = ipc->IPC_Get_TAG_INT32((char*)"face_resolution");
    //0 : default 640*480
    //1 : 320*240
    //2 : 1024*768
	resolution_mode = 0;
    if (resolution_mode == 1) {
        printf("FaceCap : resolution 320*240\n");
        cam_w = 320;
        cam_h = 240;
    } else if (resolution_mode == 2) {
        printf("FaceCap : resolution 1024*768\n");
        cam_w = 1024;
        cam_h = 768;
    }
#endif
	
	//create ttyacm dev(arduino) class.
	TTYACM_DEV arduino;
	if(argc>=4) {
		ttyACM_name = argv[3];
		arduino.Init(ttyACM_name);
		if(arduino.ToError() != NULL){
			fprintf(stderr,"FaceCap : [TTYACM_DEV] %s\n",arduino.ToError());
			exit(1);
		}
		
		ipc->IPC_Stall(3500*1000); //3.5s
		printf("FaceCap : %s init success.\n", ttyACM_name);
		ROTATER_RUN = TRUE;
	}

	//create video dev class.
	CAMERA_DEV camera(video_name);
	if(camera.ToError() != NULL){
		fprintf(stderr,"FaceCap : [CAMERA_DEV] %s\n",camera.ToError());
		exit(1);
	}
	
	//Load haarcascade xml file
    string face_cascade_name("./haarcascade_frontalface_alt2.0.xml");
    classifier=(CvHaarClassifierCascade*)cvLoad(face_cascade_name.c_str(), 0, 0, 0);
    if (!classifier) {
        fprintf(stderr, "FaceCap : face2.0 classifier loaded failly!!\n");
        exit(1);
    }

    //main function loop start***********************************************************************//
    runCount = 0;//if arg[1] = 'F', RUN_UNLIMIT = TRUE.
	char area = 'b';
	int trace_rec = 0;
    while ( (runCount<atoi(argv[1])||RUN_UNLIMIT) && !ProcessExitFlag )
    {   
		struct Rotate_Direction_Struct * rotate_array[Rotate_Directions_size];
		
		//get four mic RMS value.
		for(int i=0;i<Rotate_Directions_size;i++){
			rotate_array[i] = &(Rotate_Directions[i]);
			Rotate_Directions[i].RMS_value = ipc->IPC_Get_TAG_INT32((char *)Rotate_Directions[i].RMS);
		}

		//sort four mic' RMS value(big->small).
		for(int i=0;i<Rotate_Directions_size;i++){
			for(int j=i+1;j<Rotate_Directions_size;j++){
				if(rotate_array[i]->RMS_value < rotate_array[j]->RMS_value){
					struct Rotate_Direction_Struct * tmp = rotate_array[i];
					rotate_array[i] = rotate_array[j];
					rotate_array[j] = tmp;
				}
			}
		}

		if(rotate_array[0]->RMS_value == 0) continue;
		
		//*start detect voice form a|b|c area.
		double dir_area_proportion = (double)((double)rotate_array[0]->RMS_value / (double)rotate_array[1]->RMS_value);
		char new_area = area;
		unsigned char step_byte_command = (char)0;
		
		//detect area (a, b, c).
		if((rotate_array[0]->dir == 0 && rotate_array[1]->dir == 1)||(rotate_array[0]->dir == 1 && rotate_array[1]->dir == 0)) new_area = 'a';
		if((rotate_array[0]->dir == 1 && rotate_array[1]->dir == 2)||(rotate_array[0]->dir == 2 && rotate_array[1]->dir == 1)) new_area = 'b';
		if((rotate_array[0]->dir == 2 && rotate_array[1]->dir == 3)||(rotate_array[0]->dir == 3 && rotate_array[1]->dir == 2)) new_area = 'c';
		
		//calculate steps that go to dest area.
		if((area=='a'&&new_area=='b'))step_byte_command = (char)(128+trace_rec);
		else if((area=='a'&&new_area=='c')&&(trace_rec>40))step_byte_command = (char)(128+80+(trace_rec-40));
		else if((area=='a'&&new_area=='c')&&(trace_rec<40))step_byte_command = (char)(128+80+(40-trace_rec));
		else if((area=='b'&&new_area=='a'))step_byte_command = (char)(40-trace_rec);
		else if((area=='b'&&new_area=='c'))step_byte_command = (char)(128+40+trace_rec);
		else if((area=='c'&&new_area=='a'))step_byte_command = (char)(40-trace_rec);
		else if((area=='c'&&new_area=='b'))step_byte_command = (char)(0-trace_rec);
		
		//rotate if area change.
		if(area!=new_area) {
			
			if(step_byte_command>128) trace_rec -= (step_byte_command-128+1);
			else trace_rec += (step_byte_command+1);
			
			if(ROTATER_RUN) {
				ManeaSteperWriteByte(arduino, step_byte_command);
				ManeaSteperReadBytes(arduino);
			}
			
			printf("[v_area_move] %c->%c, steps : %4d, rec : %4d\n", area, new_area, step_byte_command, trace_rec);
		}
		area = new_area;
		
		//*start detect face location form voice form.
		step_byte_command = (char)0;
		int face_x = Face_xPoint_GetbyCamera(camera, classifier, cam_w, cam_h);
		if(face_x > 0){
			int cameraSteps_onePix = cam_w/40;
			
			//if face locat in camera left area, turn anticlockwise(step_command > 128).
			if(face_x > (cam_w/2)){
				step_byte_command += ((face_x-(cam_w/2))/cameraSteps_onePix);
			}else {
				step_byte_command += 128;
				step_byte_command += (((cam_w/2)-face_x)/cameraSteps_onePix);
			}
			
			//rotater
			int rotater_threshold = 2;
			
			if(((step_byte_command < rotater_threshold)&&(step_byte_command >= 0)) ||
				((step_byte_command < (128+rotater_threshold))&&(step_byte_command >= 128))){
				step_byte_command = 0;
			}
			else if(ROTATER_RUN) {
				ManeaSteperWriteByte(arduino, step_byte_command);
				ManeaSteperReadBytes(arduino);
				
				//add trace record.
				if(step_byte_command>128) trace_rec -= (step_byte_command-128+1);
				else trace_rec += (step_byte_command+1);
			}
			
			printf("[face_x_move] face_x : %d, steps : %4d, rec : %4d\n", face_x, step_byte_command, trace_rec);
		}
	
        runCount++;
    }

	if(ROTATER_RUN) {
		printf("\n[FaceCap] : end rotate step %d\n", trace_rec);
		if(trace_rec>0)ManeaSteperWriteByte(arduino, (char)(128+trace_rec-1));
		else ManeaSteperWriteByte(arduino, (char)(0-trace_rec-1));
	}
	
    printf("\nFaceCap : exit.\n");
    ProcessShutdownHook(0);
    return 0;
}