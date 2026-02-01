 
 

#ifndef _DRBD_INT_H
#define _DRBD_INT_H

#include <crypto/hash.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched/signal.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/tcp.h>
#include <linux/mutex.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/idr.h>
#include <linux/dynamic_debug.h>
#include <net/tcp.h>
#include <linux/lru_cache.h>
#include <linux/prefetch.h>
#include <linux/drbd_genl_api.h>
#include <linux/drbd.h>
#include <linux/drbd_config.h>
#include "drbd_strings.h"
#include "drbd_state.h"
#include "drbd_protocol.h"
#include "drbd_polymorph_printk.h"

 
#ifdef CONFIG_DRBD_FAULT_INJECTION
extern int drbd_enable_faults;
extern int drbd_fault_rate;
#endif

extern unsigned int drbd_minor_count;
extern char drbd_usermode_helper[];
extern int drbd_proc_details;


 
#define DRBD_SIGKILL SIGHUP

#define ID_IN_SYNC      (4711ULL)
#define ID_OUT_OF_SYNC  (4712ULL)
#define ID_SYNCER (-1ULL)

#define UUID_NEW_BM_OFFSET ((u64)0x0001000000000000ULL)

struct drbd_device;
struct drbd_connection;
struct drbd_peer_device;

 
enum {
	DRBD_FAULT_MD_WR = 0,	 
	DRBD_FAULT_MD_RD = 1,	 
	DRBD_FAULT_RS_WR = 2,	 
	DRBD_FAULT_RS_RD = 3,
	DRBD_FAULT_DT_WR = 4,	 
	DRBD_FAULT_DT_RD = 5,
	DRBD_FAULT_DT_RA = 6,	 
	DRBD_FAULT_BM_ALLOC = 7,	 
	DRBD_FAULT_AL_EE = 8,	 
	DRBD_FAULT_RECEIVE = 9,  

	DRBD_FAULT_MAX,
};

extern unsigned int
_drbd_insert_fault(struct drbd_device *device, unsigned int type);

static inline int
drbd_insert_fault(struct drbd_device *device, unsigned int type) {
#ifdef CONFIG_DRBD_FAULT_INJECTION
	return drbd_fault_rate &&
		(drbd_enable_faults & (1<<type)) &&
		_drbd_insert_fault(device, type);
#else
	return 0;
#endif
}

 
#define div_ceil(A, B) ((A)/(B) + ((A)%(B) ? 1 : 0))
 
#define div_floor(A, B) ((A)/(B))

extern struct ratelimit_state drbd_ratelimit_state;
extern struct idr drbd_devices;  
extern struct list_head drbd_resources;  

extern const char *cmdname(enum drbd_packet cmd);

 
struct bm_xfer_ctx {
	 
	unsigned long bm_bits;
	unsigned long bm_words;
	 
	unsigned long bit_offset;
	unsigned long word_offset;

	 
	unsigned packets[2];
	unsigned bytes[2];
};

extern void INFO_bm_xfer_stats(struct drbd_peer_device *peer_device,
			       const char *direction, struct bm_xfer_ctx *c);

static inline void bm_xfer_ctx_bit_to_word_offset(struct bm_xfer_ctx *c)
{
	 
#if BITS_PER_LONG == 64
	c->word_offset = c->bit_offset >> 6;
#elif BITS_PER_LONG == 32
	c->word_offset = c->bit_offset >> 5;
	c->word_offset &= ~(1UL);
#else
# error "unsupported BITS_PER_LONG"
#endif
}

extern unsigned int drbd_header_size(struct drbd_connection *connection);

 
enum drbd_thread_state {
	NONE,
	RUNNING,
	EXITING,
	RESTARTING
};

struct drbd_thread {
	spinlock_t t_lock;
	struct task_struct *task;
	struct completion stop;
	enum drbd_thread_state t_state;
	int (*function) (struct drbd_thread *);
	struct drbd_resource *resource;
	struct drbd_connection *connection;
	int reset_cpu_mask;
	const char *name;
};

static inline enum drbd_thread_state get_t_state(struct drbd_thread *thi)
{
	 

	smp_rmb();
	return thi->t_state;
}

struct drbd_work {
	struct list_head list;
	int (*cb)(struct drbd_work *, int cancel);
};

struct drbd_device_work {
	struct drbd_work w;
	struct drbd_device *device;
};

#include "drbd_interval.h"

extern int drbd_wait_misc(struct drbd_device *, struct drbd_interval *);

extern void lock_all_resources(void);
extern void unlock_all_resources(void);

struct drbd_request {
	struct drbd_work w;
	struct drbd_device *device;

	 
	struct bio *private_bio;

	struct drbd_interval i;

	 
	unsigned int epoch;

	struct list_head tl_requests;  
	struct bio *master_bio;        

	 
	struct list_head req_pending_master_completion;
	struct list_head req_pending_local;

	 
	unsigned long start_jif;

	 

	 

	 
	unsigned long in_actlog_jif;

	 
	unsigned long pre_submit_jif;

	 
	unsigned long pre_send_jif;
	unsigned long acked_jif;
	unsigned long net_done_jif;

	 


	 
	atomic_t completion_ref;
	 
	struct kref kref;

	unsigned rq_state;  
};

struct drbd_epoch {
	struct drbd_connection *connection;
	struct list_head list;
	unsigned int barrier_nr;
	atomic_t epoch_size;  
	atomic_t active;      
	unsigned long flags;
};

 
int drbdd_init(struct drbd_thread *);
int drbd_asender(struct drbd_thread *);

 
enum {
	DE_HAVE_BARRIER_NUMBER,
};

enum epoch_event {
	EV_PUT,
	EV_GOT_BARRIER_NR,
	EV_BECAME_LAST,
	EV_CLEANUP = 32,  
};

struct digest_info {
	int digest_size;
	void *digest;
};

struct drbd_peer_request {
	struct drbd_work w;
	struct drbd_peer_device *peer_device;
	struct drbd_epoch *epoch;  
	struct page *pages;
	blk_opf_t opf;
	atomic_t pending_bios;
	struct drbd_interval i;
	 
	unsigned long flags;
	unsigned long submit_jif;
	union {
		u64 block_id;
		struct digest_info *digest;
	};
};

 
#define peer_req_op(peer_req) \
	((peer_req)->opf & REQ_OP_MASK)

 
enum {
	__EE_CALL_AL_COMPLETE_IO,
	__EE_MAY_SET_IN_SYNC,

	 
	__EE_TRIM,
	 
	__EE_ZEROOUT,

	 
	__EE_RESUBMITTED,

	 
	__EE_WAS_ERROR,

	 
	__EE_HAS_DIGEST,

	 
	__EE_RESTART_REQUESTS,

	 
	__EE_SEND_WRITE_ACK,

	 
	__EE_IN_INTERVAL_TREE,

	 
	 
	__EE_SUBMITTED,

	 
	__EE_WRITE,

	 
	__EE_WRITE_SAME,

	 
	__EE_APPLICATION,

	 
	__EE_RS_THIN_REQ,
};
#define EE_CALL_AL_COMPLETE_IO (1<<__EE_CALL_AL_COMPLETE_IO)
#define EE_MAY_SET_IN_SYNC     (1<<__EE_MAY_SET_IN_SYNC)
#define EE_TRIM                (1<<__EE_TRIM)
#define EE_ZEROOUT             (1<<__EE_ZEROOUT)
#define EE_RESUBMITTED         (1<<__EE_RESUBMITTED)
#define EE_WAS_ERROR           (1<<__EE_WAS_ERROR)
#define EE_HAS_DIGEST          (1<<__EE_HAS_DIGEST)
#define EE_RESTART_REQUESTS	(1<<__EE_RESTART_REQUESTS)
#define EE_SEND_WRITE_ACK	(1<<__EE_SEND_WRITE_ACK)
#define EE_IN_INTERVAL_TREE	(1<<__EE_IN_INTERVAL_TREE)
#define EE_SUBMITTED		(1<<__EE_SUBMITTED)
#define EE_WRITE		(1<<__EE_WRITE)
#define EE_WRITE_SAME		(1<<__EE_WRITE_SAME)
#define EE_APPLICATION		(1<<__EE_APPLICATION)
#define EE_RS_THIN_REQ		(1<<__EE_RS_THIN_REQ)

 
enum {
	UNPLUG_REMOTE,		 
	MD_DIRTY,		 
	USE_DEGR_WFC_T,		 
	CL_ST_CHG_SUCCESS,
	CL_ST_CHG_FAIL,
	CRASHED_PRIMARY,	 
	CONSIDER_RESYNC,

	MD_NO_FUA,		 

	BITMAP_IO,		 
	BITMAP_IO_QUEUED,        
	WAS_IO_ERROR,		 
	WAS_READ_ERROR,		 
	FORCE_DETACH,		 
	RESYNC_AFTER_NEG,        
	RESIZE_PENDING,		 
	NEW_CUR_UUID,		 
	AL_SUSPENDED,		 
	AHEAD_TO_SYNC_SOURCE,    
	B_RS_H_DONE,		 
	DISCARD_MY_DATA,	 
	READ_BALANCE_RR,

	FLUSH_PENDING,		 

	 
	GOING_DISKLESS,		 

	 
	GO_DISKLESS,		 
	DESTROY_DISK,		 
	MD_SYNC,		 
	RS_START,		 
	RS_PROGRESS,		 
	RS_DONE,		 
};

struct drbd_bitmap;  

 
enum bm_flag {
	 
	BM_LOCKED_MASK = 0xf,

	 
	BM_DONT_CLEAR = 0x1,
	BM_DONT_SET   = 0x2,
	BM_DONT_TEST  = 0x4,

	 
	BM_IS_LOCKED  = 0x8,

	 
	BM_LOCKED_TEST_ALLOWED = BM_DONT_CLEAR | BM_DONT_SET | BM_IS_LOCKED,

	 
	BM_LOCKED_SET_ALLOWED = BM_DONT_CLEAR | BM_IS_LOCKED,

	 
	BM_LOCKED_CHANGE_ALLOWED = BM_IS_LOCKED,
};

struct drbd_work_queue {
	struct list_head q;
	spinlock_t q_lock;   
	wait_queue_head_t q_wait;
};

struct drbd_socket {
	struct mutex mutex;
	struct socket    *socket;
	 
	void *sbuf;
	void *rbuf;
};

struct drbd_md {
	u64 md_offset;		 

	u64 la_size_sect;	 
	spinlock_t uuid_lock;
	u64 uuid[UI_SIZE];
	u64 device_uuid;
	u32 flags;
	u32 md_size_sect;

	s32 al_offset;	 
	s32 bm_offset;	 

	 
	s32 meta_dev_idx;

	 
	u32 al_stripes;
	u32 al_stripe_size_4k;
	u32 al_size_4k;  
};

struct drbd_backing_dev {
	struct block_device *backing_bdev;
	struct block_device *md_bdev;
	struct drbd_md md;
	struct disk_conf *disk_conf;  
	sector_t known_size;  
};

struct drbd_md_io {
	struct page *page;
	unsigned long start_jif;	 
	unsigned long submit_jif;	 
	const char *current_use;
	atomic_t in_use;
	unsigned int done;
	int error;
};

struct bm_io_work {
	struct drbd_work w;
	struct drbd_peer_device *peer_device;
	char *why;
	enum bm_flag flags;
	int (*io_fn)(struct drbd_device *device, struct drbd_peer_device *peer_device);
	void (*done)(struct drbd_device *device, int rv);
};

struct fifo_buffer {
	unsigned int head_index;
	unsigned int size;
	int total;  
	int values[];
};
extern struct fifo_buffer *fifo_alloc(unsigned int fifo_size);

 
enum {
	NET_CONGESTED,		 
	RESOLVE_CONFLICTS,	 
	SEND_PING,
	GOT_PING_ACK,		 
	CONN_WD_ST_CHG_REQ,	 
	CONN_WD_ST_CHG_OKAY,
	CONN_WD_ST_CHG_FAIL,
	CONN_DRY_RUN,		 
	CREATE_BARRIER,		 
	STATE_SENT,		 
	CALLBACK_PENDING,	 
	DISCONNECT_SENT,

	DEVICE_WORK_PENDING,	 
};

enum which_state { NOW, OLD = NOW, NEW };

struct drbd_resource {
	char *name;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_res;
	struct dentry *debugfs_res_volumes;
	struct dentry *debugfs_res_connections;
	struct dentry *debugfs_res_in_flight_summary;
#endif
	struct kref kref;
	struct idr devices;		 
	struct list_head connections;
	struct list_head resources;
	struct res_opts res_opts;
	struct mutex conf_update;	 
	struct mutex adm_mutex;		 
	spinlock_t req_lock;

	unsigned susp:1;		 
	unsigned susp_nod:1;		 
	unsigned susp_fen:1;		 

	enum write_ordering_e write_ordering;

	cpumask_var_t cpu_mask;
};

struct drbd_thread_timing_details
{
	unsigned long start_jif;
	void *cb_addr;
	const char *caller_fn;
	unsigned int line;
	unsigned int cb_nr;
};

struct drbd_connection {
	struct list_head connections;
	struct drbd_resource *resource;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_conn;
	struct dentry *debugfs_conn_callback_history;
	struct dentry *debugfs_conn_oldest_requests;
#endif
	struct kref kref;
	struct idr peer_devices;	 
	enum drbd_conns cstate;		 
	struct mutex cstate_mutex;	 
	unsigned int connect_cnt;	 

	unsigned long flags;
	struct net_conf *net_conf;	 
	wait_queue_head_t ping_wait;	 

	struct sockaddr_storage my_addr;
	int my_addr_len;
	struct sockaddr_storage peer_addr;
	int peer_addr_len;

	struct drbd_socket data;	 
	struct drbd_socket meta;	 
	int agreed_pro_version;		 
	u32 agreed_features;
	unsigned long last_received;	 
	unsigned int ko_count;

	struct list_head transfer_log;	 

	struct crypto_shash *cram_hmac_tfm;
	struct crypto_shash *integrity_tfm;   
	struct crypto_shash *peer_integrity_tfm;   
	struct crypto_shash *csums_tfm;
	struct crypto_shash *verify_tfm;
	void *int_dig_in;
	void *int_dig_vv;

	 
	struct drbd_epoch *current_epoch;
	spinlock_t epoch_lock;
	unsigned int epochs;
	atomic_t current_tle_nr;	 
	unsigned current_tle_writes;	 

	unsigned long last_reconnect_jif;
	 
	struct blk_plug receiver_plug;
	struct drbd_thread receiver;
	struct drbd_thread worker;
	struct drbd_thread ack_receiver;
	struct workqueue_struct *ack_sender;

	 
	struct drbd_request *req_next;  
	struct drbd_request *req_ack_pending;
	struct drbd_request *req_not_net_done;

	 
	struct drbd_work_queue sender_work;

#define DRBD_THREAD_DETAILS_HIST	16
	unsigned int w_cb_nr;  
	unsigned int r_cb_nr;  
	struct drbd_thread_timing_details w_timing_details[DRBD_THREAD_DETAILS_HIST];
	struct drbd_thread_timing_details r_timing_details[DRBD_THREAD_DETAILS_HIST];

	struct {
		unsigned long last_sent_barrier_jif;

		 
		bool seen_any_write_yet;

		 
		int current_epoch_nr;

		 
		unsigned current_epoch_writes;
	} send;
};

static inline bool has_net_conf(struct drbd_connection *connection)
{
	bool has_net_conf;

	rcu_read_lock();
	has_net_conf = rcu_dereference(connection->net_conf);
	rcu_read_unlock();

	return has_net_conf;
}

void __update_timing_details(
		struct drbd_thread_timing_details *tdp,
		unsigned int *cb_nr,
		void *cb,
		const char *fn, const unsigned int line);

#define update_worker_timing_details(c, cb) \
	__update_timing_details(c->w_timing_details, &c->w_cb_nr, cb, __func__ , __LINE__ )
#define update_receiver_timing_details(c, cb) \
	__update_timing_details(c->r_timing_details, &c->r_cb_nr, cb, __func__ , __LINE__ )

struct submit_worker {
	struct workqueue_struct *wq;
	struct work_struct worker;

	 
	struct list_head writes;
};

struct drbd_peer_device {
	struct list_head peer_devices;
	struct drbd_device *device;
	struct drbd_connection *connection;
	struct work_struct send_acks_work;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_peer_dev;
#endif
};

struct drbd_device {
	struct drbd_resource *resource;
	struct list_head peer_devices;
	struct list_head pending_bitmap_io;

	unsigned long flush_jif;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_minor;
	struct dentry *debugfs_vol;
	struct dentry *debugfs_vol_oldest_requests;
	struct dentry *debugfs_vol_act_log_extents;
	struct dentry *debugfs_vol_resync_extents;
	struct dentry *debugfs_vol_data_gen_id;
	struct dentry *debugfs_vol_ed_gen_id;
#endif

	unsigned int vnr;	 
	unsigned int minor;	 

	struct kref kref;

	 
	unsigned long flags;

	 
	struct drbd_backing_dev *ldev;

	sector_t p_size;      
	struct request_queue *rq_queue;
	struct gendisk	    *vdisk;

	unsigned long last_reattach_jif;
	struct drbd_work resync_work;
	struct drbd_work unplug_work;
	struct timer_list resync_timer;
	struct timer_list md_sync_timer;
	struct timer_list start_resync_timer;
	struct timer_list request_timer;

	 
	union drbd_state new_state_tmp;

	union drbd_dev_state state;
	wait_queue_head_t misc_wait;
	wait_queue_head_t state_wait;   
	unsigned int send_cnt;
	unsigned int recv_cnt;
	unsigned int read_cnt;
	unsigned int writ_cnt;
	unsigned int al_writ_cnt;
	unsigned int bm_writ_cnt;
	atomic_t ap_bio_cnt;	  
	atomic_t ap_actlog_cnt;   
	atomic_t ap_pending_cnt;  
	atomic_t rs_pending_cnt;  
	atomic_t unacked_cnt;	  
	atomic_t local_cnt;	  
	atomic_t suspend_cnt;

	 
	struct rb_root read_requests;
	struct rb_root write_requests;

	 
	 
	struct list_head pending_master_completion[2];
	struct list_head pending_completion[2];

	 
	bool use_csums;
	 
	unsigned long rs_total;
	 
	unsigned long rs_failed;
	 
	unsigned long rs_start;
	 
	unsigned long rs_paused;
	 
	unsigned long rs_same_csum;
#define DRBD_SYNC_MARKS 8
#define DRBD_SYNC_MARK_STEP (3*HZ)
	 
	unsigned long rs_mark_left[DRBD_SYNC_MARKS];
	 
	unsigned long rs_mark_time[DRBD_SYNC_MARKS];
	 
	int rs_last_mark;
	unsigned long rs_last_bcast;  

	 
	sector_t ov_start_sector;
	sector_t ov_stop_sector;
	 
	sector_t ov_position;
	 
	sector_t ov_last_oos_start;
	 
	sector_t ov_last_oos_size;
	unsigned long ov_left;  

	struct drbd_bitmap *bitmap;
	unsigned long bm_resync_fo;  

	 
	struct lru_cache *resync;
	 
	unsigned int resync_locked;
	 
	unsigned int resync_wenr;

	int open_cnt;
	u64 *p_uuid;

	struct list_head active_ee;  
	struct list_head sync_ee;    
	struct list_head done_ee;    
	struct list_head read_ee;    
	struct list_head net_ee;     

	int next_barrier_nr;
	struct list_head resync_reads;
	atomic_t pp_in_use;		 
	atomic_t pp_in_use_by_net;	 
	wait_queue_head_t ee_wait;
	struct drbd_md_io md_io;
	spinlock_t al_lock;
	wait_queue_head_t al_wait;
	struct lru_cache *act_log;	 
	unsigned int al_tr_number;
	int al_tr_cycle;
	wait_queue_head_t seq_wait;
	atomic_t packet_seq;
	unsigned int peer_seq;
	spinlock_t peer_seq_lock;
	unsigned long comm_bm_set;  
	struct bm_io_work bm_io_work;
	u64 ed_uuid;  
	struct mutex own_state_mutex;
	struct mutex *state_mutex;  
	char congestion_reason;   
	atomic_t rs_sect_in;  
	atomic_t rs_sect_ev;  
	int rs_last_sect_ev;  
	int rs_last_events;   
	int c_sync_rate;  
	struct fifo_buffer *rs_plan_s;  
	int rs_in_flight;  
	atomic_t ap_in_flight;  
	unsigned int peer_max_bio_size;
	unsigned int local_max_bio_size;

	 
	struct submit_worker submit;
};

struct drbd_bm_aio_ctx {
	struct drbd_device *device;
	struct list_head list;  ;
	unsigned long start_jif;
	atomic_t in_flight;
	unsigned int done;
	unsigned flags;
#define BM_AIO_COPY_PAGES	1
#define BM_AIO_WRITE_HINTED	2
#define BM_AIO_WRITE_ALL_PAGES	4
#define BM_AIO_READ		8
	int error;
	struct kref kref;
};

struct drbd_config_context {
	 
	unsigned int minor;
	 
	unsigned int volume;
#define VOLUME_UNSPECIFIED		(-1U)
	 
	char *resource_name;
	struct nlattr *my_addr;
	struct nlattr *peer_addr;

	 
	struct sk_buff *reply_skb;
	 
	struct drbd_genlmsghdr *reply_dh;
	 
	struct drbd_device *device;
	struct drbd_resource *resource;
	struct drbd_connection *connection;
};

static inline struct drbd_device *minor_to_device(unsigned int minor)
{
	return (struct drbd_device *)idr_find(&drbd_devices, minor);
}

static inline struct drbd_peer_device *first_peer_device(struct drbd_device *device)
{
	return list_first_entry_or_null(&device->peer_devices, struct drbd_peer_device, peer_devices);
}

static inline struct drbd_peer_device *
conn_peer_device(struct drbd_connection *connection, int volume_number)
{
	return idr_find(&connection->peer_devices, volume_number);
}

#define for_each_resource(resource, _resources) \
	list_for_each_entry(resource, _resources, resources)

#define for_each_resource_rcu(resource, _resources) \
	list_for_each_entry_rcu(resource, _resources, resources)

#define for_each_resource_safe(resource, tmp, _resources) \
	list_for_each_entry_safe(resource, tmp, _resources, resources)

#define for_each_connection(connection, resource) \
	list_for_each_entry(connection, &resource->connections, connections)

#define for_each_connection_rcu(connection, resource) \
	list_for_each_entry_rcu(connection, &resource->connections, connections)

#define for_each_connection_safe(connection, tmp, resource) \
	list_for_each_entry_safe(connection, tmp, &resource->connections, connections)

#define for_each_peer_device(peer_device, device) \
	list_for_each_entry(peer_device, &device->peer_devices, peer_devices)

#define for_each_peer_device_rcu(peer_device, device) \
	list_for_each_entry_rcu(peer_device, &device->peer_devices, peer_devices)

#define for_each_peer_device_safe(peer_device, tmp, device) \
	list_for_each_entry_safe(peer_device, tmp, &device->peer_devices, peer_devices)

static inline unsigned int device_to_minor(struct drbd_device *device)
{
	return device->minor;
}

 

 

enum dds_flags {
	DDSF_FORCED    = 1,
	DDSF_NO_RESYNC = 2,  
};

extern void drbd_init_set_defaults(struct drbd_device *device);
extern int  drbd_thread_start(struct drbd_thread *thi);
extern void _drbd_thread_stop(struct drbd_thread *thi, int restart, int wait);
#ifdef CONFIG_SMP
extern void drbd_thread_current_set_cpu(struct drbd_thread *thi);
#else
#define drbd_thread_current_set_cpu(A) ({})
#endif
extern void tl_release(struct drbd_connection *, unsigned int barrier_nr,
		       unsigned int set_size);
extern void tl_clear(struct drbd_connection *);
extern void drbd_free_sock(struct drbd_connection *connection);
extern int drbd_send(struct drbd_connection *connection, struct socket *sock,
		     void *buf, size_t size, unsigned msg_flags);
extern int drbd_send_all(struct drbd_connection *, struct socket *, void *, size_t,
			 unsigned);

extern int __drbd_send_protocol(struct drbd_connection *connection, enum drbd_packet cmd);
extern int drbd_send_protocol(struct drbd_connection *connection);
extern int drbd_send_uuids(struct drbd_peer_device *);
extern int drbd_send_uuids_skip_initial_sync(struct drbd_peer_device *);
extern void drbd_gen_and_send_sync_uuid(struct drbd_peer_device *);
extern int drbd_send_sizes(struct drbd_peer_device *, int trigger_reply, enum dds_flags flags);
extern int drbd_send_state(struct drbd_peer_device *, union drbd_state s);
extern int drbd_send_current_state(struct drbd_peer_device *);
extern int drbd_send_sync_param(struct drbd_peer_device *);
extern void drbd_send_b_ack(struct drbd_connection *connection, u32 barrier_nr,
			    u32 set_size);
extern int drbd_send_ack(struct drbd_peer_device *, enum drbd_packet,
			 struct drbd_peer_request *);
extern void drbd_send_ack_rp(struct drbd_peer_device *, enum drbd_packet,
			     struct p_block_req *rp);
extern void drbd_send_ack_dp(struct drbd_peer_device *, enum drbd_packet,
			     struct p_data *dp, int data_size);
extern int drbd_send_ack_ex(struct drbd_peer_device *, enum drbd_packet,
			    sector_t sector, int blksize, u64 block_id);
extern int drbd_send_out_of_sync(struct drbd_peer_device *, struct drbd_request *);
extern int drbd_send_block(struct drbd_peer_device *, enum drbd_packet,
			   struct drbd_peer_request *);
extern int drbd_send_dblock(struct drbd_peer_device *, struct drbd_request *req);
extern int drbd_send_drequest(struct drbd_peer_device *, int cmd,
			      sector_t sector, int size, u64 block_id);
extern int drbd_send_drequest_csum(struct drbd_peer_device *, sector_t sector,
				   int size, void *digest, int digest_size,
				   enum drbd_packet cmd);
extern int drbd_send_ov_request(struct drbd_peer_device *, sector_t sector, int size);

extern int drbd_send_bitmap(struct drbd_device *device, struct drbd_peer_device *peer_device);
extern void drbd_send_sr_reply(struct drbd_peer_device *, enum drbd_state_rv retcode);
extern void conn_send_sr_reply(struct drbd_connection *connection, enum drbd_state_rv retcode);
extern int drbd_send_rs_deallocated(struct drbd_peer_device *, struct drbd_peer_request *);
extern void drbd_backing_dev_free(struct drbd_device *device, struct drbd_backing_dev *ldev);
extern void drbd_device_cleanup(struct drbd_device *device);
extern void drbd_print_uuids(struct drbd_device *device, const char *text);
extern void drbd_queue_unplug(struct drbd_device *device);

extern void conn_md_sync(struct drbd_connection *connection);
extern void drbd_md_write(struct drbd_device *device, void *buffer);
extern void drbd_md_sync(struct drbd_device *device);
extern int  drbd_md_read(struct drbd_device *device, struct drbd_backing_dev *bdev);
extern void drbd_uuid_set(struct drbd_device *device, int idx, u64 val) __must_hold(local);
extern void _drbd_uuid_set(struct drbd_device *device, int idx, u64 val) __must_hold(local);
extern void drbd_uuid_new_current(struct drbd_device *device) __must_hold(local);
extern void drbd_uuid_set_bm(struct drbd_device *device, u64 val) __must_hold(local);
extern void drbd_uuid_move_history(struct drbd_device *device) __must_hold(local);
extern void __drbd_uuid_set(struct drbd_device *device, int idx, u64 val) __must_hold(local);
extern void drbd_md_set_flag(struct drbd_device *device, int flags) __must_hold(local);
extern void drbd_md_clear_flag(struct drbd_device *device, int flags)__must_hold(local);
extern int drbd_md_test_flag(struct drbd_backing_dev *, int);
extern void drbd_md_mark_dirty(struct drbd_device *device);
extern void drbd_queue_bitmap_io(struct drbd_device *device,
				 int (*io_fn)(struct drbd_device *, struct drbd_peer_device *),
				 void (*done)(struct drbd_device *, int),
				 char *why, enum bm_flag flags,
				 struct drbd_peer_device *peer_device);
extern int drbd_bitmap_io(struct drbd_device *device,
		int (*io_fn)(struct drbd_device *, struct drbd_peer_device *),
		char *why, enum bm_flag flags,
		struct drbd_peer_device *peer_device);
extern int drbd_bitmap_io_from_worker(struct drbd_device *device,
		int (*io_fn)(struct drbd_device *, struct drbd_peer_device *),
		char *why, enum bm_flag flags,
		struct drbd_peer_device *peer_device);
extern int drbd_bmio_set_n_write(struct drbd_device *device,
		struct drbd_peer_device *peer_device) __must_hold(local);
extern int drbd_bmio_clear_n_write(struct drbd_device *device,
		struct drbd_peer_device *peer_device) __must_hold(local);

 

 
#define MD_128MB_SECT (128LLU << 11)   
#define MD_4kB_SECT	 8
#define MD_32kB_SECT	64

 
#define AL_EXTENT_SHIFT 22
#define AL_EXTENT_SIZE (1<<AL_EXTENT_SHIFT)

 
#define AL_UPDATES_PER_TRANSACTION	 64	
#define AL_CONTEXT_PER_TRANSACTION	919	

#if BITS_PER_LONG == 32
#define LN2_BPL 5
#define cpu_to_lel(A) cpu_to_le32(A)
#define lel_to_cpu(A) le32_to_cpu(A)
#elif BITS_PER_LONG == 64
#define LN2_BPL 6
#define cpu_to_lel(A) cpu_to_le64(A)
#define lel_to_cpu(A) le64_to_cpu(A)
#else
#error "LN2 of BITS_PER_LONG unknown!"
#endif

 
 
struct bm_extent {
	int rs_left;  
	int rs_failed;  
	unsigned long flags;
	struct lc_element lce;
};

#define BME_NO_WRITES  0   
#define BME_LOCKED     1   
#define BME_PRIORITY   2   

 
 

#define SLEEP_TIME (HZ/10)

 
#define BM_BLOCK_SHIFT	12			  
#define BM_BLOCK_SIZE	 (1<<BM_BLOCK_SHIFT)
 
#define BM_EXT_SHIFT	 24	 
#define BM_EXT_SIZE	 (1<<BM_EXT_SHIFT)

#if (BM_EXT_SHIFT != 24) || (BM_BLOCK_SHIFT != 12)
#error "HAVE YOU FIXED drbdmeta AS WELL??"
#endif

 
#define BM_SECT_TO_BIT(x)   ((x)>>(BM_BLOCK_SHIFT-9))
#define BM_BIT_TO_SECT(x)   ((sector_t)(x)<<(BM_BLOCK_SHIFT-9))
#define BM_SECT_PER_BIT     BM_BIT_TO_SECT(1)

 
#define Bit2KB(bits) ((bits)<<(BM_BLOCK_SHIFT-10))

 
#define BM_SECT_TO_EXT(x)   ((x)>>(BM_EXT_SHIFT-9))
#define BM_BIT_TO_EXT(x)    ((x) >> (BM_EXT_SHIFT - BM_BLOCK_SHIFT))

 
#define BM_EXT_TO_SECT(x)   ((sector_t)(x) << (BM_EXT_SHIFT-9))
 
#define BM_SECT_PER_EXT     BM_EXT_TO_SECT(1)
 
#define BM_BITS_PER_EXT     (1UL << (BM_EXT_SHIFT - BM_BLOCK_SHIFT))

#define BM_BLOCKS_PER_BM_EXT_MASK  (BM_BITS_PER_EXT - 1)


 
#define AL_EXT_PER_BM_SECT  (1 << (BM_EXT_SHIFT - AL_EXTENT_SHIFT))

 

#define DRBD_MAX_SECTORS_32 (0xffffffffLU)
 

#define DRBD_MAX_SECTORS_FIXED_BM \
	  ((MD_128MB_SECT - MD_32kB_SECT - MD_4kB_SECT) * (1LL<<(BM_EXT_SHIFT-9)))
#define DRBD_MAX_SECTORS      DRBD_MAX_SECTORS_FIXED_BM
 
#if BITS_PER_LONG == 32
 
#define DRBD_MAX_SECTORS_FLEX BM_BIT_TO_SECT(0xffff7fff)
#else
 
#define DRBD_MAX_SECTORS_FLEX (1UL << 51)
 
#endif

 
#define DRBD_MAX_BIO_SIZE (1U << 20)
#if DRBD_MAX_BIO_SIZE > (BIO_MAX_VECS << PAGE_SHIFT)
#error Architecture not supported: DRBD_MAX_BIO_SIZE > BIO_MAX_SIZE
#endif
#define DRBD_MAX_BIO_SIZE_SAFE (1U << 12)        

#define DRBD_MAX_SIZE_H80_PACKET (1U << 15)  
#define DRBD_MAX_BIO_SIZE_P95    (1U << 17)  

 
#define DRBD_MAX_BATCH_BIO_SIZE	 (AL_UPDATES_PER_TRANSACTION/2*AL_EXTENT_SIZE)
#define DRBD_MAX_BBIO_SECTORS    (DRBD_MAX_BATCH_BIO_SIZE >> 9)

extern int  drbd_bm_init(struct drbd_device *device);
extern int  drbd_bm_resize(struct drbd_device *device, sector_t sectors, int set_new_bits);
extern void drbd_bm_cleanup(struct drbd_device *device);
extern void drbd_bm_set_all(struct drbd_device *device);
extern void drbd_bm_clear_all(struct drbd_device *device);
 
extern int  drbd_bm_set_bits(
		struct drbd_device *device, unsigned long s, unsigned long e);
extern int  drbd_bm_clear_bits(
		struct drbd_device *device, unsigned long s, unsigned long e);
extern int drbd_bm_count_bits(
	struct drbd_device *device, const unsigned long s, const unsigned long e);
 
extern void _drbd_bm_set_bits(struct drbd_device *device,
		const unsigned long s, const unsigned long e);
extern int  drbd_bm_test_bit(struct drbd_device *device, unsigned long bitnr);
extern int  drbd_bm_e_weight(struct drbd_device *device, unsigned long enr);
extern int  drbd_bm_read(struct drbd_device *device,
		struct drbd_peer_device *peer_device) __must_hold(local);
extern void drbd_bm_mark_for_writeout(struct drbd_device *device, int page_nr);
extern int  drbd_bm_write(struct drbd_device *device,
		struct drbd_peer_device *peer_device) __must_hold(local);
extern void drbd_bm_reset_al_hints(struct drbd_device *device) __must_hold(local);
extern int  drbd_bm_write_hinted(struct drbd_device *device) __must_hold(local);
extern int  drbd_bm_write_lazy(struct drbd_device *device, unsigned upper_idx) __must_hold(local);
extern int drbd_bm_write_all(struct drbd_device *device,
		struct drbd_peer_device *peer_device) __must_hold(local);
extern int  drbd_bm_write_copy_pages(struct drbd_device *device,
		struct drbd_peer_device *peer_device) __must_hold(local);
extern size_t	     drbd_bm_words(struct drbd_device *device);
extern unsigned long drbd_bm_bits(struct drbd_device *device);
extern sector_t      drbd_bm_capacity(struct drbd_device *device);

#define DRBD_END_OF_BITMAP	(~(unsigned long)0)
extern unsigned long drbd_bm_find_next(struct drbd_device *device, unsigned long bm_fo);
 
extern unsigned long _drbd_bm_find_next(struct drbd_device *device, unsigned long bm_fo);
extern unsigned long _drbd_bm_find_next_zero(struct drbd_device *device, unsigned long bm_fo);
extern unsigned long _drbd_bm_total_weight(struct drbd_device *device);
extern unsigned long drbd_bm_total_weight(struct drbd_device *device);
 
extern void drbd_bm_merge_lel(struct drbd_device *device, size_t offset,
		size_t number, unsigned long *buffer);
 
extern void drbd_bm_get_lel(struct drbd_device *device, size_t offset,
		size_t number, unsigned long *buffer);

extern void drbd_bm_lock(struct drbd_device *device, char *why, enum bm_flag flags);
extern void drbd_bm_unlock(struct drbd_device *device);
 

extern struct kmem_cache *drbd_request_cache;
extern struct kmem_cache *drbd_ee_cache;	 
extern struct kmem_cache *drbd_bm_ext_cache;	 
extern struct kmem_cache *drbd_al_ext_cache;	 
extern mempool_t drbd_request_mempool;
extern mempool_t drbd_ee_mempool;

 
extern struct page *drbd_pp_pool;
extern spinlock_t   drbd_pp_lock;
extern int	    drbd_pp_vacant;
extern wait_queue_head_t drbd_pp_wait;

 
#define DRBD_MIN_POOL_PAGES	128
extern mempool_t drbd_md_io_page_pool;

 
extern struct bio_set drbd_md_io_bio_set;

 
extern struct bio_set drbd_io_bio_set;

extern struct mutex resources_mutex;

extern int conn_lowest_minor(struct drbd_connection *connection);
extern enum drbd_ret_code drbd_create_device(struct drbd_config_context *adm_ctx, unsigned int minor);
extern void drbd_destroy_device(struct kref *kref);
extern void drbd_delete_device(struct drbd_device *device);

extern struct drbd_resource *drbd_create_resource(const char *name);
extern void drbd_free_resource(struct drbd_resource *resource);

extern int set_resource_options(struct drbd_resource *resource, struct res_opts *res_opts);
extern struct drbd_connection *conn_create(const char *name, struct res_opts *res_opts);
extern void drbd_destroy_connection(struct kref *kref);
extern struct drbd_connection *conn_get_by_addrs(void *my_addr, int my_addr_len,
					    void *peer_addr, int peer_addr_len);
extern struct drbd_resource *drbd_find_resource(const char *name);
extern void drbd_destroy_resource(struct kref *kref);
extern void conn_free_crypto(struct drbd_connection *connection);

 
extern void do_submit(struct work_struct *ws);
extern void __drbd_make_request(struct drbd_device *, struct bio *);
void drbd_submit_bio(struct bio *bio);
extern int drbd_read_remote(struct drbd_device *device, struct drbd_request *req);
extern int is_valid_ar_handle(struct drbd_request *, sector_t);


 

extern struct mutex notification_mutex;

extern void drbd_suspend_io(struct drbd_device *device);
extern void drbd_resume_io(struct drbd_device *device);
extern char *ppsize(char *buf, unsigned long long size);
extern sector_t drbd_new_dev_size(struct drbd_device *, struct drbd_backing_dev *, sector_t, int);
enum determine_dev_size {
	DS_ERROR_SHRINK = -3,
	DS_ERROR_SPACE_MD = -2,
	DS_ERROR = -1,
	DS_UNCHANGED = 0,
	DS_SHRUNK = 1,
	DS_GREW = 2,
	DS_GREW_FROM_ZERO = 3,
};
extern enum determine_dev_size
drbd_determine_dev_size(struct drbd_device *, enum dds_flags, struct resize_parms *) __must_hold(local);
extern void resync_after_online_grow(struct drbd_device *);
extern void drbd_reconsider_queue_parameters(struct drbd_device *device,
			struct drbd_backing_dev *bdev, struct o_qlim *o);
extern enum drbd_state_rv drbd_set_role(struct drbd_device *device,
					enum drbd_role new_role,
					int force);
extern bool conn_try_outdate_peer(struct drbd_connection *connection);
extern void conn_try_outdate_peer_async(struct drbd_connection *connection);
extern enum drbd_peer_state conn_khelper(struct drbd_connection *connection, char *cmd);
extern int drbd_khelper(struct drbd_device *device, char *cmd);

 
 
extern void drbd_md_endio(struct bio *bio);
extern void drbd_peer_request_endio(struct bio *bio);
extern void drbd_request_endio(struct bio *bio);
extern int drbd_worker(struct drbd_thread *thi);
enum drbd_ret_code drbd_resync_after_valid(struct drbd_device *device, int o_minor);
void drbd_resync_after_changed(struct drbd_device *device);
extern void drbd_start_resync(struct drbd_device *device, enum drbd_conns side);
extern void resume_next_sg(struct drbd_device *device);
extern void suspend_other_sg(struct drbd_device *device);
extern int drbd_resync_finished(struct drbd_peer_device *peer_device);
 
extern void *drbd_md_get_buffer(struct drbd_device *device, const char *intent);
extern void drbd_md_put_buffer(struct drbd_device *device);
extern int drbd_md_sync_page_io(struct drbd_device *device,
		struct drbd_backing_dev *bdev, sector_t sector, enum req_op op);
extern void drbd_ov_out_of_sync_found(struct drbd_peer_device *peer_device,
		sector_t sector, int size);
extern void wait_until_done_or_force_detached(struct drbd_device *device,
		struct drbd_backing_dev *bdev, unsigned int *done);
extern void drbd_rs_controller_reset(struct drbd_peer_device *peer_device);

static inline void ov_out_of_sync_print(struct drbd_peer_device *peer_device)
{
	struct drbd_device *device = peer_device->device;

	if (device->ov_last_oos_size) {
		drbd_err(peer_device, "Out of sync: start=%llu, size=%lu (sectors)\n",
		     (unsigned long long)device->ov_last_oos_start,
		     (unsigned long)device->ov_last_oos_size);
	}
	device->ov_last_oos_size = 0;
}


extern void drbd_csum_bio(struct crypto_shash *, struct bio *, void *);
extern void drbd_csum_ee(struct crypto_shash *, struct drbd_peer_request *,
			 void *);
 
extern int w_e_end_data_req(struct drbd_work *, int);
extern int w_e_end_rsdata_req(struct drbd_work *, int);
extern int w_e_end_csum_rs_req(struct drbd_work *, int);
extern int w_e_end_ov_reply(struct drbd_work *, int);
extern int w_e_end_ov_req(struct drbd_work *, int);
extern int w_ov_finished(struct drbd_work *, int);
extern int w_resync_timer(struct drbd_work *, int);
extern int w_send_write_hint(struct drbd_work *, int);
extern int w_send_dblock(struct drbd_work *, int);
extern int w_send_read_req(struct drbd_work *, int);
extern int w_e_reissue(struct drbd_work *, int);
extern int w_restart_disk_io(struct drbd_work *, int);
extern int w_send_out_of_sync(struct drbd_work *, int);

extern void resync_timer_fn(struct timer_list *t);
extern void start_resync_timer_fn(struct timer_list *t);

extern void drbd_endio_write_sec_final(struct drbd_peer_request *peer_req);

 
extern int drbd_issue_discard_or_zero_out(struct drbd_device *device,
		sector_t start, unsigned int nr_sectors, int flags);
extern int drbd_receiver(struct drbd_thread *thi);
extern int drbd_ack_receiver(struct drbd_thread *thi);
extern void drbd_send_ping_wf(struct work_struct *ws);
extern void drbd_send_acks_wf(struct work_struct *ws);
extern bool drbd_rs_c_min_rate_throttle(struct drbd_device *device);
extern bool drbd_rs_should_slow_down(struct drbd_peer_device *peer_device, sector_t sector,
		bool throttle_if_app_is_waiting);
extern int drbd_submit_peer_request(struct drbd_peer_request *peer_req);
extern int drbd_free_peer_reqs(struct drbd_device *, struct list_head *);
extern struct drbd_peer_request *drbd_alloc_peer_req(struct drbd_peer_device *, u64,
						     sector_t, unsigned int,
						     unsigned int,
						     gfp_t) __must_hold(local);
extern void __drbd_free_peer_req(struct drbd_device *, struct drbd_peer_request *,
				 int);
#define drbd_free_peer_req(m,e) __drbd_free_peer_req(m, e, 0)
#define drbd_free_net_peer_req(m,e) __drbd_free_peer_req(m, e, 1)
extern struct page *drbd_alloc_pages(struct drbd_peer_device *, unsigned int, bool);
extern void drbd_set_recv_tcq(struct drbd_device *device, int tcq_enabled);
extern void _drbd_clear_done_ee(struct drbd_device *device, struct list_head *to_be_freed);
extern int drbd_connected(struct drbd_peer_device *);

 
void drbd_set_my_capacity(struct drbd_device *device, sector_t size);

 
static inline void drbd_submit_bio_noacct(struct drbd_device *device,
					     int fault_type, struct bio *bio)
{
	__release(local);
	if (!bio->bi_bdev) {
		drbd_err(device, "drbd_submit_bio_noacct: bio->bi_bdev == NULL\n");
		bio->bi_status = BLK_STS_IOERR;
		bio_endio(bio);
		return;
	}

	if (drbd_insert_fault(device, fault_type))
		bio_io_error(bio);
	else
		submit_bio_noacct(bio);
}

void drbd_bump_write_ordering(struct drbd_resource *resource, struct drbd_backing_dev *bdev,
			      enum write_ordering_e wo);

 
extern struct proc_dir_entry *drbd_proc;
int drbd_seq_show(struct seq_file *seq, void *v);

 
extern bool drbd_al_begin_io_prepare(struct drbd_device *device, struct drbd_interval *i);
extern int drbd_al_begin_io_nonblock(struct drbd_device *device, struct drbd_interval *i);
extern void drbd_al_begin_io_commit(struct drbd_device *device);
extern bool drbd_al_begin_io_fastpath(struct drbd_device *device, struct drbd_interval *i);
extern void drbd_al_begin_io(struct drbd_device *device, struct drbd_interval *i);
extern void drbd_al_complete_io(struct drbd_device *device, struct drbd_interval *i);
extern void drbd_rs_complete_io(struct drbd_device *device, sector_t sector);
extern int drbd_rs_begin_io(struct drbd_device *device, sector_t sector);
extern int drbd_try_rs_begin_io(struct drbd_peer_device *peer_device, sector_t sector);
extern void drbd_rs_cancel_all(struct drbd_device *device);
extern int drbd_rs_del_all(struct drbd_device *device);
extern void drbd_rs_failed_io(struct drbd_peer_device *peer_device,
		sector_t sector, int size);
extern void drbd_advance_rs_marks(struct drbd_peer_device *peer_device, unsigned long still_to_go);

enum update_sync_bits_mode { RECORD_RS_FAILED, SET_OUT_OF_SYNC, SET_IN_SYNC };
extern int __drbd_change_sync(struct drbd_peer_device *peer_device, sector_t sector, int size,
		enum update_sync_bits_mode mode);
#define drbd_set_in_sync(peer_device, sector, size) \
	__drbd_change_sync(peer_device, sector, size, SET_IN_SYNC)
#define drbd_set_out_of_sync(peer_device, sector, size) \
	__drbd_change_sync(peer_device, sector, size, SET_OUT_OF_SYNC)
#define drbd_rs_failed_io(peer_device, sector, size) \
	__drbd_change_sync(peer_device, sector, size, RECORD_RS_FAILED)
extern void drbd_al_shrink(struct drbd_device *device);
extern int drbd_al_initialize(struct drbd_device *, void *);

 
 
struct sib_info {
	enum drbd_state_info_bcast_reason sib_reason;
	union {
		struct {
			char *helper_name;
			unsigned helper_exit_code;
		};
		struct {
			union drbd_state os;
			union drbd_state ns;
		};
	};
};
void drbd_bcast_event(struct drbd_device *device, const struct sib_info *sib);

extern int notify_resource_state(struct sk_buff *,
				  unsigned int,
				  struct drbd_resource *,
				  struct resource_info *,
				  enum drbd_notification_type);
extern int notify_device_state(struct sk_buff *,
				unsigned int,
				struct drbd_device *,
				struct device_info *,
				enum drbd_notification_type);
extern int notify_connection_state(struct sk_buff *,
				    unsigned int,
				    struct drbd_connection *,
				    struct connection_info *,
				    enum drbd_notification_type);
extern int notify_peer_device_state(struct sk_buff *,
				     unsigned int,
				     struct drbd_peer_device *,
				     struct peer_device_info *,
				     enum drbd_notification_type);
extern void notify_helper(enum drbd_notification_type, struct drbd_device *,
			  struct drbd_connection *, const char *, int);

 

 
static inline struct page *page_chain_next(struct page *page)
{
	return (struct page *)page_private(page);
}
#define page_chain_for_each(page) \
	for (; page && ({ prefetch(page_chain_next(page)); 1; }); \
			page = page_chain_next(page))
#define page_chain_for_each_safe(page, n) \
	for (; page && ({ n = page_chain_next(page); 1; }); page = n)


static inline int drbd_peer_req_has_active_page(struct drbd_peer_request *peer_req)
{
	struct page *page = peer_req->pages;
	page_chain_for_each(page) {
		if (page_count(page) > 1)
			return 1;
	}
	return 0;
}

static inline union drbd_state drbd_read_state(struct drbd_device *device)
{
	struct drbd_resource *resource = device->resource;
	union drbd_state rv;

	rv.i = device->state.i;
	rv.susp = resource->susp;
	rv.susp_nod = resource->susp_nod;
	rv.susp_fen = resource->susp_fen;

	return rv;
}

enum drbd_force_detach_flags {
	DRBD_READ_ERROR,
	DRBD_WRITE_ERROR,
	DRBD_META_IO_ERROR,
	DRBD_FORCE_DETACH,
};

#define __drbd_chk_io_error(m,f) __drbd_chk_io_error_(m,f, __func__)
static inline void __drbd_chk_io_error_(struct drbd_device *device,
		enum drbd_force_detach_flags df,
		const char *where)
{
	enum drbd_io_error_p ep;

	rcu_read_lock();
	ep = rcu_dereference(device->ldev->disk_conf)->on_io_error;
	rcu_read_unlock();
	switch (ep) {
	case EP_PASS_ON:  
		if (df == DRBD_READ_ERROR || df == DRBD_WRITE_ERROR) {
			if (drbd_ratelimit())
				drbd_err(device, "Local IO failed in %s.\n", where);
			if (device->state.disk > D_INCONSISTENT)
				_drbd_set_state(_NS(device, disk, D_INCONSISTENT), CS_HARD, NULL);
			break;
		}
		fallthrough;	 
	case EP_DETACH:
	case EP_CALL_HELPER:
		 
		set_bit(WAS_IO_ERROR, &device->flags);
		if (df == DRBD_READ_ERROR)
			set_bit(WAS_READ_ERROR, &device->flags);
		if (df == DRBD_FORCE_DETACH)
			set_bit(FORCE_DETACH, &device->flags);
		if (device->state.disk > D_FAILED) {
			_drbd_set_state(_NS(device, disk, D_FAILED), CS_HARD, NULL);
			drbd_err(device,
				"Local IO failed in %s. Detaching...\n", where);
		}
		break;
	}
}

 
#define drbd_chk_io_error(m,e,f) drbd_chk_io_error_(m,e,f, __func__)
static inline void drbd_chk_io_error_(struct drbd_device *device,
	int error, enum drbd_force_detach_flags forcedetach, const char *where)
{
	if (error) {
		unsigned long flags;
		spin_lock_irqsave(&device->resource->req_lock, flags);
		__drbd_chk_io_error_(device, forcedetach, where);
		spin_unlock_irqrestore(&device->resource->req_lock, flags);
	}
}


 
static inline sector_t drbd_md_first_sector(struct drbd_backing_dev *bdev)
{
	switch (bdev->md.meta_dev_idx) {
	case DRBD_MD_INDEX_INTERNAL:
	case DRBD_MD_INDEX_FLEX_INT:
		return bdev->md.md_offset + bdev->md.bm_offset;
	case DRBD_MD_INDEX_FLEX_EXT:
	default:
		return bdev->md.md_offset;
	}
}

 
static inline sector_t drbd_md_last_sector(struct drbd_backing_dev *bdev)
{
	switch (bdev->md.meta_dev_idx) {
	case DRBD_MD_INDEX_INTERNAL:
	case DRBD_MD_INDEX_FLEX_INT:
		return bdev->md.md_offset + MD_4kB_SECT -1;
	case DRBD_MD_INDEX_FLEX_EXT:
	default:
		return bdev->md.md_offset + bdev->md.md_size_sect -1;
	}
}

 
static inline sector_t drbd_get_capacity(struct block_device *bdev)
{
	return bdev ? bdev_nr_sectors(bdev) : 0;
}

 
static inline sector_t drbd_get_max_capacity(struct drbd_backing_dev *bdev)
{
	sector_t s;

	switch (bdev->md.meta_dev_idx) {
	case DRBD_MD_INDEX_INTERNAL:
	case DRBD_MD_INDEX_FLEX_INT:
		s = drbd_get_capacity(bdev->backing_bdev)
			? min_t(sector_t, DRBD_MAX_SECTORS_FLEX,
				drbd_md_first_sector(bdev))
			: 0;
		break;
	case DRBD_MD_INDEX_FLEX_EXT:
		s = min_t(sector_t, DRBD_MAX_SECTORS_FLEX,
				drbd_get_capacity(bdev->backing_bdev));
		 
		s = min_t(sector_t, s,
			BM_EXT_TO_SECT(bdev->md.md_size_sect
				     - bdev->md.bm_offset));
		break;
	default:
		s = min_t(sector_t, DRBD_MAX_SECTORS,
				drbd_get_capacity(bdev->backing_bdev));
	}
	return s;
}

 
static inline sector_t drbd_md_ss(struct drbd_backing_dev *bdev)
{
	const int meta_dev_idx = bdev->md.meta_dev_idx;

	if (meta_dev_idx == DRBD_MD_INDEX_FLEX_EXT)
		return 0;

	 
	if (meta_dev_idx == DRBD_MD_INDEX_INTERNAL ||
	    meta_dev_idx == DRBD_MD_INDEX_FLEX_INT)
		return (drbd_get_capacity(bdev->backing_bdev) & ~7ULL) - 8;

	 
	return MD_128MB_SECT * bdev->md.meta_dev_idx;
}

static inline void
drbd_queue_work(struct drbd_work_queue *q, struct drbd_work *w)
{
	unsigned long flags;
	spin_lock_irqsave(&q->q_lock, flags);
	list_add_tail(&w->list, &q->q);
	spin_unlock_irqrestore(&q->q_lock, flags);
	wake_up(&q->q_wait);
}

static inline void
drbd_queue_work_if_unqueued(struct drbd_work_queue *q, struct drbd_work *w)
{
	unsigned long flags;
	spin_lock_irqsave(&q->q_lock, flags);
	if (list_empty_careful(&w->list))
		list_add_tail(&w->list, &q->q);
	spin_unlock_irqrestore(&q->q_lock, flags);
	wake_up(&q->q_wait);
}

static inline void
drbd_device_post_work(struct drbd_device *device, int work_bit)
{
	if (!test_and_set_bit(work_bit, &device->flags)) {
		struct drbd_connection *connection =
			first_peer_device(device)->connection;
		struct drbd_work_queue *q = &connection->sender_work;
		if (!test_and_set_bit(DEVICE_WORK_PENDING, &connection->flags))
			wake_up(&q->q_wait);
	}
}

extern void drbd_flush_workqueue(struct drbd_work_queue *work_queue);

 
static inline void wake_ack_receiver(struct drbd_connection *connection)
{
	struct task_struct *task = connection->ack_receiver.task;
	if (task && get_t_state(&connection->ack_receiver) == RUNNING)
		send_sig(SIGXCPU, task, 1);
}

static inline void request_ping(struct drbd_connection *connection)
{
	set_bit(SEND_PING, &connection->flags);
	wake_ack_receiver(connection);
}

extern void *conn_prepare_command(struct drbd_connection *, struct drbd_socket *);
extern void *drbd_prepare_command(struct drbd_peer_device *, struct drbd_socket *);
extern int conn_send_command(struct drbd_connection *, struct drbd_socket *,
			     enum drbd_packet, unsigned int, void *,
			     unsigned int);
extern int drbd_send_command(struct drbd_peer_device *, struct drbd_socket *,
			     enum drbd_packet, unsigned int, void *,
			     unsigned int);

extern int drbd_send_ping(struct drbd_connection *connection);
extern int drbd_send_ping_ack(struct drbd_connection *connection);
extern int drbd_send_state_req(struct drbd_peer_device *, union drbd_state, union drbd_state);
extern int conn_send_state_req(struct drbd_connection *, union drbd_state, union drbd_state);

static inline void drbd_thread_stop(struct drbd_thread *thi)
{
	_drbd_thread_stop(thi, false, true);
}

static inline void drbd_thread_stop_nowait(struct drbd_thread *thi)
{
	_drbd_thread_stop(thi, false, false);
}

static inline void drbd_thread_restart_nowait(struct drbd_thread *thi)
{
	_drbd_thread_stop(thi, true, false);
}

 
static inline void inc_ap_pending(struct drbd_device *device)
{
	atomic_inc(&device->ap_pending_cnt);
}

#define dec_ap_pending(device) ((void)expect((device), __dec_ap_pending(device) >= 0))
static inline int __dec_ap_pending(struct drbd_device *device)
{
	int ap_pending_cnt = atomic_dec_return(&device->ap_pending_cnt);

	if (ap_pending_cnt == 0)
		wake_up(&device->misc_wait);
	return ap_pending_cnt;
}

 
static inline void inc_rs_pending(struct drbd_peer_device *peer_device)
{
	atomic_inc(&peer_device->device->rs_pending_cnt);
}

#define dec_rs_pending(peer_device) \
	((void)expect((peer_device), __dec_rs_pending(peer_device) >= 0))
static inline int __dec_rs_pending(struct drbd_peer_device *peer_device)
{
	return atomic_dec_return(&peer_device->device->rs_pending_cnt);
}

 
static inline void inc_unacked(struct drbd_device *device)
{
	atomic_inc(&device->unacked_cnt);
}

#define dec_unacked(device) ((void)expect(device, __dec_unacked(device) >= 0))
static inline int __dec_unacked(struct drbd_device *device)
{
	return atomic_dec_return(&device->unacked_cnt);
}

#define sub_unacked(device, n) ((void)expect(device, __sub_unacked(device) >= 0))
static inline int __sub_unacked(struct drbd_device *device, int n)
{
	return atomic_sub_return(n, &device->unacked_cnt);
}

static inline bool is_sync_target_state(enum drbd_conns connection_state)
{
	return	connection_state == C_SYNC_TARGET ||
		connection_state == C_PAUSED_SYNC_T;
}

static inline bool is_sync_source_state(enum drbd_conns connection_state)
{
	return	connection_state == C_SYNC_SOURCE ||
		connection_state == C_PAUSED_SYNC_S;
}

static inline bool is_sync_state(enum drbd_conns connection_state)
{
	return	is_sync_source_state(connection_state) ||
		is_sync_target_state(connection_state);
}

 
#define get_ldev_if_state(_device, _min_state)				\
	(_get_ldev_if_state((_device), (_min_state)) ?			\
	 ({ __acquire(x); true; }) : false)
#define get_ldev(_device) get_ldev_if_state(_device, D_INCONSISTENT)

static inline void put_ldev(struct drbd_device *device)
{
	enum drbd_disk_state disk_state = device->state.disk;
	 
	int i = atomic_dec_return(&device->local_cnt);

	 

	__release(local);
	D_ASSERT(device, i >= 0);
	if (i == 0) {
		if (disk_state == D_DISKLESS)
			 
			drbd_device_post_work(device, DESTROY_DISK);
		if (disk_state == D_FAILED)
			 
			if (!test_and_set_bit(GOING_DISKLESS, &device->flags))
				drbd_device_post_work(device, GO_DISKLESS);
		wake_up(&device->misc_wait);
	}
}

#ifndef __CHECKER__
static inline int _get_ldev_if_state(struct drbd_device *device, enum drbd_disk_state mins)
{
	int io_allowed;

	 
	if (device->state.disk == D_DISKLESS)
		return 0;

	atomic_inc(&device->local_cnt);
	io_allowed = (device->state.disk >= mins);
	if (!io_allowed)
		put_ldev(device);
	return io_allowed;
}
#else
extern int _get_ldev_if_state(struct drbd_device *device, enum drbd_disk_state mins);
#endif

 
static inline int drbd_get_max_buffers(struct drbd_device *device)
{
	struct net_conf *nc;
	int mxb;

	rcu_read_lock();
	nc = rcu_dereference(first_peer_device(device)->connection->net_conf);
	mxb = nc ? nc->max_buffers : 1000000;   
	rcu_read_unlock();

	return mxb;
}

static inline int drbd_state_is_stable(struct drbd_device *device)
{
	union drbd_dev_state s = device->state;

	 

	switch ((enum drbd_conns)s.conn) {
	 
	case C_STANDALONE:
	case C_WF_CONNECTION:
	 
	case C_CONNECTED:
	case C_SYNC_SOURCE:
	case C_SYNC_TARGET:
	case C_VERIFY_S:
	case C_VERIFY_T:
	case C_PAUSED_SYNC_S:
	case C_PAUSED_SYNC_T:
	case C_AHEAD:
	case C_BEHIND:
		 
	case C_DISCONNECTING:
	case C_UNCONNECTED:
	case C_TIMEOUT:
	case C_BROKEN_PIPE:
	case C_NETWORK_FAILURE:
	case C_PROTOCOL_ERROR:
	case C_TEAR_DOWN:
	case C_WF_REPORT_PARAMS:
	case C_STARTING_SYNC_S:
	case C_STARTING_SYNC_T:
		break;

		 
	case C_WF_BITMAP_S:
		if (first_peer_device(device)->connection->agreed_pro_version < 96)
			return 0;
		break;

		 
	case C_WF_BITMAP_T:
	case C_WF_SYNC_UUID:
	case C_MASK:
		 
		return 0;
	}

	switch ((enum drbd_disk_state)s.disk) {
	case D_DISKLESS:
	case D_INCONSISTENT:
	case D_OUTDATED:
	case D_CONSISTENT:
	case D_UP_TO_DATE:
	case D_FAILED:
		 
		break;

	 
	case D_ATTACHING:
	case D_NEGOTIATING:
	case D_UNKNOWN:
	case D_MASK:
		 
		return 0;
	}

	return 1;
}

static inline int drbd_suspended(struct drbd_device *device)
{
	struct drbd_resource *resource = device->resource;

	return resource->susp || resource->susp_fen || resource->susp_nod;
}

static inline bool may_inc_ap_bio(struct drbd_device *device)
{
	int mxb = drbd_get_max_buffers(device);

	if (drbd_suspended(device))
		return false;
	if (atomic_read(&device->suspend_cnt))
		return false;

	 

	 
	if (!drbd_state_is_stable(device))
		return false;

	 
	if (atomic_read(&device->ap_bio_cnt) > mxb)
		return false;
	if (test_bit(BITMAP_IO, &device->flags))
		return false;
	return true;
}

static inline bool inc_ap_bio_cond(struct drbd_device *device)
{
	bool rv = false;

	spin_lock_irq(&device->resource->req_lock);
	rv = may_inc_ap_bio(device);
	if (rv)
		atomic_inc(&device->ap_bio_cnt);
	spin_unlock_irq(&device->resource->req_lock);

	return rv;
}

static inline void inc_ap_bio(struct drbd_device *device)
{
	 

	wait_event(device->misc_wait, inc_ap_bio_cond(device));
}

static inline void dec_ap_bio(struct drbd_device *device)
{
	int mxb = drbd_get_max_buffers(device);
	int ap_bio = atomic_dec_return(&device->ap_bio_cnt);

	D_ASSERT(device, ap_bio >= 0);

	if (ap_bio == 0 && test_bit(BITMAP_IO, &device->flags)) {
		if (!test_and_set_bit(BITMAP_IO_QUEUED, &device->flags))
			drbd_queue_work(&first_peer_device(device)->
				connection->sender_work,
				&device->bm_io_work.w);
	}

	 
	if (ap_bio < mxb)
		wake_up(&device->misc_wait);
}

static inline bool verify_can_do_stop_sector(struct drbd_device *device)
{
	return first_peer_device(device)->connection->agreed_pro_version >= 97 &&
		first_peer_device(device)->connection->agreed_pro_version != 100;
}

static inline int drbd_set_ed_uuid(struct drbd_device *device, u64 val)
{
	int changed = device->ed_uuid != val;
	device->ed_uuid = val;
	return changed;
}

static inline int drbd_queue_order_type(struct drbd_device *device)
{
	 
#ifndef QUEUE_ORDERED_NONE
#define QUEUE_ORDERED_NONE 0
#endif
	return QUEUE_ORDERED_NONE;
}

static inline struct drbd_connection *first_connection(struct drbd_resource *resource)
{
	return list_first_entry_or_null(&resource->connections,
				struct drbd_connection, connections);
}

#endif
