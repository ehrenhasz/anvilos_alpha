


#ifndef __UBIFS_H__
#define __UBIFS_H__

#include <asm/div64.h>
#include <linux/statfs.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/mtd/ubi.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/security.h>
#include <linux/xattr.h>
#include <linux/random.h>
#include <linux/sysfs.h>
#include <linux/completion.h>
#include <crypto/hash_info.h>
#include <crypto/hash.h>
#include <crypto/algapi.h>

#include <linux/fscrypt.h>

#include "ubifs-media.h"


#define UBIFS_VERSION 1


#define UBIFS_SUPER_MAGIC 0x24051905


#define UBIFS_BLOCKS_PER_PAGE (PAGE_SIZE / UBIFS_BLOCK_SIZE)
#define UBIFS_BLOCKS_PER_PAGE_SHIFT (PAGE_SHIFT - UBIFS_BLOCK_SHIFT)


#define SQNUM_WARN_WATERMARK 0xFFFFFFFF00000000ULL
#define SQNUM_WATERMARK      0xFFFFFFFFFF000000ULL


#define MIN_INDEX_LEBS 2


#define MIN_WRITE_SZ (UBIFS_DATA_NODE_SZ + 8)


#define INUM_WARN_WATERMARK 0xFFF00000
#define INUM_WATERMARK      0xFFFFFF00


#define LPT_HEAP_SZ 256


#define BGT_NAME_PATTERN "ubifs_bgt%d_%d"


#define MAX_INUM 0xFFFFFFFF


#define NONDATA_JHEADS_CNT 2


#define GCHD   UBIFS_GC_HEAD
#define BASEHD UBIFS_BASE_HEAD
#define DATAHD UBIFS_DATA_HEAD


#define LPROPS_NC 0x80000001


#define UBIFS_TRUN_KEY    UBIFS_KEY_TYPES_CNT
#define UBIFS_INVALID_KEY UBIFS_KEY_TYPES_CNT


#define CALC_DENT_SIZE(name_len) ALIGN(UBIFS_DENT_NODE_SZ + (name_len) + 1, 8)


#define CALC_XATTR_BYTES(data_len) ALIGN(UBIFS_INO_NODE_SZ + (data_len) + 1, 8)


#define OLD_ZNODE_AGE 20
#define YOUNG_ZNODE_AGE 5


#define WORST_COMPR_FACTOR 2

#ifdef CONFIG_FS_ENCRYPTION
#define UBIFS_CIPHER_BLOCK_SIZE FSCRYPT_CONTENTS_ALIGNMENT
#else
#define UBIFS_CIPHER_BLOCK_SIZE 0
#endif


#define COMPRESSED_DATA_NODE_BUF_SZ \
	(UBIFS_DATA_NODE_SZ + UBIFS_BLOCK_SIZE * WORST_COMPR_FACTOR)


#define BOTTOM_UP_HEIGHT 64


#define UBIFS_MAX_BULK_READ 32

#ifdef CONFIG_UBIFS_FS_AUTHENTICATION
#define UBIFS_HASH_ARR_SZ UBIFS_MAX_HASH_LEN
#define UBIFS_HMAC_ARR_SZ UBIFS_MAX_HMAC_LEN
#else
#define UBIFS_HASH_ARR_SZ 0
#define UBIFS_HMAC_ARR_SZ 0
#endif


#define UBIFS_DFS_DIR_NAME "ubi%d_%d"
#define UBIFS_DFS_DIR_LEN  (3 + 1 + 2*2 + 1)


enum {
	WB_MUTEX_1 = 0,
	WB_MUTEX_2 = 1,
	WB_MUTEX_3 = 2,
	WB_MUTEX_4 = 3,
};


enum {
	DIRTY_ZNODE    = 0,
	COW_ZNODE      = 1,
	OBSOLETE_ZNODE = 2,
};


enum {
	COMMIT_RESTING = 0,
	COMMIT_BACKGROUND,
	COMMIT_REQUIRED,
	COMMIT_RUNNING_BACKGROUND,
	COMMIT_RUNNING_REQUIRED,
	COMMIT_BROKEN,
};


enum {
	SCANNED_GARBAGE        = 0,
	SCANNED_EMPTY_SPACE    = -1,
	SCANNED_A_NODE         = -2,
	SCANNED_A_CORRUPT_NODE = -3,
	SCANNED_A_BAD_PAD_NODE = -4,
};


enum {
	DIRTY_CNODE    = 0,
	OBSOLETE_CNODE = 1,
	COW_CNODE      = 2,
};


enum {
	LTAB_DIRTY  = 1,
	LSAVE_DIRTY = 2,
};


enum {
	LEB_FREED,
	LEB_FREED_IDX,
	LEB_RETAINED,
};


enum {
	ASSACT_REPORT = 0,
	ASSACT_RO,
	ASSACT_PANIC,
};


struct ubifs_old_idx {
	struct rb_node rb;
	int lnum;
	int offs;
};


union ubifs_key {
	uint8_t u8[UBIFS_SK_LEN];
	uint32_t u32[UBIFS_SK_LEN/4];
	uint64_t u64[UBIFS_SK_LEN/8];
	__le32 j32[UBIFS_SK_LEN/4];
};


struct ubifs_scan_node {
	struct list_head list;
	union ubifs_key key;
	unsigned long long sqnum;
	int type;
	int offs;
	int len;
	void *node;
};


struct ubifs_scan_leb {
	int lnum;
	int nodes_cnt;
	struct list_head nodes;
	int endpt;
	void *buf;
};


struct ubifs_gced_idx_leb {
	struct list_head list;
	int lnum;
	int unmap;
};


struct ubifs_inode {
	struct inode vfs_inode;
	unsigned long long creat_sqnum;
	unsigned long long del_cmtno;
	unsigned int xattr_size;
	unsigned int xattr_cnt;
	unsigned int xattr_names;
	unsigned int dirty:1;
	unsigned int xattr:1;
	unsigned int bulk_read:1;
	unsigned int compr_type:2;
	struct mutex ui_mutex;
	struct rw_semaphore xattr_sem;
	spinlock_t ui_lock;
	loff_t synced_i_size;
	loff_t ui_size;
	int flags;
	pgoff_t last_page_read;
	pgoff_t read_in_a_row;
	int data_len;
	void *data;
};


struct ubifs_unclean_leb {
	struct list_head list;
	int lnum;
	int endpt;
};


enum {
	LPROPS_UNCAT     =  0,
	LPROPS_DIRTY     =  1,
	LPROPS_DIRTY_IDX =  2,
	LPROPS_FREE      =  3,
	LPROPS_HEAP_CNT  =  3,
	LPROPS_EMPTY     =  4,
	LPROPS_FREEABLE  =  5,
	LPROPS_FRDI_IDX  =  6,
	LPROPS_CAT_MASK  = 15,
	LPROPS_TAKEN     = 16,
	LPROPS_INDEX     = 32,
};


struct ubifs_lprops {
	int free;
	int dirty;
	int flags;
	int lnum;
	union {
		struct list_head list;
		int hpos;
	};
};


struct ubifs_lpt_lprops {
	int free;
	int dirty;
	unsigned tgc:1;
	unsigned cmt:1;
};


struct ubifs_lp_stats {
	int empty_lebs;
	int taken_empty_lebs;
	int idx_lebs;
	long long total_free;
	long long total_dirty;
	long long total_used;
	long long total_dead;
	long long total_dark;
};

struct ubifs_nnode;


struct ubifs_cnode {
	struct ubifs_nnode *parent;
	struct ubifs_cnode *cnext;
	unsigned long flags;
	int iip;
	int level;
	int num;
};


struct ubifs_pnode {
	struct ubifs_nnode *parent;
	struct ubifs_cnode *cnext;
	unsigned long flags;
	int iip;
	int level;
	int num;
	struct ubifs_lprops lprops[UBIFS_LPT_FANOUT];
};


struct ubifs_nbranch {
	int lnum;
	int offs;
	union {
		struct ubifs_nnode *nnode;
		struct ubifs_pnode *pnode;
		struct ubifs_cnode *cnode;
	};
};


struct ubifs_nnode {
	struct ubifs_nnode *parent;
	struct ubifs_cnode *cnext;
	unsigned long flags;
	int iip;
	int level;
	int num;
	struct ubifs_nbranch nbranch[UBIFS_LPT_FANOUT];
};


struct ubifs_lpt_heap {
	struct ubifs_lprops **arr;
	int cnt;
	int max_cnt;
};


enum {
	LPT_SCAN_CONTINUE = 0,
	LPT_SCAN_ADD = 1,
	LPT_SCAN_STOP = 2,
};

struct ubifs_info;


typedef int (*ubifs_lpt_scan_callback)(struct ubifs_info *c,
				       const struct ubifs_lprops *lprops,
				       int in_tree, void *data);


struct ubifs_wbuf {
	struct ubifs_info *c;
	void *buf;
	int lnum;
	int offs;
	int avail;
	int used;
	int size;
	int jhead;
	int (*sync_callback)(struct ubifs_info *c, int lnum, int free, int pad);
	struct mutex io_mutex;
	spinlock_t lock;
	struct hrtimer timer;
	unsigned int no_timer:1;
	unsigned int need_sync:1;
	int next_ino;
	ino_t *inodes;
};


struct ubifs_bud {
	int lnum;
	int start;
	int jhead;
	struct list_head list;
	struct rb_node rb;
	struct shash_desc *log_hash;
};


struct ubifs_jhead {
	struct ubifs_wbuf wbuf;
	struct list_head buds_list;
	unsigned int grouped:1;
	struct shash_desc *log_hash;
};


struct ubifs_zbranch {
	union ubifs_key key;
	union {
		struct ubifs_znode *znode;
		void *leaf;
	};
	int lnum;
	int offs;
	int len;
	u8 hash[UBIFS_HASH_ARR_SZ];
};


struct ubifs_znode {
	struct ubifs_znode *parent;
	struct ubifs_znode *cnext;
	struct ubifs_znode *cparent;
	int ciip;
	unsigned long flags;
	time64_t time;
	int level;
	int child_cnt;
	int iip;
	int alt;
	int lnum;
	int offs;
	int len;
	struct ubifs_zbranch zbranch[];
};


struct bu_info {
	union ubifs_key key;
	struct ubifs_zbranch zbranch[UBIFS_MAX_BULK_READ];
	void *buf;
	int buf_len;
	int gc_seq;
	int cnt;
	int blk_cnt;
	int eof;
};


struct ubifs_node_range {
	union {
		int len;
		int min_len;
	};
	int max_len;
};


struct ubifs_compressor {
	int compr_type;
	struct crypto_comp *cc;
	struct mutex *comp_mutex;
	struct mutex *decomp_mutex;
	const char *name;
	const char *capi_name;
};


struct ubifs_budget_req {
	unsigned int fast:1;
	unsigned int recalculate:1;
#ifndef UBIFS_DEBUG
	unsigned int new_page:1;
	unsigned int dirtied_page:1;
	unsigned int new_dent:1;
	unsigned int mod_dent:1;
	unsigned int new_ino:1;
	unsigned int new_ino_d:13;
	unsigned int dirtied_ino:4;
	unsigned int dirtied_ino_d:15;
#else
	
	unsigned int new_page;
	unsigned int dirtied_page;
	unsigned int new_dent;
	unsigned int mod_dent;
	unsigned int new_ino;
	unsigned int new_ino_d;
	unsigned int dirtied_ino;
	unsigned int dirtied_ino_d;
#endif
	int idx_growth;
	int data_growth;
	int dd_growth;
};


struct ubifs_orphan {
	struct rb_node rb;
	struct list_head list;
	struct list_head new_list;
	struct list_head child_list;
	struct ubifs_orphan *cnext;
	struct ubifs_orphan *dnext;
	ino_t inum;
	unsigned new:1;
	unsigned cmt:1;
	unsigned del:1;
};


struct ubifs_mount_opts {
	unsigned int unmount_mode:2;
	unsigned int bulk_read:2;
	unsigned int chk_data_crc:2;
	unsigned int override_compr:1;
	unsigned int compr_type:2;
};


struct ubifs_budg_info {
	long long idx_growth;
	long long data_growth;
	long long dd_growth;
	long long uncommitted_idx;
	unsigned long long old_idx_sz;
	int min_idx_lebs;
	unsigned int nospace:1;
	unsigned int nospace_rp:1;
	int page_budget;
	int inode_budget;
	int dent_budget;
};


struct ubifs_stats_info {
	unsigned int magic_errors;
	unsigned int node_errors;
	unsigned int crc_errors;
};

struct ubifs_debug_info;


struct ubifs_info {
	struct super_block *vfs_sb;
	struct ubifs_sb_node *sup_node;

	ino_t highest_inum;
	unsigned long long max_sqnum;
	unsigned long long cmt_no;
	spinlock_t cnt_lock;
	int fmt_version;
	int ro_compat_version;
	unsigned char uuid[16];

	int lhead_lnum;
	int lhead_offs;
	int ltail_lnum;
	struct mutex log_mutex;
	int min_log_bytes;
	long long cmt_bud_bytes;

	struct rb_root buds;
	long long bud_bytes;
	spinlock_t buds_lock;
	int jhead_cnt;
	struct ubifs_jhead *jheads;
	long long max_bud_bytes;
	long long bg_bud_bytes;
	struct list_head old_buds;
	int max_bud_cnt;

	struct rw_semaphore commit_sem;
	int cmt_state;
	spinlock_t cs_lock;
	wait_queue_head_t cmt_wq;

	struct kobject kobj;
	struct completion kobj_unregister;

	unsigned int big_lpt:1;
	unsigned int space_fixup:1;
	unsigned int double_hash:1;
	unsigned int encrypted:1;
	unsigned int no_chk_data_crc:1;
	unsigned int bulk_read:1;
	unsigned int default_compr:2;
	unsigned int rw_incompat:1;
	unsigned int assert_action:2;
	unsigned int authenticated:1;
	unsigned int superblock_need_write:1;

	struct mutex tnc_mutex;
	struct ubifs_zbranch zroot;
	struct ubifs_znode *cnext;
	struct ubifs_znode *enext;
	int *gap_lebs;
	void *cbuf;
	void *ileb_buf;
	int ileb_len;
	int ihead_lnum;
	int ihead_offs;
	int *ilebs;
	int ileb_cnt;
	int ileb_nxt;
	struct rb_root old_idx;
	int *bottom_up_buf;

	struct ubifs_mst_node *mst_node;
	int mst_offs;

	int max_bu_buf_len;
	struct mutex bu_mutex;
	struct bu_info bu;

	struct mutex write_reserve_mutex;
	void *write_reserve_buf;

	int log_lebs;
	long long log_bytes;
	int log_last;
	int lpt_lebs;
	int lpt_first;
	int lpt_last;
	int orph_lebs;
	int orph_first;
	int orph_last;
	int main_lebs;
	int main_first;
	long long main_bytes;

	uint8_t key_hash_type;
	uint32_t (*key_hash)(const char *str, int len);
	int key_fmt;
	int key_len;
	int hash_len;
	int fanout;

	int min_io_size;
	int min_io_shift;
	int max_write_size;
	int max_write_shift;
	int leb_size;
	int leb_start;
	int half_leb_size;
	int idx_leb_size;
	int leb_cnt;
	int max_leb_cnt;
	unsigned int ro_media:1;
	unsigned int ro_mount:1;
	unsigned int ro_error:1;

	atomic_long_t dirty_pg_cnt;
	atomic_long_t dirty_zn_cnt;
	atomic_long_t clean_zn_cnt;

	spinlock_t space_lock;
	struct ubifs_lp_stats lst;
	struct ubifs_budg_info bi;
	unsigned long long calc_idx_sz;

	int ref_node_alsz;
	int mst_node_alsz;
	int min_idx_node_sz;
	int max_idx_node_sz;
	long long max_inode_sz;
	int max_znode_sz;

	int leb_overhead;
	int dead_wm;
	int dark_wm;
	int block_cnt;

	struct ubifs_node_range ranges[UBIFS_NODE_TYPES_CNT];
	struct ubi_volume_desc *ubi;
	struct ubi_device_info di;
	struct ubi_volume_info vi;

	struct rb_root orph_tree;
	struct list_head orph_list;
	struct list_head orph_new;
	struct ubifs_orphan *orph_cnext;
	struct ubifs_orphan *orph_dnext;
	spinlock_t orphan_lock;
	void *orph_buf;
	int new_orphans;
	int cmt_orphans;
	int tot_orphans;
	int max_orphans;
	int ohead_lnum;
	int ohead_offs;
	int no_orphs;

	struct task_struct *bgt;
	char bgt_name[sizeof(BGT_NAME_PATTERN) + 9];
	int need_bgt;
	int need_wbuf_sync;

	int gc_lnum;
	void *sbuf;
	struct list_head idx_gc;
	int idx_gc_cnt;
	int gc_seq;
	int gced_lnum;

	struct list_head infos_list;
	struct mutex umount_mutex;
	unsigned int shrinker_run_no;

	int space_bits;
	int lpt_lnum_bits;
	int lpt_offs_bits;
	int lpt_spc_bits;
	int pcnt_bits;
	int lnum_bits;
	int nnode_sz;
	int pnode_sz;
	int ltab_sz;
	int lsave_sz;
	int pnode_cnt;
	int nnode_cnt;
	int lpt_hght;
	int pnodes_have;

	struct mutex lp_mutex;
	int lpt_lnum;
	int lpt_offs;
	int nhead_lnum;
	int nhead_offs;
	int lpt_drty_flgs;
	int dirty_nn_cnt;
	int dirty_pn_cnt;
	int check_lpt_free;
	long long lpt_sz;
	void *lpt_nod_buf;
	void *lpt_buf;
	struct ubifs_nnode *nroot;
	struct ubifs_cnode *lpt_cnext;
	struct ubifs_lpt_heap lpt_heap[LPROPS_HEAP_CNT];
	struct ubifs_lpt_heap dirty_idx;
	struct list_head uncat_list;
	struct list_head empty_list;
	struct list_head freeable_list;
	struct list_head frdi_idx_list;
	int freeable_cnt;
	int in_a_category_cnt;

	int ltab_lnum;
	int ltab_offs;
	struct ubifs_lpt_lprops *ltab;
	struct ubifs_lpt_lprops *ltab_cmt;
	int lsave_cnt;
	int lsave_lnum;
	int lsave_offs;
	int *lsave;
	int lscan_lnum;

	long long rp_size;
	long long report_rp_size;
	kuid_t rp_uid;
	kgid_t rp_gid;

	struct crypto_shash *hash_tfm;
	struct crypto_shash *hmac_tfm;
	int hmac_desc_len;
	char *auth_key_name;
	char *auth_hash_name;
	enum hash_algo auth_hash_algo;

	struct shash_desc *log_hash;

	
	unsigned int empty:1;
	unsigned int need_recovery:1;
	unsigned int replaying:1;
	unsigned int mounting:1;
	unsigned int remounting_rw:1;
	unsigned int probing:1;
	struct list_head replay_list;
	struct list_head replay_buds;
	unsigned long long cs_sqnum;
	struct list_head unclean_leb_list;
	struct ubifs_mst_node *rcvrd_mst_node;
	struct rb_root size_tree;
	struct ubifs_mount_opts mount_opts;

	struct ubifs_debug_info *dbg;
	struct ubifs_stats_info *stats;
};

extern struct list_head ubifs_infos;
extern spinlock_t ubifs_infos_lock;
extern atomic_long_t ubifs_clean_zn_cnt;
extern const struct super_operations ubifs_super_operations;
extern const struct address_space_operations ubifs_file_address_operations;
extern const struct file_operations ubifs_file_operations;
extern const struct inode_operations ubifs_file_inode_operations;
extern const struct file_operations ubifs_dir_operations;
extern const struct inode_operations ubifs_dir_inode_operations;
extern const struct inode_operations ubifs_symlink_inode_operations;
extern struct ubifs_compressor *ubifs_compressors[UBIFS_COMPR_TYPES_CNT];
extern int ubifs_default_version;


static inline int ubifs_authenticated(const struct ubifs_info *c)
{
	return (IS_ENABLED(CONFIG_UBIFS_FS_AUTHENTICATION)) && c->authenticated;
}

struct shash_desc *__ubifs_hash_get_desc(const struct ubifs_info *c);
static inline struct shash_desc *ubifs_hash_get_desc(const struct ubifs_info *c)
{
	return ubifs_authenticated(c) ? __ubifs_hash_get_desc(c) : NULL;
}

static inline int ubifs_shash_init(const struct ubifs_info *c,
				   struct shash_desc *desc)
{
	if (ubifs_authenticated(c))
		return crypto_shash_init(desc);
	else
		return 0;
}

static inline int ubifs_shash_update(const struct ubifs_info *c,
				      struct shash_desc *desc, const void *buf,
				      unsigned int len)
{
	int err = 0;

	if (ubifs_authenticated(c)) {
		err = crypto_shash_update(desc, buf, len);
		if (err < 0)
			return err;
	}

	return 0;
}

static inline int ubifs_shash_final(const struct ubifs_info *c,
				    struct shash_desc *desc, u8 *out)
{
	return ubifs_authenticated(c) ? crypto_shash_final(desc, out) : 0;
}

int __ubifs_node_calc_hash(const struct ubifs_info *c, const void *buf,
			  u8 *hash);
static inline int ubifs_node_calc_hash(const struct ubifs_info *c,
					const void *buf, u8 *hash)
{
	if (ubifs_authenticated(c))
		return __ubifs_node_calc_hash(c, buf, hash);
	else
		return 0;
}

int ubifs_prepare_auth_node(struct ubifs_info *c, void *node,
			     struct shash_desc *inhash);


static inline int ubifs_check_hash(const struct ubifs_info *c,
				   const u8 *expected, const u8 *got)
{
	return crypto_memneq(expected, got, c->hash_len);
}


static inline int ubifs_check_hmac(const struct ubifs_info *c,
				   const u8 *expected, const u8 *got)
{
	return crypto_memneq(expected, got, c->hmac_desc_len);
}

#ifdef CONFIG_UBIFS_FS_AUTHENTICATION
void ubifs_bad_hash(const struct ubifs_info *c, const void *node,
		    const u8 *hash, int lnum, int offs);
#else
static inline void ubifs_bad_hash(const struct ubifs_info *c, const void *node,
				  const u8 *hash, int lnum, int offs) {};
#endif

int __ubifs_node_check_hash(const struct ubifs_info *c, const void *buf,
			  const u8 *expected);
static inline int ubifs_node_check_hash(const struct ubifs_info *c,
					const void *buf, const u8 *expected)
{
	if (ubifs_authenticated(c))
		return __ubifs_node_check_hash(c, buf, expected);
	else
		return 0;
}

int ubifs_init_authentication(struct ubifs_info *c);
void __ubifs_exit_authentication(struct ubifs_info *c);
static inline void ubifs_exit_authentication(struct ubifs_info *c)
{
	if (ubifs_authenticated(c))
		__ubifs_exit_authentication(c);
}


static inline u8 *ubifs_branch_hash(struct ubifs_info *c,
				    struct ubifs_branch *br)
{
	return (void *)br + sizeof(*br) + c->key_len;
}


static inline void ubifs_copy_hash(const struct ubifs_info *c, const u8 *from,
				   u8 *to)
{
	if (ubifs_authenticated(c))
		memcpy(to, from, c->hash_len);
}

int __ubifs_node_insert_hmac(const struct ubifs_info *c, void *buf,
			      int len, int ofs_hmac);
static inline int ubifs_node_insert_hmac(const struct ubifs_info *c, void *buf,
					  int len, int ofs_hmac)
{
	if (ubifs_authenticated(c))
		return __ubifs_node_insert_hmac(c, buf, len, ofs_hmac);
	else
		return 0;
}

int __ubifs_node_verify_hmac(const struct ubifs_info *c, const void *buf,
			     int len, int ofs_hmac);
static inline int ubifs_node_verify_hmac(const struct ubifs_info *c,
					 const void *buf, int len, int ofs_hmac)
{
	if (ubifs_authenticated(c))
		return __ubifs_node_verify_hmac(c, buf, len, ofs_hmac);
	else
		return 0;
}


static inline int ubifs_auth_node_sz(const struct ubifs_info *c)
{
	if (ubifs_authenticated(c))
		return sizeof(struct ubifs_auth_node) + c->hmac_desc_len;
	else
		return 0;
}
int ubifs_sb_verify_signature(struct ubifs_info *c,
			      const struct ubifs_sb_node *sup);
bool ubifs_hmac_zero(struct ubifs_info *c, const u8 *hmac);

int ubifs_hmac_wkm(struct ubifs_info *c, u8 *hmac);

int __ubifs_shash_copy_state(const struct ubifs_info *c, struct shash_desc *src,
			     struct shash_desc *target);
static inline int ubifs_shash_copy_state(const struct ubifs_info *c,
					   struct shash_desc *src,
					   struct shash_desc *target)
{
	if (ubifs_authenticated(c))
		return __ubifs_shash_copy_state(c, src, target);
	else
		return 0;
}


void ubifs_ro_mode(struct ubifs_info *c, int err);
int ubifs_leb_read(const struct ubifs_info *c, int lnum, void *buf, int offs,
		   int len, int even_ebadmsg);
int ubifs_leb_write(struct ubifs_info *c, int lnum, const void *buf, int offs,
		    int len);
int ubifs_leb_change(struct ubifs_info *c, int lnum, const void *buf, int len);
int ubifs_leb_unmap(struct ubifs_info *c, int lnum);
int ubifs_leb_map(struct ubifs_info *c, int lnum);
int ubifs_is_mapped(const struct ubifs_info *c, int lnum);
int ubifs_wbuf_write_nolock(struct ubifs_wbuf *wbuf, void *buf, int len);
int ubifs_wbuf_seek_nolock(struct ubifs_wbuf *wbuf, int lnum, int offs);
int ubifs_wbuf_init(struct ubifs_info *c, struct ubifs_wbuf *wbuf);
int ubifs_read_node(const struct ubifs_info *c, void *buf, int type, int len,
		    int lnum, int offs);
int ubifs_read_node_wbuf(struct ubifs_wbuf *wbuf, void *buf, int type, int len,
			 int lnum, int offs);
int ubifs_write_node(struct ubifs_info *c, void *node, int len, int lnum,
		     int offs);
int ubifs_write_node_hmac(struct ubifs_info *c, void *buf, int len, int lnum,
			  int offs, int hmac_offs);
int ubifs_check_node(const struct ubifs_info *c, const void *buf, int len,
		     int lnum, int offs, int quiet, int must_chk_crc);
void ubifs_init_node(struct ubifs_info *c, void *buf, int len, int pad);
void ubifs_crc_node(struct ubifs_info *c, void *buf, int len);
void ubifs_prepare_node(struct ubifs_info *c, void *buf, int len, int pad);
int ubifs_prepare_node_hmac(struct ubifs_info *c, void *node, int len,
			    int hmac_offs, int pad);
void ubifs_prep_grp_node(struct ubifs_info *c, void *node, int len, int last);
int ubifs_io_init(struct ubifs_info *c);
void ubifs_pad(const struct ubifs_info *c, void *buf, int pad);
int ubifs_wbuf_sync_nolock(struct ubifs_wbuf *wbuf);
int ubifs_bg_wbufs_sync(struct ubifs_info *c);
void ubifs_wbuf_add_ino_nolock(struct ubifs_wbuf *wbuf, ino_t inum);
int ubifs_sync_wbufs_by_inode(struct ubifs_info *c, struct inode *inode);


struct ubifs_scan_leb *ubifs_scan(const struct ubifs_info *c, int lnum,
				  int offs, void *sbuf, int quiet);
void ubifs_scan_destroy(struct ubifs_scan_leb *sleb);
int ubifs_scan_a_node(const struct ubifs_info *c, void *buf, int len, int lnum,
		      int offs, int quiet);
struct ubifs_scan_leb *ubifs_start_scan(const struct ubifs_info *c, int lnum,
					int offs, void *sbuf);
void ubifs_end_scan(const struct ubifs_info *c, struct ubifs_scan_leb *sleb,
		    int lnum, int offs);
int ubifs_add_snod(const struct ubifs_info *c, struct ubifs_scan_leb *sleb,
		   void *buf, int offs);
void ubifs_scanned_corruption(const struct ubifs_info *c, int lnum, int offs,
			      void *buf);


void ubifs_add_bud(struct ubifs_info *c, struct ubifs_bud *bud);
void ubifs_create_buds_lists(struct ubifs_info *c);
int ubifs_add_bud_to_log(struct ubifs_info *c, int jhead, int lnum, int offs);
struct ubifs_bud *ubifs_search_bud(struct ubifs_info *c, int lnum);
struct ubifs_wbuf *ubifs_get_wbuf(struct ubifs_info *c, int lnum);
int ubifs_log_start_commit(struct ubifs_info *c, int *ltail_lnum);
int ubifs_log_end_commit(struct ubifs_info *c, int new_ltail_lnum);
int ubifs_log_post_commit(struct ubifs_info *c, int old_ltail_lnum);
int ubifs_consolidate_log(struct ubifs_info *c);


int ubifs_jnl_update(struct ubifs_info *c, const struct inode *dir,
		     const struct fscrypt_name *nm, const struct inode *inode,
		     int deletion, int xent);
int ubifs_jnl_write_data(struct ubifs_info *c, const struct inode *inode,
			 const union ubifs_key *key, const void *buf, int len);
int ubifs_jnl_write_inode(struct ubifs_info *c, const struct inode *inode);
int ubifs_jnl_delete_inode(struct ubifs_info *c, const struct inode *inode);
int ubifs_jnl_xrename(struct ubifs_info *c, const struct inode *fst_dir,
		      const struct inode *fst_inode,
		      const struct fscrypt_name *fst_nm,
		      const struct inode *snd_dir,
		      const struct inode *snd_inode,
		      const struct fscrypt_name *snd_nm, int sync);
int ubifs_jnl_rename(struct ubifs_info *c, const struct inode *old_dir,
		     const struct inode *old_inode,
		     const struct fscrypt_name *old_nm,
		     const struct inode *new_dir,
		     const struct inode *new_inode,
		     const struct fscrypt_name *new_nm,
		     const struct inode *whiteout, int sync);
int ubifs_jnl_truncate(struct ubifs_info *c, const struct inode *inode,
		       loff_t old_size, loff_t new_size);
int ubifs_jnl_delete_xattr(struct ubifs_info *c, const struct inode *host,
			   const struct inode *inode, const struct fscrypt_name *nm);
int ubifs_jnl_change_xattr(struct ubifs_info *c, const struct inode *inode1,
			   const struct inode *inode2);


int ubifs_budget_space(struct ubifs_info *c, struct ubifs_budget_req *req);
void ubifs_release_budget(struct ubifs_info *c, struct ubifs_budget_req *req);
void ubifs_release_dirty_inode_budget(struct ubifs_info *c,
				      struct ubifs_inode *ui);
int ubifs_budget_inode_op(struct ubifs_info *c, struct inode *inode,
			  struct ubifs_budget_req *req);
void ubifs_release_ino_dirty(struct ubifs_info *c, struct inode *inode,
				struct ubifs_budget_req *req);
void ubifs_cancel_ino_op(struct ubifs_info *c, struct inode *inode,
			 struct ubifs_budget_req *req);
long long ubifs_get_free_space(struct ubifs_info *c);
long long ubifs_get_free_space_nolock(struct ubifs_info *c);
int ubifs_calc_min_idx_lebs(struct ubifs_info *c);
void ubifs_convert_page_budget(struct ubifs_info *c);
long long ubifs_reported_space(const struct ubifs_info *c, long long free);
long long ubifs_calc_available(const struct ubifs_info *c, int min_idx_lebs);


int ubifs_find_free_space(struct ubifs_info *c, int min_space, int *offs,
			  int squeeze);
int ubifs_find_free_leb_for_idx(struct ubifs_info *c);
int ubifs_find_dirty_leb(struct ubifs_info *c, struct ubifs_lprops *ret_lp,
			 int min_space, int pick_free);
int ubifs_find_dirty_idx_leb(struct ubifs_info *c);
int ubifs_save_dirty_idx_lnums(struct ubifs_info *c);


int ubifs_lookup_level0(struct ubifs_info *c, const union ubifs_key *key,
			struct ubifs_znode **zn, int *n);
int ubifs_tnc_lookup_nm(struct ubifs_info *c, const union ubifs_key *key,
			void *node, const struct fscrypt_name *nm);
int ubifs_tnc_lookup_dh(struct ubifs_info *c, const union ubifs_key *key,
			void *node, uint32_t secondary_hash);
int ubifs_tnc_locate(struct ubifs_info *c, const union ubifs_key *key,
		     void *node, int *lnum, int *offs);
int ubifs_tnc_add(struct ubifs_info *c, const union ubifs_key *key, int lnum,
		  int offs, int len, const u8 *hash);
int ubifs_tnc_replace(struct ubifs_info *c, const union ubifs_key *key,
		      int old_lnum, int old_offs, int lnum, int offs, int len);
int ubifs_tnc_add_nm(struct ubifs_info *c, const union ubifs_key *key,
		     int lnum, int offs, int len, const u8 *hash,
		     const struct fscrypt_name *nm);
int ubifs_tnc_remove(struct ubifs_info *c, const union ubifs_key *key);
int ubifs_tnc_remove_nm(struct ubifs_info *c, const union ubifs_key *key,
			const struct fscrypt_name *nm);
int ubifs_tnc_remove_dh(struct ubifs_info *c, const union ubifs_key *key,
			uint32_t cookie);
int ubifs_tnc_remove_range(struct ubifs_info *c, union ubifs_key *from_key,
			   union ubifs_key *to_key);
int ubifs_tnc_remove_ino(struct ubifs_info *c, ino_t inum);
struct ubifs_dent_node *ubifs_tnc_next_ent(struct ubifs_info *c,
					   union ubifs_key *key,
					   const struct fscrypt_name *nm);
void ubifs_tnc_close(struct ubifs_info *c);
int ubifs_tnc_has_node(struct ubifs_info *c, union ubifs_key *key, int level,
		       int lnum, int offs, int is_idx);
int ubifs_dirty_idx_node(struct ubifs_info *c, union ubifs_key *key, int level,
			 int lnum, int offs);

void destroy_old_idx(struct ubifs_info *c);
int is_idx_node_in_tnc(struct ubifs_info *c, union ubifs_key *key, int level,
		       int lnum, int offs);
int insert_old_idx_znode(struct ubifs_info *c, struct ubifs_znode *znode);
int ubifs_tnc_get_bu_keys(struct ubifs_info *c, struct bu_info *bu);
int ubifs_tnc_bulk_read(struct ubifs_info *c, struct bu_info *bu);


struct ubifs_znode *ubifs_tnc_levelorder_next(const struct ubifs_info *c,
					      struct ubifs_znode *zr,
					      struct ubifs_znode *znode);
int ubifs_search_zbranch(const struct ubifs_info *c,
			 const struct ubifs_znode *znode,
			 const union ubifs_key *key, int *n);
struct ubifs_znode *ubifs_tnc_postorder_first(struct ubifs_znode *znode);
struct ubifs_znode *ubifs_tnc_postorder_next(const struct ubifs_info *c,
					     struct ubifs_znode *znode);
long ubifs_destroy_tnc_subtree(const struct ubifs_info *c,
			       struct ubifs_znode *zr);
struct ubifs_znode *ubifs_load_znode(struct ubifs_info *c,
				     struct ubifs_zbranch *zbr,
				     struct ubifs_znode *parent, int iip);
int ubifs_tnc_read_node(struct ubifs_info *c, struct ubifs_zbranch *zbr,
			void *node);


int ubifs_tnc_start_commit(struct ubifs_info *c, struct ubifs_zbranch *zroot);
int ubifs_tnc_end_commit(struct ubifs_info *c);


unsigned long ubifs_shrink_scan(struct shrinker *shrink,
				struct shrink_control *sc);
unsigned long ubifs_shrink_count(struct shrinker *shrink,
				 struct shrink_control *sc);


int ubifs_bg_thread(void *info);
void ubifs_commit_required(struct ubifs_info *c);
void ubifs_request_bg_commit(struct ubifs_info *c);
int ubifs_run_commit(struct ubifs_info *c);
void ubifs_recovery_commit(struct ubifs_info *c);
int ubifs_gc_should_commit(struct ubifs_info *c);
void ubifs_wait_for_commit(struct ubifs_info *c);


int ubifs_compare_master_node(struct ubifs_info *c, void *m1, void *m2);
int ubifs_read_master(struct ubifs_info *c);
int ubifs_write_master(struct ubifs_info *c);


int ubifs_read_superblock(struct ubifs_info *c);
int ubifs_write_sb_node(struct ubifs_info *c, struct ubifs_sb_node *sup);
int ubifs_fixup_free_space(struct ubifs_info *c);
int ubifs_enable_encryption(struct ubifs_info *c);


int ubifs_validate_entry(struct ubifs_info *c,
			 const struct ubifs_dent_node *dent);
int ubifs_replay_journal(struct ubifs_info *c);


int ubifs_garbage_collect(struct ubifs_info *c, int anyway);
int ubifs_gc_start_commit(struct ubifs_info *c);
int ubifs_gc_end_commit(struct ubifs_info *c);
void ubifs_destroy_idx_gc(struct ubifs_info *c);
int ubifs_get_idx_gc_leb(struct ubifs_info *c);
int ubifs_garbage_collect_leb(struct ubifs_info *c, struct ubifs_lprops *lp);


int ubifs_add_orphan(struct ubifs_info *c, ino_t inum);
void ubifs_delete_orphan(struct ubifs_info *c, ino_t inum);
int ubifs_orphan_start_commit(struct ubifs_info *c);
int ubifs_orphan_end_commit(struct ubifs_info *c);
int ubifs_mount_orphans(struct ubifs_info *c, int unclean, int read_only);
int ubifs_clear_orphans(struct ubifs_info *c);


int ubifs_calc_lpt_geom(struct ubifs_info *c);
int ubifs_create_dflt_lpt(struct ubifs_info *c, int *main_lebs, int lpt_first,
			  int *lpt_lebs, int *big_lpt, u8 *hash);
int ubifs_lpt_init(struct ubifs_info *c, int rd, int wr);
struct ubifs_lprops *ubifs_lpt_lookup(struct ubifs_info *c, int lnum);
struct ubifs_lprops *ubifs_lpt_lookup_dirty(struct ubifs_info *c, int lnum);
int ubifs_lpt_scan_nolock(struct ubifs_info *c, int start_lnum, int end_lnum,
			  ubifs_lpt_scan_callback scan_cb, void *data);


void ubifs_pack_lsave(struct ubifs_info *c, void *buf, int *lsave);
void ubifs_pack_ltab(struct ubifs_info *c, void *buf,
		     struct ubifs_lpt_lprops *ltab);
void ubifs_pack_pnode(struct ubifs_info *c, void *buf,
		      struct ubifs_pnode *pnode);
void ubifs_pack_nnode(struct ubifs_info *c, void *buf,
		      struct ubifs_nnode *nnode);
struct ubifs_pnode *ubifs_get_pnode(struct ubifs_info *c,
				    struct ubifs_nnode *parent, int iip);
struct ubifs_nnode *ubifs_get_nnode(struct ubifs_info *c,
				    struct ubifs_nnode *parent, int iip);
struct ubifs_pnode *ubifs_pnode_lookup(struct ubifs_info *c, int i);
int ubifs_read_nnode(struct ubifs_info *c, struct ubifs_nnode *parent, int iip);
void ubifs_add_lpt_dirt(struct ubifs_info *c, int lnum, int dirty);
void ubifs_add_nnode_dirt(struct ubifs_info *c, struct ubifs_nnode *nnode);
uint32_t ubifs_unpack_bits(const struct ubifs_info *c, uint8_t **addr, int *pos, int nrbits);
struct ubifs_nnode *ubifs_first_nnode(struct ubifs_info *c, int *hght);

int ubifs_unpack_nnode(const struct ubifs_info *c, void *buf,
		       struct ubifs_nnode *nnode);
int ubifs_lpt_calc_hash(struct ubifs_info *c, u8 *hash);


int ubifs_lpt_start_commit(struct ubifs_info *c);
int ubifs_lpt_end_commit(struct ubifs_info *c);
int ubifs_lpt_post_commit(struct ubifs_info *c);
void ubifs_lpt_free(struct ubifs_info *c, int wr_only);


const struct ubifs_lprops *ubifs_change_lp(struct ubifs_info *c,
					   const struct ubifs_lprops *lp,
					   int free, int dirty, int flags,
					   int idx_gc_cnt);
void ubifs_get_lp_stats(struct ubifs_info *c, struct ubifs_lp_stats *lst);
void ubifs_add_to_cat(struct ubifs_info *c, struct ubifs_lprops *lprops,
		      int cat);
void ubifs_replace_cat(struct ubifs_info *c, struct ubifs_lprops *old_lprops,
		       struct ubifs_lprops *new_lprops);
void ubifs_ensure_cat(struct ubifs_info *c, struct ubifs_lprops *lprops);
int ubifs_categorize_lprops(const struct ubifs_info *c,
			    const struct ubifs_lprops *lprops);
int ubifs_change_one_lp(struct ubifs_info *c, int lnum, int free, int dirty,
			int flags_set, int flags_clean, int idx_gc_cnt);
int ubifs_update_one_lp(struct ubifs_info *c, int lnum, int free, int dirty,
			int flags_set, int flags_clean);
int ubifs_read_one_lp(struct ubifs_info *c, int lnum, struct ubifs_lprops *lp);
const struct ubifs_lprops *ubifs_fast_find_free(struct ubifs_info *c);
const struct ubifs_lprops *ubifs_fast_find_empty(struct ubifs_info *c);
const struct ubifs_lprops *ubifs_fast_find_freeable(struct ubifs_info *c);
const struct ubifs_lprops *ubifs_fast_find_frdi_idx(struct ubifs_info *c);
int ubifs_calc_dark(const struct ubifs_info *c, int spc);


int ubifs_fsync(struct file *file, loff_t start, loff_t end, int datasync);
int ubifs_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
		  struct iattr *attr);
int ubifs_update_time(struct inode *inode, int flags);


struct inode *ubifs_new_inode(struct ubifs_info *c, struct inode *dir,
			      umode_t mode, bool is_xattr);
int ubifs_getattr(struct mnt_idmap *idmap, const struct path *path,
		  struct kstat *stat, u32 request_mask, unsigned int flags);
int ubifs_check_dir_empty(struct inode *dir);


int ubifs_xattr_set(struct inode *host, const char *name, const void *value,
		    size_t size, int flags, bool check_lock);
ssize_t ubifs_xattr_get(struct inode *host, const char *name, void *buf,
			size_t size);

#ifdef CONFIG_UBIFS_FS_XATTR
extern const struct xattr_handler *ubifs_xattr_handlers[];
ssize_t ubifs_listxattr(struct dentry *dentry, char *buffer, size_t size);
void ubifs_evict_xattr_inode(struct ubifs_info *c, ino_t xattr_inum);
int ubifs_purge_xattrs(struct inode *host);
#else
#define ubifs_listxattr NULL
#define ubifs_xattr_handlers NULL
static inline void ubifs_evict_xattr_inode(struct ubifs_info *c,
					   ino_t xattr_inum) { }
static inline int ubifs_purge_xattrs(struct inode *host)
{
	return 0;
}
#endif

#ifdef CONFIG_UBIFS_FS_SECURITY
extern int ubifs_init_security(struct inode *dentry, struct inode *inode,
			const struct qstr *qstr);
#else
static inline int ubifs_init_security(struct inode *dentry,
			struct inode *inode, const struct qstr *qstr)
{
	return 0;
}
#endif



struct inode *ubifs_iget(struct super_block *sb, unsigned long inum);


int ubifs_recover_master_node(struct ubifs_info *c);
int ubifs_write_rcvrd_mst_node(struct ubifs_info *c);
struct ubifs_scan_leb *ubifs_recover_leb(struct ubifs_info *c, int lnum,
					 int offs, void *sbuf, int jhead);
struct ubifs_scan_leb *ubifs_recover_log_leb(struct ubifs_info *c, int lnum,
					     int offs, void *sbuf);
int ubifs_recover_inl_heads(struct ubifs_info *c, void *sbuf);
int ubifs_clean_lebs(struct ubifs_info *c, void *sbuf);
int ubifs_rcvry_gc_commit(struct ubifs_info *c);
int ubifs_recover_size_accum(struct ubifs_info *c, union ubifs_key *key,
			     int deletion, loff_t new_size);
int ubifs_recover_size(struct ubifs_info *c, bool in_place);
void ubifs_destroy_size_tree(struct ubifs_info *c);


int ubifs_fileattr_get(struct dentry *dentry, struct fileattr *fa);
int ubifs_fileattr_set(struct mnt_idmap *idmap,
		       struct dentry *dentry, struct fileattr *fa);
long ubifs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
void ubifs_set_inode_flags(struct inode *inode);
#ifdef CONFIG_COMPAT
long ubifs_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#endif


int __init ubifs_compressors_init(void);
void ubifs_compressors_exit(void);
void ubifs_compress(const struct ubifs_info *c, const void *in_buf, int in_len,
		    void *out_buf, int *out_len, int *compr_type);
int ubifs_decompress(const struct ubifs_info *c, const void *buf, int len,
		     void *out, int *out_len, int compr_type);


int ubifs_sysfs_init(void);
void ubifs_sysfs_exit(void);
int ubifs_sysfs_register(struct ubifs_info *c);
void ubifs_sysfs_unregister(struct ubifs_info *c);

#include "debug.h"
#include "misc.h"
#include "key.h"

#ifndef CONFIG_FS_ENCRYPTION
static inline int ubifs_encrypt(const struct inode *inode,
				struct ubifs_data_node *dn,
				unsigned int in_len, unsigned int *out_len,
				int block)
{
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	ubifs_assert(c, 0);
	return -EOPNOTSUPP;
}
static inline int ubifs_decrypt(const struct inode *inode,
				struct ubifs_data_node *dn,
				unsigned int *out_len, int block)
{
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	ubifs_assert(c, 0);
	return -EOPNOTSUPP;
}
#else

int ubifs_encrypt(const struct inode *inode, struct ubifs_data_node *dn,
		  unsigned int in_len, unsigned int *out_len, int block);
int ubifs_decrypt(const struct inode *inode, struct ubifs_data_node *dn,
		  unsigned int *out_len, int block);
#endif

extern const struct fscrypt_operations ubifs_crypt_operations;


__printf(2, 3)
void ubifs_msg(const struct ubifs_info *c, const char *fmt, ...);
__printf(2, 3)
void ubifs_err(const struct ubifs_info *c, const char *fmt, ...);
__printf(2, 3)
void ubifs_warn(const struct ubifs_info *c, const char *fmt, ...);

#define ubifs_errc(c, fmt, ...)						\
do {									\
	if (!(c)->probing)						\
		ubifs_err(c, fmt, ##__VA_ARGS__);			\
} while (0)

#endif 
