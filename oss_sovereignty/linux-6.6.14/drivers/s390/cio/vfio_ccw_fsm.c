
 

#include <linux/vfio.h>

#include <asm/isc.h>

#include "ioasm.h"
#include "vfio_ccw_private.h"

static int fsm_io_helper(struct vfio_ccw_private *private)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);
	union orb *orb;
	int ccode;
	__u8 lpm;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(sch->lock, flags);

	orb = cp_get_orb(&private->cp, sch);
	if (!orb) {
		ret = -EIO;
		goto out;
	}

	VFIO_CCW_TRACE_EVENT(5, "stIO");
	VFIO_CCW_TRACE_EVENT(5, dev_name(&sch->dev));

	 
	ccode = ssch(sch->schid, orb);

	VFIO_CCW_HEX_EVENT(5, &ccode, sizeof(ccode));

	switch (ccode) {
	case 0:
		 
		sch->schib.scsw.cmd.actl |= SCSW_ACTL_START_PEND;
		ret = 0;
		private->state = VFIO_CCW_STATE_CP_PENDING;
		break;
	case 1:		 
	case 2:		 
		ret = -EBUSY;
		break;
	case 3:		 
	{
		lpm = orb->cmd.lpm;
		if (lpm != 0)
			sch->lpm &= ~lpm;
		else
			sch->lpm = 0;

		if (cio_update_schib(sch))
			ret = -ENODEV;
		else
			ret = sch->lpm ? -EACCES : -ENODEV;
		break;
	}
	default:
		ret = ccode;
	}
out:
	spin_unlock_irqrestore(sch->lock, flags);
	return ret;
}

static int fsm_do_halt(struct vfio_ccw_private *private)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);
	unsigned long flags;
	int ccode;
	int ret;

	spin_lock_irqsave(sch->lock, flags);

	VFIO_CCW_TRACE_EVENT(2, "haltIO");
	VFIO_CCW_TRACE_EVENT(2, dev_name(&sch->dev));

	 
	ccode = hsch(sch->schid);

	VFIO_CCW_HEX_EVENT(2, &ccode, sizeof(ccode));

	switch (ccode) {
	case 0:
		 
		sch->schib.scsw.cmd.actl |= SCSW_ACTL_HALT_PEND;
		ret = 0;
		break;
	case 1:		 
	case 2:		 
		ret = -EBUSY;
		break;
	case 3:		 
		ret = -ENODEV;
		break;
	default:
		ret = ccode;
	}
	spin_unlock_irqrestore(sch->lock, flags);
	return ret;
}

static int fsm_do_clear(struct vfio_ccw_private *private)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);
	unsigned long flags;
	int ccode;
	int ret;

	spin_lock_irqsave(sch->lock, flags);

	VFIO_CCW_TRACE_EVENT(2, "clearIO");
	VFIO_CCW_TRACE_EVENT(2, dev_name(&sch->dev));

	 
	ccode = csch(sch->schid);

	VFIO_CCW_HEX_EVENT(2, &ccode, sizeof(ccode));

	switch (ccode) {
	case 0:
		 
		sch->schib.scsw.cmd.actl = SCSW_ACTL_CLEAR_PEND;
		 
		ret = 0;
		break;
	case 3:		 
		ret = -ENODEV;
		break;
	default:
		ret = ccode;
	}
	spin_unlock_irqrestore(sch->lock, flags);
	return ret;
}

static void fsm_notoper(struct vfio_ccw_private *private,
			enum vfio_ccw_event event)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);

	VFIO_CCW_MSG_EVENT(2, "sch %x.%x.%04x: notoper event %x state %x\n",
			   sch->schid.cssid,
			   sch->schid.ssid,
			   sch->schid.sch_no,
			   event,
			   private->state);

	 
	css_sched_sch_todo(sch, SCH_TODO_UNREG);
	private->state = VFIO_CCW_STATE_NOT_OPER;

	 
	cp_free(&private->cp);
}

 
static void fsm_nop(struct vfio_ccw_private *private,
		    enum vfio_ccw_event event)
{
}

static void fsm_io_error(struct vfio_ccw_private *private,
			 enum vfio_ccw_event event)
{
	pr_err("vfio-ccw: FSM: I/O request from state:%d\n", private->state);
	private->io_region->ret_code = -EIO;
}

static void fsm_io_busy(struct vfio_ccw_private *private,
			enum vfio_ccw_event event)
{
	private->io_region->ret_code = -EBUSY;
}

static void fsm_io_retry(struct vfio_ccw_private *private,
			 enum vfio_ccw_event event)
{
	private->io_region->ret_code = -EAGAIN;
}

static void fsm_async_error(struct vfio_ccw_private *private,
			    enum vfio_ccw_event event)
{
	struct ccw_cmd_region *cmd_region = private->cmd_region;

	pr_err("vfio-ccw: FSM: %s request from state:%d\n",
	       cmd_region->command == VFIO_CCW_ASYNC_CMD_HSCH ? "halt" :
	       cmd_region->command == VFIO_CCW_ASYNC_CMD_CSCH ? "clear" :
	       "<unknown>", private->state);
	cmd_region->ret_code = -EIO;
}

static void fsm_async_retry(struct vfio_ccw_private *private,
			    enum vfio_ccw_event event)
{
	private->cmd_region->ret_code = -EAGAIN;
}

static void fsm_disabled_irq(struct vfio_ccw_private *private,
			     enum vfio_ccw_event event)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);

	 
	cio_disable_subchannel(sch);
}
inline struct subchannel_id get_schid(struct vfio_ccw_private *p)
{
	struct subchannel *sch = to_subchannel(p->vdev.dev->parent);

	return sch->schid;
}

 
static void fsm_io_request(struct vfio_ccw_private *private,
			   enum vfio_ccw_event event)
{
	union orb *orb;
	union scsw *scsw = &private->scsw;
	struct ccw_io_region *io_region = private->io_region;
	char *errstr = "request";
	struct subchannel_id schid = get_schid(private);

	private->state = VFIO_CCW_STATE_CP_PROCESSING;
	memcpy(scsw, io_region->scsw_area, sizeof(*scsw));

	if (scsw->cmd.fctl & SCSW_FCTL_START_FUNC) {
		orb = (union orb *)io_region->orb_area;

		 
		if (orb->tm.b) {
			io_region->ret_code = -EOPNOTSUPP;
			VFIO_CCW_MSG_EVENT(2,
					   "sch %x.%x.%04x: transport mode\n",
					   schid.cssid,
					   schid.ssid, schid.sch_no);
			errstr = "transport mode";
			goto err_out;
		}
		io_region->ret_code = cp_init(&private->cp, orb);
		if (io_region->ret_code) {
			VFIO_CCW_MSG_EVENT(2,
					   "sch %x.%x.%04x: cp_init=%d\n",
					   schid.cssid,
					   schid.ssid, schid.sch_no,
					   io_region->ret_code);
			errstr = "cp init";
			goto err_out;
		}

		io_region->ret_code = cp_prefetch(&private->cp);
		if (io_region->ret_code) {
			VFIO_CCW_MSG_EVENT(2,
					   "sch %x.%x.%04x: cp_prefetch=%d\n",
					   schid.cssid,
					   schid.ssid, schid.sch_no,
					   io_region->ret_code);
			errstr = "cp prefetch";
			cp_free(&private->cp);
			goto err_out;
		}

		 
		io_region->ret_code = fsm_io_helper(private);
		if (io_region->ret_code) {
			VFIO_CCW_MSG_EVENT(2,
					   "sch %x.%x.%04x: fsm_io_helper=%d\n",
					   schid.cssid,
					   schid.ssid, schid.sch_no,
					   io_region->ret_code);
			errstr = "cp fsm_io_helper";
			cp_free(&private->cp);
			goto err_out;
		}
		return;
	} else if (scsw->cmd.fctl & SCSW_FCTL_HALT_FUNC) {
		VFIO_CCW_MSG_EVENT(2,
				   "sch %x.%x.%04x: halt on io_region\n",
				   schid.cssid,
				   schid.ssid, schid.sch_no);
		 
		io_region->ret_code = -EOPNOTSUPP;
		goto err_out;
	} else if (scsw->cmd.fctl & SCSW_FCTL_CLEAR_FUNC) {
		VFIO_CCW_MSG_EVENT(2,
				   "sch %x.%x.%04x: clear on io_region\n",
				   schid.cssid,
				   schid.ssid, schid.sch_no);
		 
		io_region->ret_code = -EOPNOTSUPP;
		goto err_out;
	}

err_out:
	private->state = VFIO_CCW_STATE_IDLE;
	trace_vfio_ccw_fsm_io_request(scsw->cmd.fctl, schid,
				      io_region->ret_code, errstr);
}

 
static void fsm_async_request(struct vfio_ccw_private *private,
			      enum vfio_ccw_event event)
{
	struct ccw_cmd_region *cmd_region = private->cmd_region;

	switch (cmd_region->command) {
	case VFIO_CCW_ASYNC_CMD_HSCH:
		cmd_region->ret_code = fsm_do_halt(private);
		break;
	case VFIO_CCW_ASYNC_CMD_CSCH:
		cmd_region->ret_code = fsm_do_clear(private);
		break;
	default:
		 
		cmd_region->ret_code = -EINVAL;
	}

	trace_vfio_ccw_fsm_async_request(get_schid(private),
					 cmd_region->command,
					 cmd_region->ret_code);
}

 
static void fsm_irq(struct vfio_ccw_private *private,
		    enum vfio_ccw_event event)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);
	struct irb *irb = this_cpu_ptr(&cio_irb);

	VFIO_CCW_TRACE_EVENT(6, "IRQ");
	VFIO_CCW_TRACE_EVENT(6, dev_name(&sch->dev));

	memcpy(&private->irb, irb, sizeof(*irb));

	queue_work(vfio_ccw_work_q, &private->io_work);

	if (private->completion)
		complete(private->completion);
}

static void fsm_open(struct vfio_ccw_private *private,
		     enum vfio_ccw_event event)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);
	int ret;

	spin_lock_irq(sch->lock);
	sch->isc = VFIO_CCW_ISC;
	ret = cio_enable_subchannel(sch, (u32)(unsigned long)sch);
	if (ret)
		goto err_unlock;

	private->state = VFIO_CCW_STATE_IDLE;
	spin_unlock_irq(sch->lock);
	return;

err_unlock:
	spin_unlock_irq(sch->lock);
	vfio_ccw_fsm_event(private, VFIO_CCW_EVENT_NOT_OPER);
}

static void fsm_close(struct vfio_ccw_private *private,
		      enum vfio_ccw_event event)
{
	struct subchannel *sch = to_subchannel(private->vdev.dev->parent);
	int ret;

	spin_lock_irq(sch->lock);

	if (!sch->schib.pmcw.ena)
		goto err_unlock;

	ret = cio_disable_subchannel(sch);
	if (ret == -EBUSY)
		ret = vfio_ccw_sch_quiesce(sch);
	if (ret)
		goto err_unlock;

	private->state = VFIO_CCW_STATE_STANDBY;
	spin_unlock_irq(sch->lock);
	cp_free(&private->cp);
	return;

err_unlock:
	spin_unlock_irq(sch->lock);
	vfio_ccw_fsm_event(private, VFIO_CCW_EVENT_NOT_OPER);
}

 
fsm_func_t *vfio_ccw_jumptable[NR_VFIO_CCW_STATES][NR_VFIO_CCW_EVENTS] = {
	[VFIO_CCW_STATE_NOT_OPER] = {
		[VFIO_CCW_EVENT_NOT_OPER]	= fsm_nop,
		[VFIO_CCW_EVENT_IO_REQ]		= fsm_io_error,
		[VFIO_CCW_EVENT_ASYNC_REQ]	= fsm_async_error,
		[VFIO_CCW_EVENT_INTERRUPT]	= fsm_disabled_irq,
		[VFIO_CCW_EVENT_OPEN]		= fsm_nop,
		[VFIO_CCW_EVENT_CLOSE]		= fsm_nop,
	},
	[VFIO_CCW_STATE_STANDBY] = {
		[VFIO_CCW_EVENT_NOT_OPER]	= fsm_notoper,
		[VFIO_CCW_EVENT_IO_REQ]		= fsm_io_error,
		[VFIO_CCW_EVENT_ASYNC_REQ]	= fsm_async_error,
		[VFIO_CCW_EVENT_INTERRUPT]	= fsm_disabled_irq,
		[VFIO_CCW_EVENT_OPEN]		= fsm_open,
		[VFIO_CCW_EVENT_CLOSE]		= fsm_notoper,
	},
	[VFIO_CCW_STATE_IDLE] = {
		[VFIO_CCW_EVENT_NOT_OPER]	= fsm_notoper,
		[VFIO_CCW_EVENT_IO_REQ]		= fsm_io_request,
		[VFIO_CCW_EVENT_ASYNC_REQ]	= fsm_async_request,
		[VFIO_CCW_EVENT_INTERRUPT]	= fsm_irq,
		[VFIO_CCW_EVENT_OPEN]		= fsm_notoper,
		[VFIO_CCW_EVENT_CLOSE]		= fsm_close,
	},
	[VFIO_CCW_STATE_CP_PROCESSING] = {
		[VFIO_CCW_EVENT_NOT_OPER]	= fsm_notoper,
		[VFIO_CCW_EVENT_IO_REQ]		= fsm_io_retry,
		[VFIO_CCW_EVENT_ASYNC_REQ]	= fsm_async_retry,
		[VFIO_CCW_EVENT_INTERRUPT]	= fsm_irq,
		[VFIO_CCW_EVENT_OPEN]		= fsm_notoper,
		[VFIO_CCW_EVENT_CLOSE]		= fsm_close,
	},
	[VFIO_CCW_STATE_CP_PENDING] = {
		[VFIO_CCW_EVENT_NOT_OPER]	= fsm_notoper,
		[VFIO_CCW_EVENT_IO_REQ]		= fsm_io_busy,
		[VFIO_CCW_EVENT_ASYNC_REQ]	= fsm_async_request,
		[VFIO_CCW_EVENT_INTERRUPT]	= fsm_irq,
		[VFIO_CCW_EVENT_OPEN]		= fsm_notoper,
		[VFIO_CCW_EVENT_CLOSE]		= fsm_close,
	},
};
