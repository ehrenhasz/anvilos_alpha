

#ifndef _LINUX_NFSD_NFSFH_H
#define _LINUX_NFSD_NFSFH_H

#include <linux/crc32.h>
#include <linux/sunrpc/svc.h>
#include <linux/iversion.h>
#include <linux/exportfs.h>
#include <linux/nfs4.h>



struct knfsd_fh {
	unsigned int	fh_size;	
	union {
		char			fh_raw[NFS4_FHSIZE];
		struct {
			u8		fh_version;	
			u8		fh_auth_type;	
			u8		fh_fsid_type;
			u8		fh_fileid_type;
			u32		fh_fsid[]; 
		};
	};
};

static inline __u32 ino_t_to_u32(ino_t ino)
{
	return (__u32) ino;
}

static inline ino_t u32_to_ino_t(__u32 uino)
{
	return (ino_t) uino;
}


typedef struct svc_fh {
	struct knfsd_fh		fh_handle;	
	int			fh_maxsize;	
	struct dentry *		fh_dentry;	
	struct svc_export *	fh_export;	

	bool			fh_want_write;	
	bool			fh_no_wcc;	
	bool			fh_no_atomic_attr;
						
	int			fh_flags;	
	bool			fh_post_saved;	
	bool			fh_pre_saved;	

	
	__u64			fh_pre_size;	
	struct timespec64	fh_pre_mtime;	
	struct timespec64	fh_pre_ctime;	
	
	u64			fh_pre_change;

	
	struct kstat		fh_post_attr;	
	u64			fh_post_change; 
} svc_fh;
#define NFSD4_FH_FOREIGN (1<<0)
#define SET_FH_FLAG(c, f) ((c)->fh_flags |= (f))
#define HAS_FH_FLAG(c, f) ((c)->fh_flags & (f))

enum nfsd_fsid {
	FSID_DEV = 0,
	FSID_NUM,
	FSID_MAJOR_MINOR,
	FSID_ENCODE_DEV,
	FSID_UUID4_INUM,
	FSID_UUID8,
	FSID_UUID16,
	FSID_UUID16_INUM,
};

enum fsid_source {
	FSIDSOURCE_DEV,
	FSIDSOURCE_FSID,
	FSIDSOURCE_UUID,
};
extern enum fsid_source fsid_source(const struct svc_fh *fhp);



static inline void mk_fsid(int vers, u32 *fsidv, dev_t dev, ino_t ino,
			   u32 fsid, unsigned char *uuid)
{
	u32 *up;
	switch(vers) {
	case FSID_DEV:
		fsidv[0] = (__force __u32)htonl((MAJOR(dev)<<16) |
				 MINOR(dev));
		fsidv[1] = ino_t_to_u32(ino);
		break;
	case FSID_NUM:
		fsidv[0] = fsid;
		break;
	case FSID_MAJOR_MINOR:
		fsidv[0] = (__force __u32)htonl(MAJOR(dev));
		fsidv[1] = (__force __u32)htonl(MINOR(dev));
		fsidv[2] = ino_t_to_u32(ino);
		break;

	case FSID_ENCODE_DEV:
		fsidv[0] = new_encode_dev(dev);
		fsidv[1] = ino_t_to_u32(ino);
		break;

	case FSID_UUID4_INUM:
		
		up = (u32*)uuid;
		fsidv[0] = ino_t_to_u32(ino);
		fsidv[1] = up[0] ^ up[1] ^ up[2] ^ up[3];
		break;

	case FSID_UUID8:
		
		up = (u32*)uuid;
		fsidv[0] = up[0] ^ up[2];
		fsidv[1] = up[1] ^ up[3];
		break;

	case FSID_UUID16:
		
		memcpy(fsidv, uuid, 16);
		break;

	case FSID_UUID16_INUM:
		
		*(u64*)fsidv = (u64)ino;
		memcpy(fsidv+2, uuid, 16);
		break;
	default: BUG();
	}
}

static inline int key_len(int type)
{
	switch(type) {
	case FSID_DEV:		return 8;
	case FSID_NUM: 		return 4;
	case FSID_MAJOR_MINOR:	return 12;
	case FSID_ENCODE_DEV:	return 8;
	case FSID_UUID4_INUM:	return 8;
	case FSID_UUID8:	return 8;
	case FSID_UUID16:	return 16;
	case FSID_UUID16_INUM:	return 24;
	default: return 0;
	}
}


extern char * SVCFH_fmt(struct svc_fh *fhp);


__be32	fh_verify(struct svc_rqst *, struct svc_fh *, umode_t, int);
__be32	fh_compose(struct svc_fh *, struct svc_export *, struct dentry *, struct svc_fh *);
__be32	fh_update(struct svc_fh *);
void	fh_put(struct svc_fh *);

static __inline__ struct svc_fh *
fh_copy(struct svc_fh *dst, const struct svc_fh *src)
{
	WARN_ON(src->fh_dentry);

	*dst = *src;
	return dst;
}

static inline void
fh_copy_shallow(struct knfsd_fh *dst, const struct knfsd_fh *src)
{
	dst->fh_size = src->fh_size;
	memcpy(&dst->fh_raw, &src->fh_raw, src->fh_size);
}

static __inline__ struct svc_fh *
fh_init(struct svc_fh *fhp, int maxsize)
{
	memset(fhp, 0, sizeof(*fhp));
	fhp->fh_maxsize = maxsize;
	return fhp;
}

static inline bool fh_match(const struct knfsd_fh *fh1,
			    const struct knfsd_fh *fh2)
{
	if (fh1->fh_size != fh2->fh_size)
		return false;
	if (memcmp(fh1->fh_raw, fh2->fh_raw, fh1->fh_size) != 0)
		return false;
	return true;
}

static inline bool fh_fsid_match(const struct knfsd_fh *fh1,
				 const struct knfsd_fh *fh2)
{
	if (fh1->fh_fsid_type != fh2->fh_fsid_type)
		return false;
	if (memcmp(fh1->fh_fsid, fh2->fh_fsid, key_len(fh1->fh_fsid_type)) != 0)
		return false;
	return true;
}

#ifdef CONFIG_CRC32

static inline u32 knfsd_fh_hash(const struct knfsd_fh *fh)
{
	return ~crc32_le(0xFFFFFFFF, fh->fh_raw, fh->fh_size);
}
#else
static inline u32 knfsd_fh_hash(const struct knfsd_fh *fh)
{
	return 0;
}
#endif


static inline void fh_clear_pre_post_attrs(struct svc_fh *fhp)
{
	fhp->fh_post_saved = false;
	fhp->fh_pre_saved = false;
}

u64 nfsd4_change_attribute(struct kstat *stat, struct inode *inode);
__be32 __must_check fh_fill_pre_attrs(struct svc_fh *fhp);
__be32 fh_fill_post_attrs(struct svc_fh *fhp);
__be32 __must_check fh_fill_both_attrs(struct svc_fh *fhp);
#endif 
