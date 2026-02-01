
 

#define dev_fmt(fmt) "pciehp: " fmt

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pm_runtime.h>
#include <linux/pci.h>
#include "pciehp.h"

 

#define SAFE_REMOVAL	 true
#define SURPRISE_REMOVAL false

static void set_slot_off(struct controller *ctrl)
{
	 
	if (POWER_CTRL(ctrl)) {
		pciehp_power_off_slot(ctrl);

		 
		msleep(1000);
	}

	pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_OFF,
			      PCI_EXP_SLTCTL_ATTN_IND_ON);
}

 
static int board_added(struct controller *ctrl)
{
	int retval = 0;
	struct pci_bus *parent = ctrl->pcie->port->subordinate;

	if (POWER_CTRL(ctrl)) {
		 
		retval = pciehp_power_on_slot(ctrl);
		if (retval)
			return retval;
	}

	pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_BLINK,
			      INDICATOR_NOOP);

	 
	retval = pciehp_check_link_status(ctrl);
	if (retval)
		goto err_exit;

	 
	if (ctrl->power_fault_detected || pciehp_query_power_fault(ctrl)) {
		ctrl_err(ctrl, "Slot(%s): Power fault\n", slot_name(ctrl));
		retval = -EIO;
		goto err_exit;
	}

	retval = pciehp_configure_device(ctrl);
	if (retval) {
		if (retval != -EEXIST) {
			ctrl_err(ctrl, "Cannot add device at %04x:%02x:00\n",
				 pci_domain_nr(parent), parent->number);
			goto err_exit;
		}
	}

	pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_ON,
			      PCI_EXP_SLTCTL_ATTN_IND_OFF);
	return 0;

err_exit:
	set_slot_off(ctrl);
	return retval;
}

 
static void remove_board(struct controller *ctrl, bool safe_removal)
{
	pciehp_unconfigure_device(ctrl, safe_removal);

	if (POWER_CTRL(ctrl)) {
		pciehp_power_off_slot(ctrl);

		 
		msleep(1000);

		 
		atomic_and(~(PCI_EXP_SLTSTA_DLLSC | PCI_EXP_SLTSTA_PDC),
			   &ctrl->pending_events);
	}

	pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_OFF,
			      INDICATOR_NOOP);
}

static int pciehp_enable_slot(struct controller *ctrl);
static int pciehp_disable_slot(struct controller *ctrl, bool safe_removal);

void pciehp_request(struct controller *ctrl, int action)
{
	atomic_or(action, &ctrl->pending_events);
	if (!pciehp_poll_mode)
		irq_wake_thread(ctrl->pcie->irq, ctrl);
}

void pciehp_queue_pushbutton_work(struct work_struct *work)
{
	struct controller *ctrl = container_of(work, struct controller,
					       button_work.work);

	mutex_lock(&ctrl->state_lock);
	switch (ctrl->state) {
	case BLINKINGOFF_STATE:
		pciehp_request(ctrl, DISABLE_SLOT);
		break;
	case BLINKINGON_STATE:
		pciehp_request(ctrl, PCI_EXP_SLTSTA_PDC);
		break;
	default:
		break;
	}
	mutex_unlock(&ctrl->state_lock);
}

void pciehp_handle_button_press(struct controller *ctrl)
{
	mutex_lock(&ctrl->state_lock);
	switch (ctrl->state) {
	case OFF_STATE:
	case ON_STATE:
		if (ctrl->state == ON_STATE) {
			ctrl->state = BLINKINGOFF_STATE;
			ctrl_info(ctrl, "Slot(%s): Button press: will power off in 5 sec\n",
				  slot_name(ctrl));
		} else {
			ctrl->state = BLINKINGON_STATE;
			ctrl_info(ctrl, "Slot(%s): Button press: will power on in 5 sec\n",
				  slot_name(ctrl));
		}
		 
		pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_BLINK,
				      PCI_EXP_SLTCTL_ATTN_IND_OFF);
		schedule_delayed_work(&ctrl->button_work, 5 * HZ);
		break;
	case BLINKINGOFF_STATE:
	case BLINKINGON_STATE:
		 
		cancel_delayed_work(&ctrl->button_work);
		if (ctrl->state == BLINKINGOFF_STATE) {
			ctrl->state = ON_STATE;
			pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_ON,
					      PCI_EXP_SLTCTL_ATTN_IND_OFF);
			ctrl_info(ctrl, "Slot(%s): Button press: canceling request to power off\n",
				  slot_name(ctrl));
		} else {
			ctrl->state = OFF_STATE;
			pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_OFF,
					      PCI_EXP_SLTCTL_ATTN_IND_OFF);
			ctrl_info(ctrl, "Slot(%s): Button press: canceling request to power on\n",
				  slot_name(ctrl));
		}
		break;
	default:
		ctrl_err(ctrl, "Slot(%s): Button press: ignoring invalid state %#x\n",
			 slot_name(ctrl), ctrl->state);
		break;
	}
	mutex_unlock(&ctrl->state_lock);
}

void pciehp_handle_disable_request(struct controller *ctrl)
{
	mutex_lock(&ctrl->state_lock);
	switch (ctrl->state) {
	case BLINKINGON_STATE:
	case BLINKINGOFF_STATE:
		cancel_delayed_work(&ctrl->button_work);
		break;
	}
	ctrl->state = POWEROFF_STATE;
	mutex_unlock(&ctrl->state_lock);

	ctrl->request_result = pciehp_disable_slot(ctrl, SAFE_REMOVAL);
}

void pciehp_handle_presence_or_link_change(struct controller *ctrl, u32 events)
{
	int present, link_active;

	 
	mutex_lock(&ctrl->state_lock);
	switch (ctrl->state) {
	case BLINKINGOFF_STATE:
		cancel_delayed_work(&ctrl->button_work);
		fallthrough;
	case ON_STATE:
		ctrl->state = POWEROFF_STATE;
		mutex_unlock(&ctrl->state_lock);
		if (events & PCI_EXP_SLTSTA_DLLSC)
			ctrl_info(ctrl, "Slot(%s): Link Down\n",
				  slot_name(ctrl));
		if (events & PCI_EXP_SLTSTA_PDC)
			ctrl_info(ctrl, "Slot(%s): Card not present\n",
				  slot_name(ctrl));
		pciehp_disable_slot(ctrl, SURPRISE_REMOVAL);
		break;
	default:
		mutex_unlock(&ctrl->state_lock);
		break;
	}

	 
	mutex_lock(&ctrl->state_lock);
	present = pciehp_card_present(ctrl);
	link_active = pciehp_check_link_active(ctrl);
	if (present <= 0 && link_active <= 0) {
		if (ctrl->state == BLINKINGON_STATE) {
			ctrl->state = OFF_STATE;
			cancel_delayed_work(&ctrl->button_work);
			pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_OFF,
					      INDICATOR_NOOP);
			ctrl_info(ctrl, "Slot(%s): Card not present\n",
				  slot_name(ctrl));
		}
		mutex_unlock(&ctrl->state_lock);
		return;
	}

	switch (ctrl->state) {
	case BLINKINGON_STATE:
		cancel_delayed_work(&ctrl->button_work);
		fallthrough;
	case OFF_STATE:
		ctrl->state = POWERON_STATE;
		mutex_unlock(&ctrl->state_lock);
		if (present)
			ctrl_info(ctrl, "Slot(%s): Card present\n",
				  slot_name(ctrl));
		if (link_active)
			ctrl_info(ctrl, "Slot(%s): Link Up\n",
				  slot_name(ctrl));
		ctrl->request_result = pciehp_enable_slot(ctrl);
		break;
	default:
		mutex_unlock(&ctrl->state_lock);
		break;
	}
}

static int __pciehp_enable_slot(struct controller *ctrl)
{
	u8 getstatus = 0;

	if (MRL_SENS(ctrl)) {
		pciehp_get_latch_status(ctrl, &getstatus);
		if (getstatus) {
			ctrl_info(ctrl, "Slot(%s): Latch open\n",
				  slot_name(ctrl));
			return -ENODEV;
		}
	}

	if (POWER_CTRL(ctrl)) {
		pciehp_get_power_status(ctrl, &getstatus);
		if (getstatus) {
			ctrl_info(ctrl, "Slot(%s): Already enabled\n",
				  slot_name(ctrl));
			return 0;
		}
	}

	return board_added(ctrl);
}

static int pciehp_enable_slot(struct controller *ctrl)
{
	int ret;

	pm_runtime_get_sync(&ctrl->pcie->port->dev);
	ret = __pciehp_enable_slot(ctrl);
	if (ret && ATTN_BUTTN(ctrl))
		 
		pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_OFF,
				      INDICATOR_NOOP);
	pm_runtime_put(&ctrl->pcie->port->dev);

	mutex_lock(&ctrl->state_lock);
	ctrl->state = ret ? OFF_STATE : ON_STATE;
	mutex_unlock(&ctrl->state_lock);

	return ret;
}

static int __pciehp_disable_slot(struct controller *ctrl, bool safe_removal)
{
	u8 getstatus = 0;

	if (POWER_CTRL(ctrl)) {
		pciehp_get_power_status(ctrl, &getstatus);
		if (!getstatus) {
			ctrl_info(ctrl, "Slot(%s): Already disabled\n",
				  slot_name(ctrl));
			return -EINVAL;
		}
	}

	remove_board(ctrl, safe_removal);
	return 0;
}

static int pciehp_disable_slot(struct controller *ctrl, bool safe_removal)
{
	int ret;

	pm_runtime_get_sync(&ctrl->pcie->port->dev);
	ret = __pciehp_disable_slot(ctrl, safe_removal);
	pm_runtime_put(&ctrl->pcie->port->dev);

	mutex_lock(&ctrl->state_lock);
	ctrl->state = OFF_STATE;
	mutex_unlock(&ctrl->state_lock);

	return ret;
}

int pciehp_sysfs_enable_slot(struct hotplug_slot *hotplug_slot)
{
	struct controller *ctrl = to_ctrl(hotplug_slot);

	mutex_lock(&ctrl->state_lock);
	switch (ctrl->state) {
	case BLINKINGON_STATE:
	case OFF_STATE:
		mutex_unlock(&ctrl->state_lock);
		 
		ctrl->request_result = -ENODEV;
		pciehp_request(ctrl, PCI_EXP_SLTSTA_PDC);
		wait_event(ctrl->requester,
			   !atomic_read(&ctrl->pending_events) &&
			   !ctrl->ist_running);
		return ctrl->request_result;
	case POWERON_STATE:
		ctrl_info(ctrl, "Slot(%s): Already in powering on state\n",
			  slot_name(ctrl));
		break;
	case BLINKINGOFF_STATE:
	case ON_STATE:
	case POWEROFF_STATE:
		ctrl_info(ctrl, "Slot(%s): Already enabled\n",
			  slot_name(ctrl));
		break;
	default:
		ctrl_err(ctrl, "Slot(%s): Invalid state %#x\n",
			 slot_name(ctrl), ctrl->state);
		break;
	}
	mutex_unlock(&ctrl->state_lock);

	return -ENODEV;
}

int pciehp_sysfs_disable_slot(struct hotplug_slot *hotplug_slot)
{
	struct controller *ctrl = to_ctrl(hotplug_slot);

	mutex_lock(&ctrl->state_lock);
	switch (ctrl->state) {
	case BLINKINGOFF_STATE:
	case ON_STATE:
		mutex_unlock(&ctrl->state_lock);
		pciehp_request(ctrl, DISABLE_SLOT);
		wait_event(ctrl->requester,
			   !atomic_read(&ctrl->pending_events) &&
			   !ctrl->ist_running);
		return ctrl->request_result;
	case POWEROFF_STATE:
		ctrl_info(ctrl, "Slot(%s): Already in powering off state\n",
			  slot_name(ctrl));
		break;
	case BLINKINGON_STATE:
	case OFF_STATE:
	case POWERON_STATE:
		ctrl_info(ctrl, "Slot(%s): Already disabled\n",
			  slot_name(ctrl));
		break;
	default:
		ctrl_err(ctrl, "Slot(%s): Invalid state %#x\n",
			 slot_name(ctrl), ctrl->state);
		break;
	}
	mutex_unlock(&ctrl->state_lock);

	return -ENODEV;
}
