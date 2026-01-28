#ifndef _VFIO_CCW_CP_H_
#define _VFIO_CCW_CP_H_
#include <asm/cio.h>
#include <asm/scsw.h>
#include "orb.h"
#include "vfio_ccw_trace.h"
#define CCWCHAIN_LEN_MAX	256
struct channel_program {
	struct list_head ccwchain_list;
	union orb orb;
	bool initialized;
	struct ccw1 *guest_cp;
};
int cp_init(struct channel_program *cp, union orb *orb);
void cp_free(struct channel_program *cp);
int cp_prefetch(struct channel_program *cp);
union orb *cp_get_orb(struct channel_program *cp, struct subchannel *sch);
void cp_update_scsw(struct channel_program *cp, union scsw *scsw);
bool cp_iova_pinned(struct channel_program *cp, u64 iova, u64 length);
#endif
