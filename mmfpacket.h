#ifndef MMFPACKET_H_INCLUDED
#define MMFPACKET_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include "mmfutil.h"

typedef enum MMFPacketFormat {
	PACKET_FORMAT_MTS   = 0x00
} MMFPacketFormat;

typedef enum MMFPacketFlags {
    PACKET_FLAG_NONE    = 0x00,
    PACKET_FLAG_KEY     = 0x01,
} MMFPacketFlags;

typedef struct MMFPacket {
	int64_t size, capacity;
	void *data;

	int64_t pts;
	int64_t dts;
	int64_t duration;
	int32_t flags;

	int32_t stream_id;
} MMFPacket;

MMFRES mmf_packet_alloc(MMFPacket **pkt);
MMFRES mmf_packet_free(MMFPacket **pkt);

#endif // MMFPACKET_H_INCLUDED
