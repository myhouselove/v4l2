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

#define LOG_TAG "CameraHardware"
#define LOG_NDDEBUG 0
//#define LOG_NIDEBUG 0
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <cutils/properties.h>

#include "CameraHardware.h"
#include "Exif.h"
#include <mmap.h>
#include <MsTypes.h>
#include <MsIRQ.h>
#include <MsOS.h>
#include <drvMMIO.h>
#include <drvMIU.h>
#include <api_mfe_frmformat.h>

#include <dirent.h>

#include "AitUVC.h"
#include "AitXU.h"
#include "demux.h"
#include <time.h>
#include <vector>


#define DUMP_JPEG 0
#define COUNT_FPS 0
#define EYESIGHT_FPS 1
#define DUMP_EYESIGHT_YUV  1
#define DUMP_YUV  0
#define SUPPORT_352X288_384X216_176X144_FLAG 1
#define H264 V4L2_PIX_FMT_H264
#define EYESIGHT_STREAM 1
#define MAIN_STREAM     0
#define EYESIGHT_WIDTH 640
#define EYESIGHT_HEIGHT 480

#define FOR_CAMERA_CTS 1
static int my_select(const struct dirent *dp) {
    if (strstr(dp->d_name,"video"))
        return 1;

    return 0;
}

namespace android {

/**
   @brief Constructor of CameraHardware

   Member variables are initialized here.  No allocations should be done here as we
   don't use c++ exceptions in the code.

 */

//ANDROID_SINGLETON_STATIC_INSTANCE(CameraHardware);
Mutex CameraHardware::sLock;
CameraHardware* CameraHardware::sInstance;
bool  CameraHardware::mCameraUsed[CAMERA_NUMBER];

static void switchStream(AitXUHandle fd, unsigned char number, unsigned char status) {

    unsigned char cmd_in_16[16] = {0};
    unsigned char cmd_out_16[16] = {0};

    cmd_in_16[0] = 0x06;
    cmd_in_16[1] = number;
    cmd_in_16[2] = status;

    AitXU_Mmp16Cmd(fd, cmd_in_16,cmd_out_16);
    if(!cmd_out_16[0])
        ALOGI("turn stream: %d %s, ok", number, status ? "ON":"OFF");
    else
        ALOGI("turn stream: %d %s, fail", number, status ? "ON":"OFF");

    return;
}

CameraHardware::CameraHardware()
: mParameters(),
//Singleton<CameraHardware>(),
mTakingId(0),
mCameraOpened(true),
mPreviewEnabled(0),
mRecordingEnabled(0),
mTakePicture(0),
m720pSupported(false),
mPreviewBufs(NULL),
mTakePictureBuf(NULL),
mBufferHandle(NULL),
mPreviewMemory(0),
mTempPreviewMemory(0),
mPreviewMemory1(0),
mTempPreviewMemory1(0),
mRecordingMemory(NULL),
mJpegMemory(NULL),
mRawDataMemory(NULL),
mPreviewFrameSize(0),
mRecordingFrameSize(0),
videoBufIndex(0),
mCamera(0),
mH264Frame(NULL), mH264FrameSize(0), mAitXuHandle(NULL), mJpegDecoder(), mCameraBuffer(),
mbAIT8431(false), mbAIT8589(false), mVideoBufNum(1) {
    struct dirent **namelist;
    int total = 0;
    char video_dev[8];
    int found = 0;
    status_t ret = NO_ERROR;
    int fd = -1;

    for(int i = 0; i < CAMERA_NUMBER; i++)
    {
        mANativeWindow[i] = NULL;

        mCameraUsed[i] = false;

        mNotifyCb[i] = NULL;
        mDataCb[i] = NULL;
        mDataCbTimestamp[i] = NULL;
        mRequestMemory[i] = NULL;
        mCallbackCookie[i] = NULL;
        mMsgEnabled[i] = 0;

        mPreviewMemorys[i] = NULL;
        mTempPreviewMemorys[i] = NULL;
    }

    mCameraSupportedSize = 0;
    mVideoSupportedSize = 0;
    mHighQualityPreviewSizeForVideo = 0;

    mmap_init();

    initDefaultParameters();

    if (!mCamera) {
        delete mCamera;
        mCamera = new V4L2Camera();
    }

    total = scandir("/dev", &namelist, my_select, 0);

    if ( total > 0) {
        for (int i = 0 ; i < 3; i ++) {
            snprintf(video_dev,7,"video%d",i);
            found = 0;
            for (int j = 0 ; j < total ; j ++) {
                if (strcmp(video_dev,namelist[j]->d_name) == 0) {
                    ALOGI("found camera dev = %s", namelist[j]->d_name);
                    found = 1;
                    break;
                }
            }

            if (found == 1)
                break;
        }

        while (total --)
            free(namelist[total]);

        free(namelist);
    }

    if (found == 1)
        snprintf(mCameraDev,12, "/dev/%s", video_dev);
    else
        snprintf(mCameraDev,12 ,"%s", VIDEO_DEVICE);

    fd = mCamera->Open(mCameraDev);

    if (fd == -1) {
        ALOGE("Camera device %s open failed.", mCameraDev);
        //return UNKNOWN_ERROR;
        mCameraOpened = false;
        goto OP_FAILED_1;
    }

    if (strncmp(mCamera->getDeviceName(), AIT_DEVICE_8431, 8) == 0) {
        mbAIT8431 = true;
        mVideoBufNum = NB_BUFFER;
    }

    ALOGE("camera name: %s", mCamera->getDeviceName());

    if (strncmp(mCamera->getDeviceName(), AIT_DEVICE_8589, 8) == 0) {
        mbAIT8589 = true;
        mVideoBufNum = NB_BUFFER;
    }

    tryCameraSupportedSize();

    if (m720pSupported == false) {
        mImageFormat = V4L2_PIX_FMT_YUYV;
    } else {
#if YUV_DATA_ONLY
        mImageFormat = V4L2_PIX_FMT_YUYV;
#else
        mImageFormat = V4L2_PIX_FMT_MJPEG;
#endif

        if (mbAIT8589) {

            if (mAitXuHandle) {
                AitXU_Release(&mAitXuHandle);
            }

            mAitXuHandle = AitXU_Init(mCameraDev);

            char fw_build_day[16]={0};
            unsigned char fw_ver[6]={0};

            AitXU_GetFWBuildDate(mAitXuHandle ,fw_build_day);
            AitXU_GetFWVersion(mAitXuHandle ,fw_ver);

            ALOGE("Firmware build day: %s\r\n",fw_build_day);
            ALOGE("Firmware ver: %d.%d.%d\r\n", fw_ver[1]+fw_ver[0]*0x100,
                                                fw_ver[3]+fw_ver[2]*0x100,
                                                fw_ver[5]+fw_ver[4]*0x100);

            //switchStream(mAitXuHandle, 1, 0);
            //switchStream(mAitXuHandle, 4, 0);

            ret = AitXU_SetMode(mAitXuHandle, STREAM_MODE_FRAMEBASE_H264L_MJPEG_YUY2_NV12);
            if (ret < 0) {
                ALOGE("AIT8589: Failed to set mode\r\n");
            }

            mWidth = 1280;
            mHeight = 720;

            //YUY2 main resolution
            if(mWidth == 1920 && mHeight == 1080)
                ret = AitXU_SetEncRes(mAitXuHandle, 0x0B);
            else if(mWidth == 1280 && mHeight == 720)
                ret = AitXU_SetEncRes(mAitXuHandle, 0x0A); //1280x720
            else if(mWidth == 640 && mHeight == 480)
                ret = AitXU_SetEncRes(mAitXuHandle, 0x03);
            else if(mWidth == 320 && mHeight == 240)
                ret = AitXU_SetEncRes(mAitXuHandle, 0x02);
            else if(mWidth == 320 && mHeight == 180)
                ret = AitXU_SetEncRes(mAitXuHandle, 0x13);
            else if(mWidth == 160 && mHeight == 120)
                ret = AitXU_SetEncRes(mAitXuHandle, 0x01);
            if (ret < 0) {
                ALOGE("AIT8589: Failed to select resolution \r\n");
            }
            mImageFormat = H264;
        } else {
            //init with 640x480. for eyesight gesture
            // FIXME
            mWidth = EYESIGHT_WIDTH;
            mHeight = EYESIGHT_HEIGHT;
        }
        /*
        if (mbAIT8431) {
            if (mAitXuHandle) {
                SkypeXU_Release(&mAitXuHandle);
            }
            mAitXuHandle = SkypeXU_Init(fd);
            SkypeXU_SetMode(mAitXuHandle, 2);
            ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_1280X720);
            if (ret < 0) {
                ALOGE("Failed to select resolution \r\n");
            }
        }
        */
    }
    //H264 resolution if mbAIT8589 is ture
    if(mImageFormat == H264) {
        ret = mCamera->setParameters(320, 240, mImageFormat);
    } else {
        ret = mCamera->setParameters(mWidth, mHeight, mImageFormat);
    }

    if (ret) {
        ALOGE("Camera setParameters failed: %s", strerror(errno));
        //return UNKNOWN_ERROR;
        mCameraOpened = false;
        goto OP_FAILED_2;
    }


    insertSupportedParams();

    ret = mCamera->Init();
    if (ret) {
        ALOGE("Camera Init failed: %s", strerror(errno));
        // return UNKNOWN_ERROR;
        mCameraOpened = false;
        goto OP_FAILED_3;
    }

    ret = mCamera->StartStreaming();
    if (ret) {
        ALOGE("Camera StartStreaming failed: %s", strerror(errno));
        // return UNKNOWN_ERROR;
        mCameraOpened = false;
        goto OP_FAILED_4;
    }

    mThumbnailWidth = MIN_THUMBNAIL_WIDTH;
    mThumbnailHeight = MIN_THUMBNAIL_HEIGHT;
    return;

OP_FAILED_4:
    //mCamera->StopStreaming();
    mCamera->Uninit();
OP_FAILED_3:
OP_FAILED_2:
    if (mbAIT8589 && mAitXuHandle) {
        AitXU_Release(&mAitXuHandle);
    }

    mCamera->Close();
OP_FAILED_1:
    return;
}

CameraHardware* CameraHardware::getInstance(int cameraid) {
    Mutex::Autolock _l(sLock);
    CameraHardware* instance = sInstance;
    if (instance == 0) {
        instance = new CameraHardware();
        if(instance->isCameraOpened()) {
            sInstance = instance;
            // for CTS: android.hardware.cts.CameraTest#testMultipleCameras
            if((!instance->mbAIT8589) && (cameraid == 1)) {
                instance->release(cameraid);
                delete instance;
                sInstance = NULL;
                ALOGE("not eyesight camera, camera id %d is not allowed for CTS!!!", cameraid);
                return NULL;
            }
        } else {
            delete instance;
            ALOGE("Camera is not opened!!!");
            return NULL;
        }
    } else {
        if(!instance->mbAIT8589) {
            ALOGE("Camera is used, it's not support multi stream!!!");
            return NULL;
        }
    }
    mCameraUsed[cameraid] = true;
    return instance;
}

/**
   @brief Destructor of CameraHardware

   This function simply calls deinitialize() to free up memory allocate during construct
   phase
 */
CameraHardware::~CameraHardware() {
    ALOGI("CameraHardware::~CameraHardware()");
    for(int i = 0; i < CAMERA_NUMBER; i++)
    {
        if(mCameraUsed[i] == true) {
            ALOGI("Camera used by id: %d, no delete", i);
            return;
        }
    }
    if (mCamera) {
        delete mCamera;
        mCamera = 0;
    }
    if (mCameraSupportedSize) {
        delete [] mCameraSupportedSize;
        mCameraSupportedSize = 0;
    }
    if (mVideoSupportedSize) {
        delete [] mVideoSupportedSize;
        mVideoSupportedSize = 0;
    }
    if (mHighQualityPreviewSizeForVideo) {
        delete [] mHighQualityPreviewSizeForVideo;
        mHighQualityPreviewSizeForVideo = 0;
    }
    mCameraOpened = false;
    sInstance = NULL;
}

bool CameraHardware::setCameraUsed(int cameraid, bool status)
{
    ALOGI("set Camera id: %d to %d", cameraid, status);

    mCameraUsed[cameraid] = status;

    if(status == false) {
        mNotifyCb[cameraid] = NULL;
        mDataCb[cameraid] = NULL;
        mDataCbTimestamp[cameraid] = NULL;
        mRequestMemory[cameraid] = NULL;
        mCallbackCookie[cameraid] = NULL;
        mMsgEnabled[cameraid] = 0;

        mPreviewEnabled &= ~(1<<(4*cameraid));
        mRecordingEnabled &= ~(1<<(4*cameraid));
        mTakePicture &= ~(1<<(4*cameraid));
    }

    return true;
}

bool CameraHardware::isCameraOpened() {
    return mCameraOpened;
};

/**
   @brief Open camera

   @param None
   @return NO_ERROR - On success

   @remarks Camera Hal internal function

 */

/**
  Callback function to receive orientation events from SensorListener
 */
void CameraHardware::setCallbacks(camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void *user, int cameraid)
{
    Mutex::Autolock lock(mLock);
    mNotifyCb[cameraid] = notify_cb;
    mDataCb[cameraid] = data_cb;
    mDataCbTimestamp[cameraid] = data_cb_timestamp;
    mRequestMemory[cameraid] = get_memory;
    mCallbackCookie[cameraid] = user;
}

/**
  try camera supported size
 */
void CameraHardware::tryCameraSupportedSize() {
    const char *ResolutionSize[] = {"1280x720", "640x480", "352x288", "384x216", "320x240", "176x144"};
    int count = 0;
    int ImageFormat;
    mCameraSupportedSize = new char[60];
    mVideoSupportedSize = new char[60];
    mVideoSupportedSize[0] = '\0';
    mHighQualityPreviewSizeForVideo = new char[10];
    std::vector<char *> ResolutionSizeSizeList(ResolutionSize, ResolutionSize + sizeof(ResolutionSize)/sizeof(char *));
    for (std::vector<char *>::iterator it = ResolutionSizeSizeList.begin(); it != ResolutionSizeSizeList.end(); it++) {
        char *end;
        int width = (int)strtol(*it, &end, 10);
        int height = (int)strtol(end+1, &end, 10);
        if (width == WIDTH_1080P && (!mJpegDecoder.is1080pSupport())) {
            it++;
            width = (int)strtol(*it, &end, 10);
            height = (int)strtol(end+1, &end, 10);
        }

#if YUV_DATA_ONLY
        ImageFormat = V4L2_PIX_FMT_YUYV;
#else
        if (width >= WIDTH_720P) {
            ImageFormat = V4L2_PIX_FMT_MJPEG;
        } else {
            ImageFormat = V4L2_PIX_FMT_YUYV;
        }
#endif
        if (!(mCamera->tryParameters(width, height, ImageFormat))) {
            if (count == 0) {
                mWidth = width;
                mHeight = height;
                if (width >= WIDTH_720P) {
                    m720pSupported = true;
                }
                sprintf(mHighQualityPreviewSizeForVideo, "%s", *it);
                sprintf(mCameraSupportedSize, "%s", *it);

            } else {
                sprintf(mCameraSupportedSize, "%s,%s", mCameraSupportedSize, *it);
            }

            if (mVideoSupportedSize[0] == '\0') {
                sprintf(mVideoSupportedSize, "%s", *it);
            } else {
                sprintf(mVideoSupportedSize, "%s,%s", mVideoSupportedSize, *it);
            }

            count++;
        } else {
            // always add video size 352x288 and 176x144 for CTS
            if ((width == 352 && height == 288) || (width == 176 && height == 144)) {
                if (mVideoSupportedSize[0] == '\0') {
                    sprintf(mVideoSupportedSize, "%s", *it);
                } else {
                    sprintf(mVideoSupportedSize, "%s,%s", mVideoSupportedSize, *it);
                }
            }
        }
    }
    ALOGI("Camera Supported Size list: %s", mCameraSupportedSize);
}

/**
  Set supported parameters
 */
void CameraHardware::insertSupportedParams() {
    CameraParameters &p = mParameters;
    String8 previewColorString;

    previewColorString = CameraParameters::PIXEL_FORMAT_YUV420SP;
    previewColorString.append(",");
    previewColorString.append(CameraParameters::PIXEL_FORMAT_YUV420P);
    previewColorString.append(",");
    previewColorString.append(CameraParameters::PIXEL_FORMAT_YUV422I);


    if (m720pSupported && mbAIT8431) {
        p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "352x288,384x216,320x240,176x144");
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, "352x288,384x216,320x240,176x144");
        p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, "1280x720,640x480,352x288,384x216,320x240,176x144,160x120");
        p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, "384x216");
        p.setVideoSize(WIDTH_720P, HEIGHT_720P);
        p.setPreviewSize(384, 216);
        p.setPictureSize(384, 216);
        // update video format to avc
        p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_AVC);
    } else if (mbAIT8589) {
        p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, mCameraSupportedSize);
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, mCameraSupportedSize);
        p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, mVideoSupportedSize);
        p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, mHighQualityPreviewSizeForVideo);
        p.setVideoSize(mWidth, mHeight);
        p.setPreviewSize(mWidth, mHeight);
        p.setPictureSize(mWidth, mHeight);
        // update video format to avc
        // p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_AVC);
    } else {
        p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, mCameraSupportedSize);
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, mCameraSupportedSize);
        p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, mVideoSupportedSize);
        p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, mHighQualityPreviewSizeForVideo);
        p.setVideoSize(mWidth, mHeight);
        p.setPreviewSize(mWidth, mHeight);
        p.setPictureSize(mWidth, mHeight);
    }

    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, "auto");
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, previewColorString.string());
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, "jpeg");
    p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, "auto,on,off,torch");
    //p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "30");
    p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "160x120,0x0");
    p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, "auto");
    p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, "none");
    p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "auto");
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(5000,30000)");   //(1000,33000)
    p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "5000,30000"); //"1000,33000"
    p.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "51.2");
    p.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "39.4");
    p.set(CameraParameters::KEY_ZOOM_RATIOS, "320");
    p.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "auto");
    p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "true");

    // set preview frame rate according to KEY_PREVIEW_FPS_RANGE
    int min_fps = 0, max_fps = 0;
    char supportedFrameRates[256], fps_value[10];

    p.getPreviewFpsRange(&min_fps, &max_fps);
    ALOGI("KEY_PREVIEW_FPS_RANGE %d ~ %d\n", min_fps, max_fps);
    if ((min_fps >= max_fps) || (min_fps <= 0) || (max_fps <= 0)) {
        // default 1~30fps
        min_fps = 1000;
        max_fps = 30000;
    }
    min_fps /= 1000;
    max_fps /= 1000;
    memset(supportedFrameRates, 0, sizeof(supportedFrameRates));
    for (int i = min_fps; i <= max_fps; i++) {
        sprintf(fps_value, "%d", i);
        strcat(supportedFrameRates, fps_value);
        if (i != max_fps)
            strcat(supportedFrameRates, ",");
    }
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "10,20,30");

    /*p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "");
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "");
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "");
    p.set(CameraParameters::KEY_ZOOM_RATIOS, "");
    p.set(CameraParameters::KEY_MAX_ZOOM, "");
    p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "");
    p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "");
    p.set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, "");
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "");
    p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "");
    p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "");*/
    mEsParameters = mParameters;
}

/**
  Init default parameters
 */
void CameraHardware::initDefaultParameters() {
    //#ifndef NEON_MFEALIG64
    const mmap_info_t * MmapPhyBuffInfo = NULL;
    //#endif

    CameraParameters &p = mParameters;

    mWidth = MIN_WIDTH;
    mHeight = MIN_HEIGHT;

    mPictureWidth = MIN_WIDTH;
    mPictureHeight = MIN_HEIGHT;

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(30);
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);
    p.setPictureFormat("jpeg");
    p.setPictureSize(MIN_WIDTH, MIN_HEIGHT);
    p.setVideoSize(MIN_WIDTH, MIN_HEIGHT);

    p.set(CameraParameters::KEY_JPEG_QUALITY, "100");
    p.set(CameraParameters::KEY_SCENE_MODE, "auto");
    p.set(CameraParameters::KEY_FLASH_MODE, "off");
    p.set(CameraParameters::KEY_WHITE_BALANCE, "auto");
    p.set(CameraParameters::KEY_FOCUS_MODE, "auto");
    p.set(CameraParameters::KEY_FOCUS_DISTANCES,"0.10,1.20,Infinity");
    p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420TILE);
    p.set(CameraParameters::KEY_EFFECT, "none");
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");
    p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "4");
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "-4");
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0.5");
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "160");
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "120");
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "100");
    p.set(CameraParameters::KEY_FOCAL_LENGTH, "3.43");
    p.set(CameraParameters::KEY_ANTIBANDING, "");
#if 0
    p.set(CameraParameters::KEY_EFFECT,  "");
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "");
    p.set(CameraParameters::KEY_ZOOM, "");
    p.set(CameraParameters::KEY_VIDEO_STABILIZATION, "");
    p.set(CameraParameters::KEY_FOCAL_LENGTH, "");
    p.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "");
    p.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "");
    p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "");
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "");
    p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, "");
    p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW, "");
    p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, "");
    p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, "");
    p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, "");
    p.set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, "");
#endif

#ifndef NEON_MFEALIG64
    MmapPhyBuffInfo = MApi_MMAP_GetInfo("E_MMAP_ID_VE");
    if (MmapPhyBuffInfo == NULL) {
        ALOGE("[CameraHardware] no buffer for dip input memory in mmap\n");
        return;
    }
    u32DipInputBufPA = ALIGNADDRESS(MmapPhyBuffInfo->addr, ALIGNED256);
    u32DipInputBufSize = MmapPhyBuffInfo->size - (u32DipInputBufPA - MmapPhyBuffInfo->addr);

    //avoid mmap same memory range more than once (cacheable map)
    if (MsOS_PA2KSEG0(MmapPhyBuffInfo->addr) == 0) {
        if (!MsOS_MPool_Mapping(0, MmapPhyBuffInfo->addr,  MmapPhyBuffInfo->size, 0)) {
            ALOGE("[CameraHardware]: map dip input buffer error*********************\n");
            return;
        }
    }
#endif
}

status_t CameraHardware::initNativeWindow(int previewFormat,
                                          unsigned int buffercount, int id)
{
    status_t err = NO_ERROR;
    int stride;

    // Set gralloc usage bits for window.
    err = mANativeWindow[id]->set_usage(mANativeWindow[id], CAMHAL_GRALLOC_USAGE);
    if (err != 0) {
        ALOGE("native_window_set_usage failed: %s (%d)", strerror(-err), -err);
        goto fail;
    }

    ///Set the number of buffers needed for camera preview
    err = mANativeWindow[id]->set_buffer_count(mANativeWindow[id], buffercount);
    if (err != 0) {
        ALOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err), -err);
        goto fail;
    }

    // Set window geometry
    if (mbAIT8589 && (id == EYESIGHT_STREAM)) {
        //mWidth = EYESIGHT_WIDTH;
        //mHeight = EYESIGHT_HEIGHT;
        err = mANativeWindow[id]->set_buffers_geometry(
                                                         mANativeWindow[id],
                                                         EYESIGHT_WIDTH,
                                                         EYESIGHT_HEIGHT,
                                                         previewFormat);
    }
/*
    } else if(id == 0) {
        err = mANativeWindow[id]->set_buffers_geometry(
                                                         mANativeWindow[id],
                                                         mWidth,
                                                         mHeight,
                                                         previewFormat);
*/
    else {
        err = mANativeWindow[id]->set_buffers_geometry(
                                                         mANativeWindow[id],
                                                         mWidth,
                                                         mHeight,
                                                         previewFormat);
    }

    if (err != 0) {
        ALOGE("native_window_set_buffers_geometry failed: %s (%d)", strerror(-err), -err);
        goto fail;
    }

    return err;

    fail:
    if ( ENODEV == err ) {
        ALOGE("Preview surface abandoned!");
        mANativeWindow[id] = NULL;
    }

    return err;
}

status_t CameraHardware::freePreviewBufs() {
    ALOGI("CameraHardware::freePreviewBufs");
    return NO_ERROR;
}

int PayloadParser(char *frame, uint32_t frame_size, AitH264_MediaSample *h264_sample,uint16_t* customInfoSize,void* pCustomerInfoData)
{
    memset(h264_sample,0,sizeof(AitH264_MediaSample));
    uint32_t offset = 0;

    //VDBG("frame_size = %d\r\n",frame_size);

    while(offset<frame_size)
    {
        FramebaseHeader *h = (FramebaseHeader*) (frame+offset);

        if(offset >= frame_size )
        {
            ALOGE("Parser Error, offset >= frame_size \r\n");
            return 0;
        }

        if(h->wVersion == 0xD8FF)
        {
            ALOGE("Format ID: UVC MJPEG\r\n");

            h264_sample->mjpg = (uint8_t*)frame;
            h264_sample->mjpg_size = frame_size;

            return 0;
        }

        if( offset+h->dwPlayloadSize > frame_size)
        {
#if 1
            ALOGD("wVersion: 0x%x\n",(uint16_t)h->wVersion); //total size and payload ver
            ALOGD("wHeaderLen: 0x%x\n",(uint16_t)h->wHeaderLen);
            ALOGD("wStreamType: 0x%x\n",(uint16_t)h->wStreamType);
            ALOGD("wImageWidth: 0x%x\n",(uint16_t)h->wImageWidth);
            ALOGD("wImageHeight: 0x%x\n",(uint16_t)h->wImageHeight);
            ALOGD("wFrameRate: 0x%x\n",(uint16_t)h->wFrameRate);
            //vender info
            ALOGD("dwFrameSeq: 0x%x\n",(uint32_t)h->dwFrameSeq);
            ALOGD("dwTimeStamp: 0x%x\n",(uint32_t)h->dwTimeStamp);
            ALOGD("dwFlag: 0x%x\n",(uint32_t)h->dwFlag);
            ALOGD("dwReserved: 0x%x\n",(uint32_t)h->dwReserved);
            ALOGD("dwPlayloadSize: 0x%x\n",(uint32_t)h->dwPlayloadSize);
#endif
            ALOGE("Parser Error, offset+h->dwPlayloadSize > frame_size , Stream Type = 0x%X,offse+h->dwPlayloadSize=%d\r\n"
                   ,(uint16_t)h->wStreamType
                   ,h->dwPlayloadSize);
            return 0;
        }

        *customInfoSize = h->wHeaderLen - sizeof(FramebaseHeader)+4;
        if(*customInfoSize)
            ALOGE("customInfoSize  = %d\n",*customInfoSize );

        if(!customInfoSize)
            pCustomerInfoData = ((uint8_t*)h) + (h->wHeaderLen);
        else
            pCustomerInfoData = NULL;

        switch(h->wStreamType)
        {
            case ST_H264:
                h264_sample->h264 = ((uint8_t*)h) + (h->wHeaderLen+4);
                //ALOGE("Format: H264, offset: %x, addr: %x", offset, h264_sample->h264);
                h264_sample->h264_size = h->dwReserved;
                //ALOGE("Format: H264, offset: %x, size: %x", offset, h264_sample->h264_size);
                h264_sample->h264_width = h->wImageWidth;
                h264_sample->h264_height = h->wImageHeight;
                h264_sample->h264_frame_seq = h->dwFrameSeq;
                break;
            case ST_H264_SUB:
                h264_sample->h264_2 = ((uint8_t*)h) + (h->wHeaderLen+4);
                //ALOGE("Format: H264_SUB, offset: %x, addr: %x", offset, h264_sample->h264_2);
                h264_sample->h264_2_size = h->dwReserved;
                //ALOGE("Format: H264_SUB, offset: %x, size: %x", offset, h264_sample->h264_2_size);
                h264_sample->h264_2_width = h->wImageWidth;
                h264_sample->h264_2_height = h->wImageHeight;
                h264_sample->h264_2_frame_seq = h->dwFrameSeq;
                break;
            case ST_MJPG:
                h264_sample->mjpg = ((uint8_t*)h) + (h->wHeaderLen+4);
                //ALOGE("Format: MJPG, offset: %x, addr: %x", offset, h264_sample->mjpg);
                h264_sample->mjpg_size = h->dwPlayloadSize;
                //ALOGE("Format: MJPG, offset: %x, size: %x", offset, h264_sample->mjpg_size);
                h264_sample->mjpg_width = h->wImageWidth;
                h264_sample->mjpg_height = h->wImageHeight;
                h264_sample->mjpg_frame_seq = h->dwFrameSeq;
                break;
            case ST_YUY2:
                h264_sample->yuy2 = ((uint8_t*)h) + (h->wHeaderLen+4);
                //ALOGE("Format: YUY2, offset: %x, addr: %x", offset, h264_sample->yuy2);
                h264_sample->yuy2_size = h->dwPlayloadSize;
                //ALOGE("Format: YUY2, offset: %x, size: %x", offset, h264_sample->yuy2_size);
                h264_sample->yuy2_width = h->wImageWidth;
                h264_sample->yuy2_height = h->wImageHeight;
                h264_sample->yuy2_frame_seq = h->dwFrameSeq;
                break;

            case ST_YUY2_MAIN: //arthur_tsao@20141017
                h264_sample->yuy2_m = ((uint8_t*)h) + (h->wHeaderLen+4);
                h264_sample->yuy2_m_size = h->dwPlayloadSize;
                h264_sample->yuy2_m_width = h->wImageWidth;
                h264_sample->yuy2_m_height = h->wImageHeight;
                h264_sample->yuy2_m_frame_seq = h->dwFrameSeq;
            break;

            case ST_NV12: //arthur_tsao@20141017
                h264_sample->nv12 = ((uint8_t*)h) + (h->wHeaderLen+4);
                h264_sample->nv12_size = h->dwPlayloadSize;
                h264_sample->nv12_width = h->wImageWidth;
                h264_sample->nv12_height = h->wImageHeight;
                h264_sample->nv12_frame_seq = h->dwFrameSeq;
            break;

            default:
                ALOGE("Unknown Format ID %d \r\n",h->wStreamType);
                break;
        }

        offset += h->wHeaderLen + 4 + h->dwPlayloadSize;
    }
    return 0;
}

int CameraHardware::demuxThread()
{
    if (mPreviewEnabled && mImageFormat == H264
        &&( mANativeWindow[0] != NULL || mANativeWindow[1] != NULL || mRecordingEnabled)) {
        //allocate frame buffer
        //MediaSample is a sample image buffer class designed AIT we use it to receive the image frame from V4l2Capture
        //MediaSample v4l2_sample;
        Mutex::Autolock demuxlock(mDemuxLock);
        AitH264_MediaSample media_sample;
        memset(&media_sample,0,sizeof(AitH264_MediaSample));

        void *buf = NULL, *buf1 = NULL;

        if (mPreviewEnabled & (1<<(4*MAIN_STREAM))) {
            buf = mJpegDecoder.GetEmptyYuvBuffer();
            if (buf == NULL) {
                ALOGE("%s: GetEmptyYuvBuffer failed, buf= %x", __func__, buf);
            }
        }
        if (mRecordingEnabled || (mPreviewEnabled & (1<<(4*EYESIGHT_STREAM)))) {
            buf1 = mCameraBuffer.GetEmptyYuvBuffer();
            if (buf1 == NULL) {
                ALOGE("%s: GetEmptyYuvBuffer failed, buf1= %x", __func__, buf1);
            }
        }

        //ALOGV("%s: GetEmptyYuvBuffer, buf= %x, buf1= %x", __func__, buf, buf1);

        char *rawFramePointer = mCamera->GrabRawFrame();
        //ALOGV("%s: GrabRawFrame() return", __func__);
        if (rawFramePointer) {
            uint16_t i;
            char* p;

            //ALOGE("buffer Size = 0x%X\r\n", mCamera->videoIn->buf.bytesused);//arthur_tsao@20130604
            PayloadParser(rawFramePointer, mCamera->videoIn->buf.bytesused, &media_sample, &i, (void*)p);
            if((mRecordingEnabled || (mPreviewEnabled & (1<<(4*EYESIGHT_STREAM)))) && media_sample.yuy2)
            {
                static uint32_t yuy2_fseq = 0;
                if((yuy2_fseq+1)!= media_sample.yuy2_frame_seq)
                    ALOGE("frame discarded, yuy2 seq number = %d %d\r\n",yuy2_fseq,media_sample.yuy2_frame_seq);
                yuy2_fseq = media_sample.yuy2_frame_seq;

                /*
                if (F_Calculator[3] == NULL)
                {
                    F_Calculator[3] = new FpsCalculator;
                    B_Calculator[3] = new BpsCalculator;
                    Sid_Array[Sid_Index++] = 3;
                }
                else if (F_Calculator[3])
                {
                    F_Calculator[3]->IncFrmCnt();
                    B_Calculator[3]->IncDataCnt(media_sample.yuy2_size);
                }
                */
                //ALOGV("%s: YUY2 Size = 0x%X\r\n", __func__, media_sample.yuy2_size);//arthur_tsao@20130604
                if(buf1) {
                    memcpy(buf1, media_sample.yuy2, media_sample.yuy2_size);
                    mCameraBuffer.SetYuvBufferState(buf1, false);
                    //ALOGV("%s: mCameraBuffer SetYuvBufferState, buf: %x false", __func__, buf1);
                } else {
                    ALOGE("%s: get buf failed, discard one YUY2 frame", __func__);
                }
            }

            if(media_sample.h264) {
                //ALOGV("%s: H264 Size = 0x%X", __func__, media_sample.h264_size);
            }

            if(media_sample.h264_2) {
                //ALOGV("%s: H264_2 Size = 0x%X", __func__, media_sample.h264_2_size);
            }

            if((mPreviewEnabled & (1<<(4*MAIN_STREAM))) && media_sample.mjpg) {
                //ALOGV("%s: MJPG Size = 0x%X", __func__, media_sample.mjpg_size);
                if (mJpegDecoder.Decode(NULL, media_sample.mjpg, media_sample.mjpg_size, buf))
                    mJpegDecoder.SetYuvBufferState(buf, false);
            }

            if((mPreviewEnabled & (1<<(4*MAIN_STREAM))) && media_sample.yuy2_m)
            {
                //ALOGV("%s: yuy2_m size = 0x%X", __func__, media_sample.yuy2_m_size);
                if(buf) {
                    memcpy(buf, media_sample.yuy2_m, media_sample.yuy2_m_size);
                    mJpegDecoder.SetYuvBufferState(buf, false);
                    //ALOGV("%s: mJpegDecoder SetYuvBufferState, buf: %x false", __func__, buf);
                }
            }

            if(media_sample.nv12)
            {
                //memcpy(buf, media_sample.nv12, media_sample.nv12_size);
                //ALOGV("%s: nv12 size = 0x%X", __func__, media_sample.nv12_size);

            }

            /*
            if (mJpegDecoder.Decode(mPreviewBufs, rawFramePointer, mCamera->videoIn->buf.bytesused, buf))
                mJpegDecoder.SetYuvBufferState(buf, false);
                */
            mCamera->ProcessRawFrameDone();
        } else if (!rawFramePointer) {
            ALOGE("%s: Got EMPTY raw data !!!!!", __func__);
            /*
            if((mMsgEnabled & CAMERA_MSG_ERROR) && mNotifyCb) {
                //mNotifyCb(CAMERA_MSG_ERROR, 0, 0, mCallbackCookie);
                ALOGE("%s: after notify callback, CAMERA_MSG_ERROR", __func__);
            }
            */
            usleep(1000 * 10 );
            return UNKNOWN_ERROR;
        }
    } else {
        usleep(1000 * 50 );
    }

    return NO_ERROR;
}

int CameraHardware::decodeThread() {
    if (mPreviewEnabled && mImageFormat == V4L2_PIX_FMT_MJPEG
        &&( mANativeWindow[0] != NULL || mANativeWindow[1] != NULL || mRecordingEnabled)) {
        void* buf = mJpegDecoder.GetEmptyYuvBuffer();

        if (buf == NULL)
            return NO_ERROR;

        char *rawFramePointer = mCamera->GrabRawFrame();
        if (rawFramePointer) {
            if (mJpegDecoder.Decode(mPreviewBufs, rawFramePointer, mCamera->videoIn->buf.bytesused, buf))
                mJpegDecoder.SetYuvBufferState(buf, false);
        } else if (!rawFramePointer) {
            ALOGE("decodeThread: Got EMPTY raw data !!!!!");
            /*
            for(int i = 0; i < CAMERA_NUMBER; i++)
            {
                if((mMsgEnabled[i] & CAMERA_MSG_ERROR) && mNotifyCb[i]) {
                    mNotifyCb[i](CAMERA_MSG_ERROR, 0, 0, mCallbackCookie[i]);
                    ALOGE("%s: after notify callback, CAMERA_MSG_ERROR", __func__);
                }
            }
            */
            usleep(1000 * 10 );
            return UNKNOWN_ERROR;
        }

#if DUMP_JPEG
        char fname[50];
        FILE *file;
        static int nCapture = 0;
        sprintf(fname, "/var/tmp/media/capture-%06u.jpg", nCapture);
        nCapture++;

        if (nCapture >= 5)
            nCapture = 0;
        file = fopen(fname, "wb");
        if (file != NULL) {
            fwrite(rawFramePointer, mCamera->videoIn->buf.bytesused, 1, file);
            fclose(file);
        }
#endif

        mCamera->ProcessRawFrameDone();
    } else {
        usleep(1000 * 50 );
    }

    return NO_ERROR;
}

int CameraHardware::previewThread() {
    bool postPreviewFrameMsgs[CAMERA_NUMBER] = {false, false, false};
    int  postVideoFrameMsg = 0;

    {
        Mutex::Autolock lock(mPreviewLock);
        if (mPreviewEnabled && (mANativeWindow[0]!=NULL || mRecordingEnabled || mANativeWindow[1]!=NULL)) {
            status_t err;
            int stride;
            GraphicBufferMapper &mapper = GraphicBufferMapper::get();
            char *rawFramePointer = NULL;
            char *rawFramePointer1 = NULL;
            unsigned char *yuvFramePointer = NULL;
            char *rawFramePointers[2] = {NULL, NULL};

            //ALOGV("%s:                   get lock", __func__);
            if (mImageFormat == V4L2_PIX_FMT_MJPEG) {
                rawFramePointer = (char *)mJpegDecoder.GetYuvBuffer();//rbRead(&mYUVBufs, (void**)(&rawFramePointer));
                rawFramePointers[0] = rawFramePointer;
                rawFramePointers[1] = rawFramePointer;
            } else if (mImageFormat == H264) {
                if (!mTakePicture && (mPreviewEnabled & (1<<(4*MAIN_STREAM))))
                    rawFramePointer = (char *)mJpegDecoder.GetYuvBuffer();
                if (mRecordingEnabled || (mPreviewEnabled & (1<<(4*EYESIGHT_STREAM))))
                    rawFramePointer1 = (char *)mCameraBuffer.GetYuvBuffer();

	        rawFramePointers[EYESIGHT_STREAM] = rawFramePointer1;
	        rawFramePointers[MAIN_STREAM] = rawFramePointer;
            } else {
                rawFramePointer = mCamera->GrabRawFrame();
                //ALOGV("%s: GrabRawFrame() return", __func__);
                rawFramePointers[0] = rawFramePointer;
                rawFramePointers[1] = rawFramePointer;
            }

            for(int i = 0; i < CAMERA_NUMBER; i++)
            {
                if (rawFramePointers[i] && mANativeWindow[i] != NULL) {

                    if (!(mPreviewEnabled & (1<<(4*i)))) {
                        //ALOGV("%s: camera %d: preview is not enabled", __func__, i);
                        continue;
                    }

                    err = mANativeWindow[i]->dequeue_buffer(mANativeWindow[i], (buffer_handle_t**) &mBufferHandle, &stride);
                    if (err != 0) {
                        ALOGE("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
                        mCamera->ProcessRawFrameDone();
                        return UNKNOWN_ERROR;
                    }

                    mANativeWindow[i]->lock_buffer(mANativeWindow[i], mBufferHandle);
                    if(mapper.lock(*mBufferHandle, CAMHAL_GRALLOC_USAGE, mBounds, &mPreviewBufs) != NO_ERROR)
                    {
                        ALOGE("FAILED to lock buffer!!!!!!!!!!!!!!");
                        mANativeWindow[i]->enqueue_buffer(mANativeWindow[i], mBufferHandle);
                        mCamera->ProcessRawFrameDone();
                        return UNKNOWN_ERROR;
                    }

                    if (mImageFormat == V4L2_PIX_FMT_MJPEG || mImageFormat == H264) {
                        if(mbAIT8589 && (i == EYESIGHT_STREAM)) {
                            memcpy(mPreviewBufs, rawFramePointers[i], EYESIGHT_WIDTH*EYESIGHT_HEIGHT*2);
                        } else {
                            memcpy(mPreviewBufs, rawFramePointers[i], mWidth * mHeight *2);
                        }
                        yuvFramePointer = (unsigned char *) rawFramePointers[i];
                    } else {
#if SUPPORT_352X288_384X216_176X144_FLAG
                    //ALOGI("mCamera->videoIn->buf.bytesused = %d",mCamera->videoIn->buf.bytesused);
                    //ALOGI("set this %d*%d", mWidth,mHeight);
                    if (mWidth*mHeight*2 <= mCamera->videoIn->buf.bytesused) {
                        memset(mTempPreviewMemorys[i]->data, 0, mWidth*mHeight*2);
                        memcpy(mTempPreviewMemorys[i]->data, rawFramePointer, mWidth*mHeight*2);
                        memcpy(mPreviewBufs, rawFramePointer, mWidth * mHeight *2);
                    } else if (mWidth*mHeight*2 > mCamera->videoIn->buf.bytesused) {
                        memset(mTempPreviewMemorys[i]->data, 0, mWidth*mHeight*2);
                        memcpy(mTempPreviewMemorys[i]->data, rawFramePointer, mCamera->videoIn->buf.bytesused);
                        memcpy(mPreviewBufs, rawFramePointer, mCamera->videoIn->buf.bytesused);
                    }
                    rawFramePointer = (char*)mTempPreviewMemorys[i]->data;
                    yuvFramePointer = (unsigned char *) rawFramePointer;
#else
                    memset(mTempPreviewMemorys[i]->data, 0, mWidth*mHeight*2);
                    memcpy(mTempPreviewMemorys[i]->data, rawFramePointer, mCamera->videoIn->buf.bytesused);
                    memcpy(mPreviewBufs, rawFramePointer, mWidth * mHeight *2);
                    rawFramePointer = (char*)mTempPreviewMemorys[i]->data;
                    yuvFramePointer = (unsigned char *) rawFramePointer;
#endif
                }

                    err = mANativeWindow[i]->enqueue_buffer(mANativeWindow[i], mBufferHandle);
                    if (err != 0) {
                        ALOGE("Surface::queueBuffer returned error %s (%d)", strerror(-err), -err);
                        mapper.unlock(*mBufferHandle);
                        mCamera->ProcessRawFrameDone();
                        return UNKNOWN_ERROR;
                    }
                    mapper.unlock(*mBufferHandle);

                    if (mMsgEnabled[i] & CAMERA_MSG_PREVIEW_FRAME) {
                        //ALOGV("cameraid: %d, MsgEnabled: CAMERA_MSG_PREVIEW_FRAME", i);
                        if(mbAIT8589 && (i == EYESIGHT_STREAM)) {
                            if (strcmp(mEsParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420SP) == 0 ||
                                strcmp(mEsParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ) {

#ifdef __ARM_NEON__
                                yuv422tonv21((unsigned char *) yuvFramePointer, (unsigned char *) mPreviewMemorys[i]->data, EYESIGHT_WIDTH, EYESIGHT_HEIGHT);
#else
                                yuyv422_to_yuv420sp((unsigned char *) yuvFramePointer, (unsigned char *) mPreviewMemorys[i]->data, EYESIGHT_WIDTH, EYESIGHT_HEIGHT);
#endif
                            } else {
                                memcpy(mPreviewMemorys[i]->data, yuvFramePointer, EYESIGHT_WIDTH*EYESIGHT_HEIGHT*2);
                            }
                            //mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewMemory, 0, NULL, mCallbackCookie);
                        } else {
                            if (strcmp(mParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {

#ifdef __ARM_NEON__
                                yuv422tonv21((unsigned char *) yuvFramePointer, (unsigned char *) mPreviewMemorys[i]->data, mWidth, mHeight);
#else
                                yuyv422_to_yuv420sp((unsigned char *) yuvFramePointer, (unsigned char *) mPreviewMemorys[i]->data, mWidth, mHeight);
#endif
                            } else if (strcmp(mParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ) {
                                yuyv422_to_yuv420((unsigned char *) yuvFramePointer, (unsigned char *) mPreviewMemorys[i]->data, mWidth, mHeight);
                            } else {
                                memcpy(mPreviewMemorys[i]->data, yuvFramePointer, mWidth*mHeight*2);
                            }
                        }
                        postPreviewFrameMsgs[i] = true;
                    }
                } else if (!rawFramePointers[i] && mANativeWindow[i] != NULL) {
                    usleep(1000 * 5);
                    if (mImageFormat == V4L2_PIX_FMT_YUYV) {
                        //ALOGE("%s: Got EMPTY raw data, id: %d !!!!!", __func__, i);
                        return UNKNOWN_ERROR;
                    } else if (mImageFormat == V4L2_PIX_FMT_MJPEG) {
                        //ALOGV("%s: Got EMPTY raw data, format: MJPEG, id: %d !!!!!", __func__, i);
                        return UNKNOWN_ERROR;
                    }
                }
            }

#if DUMP_YUV
            static bool isFirst = true;
            FILE *file;

            if (isFirst) {
                file = fopen("/var/tmp/media/local_h264.bin", "wb");
                isFirst = false;
            } else {
                file = fopen("/var/tmp/media/local_h264.bin", "ab");
            }

            if (file != NULL) {
                fwrite(rawFramePointer, mWidth * mHeight *2, 1, file);
                fclose(file);
            }
#endif

#if COUNT_FPS
            static time_t start, end;
            static int totalframe = 0;

            if (totalframe == 0)
                time(&start);
            time(&end);

            totalframe ++;
            if (difftime (end,start) >= 1) {
                ALOGI("FPS(%d, %d) ::::::::::::::::: %d", mWidth, mHeight, totalframe);
                totalframe = 0;
            }
#endif

            // video frame
            for(int i = 0; i < CAMERA_NUMBER; i++)
            {
                //just main stream can be recording
                if(i != MAIN_STREAM) {
#ifdef FOR_CAMERA_CTS
                    if ((mMsgEnabled[i] & CAMERA_MSG_VIDEO_FRAME) && (mRecordingEnabled != 0)
                            && yuvFramePointer != NULL) {
                        memcpy((MS_U8*)MsOS_MPool_PA2KSEG1(mMEFInputBufPA), yuvFramePointer, EYESIGHT_WIDTH*EYESIGHT_HEIGHT*2);
                        MsOS_FlushMemory();

                        MS_MfeFrameInfo_t* pFrameInfo = (MS_MfeFrameInfo_t*)(mRecordingMemory->data + mRecordingFrameSize * videoBufIndex);
                        memset(pFrameInfo, 0, sizeof(MS_MfeFrameInfo_t));

                        pFrameInfo->advbuf.colorformat = MFE_API_MFE_COLORFORMAT_YUYV;
                        pFrameInfo->starcode[3] = 0x01;
                        pFrameInfo->starcode[4] = 0x47;
                        pFrameInfo->inbuf.Cur_PhyY0 = (unsigned int)mMEFInputBufPA;  //start address of y
                        pFrameInfo->inbuf.Cur_VirY0 = (unsigned char*)MsOS_MPool_PA2KSEG1(mMEFInputBufPA);

                        mDataCbTimestamp[EYESIGHT_STREAM](systemTime(), CAMERA_MSG_VIDEO_FRAME, mRecordingMemory, videoBufIndex, mCallbackCookie[EYESIGHT_STREAM]);
                        videoBufIndex = (videoBufIndex+1)%mVideoBufNum;
                    }
#endif
                    continue;
                }

                Mutex::Autolock lock(mRecordingLock);
                if ((mMsgEnabled[i] & CAMERA_MSG_VIDEO_FRAME) && mRecordingEnabled != 0) {
                    ALOGV("%s: MsgEnabled: CAMERA_MSG_VIDEO_FRAME", __func__);
                    //if (mbAIT8431 | mbAIT8589)
                    if (mbAIT8431) {
                        MS_Bypass_FrameInfo_t* pFrameInfo = (MS_Bypass_FrameInfo_t*)(mRecordingMemory->data + mRecordingFrameSize*videoBufIndex);
                        pFrameInfo->mXuHandle = (void *)mAitXuHandle;
                        pFrameInfo->nStreamType = AIT_H264_DUMMY_FRAME;
#if VIDEO_BUF_DEMUX
                        memcpy(&pFrameInfo->starcode, MSTR_BYPASS_ENC_DEMUX, sizeof(pFrameInfo->starcode));
                        pFrameInfo->pBypassBuf = (unsigned char*)(mRecordingMemory->data + mRecordingFrameSize*videoBufIndex+sizeof(MS_Bypass_FrameInfo_t));
                        pFrameInfo->nByPassSize = mRecordingFrameSize-sizeof(MS_Bypass_FrameInfo_t);
                        pFrameInfo->mXuHandle = (void *)mAitXuHandle;
                        if ((pFrameInfo->nStreamType = AitH264Demuxer_MJPG_H264((unsigned char*)rawFramePointer, /*mWidth * mHeight * 2*/mCamera->videoIn->buf.bytesused, (unsigned char*)pFrameInfo->pBypassBuf, pFrameInfo->nByPassSize, &mH264FrameSize)) != AIT_H264_DUMMY_FRAME) {
                            /*static int isFirst = 1;
                            FILE *file;

                            if (isFirst) {
                                ALOGI("###############isFirst################ %d", mH264FrameSize);

                                file = fopen("/var/tmp/media/local_h264.bin", "wb");
                                isFirst = 0;
                            } else {
                                ALOGI("###############NotFirst################ %d", mH264FrameSize);

                                file = fopen("/var/tmp/media/local_h264.bin", "ab");
                            }

                            if (file != NULL) {
                                fwrite(mRecordingMemory->data, mH264FrameSize, 1, file);
                                fclose(file);
                            }*/
                        }
                        pFrameInfo->nByPassSize = mH264FrameSize;
#else
                        memcpy(&pFrameInfo->starcode, MSTR_BYPASS_ENC, sizeof(pFrameInfo->starcode));
                        pFrameInfo->pBypassBuf = (unsigned char*)rawFramePointer;
                        pFrameInfo->nByPassSize = mCamera->videoIn->buf.bytesused;
#endif
                        // add vendor info
                        MSTR_BYPASS_START_CODE_WITH_VENDOR(pFrameInfo->starcode, MSTR_BYPASS_VENDOR_AIT);
                    } else {
#ifdef MFE_SUPPORT_YUV422
                        if(mbAIT8589) {
                            switch (mRecordWidth) {
                                case 1280:
                                    yuvFramePointer = (unsigned char *) rawFramePointers[MAIN_STREAM];
                                    break;
                                case 640:
                                    yuvFramePointer = (unsigned char *) rawFramePointers[EYESIGHT_STREAM];
                                    break;
                                default:
                                    ALOGE("Unknown recording resolution!!!");
                                    yuvFramePointer = NULL;
                            }

                            if(!yuvFramePointer) {
                                ALOGV("Got empty recording buffer!!!");
                                break;
                            }
                        }

#ifdef CAMERA_ENABLE_CMA
#ifdef CAMERA_COBUFFER_WITH_VDEC
                        memcpy((MS_U8*)MsOS_MPool_PA2KSEG0(mMEFInputBufPA), yuvFramePointer, mWidth*mHeight*2);
                        MsOS_MPool_Dcache_Flush(MsOS_MPool_PA2KSEG0(mMEFInputBufPA), mMEFInputBufSize);
#else
                        memcpy((MS_U8*)MsOS_MPool_PA2KSEG1(mMEFInputBufPA), yuvFramePointer, mWidth*mHeight*2);
                        MsOS_FlushMemory();
#endif
#else
                        memcpy((MS_U8*)MsOS_MPool_PA2KSEG1(mMEFInputBufPA), yuvFramePointer, mWidth*mHeight*2);
                        MsOS_FlushMemory();
#endif

                        MS_MfeFrameInfo_t* pFrameInfo = (MS_MfeFrameInfo_t*)(mRecordingMemory->data + mRecordingFrameSize * videoBufIndex);
                        memset(pFrameInfo, 0, sizeof(MS_MfeFrameInfo_t));

                        pFrameInfo->advbuf.colorformat = MFE_API_MFE_COLORFORMAT_YUYV;
                        pFrameInfo->starcode[3] = 0x01;
                        pFrameInfo->starcode[4] = 0x47;
                        pFrameInfo->inbuf.Cur_PhyY0 = (unsigned int)mMEFInputBufPA;  //start address of y
                        pFrameInfo->inbuf.Cur_VirY0 = (unsigned char*)MsOS_MPool_PA2KSEG1(mMEFInputBufPA);
                        postVideoFrameMsg = i+1;
#else
                        // FIXME, empty
#endif
                    }
                }
            }
            if (mImageFormat == V4L2_PIX_FMT_MJPEG) {
                mJpegDecoder.SetYuvBufferState(rawFramePointer, true);
            } else if (mImageFormat == H264) {
                mJpegDecoder.SetYuvBufferState(rawFramePointer, true);
                mCameraBuffer.SetYuvBufferState(rawFramePointer1, true);
                ALOGV("%s: SetYuvBufferState, buf: %x, buf1: %x, true", __func__, rawFramePointer, rawFramePointer1);
            } else {
                mCamera->ProcessRawFrameDone();
            }
        }
    }

    //Can I call mDataCb() or mDataCbTimestamp() ?
    //if not, i have no need to get the lock mPreviewLock
    if (mPreviewEnabled) {
        //get the lock mPreviewLock
        Mutex::Autolock lock(mPreviewLock);
        //really can call mDataCb() or mDataCbTimestamp() ?
        //if not, I should release the lock mPreviewLock quickly
        if (mPreviewEnabled) {
            for(int i = 0; i < CAMERA_NUMBER; i++) {
                if (postPreviewFrameMsgs[i] && mPreviewMemorys[i]) {
                    char value[PROPERTY_VALUE_MAX] = {'\0'};
#if EYESIGHT_FPS
            property_get("mstar.eyesight.fps", value, "false");
            if (strcmp(value, "true") == 0) {
                static time_t start, end;
                static timeval once_start, once_end;
                static int totalframe = 0;

                if (totalframe == 0) {
                    time(&start);
                }
                time(&end);
                gettimeofday(&once_end, NULL);

                ALOGI("preview enabled: %x", mPreviewEnabled);
                ALOGI("Latency(, ) =================  %d ms", 1000*(once_end.tv_sec-once_start.tv_sec)+(once_end.tv_usec-once_start.tv_usec)/1000);

                totalframe ++;
                if (difftime (end,start) >= 1) {
                    ALOGI("FPS(%d, %d) ::::::::::::::::: %d", mWidth, mHeight, totalframe);
                    totalframe = 0;
                }
                gettimeofday(&once_start, NULL);
            }
#endif
#if DUMP_EYESIGHT_YUV
            property_get("mstar.eyesight.yuv", value, "false");
            if (strcmp(value, "true") == 0) {
                static bool isFirst = true;
                FILE *file;
                int size = 0;

                if (isFirst) {
                    file = fopen("/var/tmp/media/eyesight_yuv.yuv", "wb");
                    isFirst = false;
                } else {
                    file = fopen("/var/tmp/media/eyesight_yuv.yuv", "ab");
                }

                if (strcmp(mEsParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420SP) == 0 ||
                    strcmp(mEsParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ) {
                    size = EYESIGHT_WIDTH*EYESIGHT_HEIGHT*3/2;
                } else {
                    size = EYESIGHT_WIDTH*EYESIGHT_HEIGHT*2;
                }
                if (file != NULL) {
                    fwrite(mPreviewMemorys[i]->data, size, 1, file);
                    fclose(file);
                }
            }
#endif
                    //ALOGV("cameraid: %d, MsgEnabled: CAMERA_MSG_PREVIEW_FRAME, callback: %x", i, mPreviewMemorys[i]);
                    mDataCb[i](CAMERA_MSG_PREVIEW_FRAME, mPreviewMemorys[i], 0, NULL, mCallbackCookie[i]);
                }
            }

            if (postVideoFrameMsg) {
                mDataCbTimestamp[postVideoFrameMsg-1](systemTime(), CAMERA_MSG_VIDEO_FRAME, mRecordingMemory, videoBufIndex, mCallbackCookie[postVideoFrameMsg-1]);
                videoBufIndex = (videoBufIndex+1)%mVideoBufNum;
            }
        }
    } else {
        //ALOGE("%s: preview is not enabled!!!!!", __func__);
        usleep(1000 * 10 );
    }

    return NO_ERROR;
}

int CameraHardware::beginAutoFocusThread(void *cookie) {
    CameraHardware *c = (CameraHardware *)cookie;
    return c->autoFocusThread();
}

int CameraHardware::autoFocusThread()
{
    if (mMsgEnabled[0] & CAMERA_MSG_FOCUS)
        mNotifyCb[0](CAMERA_MSG_FOCUS, true, 0, mCallbackCookie[0]);
    return NO_ERROR;
}

/**
   @brief Enable a message, or set of messages.

   @param[in] msgtype Bitmask of the messages to enable (defined in include/ui/Camera.h)
   @return none

 */
void CameraHardware::enableMsgType(int32_t msgType, int cameraid)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled[cameraid] |= msgType;
    ALOGV("camera: %d, enable Msg Type: %x, type: %x", cameraid, msgType, mMsgEnabled[cameraid]);
}

/**
   @brief Disable a message, or set of messages.

   @param[in] msgtype Bitmask of the messages to disable (defined in include/ui/Camera.h)
   @return none

 */
void CameraHardware::disableMsgType(int32_t msgType, int cameraid)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled[cameraid] &= ~msgType;
    ALOGV("camera: %d, disable Msg Type: %x, type: %x", cameraid, msgType, mMsgEnabled[cameraid]);
}

/**
   @brief Query whether a message, or a set of messages, is enabled.

   Note that this is operates as an AND, if any of the messages queried are off, this will
   return false.

   @param[in] msgtype Bitmask of the messages to query (defined in include/ui/Camera.h)
   @return true If all message types are enabled
          false If any message type

 */
int CameraHardware::msgTypeEnabled(int32_t msgType, int cameraid)
{
    Mutex::Autolock lock(mLock);
    return(mMsgEnabled[cameraid] & msgType);
}

/**
   @brief Start preview mode.

   @param none
   @return NO_ERROR Camera switched to VF mode
   @todo Update function header with the different errors that are possible

 */
status_t CameraHardware:: startPreview(int cameraid)
{

    status_t ret = NO_ERROR;
    int PreviewFrameSize;
    int32_t tempPreviewEnabled = mPreviewEnabled;
    if (mCameraOpened == false)
        return UNKNOWN_ERROR;

    ALOGI("startPreview, cameraid: %d", cameraid);

    mPreviewEnabled = 0;
    Mutex::Autolock lock(mPreviewLock);
    mPreviewEnabled = tempPreviewEnabled;

    if ((mPreviewEnabled && (!mbAIT8589)) || (mPreviewEnabled & (1<<(4*cameraid)))) {
        ALOGE("Preview already running");
        return ALREADY_EXISTS;
    }

/*
    struct dirent **namelist;
    int total = 0;
    char video_dev[8];
    char camera_dev[16];
    int found = 0;

    if (!mCamera) {
        delete mCamera;
        mCamera = new V4L2Camera();
    }

    Mutex::Autolock lock(mPreviewLock);
    if (mPreviewEnabled) {
        ALOGE("Preview already running");
        return ALREADY_EXISTS;
    }

    total = scandir("/dev", &namelist, my_select, 0);

    if ( total > 0) {
        for (int i = 0 ; i < 3; i ++) {
            snprintf(video_dev,7,"video%d",i);
            found = 0;
            for (int j = 0 ; j < total ; j ++) {
                if (strcmp(video_dev,namelist[j]->d_name) == 0) {
                    ALOGE("found camera dev = %s", namelist[j]->d_name);
                    found = 1;
                    break;
                }
            }

            if (found == 1)
                break;
        }

        while (total --)
            free(namelist[total]);

        free(namelist);
    }

    if (found == 1) {
        snprintf(camera_dev,12,"/dev/%s",video_dev);
        ret = mCamera->Open(camera_dev);
    } else
        ret = mCamera->Open(VIDEO_DEVICE);
    if (ret) {
        ALOGE("Camera device open failed.");
        return UNKNOWN_ERROR;
    }

    ret = mCamera->setParameters(mWidth, mHeight, PIXEL_FORMAT);
    if (ret) {
        ALOGE("Camera setParameters failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    ret = mCamera->Init();
    if (ret) {
        ALOGE("Camera Init failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    ret = mCamera->StartStreaming();
    if (ret) {
        ALOGE("Camera StartStreaming failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
*/

    if(mbAIT8589 && cameraid == EYESIGHT_STREAM) {
        if (strcmp(mEsParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420SP) == 0 ||
            strcmp(mEsParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ) {
            mPreviewMemory1 = mRequestMemory[cameraid](-1, EYESIGHT_WIDTH*EYESIGHT_HEIGHT*3/2, 1, NULL);
        } else {
            mPreviewMemory1 = mRequestMemory[cameraid](-1, EYESIGHT_WIDTH*EYESIGHT_HEIGHT*2, 1, NULL);
        }
        mTempPreviewMemory1 = mRequestMemory[cameraid](-1, EYESIGHT_WIDTH*EYESIGHT_HEIGHT*2, 1, NULL);
        mPreviewMemorys[cameraid] = mPreviewMemory1;
        mTempPreviewMemorys[cameraid] = mTempPreviewMemory1;
        ALOGI("mPreviewMemory1 alloc %x", (unsigned int)mPreviewMemory1);
    } else {
        if (strcmp(mParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
            // YUV420SP
            mPreviewFrameSize = mWidth * mHeight * 3 / 2;
        } else if (strcmp(mParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ) {
            int uvstride = ((mWidth-1)/32 + 1) * 16;
            mPreviewFrameSize = (mWidth + uvstride) * mHeight;
        } else {
            // YUV422
            mPreviewFrameSize = mWidth * mHeight * 2;
        }
        mPreviewMemory = mRequestMemory[cameraid](-1, mPreviewFrameSize, 1, NULL);
        mTempPreviewMemory = mRequestMemory[cameraid](-1, mWidth * mHeight * 2, 1, NULL);
        ALOGI("mPreviewMemory alloc %x, size: %x", (unsigned int)mPreviewMemory, mPreviewFrameSize);
        mPreviewMemorys[cameraid] = mPreviewMemory;
        mTempPreviewMemorys[cameraid] = mTempPreviewMemory;
    }

#ifdef FOR_CAMERA_CTS
#else
    if(cameraid != EYESIGHT_STREAM)
#endif
    {
        //if (mbAIT8431 | mbAIT8589) {
        if (mbAIT8431) {
#if VIDEO_BUF_DEMUX
            mRecordingFrameSize = mWidth * mHeight + sizeof(MS_Bypass_FrameInfo_t);
#else
            mRecordingFrameSize = sizeof(MS_Bypass_FrameInfo_t);
#endif
            mRecordingMemory = mRequestMemory[cameraid](-1, mRecordingFrameSize, mVideoBufNum, NULL);

        } else {
            mRecordingFrameSize = sizeof(_MS_MfeFrameInfo);
            mRecordingMemory = mRequestMemory[cameraid](-1, mRecordingFrameSize, mVideoBufNum, NULL);
        }
    }

    if (mPreviewEnabled) {
        ALOGI("Preview: %x already running, enable: %d", mPreviewEnabled, cameraid);
        mPreviewEnabled |= (1<<(4*cameraid));
        return 0;
    } else {
        Mutex::Autolock lock(mPreviewEnableLock);
        mPreviewEnabled = 1<<(4*cameraid);
        ALOGI("enable preview (id: %d)", cameraid);
    }

    mBounds.left = 0;
    mBounds.top = 0;
    mBounds.right = mWidth;
    mBounds.bottom = mHeight;

    mJpegDecoder.Init();
    mCameraBuffer.Init();

    if (mImageFormat == V4L2_PIX_FMT_MJPEG)
        mDecodeThread = new DecodeThread(this);
    else
        mDecodeThread = NULL;

    if (mImageFormat == H264)
        mDemuxThread = new DemuxThread(this);
    else
        mDemuxThread = NULL;
    mPreviewThread = new PreviewThread(this);

    return ret;
}

/**
   @brief Sets ANativeWindow object.

   Preview buffers provided to CameraHardware via this object. DisplayAdapter will be interfacing with it
   to render buffers to display.

   @param[in] window The ANativeWindow object created by Surface flinger
   @return NO_ERROR If the ANativeWindow object passes validation criteria
   @todo Define validation criteria for ANativeWindow object. Define error codes for scenarios

 */
status_t CameraHardware::setPreviewWindow(struct preview_stream_ops *window, int cameraid)
{
    ALOGI("%s: enter", __func__);

    int32_t tempPreviewEnabled = 0;

    if (mbAIT8589 && mPreviewEnabled) {
        Mutex::Autolock lock(mPreviewEnableLock);
        tempPreviewEnabled = mPreviewEnabled;
        mPreviewEnabled = 0;
    }
    Mutex::Autolock lock(mPreviewLock);
    if(tempPreviewEnabled != 0)
        mPreviewEnabled = tempPreviewEnabled;

    ALOGI("setPreviewWindow: Cameraid: %d", cameraid);
    if (!window) {
        ALOGE("NULL window object");
        mANativeWindow[cameraid] = NULL;
        return BAD_VALUE;
    } else {
        mANativeWindow[cameraid] = window;
#ifdef __ARM_NEON__
        initNativeWindow(HAL_PIXEL_FORMAT_YCbCr_422_I, 3, cameraid);
#else
        initNativeWindow(HAL_PIXEL_FORMAT_RGB_565, 3, cameraid);
#endif
    }
    return NO_ERROR;
}

/**
   @brief Stop a previously started preview.

   @param none
   @return none

 */
void CameraHardware::stopPreview(int cameraid)
{
    sp<DecodeThread> decodeThread;
    sp<DemuxThread> demuxThread;
    sp<PreviewThread> previewThread;

    ALOGI("stopPreview, cameraid: %d", cameraid);

    mPreviewEnabled &= ~(1<<(4*cameraid));
    if (mbAIT8589 && mPreviewEnabled) {
        int32_t tempPreviewEnabled = mPreviewEnabled;

        mPreviewEnabled = 0;
        Mutex::Autolock lock(mPreviewLock);
        mPreviewEnabled = tempPreviewEnabled;

        if (mPreviewMemorys[cameraid] != NULL) {
            mPreviewMemorys[cameraid]->release(mPreviewMemorys[cameraid]);
            mPreviewMemorys[cameraid] = NULL;
        }

        if (mTempPreviewMemorys[cameraid] != NULL) {
            mTempPreviewMemorys[cameraid]->release(mTempPreviewMemorys[cameraid]);
            mTempPreviewMemorys[cameraid] = NULL;
        }

        if ((cameraid == MAIN_STREAM) && mRecordingMemory) {
            mRecordingMemory->release(mRecordingMemory);
            mRecordingMemory = NULL;
            mRecordingEnabled = 0;
        }

        ALOGI("other camera: %x is still used", mPreviewEnabled);
        return;
    }

    // prevent locked by preview too long
    mPreviewEnabled = 0;

    {
        Mutex::Autolock lock(mPreviewLock);
        //mPreviewEnabled = false;
        decodeThread = mDecodeThread;
        demuxThread = mDemuxThread;
        previewThread = mPreviewThread;
    }

    if (mDecodeThread != 0){
        ALOGI("%s:try to close decodeThread", __func__);
        decodeThread->requestExitAndWait();
        ALOGI("%s:successfully close decodeThread", __func__ );
    }

    if (mDemuxThread != 0){
        ALOGI("%s:try to close demuxThread", __func__ );

        demuxThread->requestExitAndWait();
        ALOGI("%s:successfully close demuxThread", __func__ );
    }

    if (mPreviewThread != 0) {
        ALOGI("%s:try to close previewThread", __func__ );
        previewThread->requestExitAndWait();
        ALOGI("%s:successfully to close previewThread", __func__ );
    }

    ALOGI( "%s:Successfully close all threads", __func__ );

    {
        Mutex::Autolock lock(mPreviewLock);
        if (mPreviewMemorys[cameraid] != NULL) {
            mPreviewMemorys[cameraid]->release(mPreviewMemorys[cameraid]);
            mPreviewMemorys[cameraid] = NULL;
        }

        if (mTempPreviewMemorys[cameraid] != NULL) {
            mTempPreviewMemorys[cameraid]->release(mTempPreviewMemorys[cameraid]);
            mTempPreviewMemorys[cameraid] = NULL;
        }

        if (mRecordingMemory) {
            mRecordingMemory->release(mRecordingMemory);
            mRecordingMemory = NULL;
        }

        while(char* p = (char *)mJpegDecoder.GetYuvBuffer())
        {
            ALOGI( "%s: set jpeg buffer state: %p", __func__, p );
            mJpegDecoder.SetYuvBufferState(p, true);
        }
        while(char* p = (char *)mCameraBuffer.GetYuvBuffer())
        {
            ALOGI( "%s: set camera buffer state: %p", __func__, p );
            mCameraBuffer.SetYuvBufferState(p, true);
        }

        mPreviewThread.clear();
        if (mDecodeThread != 0)
            mDecodeThread.clear();
        if (mDemuxThread != 0)
            mDemuxThread.clear();
    }

    {
	    if(mTakePicture == true) {
            disableMsgType(CAMERA_MSG_COMPRESSED_IMAGE,cameraid);
            ALOGD("disableMsgType CAMERA_MSG_COMPRESSED_IMAGE");
        }
        Mutex::Autolock lock(mTakePictureLock);
        if (mJpegMemory != NULL) {
            mJpegMemory->release(mJpegMemory);
            mJpegMemory = NULL;
        }
    }

    mRecordingEnabled = 0;
    ALOGI( "%s: Ok", __func__ );
}

/**
   @brief Returns true if preview is enabled

   @param none
   @return true If preview is running currently
         false If preview has been stopped

 */
bool CameraHardware::previewEnabled(int cameraid)
{
    return (mPreviewEnabled>>(4*cameraid)) & 0x01;
}

/**
   @brief Start record mode.

  When a record image is available a CAMERA_MSG_VIDEO_FRAME message is sent with
  the corresponding frame. Every record frame must be released by calling
  releaseRecordingFrame().

   @param none
   @return NO_ERROR If recording could be started without any issues
   @todo Update the header with possible error values in failure scenarios

 */
status_t CameraHardware::startRecording(int cameraid)
{
    int width, height;
    bool bRet;

    ALOGI("startRecording, camera id: %d", cameraid);

#ifdef FOR_CAMERA_CTS
#else
    if(cameraid != MAIN_STREAM) {
        ALOGE("camera %d is not main stream, cann't surpport recording!!!", cameraid);
        return UNKNOWN_ERROR;
    }
#endif

    Mutex::Autolock lock(mRecordingLock);
    if(mRecordingEnabled != 0) {
        ALOGE("the other used recording, just support one recording");
        return UNKNOWN_ERROR;
    }

#ifdef CAMERA_ENABLE_CMA
#ifdef CAMERA_COBUFFER_WITH_VDEC
    MS_U64 mem_offset = 0;
    mCMAInitParam.heap_id = ION_VDEC_HEAP_ID;
    mCMAInitParam.flags = CMA_FLAG_MAP_VMA| CMA_FLAG_CACHED;
    mCMAUsed = false;
    mGetMemory = false;
    if (MApi_CMA_Pool_Init(&mCMAInitParam) == FALSE) {
        mCMAUsed = false;
        ALOGE("Not use CMA!\n");
    } else {
        mCMAUsed = true;
        ALOGI("MApi_CMA_Pool_Init: pool_handle_id=0x%lx, miu=%ld, offset=0x%lx, length=0x%lx\n", mCMAInitParam.pool_handle_id, mCMAInitParam.miu, mCMAInitParam.heap_miu_start_offset, mCMAInitParam.heap_length);
    }
    if (mCMAUsed) {
        const mmap_info_t * MmapPhyBuffInfo = NULL;

        MmapPhyBuffInfo = mmap_get_info("E_MMAP_ID_CAMERA");
        if (MmapPhyBuffInfo == NULL) {
            ALOGE("[no buffer for mfe input memory in mmap\n");
        } else {
            if (mCMAInitParam.miu == 0) {
                mem_offset = 0;
            } else if (mCMAInitParam.miu == 1) {
                mem_offset = mmap_get_miu0_offset();
            } else if (mCMAInitParam.miu == 2) {
                mem_offset = mmap_get_miu1_offset();
            } else {
                ALOGE("miu offset = %d, return ERROR.\n", mCMAInitParam.miu);
            }

            struct CMA_Pool_Alloc_Param alloc_param;
            alloc_param.pool_handle_id = mCMAInitParam.pool_handle_id;
            alloc_param.length = MmapPhyBuffInfo->size;
            alloc_param.offset_in_pool = MmapPhyBuffInfo->addr - mem_offset - mCMAInitParam.heap_miu_start_offset ;
            alloc_param.flags = CMA_FLAG_VIRT_ADDR;

            if (MApi_CMA_Pool_GetMem(&alloc_param) == FALSE) {
                mGetMemory = false;
                ALOGE("MApi_CMA_Pool_GetMem fail:offset=0x%lx, len=0x%lx, miu=%ld\n", alloc_param.offset_in_pool, alloc_param.length, mCMAInitParam.miu);
            } else {
                mGetMemory = true;
                mCMAFreeParam.pool_handle_id = alloc_param.pool_handle_id;
                mCMAFreeParam.offset_in_pool = alloc_param.offset_in_pool;
                mCMAFreeParam.length = alloc_param.length;

                mMEFInputBufPA = ALIGNADDRESS(MmapPhyBuffInfo->addr, ALIGNED256);
                mMEFInputBufSize = MmapPhyBuffInfo->size - (mMEFInputBufPA - MmapPhyBuffInfo->addr);
            }
        }
    } else
#endif
#endif
    {
#ifdef MFE_SUPPORT_YUV422
        const mmap_info_t * MmapPhyBuffInfo = NULL;

        MmapPhyBuffInfo = mmap_get_info("E_MMAP_ID_CAMERA");
        if (MmapPhyBuffInfo == NULL) {
            ALOGE("[no buffer for mfe input memory in mmap\n");
        } else {
            mMEFInputBufPA = ALIGNADDRESS(MmapPhyBuffInfo->addr, ALIGNED256);
            mMEFInputBufSize = MmapPhyBuffInfo->size - (mMEFInputBufPA - MmapPhyBuffInfo->addr);

            //avoid mmap same memory range more than once (cacheable map)
            if (MsOS_MPool_PA2KSEG1(MmapPhyBuffInfo->addr) == 0) {
                const mmap_info_t * mmapInfo_miu = mmap_get_info("MIU_INTERVAL");
                int miu_offset = MmapPhyBuffInfo->addr >= mmapInfo_miu->size ? mmapInfo_miu->size : 0;
                if (!MsOS_MPool_Mapping(MmapPhyBuffInfo->addr >= mmapInfo_miu->size ? 1 : 0, MmapPhyBuffInfo->addr - miu_offset,  MmapPhyBuffInfo->size, 1)) {
                    ALOGE("map mfe input buffer error*********************\n");
                }
            }
        }
#endif
    }

    mParameters.getVideoSize(&width, &height);

    mRecordWidth = width;
    mRecordHeight = height;

    if ((mWidth != width) || (mHeight != height)) {
        ALOGI("startRecording... getVideoSize(%d, %d) != getPreviewSize(%d,%d) @ CameraHardware.cpp",width,height,mWidth,mHeight);
#if 0 // Added when VideoSize is different from PreviewSize
        stopPreview();

        mParameters.getVideoSize(&mWidth, &mHeight);

        //mParameters.getPreviewSize(&mWidth, &mHeight);

        if (mANativeWindow) {
#ifdef __ARM_NEON__
            initNativeWindow(HAL_PIXEL_FORMAT_BGRA_8888, 3);
#else
            initNativeWindow(HAL_PIXEL_FORMAT_RGB_565, 3);
#endif
        }

        startPreview();
#endif
    }

#ifdef MFE_SUPPORT_YUV422
    mRecordingEnabled = 1<<(4*cameraid);
#else
    //FIXME, empty
#endif

    return NO_ERROR;
}

/**
   @brief Stop a previously started recording.

   @param none
   @return none

 */
void CameraHardware::stopRecording(int cameraid)
{
    ALOGI("stopRecording, camera id: %d", cameraid);
    Mutex::Autolock lock(mRecordingLock);

#ifdef CAMERA_ENABLE_CMA
    if (mCMAUsed && mGetMemory) {
        if (MApi_CMA_Pool_PutMem(&mCMAFreeParam) == FALSE) {
            ALOGE("MApi_CMA_Pool_PutMem fail: offset=0x%lx, len=0x%lx\n", mCMAFreeParam.offset_in_pool, mCMAFreeParam.length);
        }
        ALOGI("MApi_CMA_Pool_PutMem: offset=0x%lx, len=0x%lx\n", mCMAFreeParam.offset_in_pool, mCMAFreeParam.length);
    }
#endif

    mRecordingEnabled &= ~(1<<(4*cameraid));
}

/**
   @brief Returns true if recording is enabled.

   @param none
   @return true If recording is currently running
         false If recording has been stopped

 */
int CameraHardware::recordingEnabled(int cameraid)
{
    return (mRecordingEnabled>>(4*cameraid)) & 0x01;
}

/**
   @brief Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.

   @param[in] mem MemoryBase pointer to the frame being released. Must be one of the buffers
               previously given by CameraHardware
   @return none

 */
void CameraHardware::releaseRecordingFrame(const void* mem) {
    //mRecordingLock.lock();
    //mRecordingBufferUsing = false;
    //mRecordingLock.unlock();
}

/**
   @brief Start auto focus

   This call asynchronous.
   The notification callback routine is called with CAMERA_MSG_FOCUS once when
   focusing is complete. autoFocus() will be called again if another auto focus is
   needed.

   @param none
   @return NO_ERROR
   @todo Define the error codes if the focus is not locked

 */
status_t CameraHardware::autoFocus() {
    Mutex::Autolock lock(mLock);
    if (createThread(beginAutoFocusThread, this) == false) {
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

/**
   @brief Cancels auto-focus function.

   If the auto-focus is still in progress, this function will cancel it.
   Whether the auto-focus is in progress or not, this function will return the
   focus position to the default. If the camera does not support auto-focus, this is a no-op.


   @param none
   @return NO_ERROR If the cancel succeeded
   @todo Define error codes if cancel didnt succeed

 */
status_t CameraHardware::cancelAutoFocus() {
    return NO_ERROR;
}

int CameraHardware::beginPictureThread(void *cookie) {
    CameraHardware *c = (CameraHardware *)cookie;
    return c->pictureThread();
}

int CameraHardware::pictureThread() {
    ALOGI("pictureThread: E");
    Mutex::Autolock lock(mTakePictureLock);

    int id = mTakingId;
    mTakingId = 0;

    if (mMsgEnabled[id] & CAMERA_MSG_SHUTTER) {
        ALOGI("mTakePictureLock  shutter !!!!!");
        mNotifyCb[id](CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie[id]);
    }

    // When setting CAMERA_MSG_RAW_IMAGE without giving a raw image buffer,
    // CAMERA_MSG_RAW_IMAGE_NOTIFY will be set instead
    if (mMsgEnabled[id] & CAMERA_MSG_RAW_IMAGE && mTakePictureBuf != NULL) {
        ALOGI("CAMERA_MSG_RAW_IMAGE  234 !!!!!");
        mRawDataMemory = mRequestMemory[id](-1, mWidth * mHeight * 2, 1, NULL);
        memcpy((void *)mRawDataMemory->data, mTakePictureBuf, mPictureWidth * mPictureHeight * 2);
        mDataCb[id](CAMERA_MSG_RAW_IMAGE, mRawDataMemory,0, NULL, mCallbackCookie[id]);
    } else if (mMsgEnabled[id] & CAMERA_MSG_RAW_IMAGE_NOTIFY) {
        // Just notify a msg that there is a raw image. CAMERA_MSG_RAW_IMAGE_NOTIFY will be changed to
        //CAMERA_MSG_RAW_IMAGE, so the app will get CAMERA_MSG_RAW_IMAGE msg , not CAMERA_MSG_RAW_IMAGE_NOTIFY msg
        ALOGI("CAMERA_MSG_RAW_IMAGE_NOTIFY");
        mNotifyCb[id](CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, mCallbackCookie[id]);
    }

    if (mMsgEnabled[id] & CAMERA_MSG_COMPRESSED_IMAGE && mTakePictureBuf != NULL) {
        FILE *output = NULL;
        FILE *input = NULL;
        int fileSize = 0;

        output = fopen(TMP_JPEG, "wb");
        if (output == NULL) {
            ALOGE("GrabJpegFrame: Ouput file == NULL");
        } else {
            ALOGI("saveYUYVtoJPEG %x , %d, %d ", (unsigned int)mTakePictureBuf,mPictureWidth, mPictureHeight);

            fileSize = saveYUYVtoJPEG((unsigned char *)mTakePictureBuf, mPictureWidth, mPictureHeight, output, 85);

            fclose(output);

            input = fopen(TMP_JPEG, "rb");

            if (input == NULL) {
                ALOGE("GrabJpegFrame: Input file == NULL");
            } else {
                // first store the src image, because the image has a jfif head, not exif head.
                // we replace the jfif head with a exif head.
                int srcImgFileLength = fileSize;
                void *srcImgFileBuf = malloc(fileSize);
                if (srcImgFileBuf == NULL) {
                    ALOGE("GrabJpegFrame: Can not allocate memory to store source image file");
                    fclose(input);
                } else {
                    ALOGD("store the source image file");
                    fread(srcImgFileBuf, 1, fileSize, input);
                    fclose(input);

                    //create thumbnail
                    void *thumbnailBuf = NULL;
                    int thumbnailWidth = mThumbnailWidth;
                    int thumbnailHeight = mThumbnailHeight;
                    int thumbnailLength = 0; //thumbnail file length
                    if (thumbnailWidth == 0 || thumbnailHeight == 0) {
                        ALOGD("GrabJpegFrame: Do not need to create thumbnail");
                    } else {
                        ALOGD("need to create thumbnail: thumbnailWidth=%i thumbnailHeight=%i",thumbnailWidth,thumbnailHeight);
                        thumbnailBuf = malloc(thumbnailWidth*thumbnailHeight*2);
                        if (thumbnailBuf == NULL) {
                            ALOGE("GrabJpegFrame: Can not allocate memory to store thumbnail image data");
                            free(srcImgFileBuf);
                            srcImgFileBuf = NULL;
                        } else {
                            //scale down the size of the source image to the size of thumbnail
                            ALOGD("scale down the size of the source image to the size of thumbnail");
                            scaleDownYuv422((unsigned char *)mTakePictureBuf, mPictureWidth, mPictureHeight,
                                            (unsigned char *)thumbnailBuf,thumbnailWidth,thumbnailHeight);

                            output = fopen(TMP_JPEG, "wb");
                            if (output == NULL) {
                                ALOGE("GrabJpegFrame: Ouput thumbnail file == NULL");
                                free(srcImgFileBuf);
                                srcImgFileBuf = NULL;
                                free(thumbnailBuf);
                                thumbnailBuf = NULL;
                            } else {
                                // save thumbnail to jpeg
                                ALOGD("save thumbnail to jpeg");
                                fileSize = saveYUYVtoJPEG((unsigned char *)thumbnailBuf, thumbnailWidth, thumbnailHeight, output, 85);
                                fclose(output);

                                free(thumbnailBuf);
                                thumbnailBuf = NULL;

                                // read the thumbnail jpeg file
                                input = fopen(TMP_JPEG, "rb");
                                if (input == NULL) {
                                    ALOGE("GrabJpegFrame: Input thumbnail file == NULL");
                                    free(srcImgFileBuf);
                                    srcImgFileBuf = NULL;
                                } else {
                                    thumbnailBuf = malloc(fileSize);
                                    if (thumbnailBuf == NULL) {
                                        ALOGE("GrabJpegFrame: Can not allocate memory to store thumbnail file");
                                        fclose(input);
                                        free(srcImgFileBuf);
                                        srcImgFileBuf = NULL;
                                    } else {
                                        ALOGD("store thumbnail file");
                                        fread(thumbnailBuf, 1, fileSize, input);
                                        fclose(input);

                                        thumbnailLength = fileSize;
                                    }
                                }
                            }
                        }
                    }

                    if (srcImgFileBuf != NULL) {
                        //prepare exifInfo.
                        //Please reference http://www.exif.org/Exif2-1.PDF

                        ALOGD("prepare exifInfo");
                        exif_attribute_t exifInfo;
                        memset(&exifInfo,0,sizeof(exif_attribute_t));

                        // mPictureWidth == mWidth and mPictureHeight == mHeight. So we should get the picture size
                        // which the user wants.
                        mParameters.getPictureSize((int *)&exifInfo.width,(int *)&exifInfo.height);

                        // date time
                        time_t timep;
                        struct tm *p;
                        time(&timep);
                        //ics requests UTC time, but jb requests local time
                        p = localtime(&timep);
                        snprintf((char*)exifInfo.date_time,sizeof(exifInfo.date_time),"%d:%d%d:%d%d %d%d:%d%d:%d%d",
                                 (1900+p->tm_year),(1+p->tm_mon)/10, (1+p->tm_mon)%10,
                                 p->tm_mday/10, p->tm_mday%10,p->tm_hour/10,p->tm_hour%10,
                                 p->tm_min/10,p->tm_min%10,p->tm_sec/10,p->tm_sec%10);

                        //gps
                        double latitude = mParameters.getFloat(CameraParameters::KEY_GPS_LATITUDE);
                        double longitude = mParameters.getFloat(CameraParameters::KEY_GPS_LONGITUDE);
                        double altitude = mParameters.getFloat(CameraParameters::KEY_GPS_ALTITUDE);
                        int timestamp = mParameters.getInt(CameraParameters::KEY_GPS_TIMESTAMP);
                        if (latitude == -1 || longitude == -1 || altitude == -1 || timestamp == -1) {
                            exifInfo.enableGps = false;
                        } else {
                            const char * method = mParameters.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
                            if (method != NULL) {
                                // fix coverity HIGH_IMPACT: RESOURCE_LEAK
                                //strncpy((char*)(exifInfo.gps_processing_method), method, sizeof(exifInfo.gps_processing_method));
                                strncpy((char*)(exifInfo.gps_processing_method), method, (sizeof(exifInfo.gps_processing_method)-1));
                                exifInfo.gps_processing_method[(sizeof(exifInfo.gps_processing_method)-1)] = '\0';
                            }

                            exifInfo.enableGps = true;

                            //latitude ref
                            strncpy((char*)exifInfo.gps_latitude_ref, EXIF_DEF_GPS_LATITUDE_REF, sizeof(exifInfo.gps_latitude_ref));
                            //longitude ref
                            strncpy((char*)exifInfo.gps_longitude_ref, EXIF_DEF_GPS_LATITUDE_REF, sizeof(exifInfo.gps_longitude_ref));
                            //altitude ref
                            exifInfo.gps_altitude_ref = EXIF_DEF_GPS_ALTITUDE_REF;

                            //calculate latitude
                            exifInfo.gps_latitude[EXIF_DEF_GPS_ANGLE_ANGLE].num = (int)latitude;
                            exifInfo.gps_latitude[EXIF_DEF_GPS_ANGLE_ANGLE].den = EXIF_DEF_GPS_ANGLE_ANGLE_VALUE_UNIT;

                            latitude = latitude - (int)(latitude);
                            latitude = latitude * MINUTE_PER_ANGLE * EXIF_DEF_GPS_ANGLE_MINUTE_VALUE_UNIT;
                            exifInfo.gps_latitude[EXIF_DEF_GPS_ANGLE_MINUTE].num = (int)latitude;
                            exifInfo.gps_latitude[EXIF_DEF_GPS_ANGLE_MINUTE].den = EXIF_DEF_GPS_ANGLE_MINUTE_VALUE_UNIT;

                            latitude = latitude - (int)(latitude);
                            latitude = latitude * SECOND_PER_MINUTE * EXIF_DEF_GPS_ANGLE_SECOND_VALUE_UNIT;
                            exifInfo.gps_latitude[EXIF_DEF_GPS_ANGLE_SECOND].num = (int)latitude;
                            exifInfo.gps_latitude[EXIF_DEF_GPS_ANGLE_SECOND].den = EXIF_DEF_GPS_ANGLE_SECOND_VALUE_UNIT;

                            //calculate longitude
                            exifInfo.gps_longitude[EXIF_DEF_GPS_ANGLE_ANGLE].num = (int)longitude;
                            exifInfo.gps_longitude[EXIF_DEF_GPS_ANGLE_ANGLE].den = EXIF_DEF_GPS_ANGLE_ANGLE_VALUE_UNIT;

                            longitude = longitude - (int)(longitude);
                            longitude = longitude * MINUTE_PER_ANGLE * EXIF_DEF_GPS_ANGLE_MINUTE_VALUE_UNIT;
                            exifInfo.gps_longitude[EXIF_DEF_GPS_ANGLE_MINUTE].num = (int)longitude;
                            exifInfo.gps_longitude[EXIF_DEF_GPS_ANGLE_MINUTE].den = EXIF_DEF_GPS_ANGLE_MINUTE_VALUE_UNIT;

                            longitude = longitude - (int)(longitude);
                            longitude = longitude * SECOND_PER_MINUTE * EXIF_DEF_GPS_ANGLE_SECOND_VALUE_UNIT;
                            exifInfo.gps_longitude[EXIF_DEF_GPS_ANGLE_SECOND].num = (int)longitude;
                            exifInfo.gps_longitude[EXIF_DEF_GPS_ANGLE_SECOND].den = EXIF_DEF_GPS_ANGLE_SECOND_VALUE_UNIT;

                            //calculate altitude
                            exifInfo.gps_altitude.num = altitude;
                            exifInfo.gps_altitude.den = EXIF_DEF_GPS_ALTITUDE_VALUE_UNIT;

                            //calculate datestamp and timestamp
                            time_t gpsTime = timestamp;
                            //gmtime() will return UTC Time
                            struct tm *gpsTimestamp = gmtime(&gpsTime);
                            exifInfo.gps_timestamp[EXIF_DEF_GPS_TIME_HOUR].num = gpsTimestamp->tm_hour;
                            exifInfo.gps_timestamp[EXIF_DEF_GPS_TIME_HOUR].den = EXIF_DEF_GPS_TIME_HOUR_VALUE_UNIT;
                            exifInfo.gps_timestamp[EXIF_DEF_GPS_TIME_MINUTE].num = gpsTimestamp->tm_min;
                            exifInfo.gps_timestamp[EXIF_DEF_GPS_TIME_MINUTE].den = EXIF_DEF_GPS_TIME_MINUTE_VALUE_UNIT;
                            exifInfo.gps_timestamp[EXIF_DEF_GPS_TIME_SECOND].num = gpsTimestamp->tm_sec;
                            exifInfo.gps_timestamp[EXIF_DEF_GPS_TIME_SECOND].den = EXIF_DEF_GPS_TIME_SECOND_VALUE_UNIT;

                            snprintf((char*)(exifInfo.gps_datestamp), sizeof(exifInfo.gps_datestamp),"%d:%d:%d",
                                     (1900+gpsTimestamp->tm_year),(1+gpsTimestamp->tm_mon),gpsTimestamp->tm_mday);
                        }

                        exifInfo.enableThumb = thumbnailLength != 0;

                        strncpy((char *)exifInfo.maker,EXIF_DEF_MAKER,sizeof(exifInfo.maker));
                        strncpy((char *)exifInfo.model,EXIF_DEF_MODEL,sizeof(exifInfo.model));
                        strncpy((char *)exifInfo.software,EXIF_DEF_SOFTWARE,sizeof(exifInfo.software));
                        strncpy((char *)exifInfo.exif_version,EXIF_DEF_EXIF_VERSION,sizeof(exifInfo.exif_version));
                        strncpy((char *)exifInfo.user_comment,EXIF_DEF_USERCOMMENTS,sizeof(exifInfo.user_comment));

                        exifInfo.widthThumb = thumbnailWidth;
                        exifInfo.heightThumb = thumbnailHeight;
                        exifInfo.thumbBuf = (unsigned char*)thumbnailBuf;
                        exifInfo.thumbSize = thumbnailLength;
                        exifInfo.orientation = EXIF_ORIENTATION_UP;
                        exifInfo.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;
                        exifInfo.exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
                        exifInfo.metering_mode = EXIF_DEF_EXPOSURE_MODE;
                        exifInfo.color_space = EXIF_DEF_COLOR_SPACE;

                        exifInfo.focal_length.num = EXIF_DEF_FOCAL_LEN_NUM;
                        exifInfo.focal_length.den = EXIF_DEF_FOCAL_LEN_DEN;

                        exifInfo.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
                        exifInfo.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
                        exifInfo.y_resolution.num = EXIF_DEF_RESOLUTION_NUM;
                        exifInfo.y_resolution.den = EXIF_DEF_RESOLUTION_DEN;
                        exifInfo.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;

                        exifInfo.compression_scheme = EXIF_DEF_COMPRESSION;

                        unsigned int exifOutBufSize = 0;
                        unsigned char *exifOut = (unsigned char*)malloc(EXIF_FILE_SIZE + thumbnailLength);
                        if (exifOut == NULL) {
                            ALOGE("GrabJpegFrame: Can not allocate memory to store exif head");
                            free(srcImgFileBuf);
                            srcImgFileBuf = NULL;
                            if (thumbnailBuf != NULL) {
                                free(thumbnailBuf);
                                thumbnailBuf = NULL;
                            }
                        } else {
                            ALOGD("makeExif");
                            makeExif(exifOut,&exifInfo,&exifOutBufSize);

                            output = fopen(TMP_JPEG, "wb");
                            if (output == NULL) {
                                ALOGE("GrabJpegFrame: Can not open the output file to store the jpg which has exif head");
                            } else {
                                //first write the identifier 0xFF 0xD8
                                fwrite(srcImgFileBuf,1,FILE_IDENTIFIER_LENGTH,output);
                                // then write the exif head which we create
                                fwrite(exifOut,1,exifOutBufSize,output);
                                //skip the 2 byte identifier and the 18 byte jfif head of the source image
                                fwrite((void *)((MS_U32)srcImgFileBuf + FILE_IDENTIFIER_LENGTH + JFIF_HEAD_LENGTH),1,
                                       srcImgFileLength - FILE_IDENTIFIER_LENGTH - JFIF_HEAD_LENGTH, output);
                                fclose(output);
                            }

                            free(srcImgFileBuf);
                            srcImgFileBuf = NULL;
                            if (thumbnailBuf != NULL) {
                                free(thumbnailBuf);
                                thumbnailBuf = NULL;
                            }
                            free(exifOut);
                            exifOut = NULL;
                        }

                        ALOGD("fopen TMP_JPEG");
                        input = fopen(TMP_JPEG, "rb");
                        if (input == NULL) {
                            ALOGE("GrabJpegFrame: Can not open the input file to read the jpg which has exif head");
                        } else {
                            fseek(input,0,SEEK_END);
                            fileSize = ftell(input);
                            fseek(input,0,SEEK_SET);

                            mJpegMemory = mRequestMemory[id](-1, fileSize, 1, NULL);
                            if (mJpegMemory != NULL) {
                                fread((uint8_t *)mJpegMemory->data, 1, fileSize, input);
                            }
                            fclose(input);
                            ALOGI("mMsgEnabled[%d]: 0x%x", id, mMsgEnabled[id]);
                            if(mMsgEnabled[id] & CAMERA_MSG_COMPRESSED_IMAGE) {
                                ALOGI("takePictureThread, mDataCb");
                                mDataCb[id](CAMERA_MSG_COMPRESSED_IMAGE, mJpegMemory, 0, NULL, mCallbackCookie[id]);
                                ALOGI("takePictureThread, mDataCb done");
                            }
                            ALOGI("free mJpegMemory");
                            if (mJpegMemory) {
                                mJpegMemory->release(mJpegMemory);
                                mJpegMemory = NULL;
                            }
                        }
                    }
                }
            }
        }
    }

    free(mTakePictureBuf);
    mTakePictureBuf = NULL;
    mTakePicture = false;
    ALOGI("mTakePictureLock  end !!!!!");
    return NO_ERROR;
}

/**
   @brief Take a picture.

   @param none
   @return NO_ERROR If able to switch to image capture
   @todo Define error codes if unable to switch to image capture

 */
status_t CameraHardware::takePicture(int cameraid)
{
    int width, height;
    int retry = 5;

    if(mbAIT8589 && cameraid != MAIN_STREAM)
        return UNKNOWN_ERROR;

    if(!mbAIT8589)
        stopPreview(cameraid);
    else
        mTakePicture = true;

    while (mTakePictureBuf != NULL)
        usleep(1000 *10);

    mTakingId = cameraid;

    mParameters.getPreviewSize(&width, &height);
    // use preview size first need refine
    mTakePictureBuf = malloc( mWidth * mHeight * 2 );
    ALOGI("takePicture, preview Size: %dx%d", width, height);

    char *rawFramePointer;
    while (retry >= 0) {

        if(mbAIT8589) {
            rawFramePointer = (char *)mJpegDecoder.GetYuvBuffer();
            ALOGI("takePicture, got buffer from H264 stream");
        } else {
            rawFramePointer = mCamera->GrabRawFrame();
        }

        if (rawFramePointer != NULL) {
            ALOGE("ok to grab raw frame");
            break;
        } else {
            ALOGE("failed to grab raw frame : %i",retry);
            usleep(1000 *20);
        }
        --retry;
    }

    if (rawFramePointer != NULL) {
        // use preview size first need refine
        mPictureWidth = mWidth;
        mPictureHeight = mHeight;

        if (mImageFormat == V4L2_PIX_FMT_MJPEG) {
            //refine later
            void* buf = mJpegDecoder.GetEmptyYuvBuffer();
            mJpegDecoder.Decode(NULL, rawFramePointer, mCamera->videoIn->buf.bytesused, buf);
            mJpegDecoder.SetYuvBufferState(buf, false);
            memcpy(mTakePictureBuf, buf, width * height * 2);
            mJpegDecoder.SetYuvBufferState(buf, true);
        } else if (mImageFormat == H264) {
            memcpy(mTakePictureBuf, (void *)rawFramePointer, mWidth*mHeight*2);
        } else {
#if SUPPORT_352X288_384X216_176X144_FLAG
            if (mWidth*mHeight*2 <= mCamera->videoIn->buf.bytesused) {
                memcpy(mTakePictureBuf, (void *)rawFramePointer, mWidth*mHeight*2);
            } else if (mWidth*mHeight*2 > mCamera->videoIn->buf.bytesused) {
                memcpy(mTakePictureBuf, (void *)rawFramePointer, mCamera->videoIn->buf.bytesused);
            }
#else
            memcpy(mTakePictureBuf, (void *)rawFramePointer, mCamera->videoIn->buf.bytesused);
#endif
        }
    } else {
        free(mTakePictureBuf);
        mTakePictureBuf = NULL;
        if(mbAIT8589) {
            mJpegDecoder.SetYuvBufferState(rawFramePointer, true);
        } else {
            mCamera->ProcessRawFrameDone();
        }
        return UNKNOWN_ERROR;
    }
    if (createThread(beginPictureThread, this) == false) {
        if(mbAIT8589) {
            mJpegDecoder.SetYuvBufferState(rawFramePointer, true);
        } else {
            mCamera->ProcessRawFrameDone();
        }
        return UNKNOWN_ERROR;
    }
    //Mutex::Autolock lock(mTakePictureLock);
    mTakePicture = true;

    if(mbAIT8589) {
        mJpegDecoder.SetYuvBufferState(rawFramePointer, true);
        return NO_ERROR;
    }

    mCamera->ProcessRawFrameDone();
    return NO_ERROR;
}

/**
   @brief Cancel a picture that was started with takePicture.

   Calling this method when no picture is being taken is a no-op.

   @param none
   @return NO_ERROR If cancel succeeded. Cancel can succeed if image callback is not sent
   @todo Define error codes

 */
status_t CameraHardware::cancelPicture(int cameraid)
{
    ALOGI("CameraHardware::cancelPicture");
    return NO_ERROR;
}

/**
   @brief Set the camera parameters.

   @param[in] params Camera parameters to configure the camera
   @return NO_ERROR
   @todo Define error codes

 */
int CameraHardware::setParameters(const char* parameters, int cameraid)
{
    unsigned int i;
    int width, height;
    int min_fps, max_fps;
    Vector<Size> sizes;
    CameraParameters params;
    String8 str_params(parameters);
    params.unflatten(str_params);
    status_t ret = NO_ERROR;
    int32_t tempPreviewEnabled = mPreviewEnabled;

    ALOGE("setParameters: %s", parameters);

    mPreviewEnabled = 0;
    Mutex::Autolock lock(mLock);
    //Mutex::Autolock lock(mTakePictureLock);

    params.getSupportedPreviewSizes(sizes);
    params.getPreviewSize(&width, &height);

    params.getPreviewFpsRange(&min_fps ,&max_fps);
    if (min_fps < 0 || max_fps < 0)
        return BAD_VALUE;
    else if ((max_fps - min_fps) < 0)
        return BAD_VALUE;
    if(mbAIT8589 && (cameraid == EYESIGHT_STREAM)) {
        min_fps /= 1000;
        max_fps /= 1000;
        AitXU_SetMinFps(mAitXuHandle, min_fps);
        ALOGI("set fps(camera id: %d), min fps: %d, max fps: %d",
                cameraid, min_fps, max_fps);

        int fps;
        fps = params.getPreviewFrameRate();
        if(fps >= min_fps && fps <= max_fps) {
            AitXU_SetFrameRate(mAitXuHandle, fps);
            ALOGI("set fps(camera id: %d), fps: %d", cameraid, fps);
        }
    }

    if (width < 0 || height < 0)
        return BAD_VALUE;

    if (strcmp("invalid",params.get(CameraParameters::KEY_FOCUS_MODE)) == 0)
        return BAD_VALUE;

    if (strcmp("invalid",params.get(CameraParameters::KEY_FLASH_MODE)) == 0)
        return BAD_VALUE;

    if(mbAIT8589 && cameraid == EYESIGHT_STREAM) { //FIXME
        mEsParameters = params;
        mPreviewEnabled = tempPreviewEnabled;
        return NO_ERROR;
    } else {
        mParameters = params;
    }

    //Add some camera parameter logs for debug
    //ALOGD("%s - CameraParameters::KEY_FLASH_MODE is %s", __FUNCTION__, mParameters.get(CameraParameters::KEY_FLASH_MODE));
    //ALOGD("%s  - CameraParameters::KEY_AUTO_EXPOSURE_LOCK is %s", __FUNCTION__, mParameters.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK));

    width = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    height = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    mParameters.getSupportedThumbnailPictureSizes(sizes);

    for (i = 0; i < sizes.size(); i++) {
        if (width == sizes[i].width && height == sizes[i].height) {
            break;
        }
    }

    if (i == sizes.size()) {
        width = MIN_THUMBNAIL_WIDTH;
        height = MIN_THUMBNAIL_HEIGHT;
        ALOGI("Change thumbnail size to %d x %d", width, height);
    }
    mThumbnailWidth = width;
    mThumbnailHeight = height;

    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, mThumbnailWidth);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, mThumbnailHeight);

    if (mbAIT8431) {
        mParameters.getVideoSize(&width, &height);
        switch (width) {
            case 1280:
                width = 384;
                height = 216;
                break;
            default:
                width = 320;
                height = 240;
        }
        /*
    } else if (mbAIT8589) {
        mParameters.getVideoSize(&width, &height);
        switch(width)
        {
            case 1280:
                width = 320;
                height = 240;
                break;
            default:
                width = 320;
                height = 240;
        }
        */
    } else {
        mParameters.getSupportedPreviewSizes(sizes);
        mParameters.getPreviewSize(&width, &height);

        for (i = 0; i < sizes.size(); i++)
            if (width == sizes[i].width && height == sizes[i].height)
                break;

        if (i == sizes.size()) {
            params.getPreferredPreviewSizeForVideo(&width, &height);
            if ((width <= 0) || (height <= 0))
                mParameters.getPreviewSize(&width, &height);

            ALOGI("Change preview size to %dx%d", width, height);
        }
    }

/* dup to nu 2574
    if(mbAIT8589 && cameraid == EYESIGHT_STREAM) { //FIXME
        return NO_ERROR;
    }
    */

    //Modify
    mParameters.setPreviewSize(width, height);
    ALOGI("setPreviewSize to %d, %d",width,height);

    if (mWidth == width && mHeight == height) {
        mPreviewEnabled = tempPreviewEnabled;
        return NO_ERROR;
    }

    ALOGI("Change PreviewSize to : %d, %d", width, height);
    //If change the camera preview size, mPreviewLock should be lock
    Mutex::Autolock previewlock(mPreviewLock);
    Mutex::Autolock demuxlock(mDemuxLock);
    mPreviewEnabled = tempPreviewEnabled;
    mWidth = width;
    mHeight = height;

    mCamera->StopStreaming();
    mCamera->Uninit();
    mCamera->Close();

    if (mPreviewMemorys[cameraid])
        mPreviewMemorys[cameraid]->release(mPreviewMemorys[cameraid]);

    ret = mCamera->Open(mCameraDev);
    if (ret == -1) {
        ALOGE("Camera device open failed.");
    }

    if (mbAIT8589) {
        if (m720pSupported && ret != -1) {
            if (mAitXuHandle) {
                AitXU_Release(&mAitXuHandle);
            }

            mAitXuHandle = AitXU_Init(mCameraDev);

            //ret = AitXU_SetMode(mAitXuHandle, STREAM_MODE_FRAMEBASE_H264);//arthur_tsao@20130423 for Framebase H264
            ret = AitXU_SetMode(mAitXuHandle, STREAM_MODE_FRAMEBASE_H264L_MJPEG_YUY2_NV12);
            if (ret) {
                ALOGE("AIT8589: Failed to set mode\r\n");
            }

            mParameters.getVideoSize(&width, &height);
            switch(mWidth)
            {
                case 1920:
                    ret = AitXU_SetEncRes(mAitXuHandle, 0x0B);
                case 1280:
                    ret = AitXU_SetEncRes(mAitXuHandle, 0x0A);   //arthur_tsao@20130604 for new second resolution defination
                    break;
                case 640:
                    ret = AitXU_SetEncRes(mAitXuHandle, 0x03);   //arthur_tsao@20130423 for new second resolution defination
                    break;
                    /*
                case 352:
                    ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_352X288);
                    break;
                    */
                case 320:
                    ret = AitXU_SetEncRes(mAitXuHandle, 0x02);  //arthur_tsao@20130423 for new second resolution defination
                    break;
                case 160:
                    ret = AitXU_SetEncRes(mAitXuHandle, 0x01);  //arthur_tsao@20130423 for new second resolution defination
                    break;
                default:
                    ret = -1;
            }

            if (ret) {
                ALOGE("AIT8589: Failed to select resolution \r\n");
            }
        }
    }
    /*
    if (mbAIT8431) {
        if (m720pSupported && ret != -1) {
            if (mAitXuHandle) {
                SkypeXU_Release(&mAitXuHandle);
            }

            mAitXuHandle = SkypeXU_Init(ret);
            SkypeXU_SetMode(mAitXuHandle, 2);

            mParameters.getVideoSize(&width, &height);
            switch (width) {
                case 1280:
                    ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_1280X720);
                    break;
                case 640:
                    ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_640X480);
                    break;
                case 352:
                    ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_352X288);
                    break;
                case 320:
                    ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_320X240);
                    break;
                case 160:
                    ret = SkypeXU_SetEncRes(mAitXuHandle, SKYPE_H264_160X120);
                    break;
                default:
                    ret = -1;
            }

            if (ret < 0) {
                ALOGE("Failed to select resolution \r\n");
            }
        }
    }
    */

    if(mbAIT8589) {
        mImageFormat = H264;
    } else if (mbAIT8431 || ((mWidth >= WIDTH_720P) && (mHeight >= HEIGHT_720P))) {
#if YUV_DATA_ONLY   //FIXME MJPEG
        mImageFormat = V4L2_PIX_FMT_YUYV;
#else
        mImageFormat = V4L2_PIX_FMT_MJPEG;
#endif
    } else {
        ALOGE("not 720P , change yuv ");
        mImageFormat = V4L2_PIX_FMT_YUYV;
    }

    ret = mCamera->setParameters(mWidth, mHeight, mImageFormat);
    if (ret) {
        ALOGE("Camera setParameters failed: %s", strerror(errno));
    }

    ret = mCamera->Init();
    if (ret) {
        ALOGE("Camera Init failed: %s", strerror(errno));
    }

    ret = mCamera->StartStreaming();
    if (ret) {
        ALOGE("Camera StartStreaming failed: %s", strerror(errno));
    }

    if (strcmp(mParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420SP) == 0 ||
        strcmp(mParameters.getPreviewFormat(), CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ) {
        // YUV420SP
        mPreviewFrameSize = mWidth * mHeight * 3 / 2;
    } else {
        // YUV422
        mPreviewFrameSize = mWidth * mHeight * 2;
    }
    mPreviewMemorys[cameraid] = mRequestMemory[cameraid](-1, mPreviewFrameSize, 1, NULL);

    while(char* p = (char *)mJpegDecoder.GetYuvBuffer())
    {
        mJpegDecoder.SetYuvBufferState(p, true);
    }
    while(char* p = (char *)mCameraBuffer.GetYuvBuffer())
    {
        mCameraBuffer.SetYuvBufferState(p, true);
    }
    mBounds.left = 0;
    mBounds.top = 0;
    mBounds.right = mWidth;
    mBounds.bottom = mHeight;

    return NO_ERROR;
}

/**
   @brief Return the camera parameters.

   @param none
   @return Currently configured camera parameters

 */
char* CameraHardware::getParameters(int cameraid)
{
    String8 params_str8;
    CameraParameters params;
    char* params_string;

    {
        Mutex::Autolock lock(mLock);
        if(mbAIT8589 && cameraid == EYESIGHT_STREAM) { //FIXME
            params = mEsParameters;
        } else {
            params = mParameters;
        }
    }

    params_str8 = params.flatten();

    params_string = (char*) malloc(sizeof(char) * (params_str8.length()+1));
    strcpy(params_string, params_str8.string());

    ALOGE("getParameters: %s", params_string);
    return params_string;
}

void CameraHardware::putParameters(char *parms)
{
    free(parms);
}

/**
   @brief Send command to camera driver.

   @param none
   @return NO_ERROR If the command succeeds
   @todo Define the error codes that this function can return

 */
status_t CameraHardware::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2) {
    status_t ret = NO_ERROR;

    if (cmd == CAMERA_CMD_START_FACE_DETECTION)
        ret = BAD_VALUE;

    return ret;
}

/**
   @brief Release the hardware resources owned by this object.

   Note that this is *not* done in the destructor.

   @param none
   @return none

 */
void CameraHardware:: release(int cameraid)
{
    stopPreview(cameraid);
    mCameraUsed[cameraid] = false;

    for(int i = 0; i < CAMERA_NUMBER; i++)
    {
        if(mCameraUsed[i] == true) {
            ALOGI("Camera used by id: %d, no release", i);
            return;
        }
    }
    mCamera->StopStreaming();
    mCamera->Uninit();
    /*
    if (mbAIT8431 && mAitXuHandle) {
        SkypeXU_Release(&mAitXuHandle);
    }
    */
    if (mbAIT8589 && mAitXuHandle) {
        AitXU_Release(&mAitXuHandle);
    }

    mCamera->Close();
}

/**
   @brief Dump state of the camera hardware

   @param[in] fd    File descriptor
   @param[in] args  Arguments
   @return NO_ERROR Dump succeeded
   @todo  Error codes for dump fail

 */
status_t  CameraHardware::dump(int fd) const {
    return NO_ERROR;
}

status_t CameraHardware::storeMetaDataInBuffers(bool enable) {
    //return android::INVALID_OPERATION;
    return android::OK;
}

} //namespace android
