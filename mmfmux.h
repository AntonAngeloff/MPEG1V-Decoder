#ifndef MMFMUX_H_INCLUDED
#define MMFMUX_H_INCLUDED

#include "mmfutil.h"

/**
 * Elementary stream
 */
typedef struct MMFElementaryStream {
    int stream_id;
    MMFMediaType media_type;

    /*
     * Stream timebase
     */
    MMFTimeBase time_base;

    /*
     * Codec by which the elementary stream is
     * encodec/decodec
     */
    MMFCodec *codec;

    /*
     * Holder for private data (set by [de]muxer)
     */
    void *priv_data;
    int priv_data_size;
} MMFElementaryStream;

/**
 * (De)Muxer context
 */
typedef struct MMFMuxState {
    /*
     * Set by mmf_codec_open()
     */
    const struct MMFClass *cls;

    /*
     * Pointer to parent (de)muxer
     */
    const struct MMFMux *mux;

    /*
     * Holder for private data (set by [de]muxer)
     */
    void *priv_data;
    int priv_data_size;

    /*
     * Holder for user-defined data
     */
    void *pub_data;
    int pub_data_size;

    /*
     * Elementary streams
     */
    int stream_count;
    MMFElementaryStream **streams;

    /*
     * Container time-base
     */
    MMFTimeBase time_base;
} MMFMuxContext;

/**
 * Represents a muxer/demuxer
 */
typedef struct MMFMux {
    char name[32];
    char description[256];
    char mime_type[32];

    int8_t flags;

    //Todo:
    MMFRES(*open)(MMFMuxContext*);
	MMFRES(*close)(MMFMuxContext*);
	MMFRES(*write)(MMFMuxContext*, const MMFPacket*);
	MMFRES(*flush)(MMFMuxContext*);
} MMFMux;

#endif // MMFMUX_H_INCLUDED
