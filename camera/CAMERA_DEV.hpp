#ifndef _CAMERA_DEV_HPP_
#define _CAMERA_DEV_HPP_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <iostream>

class CAMERA_DEV
{
private:
	void *camera_data;
public:
	CAMERA_DEV(const char *dev_name,int width=640,int height=480);
	~CAMERA_DEV();
	const unsigned char *Capture();
	int Size();
	int Width();
	int Height();
	const char *ToError();
	const unsigned char *GetVideoBuffer();
	
	friend std::ostream & operator<<(std::ostream &out, CAMERA_DEV &camera);
};



#endif


