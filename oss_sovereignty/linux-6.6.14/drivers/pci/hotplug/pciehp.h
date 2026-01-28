#ifndef _PCIEHP_H
#define _PCIEHP_H
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/workqueue.h>
#include "../pcie/portdrv.h"
extern bool pciehp_poll_mode;
extern int pciehp_poll_time;
#define ctrl_dbg(ctrl, format, arg...)					\
	pci_dbg(ctrl->pcie->port, format, ## arg)
#define ctrl_err(ctrl, format, arg...)					\
	pci_err(ctrl->pcie->port, format, ## arg)
#define ctrl_info(ctrl, format, arg...)					\
	pci_info(ctrl->pcie->port, format, ## arg)
#define ctrl_warn(ctrl, format, arg...)					\
	pci_warn(ctrl->pcie->port, format, ## arg)
#define SLOT_NAME_SIZE 10
struct controller {
	struct pcie_device *pcie;
	u32 slot_cap;				 
	unsigned int inband_presence_disabled:1;
	u16 slot_ctrl;				 
	struct mutex ctrl_lock;
	unsigned long cmd_started;
	unsigned int cmd_busy:1;
	wait_queue_head_t queue;
	atomic_t pending_events;		 
	unsigned int notification_enabled:1;
	unsigned int power_fault_detected;
	struct task_struct *poll_thread;
	u8 state;				 
	struct mutex state_lock;
	struct delayed_work button_work;
	struct hotplug_slot hotplug_slot;	 
	struct rw_semaphore reset_lock;
	unsigned int depth;
	unsigned int ist_running;
	int request_result;
	wait_queue_head_t requester;
};
#define OFF_STATE			0
#define BLINKINGON_STATE		1
#define BLINKINGOFF_STATE		2
#define POWERON_STATE			3
#define POWEROFF_STATE			4
#define ON_STATE			5
#define DISABLE_SLOT		(1 << 16)
#define RERUN_ISR		(1 << 17)
#define ATTN_BUTTN(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_ABP)
#define POWER_CTRL(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_PCP)
#define MRL_SENS(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_MRLSP)
#define ATTN_LED(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_AIP)
#define PWR_LED(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_PIP)
#define NO_CMD_CMPL(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_NCCS)
#define PSN(ctrl)		(((ctrl)->slot_cap & PCI_EXP_SLTCAP_PSN) >> 19)
void pciehp_request(struct controller *ctrl, int action);
void pciehp_handle_button_press(struct controller *ctrl);
void pciehp_handle_disable_request(struct controller *ctrl);
void pciehp_handle_presence_or_link_change(struct controller *ctrl, u32 events);
int pciehp_configure_device(struct controller *ctrl);
void pciehp_unconfigure_device(struct controller *ctrl, bool presence);
void pciehp_queue_pushbutton_work(struct work_struct *work);
struct controller *pcie_init(struct pcie_device *dev);
int pcie_init_notification(struct controller *ctrl);
void pcie_shutdown_notification(struct controller *ctrl);
void pcie_clear_hotplug_events(struct controller *ctrl);
void pcie_enable_interrupt(struct controller *ctrl);
void pcie_disable_interrupt(struct controller *ctrl);
int pciehp_power_on_slot(struct controller *ctrl);
void pciehp_power_off_slot(struct controller *ctrl);
void pciehp_get_power_status(struct controller *ctrl, u8 *status);
#define INDICATOR_NOOP -1	 
void pciehp_set_indicators(struct controller *ctrl, int pwr, int attn);
void pciehp_get_latch_status(struct controller *ctrl, u8 *status);
int pciehp_query_power_fault(struct controller *ctrl);
int pciehp_card_present(struct controller *ctrl);
int pciehp_card_present_or_link_active(struct controller *ctrl);
int pciehp_check_link_status(struct controller *ctrl);
int pciehp_check_link_active(struct controller *ctrl);
void pciehp_release_ctrl(struct controller *ctrl);
int pciehp_sysfs_enable_slot(struct hotplug_slot *hotplug_slot);
int pciehp_sysfs_disable_slot(struct hotplug_slot *hotplug_slot);
int pciehp_reset_slot(struct hotplug_slot *hotplug_slot, bool probe);
int pciehp_get_attention_status(struct hotplug_slot *hotplug_slot, u8 *status);
int pciehp_set_raw_indicator_status(struct hotplug_slot *h_slot, u8 status);
int pciehp_get_raw_indicator_status(struct hotplug_slot *h_slot, u8 *status);
int pciehp_slot_reset(struct pcie_device *dev);
static inline const char *slot_name(struct controller *ctrl)
{
	return hotplug_slot_name(&ctrl->hotplug_slot);
}
static inline struct controller *to_ctrl(struct hotplug_slot *hotplug_slot)
{
	return container_of(hotplug_slot, struct controller, hotplug_slot);
}
#endif				 
