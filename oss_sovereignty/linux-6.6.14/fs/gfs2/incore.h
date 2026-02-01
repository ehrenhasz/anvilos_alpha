 
 

#ifndef __INCORE_DOT_H__
#define __INCORE_DOT_H__

#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/workqueue.h>
#include <linux/dlm.h>
#include <linux/buffer_head.h>
#include <linux/rcupdate.h>
#include <linux/rculist_bl.h>
#include <linux/completion.h>
#include <linux/rbtree.h>
#include <linux/ktime.h>
#include <linux/percpu.h>
#include <linux/lockref.h>
#include <linux/rhashtable.h>
#include <linux/mutex.h>

#define DIO_WAIT	0x00000010
#define DIO_METADATA	0x00000020

struct gfs2_log_operations;
struct gfs2_bufdata;
struct gfs2_holder;
struct gfs2_glock;
struct gfs2_quota_data;
struct gfs2_trans;
struct gfs2_jdesc;
struct gfs2_sbd;
struct lm_lockops;

typedef void (*gfs2_glop_bh_t) (struct gfs2_glock *gl, unsigned int ret);

struct gfs2_log_header_host {
	u64 lh_sequence;	 
	u32 lh_flags;		 
	u32 lh_tail;		 
	u32 lh_blkno;

	s64 lh_local_total;
	s64 lh_local_free;
	s64 lh_local_dinodes;
};

 

struct gfs2_log_operations {
	void (*lo_before_commit) (struct gfs2_sbd *sdp, struct gfs2_trans *tr);
	void (*lo_after_commit) (struct gfs2_sbd *sdp, struct gfs2_trans *tr);
	void (*lo_before_scan) (struct gfs2_jdesc *jd,
				struct gfs2_log_header_host *head, int pass);
	int (*lo_scan_elements) (struct gfs2_jdesc *jd, unsigned int start,
				 struct gfs2_log_descriptor *ld, __be64 *ptr,
				 int pass);
	void (*lo_after_scan) (struct gfs2_jdesc *jd, int error, int pass);
	const char *lo_name;
};

#define GBF_FULL 1

 
struct gfs2_bitmap {
	struct buffer_head *bi_bh;
	char *bi_clone;
	unsigned long bi_flags;
	u32 bi_offset;
	u32 bi_start;
	u32 bi_bytes;
	u32 bi_blocks;
};

struct gfs2_rgrpd {
	struct rb_node rd_node;		 
	struct gfs2_glock *rd_gl;	 
	u64 rd_addr;			 
	u64 rd_data0;			 
	u32 rd_length;			 
	u32 rd_data;			 
	u32 rd_bitbytes;		 
	u32 rd_free;
	u32 rd_requested;		 
	u32 rd_reserved;		 
	u32 rd_free_clone;
	u32 rd_dinodes;
	u64 rd_igeneration;
	struct gfs2_bitmap *rd_bits;
	struct gfs2_sbd *rd_sbd;
	struct gfs2_rgrp_lvb *rd_rgl;
	u32 rd_last_alloc;
	u32 rd_flags;
	u32 rd_extfail_pt;		 
#define GFS2_RDF_CHECK		0x10000000  
#define GFS2_RDF_ERROR		0x40000000  
#define GFS2_RDF_PREFERRED	0x80000000  
#define GFS2_RDF_MASK		0xf0000000  
	spinlock_t rd_rsspin;            
	struct mutex rd_mutex;
	struct rb_root rd_rstree;        
};

enum gfs2_state_bits {
	BH_Pinned = BH_PrivateStart,
	BH_Escaped = BH_PrivateStart + 1,
};

BUFFER_FNS(Pinned, pinned)
TAS_BUFFER_FNS(Pinned, pinned)
BUFFER_FNS(Escaped, escaped)
TAS_BUFFER_FNS(Escaped, escaped)

struct gfs2_bufdata {
	struct buffer_head *bd_bh;
	struct gfs2_glock *bd_gl;
	u64 bd_blkno;

	struct list_head bd_list;

	struct gfs2_trans *bd_tr;
	struct list_head bd_ail_st_list;
	struct list_head bd_ail_gl_list;
};

 

#define GDLM_STRNAME_BYTES	25
#define GDLM_LVB_SIZE		32

 

enum {
	DFL_BLOCK_LOCKS		= 0,
	DFL_NO_DLM_OPS		= 1,
	DFL_FIRST_MOUNT		= 2,
	DFL_FIRST_MOUNT_DONE	= 3,
	DFL_MOUNT_DONE		= 4,
	DFL_UNMOUNT		= 5,
	DFL_DLM_RECOVERY	= 6,
};

 
struct lm_lockname {
	u64 ln_number;
	struct gfs2_sbd *ln_sbd;
	unsigned int ln_type;
};

#define lm_name_equal(name1, name2) \
        (((name1)->ln_number == (name2)->ln_number) &&	\
	 ((name1)->ln_type == (name2)->ln_type) &&	\
	 ((name1)->ln_sbd == (name2)->ln_sbd))


struct gfs2_glock_operations {
	int (*go_sync) (struct gfs2_glock *gl);
	int (*go_xmote_bh)(struct gfs2_glock *gl);
	void (*go_inval) (struct gfs2_glock *gl, int flags);
	int (*go_demote_ok) (const struct gfs2_glock *gl);
	int (*go_instantiate) (struct gfs2_glock *gl);
	int (*go_held)(struct gfs2_holder *gh);
	void (*go_dump)(struct seq_file *seq, const struct gfs2_glock *gl,
			const char *fs_id_buf);
	void (*go_callback)(struct gfs2_glock *gl, bool remote);
	void (*go_free)(struct gfs2_glock *gl);
	const int go_subclass;
	const int go_type;
	const unsigned long go_flags;
#define GLOF_ASPACE 1  
#define GLOF_LVB    2  
#define GLOF_LRU    4  
#define GLOF_NONDISK   8  
};

enum {
	GFS2_LKS_SRTT = 0,	 
	GFS2_LKS_SRTTVAR = 1,	 
	GFS2_LKS_SRTTB = 2,	 
	GFS2_LKS_SRTTVARB = 3,	 
	GFS2_LKS_SIRT = 4,	 
	GFS2_LKS_SIRTVAR = 5,	 
	GFS2_LKS_DCOUNT = 6,	 
	GFS2_LKS_QCOUNT = 7,	 
	GFS2_NR_LKSTATS
};

struct gfs2_lkstats {
	u64 stats[GFS2_NR_LKSTATS];
};

enum {
	 
	HIF_HOLDER		= 6,   
	HIF_WAIT		= 10,
};

struct gfs2_holder {
	struct list_head gh_list;

	struct gfs2_glock *gh_gl;
	struct pid *gh_owner_pid;
	u16 gh_flags;
	u16 gh_state;

	int gh_error;
	unsigned long gh_iflags;  
	unsigned long gh_ip;
};

 
#define GFS2_MAXQUOTAS 2

struct gfs2_qadata {  
	 
	struct gfs2_quota_data *qa_qd[2 * GFS2_MAXQUOTAS];
	struct gfs2_holder qa_qd_ghs[2 * GFS2_MAXQUOTAS];
	unsigned int qa_qd_num;
	int qa_ref;
};

 

struct gfs2_blkreserv {
	struct rb_node rs_node;        
	struct gfs2_rgrpd *rs_rgd;
	u64 rs_start;
	u32 rs_requested;
	u32 rs_reserved;               
};

 
struct gfs2_alloc_parms {
	u64 target;
	u32 min_target;
	u32 aflags;
	u64 allowed;
};

enum {
	GLF_LOCK			= 1,
	GLF_INSTANTIATE_NEEDED		= 2,  
	GLF_DEMOTE			= 3,
	GLF_PENDING_DEMOTE		= 4,
	GLF_DEMOTE_IN_PROGRESS		= 5,
	GLF_DIRTY			= 6,
	GLF_LFLUSH			= 7,
	GLF_INVALIDATE_IN_PROGRESS	= 8,
	GLF_REPLY_PENDING		= 9,
	GLF_INITIAL			= 10,
	GLF_FROZEN			= 11,
	GLF_INSTANTIATE_IN_PROG		= 12,  
	GLF_LRU				= 13,
	GLF_OBJECT			= 14,  
	GLF_BLOCKING			= 15,
	GLF_FREEING			= 16,  
	GLF_TRY_TO_EVICT		= 17,  
	GLF_VERIFY_EVICT		= 18,  
};

struct gfs2_glock {
	unsigned long gl_flags;		 
	struct lm_lockname gl_name;

	struct lockref gl_lockref;

	 
	unsigned int gl_state:2,	 
		     gl_target:2,	 
		     gl_demote_state:2,	 
		     gl_req:2,		 
		     gl_reply:8;	 

	unsigned long gl_demote_time;  
	long gl_hold_time;
	struct list_head gl_holders;

	const struct gfs2_glock_operations *gl_ops;
	ktime_t gl_dstamp;
	struct gfs2_lkstats gl_stats;
	struct dlm_lksb gl_lksb;
	unsigned long gl_tchange;
	void *gl_object;

	struct list_head gl_lru;
	struct list_head gl_ail_list;
	atomic_t gl_ail_count;
	atomic_t gl_revokes;
	struct delayed_work gl_work;
	 
	struct {
		struct delayed_work gl_delete;
		u64 gl_no_formal_ino;
	};
	struct rcu_head gl_rcu;
	struct rhash_head gl_node;
};

enum {
	GIF_QD_LOCKED		= 1,
	GIF_ALLOC_FAILED	= 2,
	GIF_SW_PAGED		= 3,
	GIF_FREE_VFS_INODE      = 5,
	GIF_GLOP_PENDING	= 6,
	GIF_DEFERRED_DELETE	= 7,
};

struct gfs2_inode {
	struct inode i_inode;
	u64 i_no_addr;
	u64 i_no_formal_ino;
	u64 i_generation;
	u64 i_eattr;
	unsigned long i_flags;		 
	struct gfs2_glock *i_gl;
	struct gfs2_holder i_iopen_gh;
	struct gfs2_qadata *i_qadata;  
	struct gfs2_holder i_rgd_gh;
	struct gfs2_blkreserv i_res;  
	u64 i_goal;	 
	atomic_t i_sizehint;   
	struct rw_semaphore i_rw_mutex;
	struct list_head i_ordered;
	__be64 *i_hash_cache;
	u32 i_entries;
	u32 i_diskflags;
	u8 i_height;
	u8 i_depth;
	u16 i_rahead;
};

 
static inline struct gfs2_inode *GFS2_I(struct inode *inode)
{
	return container_of(inode, struct gfs2_inode, i_inode);
}

static inline struct gfs2_sbd *GFS2_SB(const struct inode *inode)
{
	return inode->i_sb->s_fs_info;
}

struct gfs2_file {
	struct mutex f_fl_mutex;
	struct gfs2_holder f_fl_gh;
};

struct gfs2_revoke_replay {
	struct list_head rr_list;
	u64 rr_blkno;
	unsigned int rr_where;
};

enum {
	QDF_CHANGE		= 1,
	QDF_LOCKED		= 2,
	QDF_REFRESH		= 3,
	QDF_QMSG_QUIET          = 4,
};

struct gfs2_quota_data {
	struct hlist_bl_node qd_hlist;
	struct list_head qd_list;
	struct kqid qd_id;
	struct gfs2_sbd *qd_sbd;
	struct lockref qd_lockref;
	struct list_head qd_lru;
	unsigned qd_hash;

	unsigned long qd_flags;		 

	s64 qd_change;
	s64 qd_change_sync;

	unsigned int qd_slot;
	unsigned int qd_slot_ref;

	struct buffer_head *qd_bh;
	struct gfs2_quota_change *qd_bh_qc;
	unsigned int qd_bh_count;

	struct gfs2_glock *qd_gl;
	struct gfs2_quota_lvb qd_qb;

	u64 qd_sync_gen;
	unsigned long qd_last_warn;
	struct rcu_head qd_rcu;
};

enum {
	TR_TOUCHED = 1,
	TR_ATTACHED = 2,
	TR_ONSTACK = 3,
};

struct gfs2_trans {
	unsigned long tr_ip;

	unsigned int tr_blocks;
	unsigned int tr_revokes;
	unsigned int tr_reserved;
	unsigned long tr_flags;

	unsigned int tr_num_buf_new;
	unsigned int tr_num_databuf_new;
	unsigned int tr_num_buf_rm;
	unsigned int tr_num_databuf_rm;
	unsigned int tr_num_revoke;

	struct list_head tr_list;
	struct list_head tr_databuf;
	struct list_head tr_buf;

	unsigned int tr_first;
	struct list_head tr_ail1_list;
	struct list_head tr_ail2_list;
};

struct gfs2_journal_extent {
	struct list_head list;

	unsigned int lblock;  
	u64 dblock;  
	u64 blocks;
};

struct gfs2_jdesc {
	struct list_head jd_list;
	struct list_head extent_list;
	unsigned int nr_extents;
	struct work_struct jd_work;
	struct inode *jd_inode;
	struct bio *jd_log_bio;
	unsigned long jd_flags;
#define JDF_RECOVERY 1
	unsigned int jd_jid;
	u32 jd_blocks;
	int jd_recover_error;
	 

	unsigned int jd_found_blocks;
	unsigned int jd_found_revokes;
	unsigned int jd_replayed_blocks;

	struct list_head jd_revoke_list;
	unsigned int jd_replay_tail;

	u64 jd_no_addr;
};

struct gfs2_statfs_change_host {
	s64 sc_total;
	s64 sc_free;
	s64 sc_dinodes;
};

#define GFS2_QUOTA_DEFAULT	GFS2_QUOTA_OFF
#define GFS2_QUOTA_OFF		0
#define GFS2_QUOTA_ACCOUNT	1
#define GFS2_QUOTA_ON		2
#define GFS2_QUOTA_QUIET	3  

#define GFS2_DATA_DEFAULT	GFS2_DATA_ORDERED
#define GFS2_DATA_WRITEBACK	1
#define GFS2_DATA_ORDERED	2

#define GFS2_ERRORS_DEFAULT     GFS2_ERRORS_WITHDRAW
#define GFS2_ERRORS_WITHDRAW    0
#define GFS2_ERRORS_CONTINUE    1  
#define GFS2_ERRORS_RO          2  
#define GFS2_ERRORS_PANIC       3

struct gfs2_args {
	char ar_lockproto[GFS2_LOCKNAME_LEN];	 
	char ar_locktable[GFS2_LOCKNAME_LEN];	 
	char ar_hostdata[GFS2_LOCKNAME_LEN];	 
	unsigned int ar_spectator:1;		 
	unsigned int ar_localflocks:1;		 
	unsigned int ar_debug:1;		 
	unsigned int ar_posix_acl:1;		 
	unsigned int ar_quota:2;		 
	unsigned int ar_suiddir:1;		 
	unsigned int ar_data:2;			 
	unsigned int ar_meta:1;			 
	unsigned int ar_discard:1;		 
	unsigned int ar_errors:2;                
	unsigned int ar_nobarrier:1;             
	unsigned int ar_rgrplvb:1;		 
	unsigned int ar_got_rgrplvb:1;		 
	unsigned int ar_loccookie:1;		 
	s32 ar_commit;				 
	s32 ar_statfs_quantum;			 
	s32 ar_quota_quantum;			 
	s32 ar_statfs_percent;			 
};

struct gfs2_tune {
	spinlock_t gt_spin;

	unsigned int gt_logd_secs;

	unsigned int gt_quota_warn_period;  
	unsigned int gt_quota_scale_num;  
	unsigned int gt_quota_scale_den;  
	unsigned int gt_quota_quantum;  
	unsigned int gt_new_files_jdata;
	unsigned int gt_max_readahead;  
	unsigned int gt_complain_secs;
	unsigned int gt_statfs_quantum;
	unsigned int gt_statfs_slow;
};

enum {
	SDF_JOURNAL_CHECKED	= 0,
	SDF_JOURNAL_LIVE	= 1,
	SDF_WITHDRAWN		= 2,
	SDF_NOBARRIERS		= 3,
	SDF_NORECOVERY		= 4,
	SDF_DEMOTE		= 5,
	SDF_NOJOURNALID		= 6,
	SDF_RORECOVERY		= 7,  
	SDF_SKIP_DLM_UNLOCK	= 8,
	SDF_FORCE_AIL_FLUSH     = 9,
	SDF_FREEZE_INITIATOR	= 10,
	SDF_WITHDRAWING		= 11,  
	SDF_WITHDRAW_IN_PROG	= 12,  
	SDF_REMOTE_WITHDRAW	= 13,  
	SDF_WITHDRAW_RECOVERY	= 14,  
	SDF_KILL		= 15,
	SDF_EVICTING		= 16,
	SDF_FROZEN		= 17,
};

#define GFS2_FSNAME_LEN		256

struct gfs2_inum_host {
	u64 no_formal_ino;
	u64 no_addr;
};

struct gfs2_sb_host {
	u32 sb_magic;
	u32 sb_type;

	u32 sb_fs_format;
	u32 sb_multihost_format;
	u32 sb_bsize;
	u32 sb_bsize_shift;

	struct gfs2_inum_host sb_master_dir;
	struct gfs2_inum_host sb_root_dir;

	char sb_lockproto[GFS2_LOCKNAME_LEN];
	char sb_locktable[GFS2_LOCKNAME_LEN];
};

 

struct lm_lockstruct {
	int ls_jid;
	unsigned int ls_first;
	const struct lm_lockops *ls_ops;
	dlm_lockspace_t *ls_dlm;

	int ls_recover_jid_done;    
	int ls_recover_jid_status;  

	struct dlm_lksb ls_mounted_lksb;  
	struct dlm_lksb ls_control_lksb;  
	char ls_control_lvb[GDLM_LVB_SIZE];  
	struct completion ls_sync_wait;  
	char *ls_lvb_bits;

	spinlock_t ls_recover_spin;  
	unsigned long ls_recover_flags;  
	uint32_t ls_recover_mount;  
	uint32_t ls_recover_start;  
	uint32_t ls_recover_block;  
	uint32_t ls_recover_size;  
	uint32_t *ls_recover_submit;  
	uint32_t *ls_recover_result;  
};

struct gfs2_pcpu_lkstats {
	 
	struct gfs2_lkstats lkstats[10];
};

 
struct local_statfs_inode {
	struct list_head si_list;
	struct inode *si_sc_inode;
	unsigned int si_jid;  
};

struct gfs2_sbd {
	struct super_block *sd_vfs;
	struct gfs2_pcpu_lkstats __percpu *sd_lkstats;
	struct kobject sd_kobj;
	struct completion sd_kobj_unregister;
	unsigned long sd_flags;	 
	struct gfs2_sb_host sd_sb;

	 

	u32 sd_fsb2bb;
	u32 sd_fsb2bb_shift;
	u32 sd_diptrs;	 
	u32 sd_inptrs;	 
	u32 sd_ldptrs;   
	u32 sd_jbsize;	 
	u32 sd_hash_bsize;	 
	u32 sd_hash_bsize_shift;
	u32 sd_hash_ptrs;	 
	u32 sd_qc_per_block;
	u32 sd_blocks_per_bitmap;
	u32 sd_max_dirres;	 
	u32 sd_max_height;	 
	u64 sd_heightsize[GFS2_MAX_META_HEIGHT + 1];
	u32 sd_max_dents_per_leaf;  

	struct gfs2_args sd_args;	 
	struct gfs2_tune sd_tune;	 

	 

	struct lm_lockstruct sd_lockstruct;
	struct gfs2_holder sd_live_gh;
	struct gfs2_glock *sd_rename_gl;
	struct gfs2_glock *sd_freeze_gl;
	struct work_struct sd_freeze_work;
	wait_queue_head_t sd_kill_wait;
	wait_queue_head_t sd_async_glock_wait;
	atomic_t sd_glock_disposal;
	struct completion sd_locking_init;
	struct completion sd_wdack;
	struct delayed_work sd_control_work;

	 

	struct dentry *sd_master_dir;
	struct dentry *sd_root_dir;

	struct inode *sd_jindex;
	struct inode *sd_statfs_inode;
	struct inode *sd_sc_inode;
	struct list_head sd_sc_inodes_list;
	struct inode *sd_qc_inode;
	struct inode *sd_rindex;
	struct inode *sd_quota_inode;

	 

	spinlock_t sd_statfs_spin;
	struct gfs2_statfs_change_host sd_statfs_master;
	struct gfs2_statfs_change_host sd_statfs_local;
	int sd_statfs_force_sync;

	 

	int sd_rindex_uptodate;
	spinlock_t sd_rindex_spin;
	struct rb_root sd_rindex_tree;
	unsigned int sd_rgrps;
	unsigned int sd_max_rg_data;

	 

	struct list_head sd_jindex_list;
	spinlock_t sd_jindex_spin;
	struct mutex sd_jindex_mutex;
	unsigned int sd_journals;

	struct gfs2_jdesc *sd_jdesc;
	struct gfs2_holder sd_journal_gh;
	struct gfs2_holder sd_jinode_gh;
	struct gfs2_glock *sd_jinode_gl;

	struct gfs2_holder sd_sc_gh;
	struct buffer_head *sd_sc_bh;
	struct gfs2_holder sd_qc_gh;

	struct completion sd_journal_ready;

	 

	struct workqueue_struct *sd_delete_wq;

	 

	struct task_struct *sd_logd_process;
	struct task_struct *sd_quotad_process;

	 

	struct list_head sd_quota_list;
	atomic_t sd_quota_count;
	struct mutex sd_quota_mutex;
	struct mutex sd_quota_sync_mutex;
	wait_queue_head_t sd_quota_wait;

	unsigned int sd_quota_slots;
	unsigned long *sd_quota_bitmap;
	spinlock_t sd_bitmap_lock;

	u64 sd_quota_sync_gen;

	 

	struct address_space sd_aspace;

	spinlock_t sd_log_lock;

	struct gfs2_trans *sd_log_tr;
	unsigned int sd_log_blks_reserved;

	atomic_t sd_log_pinned;
	unsigned int sd_log_num_revoke;

	struct list_head sd_log_revokes;
	struct list_head sd_log_ordered;
	spinlock_t sd_ordered_lock;

	atomic_t sd_log_thresh1;
	atomic_t sd_log_thresh2;
	atomic_t sd_log_blks_free;
	atomic_t sd_log_blks_needed;
	atomic_t sd_log_revokes_available;
	wait_queue_head_t sd_log_waitq;
	wait_queue_head_t sd_logd_waitq;

	u64 sd_log_sequence;
	int sd_log_idle;

	struct rw_semaphore sd_log_flush_lock;
	atomic_t sd_log_in_flight;
	wait_queue_head_t sd_log_flush_wait;
	int sd_log_error;  
	wait_queue_head_t sd_withdraw_wait;

	unsigned int sd_log_tail;
	unsigned int sd_log_flush_tail;
	unsigned int sd_log_head;
	unsigned int sd_log_flush_head;

	spinlock_t sd_ail_lock;
	struct list_head sd_ail1_list;
	struct list_head sd_ail2_list;

	 
	struct gfs2_holder sd_freeze_gh;
	struct mutex sd_freeze_mutex;

	char sd_fsname[GFS2_FSNAME_LEN + 3 * sizeof(int) + 2];
	char sd_table_name[GFS2_FSNAME_LEN];
	char sd_proto_name[GFS2_FSNAME_LEN];

	 

	unsigned long sd_last_warning;
	struct dentry *debugfs_dir;     
	unsigned long sd_glock_dqs_held;
};

static inline void gfs2_glstats_inc(struct gfs2_glock *gl, int which)
{
	gl->gl_stats.stats[which]++;
}

static inline void gfs2_sbstats_inc(const struct gfs2_glock *gl, int which)
{
	const struct gfs2_sbd *sdp = gl->gl_name.ln_sbd;
	preempt_disable();
	this_cpu_ptr(sdp->sd_lkstats)->lkstats[gl->gl_name.ln_type].stats[which]++;
	preempt_enable();
}

extern struct gfs2_rgrpd *gfs2_glock2rgrp(struct gfs2_glock *gl);

static inline unsigned gfs2_max_stuffed_size(const struct gfs2_inode *ip)
{
	return GFS2_SB(&ip->i_inode)->sd_sb.sb_bsize - sizeof(struct gfs2_dinode);
}

#endif  

