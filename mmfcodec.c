#include "mmfcodec.h"

//static MMFCodec mmf_codec_sentry = {
    //.name = "",
    //.description = "",
    //.id = CODEC_ID_UNKNOWN,
//};

static MMFCodec **mmf_codec_list;
static int mmf_codec_list_count = 0;

MMFRES mmf_packet_ensure_size(MMFCodecState *cs, MMFPacket *pkt, int size)
{
    /* Handle if pkt=null */
	if(pkt==NULL) {
		return RC_INVALIDPOINTER;
	}

    if (pkt->capacity < size) {
        //Realloc packet data
        pkt->data = mmf_realloc(pkt->data, size);

        if(!pkt->data) {
            return RC_OUTOFMEM;
        }

        //Set new size
        pkt->capacity = size;
    }

    return RC_OK;
}

MMFRES mmf_codec_state_alloc(MMFCodec *pCodec, MMFCodecState **ppState)
{
    if (!pCodec) {
        return RC_INVALIDARG;
    }

    if( !(*ppState) ) {
        //Allocate memory
        (*ppState) = mmf_allocz( sizeof(MMFCodecState) );

        if (!(*ppState)) {
            return RC_OUTOFMEM;
        }
    }

    //todo: init
    MMFCodecState *cs = *ppState;
    cs->priv_data = mmf_allocz( pCodec->private_data_size );

    return RC_OK;
}

MMFRES mmf_codec_state_free(MMFCodecState **ppState)
{
    mmf_free( (*ppState)->priv_data );
    mmf_free(*ppState);
    (*ppState) = NULL;

    return RC_OK;
}

MMFRES mmf_codec_register(MMFCodec *c) {
    //todo: check if c already exist in list

    if (mmf_codec_list) {
        mmf_codec_list = mmf_realloc( mmf_codec_list, (mmf_codec_list_count++) * sizeof(int32_t) );
    } else {
        mmf_codec_list = mmf_alloc( (mmf_codec_list_count++) * sizeof(int32_t) );
    }

    if(!mmf_codec_list) {
        return RC_OUTOFMEM;
    }

    /*
     * Add codec to list
     */
    mmf_codec_list[mmf_codec_list_count-1] = c;

    return RC_OK;
}

MMFRES mmf_codec_unregister(MMFCodec *c) {
    int i, cid=-1;

    for(i=0;i<mmf_codec_list_count;i++) {
        if(c == mmf_codec_list[i]) {
            cid = i;
            break;
        }
    }

    if(cid==-1) {
        //Not found
        return RC_INVALIDARG;
    }

    for(i=cid;i<mmf_codec_list_count-1;i++) {
        mmf_codec_list[i] = mmf_codec_list[i+1];
    }

    mmf_codec_list_count--;
    return RC_OK;
}

MMFRES mmf_codec_find(MMFCodecId id, MMFCodec **ppc) {
    int i;

    for(i=0; i<mmf_codec_list_count; i++) {
        if (mmf_codec_list[i]->id == id) {
            //Found
            (*ppc) = mmf_codec_list[i];

            return RC_OK;
        }
    }

    return RC_INVALIDARG;
}

#define ENABLE_ENCODER_NVENC
#define REGISTER_ENCODER(X, x)                                          \
    {                                                                   \
        extern MMFCodec mmf_##x##_encoder;                              \
        mmf_codec_register(&mmf_##x##_encoder);                         \
    }

MMFRES mmf_codec_initialize()
{
    REGISTER_ENCODER(NVENC, nvenc);

    return RC_OK;
}

MMFRES mmf_codec_finalize()
{
    mmf_free(mmf_codec_list);
    return RC_OK;
}

MMFRES mmf_codec_open(MMFCodec *codec, MMFCodecState *cs)
{
    if(!codec || cs->codec) {
        //No codec given, or codec is already opened
        return RC_INVALIDARG;
    }

    if(!codec->open) {
        return RC_FAIL;
    }

    //Attack codec to codec state
    cs->codec = codec;

    return codec->open(cs);
}

MMFRES mmf_codec_close(MMFCodecState *cs)
{
    if (!cs || !cs->codec) {
        return RC_INVALIDARG;
    }

    MMFRES res = cs->codec->close(cs);

    //Detach codec from codec state
    cs->codec = NULL;

    return res;
}

MMFRES mmf_codec_encode(MMFCodecState* cs, const MMFSample* sample, MMFPacket* pkt, MMFCodecOperationStatus* status)
{
    if(!cs || !cs->codec) {
        return RC_INVALIDARG;
    }

    return cs->codec->encode(cs, sample, pkt, status);
}
