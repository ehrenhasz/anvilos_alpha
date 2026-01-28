#ifndef _SYS_VNODE_IMPL_H
#define	_SYS_VNODE_IMPL_H
#define	IS_DEVVP(vp)	\
	((vp)->v_type == VCHR || (vp)->v_type == VBLK || (vp)->v_type == VFIFO)
#define	AV_SCANSTAMP_SZ	32		 
#define	XVA_MAPSIZE	3		 
#define	XVA_MAGIC	0x78766174	 
#define	XAT0_INDEX	0LL		 
#define	XAT0_CREATETIME	0x00000001	 
#define	XAT0_ARCHIVE	0x00000002	 
#define	XAT0_SYSTEM	0x00000004	 
#define	XAT0_READONLY	0x00000008	 
#define	XAT0_HIDDEN	0x00000010	 
#define	XAT0_NOUNLINK	0x00000020	 
#define	XAT0_IMMUTABLE	0x00000040	 
#define	XAT0_APPENDONLY	0x00000080	 
#define	XAT0_NODUMP	0x00000100	 
#define	XAT0_OPAQUE	0x00000200	 
#define	XAT0_AV_QUARANTINED	0x00000400	 
#define	XAT0_AV_MODIFIED	0x00000800	 
#define	XAT0_AV_SCANSTAMP	0x00001000	 
#define	XAT0_REPARSE	0x00002000	 
#define	XAT0_GEN	0x00004000	 
#define	XAT0_OFFLINE	0x00008000	 
#define	XAT0_SPARSE	0x00010000	 
#define	XVA_MASK		0xffffffff	 
#define	XVA_SHFT		32		 
#define	XVA_INDEX(attr)		((uint32_t)(((attr) >> XVA_SHFT) & XVA_MASK))
#define	XVA_ATTRBIT(attr)	((uint32_t)((attr) & XVA_MASK))
#define	XAT_CREATETIME		((XAT0_INDEX << XVA_SHFT) | XAT0_CREATETIME)
#define	XAT_ARCHIVE		((XAT0_INDEX << XVA_SHFT) | XAT0_ARCHIVE)
#define	XAT_SYSTEM		((XAT0_INDEX << XVA_SHFT) | XAT0_SYSTEM)
#define	XAT_READONLY		((XAT0_INDEX << XVA_SHFT) | XAT0_READONLY)
#define	XAT_HIDDEN		((XAT0_INDEX << XVA_SHFT) | XAT0_HIDDEN)
#define	XAT_NOUNLINK		((XAT0_INDEX << XVA_SHFT) | XAT0_NOUNLINK)
#define	XAT_IMMUTABLE		((XAT0_INDEX << XVA_SHFT) | XAT0_IMMUTABLE)
#define	XAT_APPENDONLY		((XAT0_INDEX << XVA_SHFT) | XAT0_APPENDONLY)
#define	XAT_NODUMP		((XAT0_INDEX << XVA_SHFT) | XAT0_NODUMP)
#define	XAT_OPAQUE		((XAT0_INDEX << XVA_SHFT) | XAT0_OPAQUE)
#define	XAT_AV_QUARANTINED	((XAT0_INDEX << XVA_SHFT) | XAT0_AV_QUARANTINED)
#define	XAT_AV_MODIFIED		((XAT0_INDEX << XVA_SHFT) | XAT0_AV_MODIFIED)
#define	XAT_AV_SCANSTAMP	((XAT0_INDEX << XVA_SHFT) | XAT0_AV_SCANSTAMP)
#define	XAT_REPARSE		((XAT0_INDEX << XVA_SHFT) | XAT0_REPARSE)
#define	XAT_GEN			((XAT0_INDEX << XVA_SHFT) | XAT0_GEN)
#define	XAT_OFFLINE		((XAT0_INDEX << XVA_SHFT) | XAT0_OFFLINE)
#define	XAT_SPARSE		((XAT0_INDEX << XVA_SHFT) | XAT0_SPARSE)
#define	XVA_RTNATTRMAP(xvap)	((xvap)->xva_rtnattrmapp)
#define	MODEMASK	07777		 
#define	PERMMASK	00777		 
enum rm		{ RMFILE, RMDIRECTORY };	 
enum create	{ CRCREAT, CRMKNOD, CRMKDIR };	 
typedef struct caller_context {
	pid_t		cc_pid;		 
	int		cc_sysid;	 
	u_longlong_t	cc_caller_id;	 
	ulong_t		cc_flags;
} caller_context_t;
struct taskq;
#define	LOOKUP_DIR		0x01	 
#define	LOOKUP_XATTR		0x02	 
#define	CREATE_XATTR_DIR	0x04	 
#define	LOOKUP_HAVE_SYSATTR_DIR	0x08	 
void	vn_rele_async(struct vnode *vp, struct taskq *taskq);
#define	VN_RELE_ASYNC(vp, taskq)	{ \
	vn_rele_async(vp, taskq); \
}
#define	ATTR_UTIME	0x01	 
#define	ATTR_EXEC	0x02	 
#define	ATTR_COMM	0x04	 
#define	ATTR_HINT	0x08	 
#define	ATTR_REAL	0x10	 
#define	ATTR_NOACLCHECK	0x20	 
#define	ATTR_TRIGGER	0x40	 
#ifdef	__cplusplus
}
#endif
#endif	 
