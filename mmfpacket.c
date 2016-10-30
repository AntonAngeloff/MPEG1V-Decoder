#include "mmfpacket.h"

MMFRES mmf_packet_alloc(MMFPacket **pkt)
{
    #ifdef DEBUG
    if((*pkt)) {
        mmf_log(NULL, LOG_LEVEL_WARNING, "mmf_packet_alloc(): overwriting non-null pointer.");
    }
    #endif

    *pkt = mmf_allocz(sizeof(MMFPacket));
    return RC_OK;
}

MMFRES mmf_packet_free(MMFPacket **pkt)
{
    #ifdef DEBUG
    if(!(*pkt)) {
        mmf_log(NULL, LOG_LEVEL_WARNING, "mmf_packet_free(): *pkt = null.");
    }
    #endif

    mmf_free(*pkt);
    *pkt = NULL;

    return RC_OK;
}
