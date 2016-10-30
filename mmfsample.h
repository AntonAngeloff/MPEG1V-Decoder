/**
 * @author Anton Angelov (ant0n@mail.bg)
 */

#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "mmfutil.h"

#define max_buffer_count 8

typedef enum MMFSampleFormat {
    SAMPLE_FORMAT_NONE      = 0x0,      //!< SAMPLE_FORMAT_NONE

	SAMPLE_FORMAT_RGBA32    = 0x100,       //!< SAMPLE_FORMAT_RGBA32
	SAMPLE_FORMAT_NV12,                    //!< SAMPLE_FORMAT_NV12
	SAMPLE_FORMAT_YUV444P,                 //!< SAMPLE_FORMAT_YUV444P
	SAMPLE_FORMAT_YUV420P,                 //!< SAMPLE_FORMAT_YUV420P

	SAMPLE_FORMAT_PCM_S16P  = 0x200,       //!< SAMPLE_FORMAT_PCM_S16P
	SAMPLE_FORMAT_FORCE_DWORD = 0xFFFFFFFF,//!< SAMPLE_FORMAT_FORCE_DWORD
} MMFSampleFormat;

typedef struct {
	/**
	 * Number of (sub)buffers
	 */
	int32_t buffer_count;

	/**
	 * - for video this is pixel format
	 * - for audio it's sample format
	 */
	MMFSampleFormat format;

	//Sample presentation time stamp
	int64_t pts, pkt_pts;

	//Decoder time stamp
	int64_t dts, pkt_dts;

	//Sample duration
	int64_t duration, pkt_dur;

	//Array of pointers to (sub)buffer's data
	void* buffer_data[max_buffer_count];

	/**
	 * Strides of (sub)buffers
	 */
	int32_t buffer_stride[max_buffer_count];

    /**
     * Each buffer may have additional flags
     */
    #define SAMPLE_BUFFER_FLAGS_NONE 0
    #define SAMPLE_BUFFER_FLAGS_DONT_RELEASE 1
	int32_t buffer_flags[max_buffer_count];

    //Size of video frame (this applies only for video sample type
    int32_t width, height;
} MMFSample;

/**
 * Function for allocating and initializing new sample structure
 * @param ppSample Pointer to a pointer variable, which will receive the new struct address
 * @return RC_OK on success, RC_OUTOFMEM when there is no sufficient memory.
 */
MMFRES mmf_sample_allocate(MMFSample **ppSample);

/**
 * Releases a sample structure and all it's related sub-resources. On success, the value of *pSample is set to NULL.
 * @param ppSample Pointer to a pointer variable, which points at the sample struct.
 * @return RC_OK on success. Different return code, otherwise.
 */
MMFRES mmf_sample_free(MMFSample **ppSample);
MMFRES mmf_allocate_video_frame(MMFSampleFormat fmt, int w, int h, MMFSample **ppSample);

MMFRES mmf_sample_copy_plane(void *src, int src_stride, void *dst, int dst_stride, int bytewidth, int h);
MMFRES mmf_sample_read_plane(FILE *src, int src_stride, void *dst, int dst_stride, int bytewidth, int h);
MMFRES mmf_sample_write_plane(FILE *dst, int dst_stride, void *src, int src_stride, int bytewidth, int h);
