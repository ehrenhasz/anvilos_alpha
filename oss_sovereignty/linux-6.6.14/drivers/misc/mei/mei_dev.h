 
 

#ifndef _MEI_DEV_H_
#define _MEI_DEV_H_

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/mei.h>
#include <linux/mei_cl_bus.h>

static inline int uuid_le_cmp(const uuid_le u1, const uuid_le u2)
{
	return memcmp(&u1, &u2, sizeof(uuid_le));
}

#include "hw.h"
#include "hbm.h"

#define MEI_SLOT_SIZE             sizeof(u32)
#define MEI_RD_MSG_BUF_SIZE       (128 * MEI_SLOT_SIZE)

 
#define MEI_CLIENTS_MAX 256

 
#define MEI_MAX_CONSEC_RESET  3

 
#define  MEI_MAX_OPEN_HANDLE_COUNT (MEI_CLIENTS_MAX - 1)

 
enum file_state {
	MEI_FILE_UNINITIALIZED = 0,
	MEI_FILE_INITIALIZING,
	MEI_FILE_CONNECTING,
	MEI_FILE_CONNECTED,
	MEI_FILE_DISCONNECTING,
	MEI_FILE_DISCONNECT_REPLY,
	MEI_FILE_DISCONNECT_REQUIRED,
	MEI_FILE_DISCONNECTED,
};

 
enum mei_dev_state {
	MEI_DEV_INITIALIZING = 0,
	MEI_DEV_INIT_CLIENTS,
	MEI_DEV_ENABLED,
	MEI_DEV_RESETTING,
	MEI_DEV_DISABLED,
	MEI_DEV_POWERING_DOWN,
	MEI_DEV_POWER_DOWN,
	MEI_DEV_POWER_UP
};

 
enum mei_dev_pxp_mode {
	MEI_DEV_PXP_DEFAULT = 0,
	MEI_DEV_PXP_INIT    = 1,
	MEI_DEV_PXP_SETUP   = 2,
	MEI_DEV_PXP_READY   = 3,
};

const char *mei_dev_state_str(int state);

enum mei_file_transaction_states {
	MEI_IDLE,
	MEI_WRITING,
	MEI_WRITE_COMPLETE,
};

 
enum mei_cb_file_ops {
	MEI_FOP_READ = 0,
	MEI_FOP_WRITE,
	MEI_FOP_CONNECT,
	MEI_FOP_DISCONNECT,
	MEI_FOP_DISCONNECT_RSP,
	MEI_FOP_NOTIFY_START,
	MEI_FOP_NOTIFY_STOP,
	MEI_FOP_DMA_MAP,
	MEI_FOP_DMA_UNMAP,
};

 
enum mei_cl_io_mode {
	MEI_CL_IO_TX_BLOCKING = BIT(0),
	MEI_CL_IO_TX_INTERNAL = BIT(1),

	MEI_CL_IO_RX_NONBLOCK = BIT(2),

	MEI_CL_IO_SGL         = BIT(3),
};

 
struct mei_msg_data {
	size_t size;
	unsigned char *data;
};

struct mei_dma_data {
	u8 buffer_id;
	void *vaddr;
	dma_addr_t daddr;
	size_t size;
};

 
struct mei_dma_dscr {
	void *vaddr;
	dma_addr_t daddr;
	size_t size;
};

 
#define MEI_FW_STATUS_MAX 6
 
#define MEI_FW_STATUS_STR_SZ (MEI_FW_STATUS_MAX * (8 + 1))


 
struct mei_fw_status {
	int count;
	u32 status[MEI_FW_STATUS_MAX];
};

 
struct mei_me_client {
	struct list_head list;
	struct kref refcnt;
	struct mei_client_properties props;
	u8 client_id;
	u8 tx_flow_ctrl_creds;
	u8 connect_count;
	u8 bus_added;
};


struct mei_cl;

 
struct mei_cl_cb {
	struct list_head list;
	struct mei_cl *cl;
	enum mei_cb_file_ops fop_type;
	struct mei_msg_data buf;
	size_t buf_idx;
	u8 vtag;
	const struct file *fp;
	int status;
	u32 internal:1;
	u32 blocking:1;
	struct mei_ext_hdr *ext_hdr;
};

 
struct mei_cl_vtag {
	struct list_head list;
	const struct file *fp;
	u8 vtag;
	u8 pending_read:1;
};

 
struct mei_cl {
	struct list_head link;
	struct mei_device *dev;
	enum file_state state;
	wait_queue_head_t tx_wait;
	wait_queue_head_t rx_wait;
	wait_queue_head_t wait;
	wait_queue_head_t ev_wait;
	struct fasync_struct *ev_async;
	int status;
	struct mei_me_client *me_cl;
	const struct file *fp;
	u8 host_client_id;
	struct list_head vtag_map;
	u8 tx_flow_ctrl_creds;
	u8 rx_flow_ctrl_creds;
	u8 timer_count;
	u8 notify_en;
	u8 notify_ev;
	u8 tx_cb_queued;
	enum mei_file_transaction_states writing_state;
	struct list_head rd_pending;
	spinlock_t rd_completed_lock;  
	struct list_head rd_completed;
	struct mei_dma_data dma;
	u8 dma_mapped;

	struct mei_cl_device *cldev;
};

#define MEI_TX_QUEUE_LIMIT_DEFAULT 50
#define MEI_TX_QUEUE_LIMIT_MAX 255
#define MEI_TX_QUEUE_LIMIT_MIN 30

 
struct mei_hw_ops {

	bool (*host_is_ready)(struct mei_device *dev);

	bool (*hw_is_ready)(struct mei_device *dev);
	int (*hw_reset)(struct mei_device *dev, bool enable);
	int (*hw_start)(struct mei_device *dev);
	int (*hw_config)(struct mei_device *dev);

	int (*fw_status)(struct mei_device *dev, struct mei_fw_status *fw_sts);
	int (*trc_status)(struct mei_device *dev, u32 *trc);

	enum mei_pg_state (*pg_state)(struct mei_device *dev);
	bool (*pg_in_transition)(struct mei_device *dev);
	bool (*pg_is_enabled)(struct mei_device *dev);

	void (*intr_clear)(struct mei_device *dev);
	void (*intr_enable)(struct mei_device *dev);
	void (*intr_disable)(struct mei_device *dev);
	void (*synchronize_irq)(struct mei_device *dev);

	int (*hbuf_free_slots)(struct mei_device *dev);
	bool (*hbuf_is_ready)(struct mei_device *dev);
	u32 (*hbuf_depth)(const struct mei_device *dev);
	int (*write)(struct mei_device *dev,
		     const void *hdr, size_t hdr_len,
		     const void *data, size_t data_len);

	int (*rdbuf_full_slots)(struct mei_device *dev);

	u32 (*read_hdr)(const struct mei_device *dev);
	int (*read)(struct mei_device *dev,
		     unsigned char *buf, unsigned long len);
};

 
void mei_cl_bus_rescan_work(struct work_struct *work);
void mei_cl_bus_dev_fixup(struct mei_cl_device *dev);
ssize_t __mei_cl_send(struct mei_cl *cl, const u8 *buf, size_t length, u8 vtag,
		      unsigned int mode);
ssize_t __mei_cl_send_timeout(struct mei_cl *cl, const u8 *buf, size_t length, u8 vtag,
			      unsigned int mode, unsigned long timeout);
ssize_t __mei_cl_recv(struct mei_cl *cl, u8 *buf, size_t length, u8 *vtag,
		      unsigned int mode, unsigned long timeout);
bool mei_cl_bus_rx_event(struct mei_cl *cl);
bool mei_cl_bus_notify_event(struct mei_cl *cl);
void mei_cl_bus_remove_devices(struct mei_device *bus);
int mei_cl_bus_init(void);
void mei_cl_bus_exit(void);

 
enum mei_pg_event {
	MEI_PG_EVENT_IDLE,
	MEI_PG_EVENT_WAIT,
	MEI_PG_EVENT_RECEIVED,
	MEI_PG_EVENT_INTR_WAIT,
	MEI_PG_EVENT_INTR_RECEIVED,
};

 
enum mei_pg_state {
	MEI_PG_OFF = 0,
	MEI_PG_ON =  1,
};

const char *mei_pg_state_str(enum mei_pg_state state);

 
struct mei_fw_version {
	u8 platform;
	u8 major;
	u16 minor;
	u16 buildno;
	u16 hotfix;
};

#define MEI_MAX_FW_VER_BLOCKS 3

struct mei_dev_timeouts {
	unsigned long hw_ready;  
	int connect;  
	unsigned long cl_connect;  
	int client_init;  
	unsigned long pgi;  
	unsigned int d0i3;  
	unsigned long hbm;  
	unsigned long mkhi_recv;  
};

 
struct mei_device {
	struct device *dev;
	struct cdev cdev;
	int minor;

	struct list_head write_list;
	struct list_head write_waiting_list;
	struct list_head ctrl_wr_list;
	struct list_head ctrl_rd_list;
	u8 tx_queue_limit;

	struct list_head file_list;
	long open_handle_count;

	struct mutex device_lock;
	struct delayed_work timer_work;

	bool recvd_hw_ready;
	 
	wait_queue_head_t wait_hw_ready;
	wait_queue_head_t wait_pg;
	wait_queue_head_t wait_hbm_start;

	 
	unsigned long reset_count;
	enum mei_dev_state dev_state;
	enum mei_hbm_state hbm_state;
	enum mei_dev_pxp_mode pxp_mode;
	u16 init_clients_timer;

	 
	enum mei_pg_event pg_event;
#ifdef CONFIG_PM
	struct dev_pm_domain pg_domain;
#endif  

	unsigned char rd_msg_buf[MEI_RD_MSG_BUF_SIZE];
	u32 rd_msg_hdr[MEI_RD_MSG_BUF_SIZE];
	int rd_msg_hdr_count;

	 
	bool hbuf_is_ready;

	struct mei_dma_dscr dr_dscr[DMA_DSCR_NUM];

	struct hbm_version version;
	unsigned int hbm_f_pg_supported:1;
	unsigned int hbm_f_dc_supported:1;
	unsigned int hbm_f_dot_supported:1;
	unsigned int hbm_f_ev_supported:1;
	unsigned int hbm_f_fa_supported:1;
	unsigned int hbm_f_ie_supported:1;
	unsigned int hbm_f_os_supported:1;
	unsigned int hbm_f_dr_supported:1;
	unsigned int hbm_f_vt_supported:1;
	unsigned int hbm_f_cap_supported:1;
	unsigned int hbm_f_cd_supported:1;
	unsigned int hbm_f_gsc_supported:1;

	struct mei_fw_version fw_ver[MEI_MAX_FW_VER_BLOCKS];

	unsigned int fw_f_fw_ver_supported:1;
	unsigned int fw_ver_received:1;

	struct rw_semaphore me_clients_rwsem;
	struct list_head me_clients;
	DECLARE_BITMAP(me_clients_map, MEI_CLIENTS_MAX);
	DECLARE_BITMAP(host_clients_map, MEI_CLIENTS_MAX);

	bool allow_fixed_address;
	bool override_fixed_address;

	struct mei_dev_timeouts timeouts;

	struct work_struct reset_work;
	struct work_struct bus_rescan_work;

	 
	struct list_head device_list;
	struct mutex cl_bus_lock;

	const char *kind;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs_dir;
#endif  

	const struct mei_hw_ops *ops;
	char hw[] __aligned(sizeof(void *));
};

static inline unsigned long mei_secs_to_jiffies(unsigned long sec)
{
	return msecs_to_jiffies(sec * MSEC_PER_SEC);
}

 
static inline u32 mei_data2slots(size_t length)
{
	return DIV_ROUND_UP(length, MEI_SLOT_SIZE);
}

 
static inline u32 mei_hbm2slots(size_t length)
{
	return DIV_ROUND_UP(sizeof(struct mei_msg_hdr) + length, MEI_SLOT_SIZE);
}

 
static inline u32 mei_slots2data(int slots)
{
	return slots * MEI_SLOT_SIZE;
}

 
void mei_device_init(struct mei_device *dev,
		     struct device *device,
		     bool slow_fw,
		     const struct mei_hw_ops *hw_ops);
int mei_reset(struct mei_device *dev);
int mei_start(struct mei_device *dev);
int mei_restart(struct mei_device *dev);
void mei_stop(struct mei_device *dev);
void mei_cancel_work(struct mei_device *dev);

void mei_set_devstate(struct mei_device *dev, enum mei_dev_state state);

int mei_dmam_ring_alloc(struct mei_device *dev);
void mei_dmam_ring_free(struct mei_device *dev);
bool mei_dma_ring_is_allocated(struct mei_device *dev);
void mei_dma_ring_reset(struct mei_device *dev);
void mei_dma_ring_read(struct mei_device *dev, unsigned char *buf, u32 len);
void mei_dma_ring_write(struct mei_device *dev, unsigned char *buf, u32 len);
u32 mei_dma_ring_empty_slots(struct mei_device *dev);

 

void mei_timer(struct work_struct *work);
void mei_schedule_stall_timer(struct mei_device *dev);
int mei_irq_read_handler(struct mei_device *dev,
			 struct list_head *cmpl_list, s32 *slots);

int mei_irq_write_handler(struct mei_device *dev, struct list_head *cmpl_list);
void mei_irq_compl_handler(struct mei_device *dev, struct list_head *cmpl_list);

 


static inline int mei_hw_config(struct mei_device *dev)
{
	return dev->ops->hw_config(dev);
}

static inline enum mei_pg_state mei_pg_state(struct mei_device *dev)
{
	return dev->ops->pg_state(dev);
}

static inline bool mei_pg_in_transition(struct mei_device *dev)
{
	return dev->ops->pg_in_transition(dev);
}

static inline bool mei_pg_is_enabled(struct mei_device *dev)
{
	return dev->ops->pg_is_enabled(dev);
}

static inline int mei_hw_reset(struct mei_device *dev, bool enable)
{
	return dev->ops->hw_reset(dev, enable);
}

static inline int mei_hw_start(struct mei_device *dev)
{
	return dev->ops->hw_start(dev);
}

static inline void mei_clear_interrupts(struct mei_device *dev)
{
	dev->ops->intr_clear(dev);
}

static inline void mei_enable_interrupts(struct mei_device *dev)
{
	dev->ops->intr_enable(dev);
}

static inline void mei_disable_interrupts(struct mei_device *dev)
{
	dev->ops->intr_disable(dev);
}

static inline void mei_synchronize_irq(struct mei_device *dev)
{
	dev->ops->synchronize_irq(dev);
}

static inline bool mei_host_is_ready(struct mei_device *dev)
{
	return dev->ops->host_is_ready(dev);
}
static inline bool mei_hw_is_ready(struct mei_device *dev)
{
	return dev->ops->hw_is_ready(dev);
}

static inline bool mei_hbuf_is_ready(struct mei_device *dev)
{
	return dev->ops->hbuf_is_ready(dev);
}

static inline int mei_hbuf_empty_slots(struct mei_device *dev)
{
	return dev->ops->hbuf_free_slots(dev);
}

static inline u32 mei_hbuf_depth(const struct mei_device *dev)
{
	return dev->ops->hbuf_depth(dev);
}

static inline int mei_write_message(struct mei_device *dev,
				    const void *hdr, size_t hdr_len,
				    const void *data, size_t data_len)
{
	return dev->ops->write(dev, hdr, hdr_len, data, data_len);
}

static inline u32 mei_read_hdr(const struct mei_device *dev)
{
	return dev->ops->read_hdr(dev);
}

static inline void mei_read_slots(struct mei_device *dev,
		     unsigned char *buf, unsigned long len)
{
	dev->ops->read(dev, buf, len);
}

static inline int mei_count_full_read_slots(struct mei_device *dev)
{
	return dev->ops->rdbuf_full_slots(dev);
}

static inline int mei_trc_status(struct mei_device *dev, u32 *trc)
{
	if (dev->ops->trc_status)
		return dev->ops->trc_status(dev, trc);
	return -EOPNOTSUPP;
}

static inline int mei_fw_status(struct mei_device *dev,
				struct mei_fw_status *fw_status)
{
	return dev->ops->fw_status(dev, fw_status);
}

bool mei_hbuf_acquire(struct mei_device *dev);

bool mei_write_is_idle(struct mei_device *dev);

#if IS_ENABLED(CONFIG_DEBUG_FS)
void mei_dbgfs_register(struct mei_device *dev, const char *name);
void mei_dbgfs_deregister(struct mei_device *dev);
#else
static inline void mei_dbgfs_register(struct mei_device *dev, const char *name) {}
static inline void mei_dbgfs_deregister(struct mei_device *dev) {}
#endif  

int mei_register(struct mei_device *dev, struct device *parent);
void mei_deregister(struct mei_device *dev);

#define MEI_HDR_FMT "hdr:host=%02d me=%02d len=%d dma=%1d ext=%1d internal=%1d comp=%1d"
#define MEI_HDR_PRM(hdr)                  \
	(hdr)->host_addr, (hdr)->me_addr, \
	(hdr)->length, (hdr)->dma_ring, (hdr)->extended, \
	(hdr)->internal, (hdr)->msg_complete

ssize_t mei_fw_status2str(struct mei_fw_status *fw_sts, char *buf, size_t len);
 
static inline ssize_t mei_fw_status_str(struct mei_device *dev,
					char *buf, size_t len)
{
	struct mei_fw_status fw_status;
	int ret;

	buf[0] = '\0';

	ret = mei_fw_status(dev, &fw_status);
	if (ret)
		return ret;

	ret = mei_fw_status2str(&fw_status, buf, MEI_FW_STATUS_STR_SZ);

	return ret;
}


#endif
