 

#ifndef _QAIC_H_
#define _QAIC_H_

#include <linux/interrupt.h>
#include <linux/kref.h>
#include <linux/mhi.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <drm/drm_device.h>
#include <drm/drm_gem.h>

#define QAIC_DBC_BASE		SZ_128K
#define QAIC_DBC_SIZE		SZ_4K

#define QAIC_NO_PARTITION	-1

#define QAIC_DBC_OFF(i)		((i) * QAIC_DBC_SIZE + QAIC_DBC_BASE)

#define to_qaic_bo(obj) container_of(obj, struct qaic_bo, base)

extern bool datapath_polling;

struct qaic_user {
	 
	int			handle;
	struct kref		ref_count;
	 
	struct qaic_drm_device	*qddev;
	 
	struct list_head	node;
	 
	struct srcu_struct	qddev_lock;
	atomic_t		chunk_id;
};

struct dma_bridge_chan {
	 
	struct qaic_device	*qdev;
	 
	unsigned int		id;
	 
	spinlock_t		xfer_lock;
	 
	void			*req_q_base;
	 
	void			*rsp_q_base;
	 
	dma_addr_t		dma_addr;
	 
	u32			total_size;
	 
	u32			nelem;
	 
	struct qaic_user	*usr;
	 
	u16			next_req_id;
	 
	bool			in_use;
	 
	void __iomem		*dbc_base;
	 
	struct list_head	xfer_list;
	 
	struct srcu_struct	ch_lock;
	 
	wait_queue_head_t	dbc_release;
	 
	struct list_head	bo_lists;
	 
	unsigned int		irq;
	 
	struct work_struct	poll_work;
};

struct qaic_device {
	 
	struct pci_dev		*pdev;
	 
	u32			next_seq_num;
	 
	void __iomem		*bar_0;
	 
	void __iomem		*bar_2;
	 
	struct mhi_controller	*mhi_cntrl;
	 
	struct mhi_device	*cntl_ch;
	 
	struct list_head	cntl_xfer_list;
	 
	struct mutex		cntl_mutex;
	 
	struct dma_bridge_chan	*dbc;
	 
	struct workqueue_struct	*cntl_wq;
	 
	struct srcu_struct	dev_lock;
	 
	bool			in_reset;
	 
	bool			cntl_lost_buf;
	 
	u32			num_dbc;
	 
	struct qaic_drm_device	*qddev;
	 
	u32 (*gen_crc)(void *msg);
	 
	bool (*valid_crc)(void *msg);
};

struct qaic_drm_device {
	 
	struct qaic_device	*qdev;
	 
	s32			partition_id;
	 
	struct drm_device	*ddev;
	 
	struct list_head	users;
	 
	struct mutex		users_mutex;
};

struct qaic_bo {
	struct drm_gem_object	base;
	 
	struct sg_table		*sgt;
	 
	u64			size;
	 
	struct list_head	slices;
	 
	int			total_slice_nents;
	 
	int			dir;
	 
	struct dma_bridge_chan	*dbc;
	 
	u32			nr_slice;
	 
	u32			nr_slice_xfer_done;
	 
	bool			queued;
	 
	bool			sliced;
	 
	u16			req_id;
	 
	u32			handle;
	 
	struct completion	xfer_done;
	 
	struct list_head	xfer_list;
	 
	struct list_head	bo_list;
	struct {
		 
		u64		req_received_ts;
		 
		u64		req_submit_ts;
		 
		u64		req_processed_ts;
		 
		u32		queue_level_before;
	} perf_stats;

};

struct bo_slice {
	 
	struct sg_table		*sgt;
	 
	int			nents;
	 
	int			dir;
	 
	struct dbc_req		*reqs;
	struct kref		ref_count;
	 
	bool			no_xfer;
	 
	struct qaic_bo		*bo;
	 
	struct list_head	slice;
	 
	u64			size;
	 
	u64			offset;
};

int get_dbc_req_elem_size(void);
int get_dbc_rsp_elem_size(void);
int get_cntl_version(struct qaic_device *qdev, struct qaic_user *usr, u16 *major, u16 *minor);
int qaic_manage_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
void qaic_mhi_ul_xfer_cb(struct mhi_device *mhi_dev, struct mhi_result *mhi_result);

void qaic_mhi_dl_xfer_cb(struct mhi_device *mhi_dev, struct mhi_result *mhi_result);

int qaic_control_open(struct qaic_device *qdev);
void qaic_control_close(struct qaic_device *qdev);
void qaic_release_usr(struct qaic_device *qdev, struct qaic_user *usr);

irqreturn_t dbc_irq_threaded_fn(int irq, void *data);
irqreturn_t dbc_irq_handler(int irq, void *data);
int disable_dbc(struct qaic_device *qdev, u32 dbc_id, struct qaic_user *usr);
void enable_dbc(struct qaic_device *qdev, u32 dbc_id, struct qaic_user *usr);
void wakeup_dbc(struct qaic_device *qdev, u32 dbc_id);
void release_dbc(struct qaic_device *qdev, u32 dbc_id);

void wake_all_cntl(struct qaic_device *qdev);
void qaic_dev_reset_clean_local_state(struct qaic_device *qdev, bool exit_reset);

struct drm_gem_object *qaic_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf);

int qaic_create_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int qaic_mmap_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int qaic_attach_slice_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int qaic_execute_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int qaic_partial_execute_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int qaic_wait_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int qaic_perf_stats_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
void irq_polling_work(struct work_struct *work);

#endif  
