


#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/filelock.h>
#include <linux/pagemap.h>
#include <linux/rxrpc.h>
#include <linux/key.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/fscache.h>
#include <linux/backing-dev.h>
#include <linux/uuid.h>
#include <linux/mm_types.h>
#include <linux/dns_resolver.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/sock.h>
#include <net/af_rxrpc.h>

#include "afs.h"
#include "afs_vl.h"

#define AFS_CELL_MAX_ADDRS 15

struct pagevec;
struct afs_call;
struct afs_vnode;


enum afs_flock_mode {
	afs_flock_mode_unset,
	afs_flock_mode_local,	
	afs_flock_mode_openafs,	
	afs_flock_mode_strict,	
	afs_flock_mode_write,	
};

struct afs_fs_context {
	bool			force;		
	bool			autocell;	
	bool			dyn_root;	
	bool			no_cell;	
	enum afs_flock_mode	flock_mode;	
	afs_voltype_t		type;		
	unsigned int		volnamesz;	
	const char		*volname;	
	struct afs_net		*net;		
	struct afs_cell		*cell;		
	struct afs_volume	*volume;	
	struct key		*key;		
};

enum afs_call_state {
	AFS_CALL_CL_REQUESTING,		
	AFS_CALL_CL_AWAIT_REPLY,	
	AFS_CALL_CL_PROC_REPLY,		
	AFS_CALL_SV_AWAIT_OP_ID,	
	AFS_CALL_SV_AWAIT_REQUEST,	
	AFS_CALL_SV_REPLYING,		
	AFS_CALL_SV_AWAIT_ACK,		
	AFS_CALL_COMPLETE,		
};


struct afs_addr_list {
	struct rcu_head		rcu;
	refcount_t		usage;
	u32			version;	
	unsigned char		max_addrs;
	unsigned char		nr_addrs;
	unsigned char		preferred;	
	unsigned char		nr_ipv4;	
	enum dns_record_source	source:8;
	enum dns_lookup_status	status:8;
	unsigned long		failed;		
	unsigned long		responded;	
	struct sockaddr_rxrpc	addrs[];
#define AFS_MAX_ADDRESSES ((unsigned int)(sizeof(unsigned long) * 8))
};


struct afs_call {
	const struct afs_call_type *type;	
	struct afs_addr_list	*alist;		
	wait_queue_head_t	waitq;		
	struct work_struct	async_work;	
	struct work_struct	work;		
	struct rxrpc_call	*rxcall;	
	struct key		*key;		
	struct afs_net		*net;		
	struct afs_server	*server;	
	struct afs_vlserver	*vlserver;	
	void			*request;	
	size_t			iov_len;	
	struct iov_iter		def_iter;	
	struct iov_iter		*write_iter;	
	struct iov_iter		*iter;		
	union {	
		struct kvec	kvec[1];
		struct bio_vec	bvec[1];
	};
	void			*buffer;	
	union {
		long			ret0;	
		struct afs_addr_list	*ret_alist;
		struct afs_vldb_entry	*ret_vldb;
		char			*ret_str;
	};
	struct afs_operation	*op;
	unsigned int		server_index;
	refcount_t		ref;
	enum afs_call_state	state;
	spinlock_t		state_lock;
	int			error;		
	u32			abort_code;	
	unsigned int		max_lifespan;	
	unsigned		request_size;	
	unsigned		reply_max;	
	unsigned		count2;		
	unsigned char		unmarshall;	
	unsigned char		addr_ix;	
	bool			drop_ref;	
	bool			need_attention;	
	bool			async;		
	bool			upgrade;	
	bool			intr;		
	bool			unmarshalling_error; 
	u16			service_id;	
	unsigned int		debug_id;	
	u32			operation_ID;	
	u32			count;		
	union {					
		struct {
			__be32	tmp_u;
			__be32	tmp;
		} __attribute__((packed));
		__be64		tmp64;
	};
	ktime_t			issue_time;	
};

struct afs_call_type {
	const char *name;
	unsigned int op; 

	
	int (*deliver)(struct afs_call *call);

	
	void (*destructor)(struct afs_call *call);

	
	void (*work)(struct work_struct *work);

	
	void (*done)(struct afs_call *call);
};


struct afs_wb_key {
	refcount_t		usage;
	struct key		*key;
	struct list_head	vnode_link;	
};


struct afs_file {
	struct key		*key;		
	struct afs_wb_key	*wb;		
};

static inline struct key *afs_file_key(struct file *file)
{
	struct afs_file *af = file->private_data;

	return af->key;
}


struct afs_read {
	loff_t			pos;		
	loff_t			len;		
	loff_t			actual_len;	
	loff_t			file_size;	
	struct key		*key;		
	struct afs_vnode	*vnode;		
	struct netfs_io_subrequest *subreq;	
	afs_dataversion_t	data_version;	
	refcount_t		usage;
	unsigned int		call_debug_id;
	unsigned int		nr_pages;
	int			error;
	void (*done)(struct afs_read *);
	void (*cleanup)(struct afs_read *);
	struct iov_iter		*iter;		
	struct iov_iter		def_iter;	
};


struct afs_super_info {
	struct net		*net_ns;	
	struct afs_cell		*cell;		
	struct afs_volume	*volume;	
	enum afs_flock_mode	flock_mode:8;	
	bool			dyn_root;	
};

static inline struct afs_super_info *AFS_FS_S(struct super_block *sb)
{
	return sb->s_fs_info;
}

extern struct file_system_type afs_fs_type;


struct afs_sysnames {
#define AFS_NR_SYSNAME 16
	char			*subs[AFS_NR_SYSNAME];
	refcount_t		usage;
	unsigned short		nr;
	char			blank[1];
};


struct afs_net {
	struct net		*net;		
	struct afs_uuid		uuid;
	bool			live;		

	
	struct socket		*socket;
	struct afs_call		*spare_incoming_call;
	struct work_struct	charge_preallocation_work;
	struct mutex		socket_mutex;
	atomic_t		nr_outstanding_calls;
	atomic_t		nr_superblocks;

	
	struct rb_root		cells;
	struct afs_cell		*ws_cell;
	struct work_struct	cells_manager;
	struct timer_list	cells_timer;
	atomic_t		cells_outstanding;
	struct rw_semaphore	cells_lock;
	struct mutex		cells_alias_lock;

	struct mutex		proc_cells_lock;
	struct hlist_head	proc_cells;

	
	seqlock_t		fs_lock;	
	struct rb_root		fs_servers;	
	struct list_head	fs_probe_fast;	
	struct list_head	fs_probe_slow;	
	struct hlist_head	fs_proc;	

	struct hlist_head	fs_addresses4;	
	struct hlist_head	fs_addresses6;	
	seqlock_t		fs_addr_lock;	

	struct work_struct	fs_manager;
	struct timer_list	fs_timer;

	struct work_struct	fs_prober;
	struct timer_list	fs_probe_timer;
	atomic_t		servers_outstanding;

	
	struct mutex		lock_manager_mutex;

	
	struct super_block	*dynroot_sb;	
	struct proc_dir_entry	*proc_afs;	
	struct afs_sysnames	*sysnames;
	rwlock_t		sysnames_lock;

	
	atomic_t		n_lookup;	
	atomic_t		n_reval;	
	atomic_t		n_inval;	
	atomic_t		n_relpg;	
	atomic_t		n_read_dir;	
	atomic_t		n_dir_cr;	
	atomic_t		n_dir_rm;	
	atomic_t		n_stores;	
	atomic_long_t		n_store_bytes;	
	atomic_long_t		n_fetch_bytes;	
	atomic_t		n_fetches;	
};

extern const char afs_init_sysname[];

enum afs_cell_state {
	AFS_CELL_UNSET,
	AFS_CELL_ACTIVATING,
	AFS_CELL_ACTIVE,
	AFS_CELL_DEACTIVATING,
	AFS_CELL_INACTIVE,
	AFS_CELL_FAILED,
	AFS_CELL_REMOVED,
};


struct afs_cell {
	union {
		struct rcu_head	rcu;
		struct rb_node	net_node;	
	};
	struct afs_net		*net;
	struct afs_cell		*alias_of;	
	struct afs_volume	*root_volume;	
	struct key		*anonymous_key;	
	struct work_struct	manager;	
	struct hlist_node	proc_link;	
	time64_t		dns_expiry;	
	time64_t		last_inactive;	
	refcount_t		ref;		
	atomic_t		active;		
	unsigned long		flags;
#define AFS_CELL_FL_NO_GC	0		
#define AFS_CELL_FL_DO_LOOKUP	1		
#define AFS_CELL_FL_CHECK_ALIAS	2		
	enum afs_cell_state	state;
	short			error;
	enum dns_record_source	dns_source:8;	
	enum dns_lookup_status	dns_status:8;	
	unsigned int		dns_lookup_count; 
	unsigned int		debug_id;

	
	struct rb_root		volumes;	
	struct hlist_head	proc_volumes;	
	seqlock_t		volume_lock;	

	
	struct rb_root		fs_servers;	
	seqlock_t		fs_lock;	
	struct rw_semaphore	fs_open_mmaps_lock;
	struct list_head	fs_open_mmaps;	
	atomic_t		fs_s_break;	

	
	rwlock_t		vl_servers_lock; 
	struct afs_vlserver_list __rcu *vl_servers;

	u8			name_len;	
	char			*name;		
};


struct afs_vlserver {
	struct rcu_head		rcu;
	struct afs_addr_list	__rcu *addresses; 
	unsigned long		flags;
#define AFS_VLSERVER_FL_PROBED	0		
#define AFS_VLSERVER_FL_PROBING	1		
#define AFS_VLSERVER_FL_IS_YFS	2		
#define AFS_VLSERVER_FL_RESPONDING 3		
	rwlock_t		lock;		
	refcount_t		ref;
	unsigned int		rtt;		

	
	wait_queue_head_t	probe_wq;
	atomic_t		probe_outstanding;
	spinlock_t		probe_lock;
	struct {
		unsigned int	rtt;		
		u32		abort_code;
		short		error;
		unsigned short	flags;
#define AFS_VLSERVER_PROBE_RESPONDED		0x01 
#define AFS_VLSERVER_PROBE_IS_YFS		0x02 
#define AFS_VLSERVER_PROBE_NOT_YFS		0x04 
#define AFS_VLSERVER_PROBE_LOCAL_FAILURE	0x08 
	} probe;

	u16			port;
	u16			name_len;	
	char			name[];		
};


struct afs_vlserver_entry {
	u16			priority;	
	u16			weight;		
	enum dns_record_source	source:8;
	enum dns_lookup_status	status:8;
	struct afs_vlserver	*server;
};

struct afs_vlserver_list {
	struct rcu_head		rcu;
	refcount_t		ref;
	u8			nr_servers;
	u8			index;		
	u8			preferred;	
	enum dns_record_source	source:8;
	enum dns_lookup_status	status:8;
	rwlock_t		lock;
	struct afs_vlserver_entry servers[];
};


struct afs_vldb_entry {
	afs_volid_t		vid[3];		

	unsigned long		flags;
#define AFS_VLDB_HAS_RW		0		
#define AFS_VLDB_HAS_RO		1		
#define AFS_VLDB_HAS_BAK	2		
#define AFS_VLDB_QUERY_VALID	3		
#define AFS_VLDB_QUERY_ERROR	4		

	uuid_t			fs_server[AFS_NMAXNSERVERS];
	u32			addr_version[AFS_NMAXNSERVERS]; 
	u8			fs_mask[AFS_NMAXNSERVERS];
#define AFS_VOL_VTM_RW	0x01 
#define AFS_VOL_VTM_RO	0x02 
#define AFS_VOL_VTM_BAK	0x04 
	short			error;
	u8			nr_servers;	
	u8			name_len;
	u8			name[AFS_MAXVOLNAME + 1]; 
};


struct afs_server {
	struct rcu_head		rcu;
	union {
		uuid_t		uuid;		
		struct afs_uuid	_uuid;
	};

	struct afs_addr_list	__rcu *addresses;
	struct afs_cell		*cell;		
	struct rb_node		uuid_rb;	
	struct afs_server __rcu	*uuid_next;	
	struct afs_server	*uuid_prev;	
	struct list_head	probe_link;	
	struct hlist_node	addr4_link;	
	struct hlist_node	addr6_link;	
	struct hlist_node	proc_link;	
	struct work_struct	initcb_work;	
	struct afs_server	*gc_next;	
	time64_t		unuse_time;	
	unsigned long		flags;
#define AFS_SERVER_FL_RESPONDING 0		
#define AFS_SERVER_FL_UPDATING	1
#define AFS_SERVER_FL_NEEDS_UPDATE 2		
#define AFS_SERVER_FL_NOT_READY	4		
#define AFS_SERVER_FL_NOT_FOUND	5		
#define AFS_SERVER_FL_VL_FAIL	6		
#define AFS_SERVER_FL_MAY_HAVE_CB 8		
#define AFS_SERVER_FL_IS_YFS	16		
#define AFS_SERVER_FL_NO_IBULK	17		
#define AFS_SERVER_FL_NO_RM2	18		
#define AFS_SERVER_FL_HAS_FS64	19		
	refcount_t		ref;		
	atomic_t		active;		
	u32			addr_version;	
	unsigned int		rtt;		
	unsigned int		debug_id;	

	
	rwlock_t		fs_lock;	

	
	unsigned		cb_s_break;	

	
	unsigned long		probed_at;	
	wait_queue_head_t	probe_wq;
	atomic_t		probe_outstanding;
	spinlock_t		probe_lock;
	struct {
		unsigned int	rtt;		
		u32		abort_code;
		short		error;
		bool		responded:1;
		bool		is_yfs:1;
		bool		not_yfs:1;
		bool		local_failure:1;
	} probe;
};


struct afs_server_entry {
	struct afs_server	*server;
};

struct afs_server_list {
	struct rcu_head		rcu;
	afs_volid_t		vids[AFS_MAXTYPES]; 
	refcount_t		usage;
	unsigned char		nr_servers;
	unsigned char		preferred;	
	unsigned short		vnovol_mask;	
	unsigned int		seq;		
	rwlock_t		lock;
	struct afs_server_entry	servers[];
};


struct afs_volume {
	union {
		struct rcu_head	rcu;
		afs_volid_t	vid;		
	};
	refcount_t		ref;
	time64_t		update_at;	
	struct afs_cell		*cell;		
	struct rb_node		cell_node;	
	struct hlist_node	proc_link;	
	struct super_block __rcu *sb;		
	unsigned long		flags;
#define AFS_VOLUME_NEEDS_UPDATE	0	
#define AFS_VOLUME_UPDATING	1	
#define AFS_VOLUME_WAIT		2	
#define AFS_VOLUME_DELETED	3	
#define AFS_VOLUME_OFFLINE	4	
#define AFS_VOLUME_BUSY		5	
#define AFS_VOLUME_MAYBE_NO_IBULK 6	
#define AFS_VOLUME_RM_TREE	7	
#ifdef CONFIG_AFS_FSCACHE
	struct fscache_volume	*cache;		
#endif
	struct afs_server_list __rcu *servers;	
	rwlock_t		servers_lock;	
	unsigned int		servers_seq;	

	unsigned		cb_v_break;	
	rwlock_t		cb_v_break_lock;

	afs_voltype_t		type;		
	char			type_force;	
	u8			name_len;
	u8			name[AFS_MAXVOLNAME + 1]; 
};

enum afs_lock_state {
	AFS_VNODE_LOCK_NONE,		
	AFS_VNODE_LOCK_WAITING_FOR_CB,	
	AFS_VNODE_LOCK_SETTING,		
	AFS_VNODE_LOCK_GRANTED,		
	AFS_VNODE_LOCK_EXTENDING,	
	AFS_VNODE_LOCK_NEED_UNLOCK,	
	AFS_VNODE_LOCK_UNLOCKING,	
	AFS_VNODE_LOCK_DELETED,		
};


struct afs_vnode {
	struct netfs_inode	netfs;		
	struct afs_volume	*volume;	
	struct afs_fid		fid;		
	struct afs_file_status	status;		
	afs_dataversion_t	invalid_before;	
	struct afs_permits __rcu *permit_cache;	
	struct mutex		io_lock;	
	struct rw_semaphore	validate_lock;	
	struct rw_semaphore	rmdir_lock;	
	struct key		*silly_key;	
	spinlock_t		wb_lock;	
	spinlock_t		lock;		
	unsigned long		flags;
#define AFS_VNODE_CB_PROMISED	0		
#define AFS_VNODE_UNSET		1		
#define AFS_VNODE_DIR_VALID	2		
#define AFS_VNODE_ZAP_DATA	3		
#define AFS_VNODE_DELETED	4		
#define AFS_VNODE_MOUNTPOINT	5		
#define AFS_VNODE_AUTOCELL	6		
#define AFS_VNODE_PSEUDODIR	7 		
#define AFS_VNODE_NEW_CONTENT	8		
#define AFS_VNODE_SILLY_DELETED	9		
#define AFS_VNODE_MODIFYING	10		

	struct list_head	wb_keys;	
	struct list_head	pending_locks;	
	struct list_head	granted_locks;	
	struct delayed_work	lock_work;	
	struct key		*lock_key;	
	ktime_t			locked_at;	
	enum afs_lock_state	lock_state : 8;
	afs_lock_type_t		lock_type : 8;

	
	struct work_struct	cb_work;	
	struct list_head	cb_mmap_link;	
	void			*cb_server;	
	atomic_t		cb_nr_mmap;	
	unsigned int		cb_fs_s_break;	
	unsigned int		cb_s_break;	
	unsigned int		cb_v_break;	
	unsigned int		cb_break;	
	seqlock_t		cb_lock;	

	time64_t		cb_expires_at;	
};

static inline struct fscache_cookie *afs_vnode_cache(struct afs_vnode *vnode)
{
#ifdef CONFIG_AFS_FSCACHE
	return netfs_i_cookie(&vnode->netfs);
#else
	return NULL;
#endif
}

static inline void afs_vnode_set_cache(struct afs_vnode *vnode,
				       struct fscache_cookie *cookie)
{
#ifdef CONFIG_AFS_FSCACHE
	vnode->netfs.cache = cookie;
	if (cookie)
		mapping_set_release_always(vnode->netfs.inode.i_mapping);
#endif
}


struct afs_permit {
	struct key		*key;		
	afs_access_t		access;		
};


struct afs_permits {
	struct rcu_head		rcu;
	struct hlist_node	hash_node;	
	unsigned long		h;		
	refcount_t		usage;
	unsigned short		nr_permits;	
	bool			invalidated;	
	struct afs_permit	permits[];	
};


struct afs_error {
	short	error;			
	bool	responded;		
};


struct afs_addr_cursor {
	struct afs_addr_list	*alist;		
	unsigned long		tried;		
	signed char		index;		
	bool			responded;	
	unsigned short		nr_iterations;	
	short			error;
	u32			abort_code;
};


struct afs_vl_cursor {
	struct afs_addr_cursor	ac;
	struct afs_cell		*cell;		
	struct afs_vlserver_list *server_list;	
	struct afs_vlserver	*server;	
	struct key		*key;		
	unsigned long		untried;	
	short			index;		
	short			error;
	unsigned short		flags;
#define AFS_VL_CURSOR_STOP	0x0001		
#define AFS_VL_CURSOR_RETRY	0x0002		
#define AFS_VL_CURSOR_RETRIED	0x0004		
	unsigned short		nr_iterations;	
};


struct afs_operation_ops {
	void (*issue_afs_rpc)(struct afs_operation *op);
	void (*issue_yfs_rpc)(struct afs_operation *op);
	void (*success)(struct afs_operation *op);
	void (*aborted)(struct afs_operation *op);
	void (*failed)(struct afs_operation *op);
	void (*edit_dir)(struct afs_operation *op);
	void (*put)(struct afs_operation *op);
};

struct afs_vnode_param {
	struct afs_vnode	*vnode;
	struct afs_fid		fid;		
	struct afs_status_cb	scb;		
	afs_dataversion_t	dv_before;	
	unsigned int		cb_break_before; 
	u8			dv_delta;	
	bool			put_vnode:1;	
	bool			need_io_lock:1;	
	bool			update_ctime:1;	
	bool			set_size:1;	
	bool			op_unlinked:1;	
	bool			speculative:1;	
	bool			modification:1;	
};


struct afs_operation {
	struct afs_net		*net;		
	struct key		*key;		
	const struct afs_call_type *type;	
	const struct afs_operation_ops *ops;

	
	struct afs_volume	*volume;	
	struct afs_vnode_param	file[2];
	struct afs_vnode_param	*more_files;
	struct afs_volsync	volsync;
	struct dentry		*dentry;	
	struct dentry		*dentry_2;	
	struct timespec64	mtime;		
	struct timespec64	ctime;		
	short			nr_files;	
	short			error;
	unsigned int		debug_id;

	unsigned int		cb_v_break;	
	unsigned int		cb_s_break;	

	union {
		struct {
			int	which;		
		} fetch_status;
		struct {
			int	reason;		
			mode_t	mode;
			const char *symlink;
		} create;
		struct {
			bool	need_rehash;
		} unlink;
		struct {
			struct dentry *rehash;
			struct dentry *tmp;
			bool	new_negative;
		} rename;
		struct {
			struct afs_read *req;
		} fetch;
		struct {
			afs_lock_type_t type;
		} lock;
		struct {
			struct iov_iter	*write_iter;
			loff_t	pos;
			loff_t	size;
			loff_t	i_size;
			bool	laundering;	
		} store;
		struct {
			struct iattr	*attr;
			loff_t		old_i_size;
		} setattr;
		struct afs_acl	*acl;
		struct yfs_acl	*yacl;
		struct {
			struct afs_volume_status vs;
			struct kstatfs		*buf;
		} volstatus;
	};

	
	struct afs_addr_cursor	ac;
	struct afs_server_list	*server_list;	
	struct afs_server	*server;	
	struct afs_call		*call;
	unsigned long		untried;	
	short			index;		
	unsigned short		nr_iterations;	

	unsigned int		flags;
#define AFS_OPERATION_STOP		0x0001	
#define AFS_OPERATION_VBUSY		0x0002	
#define AFS_OPERATION_VMOVED		0x0004	
#define AFS_OPERATION_VNOVOL		0x0008	
#define AFS_OPERATION_CUR_ONLY		0x0010	
#define AFS_OPERATION_NO_VSLEEP		0x0020	
#define AFS_OPERATION_UNINTR		0x0040	
#define AFS_OPERATION_DOWNGRADE		0x0080	
#define AFS_OPERATION_LOCK_0		0x0100	
#define AFS_OPERATION_LOCK_1		0x0200	
#define AFS_OPERATION_TRIED_ALL		0x0400	
#define AFS_OPERATION_RETRY_SERVER	0x0800	
#define AFS_OPERATION_DIR_CONFLICT	0x1000	
};


struct afs_vnode_cache_aux {
	__be64			data_version;
} __packed;

static inline void afs_set_cache_aux(struct afs_vnode *vnode,
				     struct afs_vnode_cache_aux *aux)
{
	aux->data_version = cpu_to_be64(vnode->status.data_version);
}

static inline void afs_invalidate_cache(struct afs_vnode *vnode, unsigned int flags)
{
	struct afs_vnode_cache_aux aux;

	afs_set_cache_aux(vnode, &aux);
	fscache_invalidate(afs_vnode_cache(vnode), &aux,
			   i_size_read(&vnode->netfs.inode), flags);
}


#ifdef CONFIG_64BIT
#define __AFS_FOLIO_PRIV_MASK		0x7fffffffUL
#define __AFS_FOLIO_PRIV_SHIFT		32
#define __AFS_FOLIO_PRIV_MMAPPED	0x80000000UL
#else
#define __AFS_FOLIO_PRIV_MASK		0x7fffUL
#define __AFS_FOLIO_PRIV_SHIFT		16
#define __AFS_FOLIO_PRIV_MMAPPED	0x8000UL
#endif

static inline unsigned int afs_folio_dirty_resolution(struct folio *folio)
{
	int shift = folio_shift(folio) - (__AFS_FOLIO_PRIV_SHIFT - 1);
	return (shift > 0) ? shift : 0;
}

static inline size_t afs_folio_dirty_from(struct folio *folio, unsigned long priv)
{
	unsigned long x = priv & __AFS_FOLIO_PRIV_MASK;

	
	return x << afs_folio_dirty_resolution(folio);
}

static inline size_t afs_folio_dirty_to(struct folio *folio, unsigned long priv)
{
	unsigned long x = (priv >> __AFS_FOLIO_PRIV_SHIFT) & __AFS_FOLIO_PRIV_MASK;

	
	return (x + 1) << afs_folio_dirty_resolution(folio);
}

static inline unsigned long afs_folio_dirty(struct folio *folio, size_t from, size_t to)
{
	unsigned int res = afs_folio_dirty_resolution(folio);
	from >>= res;
	to = (to - 1) >> res;
	return (to << __AFS_FOLIO_PRIV_SHIFT) | from;
}

static inline unsigned long afs_folio_dirty_mmapped(unsigned long priv)
{
	return priv | __AFS_FOLIO_PRIV_MMAPPED;
}

static inline bool afs_is_folio_dirty_mmapped(unsigned long priv)
{
	return priv & __AFS_FOLIO_PRIV_MMAPPED;
}

#include <trace/events/afs.h>



static inline struct afs_addr_list *afs_get_addrlist(struct afs_addr_list *alist)
{
	if (alist)
		refcount_inc(&alist->usage);
	return alist;
}
extern struct afs_addr_list *afs_alloc_addrlist(unsigned int,
						unsigned short,
						unsigned short);
extern void afs_put_addrlist(struct afs_addr_list *);
extern struct afs_vlserver_list *afs_parse_text_addrs(struct afs_net *,
						      const char *, size_t, char,
						      unsigned short, unsigned short);
extern struct afs_vlserver_list *afs_dns_query(struct afs_cell *, time64_t *);
extern bool afs_iterate_addresses(struct afs_addr_cursor *);
extern int afs_end_cursor(struct afs_addr_cursor *);

extern void afs_merge_fs_addr4(struct afs_addr_list *, __be32, u16);
extern void afs_merge_fs_addr6(struct afs_addr_list *, __be32 *, u16);


extern void afs_invalidate_mmap_work(struct work_struct *);
extern void afs_server_init_callback_work(struct work_struct *work);
extern void afs_init_callback_state(struct afs_server *);
extern void __afs_break_callback(struct afs_vnode *, enum afs_cb_break_reason);
extern void afs_break_callback(struct afs_vnode *, enum afs_cb_break_reason);
extern void afs_break_callbacks(struct afs_server *, size_t, struct afs_callback_break *);

static inline unsigned int afs_calc_vnode_cb_break(struct afs_vnode *vnode)
{
	return vnode->cb_break + vnode->cb_v_break;
}

static inline bool afs_cb_is_broken(unsigned int cb_break,
				    const struct afs_vnode *vnode)
{
	return cb_break != (vnode->cb_break + vnode->volume->cb_v_break);
}


extern int afs_cell_init(struct afs_net *, const char *);
extern struct afs_cell *afs_find_cell(struct afs_net *, const char *, unsigned,
				      enum afs_cell_trace);
extern struct afs_cell *afs_lookup_cell(struct afs_net *, const char *, unsigned,
					const char *, bool);
extern struct afs_cell *afs_use_cell(struct afs_cell *, enum afs_cell_trace);
extern void afs_unuse_cell(struct afs_net *, struct afs_cell *, enum afs_cell_trace);
extern struct afs_cell *afs_get_cell(struct afs_cell *, enum afs_cell_trace);
extern void afs_see_cell(struct afs_cell *, enum afs_cell_trace);
extern void afs_put_cell(struct afs_cell *, enum afs_cell_trace);
extern void afs_queue_cell(struct afs_cell *, enum afs_cell_trace);
extern void afs_manage_cells(struct work_struct *);
extern void afs_cells_timer(struct timer_list *);
extern void __net_exit afs_cell_purge(struct afs_net *);


extern bool afs_cm_incoming_call(struct afs_call *);


extern const struct file_operations afs_dir_file_operations;
extern const struct inode_operations afs_dir_inode_operations;
extern const struct address_space_operations afs_dir_aops;
extern const struct dentry_operations afs_fs_dentry_operations;

extern void afs_d_release(struct dentry *);
extern void afs_check_for_remote_deletion(struct afs_operation *);


extern void afs_edit_dir_add(struct afs_vnode *, struct qstr *, struct afs_fid *,
			     enum afs_edit_dir_reason);
extern void afs_edit_dir_remove(struct afs_vnode *, struct qstr *, enum afs_edit_dir_reason);


extern int afs_sillyrename(struct afs_vnode *, struct afs_vnode *,
			   struct dentry *, struct key *);
extern int afs_silly_iput(struct dentry *, struct inode *);


extern const struct inode_operations afs_dynroot_inode_operations;
extern const struct dentry_operations afs_dynroot_dentry_operations;

extern struct inode *afs_try_auto_mntpt(struct dentry *, struct inode *);
extern int afs_dynroot_mkdir(struct afs_net *, struct afs_cell *);
extern void afs_dynroot_rmdir(struct afs_net *, struct afs_cell *);
extern int afs_dynroot_populate(struct super_block *);
extern void afs_dynroot_depopulate(struct super_block *);


extern const struct address_space_operations afs_file_aops;
extern const struct address_space_operations afs_symlink_aops;
extern const struct inode_operations afs_file_inode_operations;
extern const struct file_operations afs_file_operations;
extern const struct netfs_request_ops afs_req_ops;

extern int afs_cache_wb_key(struct afs_vnode *, struct afs_file *);
extern void afs_put_wb_key(struct afs_wb_key *);
extern int afs_open(struct inode *, struct file *);
extern int afs_release(struct inode *, struct file *);
extern int afs_fetch_data(struct afs_vnode *, struct afs_read *);
extern struct afs_read *afs_alloc_read(gfp_t);
extern void afs_put_read(struct afs_read *);
extern int afs_write_inode(struct inode *, struct writeback_control *);

static inline struct afs_read *afs_get_read(struct afs_read *req)
{
	refcount_inc(&req->usage);
	return req;
}


extern struct workqueue_struct *afs_lock_manager;

extern void afs_lock_op_done(struct afs_call *);
extern void afs_lock_work(struct work_struct *);
extern void afs_lock_may_be_available(struct afs_vnode *);
extern int afs_lock(struct file *, int, struct file_lock *);
extern int afs_flock(struct file *, int, struct file_lock *);


extern void afs_fs_fetch_status(struct afs_operation *);
extern void afs_fs_fetch_data(struct afs_operation *);
extern void afs_fs_create_file(struct afs_operation *);
extern void afs_fs_make_dir(struct afs_operation *);
extern void afs_fs_remove_file(struct afs_operation *);
extern void afs_fs_remove_dir(struct afs_operation *);
extern void afs_fs_link(struct afs_operation *);
extern void afs_fs_symlink(struct afs_operation *);
extern void afs_fs_rename(struct afs_operation *);
extern void afs_fs_store_data(struct afs_operation *);
extern void afs_fs_setattr(struct afs_operation *);
extern void afs_fs_get_volume_status(struct afs_operation *);
extern void afs_fs_set_lock(struct afs_operation *);
extern void afs_fs_extend_lock(struct afs_operation *);
extern void afs_fs_release_lock(struct afs_operation *);
extern int afs_fs_give_up_all_callbacks(struct afs_net *, struct afs_server *,
					struct afs_addr_cursor *, struct key *);
extern bool afs_fs_get_capabilities(struct afs_net *, struct afs_server *,
				    struct afs_addr_cursor *, struct key *);
extern void afs_fs_inline_bulk_status(struct afs_operation *);

struct afs_acl {
	u32	size;
	u8	data[];
};

extern void afs_fs_fetch_acl(struct afs_operation *);
extern void afs_fs_store_acl(struct afs_operation *);


extern struct afs_operation *afs_alloc_operation(struct key *, struct afs_volume *);
extern int afs_put_operation(struct afs_operation *);
extern bool afs_begin_vnode_operation(struct afs_operation *);
extern void afs_wait_for_operation(struct afs_operation *);
extern int afs_do_sync_operation(struct afs_operation *);

static inline void afs_op_nomem(struct afs_operation *op)
{
	op->error = -ENOMEM;
}

static inline void afs_op_set_vnode(struct afs_operation *op, unsigned int n,
				    struct afs_vnode *vnode)
{
	op->file[n].vnode = vnode;
	op->file[n].need_io_lock = true;
}

static inline void afs_op_set_fid(struct afs_operation *op, unsigned int n,
				  const struct afs_fid *fid)
{
	op->file[n].fid = *fid;
}


extern void afs_fileserver_probe_result(struct afs_call *);
extern void afs_fs_probe_fileserver(struct afs_net *, struct afs_server *, struct key *, bool);
extern int afs_wait_for_fs_probes(struct afs_server_list *, unsigned long);
extern void afs_probe_fileserver(struct afs_net *, struct afs_server *);
extern void afs_fs_probe_dispatcher(struct work_struct *);
extern int afs_wait_for_one_fs_probe(struct afs_server *, bool);
extern void afs_fs_probe_cleanup(struct afs_net *);


extern const struct afs_operation_ops afs_fetch_status_operation;

extern void afs_vnode_commit_status(struct afs_operation *, struct afs_vnode_param *);
extern int afs_fetch_status(struct afs_vnode *, struct key *, bool, afs_access_t *);
extern int afs_ilookup5_test_by_fid(struct inode *, void *);
extern struct inode *afs_iget_pseudo_dir(struct super_block *, bool);
extern struct inode *afs_iget(struct afs_operation *, struct afs_vnode_param *);
extern struct inode *afs_root_iget(struct super_block *, struct key *);
extern bool afs_check_validity(struct afs_vnode *);
extern int afs_validate(struct afs_vnode *, struct key *);
bool afs_pagecache_valid(struct afs_vnode *);
extern int afs_getattr(struct mnt_idmap *idmap, const struct path *,
		       struct kstat *, u32, unsigned int);
extern int afs_setattr(struct mnt_idmap *idmap, struct dentry *, struct iattr *);
extern void afs_evict_inode(struct inode *);
extern int afs_drop_inode(struct inode *);


extern struct workqueue_struct *afs_wq;
extern int afs_net_id;

static inline struct afs_net *afs_net(struct net *net)
{
	return net_generic(net, afs_net_id);
}

static inline struct afs_net *afs_sb2net(struct super_block *sb)
{
	return afs_net(AFS_FS_S(sb)->net_ns);
}

static inline struct afs_net *afs_d2net(struct dentry *dentry)
{
	return afs_sb2net(dentry->d_sb);
}

static inline struct afs_net *afs_i2net(struct inode *inode)
{
	return afs_sb2net(inode->i_sb);
}

static inline struct afs_net *afs_v2net(struct afs_vnode *vnode)
{
	return afs_i2net(&vnode->netfs.inode);
}

static inline struct afs_net *afs_sock2net(struct sock *sk)
{
	return net_generic(sock_net(sk), afs_net_id);
}

static inline void __afs_stat(atomic_t *s)
{
	atomic_inc(s);
}

#define afs_stat_v(vnode, n) __afs_stat(&afs_v2net(vnode)->n)


extern int afs_abort_to_error(u32);
extern void afs_prioritise_error(struct afs_error *, int, u32);


extern const struct inode_operations afs_mntpt_inode_operations;
extern const struct inode_operations afs_autocell_inode_operations;
extern const struct file_operations afs_mntpt_file_operations;

extern struct vfsmount *afs_d_automount(struct path *);
extern void afs_mntpt_kill_timer(void);


#ifdef CONFIG_PROC_FS
extern int __net_init afs_proc_init(struct afs_net *);
extern void __net_exit afs_proc_cleanup(struct afs_net *);
extern int afs_proc_cell_setup(struct afs_cell *);
extern void afs_proc_cell_remove(struct afs_cell *);
extern void afs_put_sysnames(struct afs_sysnames *);
#else
static inline int afs_proc_init(struct afs_net *net) { return 0; }
static inline void afs_proc_cleanup(struct afs_net *net) {}
static inline int afs_proc_cell_setup(struct afs_cell *cell) { return 0; }
static inline void afs_proc_cell_remove(struct afs_cell *cell) {}
static inline void afs_put_sysnames(struct afs_sysnames *sysnames) {}
#endif


extern bool afs_select_fileserver(struct afs_operation *);
extern void afs_dump_edestaddrreq(const struct afs_operation *);


extern struct workqueue_struct *afs_async_calls;

extern int __net_init afs_open_socket(struct afs_net *);
extern void __net_exit afs_close_socket(struct afs_net *);
extern void afs_charge_preallocation(struct work_struct *);
extern void afs_put_call(struct afs_call *);
extern void afs_make_call(struct afs_addr_cursor *, struct afs_call *, gfp_t);
extern long afs_wait_for_call_to_complete(struct afs_call *, struct afs_addr_cursor *);
extern struct afs_call *afs_alloc_flat_call(struct afs_net *,
					    const struct afs_call_type *,
					    size_t, size_t);
extern void afs_flat_call_destructor(struct afs_call *);
extern void afs_send_empty_reply(struct afs_call *);
extern void afs_send_simple_reply(struct afs_call *, const void *, size_t);
extern int afs_extract_data(struct afs_call *, bool);
extern int afs_protocol_error(struct afs_call *, enum afs_eproto_cause);

static inline void afs_make_op_call(struct afs_operation *op, struct afs_call *call,
				    gfp_t gfp)
{
	op->call = call;
	op->type = call->type;
	call->op = op;
	call->key = op->key;
	call->intr = !(op->flags & AFS_OPERATION_UNINTR);
	afs_make_call(&op->ac, call, gfp);
}

static inline void afs_extract_begin(struct afs_call *call, void *buf, size_t size)
{
	call->iov_len = size;
	call->kvec[0].iov_base = buf;
	call->kvec[0].iov_len = size;
	iov_iter_kvec(&call->def_iter, ITER_DEST, call->kvec, 1, size);
}

static inline void afs_extract_to_tmp(struct afs_call *call)
{
	call->iov_len = sizeof(call->tmp);
	afs_extract_begin(call, &call->tmp, sizeof(call->tmp));
}

static inline void afs_extract_to_tmp64(struct afs_call *call)
{
	call->iov_len = sizeof(call->tmp64);
	afs_extract_begin(call, &call->tmp64, sizeof(call->tmp64));
}

static inline void afs_extract_discard(struct afs_call *call, size_t size)
{
	call->iov_len = size;
	iov_iter_discard(&call->def_iter, ITER_DEST, size);
}

static inline void afs_extract_to_buf(struct afs_call *call, size_t size)
{
	call->iov_len = size;
	afs_extract_begin(call, call->buffer, size);
}

static inline int afs_transfer_reply(struct afs_call *call)
{
	return afs_extract_data(call, false);
}

static inline bool afs_check_call_state(struct afs_call *call,
					enum afs_call_state state)
{
	return READ_ONCE(call->state) == state;
}

static inline bool afs_set_call_state(struct afs_call *call,
				      enum afs_call_state from,
				      enum afs_call_state to)
{
	bool ok = false;

	spin_lock_bh(&call->state_lock);
	if (call->state == from) {
		call->state = to;
		trace_afs_call_state(call, from, to, 0, 0);
		ok = true;
	}
	spin_unlock_bh(&call->state_lock);
	return ok;
}

static inline void afs_set_call_complete(struct afs_call *call,
					 int error, u32 remote_abort)
{
	enum afs_call_state state;
	bool ok = false;

	spin_lock_bh(&call->state_lock);
	state = call->state;
	if (state != AFS_CALL_COMPLETE) {
		call->abort_code = remote_abort;
		call->error = error;
		call->state = AFS_CALL_COMPLETE;
		trace_afs_call_state(call, state, AFS_CALL_COMPLETE,
				     error, remote_abort);
		ok = true;
	}
	spin_unlock_bh(&call->state_lock);
	if (ok) {
		trace_afs_call_done(call);

		
		if (call->drop_ref)
			afs_put_call(call);
	}
}


extern void afs_put_permits(struct afs_permits *);
extern void afs_clear_permits(struct afs_vnode *);
extern void afs_cache_permit(struct afs_vnode *, struct key *, unsigned int,
			     struct afs_status_cb *);
extern struct key *afs_request_key(struct afs_cell *);
extern struct key *afs_request_key_rcu(struct afs_cell *);
extern int afs_check_permit(struct afs_vnode *, struct key *, afs_access_t *);
extern int afs_permission(struct mnt_idmap *, struct inode *, int);
extern void __exit afs_clean_up_permit_cache(void);


extern spinlock_t afs_server_peer_lock;

extern struct afs_server *afs_find_server(struct afs_net *,
					  const struct sockaddr_rxrpc *);
extern struct afs_server *afs_find_server_by_uuid(struct afs_net *, const uuid_t *);
extern struct afs_server *afs_lookup_server(struct afs_cell *, struct key *, const uuid_t *, u32);
extern struct afs_server *afs_get_server(struct afs_server *, enum afs_server_trace);
extern struct afs_server *afs_use_server(struct afs_server *, enum afs_server_trace);
extern void afs_unuse_server(struct afs_net *, struct afs_server *, enum afs_server_trace);
extern void afs_unuse_server_notime(struct afs_net *, struct afs_server *, enum afs_server_trace);
extern void afs_put_server(struct afs_net *, struct afs_server *, enum afs_server_trace);
extern void afs_manage_servers(struct work_struct *);
extern void afs_servers_timer(struct timer_list *);
extern void afs_fs_probe_timer(struct timer_list *);
extern void __net_exit afs_purge_servers(struct afs_net *);
extern bool afs_check_server_record(struct afs_operation *, struct afs_server *);

static inline void afs_inc_servers_outstanding(struct afs_net *net)
{
	atomic_inc(&net->servers_outstanding);
}

static inline void afs_dec_servers_outstanding(struct afs_net *net)
{
	if (atomic_dec_and_test(&net->servers_outstanding))
		wake_up_var(&net->servers_outstanding);
}

static inline bool afs_is_probing_server(struct afs_server *server)
{
	return list_empty(&server->probe_link);
}


static inline struct afs_server_list *afs_get_serverlist(struct afs_server_list *slist)
{
	refcount_inc(&slist->usage);
	return slist;
}

extern void afs_put_serverlist(struct afs_net *, struct afs_server_list *);
extern struct afs_server_list *afs_alloc_server_list(struct afs_cell *, struct key *,
						     struct afs_vldb_entry *,
						     u8);
extern bool afs_annotate_server_list(struct afs_server_list *, struct afs_server_list *);


extern int __init afs_fs_init(void);
extern void afs_fs_exit(void);


extern struct afs_vldb_entry *afs_vl_get_entry_by_name_u(struct afs_vl_cursor *,
							 const char *, int);
extern struct afs_addr_list *afs_vl_get_addrs_u(struct afs_vl_cursor *, const uuid_t *);
extern struct afs_call *afs_vl_get_capabilities(struct afs_net *, struct afs_addr_cursor *,
						struct key *, struct afs_vlserver *, unsigned int);
extern struct afs_addr_list *afs_yfsvl_get_endpoints(struct afs_vl_cursor *, const uuid_t *);
extern char *afs_yfsvl_get_cell_name(struct afs_vl_cursor *);


extern int afs_cell_detect_alias(struct afs_cell *, struct key *);


extern void afs_vlserver_probe_result(struct afs_call *);
extern int afs_send_vl_probes(struct afs_net *, struct key *, struct afs_vlserver_list *);
extern int afs_wait_for_vl_probes(struct afs_vlserver_list *, unsigned long);


extern bool afs_begin_vlserver_operation(struct afs_vl_cursor *,
					 struct afs_cell *, struct key *);
extern bool afs_select_vlserver(struct afs_vl_cursor *);
extern bool afs_select_current_vlserver(struct afs_vl_cursor *);
extern int afs_end_vlserver_operation(struct afs_vl_cursor *);


static inline struct afs_vlserver *afs_get_vlserver(struct afs_vlserver *vlserver)
{
	refcount_inc(&vlserver->ref);
	return vlserver;
}

static inline struct afs_vlserver_list *afs_get_vlserverlist(struct afs_vlserver_list *vllist)
{
	if (vllist)
		refcount_inc(&vllist->ref);
	return vllist;
}

extern struct afs_vlserver *afs_alloc_vlserver(const char *, size_t, unsigned short);
extern void afs_put_vlserver(struct afs_net *, struct afs_vlserver *);
extern struct afs_vlserver_list *afs_alloc_vlserver_list(unsigned int);
extern void afs_put_vlserverlist(struct afs_net *, struct afs_vlserver_list *);
extern struct afs_vlserver_list *afs_extract_vlserver_list(struct afs_cell *,
							   const void *, size_t);


extern struct afs_volume *afs_create_volume(struct afs_fs_context *);
extern int afs_activate_volume(struct afs_volume *);
extern void afs_deactivate_volume(struct afs_volume *);
bool afs_try_get_volume(struct afs_volume *volume, enum afs_volume_trace reason);
extern struct afs_volume *afs_get_volume(struct afs_volume *, enum afs_volume_trace);
extern void afs_put_volume(struct afs_net *, struct afs_volume *, enum afs_volume_trace);
extern int afs_check_volume_status(struct afs_volume *, struct afs_operation *);


#ifdef CONFIG_AFS_FSCACHE
bool afs_dirty_folio(struct address_space *, struct folio *);
#else
#define afs_dirty_folio filemap_dirty_folio
#endif
extern int afs_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len,
			struct page **pagep, void **fsdata);
extern int afs_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata);
extern int afs_writepage(struct page *, struct writeback_control *);
extern int afs_writepages(struct address_space *, struct writeback_control *);
extern ssize_t afs_file_write(struct kiocb *, struct iov_iter *);
extern int afs_fsync(struct file *, loff_t, loff_t, int);
extern vm_fault_t afs_page_mkwrite(struct vm_fault *vmf);
extern void afs_prune_wb_keys(struct afs_vnode *);
int afs_launder_folio(struct folio *);


extern const struct xattr_handler *afs_xattr_handlers[];


extern void yfs_fs_fetch_data(struct afs_operation *);
extern void yfs_fs_create_file(struct afs_operation *);
extern void yfs_fs_make_dir(struct afs_operation *);
extern void yfs_fs_remove_file2(struct afs_operation *);
extern void yfs_fs_remove_file(struct afs_operation *);
extern void yfs_fs_remove_dir(struct afs_operation *);
extern void yfs_fs_link(struct afs_operation *);
extern void yfs_fs_symlink(struct afs_operation *);
extern void yfs_fs_rename(struct afs_operation *);
extern void yfs_fs_store_data(struct afs_operation *);
extern void yfs_fs_setattr(struct afs_operation *);
extern void yfs_fs_get_volume_status(struct afs_operation *);
extern void yfs_fs_set_lock(struct afs_operation *);
extern void yfs_fs_extend_lock(struct afs_operation *);
extern void yfs_fs_release_lock(struct afs_operation *);
extern void yfs_fs_fetch_status(struct afs_operation *);
extern void yfs_fs_inline_bulk_status(struct afs_operation *);

struct yfs_acl {
	struct afs_acl	*acl;		
	struct afs_acl	*vol_acl;	
	u32		inherit_flag;	
	u32		num_cleaned;	
	unsigned int	flags;
#define YFS_ACL_WANT_ACL	0x01	
#define YFS_ACL_WANT_VOL_ACL	0x02	
};

extern void yfs_free_opaque_acl(struct yfs_acl *);
extern void yfs_fs_fetch_opaque_acl(struct afs_operation *);
extern void yfs_fs_store_opaque_acl2(struct afs_operation *);


static inline struct afs_vnode *AFS_FS_I(struct inode *inode)
{
	return container_of(inode, struct afs_vnode, netfs.inode);
}

static inline struct inode *AFS_VNODE_TO_I(struct afs_vnode *vnode)
{
	return &vnode->netfs.inode;
}


static inline void afs_update_dentry_version(struct afs_operation *op,
					     struct afs_vnode_param *dir_vp,
					     struct dentry *dentry)
{
	if (!op->error)
		dentry->d_fsdata =
			(void *)(unsigned long)dir_vp->scb.status.data_version;
}


static inline void afs_set_i_size(struct afs_vnode *vnode, u64 size)
{
	i_size_write(&vnode->netfs.inode, size);
	vnode->netfs.inode.i_blocks = ((size + 1023) >> 10) << 1;
}


static inline void afs_check_dir_conflict(struct afs_operation *op,
					  struct afs_vnode_param *dvp)
{
	if (dvp->dv_before + dvp->dv_delta != dvp->scb.status.data_version)
		op->flags |= AFS_OPERATION_DIR_CONFLICT;
}

static inline int afs_io_error(struct afs_call *call, enum afs_io_error where)
{
	trace_afs_io_error(call->debug_id, -EIO, where);
	return -EIO;
}

static inline int afs_bad(struct afs_vnode *vnode, enum afs_file_error where)
{
	trace_afs_file_error(vnode, -EIO, where);
	return -EIO;
}



extern unsigned afs_debug;

#define dbgprintk(FMT,...) \
	printk("[%-6.6s] "FMT"\n", current->comm ,##__VA_ARGS__)

#define kenter(FMT,...)	dbgprintk("==> %s("FMT")",__func__ ,##__VA_ARGS__)
#define kleave(FMT,...)	dbgprintk("<== %s()"FMT"",__func__ ,##__VA_ARGS__)
#define kdebug(FMT,...)	dbgprintk("    "FMT ,##__VA_ARGS__)


#if defined(__KDEBUG)
#define _enter(FMT,...)	kenter(FMT,##__VA_ARGS__)
#define _leave(FMT,...)	kleave(FMT,##__VA_ARGS__)
#define _debug(FMT,...)	kdebug(FMT,##__VA_ARGS__)

#elif defined(CONFIG_AFS_DEBUG)
#define AFS_DEBUG_KENTER	0x01
#define AFS_DEBUG_KLEAVE	0x02
#define AFS_DEBUG_KDEBUG	0x04

#define _enter(FMT,...)					\
do {							\
	if (unlikely(afs_debug & AFS_DEBUG_KENTER))	\
		kenter(FMT,##__VA_ARGS__);		\
} while (0)

#define _leave(FMT,...)					\
do {							\
	if (unlikely(afs_debug & AFS_DEBUG_KLEAVE))	\
		kleave(FMT,##__VA_ARGS__);		\
} while (0)

#define _debug(FMT,...)					\
do {							\
	if (unlikely(afs_debug & AFS_DEBUG_KDEBUG))	\
		kdebug(FMT,##__VA_ARGS__);		\
} while (0)

#else
#define _enter(FMT,...)	no_printk("==> %s("FMT")",__func__ ,##__VA_ARGS__)
#define _leave(FMT,...)	no_printk("<== %s()"FMT"",__func__ ,##__VA_ARGS__)
#define _debug(FMT,...)	no_printk("    "FMT ,##__VA_ARGS__)
#endif


#if 1 

#define ASSERT(X)						\
do {								\
	if (unlikely(!(X))) {					\
		printk(KERN_ERR "\n");				\
		printk(KERN_ERR "AFS: Assertion failed\n");	\
		BUG();						\
	}							\
} while(0)

#define ASSERTCMP(X, OP, Y)						\
do {									\
	if (unlikely(!((X) OP (Y)))) {					\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "AFS: Assertion failed\n");		\
		printk(KERN_ERR "%lu " #OP " %lu is false\n",		\
		       (unsigned long)(X), (unsigned long)(Y));		\
		printk(KERN_ERR "0x%lx " #OP " 0x%lx is false\n",	\
		       (unsigned long)(X), (unsigned long)(Y));		\
		BUG();							\
	}								\
} while(0)

#define ASSERTRANGE(L, OP1, N, OP2, H)					\
do {									\
	if (unlikely(!((L) OP1 (N)) || !((N) OP2 (H)))) {		\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "AFS: Assertion failed\n");		\
		printk(KERN_ERR "%lu "#OP1" %lu "#OP2" %lu is false\n",	\
		       (unsigned long)(L), (unsigned long)(N),		\
		       (unsigned long)(H));				\
		printk(KERN_ERR "0x%lx "#OP1" 0x%lx "#OP2" 0x%lx is false\n", \
		       (unsigned long)(L), (unsigned long)(N),		\
		       (unsigned long)(H));				\
		BUG();							\
	}								\
} while(0)

#define ASSERTIF(C, X)						\
do {								\
	if (unlikely((C) && !(X))) {				\
		printk(KERN_ERR "\n");				\
		printk(KERN_ERR "AFS: Assertion failed\n");	\
		BUG();						\
	}							\
} while(0)

#define ASSERTIFCMP(C, X, OP, Y)					\
do {									\
	if (unlikely((C) && !((X) OP (Y)))) {				\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "AFS: Assertion failed\n");		\
		printk(KERN_ERR "%lu " #OP " %lu is false\n",		\
		       (unsigned long)(X), (unsigned long)(Y));		\
		printk(KERN_ERR "0x%lx " #OP " 0x%lx is false\n",	\
		       (unsigned long)(X), (unsigned long)(Y));		\
		BUG();							\
	}								\
} while(0)

#else

#define ASSERT(X)				\
do {						\
} while(0)

#define ASSERTCMP(X, OP, Y)			\
do {						\
} while(0)

#define ASSERTRANGE(L, OP1, N, OP2, H)		\
do {						\
} while(0)

#define ASSERTIF(C, X)				\
do {						\
} while(0)

#define ASSERTIFCMP(C, X, OP, Y)		\
do {						\
} while(0)

#endif 
