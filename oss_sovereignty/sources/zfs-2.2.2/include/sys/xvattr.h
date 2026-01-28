








#ifndef _SYS_XVATTR_H
#define	_SYS_XVATTR_H

#include <sys/vnode.h>
#include <sys/string.h>

#define	AV_SCANSTAMP_SZ	32		


typedef struct xoptattr {
	inode_timespec_t xoa_createtime;	
	uint8_t		xoa_archive;
	uint8_t		xoa_system;
	uint8_t		xoa_readonly;
	uint8_t		xoa_hidden;
	uint8_t		xoa_nounlink;
	uint8_t		xoa_immutable;
	uint8_t		xoa_appendonly;
	uint8_t		xoa_nodump;
	uint8_t		xoa_opaque;
	uint8_t		xoa_av_quarantined;
	uint8_t		xoa_av_modified;
	uint8_t		xoa_av_scanstamp[AV_SCANSTAMP_SZ];
	uint8_t		xoa_reparse;
	uint64_t	xoa_generation;
	uint8_t		xoa_offline;
	uint8_t		xoa_sparse;
	uint8_t		xoa_projinherit;
	uint64_t	xoa_projid;
} xoptattr_t;



#define	XVA_MAPSIZE	3		
#define	XVA_MAGIC	0x78766174	


typedef struct xvattr {
	vattr_t		xva_vattr;	
	uint32_t	xva_magic;	
	uint32_t	xva_mapsize;	
	uint32_t	*xva_rtnattrmapp;	
	uint32_t	xva_reqattrmap[XVA_MAPSIZE];	
	uint32_t	xva_rtnattrmap[XVA_MAPSIZE];	
	xoptattr_t	xva_xoptattrs;	
} xvattr_t;


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
#define	XAT0_PROJINHERIT	0x00020000	
#define	XAT0_PROJID	0x00040000	

#define	XAT0_ALL_ATTRS	(XAT0_CREATETIME|XAT0_ARCHIVE|XAT0_SYSTEM| \
    XAT0_READONLY|XAT0_HIDDEN|XAT0_NOUNLINK|XAT0_IMMUTABLE|XAT0_APPENDONLY| \
    XAT0_NODUMP|XAT0_OPAQUE|XAT0_AV_QUARANTINED|  XAT0_AV_MODIFIED| \
    XAT0_AV_SCANSTAMP|XAT0_REPARSE|XATO_GEN|XAT0_OFFLINE|XAT0_SPARSE| \
    XAT0_PROJINHERIT | XAT0_PROJID)


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
#define	XAT_PROJINHERIT		((XAT0_INDEX << XVA_SHFT) | XAT0_PROJINHERIT)
#define	XAT_PROJID		((XAT0_INDEX << XVA_SHFT) | XAT0_PROJID)


#define	XVA_RTNATTRMAP(xvap)	((xvap)->xva_rtnattrmapp)


#define	XVA_SET_REQ(xvap, attr)					\
	ASSERT((xvap)->xva_vattr.va_mask & AT_XVATTR);		\
	ASSERT((xvap)->xva_magic == XVA_MAGIC);			\
	(xvap)->xva_reqattrmap[XVA_INDEX(attr)] |= XVA_ATTRBIT(attr)

#define	XVA_CLR_REQ(xvap, attr)					\
	ASSERT((xvap)->xva_vattr.va_mask & AT_XVATTR);		\
	ASSERT((xvap)->xva_magic == XVA_MAGIC);			\
	(xvap)->xva_reqattrmap[XVA_INDEX(attr)] &= ~XVA_ATTRBIT(attr)


#define	XVA_SET_RTN(xvap, attr)					\
	ASSERT((xvap)->xva_vattr.va_mask & AT_XVATTR);		\
	ASSERT((xvap)->xva_magic == XVA_MAGIC);			\
	(XVA_RTNATTRMAP(xvap))[XVA_INDEX(attr)] |= XVA_ATTRBIT(attr)


#define	XVA_ISSET_REQ(xvap, attr)					\
	((((xvap)->xva_vattr.va_mask & AT_XVATTR) &&			\
		((xvap)->xva_magic == XVA_MAGIC) &&			\
		((xvap)->xva_mapsize > XVA_INDEX(attr))) ?		\
	((xvap)->xva_reqattrmap[XVA_INDEX(attr)] & XVA_ATTRBIT(attr)) :	0)


#define	XVA_ISSET_RTN(xvap, attr)					\
	((((xvap)->xva_vattr.va_mask & AT_XVATTR) &&			\
		((xvap)->xva_magic == XVA_MAGIC) &&			\
		((xvap)->xva_mapsize > XVA_INDEX(attr))) ?		\
	((XVA_RTNATTRMAP(xvap))[XVA_INDEX(attr)] & XVA_ATTRBIT(attr)) : 0)


static inline void
xva_init(xvattr_t *xvap)
{
	memset(xvap, 0, sizeof (xvattr_t));
	xvap->xva_mapsize = XVA_MAPSIZE;
	xvap->xva_magic = XVA_MAGIC;
	xvap->xva_vattr.va_mask = ATTR_XVATTR;
	xvap->xva_rtnattrmapp = &(xvap->xva_rtnattrmap)[0];
}


static inline xoptattr_t *
xva_getxoptattr(xvattr_t *xvap)
{
	xoptattr_t *xoap = NULL;
	if (xvap->xva_vattr.va_mask & AT_XVATTR)
			xoap = &xvap->xva_xoptattrs;
	return (xoap);
}

#define	MODEMASK	07777		
#define	PERMMASK	00777		


#define	V_ACE_MASK	0x1	
#define	V_APPEND	0x2	



typedef struct vsecattr {
	uint_t		vsa_mask;	
	int		vsa_aclcnt;	
	void		*vsa_aclentp;	
	int		vsa_dfaclcnt;	
	void		*vsa_dfaclentp;	
	size_t		vsa_aclentsz;	
	uint_t		vsa_aclflags;	
} vsecattr_t;


#define	VSA_ACL			0x0001
#define	VSA_ACLCNT		0x0002
#define	VSA_DFACL		0x0004
#define	VSA_DFACLCNT		0x0008
#define	VSA_ACE			0x0010
#define	VSA_ACECNT		0x0020
#define	VSA_ACE_ALLTYPES	0x0040
#define	VSA_ACE_ACLFLAGS	0x0080	

#endif 
