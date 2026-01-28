#ifndef _EVENT_FIFO_LOCAL_H
#define _EVENT_FIFO_LOCAL_H
#include "event_fifo_global.h"
typedef enum {
	SP0_EVENT_ID,
	ISP0_EVENT_ID,
	STR2MIPI_EVENT_ID,
	N_EVENT_ID
} event_ID_t;
#define	EVENT_QUERY_BIT		0
static const hrt_address event_source_addr[N_EVENT_ID] = {
	0x0000000000380000ULL,
	0x0000000000380004ULL,
	0xffffffffffffffffULL
};
static const hrt_address event_source_query_addr[N_EVENT_ID] = {
	0x0000000000380010ULL,
	0x0000000000380014ULL,
	0xffffffffffffffffULL
};
static const hrt_address event_sink_addr[N_EVENT_ID] = {
	0x0000000000380008ULL,
	0x000000000038000CULL,
	0x0000000000090104ULL
};
static const hrt_address event_sink_query_addr[N_EVENT_ID] = {
	0x0000000000380018ULL,
	0x000000000038001CULL,
	0x000000000009010CULL
};
#endif  
