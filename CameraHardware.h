//<MStar Software>
//******************************************************************************
// MStar Software
// Copyright (c) 2010 - 2014 MStar Semiconductor, Inc. All rights reserved.
// All software, firmware and related documentation herein ("MStar Software") are
// intellectual property of MStar Semiconductor, Inc. ("MStar") and protected by
// law, including, but not limited to, copyright law and international treaties.
// Any use, modification, reproduction, retransmission, or republication of all
// or part of MStar Software is expressly prohibited, unless prior written
// permission has been granted by MStar.
//
// By accessing, browsing and/or using MStar Software, you acknowledge that you
// have read, understood, and agree, to be bound by below terms ("Terms") and to
// comply with all applicable laws and regulations:
//
// 1. MStar shall retain any and all right, ownership and interest to MStar
//    Software and any modification/derivatives thereof.
//    No right, ownership, or interest to MStar Software and any
//    modification/derivatives thereof is transferred to you under Terms.
//
// 2. You understand that MStar Software might include, incorporate or be
//    supplied together with third party's software and the use of MStar
//    Software may require additional licenses from third parties.
//    Therefore, you hereby agree it is your sole responsibility to separately
//    obtain any and all third party right and license necessary for your use of
//    such third party's software.
//
// 3. MStar Software and any modification/derivatives thereof shall be deemed as
//    MStar's confidential information and you agree to keep MStar's
//    confidential information in strictest confidence and not disclose to any
//    third party.
//
// 4. MStar Software is provided on an "AS IS" basis without warranties of any
//    kind. Any warranties are hereby expressly disclaimed by MStar, including
//    without limitation, any warranties of merchantability, non-infringement of
//    intellectual property rights, fitness for a particular purpose, error free
//    and in conformity with any international standard.  You agree to waive any
//    claim against MStar for any loss, damage, cost or expense that you may
//    incur related to your use of MStar Software.
//    In no event shall MStar be liable for any direct, indirect, incidental or
//    consequential damages, including without limitation, lost of profit or
//    revenues, lost or damage of data, and unauthorized system use.
//    You agree that this Section 4 shall still apply without being affected
//    even if MStar Software has been modified by MStar in accordance with your
//    request or instruction for your use, except otherwise agreed by both
//    parties in writing.
//
// 5. If requested, MStar may from time to time provide technical supports or
//    services in relation with MStar Software to you for your use of
//    MStar Software in conjunction with your or your customer's product
//    ("Services").
//    You understand and agree that, except otherwise agreed by both parties in
//    writing, Services are provided on an "AS IS" basis and the warranty
//    disclaimer set forth in Section 4 above shall apply.
//
// 6. Nothing contained herein shall be construed as by implication, estoppels
//    or otherwise:
//    (a) conferring any license or right to use MStar name, trademark, service
//        mark, symbol or any other identification;
//    (b) obligating MStar or any of its affiliates to furnish any person,
//        including without limitation, you and your customers, any assistance
//        of any kind whatsoever, or any information; or
//    (c) conferring any license or right under any intellectual property right.
//
// 7. These terms shall be governed by and construed in accordance with the laws
//    of Taiwan, R.O.C., excluding its conflict of law rules.
//    Any and all dispute arising out hereof or related hereto shall be finally
//    settled by arbitration referred to the Chinese Arbitration Association,
//    Taipei in accordance with the ROC Arbitration Law and the Arbitration
//    Rules of the Association by three (3) arbitrators appointed in accordance
//    with the said Rules.
//    The place of arbitration shall be in Taipei, Taiwan and the language shall
//    be English.
//    The arbitration award shall be final and binding to both parties.
//
//******************************************************************************
//<MStar Software>

#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <utils/threads.h>
#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/IGraphicBufferProducer.h>
#include "CameraUtility.h"
#include "CameraHal.h"
#include "V4L2Camera.h"
#include "AitUVC.h"
#include "JpegDecoder.h"
#include "CameraBuffer.h"
#include <utils/Singleton.h>

#ifdef CAMERA_ENABLE_CMA
#include <drvCMAPool.h>
#endif

namespace android {

#define VIDEO_DEVICE            "/dev/video0"

#define MIN_WIDTH               640
#define MIN_HEIGHT              480

#define MIN_THUMBNAIL_WIDTH               160
#define MIN_THUMBNAIL_HEIGHT              120

#define WIDTH_720P               1280
#define HEIGHT_720P              720

#define WIDTH_1080P              1920
#define HEIGHT_1080P             1080

#define CAMHAL_GRALLOC_USAGE    GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER \
								| GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_NEVER
#define TMP_JPEG                "/var/tmp/media/tmp.jpg"

#define AIT_DEVICE_8431         "AIT 8431"
#define AIT_DEVICE_8589         "Letv 3M Camera"
#define VIDEO_BUF_DEMUX     false
#define YUV_DATA_ONLY       false

#define CAMERA_NUMBER       3
/**
   Implementation of the Android Camera hardware abstraction layer

   This class implements the interface methods defined in CameraHardwareInterface

*/ //, public Singleton<CameraHardware>

class CameraHardware : //public Singleton<CameraHardware>,
                       public CameraHal {
//friend class Singleton<CameraHardware>;
public:

    /** Set the notification and data callbacks */
    virtual void        setCallbacks(camera_notify_callback notify_cb,
                                     camera_data_callback data_cb,
                                     camera_data_timestamp_callback data_cb_timestamp,
                                     camera_request_memory get_memory,
                                     void *user, int cameraid);
    virtual void        setCallbacks(camera_notify_callback notify_cb,
                                     camera_data_callback data_cb,
                                     camera_data_timestamp_callback data_cb_timestamp,
                                     camera_request_memory get_memory,
                                     void *user)
    {
        return;
    }

    /**
     * The following three functions all take a msgtype,
     * which is a bitmask of the messages defined in
     * include/ui/Camera.h
     */

    /**
     * Enable a message, or set of messages.
     */
    virtual void        enableMsgType(int32_t msgType, int cameraid);
    virtual void        enableMsgType(int32_t msgType)
    {
        return;
    }

    /**
     * Disable a message, or a set of messages.
     */
    virtual void        disableMsgType(int32_t msgType, int cameraid);
    virtual void        disableMsgType(int32_t msgType)
    {
        return;
    }

    /**
     * Query whether a message, or a set of messages, is enabled.
     * Note that this is operates as an AND, if any of the messages
     * queried are off, this will return false.
     */
    virtual int         msgTypeEnabled(int32_t msgType, int cameraid);
    virtual int         msgTypeEnabled(int32_t msgType)
    {
        return msgType;
    }

    /**
     * Start preview mode.
     */
    virtual int         startPreview(int cameraid);
    virtual int         startPreview()
    {
        return 0;
    }

    /**
     * Only used if overlays are used for camera preview.
     */
    virtual int         setPreviewWindow(struct preview_stream_ops *window, int cameraid);
    virtual int         setPreviewWindow(struct preview_stream_ops *window)
    {
        return NO_ERROR;
    }

    /**
     * Stop a previously started preview.
     */
    virtual void        stopPreview(int cameraid);
    virtual void        stopPreview()
    {
        return;
    }

    /**
     * Returns true if preview is enabled.
     */
    virtual bool        previewEnabled(int cameraid);
    virtual bool        previewEnabled()
    {
        return 0;
    }

    /**
     * Start record mode. When a record image is available a CAMERA_MSG_VIDEO_FRAME
     * message is sent with the corresponding frame. Every record frame must be released
     * by calling releaseRecordingFrame().
     */
    virtual int         startRecording(int cameraid);
    virtual int         startRecording()
    {
        return NO_ERROR;
    }

    /**
     * Stop a previously started recording.
     */
    virtual void        stopRecording(int cameraid);
    virtual void        stopRecording()
    {
        return;
    }

    /**
     * Returns true if recording is enabled.
     */
    virtual int         recordingEnabled(int cameraid);
    virtual int         recordingEnabled()
    {
        return 0;
    }

    /**
     * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
     */
    virtual void        releaseRecordingFrame(const void *opaque);

    /**
     * Start auto focus, the notification callback routine is called
     * with CAMERA_MSG_FOCUS once when focusing is complete. autoFocus()
     * will be called again if another auto focus is needed.
     */
    virtual int         autoFocus();

    /**
     * Cancels auto-focus function. If the auto-focus is still in progress,
     * this function will cancel it. Whether the auto-focus is in progress
     * or not, this function will return the focus position to the default.
     * If the camera does not support auto-focus, this is a no-op.
     */
    virtual int         cancelAutoFocus();

    /**
     * Take a picture.
     */
    virtual int         takePicture(int cameraid);
    virtual int         takePicture()
    {
        return 0;
    }

    /**
     * Cancel a picture that was started with takePicture.  Calling this
     * method when no picture is being taken is a no-op.
     */
    virtual int         cancelPicture(int cameraid);
    virtual int         cancelPicture()
    {
        return 0;
    }

    /** Set the camera parameters. */
    virtual int         setParameters(const char* params, int cameraid);
    virtual int         setParameters(const char* params)
    {
        return 0;
    }

    /** Return the camera parameters. */
    virtual char*       getParameters(int cameraid);
    virtual char*       getParameters()
    {
        return 0;
    }
    virtual void        putParameters(char *);

    /**
     * Send command to camera driver.
     */
    virtual int         sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

    /**
     * Release the hardware resources owned by this object.  Note that this is
     * *not* done in the destructor.
     */
    virtual void        release(int cameraid);
    virtual void        release()
    {
        return;
    }

    /**
     * Dump state of the camera hardware
     */
    virtual int         dump(int fd) const;

    virtual status_t    storeMetaDataInBuffers(bool enable);

private:
    static Mutex sLock;
    static CameraHardware* sInstance;

/*--------------------Internal Member functions - Public---------------------------------*/

public:
    /** @name internalFunctionsPublic */
    /** Constructor of CameraHal */
    CameraHardware();


    bool setCameraUsed(int cameraid, bool status);
    // Destructor of CameraHal
    virtual ~CameraHardware();

    static CameraHardware* getInstance(int cameraid);
    bool isCameraOpened();
/*--------------------Internal Member functions - Private---------------------------------*/
private:
    class PreviewThread : public Thread {
        CameraHardware* mHal;
    public:
        PreviewThread(CameraHardware* hal) :
#ifdef SINGLE_PROCESS
        // In single process mode this thread needs to be a java thread,
        // since we won't be calling through the binder.
        Thread(true),
#else
        Thread(false),
#endif
        mHal(hal) {
        }
        virtual void onFirstRef() {
            run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHal->previewThread();
            // loop until we need to quit
            return true;
        }
    };

    class DecodeThread : public Thread {
        CameraHardware* mHal;
    public:
        DecodeThread(CameraHardware* hal) :
#ifdef SINGLE_PROCESS
        // In single process mode this thread needs to be a java thread,
        // since we won't be calling through the binder.
        Thread(true),
#else
        Thread(false),
#endif
        mHal(hal) {
        }
        virtual void onFirstRef() {
            run("CameraDecodeThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHal->decodeThread();
            // loop until we need to quit
            return true;
        }
    };

    class DemuxThread : public Thread {
        CameraHardware* mHal;
    public:
        DemuxThread(CameraHardware* hal) :
#ifdef SINGLE_PROCESS
        // In single process mode this thread needs to be a java thread,
        // since we won't be calling through the binder.
        Thread(true),
#else
        Thread(false),
#endif
        mHal(hal) {}
        virtual void onFirstRef() {
            run("CameraDemuxThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHal->demuxThread();
            // loop until we need to quit
            return true;
        }
    };

    void        initDip();

    /* init default parameters */
    void        initDefaultParameters();
    void        insertSupportedParams();
    void        tryCameraSupportedSize();
    /** Allocate preview buffers */
    status_t    initNativeWindow(int previewFormat, unsigned int bufferCount, int cameraid);

    /** Free preview buffers */
    status_t    freePreviewBufs();

    int         previewThread();
    int         decodeThread();
    int         demuxThread();

    static int beginPictureThread(void *cookie);
    int pictureThread();

    static int beginAutoFocusThread(void *cookie);

    int autoFocusThread();

/*----------Member variables - Public ---------------------*/

/*----------Member variables - Private ---------------------*/

private:
    mutable Mutex                   mLock;
    mutable Mutex                   mPreviewLock;
    mutable Mutex                   mPreviewEnableLock;
    mutable Mutex                   mDemuxLock;
    Mutex                           mRecordingLock;
    Mutex                           mTakePictureLock;

    CameraParameters                mParameters;
    CameraParameters                mEsParameters;

    camera_notify_callback          mNotifyCb[CAMERA_NUMBER];
    camera_data_callback            mDataCb[CAMERA_NUMBER];
    camera_data_timestamp_callback  mDataCbTimestamp[CAMERA_NUMBER];
    camera_request_memory           mRequestMemory[CAMERA_NUMBER];
    void                            *mCallbackCookie[CAMERA_NUMBER];

    int32_t                         mMsgEnabled[CAMERA_NUMBER];
    bool                            mCameraOpened;
    int32_t                         mPreviewEnabled;
    int32_t                         mRecordingEnabled;
    int32_t                         mTakePicture;
    bool                            m720pSupported;
    void*                           mPreviewBufs;
    void*                           mTakePictureBuf;
    buffer_handle_t*                mBufferHandle;
    camera_memory_t*                mTempPreviewMemory;
    camera_memory_t*                mPreviewMemory;
    camera_memory_t*                mRecordingMemory;
    camera_memory_t*                mPreviewMemory1;
    camera_memory_t*                mTempPreviewMemory1;
    camera_memory_t*                mPreviewMemorys[2];
    camera_memory_t*                mTempPreviewMemorys[2];
    camera_memory_t*                mJpegMemory;
    camera_memory_t*                mRawDataMemory;
    int                             mPreviewFrameSize;
    int                             mRecordingFrameSize;
    int                             mWidth;
    int                             mHeight;
    int                             mRecordWidth;
    int                             mRecordHeight;
    int                             mSubWidth;
    int                             mSubHeight;
    int                             mThumbnailWidth;
    int                             mThumbnailHeight;
    int                             mPictureWidth;
    int                             mPictureHeight;
    int                             dipIndex;
    int                             videoBufIndex;

    int                             mTakingId;
    static bool                     mCameraUsed[CAMERA_NUMBER];

    //dip src buffer in mmap
    MS_U32                          u32DipInputBufSize;
    MS_U32                          u32DipInputBufPA;

    V4L2Camera*                     mCamera;
    preview_stream_ops_t*           mANativeWindow[CAMERA_NUMBER];
    sp<PreviewThread>               mPreviewThread;
    sp<DecodeThread>                mDecodeThread;
    sp<DemuxThread>                 mDemuxThread;

    Rect                            mBounds;

    unsigned char*                  mH264Frame;
    unsigned int                    mH264FrameSize;
    //SkypeXUHandle                   mSkypeXuHandle;
    AitXUHandle                     mAitXuHandle;
    int                             mImageFormat;

    JpegDecoder                     mJpegDecoder;
    CameraBuffer                    mCameraBuffer;
    char                            mCameraDev[16];

    bool                            mbAIT8431;
    bool                            mbAIT8589;
    int                             mVideoBufNum;
#ifdef MFE_SUPPORT_YUV422
    uint32_t                        mMEFInputBufSize;
    uint32_t                        mMEFInputBufPA;
#endif

#ifdef CAMERA_ENABLE_CMA
    MS_BOOL mCMAUsed;
    MS_BOOL mGetMemory;
    struct CMA_Pool_Init_Param mCMAInitParam;
    struct CMA_Pool_Free_Param mCMAFreeParam;
#endif

    char *mCameraSupportedSize;
    char *mVideoSupportedSize;
    char *mHighQualityPreviewSizeForVideo;

};

}; // namespace android

#endif
