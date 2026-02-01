
 

#include "iosm_ipc_protocol.h"

 
#define IPC_PM_ACTIVE_TIMEOUT_MS (500)

 
#define IPC_PM_SLEEP (0)
#define CONSUME_STATE (0)
#define IPC_PM_ACTIVE (1)

void ipc_pm_signal_hpda_doorbell(struct iosm_pm *ipc_pm, u32 identifier,
				 bool host_slp_check)
{
	if (host_slp_check && ipc_pm->host_pm_state != IPC_MEM_HOST_PM_ACTIVE &&
	    ipc_pm->host_pm_state != IPC_MEM_HOST_PM_ACTIVE_WAIT) {
		ipc_pm->pending_hpda_update = true;
		dev_dbg(ipc_pm->dev,
			"Pend HPDA update set. Host PM_State: %d identifier:%d",
			ipc_pm->host_pm_state, identifier);
		return;
	}

	if (!ipc_pm_trigger(ipc_pm, IPC_PM_UNIT_IRQ, true)) {
		ipc_pm->pending_hpda_update = true;
		dev_dbg(ipc_pm->dev, "Pending HPDA update set. identifier:%d",
			identifier);
		return;
	}
	ipc_pm->pending_hpda_update = false;

	 
	ipc_cp_irq_hpda_update(ipc_pm->pcie, identifier);

	ipc_pm_trigger(ipc_pm, IPC_PM_UNIT_IRQ, false);
}

 
static bool ipc_pm_link_activate(struct iosm_pm *ipc_pm)
{
	if (ipc_pm->cp_state == IPC_MEM_DEV_PM_ACTIVE)
		return true;

	if (ipc_pm->cp_state == IPC_MEM_DEV_PM_SLEEP) {
		if (ipc_pm->ap_state == IPC_MEM_DEV_PM_SLEEP) {
			 
			ipc_cp_irq_sleep_control(ipc_pm->pcie,
						 IPC_MEM_DEV_PM_WAKEUP);
			ipc_pm->ap_state = IPC_MEM_DEV_PM_ACTIVE_WAIT;

			goto not_active;
		}

		if (ipc_pm->ap_state == IPC_MEM_DEV_PM_ACTIVE_WAIT)
			goto not_active;

		return true;
	}

not_active:
	 
	return false;
}

bool ipc_pm_wait_for_device_active(struct iosm_pm *ipc_pm)
{
	bool ret_val = false;

	if (ipc_pm->ap_state != IPC_MEM_DEV_PM_ACTIVE) {
		 
		smp_mb__before_atomic();

		 
		set_bit(0, &ipc_pm->host_sleep_pend);

		 
		smp_mb__after_atomic();

		if (!wait_for_completion_interruptible_timeout
		   (&ipc_pm->host_sleep_complete,
		    msecs_to_jiffies(IPC_PM_ACTIVE_TIMEOUT_MS))) {
			dev_err(ipc_pm->dev,
				"PM timeout. Expected State:%d. Actual: %d",
				IPC_MEM_DEV_PM_ACTIVE, ipc_pm->ap_state);
			goto  active_timeout;
		}
	}

	ret_val = true;
active_timeout:
	 
	smp_mb__before_atomic();

	 
	clear_bit(0, &ipc_pm->host_sleep_pend);

	 
	smp_mb__after_atomic();

	return ret_val;
}

static void ipc_pm_on_link_sleep(struct iosm_pm *ipc_pm)
{
	 
	ipc_pm->cp_state = IPC_MEM_DEV_PM_SLEEP;
	ipc_pm->ap_state = IPC_MEM_DEV_PM_SLEEP;

	ipc_cp_irq_sleep_control(ipc_pm->pcie, IPC_MEM_DEV_PM_SLEEP);
}

static void ipc_pm_on_link_wake(struct iosm_pm *ipc_pm, bool ack)
{
	ipc_pm->ap_state = IPC_MEM_DEV_PM_ACTIVE;

	if (ack) {
		ipc_pm->cp_state = IPC_MEM_DEV_PM_ACTIVE;

		ipc_cp_irq_sleep_control(ipc_pm->pcie, IPC_MEM_DEV_PM_ACTIVE);

		 
		if (test_bit(CONSUME_STATE, &ipc_pm->host_sleep_pend))
			complete(&ipc_pm->host_sleep_complete);
	}

	 
	if (ipc_pm->pending_hpda_update &&
	    ipc_pm->host_pm_state == IPC_MEM_HOST_PM_ACTIVE)
		ipc_pm_signal_hpda_doorbell(ipc_pm, IPC_HP_PM_TRIGGER, true);
}

bool ipc_pm_trigger(struct iosm_pm *ipc_pm, enum ipc_pm_unit unit, bool active)
{
	union ipc_pm_cond old_cond;
	union ipc_pm_cond new_cond;
	bool link_active;

	 
	new_cond = ipc_pm->pm_cond;
	old_cond = ipc_pm->pm_cond;

	 
	switch (unit) {
	case IPC_PM_UNIT_IRQ:  
		new_cond.irq = active;
		break;

	case IPC_PM_UNIT_LINK:  
		new_cond.link = active;
		break;

	case IPC_PM_UNIT_HS:  
		new_cond.hs = active;
		break;

	default:
		break;
	}

	 
	if (old_cond.raw == new_cond.raw) {
		 
		link_active = old_cond.link == IPC_PM_ACTIVE;
		goto ret;
	}

	ipc_pm->pm_cond = new_cond;

	if (new_cond.link)
		ipc_pm_on_link_wake(ipc_pm, unit == IPC_PM_UNIT_LINK);
	else if (unit == IPC_PM_UNIT_LINK)
		ipc_pm_on_link_sleep(ipc_pm);

	if (old_cond.link == IPC_PM_SLEEP && new_cond.raw) {
		link_active = ipc_pm_link_activate(ipc_pm);
		goto ret;
	}

	link_active = old_cond.link == IPC_PM_ACTIVE;

ret:
	return link_active;
}

bool ipc_pm_prepare_host_sleep(struct iosm_pm *ipc_pm)
{
	 
	if (ipc_pm->host_pm_state != IPC_MEM_HOST_PM_ACTIVE) {
		dev_err(ipc_pm->dev, "host_pm_state=%d\tExpected to be: %d",
			ipc_pm->host_pm_state, IPC_MEM_HOST_PM_ACTIVE);
		return false;
	}

	ipc_pm->host_pm_state = IPC_MEM_HOST_PM_SLEEP_WAIT_D3;

	return true;
}

bool ipc_pm_prepare_host_active(struct iosm_pm *ipc_pm)
{
	if (ipc_pm->host_pm_state != IPC_MEM_HOST_PM_SLEEP) {
		dev_err(ipc_pm->dev, "host_pm_state=%d\tExpected to be: %d",
			ipc_pm->host_pm_state, IPC_MEM_HOST_PM_SLEEP);
		return false;
	}

	 
	ipc_pm->host_pm_state = IPC_MEM_HOST_PM_ACTIVE_WAIT;

	return true;
}

void ipc_pm_set_s2idle_sleep(struct iosm_pm *ipc_pm, bool sleep)
{
	if (sleep) {
		ipc_pm->ap_state = IPC_MEM_DEV_PM_SLEEP;
		ipc_pm->cp_state = IPC_MEM_DEV_PM_SLEEP;
		ipc_pm->device_sleep_notification = IPC_MEM_DEV_PM_SLEEP;
	} else {
		ipc_pm->ap_state = IPC_MEM_DEV_PM_ACTIVE;
		ipc_pm->cp_state = IPC_MEM_DEV_PM_ACTIVE;
		ipc_pm->device_sleep_notification = IPC_MEM_DEV_PM_ACTIVE;
		ipc_pm->pm_cond.link = IPC_PM_ACTIVE;
	}
}

bool ipc_pm_dev_slp_notification(struct iosm_pm *ipc_pm, u32 cp_pm_req)
{
	if (cp_pm_req == ipc_pm->device_sleep_notification)
		return false;

	ipc_pm->device_sleep_notification = cp_pm_req;

	 
	switch (ipc_pm->cp_state) {
	case IPC_MEM_DEV_PM_ACTIVE:
		switch (cp_pm_req) {
		case IPC_MEM_DEV_PM_ACTIVE:
			break;

		case IPC_MEM_DEV_PM_SLEEP:
			 
			ipc_pm_trigger(ipc_pm, IPC_PM_UNIT_LINK, false);
			return true;

		default:
			dev_err(ipc_pm->dev,
				"loc-pm=%d active: confused req-pm=%d",
				ipc_pm->cp_state, cp_pm_req);
			break;
		}
		break;

	case IPC_MEM_DEV_PM_SLEEP:
		switch (cp_pm_req) {
		case IPC_MEM_DEV_PM_ACTIVE:
			 
			ipc_pm_trigger(ipc_pm, IPC_PM_UNIT_LINK, true);
			break;

		case IPC_MEM_DEV_PM_SLEEP:
			break;

		default:
			dev_err(ipc_pm->dev,
				"loc-pm=%d sleep: confused req-pm=%d",
				ipc_pm->cp_state, cp_pm_req);
			break;
		}
		break;

	default:
		dev_err(ipc_pm->dev, "confused loc-pm=%d, req-pm=%d",
			ipc_pm->cp_state, cp_pm_req);
		break;
	}

	return false;
}

void ipc_pm_init(struct iosm_protocol *ipc_protocol)
{
	struct iosm_imem *ipc_imem = ipc_protocol->imem;
	struct iosm_pm *ipc_pm = &ipc_protocol->pm;

	ipc_pm->pcie = ipc_imem->pcie;
	ipc_pm->dev = ipc_imem->dev;

	ipc_pm->pm_cond.irq = IPC_PM_SLEEP;
	ipc_pm->pm_cond.hs = IPC_PM_SLEEP;
	ipc_pm->pm_cond.link = IPC_PM_ACTIVE;

	ipc_pm->cp_state = IPC_MEM_DEV_PM_ACTIVE;
	ipc_pm->ap_state = IPC_MEM_DEV_PM_ACTIVE;
	ipc_pm->host_pm_state = IPC_MEM_HOST_PM_ACTIVE;

	 
	init_completion(&ipc_pm->host_sleep_complete);

	 
	smp_mb__before_atomic();

	clear_bit(0, &ipc_pm->host_sleep_pend);

	 
	smp_mb__after_atomic();
}

void ipc_pm_deinit(struct iosm_protocol *proto)
{
	struct iosm_pm *ipc_pm = &proto->pm;

	complete(&ipc_pm->host_sleep_complete);
}
