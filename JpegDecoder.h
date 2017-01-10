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

#ifndef __JPEG_DECODER_H__
#define __JPEG_DECODER_H__

#include <utils/threads.h>
//#include "jpeglib.h"
#include "apiJPEG.h"

namespace android {

#define JPEG_WRITE_BUFF_SIZE 0x3FC000
#define JPEG_READ_BUFF_SIZE  0x50080
#define JPEG_INTERNAL_BUFF_SIZE  0xA0000
#define PHOTO_DECODE_JPEG_BASELINE_MAX_WIDTH        (1920)
#define PHOTO_DECODE_JPEG_BASELINE_MAX_HEIGHT       (1080)

#define PHOTO_DECODE_JPEG_PROGRESSIVE_MAX_WIDTH     (1024)
#define PHOTO_DECODE_JPEG_PROGRESSIVE_MAX_HEIGHT    (768)

#define PHOTO_DECODE_MIN_WIDTH                      (160)
#define PHOTO_DECODE_MIN_HEIGHT                     (120)
#define JPEG_1080P_BUFFUER_SIZE                     0x3F4800

typedef enum {
    DECODE_DONE = 0,
    DECODING,
    DECODE_ERR,
} E_JPEG_DECODER_STATUS;

typedef enum {
    E_MP_TYPE_BASELINE             = 0x030000
    , E_MP_TYPE_LARGE_THUMBNAIL_CLASS1           = 0x010001
    , E_MP_TYPE_LARGE_THUMBNAIL_CLASS2        = 0x010002
    , E_MP_TYPE_MULTI_FRAME_PANORAMA  = 0x020001
    , E_MP_TYPE_MULTI_FRAME_DISPARITY        = 0x020002
    , E_MP_TYPE_MULTI_FRAME_MULTI_ANGLE        = 0x020003
    , E_MP_TYPE_MASK = 0x00FFFFFF
} MP_TYPE_CODE;
extern "C" {
#ifdef __ARM_NEON__
    void yuv422_2_rgb8888_neon(uint8_t *dst_ptr,
                               const uint8_t *src_ptr,
                               int width,
                               int height,
                               int yuv_pitch,
                               int rgb_pitch);
#endif /* __ARM_NEON__ */
}

class JpegDecoder {
public:
    JpegDecoder();
    ~JpegDecoder();

    bool                    Init();
    bool                    Decode(void* dst, void* src, unsigned int len, void* buffer);
    void*                   GetYuvBuffer();
    void*                   GetEmptyYuvBuffer();
    void                    SetYuvBufferState(void* buffer, bool empty);
    bool                    is1080pSupport();

private:
    bool                    DecodeJPEGByHW(void* buffer);
    bool                    DecodeJPEGBySW(void* dst,void* src,unsigned int len);
    void                    JpegStop();
    E_JPEG_DECODER_STATUS   JpegWaitDone();

    static unsigned int     mReadPAddr;
    static unsigned int     mReadLen;
    void*                   mpVAddrRead;

    static unsigned int     mInterPAddr;
    static unsigned int     mInterLen;
    void*                   mpVAddrInternal;

    static unsigned int     mWritePAddr1;
    static unsigned int     mWritePAddr2;
    static unsigned int     mWriteLen;
    static unsigned int     mCacheableWritePAddr1;
    static unsigned int     mCacheableWritePAddr2;
    void*                   mpVAddrWrite1;
    void*                   mpVAddrWrite2;
    void*                   mpCacheableVAddrWrite1;
    void*                   mpCacheableVAddrWrite2;

    void*                   mInputAddr;
    unsigned int            mInputLen;

    unsigned int            mYUVPitch;
    unsigned int            mYUVLen;
    unsigned int            mYUVWidth;
    unsigned int            mYUVHeight;

    unsigned int            mCurrentPos;
    int                     mJpegMutex;
    bool                    mIsUseSwDecode;
    void*                   mpSwDecodeYuvBuf;
    void*                   mpSwDecodeRGBBuf;
    int                     mJpegMutexFail;
    bool                    mYuvBuf1Empty;
    bool                    mYuvBuf2Empty;
    Mutex                   mYuvBufLock;
};

}; // namespace android

#endif // __JPEG_DECODER_H__
