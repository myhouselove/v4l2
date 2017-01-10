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

#define LOG_TAG "JpegDecoder"
#define LOG_NDDEBUG 0

#include <mmap.h>
#include <setjmp.h>
#include "JpegDecoder.h"


namespace android {

unsigned int JpegDecoder::mReadPAddr = 0;
unsigned int JpegDecoder::mReadLen = 0;
unsigned int JpegDecoder::mInterPAddr = 0;
unsigned int JpegDecoder::mInterLen = 0;
unsigned int JpegDecoder::mWritePAddr1 = 0;
unsigned int JpegDecoder::mWriteLen = 0;
unsigned int JpegDecoder::mCacheableWritePAddr1 = 0;
unsigned int JpegDecoder::mWritePAddr2 = 0;
unsigned int JpegDecoder::mCacheableWritePAddr2 = 0;
extern "C" {
#include "jpeglib.h"
}
JpegDecoder::JpegDecoder()
:mpVAddrRead(NULL),
mpVAddrInternal(NULL),
mpVAddrWrite1(NULL),
mpVAddrWrite2(NULL),
mpCacheableVAddrWrite1(NULL),
mpCacheableVAddrWrite2(NULL),
mInputAddr(NULL),
mInputLen(0),
mYUVPitch(0),
mYUVLen(0),
mYUVWidth(0),
mYUVHeight(0),
mCurrentPos(0),
mJpegMutex(-1),
mIsUseSwDecode(0),
mpSwDecodeYuvBuf(NULL),
mpSwDecodeRGBBuf(NULL),
mJpegMutexFail(0),
mYuvBuf1Empty(true),
mYuvBuf2Empty(true) {

}

JpegDecoder::~JpegDecoder() {
    if (mpSwDecodeYuvBuf != NULL)
        free(mpSwDecodeYuvBuf);

    if (mpSwDecodeRGBBuf != NULL)
        free(mpSwDecodeRGBBuf);
}

bool JpegDecoder::Init() {
    const mmap_info_t* minfo = NULL;
    char value1[PROPERTY_VALUE_MAX] = "";

    property_get("mstar.disableHW_JPEG", value1, "0");
    int disableJPEGHW = atoi(value1);

    if (mpSwDecodeYuvBuf == NULL) {
        mpSwDecodeYuvBuf = malloc(JPEG_WRITE_BUFF_SIZE);
    }
    memset(mpSwDecodeYuvBuf,0,JPEG_WRITE_BUFF_SIZE);

    if (mpSwDecodeRGBBuf == NULL) {
        mpSwDecodeRGBBuf = malloc(1280*720*4);
    }
    memset(mpSwDecodeRGBBuf,0,1280*720*4);

    if (disableJPEGHW)
        return false;

    if (mReadPAddr == 0 || mInterPAddr == 0 || mWritePAddr1 == 0 || mCacheableWritePAddr1 == 0) {
        if (mmap_init() != 0) {
            return false;
        }

        MDrv_SYS_GlobalInit();

        //the area in mmap locate in miu0
        minfo = mmap_get_info("E_MMAP_ID_JPD_READ");
        if (minfo == NULL) {
            ALOGE("mmap_get_info E_MMAP_ID_JPD_READ fail.");
        }

        if (minfo->size != 0) {
            if (!MsOS_MPool_Mapping(0, minfo->addr, minfo->size, 1)) {
                ALOGE("mapping read buf failed, %lx,%ld ",minfo->addr, minfo->size);
                return false;
            }
            mReadPAddr = minfo->addr;
            mReadLen = minfo->size;
        }

        minfo = mmap_get_info("E_MMAP_ID_PHOTO_INTER");
        if (minfo == NULL) {
            ALOGE("mmap_get_info E_MMAP_ID_PHOTO_INTER fail.");
        }

        if (minfo->size != 0) {
            if (!MsOS_MPool_Mapping(0, minfo->addr, minfo->size, 1)) {
                ALOGE("mapping internal buf failed, %lx,%ld ",minfo->addr, minfo->size);
                return false;
            }
            mInterPAddr = minfo->addr;
            mInterLen = minfo->size;
        }

        minfo = mmap_get_info("E_MMAP_ID_JPD_WRITE");
        if (minfo == NULL) {
            ALOGE("mmap_get_info E_MMAP_ID_JPD_WRITE fail.");
            return false;
        }
        //return false if the length in mmap is ZERO
        if (minfo->size == 0)
            return false;

        if (!MsOS_MPool_Mapping(0, minfo->addr, minfo->size, 1)) {
            ALOGE("mapping write buf failed, %lx,%ld ",minfo->addr, minfo->size);
            return false;
        }

        if (mReadPAddr == 0) {
            mReadPAddr = minfo->addr;
            mReadLen = JPEG_READ_BUFF_SIZE;

            //if E_DFB_JPD_READ not set, we assume that the E_DFB_JPD_INTERNAL doesn't set yet.
            mInterPAddr = mReadPAddr + JPEG_READ_BUFF_SIZE;
            mInterLen = JPEG_INTERNAL_BUFF_SIZE;

            mWritePAddr1 = mInterPAddr + JPEG_INTERNAL_BUFF_SIZE;
            mWriteLen = minfo->size - JPEG_READ_BUFF_SIZE - JPEG_INTERNAL_BUFF_SIZE;

            if (mWriteLen < JPEG_WRITE_BUFF_SIZE) {
                ALOGE("the write buf len %d is too short",mWriteLen);
                return false;
            }

        } else {
            if (minfo->size < JPEG_WRITE_BUFF_SIZE) {
                ALOGE("the write buf len %ld is too short",minfo->size);
                return false;
            }
            mWritePAddr1 = minfo->addr;
            mWriteLen = minfo->size / 2;
            mWritePAddr2 = minfo->addr + mWriteLen;

            if (!MsOS_MPool_Mapping(0, minfo->addr, minfo->size, 0)) {
                ALOGE("mapping cache write buf failed, %lx,%ld ",minfo->addr, minfo->size);
                return false;
            }
            mCacheableWritePAddr1 = mWritePAddr1;
            mCacheableWritePAddr2 = mWritePAddr2;
        }
    }

    ALOGD("[camera jpeg]: readbuf addr:0x%x, size: 0x%x\n write buff addr1:0x%x,  size: 0x%x\n write buff addr2:0x%x,  size: 0x%x\n internal buff addr:0x%x,   size: 0x%x\n",
          mReadPAddr, mReadLen, mWritePAddr1, mWriteLen, mWritePAddr2, mWriteLen, mInterPAddr, mInterLen);

    mpVAddrRead = (void *)MsOS_MPool_PA2KSEG1(mReadPAddr);
    mpVAddrInternal = (void *)MsOS_MPool_PA2KSEG1(mInterPAddr);
    mpVAddrWrite1 = (void *)MsOS_MPool_PA2KSEG1(mWritePAddr1);
    mpCacheableVAddrWrite1 = (void *)MsOS_MPool_PA2KSEG0(mCacheableWritePAddr1);
    mpVAddrWrite2 = (void *)MsOS_MPool_PA2KSEG1(mWritePAddr2);
    mpCacheableVAddrWrite2 = (void *)MsOS_MPool_PA2KSEG0(mCacheableWritePAddr2);

    mJpegMutex = MsOS_CreateNamedMutex((MS_S8 *)"SkiaDecodeMutex");
    ALOGI("jpeg hw mutex s_index = %d", mJpegMutex );
    if (mJpegMutex < 0) {
        ALOGE("create named mutex JpegDecodeMutex failed.");
        return false;
    }

    return true;
}

struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */
    jmp_buf setjmp_buffer;    /* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

static void my_error_exit (j_common_ptr cinfo) {
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    char buffer[JMSG_LENGTH_MAX];

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    //(*cinfo->err->output_message) (cinfo);

    (*cinfo->err->format_message) (cinfo, buffer);

    ALOGE("SW JPG DECODE ERR %s",buffer);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

void add_huff_table (j_decompress_ptr dinfo, JHUFF_TBL **htblptr, const UINT8 *bits, const UINT8 *val) {
    if (*htblptr == NULL)
        *htblptr = jpeg_alloc_huff_table((j_common_ptr) dinfo);

    memcpy((*htblptr)->bits, bits, sizeof(bits));
    memcpy((*htblptr)->huffval, val, sizeof(val));

    /* Initialize sent_table FALSE so table will be written to JPEG file. */
    (*htblptr)->sent_table = FALSE;
}

void std_huff_tables (j_decompress_ptr dinfo) {
    static const UINT8 bits_dc_luminance[17] =
    { /* 0-base */
        0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0   };
    static const UINT8 val_dc_luminance[] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11    };

    static const UINT8 bits_dc_chrominance[17] =
    { /* 0-base */
        0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0   };
    static const UINT8 val_dc_chrominance[] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11    };

    static const UINT8 bits_ac_luminance[17] =
    { /* 0-base */
        0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d    };
    static const UINT8 val_ac_luminance[] =
    {
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
        0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
        0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa  };
    static const UINT8 bits_ac_chrominance[17] =
    { /* 0-base */
        0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77    };
    static const UINT8 val_ac_chrominance[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
        0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
        0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa  };

    add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[0],
                   bits_dc_luminance, val_dc_luminance);
    add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[0],
                   bits_ac_luminance, val_ac_luminance);
    add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[1],
                   bits_dc_chrominance, val_dc_chrominance);
    add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[1],
                   bits_ac_chrominance, val_ac_chrominance);
}

/* Read JPEG image from a memory segment */
static void init_source (j_decompress_ptr cinfo) {
}
static boolean fill_input_buffer (j_decompress_ptr cinfo) {
    ERREXIT(cinfo, JERR_INPUT_EMPTY);
    return TRUE;
}
static void skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
    struct jpeg_source_mgr* src = (struct jpeg_source_mgr*) cinfo->src;

    if (num_bytes > 0) {
        src->next_input_byte += (size_t) num_bytes;
        src->bytes_in_buffer -= (size_t) num_bytes;
    }
}
static void term_source (j_decompress_ptr cinfo) {
}

static void jpeg_mem_src (j_decompress_ptr cinfo, void* buffer, long nbytes) {
    struct jpeg_source_mgr* src;

    if (cinfo->src == NULL) {   /* first time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)
                     (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                                 sizeof(struct jpeg_source_mgr));
    }

    src = (struct jpeg_source_mgr*) cinfo->src;
    src->init_source = init_source;
    src->fill_input_buffer = fill_input_buffer;
    src->skip_input_data = skip_input_data;
    src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->term_source = term_source;
    src->bytes_in_buffer = nbytes;
    src->next_input_byte = (JOCTET*)buffer;
}

bool JpegDecoder::Decode(void* dst, void* src, unsigned int len, void* buffer) {
    mInputAddr = src;
    mInputLen = len;
    mCurrentPos = 0;
    mYUVPitch = 0;
    mYUVLen = 0;
    mYUVWidth = 0;
    mYUVHeight = 0;

    mIsUseSwDecode = 0;

    if (!DecodeJPEGByHW(buffer)) {
        //TODO : fix sw path
        //Go to SW Decode
        /*if (mJpegMutexFail == 1) {

            mIsUseSwDecode = 1;

            if (!DecodeJPEGBySW(dst,src,len)) {
                ALOGE("SW Decode Failed");
                return false;
            } else {
                //To Do: Write ARGB to YUV422 convert code here to support Camera Recording
                return true;
            }
        } else {
            ALOGE("HW Decode Failed");
            return false;
        }*/
        ALOGE("HW Decode Failed");
        return false;
    }

/*
    if (dst != NULL) {
        yuv422_2_rgb8888_neon((unsigned char *)dst, (unsigned char *)mpCacheableVAddrWrite, mYUVWidth, mYUVHeight, mYUVPitch, mYUVWidth * 4);
    }
*/
    return true;
}

void* JpegDecoder::GetYuvBuffer() {
    Mutex::Autolock lock(mYuvBufLock);

    if (mIsUseSwDecode)
        return mpSwDecodeYuvBuf;
    else if (mYuvBuf1Empty == false)
        return mpCacheableVAddrWrite1;
    else if (mYuvBuf2Empty == false)
        return mpCacheableVAddrWrite2;

    return NULL;
}

void* JpegDecoder::GetEmptyYuvBuffer() {
    Mutex::Autolock lock(mYuvBufLock);

    if (mYuvBuf1Empty)
        return mpCacheableVAddrWrite1;
    else if (mYuvBuf2Empty)
        return mpCacheableVAddrWrite2;
    else
        return NULL;
}

void JpegDecoder::SetYuvBufferState(void* buffer, bool state) {
    Mutex::Autolock lock(mYuvBufLock);


    if (buffer == mpCacheableVAddrWrite1)
        mYuvBuf1Empty = state;
    else if (buffer == mpCacheableVAddrWrite2)
        mYuvBuf2Empty = state;
    else {
        ;
        //mYuvBuf1Empty = mYuvBuf2Empty = state;
    }
}


static void RGBA2BGRA( void* dst, void* src, unsigned int width, unsigned height) {
    unsigned int i = 0, j =0;
    unsigned char a,r,g,b;
    unsigned char * dstbuffer = (unsigned char *)dst;
    unsigned char * srcbuffer = (unsigned char *)src;
    unsigned char * tmp;

    for (j = 0; j < height; j++) {
        for (i = 1; i <= width ; i++) {
            tmp = srcbuffer + ((width - i) * 4);
            r = tmp[0];
            g = tmp[1];
            b = tmp[2];
            a = tmp[3];

            dstbuffer[0] = b;
            dstbuffer[1] = g;
            dstbuffer[2] = r;
            dstbuffer[3] = a;

            dstbuffer = dstbuffer + 4;
        }
        srcbuffer = srcbuffer + (width * 4);
    }
}

bool JpegDecoder::DecodeJPEGBySW(void* dst,void* src,unsigned int len) {
    jpeg_decompress_struct  cinfo;
    unsigned char* dstbuffer = (unsigned char*)mpSwDecodeRGBBuf;
    struct my_error_mgr jerr;

    if (dst == NULL) {
        ALOGE("SW JPEG ERR, no Dst Buffer");
        return false;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object, close the input file, and return.
         */
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, src, len);

    cinfo.mem->max_memory_to_use = 5 * 1024 * 1024;

    int status = jpeg_read_header(&cinfo, true);

    if (status != JPEG_HEADER_OK) {
        ALOGE("SW JPEG Decode Header Failed");
        return false;
    }

    if (cinfo.dc_huff_tbl_ptrs[0] == NULL) {
        //Add huffman table
        std_huff_tables(&cinfo);
    }

    cinfo.dct_method = JDCT_IFAST;
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;

    //cinfo.do_fancy_upsampling = 0;
    //cinfo.do_block_smoothing = 0;
    //cinfo.dither_mode = JDITHER_NONE;
    cinfo.out_color_space = JCS_RGBA_8888;


    if (!jpeg_start_decompress(&cinfo)) {
        ALOGE("SW JPEG Decode Start Decode fail");
        return false;
    }

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int row_stride = cinfo.output_width * cinfo.output_components;//cinfo.output_components;

    while (cinfo.output_scanline < cinfo.output_height) {
        int row_count = jpeg_read_scanlines(&cinfo, &dstbuffer, 1);
        if (0 == row_count) {
            ALOGE("SW JPEG Decode No Scan Line");
            return false;
        }
        dstbuffer += row_stride;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    RGBA2BGRA(dst,mpSwDecodeRGBBuf,width,height);

    return true;

}

bool JpegDecoder::DecodeJPEGByHW(void* buffer) {
    int s16JpegDecoderErrCode = 0;
    bool enRet = false;
    E_JPEG_DECODER_STATUS eWaitResult;
    JPEG_InitParam JpegInitParam;
    unsigned int u32length = 0;
    char value1[PROPERTY_VALUE_MAX] = "",value2[PROPERTY_VALUE_MAX] = "";
    int MinWidth, MinHeight, sampleSize;
    unsigned short u16ScaleFacotr = 1,u16AdjustScaleFactor = 1;
    unsigned short u16OriginalWidthAigned = 0, u16OriginalHeightAligned = 0;
    int s32TryTimes = 0;
#define TRY_MAX_TIMES 200

    sampleSize = 1;//this->getSampleSize();

    if (mReadPAddr == NULL || mReadLen == 0) {
        return false;
    }

    if (!MsOS_LockMutex(mJpegMutex, -1)) {
        mJpegMutexFail = 1;
        return false;
    } else {
        mJpegMutexFail = 0;
    }

#ifdef USE_OJPD
    MApi_JPEG_EnableOJPD(1);
#endif

    //ALOGI("~!~read=%p,len=%d;inter=%p,len=%d; write=%p,len=%d",mReadPAddr,mReadLen,mInterPAddr,mInterLen,mWritePAddr, mWriteLen);

    MApi_JPEG_SetMaxDecodeResolution(PHOTO_DECODE_JPEG_BASELINE_MAX_WIDTH, PHOTO_DECODE_JPEG_BASELINE_MAX_HEIGHT);
    MApi_JPEG_SetProMaxDecodeResolution(PHOTO_DECODE_JPEG_PROGRESSIVE_MAX_WIDTH, PHOTO_DECODE_JPEG_PROGRESSIVE_MAX_HEIGHT);
    MApi_JPEG_SetMPOMaxDecodeResolution(PHOTO_DECODE_JPEG_BASELINE_MAX_WIDTH, PHOTO_DECODE_JPEG_BASELINE_MAX_HEIGHT);
    MApi_JPEG_SetMPOProMaxDecodeResolution(PHOTO_DECODE_JPEG_PROGRESSIVE_MAX_WIDTH, PHOTO_DECODE_JPEG_PROGRESSIVE_MAX_HEIGHT);
    MApi_JPEG_SupportCMYK(FALSE);

    JpegInitParam.u32MRCBufAddr = mReadPAddr;
    JpegInitParam.u32MRCBufSize = mReadLen;
    if (buffer == mpCacheableVAddrWrite1)
        JpegInitParam.u32MWCBufAddr = mWritePAddr1;
    else
        JpegInitParam.u32MWCBufAddr = mWritePAddr2;
    JpegInitParam.u32MWCBufSize = mWriteLen;
    JpegInitParam.u32InternalBufAddr = mInterPAddr;
    JpegInitParam.u32InternalBufSize = mInterLen;

    memcpy(mpVAddrRead, mInputAddr, mInputLen);
    MsOS_FlushMemory();

    JpegInitParam.u32DecByteRead = mInputLen;
    mCurrentPos += mInputLen;

    JpegInitParam.u8DecodeType = E_JPEG_TYPE_MAIN;
    JpegInitParam.bInitMem = true;
    JpegInitParam.pFillHdrFunc = NULL;

#ifdef USE_OJPD
    MApi_JPEG_Init_UsingOJPD(&JpegInitParam);
#else
    MApi_JPEG_Init(&JpegInitParam);
#endif

    s16JpegDecoderErrCode = MApi_JPEG_GetErrorCode();
    if (s16JpegDecoderErrCode != E_JPEG_NO_ERROR) {
        JpegStop();
        ALOGE("jpeg goto fail 0, s16JpegDecoderErrCode = %d",s16JpegDecoderErrCode);
        goto fail;
    }

    u16OriginalWidthAigned = (MApi_JPEG_GetOriginalWidth()+31)&(~31);
    u16OriginalHeightAligned = (MApi_JPEG_GetOriginalHeight()+31)&(~31);

    if (u16OriginalWidthAigned*u16OriginalHeightAligned*2 > mWriteLen) {
        MsOS_MPool_Dcache_Flush((MS_U32)buffer, mWriteLen);
    } else {
        MsOS_MPool_Dcache_Flush((MS_U32)buffer, u16OriginalWidthAigned*u16OriginalHeightAligned*2);
    }

    if ((MApi_JPEG_GetScaleDownFactor()!=1 || sampleSize != 1) && (!MApi_JPEG_IsProgressive())) {
        //if buffer is enough, try to descrease scale factor
        if (((u16OriginalWidthAigned*u16OriginalHeightAligned*2)%mWriteLen) > 0) {
            u16ScaleFacotr = (u16OriginalWidthAigned*u16OriginalHeightAligned*2) / mWriteLen + 1;
        } else {
            u16ScaleFacotr = (u16OriginalWidthAigned*u16OriginalHeightAligned*2) / mWriteLen;
        }

        if (u16ScaleFacotr <= 1) {
            u16AdjustScaleFactor = 1;
        } else if (u16ScaleFacotr <= 4) {
            u16AdjustScaleFactor = 2;
        } else if (u16ScaleFacotr <= 16) {
            u16AdjustScaleFactor = 4;
        } else {
            u16AdjustScaleFactor = 8;
        }
        //LOGD("u16AdjustScaleFactor:%d",u16AdjustScaleFactor);
        //try to closer to samplesize
        while ((sampleSize > u16AdjustScaleFactor) && (u16AdjustScaleFactor <= 8)) {
            u16AdjustScaleFactor = u16AdjustScaleFactor<<1;
        }

        if (u16AdjustScaleFactor != sampleSize) {
            //ALOGD("not support samplesize:%d", sampleSize);
            goto fail;
        }
        //ALOGD("sampleSize:%d,u16AdjustScaleFactor:%d",sampleSize,u16AdjustScaleFactor);
        if (MApi_JPEG_GetScaleDownFactor() != u16AdjustScaleFactor) {
            //ALOGI("adjust scale down factor:%d->%d", (int)MApi_JPEG_GetScaleDownFactor(),u16AdjustScaleFactor);
            //ALOGI("orignal size:%d,%d.",u16OriginalWidthAigned,u16OriginalHeightAligned);
            JpegStop();
            MApi_JPEG_SetMaxDecodeResolution(u16OriginalWidthAigned / u16AdjustScaleFactor, u16OriginalHeightAligned / u16AdjustScaleFactor);
            MApi_JPEG_SetProMaxDecodeResolution(u16OriginalWidthAigned / u16AdjustScaleFactor, u16OriginalHeightAligned / u16AdjustScaleFactor);
            MApi_JPEG_SetMPOMaxDecodeResolution(u16OriginalWidthAigned / u16AdjustScaleFactor, u16OriginalHeightAligned / u16AdjustScaleFactor);
            MApi_JPEG_SetMPOProMaxDecodeResolution(u16OriginalWidthAigned / u16AdjustScaleFactor, u16OriginalHeightAligned / u16AdjustScaleFactor);
            MApi_JPEG_SupportCMYK(FALSE);
            MApi_JPEG_Init(&JpegInitParam);
            s16JpegDecoderErrCode = MApi_JPEG_GetErrorCode();
            if (s16JpegDecoderErrCode != E_JPEG_NO_ERROR) {
                ALOGE("jpeg goto fail 4, s16JpegDecoderErrCode = %d",s16JpegDecoderErrCode);
                goto fail;
            }
        }
    }


    mYUVWidth = (unsigned int)MApi_JPEG_GetAlignedWidth();
    mYUVHeight = (unsigned int)MApi_JPEG_GetAlignedHeight();
    mYUVPitch = (unsigned int)MApi_JPEG_GetAlignedPitch()*2;
    mYUVLen = mYUVPitch * mYUVHeight;

    property_get("mstar.jpeg_width", value1, "320");
    property_get("mstar.jpeg_height", value2, "240");
    MinWidth = atoi(value1);
    MinHeight = atoi(value2);
    if ((mYUVWidth * mYUVHeight) < (unsigned int)(MinWidth * MinHeight)) {
        //too small, to sw decoding.
        enRet = false;
        goto end;
    }

    if (MApi_JPEG_DecodeHdr() == E_JPEG_FAILED) {
        s16JpegDecoderErrCode = MApi_JPEG_GetErrorCode();
        ALOGE("jpeg goto fail 1");
        goto fail;
    }


    switch (MApi_JPEG_Decode()) {
        case E_JPEG_DONE:
            break;

        case E_JPEG_OKAY:
            goto wait;
            break;

        case E_JPEG_FAILED:
            s16JpegDecoderErrCode = MApi_JPEG_GetErrorCode();
            ALOGE("jpeg goto fail 2, s16JpegDecoderErrCode= %d ",s16JpegDecoderErrCode);
            goto fail;

        default:
            break;
    }

    wait:
    eWaitResult = JpegWaitDone();

    while ((eWaitResult == DECODING) && (s32TryTimes++ < TRY_MAX_TIMES)) {
        usleep(3000);
        eWaitResult = JpegWaitDone();
    }

    switch (eWaitResult) {
        case DECODE_DONE:
            enRet = true;
            break;
        case DECODE_ERR:
        default:
            s16JpegDecoderErrCode = E_JPEG_DECODE_ERROR;
            ALOGE("jpeg goto fail 3");
            if (s32TryTimes >= TRY_MAX_TIMES) {
                ALOGE("jpeg decode time out.\n");
            }
            goto fail;
    }
    fail:
    if (s16JpegDecoderErrCode) {
        ALOGE("[Camera]:Decode jpeg fail, s16JpegDecoderErrCode = %d \n",s16JpegDecoderErrCode);
    }
    end:
    JpegStop();
#ifdef USE_OJPD
    MApi_JPEG_EnableOJPD(0);
#endif

    MsOS_UnlockMutex(mJpegMutex, 0);

    return enRet;
}

void JpegDecoder::JpegStop(void) {
    MApi_JPEG_Rst();
    MApi_JPEG_Exit();
}

E_JPEG_DECODER_STATUS JpegDecoder::JpegWaitDone() {
    E_JPEG_DECODER_STATUS enStatus = DECODING;
    JPEG_Event enEvent;

    //For H/W bug, Check Vidx.
    if (E_JPEG_FAILED == MApi_JPEG_HdlVidxChk()) {
        enEvent = E_JPEG_EVENT_DEC_ERROR_MASK;
    } else {
        enEvent = MApi_JPEG_GetJPDEventFlag();
    }

    if (E_JPEG_EVENT_DEC_DONE & enEvent) {
        enStatus = DECODE_DONE;
    } else if (E_JPEG_EVENT_DEC_ERROR_MASK & enEvent) {
        ALOGE("[Camera]:Baseline decode error\n");
        JpegStop();
        enStatus = DECODE_ERR;
    }

    return enStatus;
}

bool JpegDecoder::is1080pSupport() {
    const mmap_info_t* minfo = NULL;

    minfo = mmap_get_info("E_MMAP_ID_JPD_WRITE");
    if (minfo == NULL) {
        ALOGE("mmap_get_info E_MMAP_ID_JPD_WRITE fail.");
        return false;
    }

    if (minfo->size < JPEG_1080P_BUFFUER_SIZE * 2) {
        ALOGI("jpd doesn't support 1080p : 0x%x", minfo->size);
        return false;
    }

    return true;
}


}; // namespace android
