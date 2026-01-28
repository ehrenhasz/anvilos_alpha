#ifndef IOSM_IPC_PROTOCOL_H
#define IOSM_IPC_PROTOCOL_H
#include "iosm_ipc_imem.h"
#include "iosm_ipc_pm.h"
#include "iosm_ipc_protocol_ops.h"
#define IPC_DOORBELL_IRQ_HPDA 0
#define IPC_DOORBELL_IRQ_IPC 1
#define IPC_DOORBELL_IRQ_SLEEP 2
#define IPC_DEVICE_IRQ_VECTOR 0
#define IPC_MSG_IRQ_VECTOR 0
#define IPC_UL_PIPE_IRQ_VECTOR 0
#define IPC_DL_PIPE_IRQ_VECTOR 0
#define IPC_MEM_MSG_ENTRIES 128
#define IPC_MSG_COMPLETE_RUN_DEFAULT_TIMEOUT 500  
#define IPC_MSG_COMPLETE_BOOT_DEFAULT_TIMEOUT 500  
struct ipc_protocol_context_info {
	phys_addr_t device_info_addr;
	phys_addr_t head_array;
	phys_addr_t tail_array;
	phys_addr_t msg_head;
	phys_addr_t msg_tail;
	phys_addr_t msg_ring_addr;
	__le16 msg_ring_entries;
	u8 msg_irq_vector;
	u8 device_info_irq_vector;
};
struct ipc_protocol_device_info {
	__le32 execution_stage;
	__le32 ipc_status;
	__le32 device_sleep_notification;
};
struct ipc_protocol_ap_shm {
	struct ipc_protocol_context_info ci;
	struct ipc_protocol_device_info device_info;
	__le32 msg_head;
	__le32 head_array[IPC_MEM_MAX_PIPES];
	__le32 msg_tail;
	__le32 tail_array[IPC_MEM_MAX_PIPES];
	union ipc_mem_msg_entry msg_ring[IPC_MEM_MSG_ENTRIES];
};
struct iosm_protocol {
	struct ipc_protocol_ap_shm *p_ap_shm;
	struct iosm_pm pm;
	struct iosm_pcie *pcie;
	struct iosm_imem *imem;
	struct ipc_rsp *rsp_ring[IPC_MEM_MSG_ENTRIES];
	struct device *dev;
	dma_addr_t phy_ap_shm;
	u32 old_msg_tail;
};
struct ipc_call_msg_send_args {
	union ipc_msg_prep_args *prep_args;
	struct ipc_rsp *response;
	enum ipc_msg_prep_type msg_type;
};
int ipc_protocol_tq_msg_send(struct iosm_protocol *ipc_protocol,
			     enum ipc_msg_prep_type msg_type,
			     union ipc_msg_prep_args *prep_args,
			     struct ipc_rsp *response);
int ipc_protocol_msg_send(struct iosm_protocol *ipc_protocol,
			  enum ipc_msg_prep_type prep,
			  union ipc_msg_prep_args *prep_args);
bool ipc_protocol_suspend(struct iosm_protocol *ipc_protocol);
void ipc_protocol_s2idle_sleep(struct iosm_protocol *ipc_protocol, bool sleep);
bool ipc_protocol_resume(struct iosm_protocol *ipc_protocol);
bool ipc_protocol_pm_dev_sleep_handle(struct iosm_protocol *ipc_protocol);
void ipc_protocol_doorbell_trigger(struct iosm_protocol *ipc_protocol,
				   u32 identifier);
const char *
ipc_protocol_sleep_notification_string(struct iosm_protocol *ipc_protocol);
struct iosm_protocol *ipc_protocol_init(struct iosm_imem *ipc_imem);
void ipc_protocol_deinit(struct iosm_protocol *ipc_protocol);
#endif
