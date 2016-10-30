#include "mmfsample.h"
#include "string.h"
#include "stddef.h"

MMFRES mmf_sample_allocate(MMFSample **ppSample)
{
	//allocate new struct
	if ( (*ppSample = mmf_alloc(sizeof(MMFSample))) == NULL) {
		return RC_OUTOFMEM; //return out of mem error
	}

	//Initialize defaults to sample
    memset(*ppSample, 0, sizeof(MMFSample));

    return RC_OK;
}

MMFRES mmf_sample_free(MMFSample **ppSample)
{
    if (*ppSample == NULL) {
        //Already freed
        return RC_OK;
    }

    //Iterate (sub)buffers
    int i;
    for(i=0; i<(*ppSample)->buffer_count; i++) {
        if( (*ppSample)->buffer_data[i] ) {
            //Don't release buffers, which are explicitly marked with this flag
            if( ((*ppSample)->buffer_flags[i] & SAMPLE_BUFFER_FLAGS_DONT_RELEASE) == 0) {
                mmf_free((*ppSample)->buffer_data[i]);
            }
        }
    }

    //Free the struct
    mmf_free(*ppSample);
    *ppSample = NULL;

    return RC_OK;
}

MMFRES mmf_allocate_video_frame(MMFSampleFormat fmt, int w, int h, MMFSample **ppSample)
{
    if(*ppSample == NULL) {
        //Allocate new sample struct
        MMFRES rc = mmf_sample_allocate(ppSample);

        //Failed to allocate memory?
        if (rc >= 1) {
            return rc;
        }
    }

    MMFSample *pS = *ppSample;

    //Assign new format
    pS->format = fmt;

    //Allocate (sub)buffers according to the sample format
    switch (fmt) {
        /* Handle rgba32 format */
        case SAMPLE_FORMAT_RGBA32:
            //We have only one (sub)buffer
            pS->buffer_count = 1;

            pS->buffer_stride[0] = w * 4;
            pS->buffer_data[0] = mmf_alloc( pS->buffer_stride[0] * h );
            pS->buffer_flags[0] = SAMPLE_BUFFER_FLAGS_NONE;

            //Failed to allocate buffer?
            if (!pS->buffer_data[0]) {
                return RC_OUTOFMEM;
            }

            break;


        /* Handle NV12 */
        case SAMPLE_FORMAT_NV12:
            //NV12 does not support odd dimensions
            if(w%2 || h%2) {
                return RC_INVALIDARG;
            }

            //We have two planes: Y and U-V (interleaved)
            pS->buffer_count = 2;

            //Allocate buffer for picture (NV12 is 12bit format)
            void *pbuf = mmf_alloc( w * h + (w * h / 2) );

            //Check allocation result
            if (!pbuf) {
                return RC_OUTOFMEM;
            }

            //Attach picture to (sub)buffer pointers
            pS->buffer_stride[0] = w; //1 byte per Y component
            pS->buffer_data[0] = pbuf;
            pS->buffer_flags[0] = SAMPLE_BUFFER_FLAGS_NONE;

            //Attach the UV part of the picture to the second (sub)buffer
            pS->buffer_stride[1] = w; //1 byte per UV component (but height is half)
            pS->buffer_data[1] = pbuf+(w*h);
            pS->buffer_flags[1] = SAMPLE_BUFFER_FLAGS_DONT_RELEASE;

            break;

        /* Handle YUV420p */
        case SAMPLE_FORMAT_YUV420P:
            //This format does not support odd dimensions
            if(w%2 || h%2) {
                return RC_INVALIDARG;
            }

            //We have three planes: Y, U and V
            pS->buffer_count = 3;

            //Allocate buffer for picture (NV12 is 12bit format)
            void *pbuf_yuv = mmf_alloc( w * h + (w * h / 2) );

            //Check allocation result
            if (!pbuf_yuv) {
                return RC_OUTOFMEM;
            }

            //Attach picture to (sub)buffer pointers
            pS->buffer_stride[0] = w; //1 byte per Y component
            pS->buffer_data[0] = pbuf_yuv;
            pS->buffer_flags[0] = SAMPLE_BUFFER_FLAGS_NONE;

            //Attach the U part of the picture to the second (sub)buffer
            pS->buffer_stride[1] = w/2; //1 byte per UV component (but height is half)
            pS->buffer_data[1] = pbuf_yuv+(w*h);
            pS->buffer_flags[1] = SAMPLE_BUFFER_FLAGS_DONT_RELEASE;

            //Attach the V part of the picture to the third (sub)buffer
            pS->buffer_stride[2] = w/2; //1 byte per UV component (but height is half)
            pS->buffer_data[2] = pS->buffer_data[1] + (w*h/4);
            pS->buffer_flags[2] = SAMPLE_BUFFER_FLAGS_DONT_RELEASE;
            break;

        /* Handle if format is unknown */
        default:
            //Invalid argument
            return RC_INVALIDARG;
    }

    pS->width = w;
    pS->height = h;

    //Success
    return RC_OK;
}

MMFRES mmf_sample_copy_plane(void *src, int src_stride, void *dst, int dst_stride, int bytewidth, int h)
{
    if(!src || !dst) {
        return RC_INVALIDARG;
    }

    void *s = src, *d = dst;

    //Make sure we receive valid strides
    mmf_assert( abs(src_stride) >= bytewidth && abs(dst_stride) >= bytewidth );

    if (src_stride == dst_stride && src_stride == bytewidth) {
        //If both strides are identical, copy whole plane
        memcpy(s, d, bytewidth*h);
    } else {
        //Copy plane line by line
        for(; h>0; h--) {
            memcpy(s, d, bytewidth);

            s+=src_stride;
            d+=dst_stride;
        }
    }

    return RC_OK;
}

MMFRES mmf_sample_read_plane(FILE *src, int src_stride, void *dst, int dst_stride, int bytewidth, int h)
{
    if(!src || !dst) {
        return RC_INVALIDARG;
    }

    void *d = dst;

    #ifdef debug
    //Make sure we receive valid strides
    mmf_assert( abs(src_stride) >= bytewidth && abs(dst_stride) >= bytewidth );
    #endif

    if (src_stride == dst_stride && src_stride == bytewidth) {
        //If both strides are identical, copy whole plane
        fread(d, bytewidth*h, 1, src);
        //memcpy(s, d, bytewidth*h);
    } else {
        //Copy plane line by line
        for(; h>0; h--) {
            fread(d, bytewidth, 1, src);
            //memcpy(s, d, bytewidth);

            if(src_stride > bytewidth) {
                fseek(src, src_stride-bytewidth, SEEK_CUR);
            }

            //s+=src_stride;
            d+=dst_stride;
        }
    }

    return RC_OK;
}

MMFRES mmf_sample_write_plane(FILE *dst, int dst_stride, void *src, int src_stride, int bytewidth, int h)
{
    size_t bytes;

    if(!src || !dst) {
        return RC_INVALIDARG;
    }

    void *s = src;

    //Copy plane line by line
    for(; h>0; h--) {
        bytes  = fwrite(s, 1, bytewidth, dst);

        if(src_stride > bytewidth) {
            /* Write n zeroes to file */
            int8_t z = 0;
            fwrite(&z, 1, (src_stride - bytewidth), dst);
        }

        s+=src_stride;
    }

    return RC_OK;
}
