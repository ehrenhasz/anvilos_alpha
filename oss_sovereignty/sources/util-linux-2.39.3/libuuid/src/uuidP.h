
#ifndef _UUID_UUID_PRIVATE_H
#define _UUID_UUID_PRIVATE_H

#include <inttypes.h>
#include <sys/types.h>

#include "uuid.h"

#define LIBUUID_CLOCK_FILE	"/var/lib/libuuid/clock.txt"


#define TIME_OFFSET_HIGH 0x01B21DD2
#define TIME_OFFSET_LOW  0x13814000


struct uuid {
	uint32_t	time_low;
	uint16_t	time_mid;
	uint16_t	time_hi_and_version;
	uint16_t	clock_seq;
	uint8_t	node[6];
};



void uuid_pack(const struct uuid *uu, uuid_t ptr);
void uuid_unpack(const uuid_t in, struct uuid *uu);

#endif 
