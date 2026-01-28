#ifndef _ISHTP_CLIENT_H_
#define _ISHTP_CLIENT_H_
#include <linux/types.h>
#include "ishtp-dev.h"
#define	CL_DEF_RX_RING_SIZE	2
#define	CL_DEF_TX_RING_SIZE	2
#define	CL_MAX_RX_RING_SIZE	32
#define	CL_MAX_TX_RING_SIZE	32
#define DMA_SLOT_SIZE		4096
#define	DMA_WORTH_THRESHOLD	3
#define	CL_TX_PATH_DEFAULT	0
#define	CL_TX_PATH_IPC		1
#define	CL_TX_PATH_DMA		2
struct ishtp_cl_tx_ring {
	struct list_head	list;
	struct ishtp_msg_data	send_buf;
};
struct ishtp_cl {
	struct list_head	link;
	struct ishtp_device	*dev;
	enum cl_state		state;
	int			status;
	struct ishtp_cl_device	*device;
	uint8_t	host_client_id;
	uint8_t	fw_client_id;
	uint8_t	ishtp_flow_ctrl_creds;
	uint8_t	out_flow_ctrl_creds;
	int	last_tx_path;
	int	last_dma_acked;
	unsigned char	*last_dma_addr;
	int	last_ipc_acked;
	unsigned int	rx_ring_size;
	struct ishtp_cl_rb	free_rb_list;
	spinlock_t	free_list_spinlock;
	struct ishtp_cl_rb	in_process_list;
	spinlock_t	in_process_spinlock;
	unsigned int	tx_ring_size;
	struct ishtp_cl_tx_ring	tx_list, tx_free_list;
	int		tx_ring_free_size;
	spinlock_t	tx_list_spinlock;
	spinlock_t	tx_free_list_spinlock;
	size_t	tx_offs;	 
	int	sending;
	spinlock_t	fc_spinlock;
	wait_queue_head_t	wait_ctrl_res;
	unsigned int	err_send_msg;
	unsigned int	err_send_fc;
	unsigned int	send_msg_cnt_ipc;
	unsigned int	send_msg_cnt_dma;
	unsigned int	recv_msg_cnt_ipc;
	unsigned int	recv_msg_cnt_dma;
	unsigned int	recv_msg_num_frags;
	unsigned int	ishtp_flow_ctrl_cnt;
	unsigned int	out_flow_ctrl_cnt;
	ktime_t ts_rx;
	ktime_t ts_out_fc;
	ktime_t ts_max_fc_delay;
	void *client_data;
};
int ishtp_can_client_connect(struct ishtp_device *ishtp_dev, guid_t *uuid);
int ishtp_fw_cl_by_id(struct ishtp_device *dev, uint8_t client_id);
void ishtp_cl_send_msg(struct ishtp_device *dev, struct ishtp_cl *cl);
void recv_ishtp_cl_msg(struct ishtp_device *dev,
		       struct ishtp_msg_hdr *ishtp_hdr);
int ishtp_cl_read_start(struct ishtp_cl *cl);
int ishtp_cl_alloc_rx_ring(struct ishtp_cl *cl);
int ishtp_cl_alloc_tx_ring(struct ishtp_cl *cl);
void ishtp_cl_free_rx_ring(struct ishtp_cl *cl);
void ishtp_cl_free_tx_ring(struct ishtp_cl *cl);
int ishtp_cl_get_tx_free_buffer_size(struct ishtp_cl *cl);
int ishtp_cl_get_tx_free_rings(struct ishtp_cl *cl);
void recv_ishtp_cl_msg_dma(struct ishtp_device *dev, void *msg,
			   struct dma_xfer_hbm *hbm);
void ishtp_cl_alloc_dma_buf(struct ishtp_device *dev);
void ishtp_cl_free_dma_buf(struct ishtp_device *dev);
void *ishtp_cl_get_dma_send_buf(struct ishtp_device *dev,
				uint32_t size);
void ishtp_cl_release_dma_acked_mem(struct ishtp_device *dev,
				    void *msg_addr,
				    uint8_t size);
struct ishtp_cl_rb *ishtp_io_rb_init(struct ishtp_cl *cl);
void ishtp_io_rb_free(struct ishtp_cl_rb *priv_rb);
int ishtp_io_rb_alloc_buf(struct ishtp_cl_rb *rb, size_t length);
static inline bool ishtp_cl_cmp_id(const struct ishtp_cl *cl1,
				   const struct ishtp_cl *cl2)
{
	return cl1 && cl2 &&
		(cl1->host_client_id == cl2->host_client_id) &&
		(cl1->fw_client_id == cl2->fw_client_id);
}
#endif  
