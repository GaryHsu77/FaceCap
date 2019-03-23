LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


OPENCV_CAMERA_MODULES:=on
OPENCV_INSTALL_MODULES:=on

OPENCV_LIB_TYPE :=STATIC
ifeq ("$(wildcard $(OPENCV_MK_PATH))","")
include ..\native\jni\OpenCV.mk
else
include $(OPENCV_MK_PATH)
endif
					
LOCAL_MODULE := FaceCap

#FACE_RECOGNIZER###################################################################
FACE_RECOGNIZER := face/facerec.cpp \
				   face/predict_collector.cpp \
				   face/mindist_predict_collector.cpp \
				   face/eigen_faces.cpp \
				   face/facerec_main.cpp \
				   face/face_filter.cpp \
				   face/fisher_faces.cpp \
				   face/lbph_faces.cpp
				   
#IPC###############################################################################
IPC := IPC/VoiceText_IPC.cpp \
	   IPC/IPC.cpp \
	   IPC/IPC_android_util.cpp

#Network###########################################################################
NET := network/NetIO.cpp  
	   
#PEGA_WMI##########################################################################
PEGA_WMI := pegawmi/PEGA-WMI-Util.c \
		    pegawmi/PEGA-LED.c
			
#LINUX_CAMERA######################################################################
LINUX_CAMERA := camera/CAMERA_DEV.cpp

#TTYACM_UTIL_UTIL######################################################################
TTYACM_UTIL := ttyacm/TTYACM_DEV.cpp

LOCAL_SRC_FILES += FaceCap.cpp $(FACE_RECOGNIZER) $(IPC) $(PEGA_WMI) $(LINUX_CAMERA) $(TTYACM_UTIL)

LOCAL_LDLIBS +=  -lm -llog

include $(BUILD_EXECUTABLE)