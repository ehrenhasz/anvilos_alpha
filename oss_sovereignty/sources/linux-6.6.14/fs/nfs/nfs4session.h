

#ifndef __LINUX_FS_NFS_NFS4SESSION_H
#define __LINUX_FS_NFS_NFS4SESSION_H


#define NFS4_DEF_SLOT_TABLE_SIZE (64U)
#define NFS4_DEF_CB_SLOT_TABLE_SIZE (16U)
#define NFS4_MAX_SLOT_TABLE (1024U)
#define NFS4_MAX_SLOTID (NFS4_MAX_SLOT_TABLE - 1U)
#define NFS4_NO_SLOT ((u32)-1)

#if IS_ENABLED(CONFIG_NFS_V4)


struct nfs4_slot {
	struct nfs4_slot_table	*table;
	struct nfs4_slot	*next;
	unsigned long		generation;
	u32			slot_nr;
	u32		 	seq_nr;
	u32		 	seq_nr_last_acked;
	u32		 	seq_nr_highest_sent;
	unsigned int		privileged : 1,
				seq_done : 1;
};


enum nfs4_slot_tbl_state {
	NFS4_SLOT_TBL_DRAINING,
};

#define SLOT_TABLE_SZ DIV_ROUND_UP(NFS4_MAX_SLOT_TABLE, BITS_PER_LONG)
struct nfs4_slot_table {
	struct nfs4_session *session;		
	struct nfs4_slot *slots;		
	unsigned long   used_slots[SLOT_TABLE_SZ]; 
	spinlock_t	slot_tbl_lock;
	struct rpc_wait_queue	slot_tbl_waitq;	
	wait_queue_head_t	slot_waitq;	
	u32		max_slots;		
	u32		max_slotid;		
	u32		highest_used_slotid;	
	u32		target_highest_slotid;	
	u32		server_highest_slotid;	
	s32		d_target_highest_slotid; 
	s32		d2_target_highest_slotid; 
	unsigned long	generation;		
	struct completion complete;
	unsigned long	slot_tbl_state;
};


struct nfs4_session {
	struct nfs4_sessionid		sess_id;
	u32				flags;
	unsigned long			session_state;
	u32				hash_alg;
	u32				ssv_len;

	
	struct nfs4_channel_attrs	fc_attrs;
	struct nfs4_slot_table		fc_slot_table;
	struct nfs4_channel_attrs	bc_attrs;
	struct nfs4_slot_table		bc_slot_table;
	struct nfs_client		*clp;
};

enum nfs4_session_state {
	NFS4_SESSION_INITING,
	NFS4_SESSION_ESTABLISHED,
};

extern int nfs4_setup_slot_table(struct nfs4_slot_table *tbl,
		unsigned int max_reqs, const char *queue);
extern void nfs4_shutdown_slot_table(struct nfs4_slot_table *tbl);
extern struct nfs4_slot *nfs4_alloc_slot(struct nfs4_slot_table *tbl);
extern struct nfs4_slot *nfs4_lookup_slot(struct nfs4_slot_table *tbl, u32 slotid);
extern int nfs4_slot_wait_on_seqid(struct nfs4_slot_table *tbl,
		u32 slotid, u32 seq_nr,
		unsigned long timeout);
extern bool nfs4_try_to_lock_slot(struct nfs4_slot_table *tbl, struct nfs4_slot *slot);
extern void nfs4_free_slot(struct nfs4_slot_table *tbl, struct nfs4_slot *slot);
extern void nfs4_slot_tbl_drain_complete(struct nfs4_slot_table *tbl);
bool nfs41_wake_and_assign_slot(struct nfs4_slot_table *tbl,
		struct nfs4_slot *slot);
void nfs41_wake_slot_table(struct nfs4_slot_table *tbl);

static inline bool nfs4_slot_tbl_draining(struct nfs4_slot_table *tbl)
{
	return !!test_bit(NFS4_SLOT_TBL_DRAINING, &tbl->slot_tbl_state);
}

static inline bool nfs4_test_locked_slot(const struct nfs4_slot_table *tbl,
		u32 slotid)
{
	return !!test_bit(slotid, tbl->used_slots);
}

static inline struct nfs4_session *nfs4_get_session(const struct nfs_client *clp)
{
	return clp->cl_session;
}

#if defined(CONFIG_NFS_V4_1)
extern void nfs41_set_target_slotid(struct nfs4_slot_table *tbl,
		u32 target_highest_slotid);
extern void nfs41_update_target_slotid(struct nfs4_slot_table *tbl,
		struct nfs4_slot *slot,
		struct nfs4_sequence_res *res);

extern int nfs4_setup_session_slot_tables(struct nfs4_session *ses);

extern struct nfs4_session *nfs4_alloc_session(struct nfs_client *clp);
extern void nfs4_destroy_session(struct nfs4_session *session);
extern int nfs4_init_session(struct nfs_client *clp);
extern int nfs4_init_ds_session(struct nfs_client *, unsigned long);


static inline int nfs4_has_session(const struct nfs_client *clp)
{
	if (clp->cl_session)
		return 1;
	return 0;
}

static inline int nfs4_has_persistent_session(const struct nfs_client *clp)
{
	if (nfs4_has_session(clp))
		return (clp->cl_session->flags & SESSION4_PERSIST);
	return 0;
}

static inline void nfs4_copy_sessionid(struct nfs4_sessionid *dst,
		const struct nfs4_sessionid *src)
{
	memcpy(dst->data, src->data, NFS4_MAX_SESSIONID_LEN);
}

#ifdef CONFIG_CRC32

#define nfs_session_id_hash(sess_id) \
	(~crc32_le(0xFFFFFFFF, &(sess_id)->data[0], sizeof((sess_id)->data)))
#else
#define nfs_session_id_hash(session) (0)
#endif
#else 

static inline int nfs4_init_session(struct nfs_client *clp)
{
	return 0;
}


static inline int nfs4_has_session(const struct nfs_client *clp)
{
	return 0;
}

static inline int nfs4_has_persistent_session(const struct nfs_client *clp)
{
	return 0;
}

#define nfs_session_id_hash(session) (0)

#endif 
#endif 
#endif 
