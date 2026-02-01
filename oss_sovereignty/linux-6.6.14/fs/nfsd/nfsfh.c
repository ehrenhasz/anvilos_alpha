
 

#include <linux/exportfs.h>

#include <linux/sunrpc/svcauth_gss.h>
#include "nfsd.h"
#include "vfs.h"
#include "auth.h"
#include "trace.h"

#define NFSDDBG_FACILITY		NFSDDBG_FH


 
static int nfsd_acceptable(void *expv, struct dentry *dentry)
{
	struct svc_export *exp = expv;
	int rv;
	struct dentry *tdentry;
	struct dentry *parent;

	if (exp->ex_flags & NFSEXP_NOSUBTREECHECK)
		return 1;

	tdentry = dget(dentry);
	while (tdentry != exp->ex_path.dentry && !IS_ROOT(tdentry)) {
		 
		int err;
		parent = dget_parent(tdentry);
		err = inode_permission(&nop_mnt_idmap,
				       d_inode(parent), MAY_EXEC);
		if (err < 0) {
			dput(parent);
			break;
		}
		dput(tdentry);
		tdentry = parent;
	}
	if (tdentry != exp->ex_path.dentry)
		dprintk("nfsd_acceptable failed at %p %pd\n", tdentry, tdentry);
	rv = (tdentry == exp->ex_path.dentry);
	dput(tdentry);
	return rv;
}

 
static inline __be32
nfsd_mode_check(struct svc_rqst *rqstp, struct dentry *dentry,
		umode_t requested)
{
	umode_t mode = d_inode(dentry)->i_mode & S_IFMT;

	if (requested == 0)  
		return nfs_ok;
	if (mode == requested) {
		if (mode == S_IFDIR && !d_can_lookup(dentry)) {
			WARN_ON_ONCE(1);
			return nfserr_notdir;
		}
		return nfs_ok;
	}
	 
	if (rqstp->rq_vers == 4 && mode == S_IFLNK)
		return nfserr_symlink;
	if (requested == S_IFDIR)
		return nfserr_notdir;
	if (mode == S_IFDIR)
		return nfserr_isdir;
	return nfserr_inval;
}

static bool nfsd_originating_port_ok(struct svc_rqst *rqstp, int flags)
{
	if (flags & NFSEXP_INSECURE_PORT)
		return true;
	 
	if (rqstp->rq_cred.cr_flavor >= RPC_AUTH_GSS)
		return true;
	return test_bit(RQ_SECURE, &rqstp->rq_flags);
}

static __be32 nfsd_setuser_and_check_port(struct svc_rqst *rqstp,
					  struct svc_export *exp)
{
	int flags = nfsexp_flags(rqstp, exp);

	 
	if (!nfsd_originating_port_ok(rqstp, flags)) {
		RPC_IFDEBUG(char buf[RPC_MAX_ADDRBUFLEN]);
		dprintk("nfsd: request from insecure port %s!\n",
		        svc_print_addr(rqstp, buf, sizeof(buf)));
		return nfserr_perm;
	}

	 
	return nfserrno(nfsd_setuser(rqstp, exp));
}

static inline __be32 check_pseudo_root(struct svc_rqst *rqstp,
	struct dentry *dentry, struct svc_export *exp)
{
	if (!(exp->ex_flags & NFSEXP_V4ROOT))
		return nfs_ok;
	 
	if (!nfsd_v4client(rqstp))
		return nfserr_stale;
	 
	if (unlikely(!d_is_dir(dentry) &&
		     !d_is_symlink(dentry)))
		return nfserr_stale;
	 
	if (unlikely(dentry != exp->ex_path.dentry))
		return nfserr_stale;
	return nfs_ok;
}

 
static __be32 nfsd_set_fh_dentry(struct svc_rqst *rqstp, struct svc_fh *fhp)
{
	struct knfsd_fh	*fh = &fhp->fh_handle;
	struct fid *fid = NULL;
	struct svc_export *exp;
	struct dentry *dentry;
	int fileid_type;
	int data_left = fh->fh_size/4;
	int len;
	__be32 error;

	error = nfserr_stale;
	if (rqstp->rq_vers > 2)
		error = nfserr_badhandle;
	if (rqstp->rq_vers == 4 && fh->fh_size == 0)
		return nfserr_nofilehandle;

	if (fh->fh_version != 1)
		return error;

	if (--data_left < 0)
		return error;
	if (fh->fh_auth_type != 0)
		return error;
	len = key_len(fh->fh_fsid_type) / 4;
	if (len == 0)
		return error;
	if (fh->fh_fsid_type == FSID_MAJOR_MINOR) {
		 
		len = key_len(FSID_ENCODE_DEV)/4;
		fh->fh_fsid_type = FSID_ENCODE_DEV;
		 
		fh->fh_fsid[0] = new_encode_dev(MKDEV(ntohl((__force __be32)fh->fh_fsid[0]),
						      ntohl((__force __be32)fh->fh_fsid[1])));
		fh->fh_fsid[1] = fh->fh_fsid[2];
	}
	data_left -= len;
	if (data_left < 0)
		return error;
	exp = rqst_exp_find(rqstp, fh->fh_fsid_type, fh->fh_fsid);
	fid = (struct fid *)(fh->fh_fsid + len);

	error = nfserr_stale;
	if (IS_ERR(exp)) {
		trace_nfsd_set_fh_dentry_badexport(rqstp, fhp, PTR_ERR(exp));

		if (PTR_ERR(exp) == -ENOENT)
			return error;

		return nfserrno(PTR_ERR(exp));
	}

	if (exp->ex_flags & NFSEXP_NOSUBTREECHECK) {
		 
		struct cred *new = prepare_creds();
		if (!new) {
			error =  nfserrno(-ENOMEM);
			goto out;
		}
		new->cap_effective =
			cap_raise_nfsd_set(new->cap_effective,
					   new->cap_permitted);
		put_cred(override_creds(new));
		put_cred(new);
	} else {
		error = nfsd_setuser_and_check_port(rqstp, exp);
		if (error)
			goto out;
	}

	 
	error = nfserr_stale;
	if (rqstp->rq_vers > 2)
		error = nfserr_badhandle;

	fileid_type = fh->fh_fileid_type;

	if (fileid_type == FILEID_ROOT)
		dentry = dget(exp->ex_path.dentry);
	else {
		dentry = exportfs_decode_fh_raw(exp->ex_path.mnt, fid,
						data_left, fileid_type,
						nfsd_acceptable, exp);
		if (IS_ERR_OR_NULL(dentry)) {
			trace_nfsd_set_fh_dentry_badhandle(rqstp, fhp,
					dentry ?  PTR_ERR(dentry) : -ESTALE);
			switch (PTR_ERR(dentry)) {
			case -ENOMEM:
			case -ETIMEDOUT:
				break;
			default:
				dentry = ERR_PTR(-ESTALE);
			}
		}
	}
	if (dentry == NULL)
		goto out;
	if (IS_ERR(dentry)) {
		if (PTR_ERR(dentry) != -EINVAL)
			error = nfserrno(PTR_ERR(dentry));
		goto out;
	}

	if (d_is_dir(dentry) &&
			(dentry->d_flags & DCACHE_DISCONNECTED)) {
		printk("nfsd: find_fh_dentry returned a DISCONNECTED directory: %pd2\n",
				dentry);
	}

	fhp->fh_dentry = dentry;
	fhp->fh_export = exp;

	switch (rqstp->rq_vers) {
	case 4:
		if (dentry->d_sb->s_export_op->flags & EXPORT_OP_NOATOMIC_ATTR)
			fhp->fh_no_atomic_attr = true;
		break;
	case 3:
		if (dentry->d_sb->s_export_op->flags & EXPORT_OP_NOWCC)
			fhp->fh_no_wcc = true;
		break;
	case 2:
		fhp->fh_no_wcc = true;
	}

	return 0;
out:
	exp_put(exp);
	return error;
}

 
__be32
fh_verify(struct svc_rqst *rqstp, struct svc_fh *fhp, umode_t type, int access)
{
	struct svc_export *exp = NULL;
	struct dentry	*dentry;
	__be32		error;

	if (!fhp->fh_dentry) {
		error = nfsd_set_fh_dentry(rqstp, fhp);
		if (error)
			goto out;
	}
	dentry = fhp->fh_dentry;
	exp = fhp->fh_export;

	trace_nfsd_fh_verify(rqstp, fhp, type, access);

	 
	error = check_pseudo_root(rqstp, dentry, exp);
	if (error)
		goto out;

	error = nfsd_setuser_and_check_port(rqstp, exp);
	if (error)
		goto out;

	error = nfsd_mode_check(rqstp, dentry, type);
	if (error)
		goto out;

	 
	if (access & NFSD_MAY_LOCK || access & NFSD_MAY_BYPASS_GSS)
		goto skip_pseudoflavor_check;
	 
	if (access & NFSD_MAY_BYPASS_GSS_ON_ROOT
			&& exp->ex_path.dentry == dentry)
		goto skip_pseudoflavor_check;

	error = check_nfsd_access(exp, rqstp);
	if (error)
		goto out;

skip_pseudoflavor_check:
	 
	error = nfsd_permission(rqstp, exp, dentry, access);
out:
	trace_nfsd_fh_verify_err(rqstp, fhp, type, access, error);
	if (error == nfserr_stale)
		nfsd_stats_fh_stale_inc(exp);
	return error;
}


 
static void _fh_update(struct svc_fh *fhp, struct svc_export *exp,
		struct dentry *dentry)
{
	if (dentry != exp->ex_path.dentry) {
		struct fid *fid = (struct fid *)
			(fhp->fh_handle.fh_fsid + fhp->fh_handle.fh_size/4 - 1);
		int maxsize = (fhp->fh_maxsize - fhp->fh_handle.fh_size)/4;
		int fh_flags = (exp->ex_flags & NFSEXP_NOSUBTREECHECK) ? 0 :
				EXPORT_FH_CONNECTABLE;
		int fileid_type =
			exportfs_encode_fh(dentry, fid, &maxsize, fh_flags);

		fhp->fh_handle.fh_fileid_type =
			fileid_type > 0 ? fileid_type : FILEID_INVALID;
		fhp->fh_handle.fh_size += maxsize * 4;
	} else {
		fhp->fh_handle.fh_fileid_type = FILEID_ROOT;
	}
}

static bool is_root_export(struct svc_export *exp)
{
	return exp->ex_path.dentry == exp->ex_path.dentry->d_sb->s_root;
}

static struct super_block *exp_sb(struct svc_export *exp)
{
	return exp->ex_path.dentry->d_sb;
}

static bool fsid_type_ok_for_exp(u8 fsid_type, struct svc_export *exp)
{
	switch (fsid_type) {
	case FSID_DEV:
		if (!old_valid_dev(exp_sb(exp)->s_dev))
			return false;
		fallthrough;
	case FSID_MAJOR_MINOR:
	case FSID_ENCODE_DEV:
		return exp_sb(exp)->s_type->fs_flags & FS_REQUIRES_DEV;
	case FSID_NUM:
		return exp->ex_flags & NFSEXP_FSID;
	case FSID_UUID8:
	case FSID_UUID16:
		if (!is_root_export(exp))
			return false;
		fallthrough;
	case FSID_UUID4_INUM:
	case FSID_UUID16_INUM:
		return exp->ex_uuid != NULL;
	}
	return true;
}


static void set_version_and_fsid_type(struct svc_fh *fhp, struct svc_export *exp, struct svc_fh *ref_fh)
{
	u8 version;
	u8 fsid_type;
retry:
	version = 1;
	if (ref_fh && ref_fh->fh_export == exp) {
		version = ref_fh->fh_handle.fh_version;
		fsid_type = ref_fh->fh_handle.fh_fsid_type;

		ref_fh = NULL;

		switch (version) {
		case 0xca:
			fsid_type = FSID_DEV;
			break;
		case 1:
			break;
		default:
			goto retry;
		}

		 
		if (!fsid_type_ok_for_exp(fsid_type, exp))
			goto retry;
	} else if (exp->ex_flags & NFSEXP_FSID) {
		fsid_type = FSID_NUM;
	} else if (exp->ex_uuid) {
		if (fhp->fh_maxsize >= 64) {
			if (is_root_export(exp))
				fsid_type = FSID_UUID16;
			else
				fsid_type = FSID_UUID16_INUM;
		} else {
			if (is_root_export(exp))
				fsid_type = FSID_UUID8;
			else
				fsid_type = FSID_UUID4_INUM;
		}
	} else if (!old_valid_dev(exp_sb(exp)->s_dev))
		 
		fsid_type = FSID_ENCODE_DEV;
	else
		fsid_type = FSID_DEV;
	fhp->fh_handle.fh_version = version;
	if (version)
		fhp->fh_handle.fh_fsid_type = fsid_type;
}

__be32
fh_compose(struct svc_fh *fhp, struct svc_export *exp, struct dentry *dentry,
	   struct svc_fh *ref_fh)
{
	 

	struct inode * inode = d_inode(dentry);
	dev_t ex_dev = exp_sb(exp)->s_dev;

	dprintk("nfsd: fh_compose(exp %02x:%02x/%ld %pd2, ino=%ld)\n",
		MAJOR(ex_dev), MINOR(ex_dev),
		(long) d_inode(exp->ex_path.dentry)->i_ino,
		dentry,
		(inode ? inode->i_ino : 0));

	 
	set_version_and_fsid_type(fhp, exp, ref_fh);

	 
	fhp->fh_no_wcc = ref_fh ? ref_fh->fh_no_wcc : false;

	if (ref_fh == fhp)
		fh_put(ref_fh);

	if (fhp->fh_dentry) {
		printk(KERN_ERR "fh_compose: fh %pd2 not initialized!\n",
		       dentry);
	}
	if (fhp->fh_maxsize < NFS_FHSIZE)
		printk(KERN_ERR "fh_compose: called with maxsize %d! %pd2\n",
		       fhp->fh_maxsize,
		       dentry);

	fhp->fh_dentry = dget(dentry);  
	fhp->fh_export = exp_get(exp);

	fhp->fh_handle.fh_size =
		key_len(fhp->fh_handle.fh_fsid_type) + 4;
	fhp->fh_handle.fh_auth_type = 0;

	mk_fsid(fhp->fh_handle.fh_fsid_type,
		fhp->fh_handle.fh_fsid,
		ex_dev,
		d_inode(exp->ex_path.dentry)->i_ino,
		exp->ex_fsid, exp->ex_uuid);

	if (inode)
		_fh_update(fhp, exp, dentry);
	if (fhp->fh_handle.fh_fileid_type == FILEID_INVALID) {
		fh_put(fhp);
		return nfserr_opnotsupp;
	}

	return 0;
}

 
__be32
fh_update(struct svc_fh *fhp)
{
	struct dentry *dentry;

	if (!fhp->fh_dentry)
		goto out_bad;

	dentry = fhp->fh_dentry;
	if (d_really_is_negative(dentry))
		goto out_negative;
	if (fhp->fh_handle.fh_fileid_type != FILEID_ROOT)
		return 0;

	_fh_update(fhp, fhp->fh_export, dentry);
	if (fhp->fh_handle.fh_fileid_type == FILEID_INVALID)
		return nfserr_opnotsupp;
	return 0;
out_bad:
	printk(KERN_ERR "fh_update: fh not verified!\n");
	return nfserr_serverfault;
out_negative:
	printk(KERN_ERR "fh_update: %pd2 still negative!\n",
		dentry);
	return nfserr_serverfault;
}

 
__be32 __must_check fh_fill_pre_attrs(struct svc_fh *fhp)
{
	bool v4 = (fhp->fh_maxsize == NFS4_FHSIZE);
	struct inode *inode;
	struct kstat stat;
	__be32 err;

	if (fhp->fh_no_wcc || fhp->fh_pre_saved)
		return nfs_ok;

	inode = d_inode(fhp->fh_dentry);
	err = fh_getattr(fhp, &stat);
	if (err)
		return err;

	if (v4)
		fhp->fh_pre_change = nfsd4_change_attribute(&stat, inode);

	fhp->fh_pre_mtime = stat.mtime;
	fhp->fh_pre_ctime = stat.ctime;
	fhp->fh_pre_size  = stat.size;
	fhp->fh_pre_saved = true;
	return nfs_ok;
}

 
__be32 fh_fill_post_attrs(struct svc_fh *fhp)
{
	bool v4 = (fhp->fh_maxsize == NFS4_FHSIZE);
	struct inode *inode = d_inode(fhp->fh_dentry);
	__be32 err;

	if (fhp->fh_no_wcc)
		return nfs_ok;

	if (fhp->fh_post_saved)
		printk("nfsd: inode locked twice during operation.\n");

	err = fh_getattr(fhp, &fhp->fh_post_attr);
	if (err)
		return err;

	fhp->fh_post_saved = true;
	if (v4)
		fhp->fh_post_change =
			nfsd4_change_attribute(&fhp->fh_post_attr, inode);
	return nfs_ok;
}

 
__be32 __must_check fh_fill_both_attrs(struct svc_fh *fhp)
{
	__be32 err;

	err = fh_fill_post_attrs(fhp);
	if (err)
		return err;

	fhp->fh_pre_change = fhp->fh_post_change;
	fhp->fh_pre_mtime = fhp->fh_post_attr.mtime;
	fhp->fh_pre_ctime = fhp->fh_post_attr.ctime;
	fhp->fh_pre_size = fhp->fh_post_attr.size;
	fhp->fh_pre_saved = true;
	return nfs_ok;
}

 
void
fh_put(struct svc_fh *fhp)
{
	struct dentry * dentry = fhp->fh_dentry;
	struct svc_export * exp = fhp->fh_export;
	if (dentry) {
		fhp->fh_dentry = NULL;
		dput(dentry);
		fh_clear_pre_post_attrs(fhp);
	}
	fh_drop_write(fhp);
	if (exp) {
		exp_put(exp);
		fhp->fh_export = NULL;
	}
	fhp->fh_no_wcc = false;
	return;
}

 
char * SVCFH_fmt(struct svc_fh *fhp)
{
	struct knfsd_fh *fh = &fhp->fh_handle;
	static char buf[2+1+1+64*3+1];

	if (fh->fh_size < 0 || fh->fh_size> 64)
		return "bad-fh";
	sprintf(buf, "%d: %*ph", fh->fh_size, fh->fh_size, fh->fh_raw);
	return buf;
}

enum fsid_source fsid_source(const struct svc_fh *fhp)
{
	if (fhp->fh_handle.fh_version != 1)
		return FSIDSOURCE_DEV;
	switch(fhp->fh_handle.fh_fsid_type) {
	case FSID_DEV:
	case FSID_ENCODE_DEV:
	case FSID_MAJOR_MINOR:
		if (exp_sb(fhp->fh_export)->s_type->fs_flags & FS_REQUIRES_DEV)
			return FSIDSOURCE_DEV;
		break;
	case FSID_NUM:
		if (fhp->fh_export->ex_flags & NFSEXP_FSID)
			return FSIDSOURCE_FSID;
		break;
	default:
		break;
	}
	 
	if (fhp->fh_export->ex_flags & NFSEXP_FSID)
		return FSIDSOURCE_FSID;
	if (fhp->fh_export->ex_uuid)
		return FSIDSOURCE_UUID;
	return FSIDSOURCE_DEV;
}

 
u64 nfsd4_change_attribute(struct kstat *stat, struct inode *inode)
{
	u64 chattr;

	if (stat->result_mask & STATX_CHANGE_COOKIE) {
		chattr = stat->change_cookie;
		if (S_ISREG(inode->i_mode) &&
		    !(stat->attributes & STATX_ATTR_CHANGE_MONOTONIC)) {
			chattr += (u64)stat->ctime.tv_sec << 30;
			chattr += stat->ctime.tv_nsec;
		}
	} else {
		chattr = time_to_chattr(&stat->ctime);
	}
	return chattr;
}
