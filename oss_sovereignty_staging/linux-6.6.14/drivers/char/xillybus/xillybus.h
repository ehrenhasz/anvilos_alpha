 
 

#ifndef __XILLYBUS_H
#define __XILLYBUS_H

#include <linux/list.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

struct xilly_endpoint_hardware;

struct xilly_buffer {
	void *addr;
	dma_addr_t dma_addr;
	int end_offset;  
};

struct xilly_idt_handle {
	unsigned char *chandesc;
	unsigned char *names;
	int names_len;
	int entries;
};

 

struct xilly_channel {
	struct xilly_endpoint *endpoint;
	int chan_num;
	int log2_element_size;
	int seekable;

	struct xilly_buffer **wr_buffers;  
	int num_wr_buffers;
	unsigned int wr_buf_size;  
	int wr_fpga_buf_idx;
	int wr_host_buf_idx;
	int wr_host_buf_pos;
	int wr_empty;
	int wr_ready;  
	int wr_sleepy;
	int wr_eof;
	int wr_hangup;
	spinlock_t wr_spinlock;
	struct mutex wr_mutex;
	wait_queue_head_t wr_wait;
	wait_queue_head_t wr_ready_wait;
	int wr_ref_count;
	int wr_synchronous;
	int wr_allow_partial;
	int wr_exclusive_open;
	int wr_supports_nonempty;

	struct xilly_buffer **rd_buffers;  
	int num_rd_buffers;
	unsigned int rd_buf_size;  
	int rd_fpga_buf_idx;
	int rd_host_buf_pos;
	int rd_host_buf_idx;
	int rd_full;
	spinlock_t rd_spinlock;
	struct mutex rd_mutex;
	wait_queue_head_t rd_wait;
	int rd_ref_count;
	int rd_allow_partial;
	int rd_synchronous;
	int rd_exclusive_open;
	struct delayed_work rd_workitem;
	unsigned char rd_leftovers[4];
};

struct xilly_endpoint {
	struct device *dev;
	struct module *owner;

	int dma_using_dac;  
	__iomem void *registers;
	int fatal_error;

	struct mutex register_mutex;
	wait_queue_head_t ep_wait;

	int num_channels;  
	struct xilly_channel **channels;
	int msg_counter;
	int failed_messages;
	int idtlen;

	u32 *msgbuf_addr;
	dma_addr_t msgbuf_dma_addr;
	unsigned int msg_buf_size;
};

struct xilly_mapping {
	struct device *device;
	dma_addr_t dma_addr;
	size_t size;
	int direction;
};

irqreturn_t xillybus_isr(int irq, void *data);

struct xilly_endpoint *xillybus_init_endpoint(struct device *dev);

int xillybus_endpoint_discovery(struct xilly_endpoint *endpoint);

void xillybus_endpoint_remove(struct xilly_endpoint *endpoint);

#endif  
