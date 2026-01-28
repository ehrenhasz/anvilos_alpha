

#ifndef IOSM_IPC_PM_H
#define IOSM_IPC_PM_H


#define ipc_cp_irq_sleep_control(ipc_pcie, data)                               \
	ipc_doorbell_fire(ipc_pcie, IPC_DOORBELL_IRQ_SLEEP, data)


#define ipc_cp_irq_hpda_update(ipc_pcie, data)                                 \
	ipc_doorbell_fire(ipc_pcie, IPC_DOORBELL_IRQ_HPDA, 0xFF & (data))


union ipc_pm_cond {
	unsigned int raw;

	struct {
		unsigned int irq:1,
			     hs:1,
			     link:1;
	};
};


enum ipc_mem_host_pm_state {
	IPC_MEM_HOST_PM_ACTIVE,
	IPC_MEM_HOST_PM_ACTIVE_WAIT,
	IPC_MEM_HOST_PM_SLEEP_WAIT_IDLE,
	IPC_MEM_HOST_PM_SLEEP_WAIT_D3,
	IPC_MEM_HOST_PM_SLEEP,
	IPC_MEM_HOST_PM_SLEEP_WAIT_EXIT_SLEEP,
};


enum ipc_mem_dev_pm_state {
	IPC_MEM_DEV_PM_ACTIVE,
	IPC_MEM_DEV_PM_SLEEP,
	IPC_MEM_DEV_PM_WAKEUP,
	IPC_MEM_DEV_PM_HOST_SLEEP,
	IPC_MEM_DEV_PM_ACTIVE_WAIT,
	IPC_MEM_DEV_PM_FORCE_SLEEP = 7,
	IPC_MEM_DEV_PM_FORCE_ACTIVE,
};


struct iosm_pm {
	struct iosm_pcie *pcie;
	struct device *dev;
	enum ipc_mem_host_pm_state host_pm_state;
	unsigned long host_sleep_pend;
	struct completion host_sleep_complete;
	union ipc_pm_cond pm_cond;
	enum ipc_mem_dev_pm_state ap_state;
	enum ipc_mem_dev_pm_state cp_state;
	u32 device_sleep_notification;
	u8 pending_hpda_update:1;
};


enum ipc_pm_unit {
	IPC_PM_UNIT_IRQ,
	IPC_PM_UNIT_HS,
	IPC_PM_UNIT_LINK,
};


void ipc_pm_init(struct iosm_protocol *ipc_protocol);


void ipc_pm_deinit(struct iosm_protocol *ipc_protocol);


bool ipc_pm_dev_slp_notification(struct iosm_pm *ipc_pm,
				 u32 sleep_notification);


void ipc_pm_set_s2idle_sleep(struct iosm_pm *ipc_pm, bool sleep);


bool ipc_pm_prepare_host_sleep(struct iosm_pm *ipc_pm);


bool ipc_pm_prepare_host_active(struct iosm_pm *ipc_pm);


bool ipc_pm_wait_for_device_active(struct iosm_pm *ipc_pm);


void ipc_pm_signal_hpda_doorbell(struct iosm_pm *ipc_pm, u32 identifier,
				 bool host_slp_check);

bool ipc_pm_trigger(struct iosm_pm *ipc_pm, enum ipc_pm_unit unit, bool active);

#endif
