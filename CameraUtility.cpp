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

#include <stdlib.h>
#include <stdint.h>
#include <MsTypes.h>
#include <drvDIP.h>
#include "CameraUtility.h"
#include "jpeglib.h"
/*
extern "C" {
#include "jpeglib.h"

#ifdef __ARM_NEON__
    void yuv422_2_rgb8888_neon(uint8_t *dst_ptr,
                               const uint8_t *src_ptr,
                               int width,
                               int height,
                               int yuv_pitch,
                               int rgb_pitch);
    void yuv422_2_nv21_neon(uint8_t *dst_yptr, uint8_t *dst_uvptr, const uint8_t *src_ptr, int width, int height, int yuv_pitch);
#endif /* __ARM_NEON__ */
//}

void yuv_to_rgb16(unsigned char y, unsigned char u, unsigned char v, unsigned char *rgb) {
    int r,g,b;
    int z;
    int rgb16;

    z = 0;

    r = 1.164 * (y - 16) + 1.596 * (v - 128);
    g = 1.164 * (y - 16) - 0.813 * (v - 128) - 0.391 * (u -128);
    b = 1.164 * (y - 16) + 2.018 * (u - 128);

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    rgb16 = (int)(((r >> 3)<<11) | ((g >> 2) << 5)| ((b >> 3) << 0));

    *rgb = (unsigned char)(rgb16 & 0xFF);
    rgb++;
    *rgb = (unsigned char)((rgb16 & 0xFF00) >> 8);
}

void convert(unsigned char *buf, unsigned char *rgb, int width, int height) {
    int x,y,z=0;
    int blocks;

    blocks = (width * height) * 2;

    for (y = 0; y < blocks; y+=4) {
        unsigned char Y1, Y2, U, V;

        Y1 = buf[y + 0];
        U = buf[y + 1];
        Y2 = buf[y + 2];
        V = buf[y + 3];

        yuv_to_rgb16(Y1, U, V, &rgb[y]);
        yuv_to_rgb16(Y2, U, V, &rgb[y + 2]);
    }

}
/*
#ifdef __ARM_NEON__
void fastconvert(unsigned char *buf, unsigned char *rgb, int width, int height) {
    int yuv_pitch = width * 2;
    int rgb_pitch = width * 4;

    yuv422_2_rgb8888_neon(rgb, buf, width, height, yuv_pitch, rgb_pitch);
}

void yuv422tonv21(unsigned char *buf, unsigned char *nv21, int width, int height) {
    unsigned char *uvptr = nv21 + width * height;
    int yuv_pitch = width * 2;

    yuv422_2_nv21_neon(nv21, uvptr, buf, width, height, yuv_pitch);
}
#endif
*/
//come from ics\device\samsung\crespo\libs3cjpeg\JpegEncoder.cpp
bool scaleDownYuv422(unsigned char *srcBuf, uint32_t srcWidth, uint32_t srcHight,
                     unsigned char *dstBuf, uint32_t dstWidth, uint32_t dstHight) {
    int32_t step_x, step_y;
    int32_t iXsrc, iXdst;
    int32_t x, y, src_y_start_pos, dst_pos, src_pos;

    if (dstWidth % 2 != 0 || dstHight % 2 != 0) {
        return false;
    }

    step_x = srcWidth / dstWidth;
    step_y = srcHight / dstHight;

    dst_pos = 0;
    for (uint32_t y = 0; y < dstHight; y++) {
        src_y_start_pos = (y * step_y * (srcWidth * 2));

        for (uint32_t x = 0; x < dstWidth; x += 2) {
            src_pos = src_y_start_pos + (x * (step_x * 2));

            dstBuf[dst_pos++] = srcBuf[src_pos    ];
            dstBuf[dst_pos++] = srcBuf[src_pos + 1];
            dstBuf[dst_pos++] = srcBuf[src_pos + 2];
            dstBuf[dst_pos++] = srcBuf[src_pos + 3];
        }
    }

    return true;
}

int saveYUYVtoJPEG (unsigned char *inputBuffer, int width, int height, FILE *file, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int z;
    int fileSize;

    line_buffer = (unsigned char *) calloc (width * 3, 1);
    yuyv = inputBuffer;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jpeg_stdio_dest (&cinfo, file);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);

    z = 0;
    while (cinfo.next_scanline < cinfo.image_height) {
        int x;
        unsigned char *ptr = line_buffer;

        for (x = 0; x < width; x++) {
            int r, g, b;
            int y, u, v;

            if (!z)
                y = yuyv[0] << 8;
            else
                y = yuyv[2] << 8;

            u = yuyv[1] - 128;
            v = yuyv[3] - 128;

            r = (y + (359 * v)) >> 8;
            g = (y - (88 * u) - (183 * v)) >> 8;
            b = (y + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if (z++) {
                z = 0;
                yuyv += 4;
            }
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines (&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress (&cinfo);
    fileSize = ftell(file);
    jpeg_destroy_compress (&cinfo);

    free (line_buffer);

    return fileSize;
}


void
yuyv422_to_yuv420(unsigned char *bufsrc, unsigned char *bufdest, int width, int height) {
    unsigned char *ptrsrcy1, *ptrsrcy2;
    unsigned char *ptrsrcy3, *ptrsrcy4;
    unsigned char *ptrsrccb1, *ptrsrccb2;
    unsigned char *ptrsrccb3, *ptrsrccb4;
    unsigned char *ptrsrccr1, *ptrsrccr2;
    unsigned char *ptrsrccr3, *ptrsrccr4;
    int srcystride, srcccstride;

    ptrsrcy1  = bufsrc ;
    ptrsrcy2  = bufsrc + (width<<1) ;
    ptrsrcy3  = bufsrc + (width<<1)*2 ;
    ptrsrcy4  = bufsrc + (width<<1)*3 ;

    ptrsrccb1 = bufsrc + 1;
    ptrsrccb2 = bufsrc + (width<<1) + 1;
    ptrsrccb3 = bufsrc + (width<<1)*2 + 1;
    ptrsrccb4 = bufsrc + (width<<1)*3 + 1;

    ptrsrccr1 = bufsrc + 3;
    ptrsrccr2 = bufsrc + (width<<1) + 3;
    ptrsrccr3 = bufsrc + (width<<1)*2 + 3;
    ptrsrccr4 = bufsrc + (width<<1)*3 + 3;

    srcystride  = (width<<1)*3;
    srcccstride = (width<<1)*3;

    unsigned char *ptrdesty1, *ptrdesty2;
    unsigned char *ptrdesty3, *ptrdesty4;
    unsigned char *ptrdestcb1, *ptrdestcb2;
    unsigned char *ptrdestcr1, *ptrdestcr2;
    int destystride, destccstride;

    ptrdesty1 = bufdest;
    ptrdesty2 = bufdest + width;
    ptrdesty3 = bufdest + width*2;
    ptrdesty4 = bufdest + width*3;

    int uvstride = ((width-1)/32+1)*16;

    destystride  = (width)*3;
    destccstride = uvstride;

    ptrdestcr1 = bufdest + width*height;
    ptrdestcr2 = bufdest + width*height + uvstride;

    ptrdestcb1 = bufdest + width*height + ((uvstride*height) >> 1);
    ptrdestcb2 = bufdest + width*height + ((uvstride*height) >> 1) + uvstride;


    int i, j;

    for (j=0; j<(height/4); j++) {
        for (i=0;i<(width/2);i++) {
            (*ptrdesty1++) = (*ptrsrcy1);
            (*ptrdesty2++) = (*ptrsrcy2);
            (*ptrdesty3++) = (*ptrsrcy3);
            (*ptrdesty4++) = (*ptrsrcy4);

            ptrsrcy1 += 2;
            ptrsrcy2 += 2;
            ptrsrcy3 += 2;
            ptrsrcy4 += 2;

            (*ptrdesty1++) = (*ptrsrcy1);
            (*ptrdesty2++) = (*ptrsrcy2);
            (*ptrdesty3++) = (*ptrsrcy3);
            (*ptrdesty4++) = (*ptrsrcy4);

            ptrsrcy1 += 2;
            ptrsrcy2 += 2;
            ptrsrcy3 += 2;
            ptrsrcy4 += 2;

            (*ptrdestcb1++) = (*ptrsrccb1);
            (*ptrdestcb2++) = (*ptrsrccb3);

            ptrsrccb1 += 4;
            ptrsrccb3 += 4;

            (*ptrdestcr1++) = (*ptrsrccr1);
            (*ptrdestcr2++) = (*ptrsrccr3);

            ptrsrccr1 += 4;
            ptrsrccr3 += 4;

        }

        for (i=(width/2);i<destccstride;i++) {
            *ptrdestcb1++;
            *ptrdestcb2++;
            *ptrdestcr1++;
            *ptrdestcr2++;
        }

        /* Update src pointers */
        ptrsrcy1  += srcystride;
        ptrsrcy2  += srcystride;
        ptrsrcy3  += srcystride;
        ptrsrcy4  += srcystride;

        ptrsrccb1 += srcccstride;
        ptrsrccb3 += srcccstride;

        ptrsrccr1 += srcccstride;
        ptrsrccr3 += srcccstride;


        /* Update dest pointers */
        ptrdesty1 += destystride;
        ptrdesty2 += destystride;
        ptrdesty3 += destystride;
        ptrdesty4 += destystride;

        ptrdestcb1 += destccstride;
        ptrdestcb2 += destccstride;

        ptrdestcr1 += destccstride;
        ptrdestcr2 += destccstride;

    }
}

void
yuyv422_to_yuv420sp(unsigned char *bufsrc, unsigned char *bufdest, int width, int height) {
    unsigned char *ptrsrcy1, *ptrsrcy2;
    unsigned char *ptrsrcy3, *ptrsrcy4;
    unsigned char *ptrsrccb1, *ptrsrccb2;
    unsigned char *ptrsrccb3, *ptrsrccb4;
    unsigned char *ptrsrccr1, *ptrsrccr2;
    unsigned char *ptrsrccr3, *ptrsrccr4;
    int srcystride, srcccstride;

    ptrsrcy1  = bufsrc ;
    ptrsrcy2  = bufsrc + (width<<1) ;
    ptrsrcy3  = bufsrc + (width<<1)*2 ;
    ptrsrcy4  = bufsrc + (width<<1)*3 ;

    ptrsrccb1 = bufsrc + 1;
    ptrsrccb2 = bufsrc + (width<<1) + 1;
    ptrsrccb3 = bufsrc + (width<<1)*2 + 1;
    ptrsrccb4 = bufsrc + (width<<1)*3 + 1;

    ptrsrccr1 = bufsrc + 3;
    ptrsrccr2 = bufsrc + (width<<1) + 3;
    ptrsrccr3 = bufsrc + (width<<1)*2 + 3;
    ptrsrccr4 = bufsrc + (width<<1)*3 + 3;

    srcystride  = (width<<1)*3;
    srcccstride = (width<<1)*3;

    unsigned char *ptrdesty1, *ptrdesty2;
    unsigned char *ptrdesty3, *ptrdesty4;
    unsigned char *ptrdestcb1, *ptrdestcb2;
    unsigned char *ptrdestcr1, *ptrdestcr2;
    int destystride, destccstride;

    ptrdesty1 = bufdest;
    ptrdesty2 = bufdest + width;
    ptrdesty3 = bufdest + width*2;
    ptrdesty4 = bufdest + width*3;

    ptrdestcb1 = bufdest + width*height;
    ptrdestcb2 = bufdest + width*height + width;

    ptrdestcr1 = bufdest + width*height + 1;
    ptrdestcr2 = bufdest + width*height + width + 1;

    destystride  = (width)*3;
    destccstride = width;

    int i, j;

    for (j=0; j<(height/4); j++) {
        for (i=0;i<(width/2);i++) {
            (*ptrdesty1++) = (*ptrsrcy1);
            (*ptrdesty2++) = (*ptrsrcy2);
            (*ptrdesty3++) = (*ptrsrcy3);
            (*ptrdesty4++) = (*ptrsrcy4);

            ptrsrcy1 += 2;
            ptrsrcy2 += 2;
            ptrsrcy3 += 2;
            ptrsrcy4 += 2;

            (*ptrdesty1++) = (*ptrsrcy1);
            (*ptrdesty2++) = (*ptrsrcy2);
            (*ptrdesty3++) = (*ptrsrcy3);
            (*ptrdesty4++) = (*ptrsrcy4);

            ptrsrcy1 += 2;
            ptrsrcy2 += 2;
            ptrsrcy3 += 2;
            ptrsrcy4 += 2;

            (*ptrdestcr1) = (*ptrsrccb1);
            (*ptrdestcr2) = (*ptrsrccb3);
            ptrdestcr1 += 2;
            ptrdestcr2 += 2;

            ptrsrccb1 += 4;
            ptrsrccb3 += 4;

            (*ptrdestcb1) = (*ptrsrccr1);
            (*ptrdestcb2) = (*ptrsrccr3);
            ptrdestcb1 += 2;
            ptrdestcb2 += 2;

            ptrsrccr1 += 4;
            ptrsrccr3 += 4;

        }


        /* Update src pointers */
        ptrsrcy1  += srcystride;
        ptrsrcy2  += srcystride;
        ptrsrcy3  += srcystride;
        ptrsrcy4  += srcystride;

        ptrsrccb1 += srcccstride;
        ptrsrccb3 += srcccstride;

        ptrsrccr1 += srcccstride;
        ptrsrccr3 += srcccstride;


        /* Update dest pointers */
        ptrdesty1 += destystride;
        ptrdesty2 += destystride;
        ptrdesty3 += destystride;
        ptrdesty4 += destystride;

        ptrdestcb1 += destccstride;
        ptrdestcb2 += destccstride;

        ptrdestcr1 += destccstride;
        ptrdestcr2 += destccstride;

    }
}
