#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "CAMERA_DEV.hpp"

static int xioctl(int fd, int request, void* arg)
{
	for (int i = 0; i < 100; i++) {
		int r = ioctl(fd, request, arg);
		if (r != -1 || errno != EINTR) return r;
	}
	return -1;
}

typedef struct
{
	uint8_t* start;
	size_t length;
} buffer_t;

typedef struct
{
	int fd;
	uint32_t width;
	uint32_t height;
	size_t buffer_count;
	buffer_t * buffers;
	buffer_t head;
	//Add by BIN
	bool first_time;
	const char *dev_name;
	const char *error;
} camera_t;

static camera_t * camera_open(const char *device, uint32_t width, uint32_t height)
{
	camera_t * camera = (camera_t *)malloc(sizeof (camera_t));

	camera->dev_name = (const char *)malloc(strlen(device)+1);
	strcpy((char *)camera->dev_name,device);
	camera->fd = open(camera->dev_name,O_RDWR | O_NONBLOCK, 0);
	camera->first_time = true;
	camera->error = NULL;

	camera->width = width;
	camera->height = height;
	camera->buffer_count = 0;
	camera->buffers = NULL;
	camera->head.length = 0;
	camera->head.start = NULL;

	return camera;
}

static void camera_init(camera_t* camera)
{
	struct v4l2_capability cap;

	if(xioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1){
		camera->error = "IOCTL failed on VIDIOC_QUERYCAP";
		return;
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
		camera->error = "No capability for capturing";
		return;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)){
		camera->error = "No capability for streaming";
		return;
	}

	struct v4l2_cropcap cropcap;
	memset(&cropcap, 0, sizeof cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if(xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0) {
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;
		if(xioctl(camera->fd, VIDIOC_S_CROP, &crop) == -1) {
			// cropping not supported
		}
	}
  
	struct v4l2_format format;
	memset(&format, 0, sizeof format);
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = camera->width;
	format.fmt.pix.height = camera->height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.field = V4L2_FIELD_NONE;
	if(xioctl(camera->fd, VIDIOC_S_FMT, &format) == -1){
		camera->error = "IOCTL failed on VIDIOC_S_FMT";
		return;
	}

	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof req);
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1){
		camera->error = "IOCTL failed on VIDIOC_REQBUFS";
		return;
	}

	camera->buffer_count = req.count;
	camera->buffers = (buffer_t *)calloc(req.count,sizeof (buffer_t));
	camera->head.length = 0;

	for (size_t i = 0; i < camera->buffer_count; i++) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (xioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1){
			camera->error = "IOCTL failed on VIDIOC_QUERYBUF";
			return;
		}
		if (buf.length > camera->head.length){
			camera->head.length = buf.length;
		}
		camera->buffers[i].length = buf.length;
		camera->buffers[i].start = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera->fd, buf.m.offset);
		if (camera->buffers[i].start == MAP_FAILED){
			camera->error = "mmap failed";
			return;
		}
	}
	camera->head.start = (uint8_t *)malloc(camera->head.length);
}

static int camera_start(camera_t* camera)
{
	for (size_t i = 0; i < camera->buffer_count; i++) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1){
			camera->error = "IOCTL failed on VIDIOC_QBUF";
			return -1;
		}
	}
  
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1){
		camera->error = "VIDIOC_STREAMON";
		return -1;
	}
	
	return 0;
}

static void camera_stop(camera_t* camera)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1){
		camera->error = "IOCTL failed on VIDIOC_STREAMOFF";
		return;
	}
}

static void camera_finish(camera_t* camera)
{
	if(camera->buffers != NULL){
		for (size_t i = 0; i < camera->buffer_count; i++) {
			if(camera->buffers[i].start != NULL){
				munmap(camera->buffers[i].start, camera->buffers[i].length);
			}
		}
		camera->buffer_count = 0;
		camera->buffers = NULL;
	}

	if(camera->buffers) free(camera->buffers);
	if(camera->head.start) free(camera->head.start);
	camera->head.length = 0;
	camera->head.start = NULL;
}

static void camera_close(camera_t* camera)
{
	if(camera->fd >= 0) close(camera->fd);
	free(camera);
}

static bool camera_capture(camera_t* camera)
{
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) return false;

	memcpy(camera->head.start, camera->buffers[buf.index].start, buf.bytesused);
	camera->head.length = buf.bytesused;

	if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) return false;

	return true;
}

static bool camera_frame(camera_t* camera)
{
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 10;

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(camera->fd, &fds);
	int r = select(camera->fd + 1, &fds, 0, 0, &timeout);
	if (r <= 0) return false;
	return camera_capture(camera);
}

CAMERA_DEV::CAMERA_DEV(const char *dev_name,int width,int height)
{
	this->camera_data = (void *)camera_open(dev_name,width,height);
	if( ((camera_t *)this->camera_data)->error != NULL ) return;

	camera_init((camera_t *)this->camera_data);
	if( ((camera_t *)this->camera_data)->error != NULL ) return;

	camera_start((camera_t *)this->camera_data);
	if( ((camera_t *)this->camera_data)->error != NULL ) return;
}

CAMERA_DEV::~CAMERA_DEV()
{
	if(this->camera_data == NULL) return;
	camera_stop((camera_t *)this->camera_data);
	camera_finish((camera_t *)this->camera_data);
	camera_close((camera_t *)this->camera_data);
	printf("[CAMERA_DEV] : close\n");
}

const unsigned char * CAMERA_DEV::Capture()
{
	if(this->camera_data == NULL) return NULL;
	//GDB >>
    camera_stop((camera_t *)this->camera_data);
	camera_start((camera_t *)this->camera_data);
	if( ((camera_t *)this->camera_data)->error != NULL ) return NULL;
	//GDB <<
	if( ((camera_t *)this->camera_data)->first_time ){
		/* skip 5 frames for booting a cam */
		for (int i = 0; i < 5; i++) {
			camera_frame(((camera_t *)this->camera_data));
		}
		//((camera_t *)this->camera_data)->first_time = false;
	}
    
	camera_frame(((camera_t *)this->camera_data));
    
	return ((camera_t *)this->camera_data)->head.start;
}
int CAMERA_DEV::Size()
{
	if(this->camera_data == NULL) return 0;

	return ((camera_t *)this->camera_data)->head.length;
}
int CAMERA_DEV::Width()
{
	if(this->camera_data == NULL) return 0;

	return ((camera_t *)this->camera_data)->width;
}
int CAMERA_DEV::Height()
{
	if(this->camera_data == NULL) return 0;

	return ((camera_t *)this->camera_data)->height;
}

const char * CAMERA_DEV::ToError()
{
	if(this->camera_data == NULL) return "No Initialization";
	return ((camera_t *)this->camera_data)->error;
}

const unsigned char* CAMERA_DEV::GetVideoBuffer()
{
	return (const unsigned char*)((camera_t *)this->camera_data)->head.start;
}

std::ostream& operator<<(std::ostream &out, CAMERA_DEV &camera)
{
	const char *tmp = (const char *)camera.Capture();
	int len = camera.Size();

	out.write(tmp,len);

	return out;
}


