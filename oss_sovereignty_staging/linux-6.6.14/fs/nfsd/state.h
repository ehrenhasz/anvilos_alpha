 

#ifndef _NFSD4_STATE_H
#define _NFSD4_STATE_H

#include <linux/idr.h>
#include <linux/refcount.h>
#include <linux/sunrpc/svc_xprt.h>
#include "nfsfh.h"
#include "nfsd.h"

typedef struct {
	u32             cl_boot;
	u32             cl_id;
} clientid_t;

typedef struct {
	clientid_t	so_clid;
	u32		so_id;
} stateid_opaque_t;

typedef struct {
	u32                     si_generation;
	stateid_opaque_t        si_opaque;
} stateid_t;

typedef struct {
	stateid_t		cs_stid;
#define NFS4_COPY_STID 1
#define NFS4_COPYNOTIFY_STID 2
	unsigned char		cs_type;
	refcount_t		cs_count;
} copy_stateid_t;

struct nfsd4_callback {
	struct nfs4_client *cb_clp;
	struct rpc_message cb_msg;
	const struct nfsd4_callback_ops *cb_ops;
	struct work_struct cb_work;
	int cb_seq_status;
	int cb_status;
	bool cb_need_restart;
	bool cb_holds_slot;
};

struct nfsd4_callback_ops {
	void (*prepare)(struct nfsd4_callback *);
	int (*done)(struct nfsd4_callback *, struct rpc_task *);
	void (*release)(struct nfsd4_callback *);
};

 
struct nfs4_stid {
	refcount_t		sc_count;
#define NFS4_OPEN_STID 1
#define NFS4_LOCK_STID 2
#define NFS4_DELEG_STID 4
 
#define NFS4_CLOSED_STID 8
 
#define NFS4_REVOKED_DELEG_STID 16
#define NFS4_CLOSED_DELEG_STID 32
#define NFS4_LAYOUT_STID 64
	struct list_head	sc_cp_list;
	unsigned char		sc_type;
	stateid_t		sc_stateid;
	spinlock_t		sc_lock;
	struct nfs4_client	*sc_client;
	struct nfs4_file	*sc_file;
	void			(*sc_free)(struct nfs4_stid *);
};

 
struct nfs4_cpntf_state {
	copy_stateid_t		cp_stateid;
	struct list_head	cp_list;	 
	stateid_t		cp_p_stateid;	 
	clientid_t		cp_p_clid;	 
	time64_t		cpntf_time;	 
};

 
struct nfs4_delegation {
	struct nfs4_stid	dl_stid;  
	struct list_head	dl_perfile;
	struct list_head	dl_perclnt;
	struct list_head	dl_recall_lru;   
	struct nfs4_clnt_odstate *dl_clnt_odstate;
	u32			dl_type;
	time64_t		dl_time;
 
	int			dl_retries;
	struct nfsd4_callback	dl_recall;
	bool			dl_recalled;
};

#define cb_to_delegation(cb) \
	container_of(cb, struct nfs4_delegation, dl_recall)

 
struct nfs4_cb_conn {
	 
	struct sockaddr_storage	cb_addr;
	struct sockaddr_storage	cb_saddr;
	size_t			cb_addrlen;
	u32                     cb_prog;  
	u32                     cb_ident;	 
	struct svc_xprt		*cb_xprt;	 
};

static inline struct nfs4_delegation *delegstateid(struct nfs4_stid *s)
{
	return container_of(s, struct nfs4_delegation, dl_stid);
}

 
#define NFSD_MAX_SLOTS_PER_SESSION     160
 
#define NFSD_MAX_OPS_PER_COMPOUND	50
 
#define NFSD_SLOT_CACHE_SIZE		2048
 
#define NFSD_CACHE_SIZE_SLOTS_PER_SESSION	32
#define NFSD_MAX_MEM_PER_SESSION  \
		(NFSD_CACHE_SIZE_SLOTS_PER_SESSION * NFSD_SLOT_CACHE_SIZE)

struct nfsd4_slot {
	u32	sl_seqid;
	__be32	sl_status;
	struct svc_cred sl_cred;
	u32	sl_datalen;
	u16	sl_opcnt;
#define NFSD4_SLOT_INUSE	(1 << 0)
#define NFSD4_SLOT_CACHETHIS	(1 << 1)
#define NFSD4_SLOT_INITIALIZED	(1 << 2)
#define NFSD4_SLOT_CACHED	(1 << 3)
	u8	sl_flags;
	char	sl_data[];
};

struct nfsd4_channel_attrs {
	u32		headerpadsz;
	u32		maxreq_sz;
	u32		maxresp_sz;
	u32		maxresp_cached;
	u32		maxops;
	u32		maxreqs;
	u32		nr_rdma_attrs;
	u32		rdma_attrs;
};

struct nfsd4_cb_sec {
	u32	flavor;  
	kuid_t	uid;
	kgid_t	gid;
};

struct nfsd4_create_session {
	clientid_t			clientid;
	struct nfs4_sessionid		sessionid;
	u32				seqid;
	u32				flags;
	struct nfsd4_channel_attrs	fore_channel;
	struct nfsd4_channel_attrs	back_channel;
	u32				callback_prog;
	struct nfsd4_cb_sec		cb_sec;
};

struct nfsd4_backchannel_ctl {
	u32	bc_cb_program;
	struct nfsd4_cb_sec		bc_cb_sec;
};

struct nfsd4_bind_conn_to_session {
	struct nfs4_sessionid		sessionid;
	u32				dir;
};

 
struct nfsd4_clid_slot {
	u32				sl_seqid;
	__be32				sl_status;
	struct nfsd4_create_session	sl_cr_ses;
};

struct nfsd4_conn {
	struct list_head cn_persession;
	struct svc_xprt *cn_xprt;
	struct svc_xpt_user cn_xpt_user;
	struct nfsd4_session *cn_session;
 
	unsigned char cn_flags;
};

 
struct nfsd4_session {
	atomic_t		se_ref;
	struct list_head	se_hash;	 
	struct list_head	se_perclnt;
 
#define NFS4_SESSION_DEAD	0x010
	u32			se_flags;
	struct nfs4_client	*se_client;
	struct nfs4_sessionid	se_sessionid;
	struct nfsd4_channel_attrs se_fchannel;
	struct nfsd4_channel_attrs se_bchannel;
	struct nfsd4_cb_sec	se_cb_sec;
	struct list_head	se_conns;
	u32			se_cb_prog;
	u32			se_cb_seq_nr;
	struct nfsd4_slot	*se_slots[];	 
};

 
struct nfsd4_sessionid {
	clientid_t	clientid;
	u32		sequence;
	u32		reserved;
};

#define HEXDIR_LEN     33  

 
enum {
	NFSD4_ACTIVE = 0,
	NFSD4_COURTESY,
	NFSD4_EXPIRABLE,
};

 
struct nfs4_client {
	struct list_head	cl_idhash; 	 
	struct rb_node		cl_namenode;	 
	struct list_head	*cl_ownerstr_hashtbl;
	struct list_head	cl_openowners;
	struct idr		cl_stateids;	 
	struct list_head	cl_delegations;
	struct list_head	cl_revoked;	 
	struct list_head        cl_lru;          
#ifdef CONFIG_NFSD_PNFS
	struct list_head	cl_lo_states;	 
#endif
	struct xdr_netobj	cl_name; 	 
	nfs4_verifier		cl_verifier; 	 
	time64_t		cl_time;	 
	struct sockaddr_storage	cl_addr; 	 
	bool			cl_mach_cred;	 
	struct svc_cred		cl_cred; 	 
	clientid_t		cl_clientid;	 
	nfs4_verifier		cl_confirm;	 
	u32			cl_minorversion;
	 
	struct xdr_netobj	cl_nii_domain;
	struct xdr_netobj	cl_nii_name;
	struct timespec64	cl_nii_time;

	 
	struct nfs4_cb_conn	cl_cb_conn;
#define NFSD4_CLIENT_CB_UPDATE		(0)
#define NFSD4_CLIENT_CB_KILL		(1)
#define NFSD4_CLIENT_STABLE		(2)	 
#define NFSD4_CLIENT_RECLAIM_COMPLETE	(3)	 
#define NFSD4_CLIENT_CONFIRMED		(4)	 
#define NFSD4_CLIENT_UPCALL_LOCK	(5)	 
#define NFSD4_CLIENT_CB_FLAG_MASK	(1 << NFSD4_CLIENT_CB_UPDATE | \
					 1 << NFSD4_CLIENT_CB_KILL)
#define NFSD4_CLIENT_CB_RECALL_ANY	(6)
	unsigned long		cl_flags;
	const struct cred	*cl_cb_cred;
	struct rpc_clnt		*cl_cb_client;
	u32			cl_cb_ident;
#define NFSD4_CB_UP		0
#define NFSD4_CB_UNKNOWN	1
#define NFSD4_CB_DOWN		2
#define NFSD4_CB_FAULT		3
	int			cl_cb_state;
	struct nfsd4_callback	cl_cb_null;
	struct nfsd4_session	*cl_cb_session;

	 
	spinlock_t		cl_lock;

	 
	struct list_head	cl_sessions;
	struct nfsd4_clid_slot	cl_cs_slot;	 
	u32			cl_exchange_flags;
	 
	atomic_t		cl_rpc_users;
	struct nfsdfs_client	cl_nfsdfs;
	struct nfs4_op_map      cl_spo_must_allow;

	 
	struct dentry		*cl_nfsd_dentry;
	 
	struct dentry		*cl_nfsd_info_dentry;

	 
	 
	unsigned long		cl_cb_slot_busy;
	struct rpc_wait_queue	cl_cb_waitq;	 
						 
	struct net		*net;
	struct list_head	async_copies;	 
	spinlock_t		async_lock;	 
	atomic_t		cl_cb_inflight;	 

	unsigned int		cl_state;
	atomic_t		cl_delegs_in_recall;

	struct nfsd4_cb_recall_any	*cl_ra;
	time64_t		cl_ra_time;
	struct list_head	cl_ra_cblist;
};

 
struct nfs4_client_reclaim {
	struct list_head	cr_strhash;	 
	struct nfs4_client	*cr_clp;	 
	struct xdr_netobj	cr_name;	 
	struct xdr_netobj	cr_princhash;
};

 

#define NFSD4_REPLAY_ISIZE       112 

 
struct nfs4_replay {
	__be32			rp_status;
	unsigned int		rp_buflen;
	char			*rp_buf;
	struct knfsd_fh		rp_openfh;
	struct mutex		rp_mutex;
	char			rp_ibuf[NFSD4_REPLAY_ISIZE];
};

struct nfs4_stateowner;

struct nfs4_stateowner_operations {
	void (*so_unhash)(struct nfs4_stateowner *);
	void (*so_free)(struct nfs4_stateowner *);
};

 
struct nfs4_stateowner {
	struct list_head			so_strhash;
	struct list_head			so_stateids;
	struct nfs4_client			*so_client;
	const struct nfs4_stateowner_operations	*so_ops;
	 
	atomic_t				so_count;
	u32					so_seqid;
	struct xdr_netobj			so_owner;  
	struct nfs4_replay			so_replay;
	bool					so_is_open_owner;
};

 
struct nfs4_openowner {
	struct nfs4_stateowner	oo_owner;  
	struct list_head        oo_perclient;
	 
	struct list_head	oo_close_lru;
	struct nfs4_ol_stateid *oo_last_closed_stid;
	time64_t		oo_time;  
#define NFS4_OO_CONFIRMED   1
	unsigned char		oo_flags;
};

 
struct nfs4_lockowner {
	struct nfs4_stateowner	lo_owner;	 
	struct list_head	lo_blocked;	 
};

static inline struct nfs4_openowner * openowner(struct nfs4_stateowner *so)
{
	return container_of(so, struct nfs4_openowner, oo_owner);
}

static inline struct nfs4_lockowner * lockowner(struct nfs4_stateowner *so)
{
	return container_of(so, struct nfs4_lockowner, lo_owner);
}

 
struct nfs4_clnt_odstate {
	struct nfs4_client	*co_client;
	struct nfs4_file	*co_file;
	struct list_head	co_perfile;
	refcount_t		co_odcount;
};

 
struct nfs4_file {
	refcount_t		fi_ref;
	struct inode *		fi_inode;
	bool			fi_aliased;
	spinlock_t		fi_lock;
	struct rhlist_head	fi_rlist;
	struct list_head        fi_stateids;
	union {
		struct list_head	fi_delegations;
		struct rcu_head		fi_rcu;
	};
	struct list_head	fi_clnt_odstate;
	 
	struct nfsd_file	*fi_fds[3];
	 
	atomic_t		fi_access[2];
	u32			fi_share_deny;
	struct nfsd_file	*fi_deleg_file;
	int			fi_delegees;
	struct knfsd_fh		fi_fhandle;
	bool			fi_had_conflict;
#ifdef CONFIG_NFSD_PNFS
	struct list_head	fi_lo_states;
	atomic_t		fi_lo_recalls;
#endif
};

 
struct nfs4_ol_stateid {
	struct nfs4_stid		st_stid;
	struct list_head		st_perfile;
	struct list_head		st_perstateowner;
	struct list_head		st_locks;
	struct nfs4_stateowner		*st_stateowner;
	struct nfs4_clnt_odstate	*st_clnt_odstate;
 
	unsigned char			st_access_bmap;
	unsigned char			st_deny_bmap;
	struct nfs4_ol_stateid		*st_openstp;
	struct mutex			st_mutex;
};

static inline struct nfs4_ol_stateid *openlockstateid(struct nfs4_stid *s)
{
	return container_of(s, struct nfs4_ol_stateid, st_stid);
}

struct nfs4_layout_stateid {
	struct nfs4_stid		ls_stid;
	struct list_head		ls_perclnt;
	struct list_head		ls_perfile;
	spinlock_t			ls_lock;
	struct list_head		ls_layouts;
	u32				ls_layout_type;
	struct nfsd_file		*ls_file;
	struct nfsd4_callback		ls_recall;
	stateid_t			ls_recall_sid;
	bool				ls_recalled;
	struct mutex			ls_mutex;
};

static inline struct nfs4_layout_stateid *layoutstateid(struct nfs4_stid *s)
{
	return container_of(s, struct nfs4_layout_stateid, ls_stid);
}

 
#define RD_STATE	        0x00000010
#define WR_STATE	        0x00000020

enum nfsd4_cb_op {
	NFSPROC4_CLNT_CB_NULL = 0,
	NFSPROC4_CLNT_CB_RECALL,
	NFSPROC4_CLNT_CB_LAYOUT,
	NFSPROC4_CLNT_CB_OFFLOAD,
	NFSPROC4_CLNT_CB_SEQUENCE,
	NFSPROC4_CLNT_CB_NOTIFY_LOCK,
	NFSPROC4_CLNT_CB_RECALL_ANY,
};

 
static inline bool nfsd4_stateid_generation_after(stateid_t *a, stateid_t *b)
{
	return (s32)(a->si_generation - b->si_generation) > 0;
}

 
struct nfsd4_blocked_lock {
	struct list_head	nbl_list;
	struct list_head	nbl_lru;
	time64_t		nbl_time;
	struct file_lock	nbl_lock;
	struct knfsd_fh		nbl_fh;
	struct nfsd4_callback	nbl_cb;
	struct kref		nbl_kref;
};

struct nfsd4_compound_state;
struct nfsd_net;
struct nfsd4_copy;

extern __be32 nfs4_preprocess_stateid_op(struct svc_rqst *rqstp,
		struct nfsd4_compound_state *cstate, struct svc_fh *fhp,
		stateid_t *stateid, int flags, struct nfsd_file **filp,
		struct nfs4_stid **cstid);
__be32 nfsd4_lookup_stateid(struct nfsd4_compound_state *cstate,
		     stateid_t *stateid, unsigned char typemask,
		     struct nfs4_stid **s, struct nfsd_net *nn);
struct nfs4_stid *nfs4_alloc_stid(struct nfs4_client *cl, struct kmem_cache *slab,
				  void (*sc_free)(struct nfs4_stid *));
int nfs4_init_copy_state(struct nfsd_net *nn, struct nfsd4_copy *copy);
void nfs4_free_copy_state(struct nfsd4_copy *copy);
struct nfs4_cpntf_state *nfs4_alloc_init_cpntf_state(struct nfsd_net *nn,
			struct nfs4_stid *p_stid);
void nfs4_unhash_stid(struct nfs4_stid *s);
void nfs4_put_stid(struct nfs4_stid *s);
void nfs4_inc_and_copy_stateid(stateid_t *dst, struct nfs4_stid *stid);
void nfs4_remove_reclaim_record(struct nfs4_client_reclaim *, struct nfsd_net *);
extern void nfs4_release_reclaim(struct nfsd_net *);
extern struct nfs4_client_reclaim *nfsd4_find_reclaim_client(struct xdr_netobj name,
							struct nfsd_net *nn);
extern __be32 nfs4_check_open_reclaim(struct nfs4_client *);
extern void nfsd4_probe_callback(struct nfs4_client *clp);
extern void nfsd4_probe_callback_sync(struct nfs4_client *clp);
extern void nfsd4_change_callback(struct nfs4_client *clp, struct nfs4_cb_conn *);
extern void nfsd4_init_cb(struct nfsd4_callback *cb, struct nfs4_client *clp,
		const struct nfsd4_callback_ops *ops, enum nfsd4_cb_op op);
extern bool nfsd4_run_cb(struct nfsd4_callback *cb);
extern int nfsd4_create_callback_queue(void);
extern void nfsd4_destroy_callback_queue(void);
extern void nfsd4_shutdown_callback(struct nfs4_client *);
extern void nfsd4_shutdown_copy(struct nfs4_client *clp);
extern struct nfs4_client_reclaim *nfs4_client_to_reclaim(struct xdr_netobj name,
				struct xdr_netobj princhash, struct nfsd_net *nn);
extern bool nfs4_has_reclaimed_state(struct xdr_netobj name, struct nfsd_net *nn);

void put_nfs4_file(struct nfs4_file *fi);
extern void nfs4_put_cpntf_state(struct nfsd_net *nn,
				 struct nfs4_cpntf_state *cps);
extern __be32 manage_cpntf_state(struct nfsd_net *nn, stateid_t *st,
				 struct nfs4_client *clp,
				 struct nfs4_cpntf_state **cps);
static inline void get_nfs4_file(struct nfs4_file *fi)
{
	refcount_inc(&fi->fi_ref);
}
struct nfsd_file *find_any_file(struct nfs4_file *f);

 
void nfsd4_end_grace(struct nfsd_net *nn);

 
extern int nfsd4_client_tracking_init(struct net *net);
extern void nfsd4_client_tracking_exit(struct net *net);
extern void nfsd4_client_record_create(struct nfs4_client *clp);
extern void nfsd4_client_record_remove(struct nfs4_client *clp);
extern int nfsd4_client_record_check(struct nfs4_client *clp);
extern void nfsd4_record_grace_done(struct nfsd_net *nn);

static inline bool try_to_expire_client(struct nfs4_client *clp)
{
	cmpxchg(&clp->cl_state, NFSD4_COURTESY, NFSD4_EXPIRABLE);
	return clp->cl_state == NFSD4_EXPIRABLE;
}

extern __be32 nfsd4_deleg_getattr_conflict(struct svc_rqst *rqstp,
				struct inode *inode);
#endif    
