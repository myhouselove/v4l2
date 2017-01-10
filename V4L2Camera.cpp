/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      Author: Niels Keeman <nielskeeman@gmail.com>
 *
 */

#define LOG_TAG "V4L2Camera"
#define COUNT_FPS 1
#define ALOG_NIDEBUG 0
#define ALOGE printf
//#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>
#include "V4L2Camera.h"
//#include "Thread.h"

int *rgb = NULL;
int *ybuf = NULL;


void CameraThread::run()
{
    this->task();
    ALOGE("here");
}


V4L2Camera::V4L2Camera ()
    :fd(0),nQueued(0), nDequeued(0)
{
    videoIn = (struct vdIn *) calloc (1, sizeof (struct vdIn));
}

V4L2Camera::~V4L2Camera()
{
    free(videoIn);
    close(fd);
}

int V4L2Camera::Open (const char *device)
{
    int ret;

    if ((fd = open(device, O_RDWR)) == -1) {
        ALOGE("ERROR opening V4L interface: %s", strerror(errno));
        return -1;
    }

    ret = ioctl (fd, VIDIOC_QUERYCAP, &videoIn->cap);
    if (ret < 0) {
        ALOGE("Error opening device: unable to query device.");
        return -1;
    }
    if ((videoIn->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        ALOGE("Error opening device: video capture not supported.");
        return -1;
    }

    if (!(videoIn->cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("Capture device does not support streaming i/o");
        return -1;
    }


    videoIn->FmtDesc.index = 0;
    videoIn->FmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ALOGE("Support format:\n");
    while(ioctl(fd, VIDIOC_ENUM_FMT, &videoIn->FmtDesc) != -1)
    {
        ALOGE("\t%d.%s\n",videoIn->FmtDesc.index+1,videoIn->FmtDesc.description);
        videoIn->FmtDesc.index++;
    }
    
    videoIn->width = 480;
    videoIn->height = 640;
    videoIn->framesizeIn = (640 * 480 << 1);
    videoIn->formatIn = V4L2_PIX_FMT_MJPEG;

    videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->format.fmt.pix.width = 480;
    videoIn->format.fmt.pix.height = 640;
    videoIn->format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    videoIn->format.fmt.pix.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(fd, VIDIOC_S_FMT, &videoIn->format);
    if (ret < 0) {
        ALOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
        return ret;
    }
    

    return fd;
}

void V4L2Camera::Close ()
{
    close(fd);
}

int V4L2Camera::Init()
{
    int ret;

    /* Check if camera can handle NB_BUFFER buffers */
    videoIn->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->rb.memory = V4L2_MEMORY_MMAP;
    videoIn->rb.count = NB_BUFFER;

    ret = ioctl(fd, VIDIOC_REQBUFS, &videoIn->rb);
    if (ret < 0) {
        ALOGE("Init: VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    for (int i = 0; i < NB_BUFFER; i++) {

        memset (&videoIn->buf, 0, sizeof (struct v4l2_buffer));

        videoIn->buf.index = i;
        videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->buf.memory = V4L2_MEMORY_MMAP;

        ret = ioctl (fd, VIDIOC_QUERYBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("Init: Unable to query buffer (%s)", strerror(errno));
            return ret;
        }

        ALOGE("allocate buffer: length: 0x%x, start: %x", videoIn->buf.length,
                videoIn->buf.m.offset);

        videoIn->mem[i] = mmap (NULL,
                                videoIn->buf.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd,
                                videoIn->buf.m.offset);

        if (videoIn->mem[i] == MAP_FAILED) {
            ALOGE("Init: Unable to map buffer (%s)", strerror(errno));
            return -1;
        }

        ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("Init: VIDIOC_QBUF Failed");
            return -1;
        }

        nQueued++;
    }

    return 0;
}

void V4L2Camera::Uninit ()
{
    int ret;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue everything */
    int DQcount = nQueued - nDequeued;

    for (int i = 0; i < DQcount-1; i++) {
        ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
        if (ret < 0)
            ALOGE("Uninit: VIDIOC_DQBUF Failed");
    }
    nQueued = 0;
    nDequeued = 0;

    /* Unmap buffers */
    for (int i = 0; i < NB_BUFFER; i++)
        if (munmap(videoIn->mem[i], videoIn->buf.length) < 0)
            ALOGE("Uninit: Unmap failed");
}

int V4L2Camera::StartStreaming ()
{
    enum v4l2_buf_type type;
    int ret;

    if (!videoIn->isStreaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (fd, VIDIOC_STREAMON, &type);
        if (ret < 0) {
            ALOGE("StartStreaming: Unable to start capture: %s", strerror(errno));
            return ret;
        }

        videoIn->isStreaming = true;
    }

    return 0;
}

int V4L2Camera::StopStreaming ()
{
    enum v4l2_buf_type type;
    int ret;

    if (videoIn->isStreaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (fd, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            ALOGE("StopStreaming: Unable to stop capture: %s", strerror(errno));
            return ret;
        }

        videoIn->isStreaming = false;
    }

    return 0;
}
void getFrameThread(V4L2Camera mobject)
{
    while(1)
        V4L2Camera::getUVCData();
    ALOGE("TEST\n");

}

int V4L2Camera::getUVCData(){

    //unsigned char *tmpBuffer;
    int ret;

    //tmpBuffer = (unsigned char *) calloc (1, videoIn->width * videoIn->height * 2);

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == ioctl (fd, VIDIOC_DQBUF, &videoIn->buf)) {
        ALOGE("StopStreaming: Unable to stop capture: %s", strerror(errno));
        return 0;
    }

    int fp;
    //ALOGE("LENGTH:%d.",);
    fp = open("pic001.jpg", O_RDWR | O_CREAT | O_TRUNC);
    if(fp == -1)
        perror("open fail");
    write(fp, videoIn->mem[videoIn->buf.index], videoIn->buf.bytesused);


    if (-1 == ioctl (fd, VIDIOC_QBUF, &videoIn->buf))
        {
            ALOGE("StopStreaming: Unable to stop capture: %s", strerror(errno));
         return 0;
     }
     sleep(2);
    //free(tmpBuffer);
     close(fp);
    return 1;

}
void V4L2Camera::start()
{
    
    thread_get = new CameraThread(getFrameThread);

    ALOGE("start()\n");
}


void yuyv422toABGRY(unsigned char *src)
{

    int width=0;
    int height=0;
int yuv_tbl_ready=0;
int y1192_tbl[256];
int v1634_tbl[256];
int v833_tbl[256];
int u400_tbl[256];
int u2066_tbl[256];


    width = IMG_WIDTH;
    height = IMG_HEIGHT;

    int frameSize =width*height*2;

    int i;

    if((!rgb || !ybuf)){
        return;
    }
    int *lrgb = NULL;
    int *lybuf = NULL;
        
    lrgb = &rgb[0];
    lybuf = &ybuf[0];

    if(yuv_tbl_ready==0){
        for(i=0 ; i<256 ; i++){
            y1192_tbl[i] = 1192*(i-16);
            if(y1192_tbl[i]<0){
                y1192_tbl[i]=0;
            }

            v1634_tbl[i] = 1634*(i-128);
            v833_tbl[i] = 833*(i-128);
            u400_tbl[i] = 400*(i-128);
            u2066_tbl[i] = 2066*(i-128);
        }
        yuv_tbl_ready=1;
    }

    for(i=0 ; i<frameSize ; i+=4){
        unsigned char y1, y2, u, v;
        y1 = src[i];
        u = src[i+1];
        y2 = src[i+2];
        v = src[i+3];

        int y1192_1=y1192_tbl[y1];
        int r1 = (y1192_1 + v1634_tbl[v])>>10;
        int g1 = (y1192_1 - v833_tbl[v] - u400_tbl[u])>>10;
        int b1 = (y1192_1 + u2066_tbl[u])>>10;

        int y1192_2=y1192_tbl[y2];
        int r2 = (y1192_2 + v1634_tbl[v])>>10;
        int g2 = (y1192_2 - v833_tbl[v] - u400_tbl[u])>>10;
        int b2 = (y1192_2 + u2066_tbl[u])>>10;

        r1 = r1>255 ? 255 : r1<0 ? 0 : r1;
        g1 = g1>255 ? 255 : g1<0 ? 0 : g1;
        b1 = b1>255 ? 255 : b1<0 ? 0 : b1;
        r2 = r2>255 ? 255 : r2<0 ? 0 : r2;
        g2 = g2>255 ? 255 : g2<0 ? 0 : g2;
        b2 = b2>255 ? 255 : b2<0 ? 0 : b2;

        *lrgb++ = 0xff000000 | b1<<16 | g1<<8 | r1;
        *lrgb++ = 0xff000000 | b2<<16 | g2<<8 | r2;

        if(lybuf!=NULL){
            *lybuf++ = y1;
            *lybuf++ = y2;
        }
    }

}

void V4L2Camera::GrabPreviewFrame ()
{
    
    unsigned char *tmpBuffer;
    int ret;

    tmpBuffer = (unsigned char *) calloc (1, videoIn->width * videoIn->height * 2);

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* DQ */
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        //ALOGE("GrabPreviewFrame: VIDIOC_DQBUF Failed");
        free(tmpBuffer);
        return;
    }
    nDequeued++;

   // memcpy (tmpBuffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);
    //yuyv422toABGRY((unsigned char *)videoIn->mem[videoIn->buf.index]);
    int fp;
    //ALOGE("LENGTH:%d.",);
    fp = open("pic001.jpg", O_RDWR | O_CREAT | O_TRUNC);
    if(fp == -1)
        perror("open fail");
    write(fp, videoIn->mem[videoIn->buf.index], videoIn->buf.bytesused);

    //convertYUYVtoYUV422SP((uint8_t *)tmpBuffer, (uint8_t *)previewBuffer, videoIn->width, videoIn->height);
    //convert((unsigned char *)tmpBuffer, (unsigned char *)previewBuffer, videoIn->width, videoIn->height);

    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabPreviewFrame: VIDIOC_QBUF Failed");
        free(tmpBuffer);
        return;
    }

    nQueued++;
    close(fp);
    free(tmpBuffer);
}
/*
char * V4L2Camera::GrabRawFrame()
{
    int ret;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabRawFrame: VIDIOC_DQBUF Failed");
        return NULL;
    }
    nDequeued++;

#if COUNT_FPS
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("mstar.camera.fps", value, "false");
    if (strcmp(value, "true") == 0) {
        static time_t start, end;
        static timeval once_start, once_end;
        static int totalframe = 0;

        if (totalframe == 0) {
            time(&start);
        }
        time(&end);
        gettimeofday(&once_end, NULL);

        ALOGI("Latency(, ) =================  %d ms", 1000*(once_end.tv_sec-once_start.tv_sec)+(once_end.tv_usec-once_start.tv_usec)/1000);
        totalframe ++;
        if (difftime (end,start) >= 1) {
            ALOGI("FPS(, ) ::::::::::::::::: %d", totalframe);
            totalframe = 0;
        }
        gettimeofday(&once_start, NULL);
    }
#endif

    return(char *)videoIn->mem[videoIn->buf.index];
}
*/
void V4L2Camera::ProcessRawFrameDone()
{
    int ret;
    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("postGrabRawFrame: VIDIOC_QBUF Failed");
        return;
    }

    nQueued++;
}
/*
sp<IMemory> V4L2Camera::GrabJpegFrame ()
{
    FILE *output;
    FILE *input;
    int fileSize;
    int ret;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabJpegFrame: VIDIOC_DQBUF Failed");
        return NULL;
    }
    nDequeued++;

    ALOGI("GrabJpegFrame: Generated a frame from capture device");

    
    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabJpegFrame: VIDIOC_QBUF Failed");
        return NULL;
    }
    nQueued++;

    output = fopen("/sdcard/tmp.jpg", "wb");

    if (output == NULL) {
        ALOGE("GrabJpegFrame: Ouput file == NULL");
        return NULL;
    }

    fileSize = saveYUYVtoJPEG((unsigned char *)videoIn->mem[videoIn->buf.index], videoIn->width, videoIn->height, output, 85);

    fclose(output);

    input = fopen("/sdcard/tmp.jpg", "rb");

    if (input == NULL)
        ALOGE("GrabJpegFrame: Input file == NULL");
    else {
        sp<MemoryHeapBase> mjpegPictureHeap = new MemoryHeapBase(fileSize);
        sp<MemoryBase> jpegmemBase = new MemoryBase(mjpegPictureHeap, 0, fileSize);

        fread((uint8_t *)mjpegPictureHeap->base(), 1, fileSize, input);
        fclose(input);

        return jpegmemBase;
    }

    return NULL;
}
*/
int V4L2Camera::tryParameters(const int width, const int height, const int pixelformat)
{
    int ret;

    ALOGE("tryParameters: TRY_FMT: %dx%d, fmt: %x", width, height, pixelformat);

    videoIn->width = width;
    videoIn->height = height;
    videoIn->framesizeIn = (width * height << 1);
    videoIn->formatIn = pixelformat;

    videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->format.fmt.pix.width = width;
    videoIn->format.fmt.pix.height = height;
    videoIn->format.fmt.pix.pixelformat = pixelformat;
    videoIn->format.fmt.pix.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(fd, VIDIOC_TRY_FMT, &videoIn->format);
    if (ret < 0) {
        ALOGE("Open: VIDIOC_TRY_FMT Failed: %s", strerror(errno));
        return ret;
    }

    if (videoIn->format.fmt.pix.width != width || videoIn->format.fmt.pix.height != height) {
        ALOGE("Camera doesn't support %dx%d", width, height);
        return -1;
    }

    return 0;
}

int V4L2Camera::setParameters(const int width, const int height, const int pixelformat)
{
    int ret;

    ALOGE("setParameters: S_FMT: %dx%d, fmt: %x", width, height, pixelformat);
    videoIn->width = width;
    videoIn->height = height;
    videoIn->framesizeIn = (width * height << 1);
    videoIn->formatIn = pixelformat;

    videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->format.fmt.pix.width = width;
    videoIn->format.fmt.pix.height = height;
    videoIn->format.fmt.pix.pixelformat = pixelformat;
    videoIn->format.fmt.pix.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(fd, VIDIOC_S_FMT, &videoIn->format);
    if (ret < 0) {
        ALOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
        return ret;
    }

    return 0;
}

char *V4L2Camera::getDeviceName()
{
    return (char*)videoIn->cap.card;
}
