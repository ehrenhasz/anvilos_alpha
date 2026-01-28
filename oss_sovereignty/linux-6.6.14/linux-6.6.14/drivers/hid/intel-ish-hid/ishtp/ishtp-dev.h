#ifndef _ISHTP_DEV_H_
#define _ISHTP_DEV_H_
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/intel-ish-client-if.h>
#include "bus.h"
#include "hbm.h"
#define	IPC_PAYLOAD_SIZE	128
#define ISHTP_RD_MSG_BUF_SIZE	IPC_PAYLOAD_SIZE
#define	IPC_FULL_MSG_SIZE	132
#define	RD_INT_FIFO_SIZE	64
#define	IPC_TX_FIFO_SIZE	512
#define ISHTP_CLIENTS_MAX 256
#define ISHTP_MAX_OPEN_HANDLE_COUNT (ISHTP_CLIENTS_MAX - 1)
#define ISHTP_HOST_CLIENT_ID_ANY		(-1)
#define ISHTP_HBM_HOST_CLIENT_ID		0
#define	MAX_DMA_DELAY	20
enum ishtp_dev_state {
	ISHTP_DEV_INITIALIZING = 0,
	ISHTP_DEV_INIT_CLIENTS,
	ISHTP_DEV_ENABLED,
	ISHTP_DEV_RESETTING,
	ISHTP_DEV_DISABLED,
	ISHTP_DEV_POWER_DOWN,
	ISHTP_DEV_POWER_UP
};
const char *ishtp_dev_state_str(int state);
struct ishtp_cl;
struct ishtp_fw_client {
	struct ishtp_client_properties props;
	uint8_t client_id;
};
struct wr_msg_ctl_info {
	void (*ipc_send_compl)(void *);
	void *ipc_send_compl_prm;
	size_t length;
	struct list_head	link;
	unsigned char	inline_data[IPC_FULL_MSG_SIZE];
};
struct ishtp_hw_ops {
	int	(*hw_reset)(struct ishtp_device *dev);
	int	(*ipc_reset)(struct ishtp_device *dev);
	uint32_t (*ipc_get_header)(struct ishtp_device *dev, int length,
				   int busy);
	int	(*write)(struct ishtp_device *dev,
		void (*ipc_send_compl)(void *), void *ipc_send_compl_prm,
		unsigned char *msg, int length);
	uint32_t	(*ishtp_read_hdr)(const struct ishtp_device *dev);
	int	(*ishtp_read)(struct ishtp_device *dev, unsigned char *buffer,
			unsigned long buffer_length);
	uint32_t	(*get_fw_status)(struct ishtp_device *dev);
	void	(*sync_fw_clock)(struct ishtp_device *dev);
	bool	(*dma_no_cache_snooping)(struct ishtp_device *dev);
};
struct ishtp_device {
	struct device *devc;	 
	struct pci_dev *pdev;	 
	wait_queue_head_t suspend_wait;
	bool suspend_flag;	 
	wait_queue_head_t resume_wait;
	bool resume_flag;	 
	spinlock_t device_lock;
	bool recvd_hw_ready;
	struct hbm_version version;
	int transfer_path;  
	enum ishtp_dev_state dev_state;
	enum ishtp_hbm_state hbm_state;
	struct ishtp_cl_rb read_list;
	spinlock_t read_list_spinlock;
	struct list_head cl_list;
	spinlock_t cl_list_lock;
	long open_handle_count;
	struct list_head device_list;
	spinlock_t device_list_lock;
	wait_queue_head_t wait_hw_ready;
	wait_queue_head_t wait_hbm_recvd_msg;
	unsigned char rd_msg_fifo[RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE];
	unsigned int rd_msg_fifo_head, rd_msg_fifo_tail;
	spinlock_t rd_msg_spinlock;
	struct work_struct bh_hbm_work;
	struct list_head wr_processing_list, wr_free_list;
	spinlock_t wr_processing_spinlock;
	struct ishtp_fw_client *fw_clients;  
	DECLARE_BITMAP(fw_clients_map, ISHTP_CLIENTS_MAX);
	DECLARE_BITMAP(host_clients_map, ISHTP_CLIENTS_MAX);
	uint8_t fw_clients_num;
	uint8_t fw_client_presentation_num;
	uint8_t fw_client_index;
	spinlock_t fw_clients_lock;
	int ishtp_host_dma_enabled;
	void *ishtp_host_dma_tx_buf;
	unsigned int ishtp_host_dma_tx_buf_size;
	uint64_t ishtp_host_dma_tx_buf_phys;
	int ishtp_dma_num_slots;
	uint8_t *ishtp_dma_tx_map;
	spinlock_t ishtp_dma_tx_lock;
	void *ishtp_host_dma_rx_buf;
	unsigned int ishtp_host_dma_rx_buf_size;
	uint64_t ishtp_host_dma_rx_buf_phys;
	ishtp_print_log print_log;
	unsigned int	ipc_rx_cnt;
	unsigned long long	ipc_rx_bytes_cnt;
	unsigned int	ipc_tx_cnt;
	unsigned long long	ipc_tx_bytes_cnt;
	const struct ishtp_hw_ops *ops;
	size_t	mtu;
	uint32_t	ishtp_msg_hdr;
	char hw[] __aligned(sizeof(void *));
};
static inline unsigned long ishtp_secs_to_jiffies(unsigned long sec)
{
	return msecs_to_jiffies(sec * MSEC_PER_SEC);
}
static inline int ish_ipc_reset(struct ishtp_device *dev)
{
	return dev->ops->ipc_reset(dev);
}
void	ishtp_device_init(struct ishtp_device *dev);
int	ishtp_start(struct ishtp_device *dev);
#endif  
