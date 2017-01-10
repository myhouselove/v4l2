/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      Author: Niels Keeman <nielskeeman@gmail.com>
 *
 */

#ifndef _V4L2CAMERA_H
#define _V4L2CAMERA_H

#define NB_BUFFER 3
 #define PROPERTY_VALUE_MAX 10



#define IMG_WIDTH 640
#define IMG_HEIGHT 480

#include <linux/videodev2.h>
#include "Thread.h"
struct buffer {
        void *                  start;
        size_t                  length;
};

struct vdIn {
    struct v4l2_capability cap;
    struct v4l2_fmtdesc FmtDesc;
    struct v4l2_format format;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    bool isStreaming;
    int width;
    int height;
    int formatIn;
    int framesizeIn;
};


typedef void (*ThreadFunc_t)();

class CameraThread : public Thread{
public:
    CameraThread(ThreadFunc_t p_func):task(p_func){ 
        start();}
    virtual ~CameraThread(){}

    virtual void run();
private:
    ThreadFunc_t task;

};

class V4L2Camera {

public:
    V4L2Camera();
    ~V4L2Camera();

    int Open (const char *device);
    void Close ();

    int Init ();
    void Uninit ();

    int StartStreaming ();
    int StopStreaming ();

    void GrabPreviewFrame ();
    char *GrabRawFrame();
    void ProcessRawFrameDone();
    //sp<IMemory> GrabJpegFrame ();
    friend void getFrameThread(V4L2Camera mobject);
    int getUVCData();
    void start();

    int setParameters(const int width, const int height, const int pixelformat);
    int tryParameters(const int width, const int height, const int pixelformat);

    char *getDeviceName();

    struct vdIn *videoIn;
    Thread *thread_get;

private:
    int fd;

    int nQueued;
    int nDequeued;
    

};



#endif
