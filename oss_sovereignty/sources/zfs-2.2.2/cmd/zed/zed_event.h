

#ifndef	ZED_EVENT_H
#define	ZED_EVENT_H

#include <stdint.h>

int zed_event_init(struct zed_conf *zcp);

void zed_event_fini(struct zed_conf *zcp);

int zed_event_seek(struct zed_conf *zcp, uint64_t saved_eid,
    int64_t saved_etime[]);

int zed_event_service(struct zed_conf *zcp);

#endif	
