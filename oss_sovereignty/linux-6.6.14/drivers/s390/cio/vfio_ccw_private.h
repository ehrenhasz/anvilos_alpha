 
 

#ifndef _VFIO_CCW_PRIVATE_H_
#define _VFIO_CCW_PRIVATE_H_

#include <linux/completion.h>
#include <linux/eventfd.h>
#include <linux/workqueue.h>
#include <linux/vfio_ccw.h>
#include <linux/vfio.h>
#include <linux/mdev.h>
#include <asm/crw.h>
#include <asm/debug.h>

#include "css.h"
#include "vfio_ccw_cp.h"

#define VFIO_CCW_OFFSET_SHIFT   10
#define VFIO_CCW_OFFSET_TO_INDEX(off)	(off >> VFIO_CCW_OFFSET_SHIFT)
#define VFIO_CCW_INDEX_TO_OFFSET(index)	((u64)(index) << VFIO_CCW_OFFSET_SHIFT)
#define VFIO_CCW_OFFSET_MASK	(((u64)(1) << VFIO_CCW_OFFSET_SHIFT) - 1)

 
struct vfio_ccw_private;
struct vfio_ccw_region;

struct vfio_ccw_regops {
	ssize_t	(*read)(struct vfio_ccw_private *private, char __user *buf,
			size_t count, loff_t *ppos);
	ssize_t	(*write)(struct vfio_ccw_private *private,
			 const char __user *buf, size_t count, loff_t *ppos);
	void	(*release)(struct vfio_ccw_private *private,
			   struct vfio_ccw_region *region);
};

struct vfio_ccw_region {
	u32				type;
	u32				subtype;
	const struct vfio_ccw_regops	*ops;
	void				*data;
	size_t				size;
	u32				flags;
};

int vfio_ccw_register_dev_region(struct vfio_ccw_private *private,
				 unsigned int subtype,
				 const struct vfio_ccw_regops *ops,
				 size_t size, u32 flags, void *data);
void vfio_ccw_unregister_dev_regions(struct vfio_ccw_private *private);

int vfio_ccw_register_async_dev_regions(struct vfio_ccw_private *private);
int vfio_ccw_register_schib_dev_regions(struct vfio_ccw_private *private);
int vfio_ccw_register_crw_dev_regions(struct vfio_ccw_private *private);

struct vfio_ccw_crw {
	struct list_head	next;
	struct crw		crw;
};

 
struct vfio_ccw_parent {
	struct device		dev;

	struct mdev_parent	parent;
	struct mdev_type	mdev_type;
	struct mdev_type	*mdev_types[];
};

 
struct vfio_ccw_private {
	struct vfio_device vdev;
	int			state;
	struct completion	*completion;
	struct ccw_io_region	*io_region;
	struct mutex		io_mutex;
	struct vfio_ccw_region *region;
	struct ccw_cmd_region	*cmd_region;
	struct ccw_schib_region *schib_region;
	struct ccw_crw_region	*crw_region;
	int num_regions;

	struct channel_program	cp;
	struct irb		irb;
	union scsw		scsw;
	struct list_head	crw;

	struct eventfd_ctx	*io_trigger;
	struct eventfd_ctx	*crw_trigger;
	struct eventfd_ctx	*req_trigger;
	struct work_struct	io_work;
	struct work_struct	crw_work;
} __aligned(8);

int vfio_ccw_sch_quiesce(struct subchannel *sch);
void vfio_ccw_sch_io_todo(struct work_struct *work);
void vfio_ccw_crw_todo(struct work_struct *work);

extern struct mdev_driver vfio_ccw_mdev_driver;

 
enum vfio_ccw_state {
	VFIO_CCW_STATE_NOT_OPER,
	VFIO_CCW_STATE_STANDBY,
	VFIO_CCW_STATE_IDLE,
	VFIO_CCW_STATE_CP_PROCESSING,
	VFIO_CCW_STATE_CP_PENDING,
	 
	NR_VFIO_CCW_STATES
};

 
enum vfio_ccw_event {
	VFIO_CCW_EVENT_NOT_OPER,
	VFIO_CCW_EVENT_IO_REQ,
	VFIO_CCW_EVENT_INTERRUPT,
	VFIO_CCW_EVENT_ASYNC_REQ,
	VFIO_CCW_EVENT_OPEN,
	VFIO_CCW_EVENT_CLOSE,
	 
	NR_VFIO_CCW_EVENTS
};

 
typedef void (fsm_func_t)(struct vfio_ccw_private *, enum vfio_ccw_event);
extern fsm_func_t *vfio_ccw_jumptable[NR_VFIO_CCW_STATES][NR_VFIO_CCW_EVENTS];

static inline void vfio_ccw_fsm_event(struct vfio_ccw_private *private,
				      enum vfio_ccw_event event)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);

	if (sch)
		trace_vfio_ccw_fsm_event(sch->schid, private->state, event);
	vfio_ccw_jumptable[private->state][event](private, event);
}

extern struct workqueue_struct *vfio_ccw_work_q;
extern struct kmem_cache *vfio_ccw_io_region;
extern struct kmem_cache *vfio_ccw_cmd_region;
extern struct kmem_cache *vfio_ccw_schib_region;
extern struct kmem_cache *vfio_ccw_crw_region;

 
extern debug_info_t *vfio_ccw_debug_msg_id;
extern debug_info_t *vfio_ccw_debug_trace_id;

#define VFIO_CCW_TRACE_EVENT(imp, txt) \
		debug_text_event(vfio_ccw_debug_trace_id, imp, txt)

#define VFIO_CCW_MSG_EVENT(imp, args...) \
		debug_sprintf_event(vfio_ccw_debug_msg_id, imp, ##args)

static inline void VFIO_CCW_HEX_EVENT(int level, void *data, int length)
{
	debug_event(vfio_ccw_debug_trace_id, level, data, length);
}

#endif
