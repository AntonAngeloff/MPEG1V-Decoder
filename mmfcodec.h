#ifndef MMFCODEC_H_INCLUDED
#define MMFCODEC_H_INCLUDED

#include "mmfutil.h"
#include "mmfsample.h"
#include "mmfpacket.h"
#include <stdint.h>

typedef enum MMFMediaType {
    MEDIA_TYPE_AUDIO    = 0x01,
    MEDIA_TYPE_VIDEO    = 0x02,
    MEDIA_TYPE_SUBTITLES= 0x03,
    FORCE_DWORD             = 0xFFFFFFFF,
} MMFMediaType;

typedef enum MMFCodecId {
    CODEC_ID_UNKNOWN = 0xFFFFFFFF,
    //Video
    CODEC_ID_H264    = 0x100,
    CODEC_ID_H265,
    //Audio
    CODEC_ID_AAC     = 0x200
} MMFCodecId;

typedef struct MMFTimeBase {
    //Number and denominator
    int64_t num;
    int64_t den;
} MMFTimeBase;

typedef enum MMFCodecStateFlags {
    CODEC_STATE_FLAGS_NONE           = 0x00,
    CODEC_STATE_FLAGS_GLOBAL_HEADERS = 0x01,
    CODEC_STATE_FLAGS_INTERLACED     = 0x02,
    CODEC_STATE_FLAGS_2_PASS         = 0x04,
    CODEC_STATE_FLAGS_FORCE_DWORD    = 0xFFFFFFFF,
} MMFCodecStateFlags;

typedef enum MMFCodecOperationStatus {
    CODEC_OP_STATUS_NONE    = 0x00,
    CODEC_OP_STATUS_FRAME_READY,
    CODEC_OP_STATUS_FORCE_DWORD      = 0xFFFFFFFF,
} MMFCodecOperationStatus;

typedef struct MMFClass {
    /**
     * Name of the class
     */
    char name[32];
} MMFClass;

/**
 * CODEC State - holds the current state/context of a codec.
 * Every codec can open multiple context for handling multiple coding sessions at once.
 */
typedef struct MMFCodecState {
    /**
     * Set by mmf_codec_open()
     */
    const struct MMFClass *cls;

    /**
     * Pointer to parent codec
     */
    const struct MMFCodec *codec;

    /**
     * FOURCC
     */
    uint32_t codec_fcc;

    /**
     * Private data, set by the system
     */
    void *priv_data;

    /**
     * Private data of the user, can be used to carry app specific stuff.
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    void *opaque;

    /**
     * the average bitrate
     * - encoding: Set by user; unused for constant quantizer encoding.
     * - decoding: Set by user, may be overwritten by libavcodec
     *             if this info is available in the stream
     */
    int64_t bit_rate;

    /**
     * Time base
     */
    MMFTimeBase time_base;

    /**
     * Frame size (for video media type only)
     */
    int32_t width, height;

    /**
     * Sample format, which is currently used
     */
    MMFSampleFormat sample_fmt;

    int64_t last_dts;

    int32_t gop_size;

    uint32_t flags;

    void *extra_data;
    int32_t extra_data_size;
} MMFCodecState;

/**
 * Structure describing a CODEC, also it's capabilities
 * and methods
 */
typedef struct MMFCodec{
	/**
	 * CODEC name (e.g. nvenc_h264)
	 */
	char *name;

	/**
	 * CODEC description
	 */
	char description[256];

	/**
	 * Media type (e.g. audio, video)
	 */
	uint32_t type;

    /**
     * This value is provided by the encoder for mmf_alloc_codec_state()
     */
    int32_t private_data_size;

	/**
	 * E.g. MMF_CODEC_ID_H264
	 */
	uint32_t id;

    /**
     * Array of supported sample formats
     */
    MMFSampleFormat *sample_fmts;

	//Delegated functions

	//open codec
    MMFRES (*open)(MMFCodecState*);

	//close codec
	MMFRES(*close)(MMFCodecState*);

	//encode
	MMFRES(*encode)(MMFCodecState*, const MMFSample*, MMFPacket*, MMFCodecOperationStatus*);
	//decode
	//flush codec
	MMFRES(*flush)(MMFCodecState*);
} MMFCodec;

MMFRES mmf_packet_ensure_size(MMFCodecState *cs, MMFPacket *pkt, int size);
MMFRES mmf_codec_state_alloc(MMFCodec *pCodec, MMFCodecState **ppState);
MMFRES mmf_codec_state_free(MMFCodecState **ppState);

MMFRES mmf_codec_open(MMFCodec *codec, MMFCodecState *cs);
MMFRES mmf_codec_close(MMFCodecState *cs);
MMFRES mmf_codec_encode(MMFCodecState* cs, const MMFSample* sample, MMFPacket* pkt, MMFCodecOperationStatus* status);

/**
 * Initializes the MMF CODEC Subsystem. Most important, this function
 * registers all components in the framework.
 *
 * @return RC_OK on success, error otherwise.
 */
MMFRES mmf_codec_initialize();
MMFRES mmf_codec_finalize();
MMFRES mmf_codec_register(MMFCodec *c);
MMFRES mmf_codec_unregister(MMFCodec *c);

/**
 * Finds a CODEC by given CODEC ID.
 * @todo Add another parameter to describe if the should be encoder or decoder.
 *
 * @param id ID of the codec to be found on the system.
 * @param ppc Pointer to a variable to receive pointer to a CODEC descriptor (MMFCodec).
 * @return RC_OK on success, RC_NOTFOUND when not found. It might return other error codes on failure.
 */
MMFRES mmf_codec_find(MMFCodecId id, MMFCodec **ppc);

#endif // MMFCODEC_H_INCLUDED
