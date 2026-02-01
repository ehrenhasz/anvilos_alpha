
 
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/freezer.h>
#include <linux/sched/signal.h>
#include <linux/wait_bit.h>
#include <linux/fiemap.h>
#include <asm/div64.h>
#include "cifsfs.h"
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "smb2proto.h"
#include "cifs_debug.h"
#include "cifs_fs_sb.h"
#include "cifs_unicode.h"
#include "fscache.h"
#include "fs_context.h"
#include "cifs_ioctl.h"
#include "cached_dir.h"

static void cifs_set_ops(struct inode *inode)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_op = &cifs_file_inode_ops;
		if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_DIRECT_IO) {
			if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_NO_BRL)
				inode->i_fop = &cifs_file_direct_nobrl_ops;
			else
				inode->i_fop = &cifs_file_direct_ops;
		} else if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_STRICT_IO) {
			if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_NO_BRL)
				inode->i_fop = &cifs_file_strict_nobrl_ops;
			else
				inode->i_fop = &cifs_file_strict_ops;
		} else if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_NO_BRL)
			inode->i_fop = &cifs_file_nobrl_ops;
		else {  
			inode->i_fop = &cifs_file_ops;
		}

		 
		if (cifs_sb_master_tcon(cifs_sb)->ses->server->max_read <
				PAGE_SIZE + MAX_CIFS_HDR_SIZE)
			inode->i_data.a_ops = &cifs_addr_ops_smallbuf;
		else
			inode->i_data.a_ops = &cifs_addr_ops;
		break;
	case S_IFDIR:
		if (IS_AUTOMOUNT(inode)) {
			inode->i_op = &cifs_namespace_inode_operations;
		} else {
			inode->i_op = &cifs_dir_inode_ops;
			inode->i_fop = &cifs_dir_ops;
		}
		break;
	case S_IFLNK:
		inode->i_op = &cifs_symlink_inode_ops;
		break;
	default:
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
		break;
	}
}

 
static void
cifs_revalidate_cache(struct inode *inode, struct cifs_fattr *fattr)
{
	struct cifs_fscache_inode_coherency_data cd;
	struct cifsInodeInfo *cifs_i = CIFS_I(inode);
	struct timespec64 mtime;

	cifs_dbg(FYI, "%s: revalidating inode %llu\n",
		 __func__, cifs_i->uniqueid);

	if (inode->i_state & I_NEW) {
		cifs_dbg(FYI, "%s: inode %llu is new\n",
			 __func__, cifs_i->uniqueid);
		return;
	}

	 
	if (CIFS_CACHE_READ(cifs_i)) {
		cifs_dbg(FYI, "%s: inode %llu is oplocked\n",
			 __func__, cifs_i->uniqueid);
		return;
	}

	  
	fattr->cf_mtime = timestamp_truncate(fattr->cf_mtime, inode);
	mtime = inode_get_mtime(inode);
	if (timespec64_equal(&mtime, &fattr->cf_mtime) &&
	    cifs_i->server_eof == fattr->cf_eof) {
		cifs_dbg(FYI, "%s: inode %llu is unchanged\n",
			 __func__, cifs_i->uniqueid);
		return;
	}

	cifs_dbg(FYI, "%s: invalidating inode %llu mapping\n",
		 __func__, cifs_i->uniqueid);
	set_bit(CIFS_INO_INVALID_MAPPING, &cifs_i->flags);
	 
	cifs_fscache_fill_coherency(&cifs_i->netfs.inode, &cd);
	fscache_invalidate(cifs_inode_cookie(inode), &cd, i_size_read(inode), 0);
}

 
static void
cifs_nlink_fattr_to_inode(struct inode *inode, struct cifs_fattr *fattr)
{
	 
	if (fattr->cf_flags & CIFS_FATTR_UNKNOWN_NLINK) {
		 
		if (inode->i_state & I_NEW) {
			if (fattr->cf_cifsattrs & ATTR_DIRECTORY)
				set_nlink(inode, 2);
			else
				set_nlink(inode, 1);
		}
		return;
	}

	 
	set_nlink(inode, fattr->cf_nlink);
}

 
int
cifs_fattr_to_inode(struct inode *inode, struct cifs_fattr *fattr)
{
	struct cifsInodeInfo *cifs_i = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);

	if (!(inode->i_state & I_NEW) &&
	    unlikely(inode_wrong_type(inode, fattr->cf_mode))) {
		CIFS_I(inode)->time = 0;  
		return -ESTALE;
	}

	cifs_revalidate_cache(inode, fattr);

	spin_lock(&inode->i_lock);
	fattr->cf_mtime = timestamp_truncate(fattr->cf_mtime, inode);
	fattr->cf_atime = timestamp_truncate(fattr->cf_atime, inode);
	fattr->cf_ctime = timestamp_truncate(fattr->cf_ctime, inode);
	 
	if (timespec64_compare(&fattr->cf_atime, &fattr->cf_mtime) < 0)
		inode_set_atime_to_ts(inode, fattr->cf_mtime);
	else
		inode_set_atime_to_ts(inode, fattr->cf_atime);
	inode_set_mtime_to_ts(inode, fattr->cf_mtime);
	inode_set_ctime_to_ts(inode, fattr->cf_ctime);
	inode->i_rdev = fattr->cf_rdev;
	cifs_nlink_fattr_to_inode(inode, fattr);
	inode->i_uid = fattr->cf_uid;
	inode->i_gid = fattr->cf_gid;

	 
	if (inode->i_state & I_NEW ||
	    !(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_DYNPERM))
		inode->i_mode = fattr->cf_mode;

	cifs_i->cifsAttrs = fattr->cf_cifsattrs;

	if (fattr->cf_flags & CIFS_FATTR_NEED_REVAL)
		cifs_i->time = 0;
	else
		cifs_i->time = jiffies;

	if (fattr->cf_flags & CIFS_FATTR_DELETE_PENDING)
		set_bit(CIFS_INO_DELETE_PENDING, &cifs_i->flags);
	else
		clear_bit(CIFS_INO_DELETE_PENDING, &cifs_i->flags);

	cifs_i->server_eof = fattr->cf_eof;
	 
	if (is_size_safe_to_change(cifs_i, fattr->cf_eof)) {
		i_size_write(inode, fattr->cf_eof);

		 
		inode->i_blocks = (512 - 1 + fattr->cf_bytes) >> 9;
	}

	if (S_ISLNK(fattr->cf_mode)) {
		kfree(cifs_i->symlink_target);
		cifs_i->symlink_target = fattr->cf_symlink_target;
		fattr->cf_symlink_target = NULL;
	}
	spin_unlock(&inode->i_lock);

	if (fattr->cf_flags & CIFS_FATTR_JUNCTION)
		inode->i_flags |= S_AUTOMOUNT;
	if (inode->i_state & I_NEW)
		cifs_set_ops(inode);
	return 0;
}

void
cifs_fill_uniqueid(struct super_block *sb, struct cifs_fattr *fattr)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);

	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SERVER_INUM)
		return;

	fattr->cf_uniqueid = iunique(sb, ROOT_I);
}

 
void
cifs_unix_basic_to_fattr(struct cifs_fattr *fattr, FILE_UNIX_BASIC_INFO *info,
			 struct cifs_sb_info *cifs_sb)
{
	memset(fattr, 0, sizeof(*fattr));
	fattr->cf_uniqueid = le64_to_cpu(info->UniqueId);
	fattr->cf_bytes = le64_to_cpu(info->NumOfBytes);
	fattr->cf_eof = le64_to_cpu(info->EndOfFile);

	fattr->cf_atime = cifs_NTtimeToUnix(info->LastAccessTime);
	fattr->cf_mtime = cifs_NTtimeToUnix(info->LastModificationTime);
	fattr->cf_ctime = cifs_NTtimeToUnix(info->LastStatusChange);
	 

	fattr->cf_mode = le64_to_cpu(info->Permissions);

	 
	fattr->cf_mode &= ~S_IFMT;
	switch (le32_to_cpu(info->Type)) {
	case UNIX_FILE:
		fattr->cf_mode |= S_IFREG;
		fattr->cf_dtype = DT_REG;
		break;
	case UNIX_SYMLINK:
		fattr->cf_mode |= S_IFLNK;
		fattr->cf_dtype = DT_LNK;
		break;
	case UNIX_DIR:
		fattr->cf_mode |= S_IFDIR;
		fattr->cf_dtype = DT_DIR;
		break;
	case UNIX_CHARDEV:
		fattr->cf_mode |= S_IFCHR;
		fattr->cf_dtype = DT_CHR;
		fattr->cf_rdev = MKDEV(le64_to_cpu(info->DevMajor),
				       le64_to_cpu(info->DevMinor) & MINORMASK);
		break;
	case UNIX_BLOCKDEV:
		fattr->cf_mode |= S_IFBLK;
		fattr->cf_dtype = DT_BLK;
		fattr->cf_rdev = MKDEV(le64_to_cpu(info->DevMajor),
				       le64_to_cpu(info->DevMinor) & MINORMASK);
		break;
	case UNIX_FIFO:
		fattr->cf_mode |= S_IFIFO;
		fattr->cf_dtype = DT_FIFO;
		break;
	case UNIX_SOCKET:
		fattr->cf_mode |= S_IFSOCK;
		fattr->cf_dtype = DT_SOCK;
		break;
	default:
		 
		fattr->cf_mode |= S_IFREG;
		fattr->cf_dtype = DT_REG;
		cifs_dbg(FYI, "unknown type %d\n", le32_to_cpu(info->Type));
		break;
	}

	fattr->cf_uid = cifs_sb->ctx->linux_uid;
	if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_OVERR_UID)) {
		u64 id = le64_to_cpu(info->Uid);
		if (id < ((uid_t)-1)) {
			kuid_t uid = make_kuid(&init_user_ns, id);
			if (uid_valid(uid))
				fattr->cf_uid = uid;
		}
	}
	
	fattr->cf_gid = cifs_sb->ctx->linux_gid;
	if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_OVERR_GID)) {
		u64 id = le64_to_cpu(info->Gid);
		if (id < ((gid_t)-1)) {
			kgid_t gid = make_kgid(&init_user_ns, id);
			if (gid_valid(gid))
				fattr->cf_gid = gid;
		}
	}

	fattr->cf_nlink = le64_to_cpu(info->Nlinks);
}

 
static void cifs_create_junction_fattr(struct cifs_fattr *fattr,
				       struct super_block *sb)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);

	cifs_dbg(FYI, "%s: creating fake fattr\n", __func__);

	memset(fattr, 0, sizeof(*fattr));
	fattr->cf_mode = S_IFDIR | S_IXUGO | S_IRWXU;
	fattr->cf_uid = cifs_sb->ctx->linux_uid;
	fattr->cf_gid = cifs_sb->ctx->linux_gid;
	ktime_get_coarse_real_ts64(&fattr->cf_mtime);
	fattr->cf_atime = fattr->cf_ctime = fattr->cf_mtime;
	fattr->cf_nlink = 2;
	fattr->cf_flags = CIFS_FATTR_JUNCTION;
}

 
static int update_inode_info(struct super_block *sb,
			     struct cifs_fattr *fattr,
			     struct inode **inode)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	int rc = 0;

	if (!*inode) {
		*inode = cifs_iget(sb, fattr);
		if (!*inode)
			rc = -ENOMEM;
		return rc;
	}
	 
	if (unlikely((cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SERVER_INUM) &&
		     CIFS_I(*inode)->uniqueid != fattr->cf_uniqueid)) {
		CIFS_I(*inode)->time = 0;  
		return -ESTALE;
	}
	return cifs_fattr_to_inode(*inode, fattr);
}

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
static int
cifs_get_file_info_unix(struct file *filp)
{
	int rc;
	unsigned int xid;
	FILE_UNIX_BASIC_INFO find_data;
	struct cifs_fattr fattr = {};
	struct inode *inode = file_inode(filp);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct cifsFileInfo *cfile = filp->private_data;
	struct cifs_tcon *tcon = tlink_tcon(cfile->tlink);

	xid = get_xid();

	if (cfile->symlink_target) {
		fattr.cf_symlink_target = kstrdup(cfile->symlink_target, GFP_KERNEL);
		if (!fattr.cf_symlink_target) {
			rc = -ENOMEM;
			goto cifs_gfiunix_out;
		}
	}

	rc = CIFSSMBUnixQFileInfo(xid, tcon, cfile->fid.netfid, &find_data);
	if (!rc) {
		cifs_unix_basic_to_fattr(&fattr, &find_data, cifs_sb);
	} else if (rc == -EREMOTE) {
		cifs_create_junction_fattr(&fattr, inode->i_sb);
		rc = 0;
	} else
		goto cifs_gfiunix_out;

	rc = cifs_fattr_to_inode(inode, &fattr);

cifs_gfiunix_out:
	free_xid(xid);
	return rc;
}

static int cifs_get_unix_fattr(const unsigned char *full_path,
			       struct super_block *sb,
			       struct cifs_fattr *fattr,
			       struct inode **pinode,
			       const unsigned int xid)
{
	struct TCP_Server_Info *server;
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	FILE_UNIX_BASIC_INFO find_data;
	struct cifs_tcon *tcon;
	struct tcon_link *tlink;
	int rc, tmprc;

	cifs_dbg(FYI, "Getting info on %s\n", full_path);

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);
	server = tcon->ses->server;

	 
	rc = CIFSSMBUnixQPathInfo(xid, tcon, full_path, &find_data,
				  cifs_sb->local_nls, cifs_remap(cifs_sb));
	cifs_dbg(FYI, "%s: query path info: rc = %d\n", __func__, rc);
	cifs_put_tlink(tlink);

	if (!rc) {
		cifs_unix_basic_to_fattr(fattr, &find_data, cifs_sb);
	} else if (rc == -EREMOTE) {
		cifs_create_junction_fattr(fattr, sb);
		rc = 0;
	} else {
		return rc;
	}

	if (!*pinode)
		cifs_fill_uniqueid(sb, fattr);

	 
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MF_SYMLINKS) {
		tmprc = check_mf_symlink(xid, tcon, cifs_sb, fattr, full_path);
		cifs_dbg(FYI, "check_mf_symlink: %d\n", tmprc);
	}

	if (S_ISLNK(fattr->cf_mode) && !fattr->cf_symlink_target) {
		if (!server->ops->query_symlink)
			return -EOPNOTSUPP;
		rc = server->ops->query_symlink(xid, tcon,
						cifs_sb, full_path,
						&fattr->cf_symlink_target);
		cifs_dbg(FYI, "%s: query_symlink: %d\n", __func__, rc);
	}
	return rc;
}

int cifs_get_inode_info_unix(struct inode **pinode,
			     const unsigned char *full_path,
			     struct super_block *sb, unsigned int xid)
{
	struct cifs_fattr fattr = {};
	int rc;

	rc = cifs_get_unix_fattr(full_path, sb, &fattr, pinode, xid);
	if (rc)
		goto out;

	rc = update_inode_info(sb, &fattr, pinode);
out:
	kfree(fattr.cf_symlink_target);
	return rc;
}
#else
static inline int cifs_get_unix_fattr(const unsigned char *full_path,
				      struct super_block *sb,
				      struct cifs_fattr *fattr,
				      struct inode **pinode,
				      const unsigned int xid)
{
	return -EOPNOTSUPP;
}

int cifs_get_inode_info_unix(struct inode **pinode,
			     const unsigned char *full_path,
			     struct super_block *sb, unsigned int xid)
{
	return -EOPNOTSUPP;
}
#endif  

static int
cifs_sfu_type(struct cifs_fattr *fattr, const char *path,
	      struct cifs_sb_info *cifs_sb, unsigned int xid)
{
	int rc;
	__u32 oplock;
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	struct cifs_fid fid;
	struct cifs_open_parms oparms;
	struct cifs_io_parms io_parms = {0};
	char buf[24];
	unsigned int bytes_read;
	char *pbuf;
	int buf_type = CIFS_NO_BUFFER;

	pbuf = buf;

	fattr->cf_mode &= ~S_IFMT;

	if (fattr->cf_eof == 0) {
		fattr->cf_mode |= S_IFIFO;
		fattr->cf_dtype = DT_FIFO;
		return 0;
	} else if (fattr->cf_eof < 8) {
		fattr->cf_mode |= S_IFREG;
		fattr->cf_dtype = DT_REG;
		return -EINVAL;	  
	}

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);

	oparms = (struct cifs_open_parms) {
		.tcon = tcon,
		.cifs_sb = cifs_sb,
		.desired_access = GENERIC_READ,
		.create_options = cifs_create_options(cifs_sb, CREATE_NOT_DIR),
		.disposition = FILE_OPEN,
		.path = path,
		.fid = &fid,
	};

	if (tcon->ses->server->oplocks)
		oplock = REQ_OPLOCK;
	else
		oplock = 0;
	rc = tcon->ses->server->ops->open(xid, &oparms, &oplock, NULL);
	if (rc) {
		cifs_dbg(FYI, "check sfu type of %s, open rc = %d\n", path, rc);
		cifs_put_tlink(tlink);
		return rc;
	}

	 
	io_parms.netfid = fid.netfid;
	io_parms.pid = current->tgid;
	io_parms.tcon = tcon;
	io_parms.offset = 0;
	io_parms.length = 24;

	rc = tcon->ses->server->ops->sync_read(xid, &fid, &io_parms,
					&bytes_read, &pbuf, &buf_type);
	if ((rc == 0) && (bytes_read >= 8)) {
		if (memcmp("IntxBLK", pbuf, 8) == 0) {
			cifs_dbg(FYI, "Block device\n");
			fattr->cf_mode |= S_IFBLK;
			fattr->cf_dtype = DT_BLK;
			if (bytes_read == 24) {
				 
				__u64 mjr;  
				__u64 mnr;  
				mjr = le64_to_cpu(*(__le64 *)(pbuf+8));
				mnr = le64_to_cpu(*(__le64 *)(pbuf+16));
				fattr->cf_rdev = MKDEV(mjr, mnr);
			}
		} else if (memcmp("IntxCHR", pbuf, 8) == 0) {
			cifs_dbg(FYI, "Char device\n");
			fattr->cf_mode |= S_IFCHR;
			fattr->cf_dtype = DT_CHR;
			if (bytes_read == 24) {
				 
				__u64 mjr;  
				__u64 mnr;  
				mjr = le64_to_cpu(*(__le64 *)(pbuf+8));
				mnr = le64_to_cpu(*(__le64 *)(pbuf+16));
				fattr->cf_rdev = MKDEV(mjr, mnr);
			}
		} else if (memcmp("IntxLNK", pbuf, 7) == 0) {
			cifs_dbg(FYI, "Symlink\n");
			fattr->cf_mode |= S_IFLNK;
			fattr->cf_dtype = DT_LNK;
		} else if (memcmp("LnxFIFO", pbuf, 8) == 0) {
			cifs_dbg(FYI, "FIFO\n");
			fattr->cf_mode |= S_IFIFO;
			fattr->cf_dtype = DT_FIFO;
		} else {
			fattr->cf_mode |= S_IFREG;  
			fattr->cf_dtype = DT_REG;
			rc = -EOPNOTSUPP;
		}
	} else {
		fattr->cf_mode |= S_IFREG;  
		fattr->cf_dtype = DT_REG;
		rc = -EOPNOTSUPP;  
	}

	tcon->ses->server->ops->close(xid, tcon, &fid);
	cifs_put_tlink(tlink);
	return rc;
}

#define SFBITS_MASK (S_ISVTX | S_ISGID | S_ISUID)   

 
static int cifs_sfu_mode(struct cifs_fattr *fattr, const unsigned char *path,
			 struct cifs_sb_info *cifs_sb, unsigned int xid)
{
#ifdef CONFIG_CIFS_XATTR
	ssize_t rc;
	char ea_value[4];
	__u32 mode;
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);

	if (tcon->ses->server->ops->query_all_EAs == NULL) {
		cifs_put_tlink(tlink);
		return -EOPNOTSUPP;
	}

	rc = tcon->ses->server->ops->query_all_EAs(xid, tcon, path,
			"SETFILEBITS", ea_value, 4  ,
			cifs_sb);
	cifs_put_tlink(tlink);
	if (rc < 0)
		return (int)rc;
	else if (rc > 3) {
		mode = le32_to_cpu(*((__le32 *)ea_value));
		fattr->cf_mode &= ~SFBITS_MASK;
		cifs_dbg(FYI, "special bits 0%o org mode 0%o\n",
			 mode, fattr->cf_mode);
		fattr->cf_mode = (mode & SFBITS_MASK) | fattr->cf_mode;
		cifs_dbg(FYI, "special mode bits 0%o\n", mode);
	}

	return 0;
#else
	return -EOPNOTSUPP;
#endif
}

 
static void smb311_posix_info_to_fattr(struct cifs_fattr *fattr,
				       struct cifs_open_info_data *data,
				       struct cifs_sid *owner,
				       struct cifs_sid *group,
				       struct super_block *sb)
{
	struct smb311_posix_qinfo *info = &data->posix_fi;
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct cifs_tcon *tcon = cifs_sb_master_tcon(cifs_sb);

	memset(fattr, 0, sizeof(*fattr));

	 
	fattr->cf_cifsattrs = le32_to_cpu(info->DosAttributes);
	fattr->cf_uniqueid = le64_to_cpu(info->Inode);

	if (info->LastAccessTime)
		fattr->cf_atime = cifs_NTtimeToUnix(info->LastAccessTime);
	else
		ktime_get_coarse_real_ts64(&fattr->cf_atime);

	fattr->cf_ctime = cifs_NTtimeToUnix(info->ChangeTime);
	fattr->cf_mtime = cifs_NTtimeToUnix(info->LastWriteTime);

	if (data->adjust_tz) {
		fattr->cf_ctime.tv_sec += tcon->ses->server->timeAdj;
		fattr->cf_mtime.tv_sec += tcon->ses->server->timeAdj;
	}

	fattr->cf_eof = le64_to_cpu(info->EndOfFile);
	fattr->cf_bytes = le64_to_cpu(info->AllocationSize);
	fattr->cf_createtime = le64_to_cpu(info->CreationTime);

	fattr->cf_nlink = le32_to_cpu(info->HardLinks);
	fattr->cf_mode = (umode_t) le32_to_cpu(info->Mode);
	 
	 

	if (data->symlink) {
		fattr->cf_mode |= S_IFLNK;
		fattr->cf_dtype = DT_LNK;
		fattr->cf_symlink_target = data->symlink_target;
		data->symlink_target = NULL;
	} else if (fattr->cf_cifsattrs & ATTR_DIRECTORY) {
		fattr->cf_mode |= S_IFDIR;
		fattr->cf_dtype = DT_DIR;
	} else {  
		fattr->cf_mode |= S_IFREG;
		fattr->cf_dtype = DT_REG;
	}
	 

	sid_to_id(cifs_sb, owner, fattr, SIDOWNER);
	sid_to_id(cifs_sb, group, fattr, SIDGROUP);

	cifs_dbg(FYI, "POSIX query info: mode 0x%x uniqueid 0x%llx nlink %d\n",
		fattr->cf_mode, fattr->cf_uniqueid, fattr->cf_nlink);
}

static inline dev_t nfs_mkdev(struct reparse_posix_data *buf)
{
	u64 v = le64_to_cpu(*(__le64 *)buf->DataBuffer);

	return MKDEV(v >> 32, v & 0xffffffff);
}

bool cifs_reparse_point_to_fattr(struct cifs_sb_info *cifs_sb,
				 struct cifs_fattr *fattr,
				 struct cifs_open_info_data *data)
{
	struct reparse_posix_data *buf = data->reparse.posix;
	u32 tag = data->reparse.tag;

	if (tag == IO_REPARSE_TAG_NFS && buf) {
		switch (le64_to_cpu(buf->InodeType)) {
		case NFS_SPECFILE_CHR:
			fattr->cf_mode |= S_IFCHR | cifs_sb->ctx->file_mode;
			fattr->cf_dtype = DT_CHR;
			fattr->cf_rdev = nfs_mkdev(buf);
			break;
		case NFS_SPECFILE_BLK:
			fattr->cf_mode |= S_IFBLK | cifs_sb->ctx->file_mode;
			fattr->cf_dtype = DT_BLK;
			fattr->cf_rdev = nfs_mkdev(buf);
			break;
		case NFS_SPECFILE_FIFO:
			fattr->cf_mode |= S_IFIFO | cifs_sb->ctx->file_mode;
			fattr->cf_dtype = DT_FIFO;
			break;
		case NFS_SPECFILE_SOCK:
			fattr->cf_mode |= S_IFSOCK | cifs_sb->ctx->file_mode;
			fattr->cf_dtype = DT_SOCK;
			break;
		case NFS_SPECFILE_LNK:
			fattr->cf_mode = S_IFLNK | cifs_sb->ctx->file_mode;
			fattr->cf_dtype = DT_LNK;
			break;
		default:
			WARN_ON_ONCE(1);
			return false;
		}
		return true;
	}

	switch (tag) {
	case IO_REPARSE_TAG_LX_SYMLINK:
		fattr->cf_mode |= S_IFLNK | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_LNK;
		break;
	case IO_REPARSE_TAG_LX_FIFO:
		fattr->cf_mode |= S_IFIFO | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_FIFO;
		break;
	case IO_REPARSE_TAG_AF_UNIX:
		fattr->cf_mode |= S_IFSOCK | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_SOCK;
		break;
	case IO_REPARSE_TAG_LX_CHR:
		fattr->cf_mode |= S_IFCHR | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_CHR;
		break;
	case IO_REPARSE_TAG_LX_BLK:
		fattr->cf_mode |= S_IFBLK | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_BLK;
		break;
	case 0:  
	case IO_REPARSE_TAG_SYMLINK:
	case IO_REPARSE_TAG_NFS:
		fattr->cf_mode = S_IFLNK | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_LNK;
		break;
	default:
		return false;
	}
	return true;
}

static void cifs_open_info_to_fattr(struct cifs_fattr *fattr,
				    struct cifs_open_info_data *data,
				    struct super_block *sb)
{
	struct smb2_file_all_info *info = &data->fi;
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct cifs_tcon *tcon = cifs_sb_master_tcon(cifs_sb);

	memset(fattr, 0, sizeof(*fattr));
	fattr->cf_cifsattrs = le32_to_cpu(info->Attributes);
	if (info->DeletePending)
		fattr->cf_flags |= CIFS_FATTR_DELETE_PENDING;

	if (info->LastAccessTime)
		fattr->cf_atime = cifs_NTtimeToUnix(info->LastAccessTime);
	else
		ktime_get_coarse_real_ts64(&fattr->cf_atime);

	fattr->cf_ctime = cifs_NTtimeToUnix(info->ChangeTime);
	fattr->cf_mtime = cifs_NTtimeToUnix(info->LastWriteTime);

	if (data->adjust_tz) {
		fattr->cf_ctime.tv_sec += tcon->ses->server->timeAdj;
		fattr->cf_mtime.tv_sec += tcon->ses->server->timeAdj;
	}

	fattr->cf_eof = le64_to_cpu(info->EndOfFile);
	fattr->cf_bytes = le64_to_cpu(info->AllocationSize);
	fattr->cf_createtime = le64_to_cpu(info->CreationTime);
	fattr->cf_nlink = le32_to_cpu(info->NumberOfLinks);

	if (cifs_open_data_reparse(data) &&
	    cifs_reparse_point_to_fattr(cifs_sb, fattr, data))
		goto out_reparse;

	if (fattr->cf_cifsattrs & ATTR_DIRECTORY) {
		fattr->cf_mode = S_IFDIR | cifs_sb->ctx->dir_mode;
		fattr->cf_dtype = DT_DIR;
		 
		if (!tcon->unix_ext)
			fattr->cf_flags |= CIFS_FATTR_UNKNOWN_NLINK;
	} else {
		fattr->cf_mode = S_IFREG | cifs_sb->ctx->file_mode;
		fattr->cf_dtype = DT_REG;

		 
		if (fattr->cf_cifsattrs & ATTR_READONLY)
			fattr->cf_mode &= ~(S_IWUGO);

		 
		if ((fattr->cf_nlink < 1) && !tcon->unix_ext &&
		    !info->DeletePending) {
			cifs_dbg(VFS, "bogus file nlink value %u\n",
				 fattr->cf_nlink);
			fattr->cf_flags |= CIFS_FATTR_UNKNOWN_NLINK;
		}
	}

out_reparse:
	if (S_ISLNK(fattr->cf_mode)) {
		if (likely(data->symlink_target))
			fattr->cf_eof = strnlen(data->symlink_target, PATH_MAX);
		fattr->cf_symlink_target = data->symlink_target;
		data->symlink_target = NULL;
	}

	fattr->cf_uid = cifs_sb->ctx->linux_uid;
	fattr->cf_gid = cifs_sb->ctx->linux_gid;
}

static int
cifs_get_file_info(struct file *filp)
{
	int rc;
	unsigned int xid;
	struct cifs_open_info_data data = {};
	struct cifs_fattr fattr;
	struct inode *inode = file_inode(filp);
	struct cifsFileInfo *cfile = filp->private_data;
	struct cifs_tcon *tcon = tlink_tcon(cfile->tlink);
	struct TCP_Server_Info *server = tcon->ses->server;

	if (!server->ops->query_file_info)
		return -ENOSYS;

	xid = get_xid();
	rc = server->ops->query_file_info(xid, tcon, cfile, &data);
	switch (rc) {
	case 0:
		 
		data.adjust_tz = false;
		if (data.symlink_target) {
			data.symlink = true;
			data.reparse.tag = IO_REPARSE_TAG_SYMLINK;
		}
		cifs_open_info_to_fattr(&fattr, &data, inode->i_sb);
		break;
	case -EREMOTE:
		cifs_create_junction_fattr(&fattr, inode->i_sb);
		rc = 0;
		break;
	case -EOPNOTSUPP:
	case -EINVAL:
		 
		rc = 0;
		CIFS_I(inode)->time = 0;
		goto cgfi_exit;
	default:
		goto cgfi_exit;
	}

	 
	fattr.cf_uniqueid = CIFS_I(inode)->uniqueid;
	fattr.cf_flags |= CIFS_FATTR_NEED_REVAL;
	 
	rc = cifs_fattr_to_inode(inode, &fattr);
cgfi_exit:
	cifs_free_open_info(&data);
	free_xid(xid);
	return rc;
}

 
static __u64 simple_hashstr(const char *str)
{
	const __u64 hash_mult =  1125899906842597ULL;  
	__u64 hash = 0;

	while (*str)
		hash = (hash + (__u64) *str++) * hash_mult;

	return hash;
}

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
 
static int
cifs_backup_query_path_info(int xid,
			    struct cifs_tcon *tcon,
			    struct super_block *sb,
			    const char *full_path,
			    void **resp_buf,
			    FILE_ALL_INFO **data)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct cifs_search_info info = {0};
	u16 flags;
	int rc;

	*resp_buf = NULL;
	info.endOfSearch = false;
	if (tcon->unix_ext)
		info.info_level = SMB_FIND_FILE_UNIX;
	else if ((tcon->ses->capabilities &
		  tcon->ses->server->vals->cap_nt_find) == 0)
		info.info_level = SMB_FIND_FILE_INFO_STANDARD;
	else if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SERVER_INUM)
		info.info_level = SMB_FIND_FILE_ID_FULL_DIR_INFO;
	else  
		info.info_level = SMB_FIND_FILE_DIRECTORY_INFO;

	flags = CIFS_SEARCH_CLOSE_ALWAYS |
		CIFS_SEARCH_CLOSE_AT_END |
		CIFS_SEARCH_BACKUP_SEARCH;

	rc = CIFSFindFirst(xid, tcon, full_path,
			   cifs_sb, NULL, flags, &info, false);
	if (rc)
		return rc;

	*resp_buf = (void *)info.ntwrk_buf_start;
	*data = (FILE_ALL_INFO *)info.srch_entries_start;
	return 0;
}
#endif  

static void cifs_set_fattr_ino(int xid, struct cifs_tcon *tcon, struct super_block *sb,
			       struct inode **inode, const char *full_path,
			       struct cifs_open_info_data *data, struct cifs_fattr *fattr)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct TCP_Server_Info *server = tcon->ses->server;
	int rc;

	if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SERVER_INUM)) {
		if (*inode)
			fattr->cf_uniqueid = CIFS_I(*inode)->uniqueid;
		else
			fattr->cf_uniqueid = iunique(sb, ROOT_I);
		return;
	}

	 
	rc = server->ops->get_srv_inum(xid, *inode ? NULL : tcon, cifs_sb, full_path,
				       &fattr->cf_uniqueid, data);
	if (rc) {
		 
		if (*inode)
			fattr->cf_uniqueid = CIFS_I(*inode)->uniqueid;
		else {
			fattr->cf_uniqueid = iunique(sb, ROOT_I);
			cifs_autodisable_serverino(cifs_sb);
		}
		return;
	}

	 
	if (fattr->cf_uniqueid == 0 && strlen(full_path) == 0) {
		cifs_dbg(FYI, "Invalid (0) inodenum\n");
		if (*inode) {
			 
			fattr->cf_uniqueid = CIFS_I(*inode)->uniqueid;
		} else {
			 
			fattr->cf_flags |= CIFS_FATTR_FAKE_ROOT_INO;
			fattr->cf_uniqueid = simple_hashstr(tcon->tree_name);
		}
	}
}

static inline bool is_inode_cache_good(struct inode *ino)
{
	return ino && CIFS_CACHE_READ(CIFS_I(ino)) && CIFS_I(ino)->time != 0;
}

static int reparse_info_to_fattr(struct cifs_open_info_data *data,
				 struct super_block *sb,
				 const unsigned int xid,
				 struct cifs_tcon *tcon,
				 const char *full_path,
				 struct cifs_fattr *fattr)
{
	struct TCP_Server_Info *server = tcon->ses->server;
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct kvec rsp_iov, *iov = NULL;
	int rsp_buftype = CIFS_NO_BUFFER;
	u32 tag = data->reparse.tag;
	int rc = 0;

	if (!tag && server->ops->query_reparse_point) {
		rc = server->ops->query_reparse_point(xid, tcon, cifs_sb,
						      full_path, &tag,
						      &rsp_iov, &rsp_buftype);
		if (!rc)
			iov = &rsp_iov;
	}

	rc = -EOPNOTSUPP;
	switch ((data->reparse.tag = tag)) {
	case 0:  
		if (server->ops->query_symlink) {
			rc = server->ops->query_symlink(xid, tcon,
							cifs_sb, full_path,
							&data->symlink_target);
		}
		break;
	case IO_REPARSE_TAG_MOUNT_POINT:
		cifs_create_junction_fattr(fattr, sb);
		rc = 0;
		goto out;
	default:
		if (data->symlink_target) {
			rc = 0;
		} else if (server->ops->parse_reparse_point) {
			rc = server->ops->parse_reparse_point(cifs_sb,
							      iov, data);
		}
		break;
	}

	cifs_open_info_to_fattr(fattr, data, sb);
out:
	free_rsp_buf(rsp_buftype, rsp_iov.iov_base);
	return rc;
}

static int cifs_get_fattr(struct cifs_open_info_data *data,
			  struct super_block *sb, int xid,
			  const struct cifs_fid *fid,
			  struct cifs_fattr *fattr,
			  struct inode **inode,
			  const char *full_path)
{
	struct cifs_open_info_data tmp_data = {};
	struct cifs_tcon *tcon;
	struct TCP_Server_Info *server;
	struct tcon_link *tlink;
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	void *smb1_backup_rsp_buf = NULL;
	int rc = 0;
	int tmprc = 0;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);
	server = tcon->ses->server;

	 

	if (!data) {
		rc = server->ops->query_path_info(xid, tcon, cifs_sb,
						  full_path, &tmp_data);
		data = &tmp_data;
	}

	 

	switch (rc) {
	case 0:
		 
		if (cifs_open_data_reparse(data)) {
			rc = reparse_info_to_fattr(data, sb, xid, tcon,
						   full_path, fattr);
		} else {
			cifs_open_info_to_fattr(fattr, data, sb);
		}
		break;
	case -EREMOTE:
		 
		cifs_create_junction_fattr(fattr, sb);
		rc = 0;
		break;
	case -EACCES:
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
		 
		if (backup_cred(cifs_sb) && is_smb1_server(server)) {
			 
			FILE_ALL_INFO *fi;
			FILE_DIRECTORY_INFO *fdi;
			SEARCH_ID_FULL_DIR_INFO *si;

			rc = cifs_backup_query_path_info(xid, tcon, sb,
							 full_path,
							 &smb1_backup_rsp_buf,
							 &fi);
			if (rc)
				goto out;

			move_cifs_info_to_smb2(&data->fi, fi);
			fdi = (FILE_DIRECTORY_INFO *)fi;
			si = (SEARCH_ID_FULL_DIR_INFO *)fi;

			cifs_dir_info_to_fattr(fattr, fdi, cifs_sb);
			fattr->cf_uniqueid = le64_to_cpu(si->UniqueId);
			 
			goto handle_mnt_opt;
		} else {
			 
			goto out;
		}
#else
		goto out;
#endif  
		break;
	default:
		cifs_dbg(FYI, "%s: unhandled err rc %d\n", __func__, rc);
		goto out;
	}

	 

	cifs_set_fattr_ino(xid, tcon, sb, inode, full_path, data, fattr);

	 
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
handle_mnt_opt:
#endif  
	 
	if ((fattr->cf_cifsattrs & ATTR_SYSTEM) &&
	    (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_UNX_EMUL)) {
		tmprc = cifs_sfu_type(fattr, full_path, cifs_sb, xid);
		if (tmprc)
			cifs_dbg(FYI, "cifs_sfu_type failed: %d\n", tmprc);
	}

	 
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MODE_FROM_SID) {
		rc = cifs_acl_to_fattr(cifs_sb, fattr, *inode,
				       true, full_path, fid);
		if (rc == -EREMOTE)
			rc = 0;
		if (rc) {
			cifs_dbg(FYI, "%s: Get mode from SID failed. rc=%d\n",
				 __func__, rc);
			goto out;
		}
	} else if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_CIFS_ACL) {
		rc = cifs_acl_to_fattr(cifs_sb, fattr, *inode,
				       false, full_path, fid);
		if (rc == -EREMOTE)
			rc = 0;
		if (rc) {
			cifs_dbg(FYI, "%s: Getting ACL failed with error: %d\n",
				 __func__, rc);
			goto out;
		}
	}

	 
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_UNX_EMUL)
		cifs_sfu_mode(fattr, full_path, cifs_sb, xid);

	 
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MF_SYMLINKS) {
		tmprc = check_mf_symlink(xid, tcon, cifs_sb, fattr, full_path);
		cifs_dbg(FYI, "check_mf_symlink: %d\n", tmprc);
	}

out:
	cifs_buf_release(smb1_backup_rsp_buf);
	cifs_put_tlink(tlink);
	cifs_free_open_info(&tmp_data);
	return rc;
}

int cifs_get_inode_info(struct inode **inode,
			const char *full_path,
			struct cifs_open_info_data *data,
			struct super_block *sb, int xid,
			const struct cifs_fid *fid)
{
	struct cifs_fattr fattr = {};
	int rc;

	if (is_inode_cache_good(*inode)) {
		cifs_dbg(FYI, "No need to revalidate cached inode sizes\n");
		return 0;
	}

	rc = cifs_get_fattr(data, sb, xid, fid, &fattr, inode, full_path);
	if (rc)
		goto out;

	rc = update_inode_info(sb, &fattr, inode);
out:
	kfree(fattr.cf_symlink_target);
	return rc;
}

static int smb311_posix_get_fattr(struct cifs_fattr *fattr,
				  const char *full_path,
				  struct super_block *sb,
				  const unsigned int xid)
{
	struct cifs_open_info_data data = {};
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct cifs_tcon *tcon;
	struct tcon_link *tlink;
	struct cifs_sid owner, group;
	int tmprc;
	int rc;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);

	 

	rc = smb311_posix_query_path_info(xid, tcon, cifs_sb,
					  full_path, &data,
					  &owner, &group);

	 

	switch (rc) {
	case 0:
		smb311_posix_info_to_fattr(fattr, &data, &owner, &group, sb);
		break;
	case -EREMOTE:
		 
		cifs_create_junction_fattr(fattr, sb);
		rc = 0;
		break;
	case -EACCES:
		 
		goto out;
	default:
		cifs_dbg(FYI, "%s: unhandled err rc %d\n", __func__, rc);
		goto out;
	}

	 
	 
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MF_SYMLINKS) {
		tmprc = check_mf_symlink(xid, tcon, cifs_sb, fattr, full_path);
		cifs_dbg(FYI, "check_mf_symlink: %d\n", tmprc);
	}

out:
	cifs_put_tlink(tlink);
	cifs_free_open_info(&data);
	return rc;
}

int smb311_posix_get_inode_info(struct inode **inode, const char *full_path,
				struct super_block *sb, const unsigned int xid)
{
	struct cifs_fattr fattr = {};
	int rc;

	if (is_inode_cache_good(*inode)) {
		cifs_dbg(FYI, "No need to revalidate cached inode sizes\n");
		return 0;
	}

	rc = smb311_posix_get_fattr(&fattr, full_path, sb, xid);
	if (rc)
		goto out;

	rc = update_inode_info(sb, &fattr, inode);
out:
	kfree(fattr.cf_symlink_target);
	return rc;
}

static const struct inode_operations cifs_ipc_inode_ops = {
	.lookup = cifs_lookup,
};

static int
cifs_find_inode(struct inode *inode, void *opaque)
{
	struct cifs_fattr *fattr = opaque;

	 
	if (CIFS_I(inode)->uniqueid != fattr->cf_uniqueid)
		return 0;

	 
	if (CIFS_I(inode)->createtime != fattr->cf_createtime)
		return 0;

	 
	if (inode_wrong_type(inode, fattr->cf_mode))
		return 0;

	 
	if (S_ISDIR(inode->i_mode) && !hlist_empty(&inode->i_dentry))
		fattr->cf_flags |= CIFS_FATTR_INO_COLLISION;

	return 1;
}

static int
cifs_init_inode(struct inode *inode, void *opaque)
{
	struct cifs_fattr *fattr = opaque;

	CIFS_I(inode)->uniqueid = fattr->cf_uniqueid;
	CIFS_I(inode)->createtime = fattr->cf_createtime;
	return 0;
}

 
static bool
inode_has_hashed_dentries(struct inode *inode)
{
	struct dentry *dentry;

	spin_lock(&inode->i_lock);
	hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
		if (!d_unhashed(dentry) || IS_ROOT(dentry)) {
			spin_unlock(&inode->i_lock);
			return true;
		}
	}
	spin_unlock(&inode->i_lock);
	return false;
}

 
struct inode *
cifs_iget(struct super_block *sb, struct cifs_fattr *fattr)
{
	unsigned long hash;
	struct inode *inode;

retry_iget5_locked:
	cifs_dbg(FYI, "looking for uniqueid=%llu\n", fattr->cf_uniqueid);

	 
	hash = cifs_uniqueid_to_ino_t(fattr->cf_uniqueid);

	inode = iget5_locked(sb, hash, cifs_find_inode, cifs_init_inode, fattr);
	if (inode) {
		 
		if (fattr->cf_flags & CIFS_FATTR_INO_COLLISION) {
			fattr->cf_flags &= ~CIFS_FATTR_INO_COLLISION;

			if (inode_has_hashed_dentries(inode)) {
				cifs_autodisable_serverino(CIFS_SB(sb));
				iput(inode);
				fattr->cf_uniqueid = iunique(sb, ROOT_I);
				goto retry_iget5_locked;
			}
		}

		 
		cifs_fattr_to_inode(inode, fattr);
		if (sb->s_flags & SB_NOATIME)
			inode->i_flags |= S_NOATIME | S_NOCMTIME;
		if (inode->i_state & I_NEW) {
			inode->i_ino = hash;
			cifs_fscache_get_inode_cookie(inode);
			unlock_new_inode(inode);
		}
	}

	return inode;
}

 
struct inode *cifs_root_iget(struct super_block *sb)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct cifs_fattr fattr = {};
	struct cifs_tcon *tcon = cifs_sb_master_tcon(cifs_sb);
	struct inode *inode = NULL;
	unsigned int xid;
	char *path = NULL;
	int len;
	int rc;

	if ((cifs_sb->mnt_cifs_flags & CIFS_MOUNT_USE_PREFIX_PATH)
	    && cifs_sb->prepath) {
		len = strlen(cifs_sb->prepath);
		path = kzalloc(len + 2  , GFP_KERNEL);
		if (path == NULL)
			return ERR_PTR(-ENOMEM);
		path[0] = '/';
		memcpy(path+1, cifs_sb->prepath, len);
	} else {
		path = kstrdup("", GFP_KERNEL);
		if (path == NULL)
			return ERR_PTR(-ENOMEM);
	}

	xid = get_xid();
	if (tcon->unix_ext) {
		rc = cifs_get_unix_fattr(path, sb, &fattr, &inode, xid);
		 
		if (rc != -EOPNOTSUPP)
			goto iget_root;
		cifs_dbg(VFS, "server does not support POSIX extensions\n");
		tcon->unix_ext = false;
	}

	convert_delimiter(path, CIFS_DIR_SEP(cifs_sb));
	if (tcon->posix_extensions)
		rc = smb311_posix_get_fattr(&fattr, path, sb, xid);
	else
		rc = cifs_get_fattr(NULL, sb, xid, NULL, &fattr, &inode, path);

iget_root:
	if (!rc) {
		if (fattr.cf_flags & CIFS_FATTR_JUNCTION) {
			fattr.cf_flags &= ~CIFS_FATTR_JUNCTION;
			cifs_autodisable_serverino(cifs_sb);
		}
		inode = cifs_iget(sb, &fattr);
	}

	if (!inode) {
		inode = ERR_PTR(rc);
		goto out;
	}

	if (rc && tcon->pipe) {
		cifs_dbg(FYI, "ipc connection - fake read inode\n");
		spin_lock(&inode->i_lock);
		inode->i_mode |= S_IFDIR;
		set_nlink(inode, 2);
		inode->i_op = &cifs_ipc_inode_ops;
		inode->i_fop = &simple_dir_operations;
		inode->i_uid = cifs_sb->ctx->linux_uid;
		inode->i_gid = cifs_sb->ctx->linux_gid;
		spin_unlock(&inode->i_lock);
	} else if (rc) {
		iget_failed(inode);
		inode = ERR_PTR(rc);
	}

out:
	kfree(path);
	free_xid(xid);
	kfree(fattr.cf_symlink_target);
	return inode;
}

int
cifs_set_file_info(struct inode *inode, struct iattr *attrs, unsigned int xid,
		   const char *full_path, __u32 dosattr)
{
	bool set_time = false;
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct TCP_Server_Info *server;
	FILE_BASIC_INFO	info_buf;

	if (attrs == NULL)
		return -EINVAL;

	server = cifs_sb_master_tcon(cifs_sb)->ses->server;
	if (!server->ops->set_file_info)
		return -ENOSYS;

	info_buf.Pad = 0;

	if (attrs->ia_valid & ATTR_ATIME) {
		set_time = true;
		info_buf.LastAccessTime =
			cpu_to_le64(cifs_UnixTimeToNT(attrs->ia_atime));
	} else
		info_buf.LastAccessTime = 0;

	if (attrs->ia_valid & ATTR_MTIME) {
		set_time = true;
		info_buf.LastWriteTime =
		    cpu_to_le64(cifs_UnixTimeToNT(attrs->ia_mtime));
	} else
		info_buf.LastWriteTime = 0;

	 
	if (set_time && (attrs->ia_valid & ATTR_CTIME)) {
		cifs_dbg(FYI, "CIFS - CTIME changed\n");
		info_buf.ChangeTime =
		    cpu_to_le64(cifs_UnixTimeToNT(attrs->ia_ctime));
	} else
		info_buf.ChangeTime = 0;

	info_buf.CreationTime = 0;	 
	info_buf.Attributes = cpu_to_le32(dosattr);

	return server->ops->set_file_info(inode, full_path, &info_buf, xid);
}

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
 
int
cifs_rename_pending_delete(const char *full_path, struct dentry *dentry,
			   const unsigned int xid)
{
	int oplock = 0;
	int rc;
	struct cifs_fid fid;
	struct cifs_open_parms oparms;
	struct inode *inode = d_inode(dentry);
	struct cifsInodeInfo *cifsInode = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	__u32 dosattr, origattr;
	FILE_BASIC_INFO *info_buf = NULL;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);

	 
	if (!(tcon->ses->capabilities & CAP_INFOLEVEL_PASSTHRU)) {
		rc = -EBUSY;
		goto out;
	}

	oparms = (struct cifs_open_parms) {
		.tcon = tcon,
		.cifs_sb = cifs_sb,
		.desired_access = DELETE | FILE_WRITE_ATTRIBUTES,
		.create_options = cifs_create_options(cifs_sb, CREATE_NOT_DIR),
		.disposition = FILE_OPEN,
		.path = full_path,
		.fid = &fid,
	};

	rc = CIFS_open(xid, &oparms, &oplock, NULL);
	if (rc != 0)
		goto out;

	origattr = cifsInode->cifsAttrs;
	if (origattr == 0)
		origattr |= ATTR_NORMAL;

	dosattr = origattr & ~ATTR_READONLY;
	if (dosattr == 0)
		dosattr |= ATTR_NORMAL;
	dosattr |= ATTR_HIDDEN;

	 
	if (dosattr != origattr) {
		info_buf = kzalloc(sizeof(*info_buf), GFP_KERNEL);
		if (info_buf == NULL) {
			rc = -ENOMEM;
			goto out_close;
		}
		info_buf->Attributes = cpu_to_le32(dosattr);
		rc = CIFSSMBSetFileInfo(xid, tcon, info_buf, fid.netfid,
					current->tgid);
		 
		if (!rc)
			cifsInode->cifsAttrs = dosattr;
		else
			dosattr = origattr;  
	}

	 
	rc = CIFSSMBRenameOpenFile(xid, tcon, fid.netfid, NULL,
				   cifs_sb->local_nls,
				   cifs_remap(cifs_sb));
	if (rc != 0) {
		rc = -EBUSY;
		goto undo_setattr;
	}

	 
	if (!test_bit(CIFS_INO_DELETE_PENDING, &cifsInode->flags)) {
		rc = CIFSSMBSetFileDisposition(xid, tcon, true, fid.netfid,
					       current->tgid);
		 
		if (rc == -ENOENT)
			rc = 0;
		else if (rc != 0) {
			rc = -EBUSY;
			goto undo_rename;
		}
		set_bit(CIFS_INO_DELETE_PENDING, &cifsInode->flags);
	}

out_close:
	CIFSSMBClose(xid, tcon, fid.netfid);
out:
	kfree(info_buf);
	cifs_put_tlink(tlink);
	return rc;

	 
undo_rename:
	CIFSSMBRenameOpenFile(xid, tcon, fid.netfid, dentry->d_name.name,
				cifs_sb->local_nls, cifs_remap(cifs_sb));
undo_setattr:
	if (dosattr != origattr) {
		info_buf->Attributes = cpu_to_le32(origattr);
		if (!CIFSSMBSetFileInfo(xid, tcon, info_buf, fid.netfid,
					current->tgid))
			cifsInode->cifsAttrs = origattr;
	}

	goto out_close;
}
#endif  

 
static void
cifs_drop_nlink(struct inode *inode)
{
	spin_lock(&inode->i_lock);
	if (inode->i_nlink > 0)
		drop_nlink(inode);
	spin_unlock(&inode->i_lock);
}

 
int cifs_unlink(struct inode *dir, struct dentry *dentry)
{
	int rc = 0;
	unsigned int xid;
	const char *full_path;
	void *page;
	struct inode *inode = d_inode(dentry);
	struct cifsInodeInfo *cifs_inode;
	struct super_block *sb = dir->i_sb;
	struct cifs_sb_info *cifs_sb = CIFS_SB(sb);
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	struct TCP_Server_Info *server;
	struct iattr *attrs = NULL;
	__u32 dosattr = 0, origattr = 0;

	cifs_dbg(FYI, "cifs_unlink, dir=0x%p, dentry=0x%p\n", dir, dentry);

	if (unlikely(cifs_forced_shutdown(cifs_sb)))
		return -EIO;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);
	server = tcon->ses->server;

	xid = get_xid();
	page = alloc_dentry_path();

	if (tcon->nodelete) {
		rc = -EACCES;
		goto unlink_out;
	}

	 
	full_path = build_path_from_dentry(dentry, page);
	if (IS_ERR(full_path)) {
		rc = PTR_ERR(full_path);
		goto unlink_out;
	}

	cifs_close_deferred_file_under_dentry(tcon, full_path);
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	if (cap_unix(tcon->ses) && (CIFS_UNIX_POSIX_PATH_OPS_CAP &
				le64_to_cpu(tcon->fsUnixInfo.Capability))) {
		rc = CIFSPOSIXDelFile(xid, tcon, full_path,
			SMB_POSIX_UNLINK_FILE_TARGET, cifs_sb->local_nls,
			cifs_remap(cifs_sb));
		cifs_dbg(FYI, "posix del rc %d\n", rc);
		if ((rc == 0) || (rc == -ENOENT))
			goto psx_del_no_retry;
	}
#endif  

retry_std_delete:
	if (!server->ops->unlink) {
		rc = -ENOSYS;
		goto psx_del_no_retry;
	}

	rc = server->ops->unlink(xid, tcon, full_path, cifs_sb);

psx_del_no_retry:
	if (!rc) {
		if (inode)
			cifs_drop_nlink(inode);
	} else if (rc == -ENOENT) {
		d_drop(dentry);
	} else if (rc == -EBUSY) {
		if (server->ops->rename_pending_delete) {
			rc = server->ops->rename_pending_delete(full_path,
								dentry, xid);
			if (rc == 0)
				cifs_drop_nlink(inode);
		}
	} else if ((rc == -EACCES) && (dosattr == 0) && inode) {
		attrs = kzalloc(sizeof(*attrs), GFP_KERNEL);
		if (attrs == NULL) {
			rc = -ENOMEM;
			goto out_reval;
		}

		 
		cifs_inode = CIFS_I(inode);
		origattr = cifs_inode->cifsAttrs;
		if (origattr == 0)
			origattr |= ATTR_NORMAL;
		dosattr = origattr & ~ATTR_READONLY;
		if (dosattr == 0)
			dosattr |= ATTR_NORMAL;
		dosattr |= ATTR_HIDDEN;

		rc = cifs_set_file_info(inode, attrs, xid, full_path, dosattr);
		if (rc != 0)
			goto out_reval;

		goto retry_std_delete;
	}

	 
	if (rc != 0 && dosattr != 0)
		cifs_set_file_info(inode, attrs, xid, full_path, origattr);

out_reval:
	if (inode) {
		cifs_inode = CIFS_I(inode);
		cifs_inode->time = 0;	 
		inode_set_ctime_current(inode);
	}
	inode_set_mtime_to_ts(dir, inode_set_ctime_current(dir));
	cifs_inode = CIFS_I(dir);
	CIFS_I(dir)->time = 0;	 
unlink_out:
	free_dentry_path(page);
	kfree(attrs);
	free_xid(xid);
	cifs_put_tlink(tlink);
	return rc;
}

static int
cifs_mkdir_qinfo(struct inode *parent, struct dentry *dentry, umode_t mode,
		 const char *full_path, struct cifs_sb_info *cifs_sb,
		 struct cifs_tcon *tcon, const unsigned int xid)
{
	int rc = 0;
	struct inode *inode = NULL;

	if (tcon->posix_extensions)
		rc = smb311_posix_get_inode_info(&inode, full_path, parent->i_sb, xid);
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	else if (tcon->unix_ext)
		rc = cifs_get_inode_info_unix(&inode, full_path, parent->i_sb,
					      xid);
#endif  
	else
		rc = cifs_get_inode_info(&inode, full_path, NULL, parent->i_sb,
					 xid, NULL);

	if (rc)
		return rc;

	if (!S_ISDIR(inode->i_mode)) {
		 
		 iput(inode);
		 d_drop(dentry);
		 return 0;
	}
	 
	if (inode->i_nlink < 2)
		set_nlink(inode, 2);
	mode &= ~current_umask();
	 
	if (parent->i_mode & S_ISGID)
		mode |= S_ISGID;

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	if (tcon->unix_ext) {
		struct cifs_unix_set_info_args args = {
			.mode	= mode,
			.ctime	= NO_CHANGE_64,
			.atime	= NO_CHANGE_64,
			.mtime	= NO_CHANGE_64,
			.device	= 0,
		};
		if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SET_UID) {
			args.uid = current_fsuid();
			if (parent->i_mode & S_ISGID)
				args.gid = parent->i_gid;
			else
				args.gid = current_fsgid();
		} else {
			args.uid = INVALID_UID;  
			args.gid = INVALID_GID;  
		}
		CIFSSMBUnixSetPathInfo(xid, tcon, full_path, &args,
				       cifs_sb->local_nls,
				       cifs_remap(cifs_sb));
	} else {
#else
	{
#endif  
		struct TCP_Server_Info *server = tcon->ses->server;
		if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_CIFS_ACL) &&
		    (mode & S_IWUGO) == 0 && server->ops->mkdir_setinfo)
			server->ops->mkdir_setinfo(inode, full_path, cifs_sb,
						   tcon, xid);
		if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_DYNPERM)
			inode->i_mode = (mode | S_IFDIR);

		if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SET_UID) {
			inode->i_uid = current_fsuid();
			if (inode->i_mode & S_ISGID)
				inode->i_gid = parent->i_gid;
			else
				inode->i_gid = current_fsgid();
		}
	}
	d_instantiate(dentry, inode);
	return 0;
}

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
static int
cifs_posix_mkdir(struct inode *inode, struct dentry *dentry, umode_t mode,
		 const char *full_path, struct cifs_sb_info *cifs_sb,
		 struct cifs_tcon *tcon, const unsigned int xid)
{
	int rc = 0;
	u32 oplock = 0;
	FILE_UNIX_BASIC_INFO *info = NULL;
	struct inode *newinode = NULL;
	struct cifs_fattr fattr;

	info = kzalloc(sizeof(FILE_UNIX_BASIC_INFO), GFP_KERNEL);
	if (info == NULL) {
		rc = -ENOMEM;
		goto posix_mkdir_out;
	}

	mode &= ~current_umask();
	rc = CIFSPOSIXCreate(xid, tcon, SMB_O_DIRECTORY | SMB_O_CREAT, mode,
			     NULL  , info, &oplock, full_path,
			     cifs_sb->local_nls, cifs_remap(cifs_sb));
	if (rc == -EOPNOTSUPP)
		goto posix_mkdir_out;
	else if (rc) {
		cifs_dbg(FYI, "posix mkdir returned 0x%x\n", rc);
		d_drop(dentry);
		goto posix_mkdir_out;
	}

	if (info->Type == cpu_to_le32(-1))
		 
		goto posix_mkdir_get_info;
	 

	cifs_unix_basic_to_fattr(&fattr, info, cifs_sb);
	cifs_fill_uniqueid(inode->i_sb, &fattr);
	newinode = cifs_iget(inode->i_sb, &fattr);
	if (!newinode)
		goto posix_mkdir_get_info;

	d_instantiate(dentry, newinode);

#ifdef CONFIG_CIFS_DEBUG2
	cifs_dbg(FYI, "instantiated dentry %p %pd to inode %p\n",
		 dentry, dentry, newinode);

	if (newinode->i_nlink != 2)
		cifs_dbg(FYI, "unexpected number of links %d\n",
			 newinode->i_nlink);
#endif

posix_mkdir_out:
	kfree(info);
	return rc;
posix_mkdir_get_info:
	rc = cifs_mkdir_qinfo(inode, dentry, mode, full_path, cifs_sb, tcon,
			      xid);
	goto posix_mkdir_out;
}
#endif  

int cifs_mkdir(struct mnt_idmap *idmap, struct inode *inode,
	       struct dentry *direntry, umode_t mode)
{
	int rc = 0;
	unsigned int xid;
	struct cifs_sb_info *cifs_sb;
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	struct TCP_Server_Info *server;
	const char *full_path;
	void *page;

	cifs_dbg(FYI, "In cifs_mkdir, mode = %04ho inode = 0x%p\n",
		 mode, inode);

	cifs_sb = CIFS_SB(inode->i_sb);
	if (unlikely(cifs_forced_shutdown(cifs_sb)))
		return -EIO;
	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);

	xid = get_xid();

	page = alloc_dentry_path();
	full_path = build_path_from_dentry(direntry, page);
	if (IS_ERR(full_path)) {
		rc = PTR_ERR(full_path);
		goto mkdir_out;
	}

	server = tcon->ses->server;

	if ((server->ops->posix_mkdir) && (tcon->posix_extensions)) {
		rc = server->ops->posix_mkdir(xid, inode, mode, tcon, full_path,
					      cifs_sb);
		d_drop(direntry);  
		goto mkdir_out;
	}

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	if (cap_unix(tcon->ses) && (CIFS_UNIX_POSIX_PATH_OPS_CAP &
				le64_to_cpu(tcon->fsUnixInfo.Capability))) {
		rc = cifs_posix_mkdir(inode, direntry, mode, full_path, cifs_sb,
				      tcon, xid);
		if (rc != -EOPNOTSUPP)
			goto mkdir_out;
	}
#endif  

	if (!server->ops->mkdir) {
		rc = -ENOSYS;
		goto mkdir_out;
	}

	 
	rc = server->ops->mkdir(xid, inode, mode, tcon, full_path, cifs_sb);
	if (rc) {
		cifs_dbg(FYI, "cifs_mkdir returned 0x%x\n", rc);
		d_drop(direntry);
		goto mkdir_out;
	}

	 
	rc = cifs_mkdir_qinfo(inode, direntry, mode, full_path, cifs_sb, tcon,
			      xid);
mkdir_out:
	 
	CIFS_I(inode)->time = 0;
	free_dentry_path(page);
	free_xid(xid);
	cifs_put_tlink(tlink);
	return rc;
}

int cifs_rmdir(struct inode *inode, struct dentry *direntry)
{
	int rc = 0;
	unsigned int xid;
	struct cifs_sb_info *cifs_sb;
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	struct TCP_Server_Info *server;
	const char *full_path;
	void *page = alloc_dentry_path();
	struct cifsInodeInfo *cifsInode;

	cifs_dbg(FYI, "cifs_rmdir, inode = 0x%p\n", inode);

	xid = get_xid();

	full_path = build_path_from_dentry(direntry, page);
	if (IS_ERR(full_path)) {
		rc = PTR_ERR(full_path);
		goto rmdir_exit;
	}

	cifs_sb = CIFS_SB(inode->i_sb);
	if (unlikely(cifs_forced_shutdown(cifs_sb))) {
		rc = -EIO;
		goto rmdir_exit;
	}

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink)) {
		rc = PTR_ERR(tlink);
		goto rmdir_exit;
	}
	tcon = tlink_tcon(tlink);
	server = tcon->ses->server;

	if (!server->ops->rmdir) {
		rc = -ENOSYS;
		cifs_put_tlink(tlink);
		goto rmdir_exit;
	}

	if (tcon->nodelete) {
		rc = -EACCES;
		cifs_put_tlink(tlink);
		goto rmdir_exit;
	}

	rc = server->ops->rmdir(xid, tcon, full_path, cifs_sb);
	cifs_put_tlink(tlink);

	if (!rc) {
		spin_lock(&d_inode(direntry)->i_lock);
		i_size_write(d_inode(direntry), 0);
		clear_nlink(d_inode(direntry));
		spin_unlock(&d_inode(direntry)->i_lock);
	}

	cifsInode = CIFS_I(d_inode(direntry));
	 
	cifsInode->time = 0;

	cifsInode = CIFS_I(inode);
	 
	cifsInode->time = 0;

	inode_set_ctime_current(d_inode(direntry));
	inode_set_mtime_to_ts(inode, inode_set_ctime_current(inode));

rmdir_exit:
	free_dentry_path(page);
	free_xid(xid);
	return rc;
}

static int
cifs_do_rename(const unsigned int xid, struct dentry *from_dentry,
	       const char *from_path, struct dentry *to_dentry,
	       const char *to_path)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(from_dentry->d_sb);
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	struct TCP_Server_Info *server;
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	struct cifs_fid fid;
	struct cifs_open_parms oparms;
	int oplock;
#endif  
	int rc;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);
	server = tcon->ses->server;

	if (!server->ops->rename)
		return -ENOSYS;

	 
	rc = server->ops->rename(xid, tcon, from_path, to_path, cifs_sb);

	 
	if (rc == 0 || rc != -EBUSY)
		goto do_rename_exit;

	 
	if (server->vals->protocol_id != 0)
		goto do_rename_exit;

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	 
	if (to_dentry->d_parent != from_dentry->d_parent)
		goto do_rename_exit;

	oparms = (struct cifs_open_parms) {
		.tcon = tcon,
		.cifs_sb = cifs_sb,
		 
		.desired_access = DELETE,
		.create_options = cifs_create_options(cifs_sb, CREATE_NOT_DIR),
		.disposition = FILE_OPEN,
		.path = from_path,
		.fid = &fid,
	};

	rc = CIFS_open(xid, &oparms, &oplock, NULL);
	if (rc == 0) {
		rc = CIFSSMBRenameOpenFile(xid, tcon, fid.netfid,
				(const char *) to_dentry->d_name.name,
				cifs_sb->local_nls, cifs_remap(cifs_sb));
		CIFSSMBClose(xid, tcon, fid.netfid);
	}
#endif  
do_rename_exit:
	if (rc == 0)
		d_move(from_dentry, to_dentry);
	cifs_put_tlink(tlink);
	return rc;
}

int
cifs_rename2(struct mnt_idmap *idmap, struct inode *source_dir,
	     struct dentry *source_dentry, struct inode *target_dir,
	     struct dentry *target_dentry, unsigned int flags)
{
	const char *from_name, *to_name;
	void *page1, *page2;
	struct cifs_sb_info *cifs_sb;
	struct tcon_link *tlink;
	struct cifs_tcon *tcon;
	unsigned int xid;
	int rc, tmprc;
	int retry_count = 0;
	FILE_UNIX_BASIC_INFO *info_buf_source = NULL;
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	FILE_UNIX_BASIC_INFO *info_buf_target;
#endif  

	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;

	cifs_sb = CIFS_SB(source_dir->i_sb);
	if (unlikely(cifs_forced_shutdown(cifs_sb)))
		return -EIO;

	tlink = cifs_sb_tlink(cifs_sb);
	if (IS_ERR(tlink))
		return PTR_ERR(tlink);
	tcon = tlink_tcon(tlink);

	page1 = alloc_dentry_path();
	page2 = alloc_dentry_path();
	xid = get_xid();

	from_name = build_path_from_dentry(source_dentry, page1);
	if (IS_ERR(from_name)) {
		rc = PTR_ERR(from_name);
		goto cifs_rename_exit;
	}

	to_name = build_path_from_dentry(target_dentry, page2);
	if (IS_ERR(to_name)) {
		rc = PTR_ERR(to_name);
		goto cifs_rename_exit;
	}

	cifs_close_deferred_file_under_dentry(tcon, from_name);
	if (d_inode(target_dentry) != NULL)
		cifs_close_deferred_file_under_dentry(tcon, to_name);

	rc = cifs_do_rename(xid, source_dentry, from_name, target_dentry,
			    to_name);

	if (rc == -EACCES) {
		while (retry_count < 3) {
			cifs_close_all_deferred_files(tcon);
			rc = cifs_do_rename(xid, source_dentry, from_name, target_dentry,
					    to_name);
			if (rc != -EACCES)
				break;
			retry_count++;
		}
	}

	 
	if (flags & RENAME_NOREPLACE)
		goto cifs_rename_exit;

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	if (rc == -EEXIST && tcon->unix_ext) {
		 
		info_buf_source =
			kmalloc_array(2, sizeof(FILE_UNIX_BASIC_INFO),
					GFP_KERNEL);
		if (info_buf_source == NULL) {
			rc = -ENOMEM;
			goto cifs_rename_exit;
		}

		info_buf_target = info_buf_source + 1;
		tmprc = CIFSSMBUnixQPathInfo(xid, tcon, from_name,
					     info_buf_source,
					     cifs_sb->local_nls,
					     cifs_remap(cifs_sb));
		if (tmprc != 0)
			goto unlink_target;

		tmprc = CIFSSMBUnixQPathInfo(xid, tcon, to_name,
					     info_buf_target,
					     cifs_sb->local_nls,
					     cifs_remap(cifs_sb));

		if (tmprc == 0 && (info_buf_source->UniqueId ==
				   info_buf_target->UniqueId)) {
			 
			rc = 0;
			goto cifs_rename_exit;
		}
	}
	 

unlink_target:
#endif  

	 
	if (d_really_is_positive(target_dentry) && (rc == -EACCES || rc == -EEXIST)) {
		if (d_is_dir(target_dentry))
			tmprc = cifs_rmdir(target_dir, target_dentry);
		else
			tmprc = cifs_unlink(target_dir, target_dentry);
		if (tmprc)
			goto cifs_rename_exit;
		rc = cifs_do_rename(xid, source_dentry, from_name,
				    target_dentry, to_name);
	}

	 
	CIFS_I(source_dir)->time = CIFS_I(target_dir)->time = 0;

cifs_rename_exit:
	kfree(info_buf_source);
	free_dentry_path(page2);
	free_dentry_path(page1);
	free_xid(xid);
	cifs_put_tlink(tlink);
	return rc;
}

static bool
cifs_dentry_needs_reval(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct cifsInodeInfo *cifs_i = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct cifs_tcon *tcon = cifs_sb_master_tcon(cifs_sb);
	struct cached_fid *cfid = NULL;

	if (cifs_i->time == 0)
		return true;

	if (CIFS_CACHE_READ(cifs_i))
		return false;

	if (!lookupCacheEnabled)
		return true;

	if (!open_cached_dir_by_dentry(tcon, dentry->d_parent, &cfid)) {
		spin_lock(&cfid->fid_lock);
		if (cfid->time && cifs_i->time > cfid->time) {
			spin_unlock(&cfid->fid_lock);
			close_cached_dir(cfid);
			return false;
		}
		spin_unlock(&cfid->fid_lock);
		close_cached_dir(cfid);
	}
	 
	if (S_ISDIR(inode->i_mode)) {
		if (!cifs_sb->ctx->acdirmax)
			return true;
		if (!time_in_range(jiffies, cifs_i->time,
				   cifs_i->time + cifs_sb->ctx->acdirmax))
			return true;
	} else {  
		if (!cifs_sb->ctx->acregmax)
			return true;
		if (!time_in_range(jiffies, cifs_i->time,
				   cifs_i->time + cifs_sb->ctx->acregmax))
			return true;
	}

	 
	if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SERVER_INUM) &&
	    S_ISREG(inode->i_mode) && inode->i_nlink != 1)
		return true;

	return false;
}

 
int
cifs_invalidate_mapping(struct inode *inode)
{
	int rc = 0;

	if (inode->i_mapping && inode->i_mapping->nrpages != 0) {
		rc = invalidate_inode_pages2(inode->i_mapping);
		if (rc)
			cifs_dbg(VFS, "%s: invalidate inode %p failed with rc %d\n",
				 __func__, inode, rc);
	}

	return rc;
}

 
static int
cifs_wait_bit_killable(struct wait_bit_key *key, int mode)
{
	schedule();
	if (signal_pending_state(mode, current))
		return -ERESTARTSYS;
	return 0;
}

int
cifs_revalidate_mapping(struct inode *inode)
{
	int rc;
	unsigned long *flags = &CIFS_I(inode)->flags;
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);

	 
	if (IS_SWAPFILE(inode))
		return 0;

	rc = wait_on_bit_lock_action(flags, CIFS_INO_LOCK, cifs_wait_bit_killable,
				     TASK_KILLABLE|TASK_FREEZABLE_UNSAFE);
	if (rc)
		return rc;

	if (test_and_clear_bit(CIFS_INO_INVALID_MAPPING, flags)) {
		 
		if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_RW_CACHE)
			goto skip_invalidate;

		rc = cifs_invalidate_mapping(inode);
		if (rc)
			set_bit(CIFS_INO_INVALID_MAPPING, flags);
	}

skip_invalidate:
	clear_bit_unlock(CIFS_INO_LOCK, flags);
	smp_mb__after_atomic();
	wake_up_bit(flags, CIFS_INO_LOCK);

	return rc;
}

int
cifs_zap_mapping(struct inode *inode)
{
	set_bit(CIFS_INO_INVALID_MAPPING, &CIFS_I(inode)->flags);
	return cifs_revalidate_mapping(inode);
}

int cifs_revalidate_file_attr(struct file *filp)
{
	int rc = 0;
	struct dentry *dentry = file_dentry(filp);
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	struct cifsFileInfo *cfile = (struct cifsFileInfo *) filp->private_data;
#endif  

	if (!cifs_dentry_needs_reval(dentry))
		return rc;

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	if (tlink_tcon(cfile->tlink)->unix_ext)
		rc = cifs_get_file_info_unix(filp);
	else
#endif  
		rc = cifs_get_file_info(filp);

	return rc;
}

int cifs_revalidate_dentry_attr(struct dentry *dentry)
{
	unsigned int xid;
	int rc = 0;
	struct inode *inode = d_inode(dentry);
	struct super_block *sb = dentry->d_sb;
	const char *full_path;
	void *page;
	int count = 0;

	if (inode == NULL)
		return -ENOENT;

	if (!cifs_dentry_needs_reval(dentry))
		return rc;

	xid = get_xid();

	page = alloc_dentry_path();
	full_path = build_path_from_dentry(dentry, page);
	if (IS_ERR(full_path)) {
		rc = PTR_ERR(full_path);
		goto out;
	}

	cifs_dbg(FYI, "Update attributes: %s inode 0x%p count %d dentry: 0x%p d_time %ld jiffies %ld\n",
		 full_path, inode, inode->i_count.counter,
		 dentry, cifs_get_time(dentry), jiffies);

again:
	if (cifs_sb_master_tcon(CIFS_SB(sb))->posix_extensions)
		rc = smb311_posix_get_inode_info(&inode, full_path, sb, xid);
	else if (cifs_sb_master_tcon(CIFS_SB(sb))->unix_ext)
		rc = cifs_get_inode_info_unix(&inode, full_path, sb, xid);
	else
		rc = cifs_get_inode_info(&inode, full_path, NULL, sb,
					 xid, NULL);
	if (rc == -EAGAIN && count++ < 10)
		goto again;
out:
	free_dentry_path(page);
	free_xid(xid);

	return rc;
}

int cifs_revalidate_file(struct file *filp)
{
	int rc;
	struct inode *inode = file_inode(filp);

	rc = cifs_revalidate_file_attr(filp);
	if (rc)
		return rc;

	return cifs_revalidate_mapping(inode);
}

 
int cifs_revalidate_dentry(struct dentry *dentry)
{
	int rc;
	struct inode *inode = d_inode(dentry);

	rc = cifs_revalidate_dentry_attr(dentry);
	if (rc)
		return rc;

	return cifs_revalidate_mapping(inode);
}

int cifs_getattr(struct mnt_idmap *idmap, const struct path *path,
		 struct kstat *stat, u32 request_mask, unsigned int flags)
{
	struct dentry *dentry = path->dentry;
	struct cifs_sb_info *cifs_sb = CIFS_SB(dentry->d_sb);
	struct cifs_tcon *tcon = cifs_sb_master_tcon(cifs_sb);
	struct inode *inode = d_inode(dentry);
	int rc;

	if (unlikely(cifs_forced_shutdown(CIFS_SB(inode->i_sb))))
		return -EIO;

	 
	if ((request_mask & (STATX_CTIME | STATX_MTIME | STATX_SIZE | STATX_BLOCKS)) &&
	    !CIFS_CACHE_READ(CIFS_I(inode)) &&
	    inode->i_mapping && inode->i_mapping->nrpages != 0) {
		rc = filemap_fdatawait(inode->i_mapping);
		if (rc) {
			mapping_set_error(inode->i_mapping, rc);
			return rc;
		}
	}

	if ((flags & AT_STATX_SYNC_TYPE) == AT_STATX_FORCE_SYNC)
		CIFS_I(inode)->time = 0;  

	 
	if (((flags & AT_STATX_SYNC_TYPE) != AT_STATX_DONT_SYNC) ||
	    (CIFS_I(inode)->time == 0)) {
		rc = cifs_revalidate_dentry_attr(dentry);
		if (rc)
			return rc;
	}

	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	stat->blksize = cifs_sb->ctx->bsize;
	stat->ino = CIFS_I(inode)->uniqueid;

	 
	if (CIFS_I(inode)->createtime) {
		stat->result_mask |= STATX_BTIME;
		stat->btime =
		      cifs_NTtimeToUnix(cpu_to_le64(CIFS_I(inode)->createtime));
	}

	stat->attributes_mask |= (STATX_ATTR_COMPRESSED | STATX_ATTR_ENCRYPTED);
	if (CIFS_I(inode)->cifsAttrs & FILE_ATTRIBUTE_COMPRESSED)
		stat->attributes |= STATX_ATTR_COMPRESSED;
	if (CIFS_I(inode)->cifsAttrs & FILE_ATTRIBUTE_ENCRYPTED)
		stat->attributes |= STATX_ATTR_ENCRYPTED;

	 
	if ((cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MULTIUSER) &&
	    !(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_CIFS_ACL) &&
	    !tcon->unix_ext) {
		if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_OVERR_UID))
			stat->uid = current_fsuid();
		if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_OVERR_GID))
			stat->gid = current_fsgid();
	}
	return 0;
}

int cifs_fiemap(struct inode *inode, struct fiemap_extent_info *fei, u64 start,
		u64 len)
{
	struct cifsInodeInfo *cifs_i = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(cifs_i->netfs.inode.i_sb);
	struct cifs_tcon *tcon = cifs_sb_master_tcon(cifs_sb);
	struct TCP_Server_Info *server = tcon->ses->server;
	struct cifsFileInfo *cfile;
	int rc;

	if (unlikely(cifs_forced_shutdown(cifs_sb)))
		return -EIO;

	 
	if (!CIFS_CACHE_READ(CIFS_I(inode)) && inode->i_mapping &&
	    inode->i_mapping->nrpages != 0) {
		rc = filemap_fdatawait(inode->i_mapping);
		if (rc) {
			mapping_set_error(inode->i_mapping, rc);
			return rc;
		}
	}

	cfile = find_readable_file(cifs_i, false);
	if (cfile == NULL)
		return -EINVAL;

	if (server->ops->fiemap) {
		rc = server->ops->fiemap(tcon, cfile, fei, start, len);
		cifsFileInfo_put(cfile);
		return rc;
	}

	cifsFileInfo_put(cfile);
	return -EOPNOTSUPP;
}

int cifs_truncate_page(struct address_space *mapping, loff_t from)
{
	pgoff_t index = from >> PAGE_SHIFT;
	unsigned offset = from & (PAGE_SIZE - 1);
	struct page *page;
	int rc = 0;

	page = grab_cache_page(mapping, index);
	if (!page)
		return -ENOMEM;

	zero_user_segment(page, offset, PAGE_SIZE);
	unlock_page(page);
	put_page(page);
	return rc;
}

void cifs_setsize(struct inode *inode, loff_t offset)
{
	struct cifsInodeInfo *cifs_i = CIFS_I(inode);

	spin_lock(&inode->i_lock);
	i_size_write(inode, offset);
	spin_unlock(&inode->i_lock);

	 
	cifs_i->time = 0;
	truncate_pagecache(inode, offset);
}

static int
cifs_set_file_size(struct inode *inode, struct iattr *attrs,
		   unsigned int xid, const char *full_path)
{
	int rc;
	struct cifsFileInfo *open_file;
	struct cifsInodeInfo *cifsInode = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct tcon_link *tlink = NULL;
	struct cifs_tcon *tcon = NULL;
	struct TCP_Server_Info *server;

	 
	open_file = find_writable_file(cifsInode, FIND_WR_FSUID_ONLY);
	if (open_file) {
		tcon = tlink_tcon(open_file->tlink);
		server = tcon->ses->server;
		if (server->ops->set_file_size)
			rc = server->ops->set_file_size(xid, tcon, open_file,
							attrs->ia_size, false);
		else
			rc = -ENOSYS;
		cifsFileInfo_put(open_file);
		cifs_dbg(FYI, "SetFSize for attrs rc = %d\n", rc);
	} else
		rc = -EINVAL;

	if (!rc)
		goto set_size_out;

	if (tcon == NULL) {
		tlink = cifs_sb_tlink(cifs_sb);
		if (IS_ERR(tlink))
			return PTR_ERR(tlink);
		tcon = tlink_tcon(tlink);
		server = tcon->ses->server;
	}

	 
	if (server->ops->set_path_size)
		rc = server->ops->set_path_size(xid, tcon, full_path,
						attrs->ia_size, cifs_sb, false);
	else
		rc = -ENOSYS;
	cifs_dbg(FYI, "SetEOF by path (setattrs) rc = %d\n", rc);

	if (tlink)
		cifs_put_tlink(tlink);

set_size_out:
	if (rc == 0) {
		cifsInode->server_eof = attrs->ia_size;
		cifs_setsize(inode, attrs->ia_size);
		 
		inode->i_blocks = (512 - 1 + attrs->ia_size) >> 9;

		 
		attrs->ia_ctime = attrs->ia_mtime = current_time(inode);
		attrs->ia_valid |= ATTR_CTIME | ATTR_MTIME;

		cifs_truncate_page(inode->i_mapping, inode->i_size);
	}

	return rc;
}

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
static int
cifs_setattr_unix(struct dentry *direntry, struct iattr *attrs)
{
	int rc;
	unsigned int xid;
	const char *full_path;
	void *page = alloc_dentry_path();
	struct inode *inode = d_inode(direntry);
	struct cifsInodeInfo *cifsInode = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct tcon_link *tlink;
	struct cifs_tcon *pTcon;
	struct cifs_unix_set_info_args *args = NULL;
	struct cifsFileInfo *open_file;

	cifs_dbg(FYI, "setattr_unix on file %pd attrs->ia_valid=0x%x\n",
		 direntry, attrs->ia_valid);

	xid = get_xid();

	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_NO_PERM)
		attrs->ia_valid |= ATTR_FORCE;

	rc = setattr_prepare(&nop_mnt_idmap, direntry, attrs);
	if (rc < 0)
		goto out;

	full_path = build_path_from_dentry(direntry, page);
	if (IS_ERR(full_path)) {
		rc = PTR_ERR(full_path);
		goto out;
	}

	 
	rc = filemap_write_and_wait(inode->i_mapping);
	if (is_interrupt_error(rc)) {
		rc = -ERESTARTSYS;
		goto out;
	}

	mapping_set_error(inode->i_mapping, rc);
	rc = 0;

	if (attrs->ia_valid & ATTR_SIZE) {
		rc = cifs_set_file_size(inode, attrs, xid, full_path);
		if (rc != 0)
			goto out;
	}

	 
	if (attrs->ia_valid & (ATTR_KILL_SUID|ATTR_KILL_SGID))
		attrs->ia_valid &= ~ATTR_MODE;

	args = kmalloc(sizeof(*args), GFP_KERNEL);
	if (args == NULL) {
		rc = -ENOMEM;
		goto out;
	}

	 
	if (attrs->ia_valid & ATTR_MODE)
		args->mode = attrs->ia_mode;
	else
		args->mode = NO_CHANGE_64;

	if (attrs->ia_valid & ATTR_UID)
		args->uid = attrs->ia_uid;
	else
		args->uid = INVALID_UID;  

	if (attrs->ia_valid & ATTR_GID)
		args->gid = attrs->ia_gid;
	else
		args->gid = INVALID_GID;  

	if (attrs->ia_valid & ATTR_ATIME)
		args->atime = cifs_UnixTimeToNT(attrs->ia_atime);
	else
		args->atime = NO_CHANGE_64;

	if (attrs->ia_valid & ATTR_MTIME)
		args->mtime = cifs_UnixTimeToNT(attrs->ia_mtime);
	else
		args->mtime = NO_CHANGE_64;

	if (attrs->ia_valid & ATTR_CTIME)
		args->ctime = cifs_UnixTimeToNT(attrs->ia_ctime);
	else
		args->ctime = NO_CHANGE_64;

	args->device = 0;
	open_file = find_writable_file(cifsInode, FIND_WR_FSUID_ONLY);
	if (open_file) {
		u16 nfid = open_file->fid.netfid;
		u32 npid = open_file->pid;
		pTcon = tlink_tcon(open_file->tlink);
		rc = CIFSSMBUnixSetFileInfo(xid, pTcon, args, nfid, npid);
		cifsFileInfo_put(open_file);
	} else {
		tlink = cifs_sb_tlink(cifs_sb);
		if (IS_ERR(tlink)) {
			rc = PTR_ERR(tlink);
			goto out;
		}
		pTcon = tlink_tcon(tlink);
		rc = CIFSSMBUnixSetPathInfo(xid, pTcon, full_path, args,
				    cifs_sb->local_nls,
				    cifs_remap(cifs_sb));
		cifs_put_tlink(tlink);
	}

	if (rc)
		goto out;

	if ((attrs->ia_valid & ATTR_SIZE) &&
	    attrs->ia_size != i_size_read(inode)) {
		truncate_setsize(inode, attrs->ia_size);
		fscache_resize_cookie(cifs_inode_cookie(inode), attrs->ia_size);
	}

	setattr_copy(&nop_mnt_idmap, inode, attrs);
	mark_inode_dirty(inode);

	 
	if (attrs->ia_valid & (ATTR_MTIME | ATTR_CTIME))
		cifsInode->time = 0;
out:
	kfree(args);
	free_dentry_path(page);
	free_xid(xid);
	return rc;
}
#endif  

static int
cifs_setattr_nounix(struct dentry *direntry, struct iattr *attrs)
{
	unsigned int xid;
	kuid_t uid = INVALID_UID;
	kgid_t gid = INVALID_GID;
	struct inode *inode = d_inode(direntry);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct cifsInodeInfo *cifsInode = CIFS_I(inode);
	struct cifsFileInfo *wfile;
	struct cifs_tcon *tcon;
	const char *full_path;
	void *page = alloc_dentry_path();
	int rc = -EACCES;
	__u32 dosattr = 0;
	__u64 mode = NO_CHANGE_64;

	xid = get_xid();

	cifs_dbg(FYI, "setattr on file %pd attrs->ia_valid 0x%x\n",
		 direntry, attrs->ia_valid);

	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_NO_PERM)
		attrs->ia_valid |= ATTR_FORCE;

	rc = setattr_prepare(&nop_mnt_idmap, direntry, attrs);
	if (rc < 0)
		goto cifs_setattr_exit;

	full_path = build_path_from_dentry(direntry, page);
	if (IS_ERR(full_path)) {
		rc = PTR_ERR(full_path);
		goto cifs_setattr_exit;
	}

	 
	if (attrs->ia_valid & (ATTR_MTIME | ATTR_SIZE | ATTR_CTIME)) {
		rc = filemap_write_and_wait(inode->i_mapping);
		if (is_interrupt_error(rc)) {
			rc = -ERESTARTSYS;
			goto cifs_setattr_exit;
		}
		mapping_set_error(inode->i_mapping, rc);
	}

	rc = 0;

	if ((attrs->ia_valid & ATTR_MTIME) &&
	    !(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_NOSSYNC)) {
		rc = cifs_get_writable_file(cifsInode, FIND_WR_ANY, &wfile);
		if (!rc) {
			tcon = tlink_tcon(wfile->tlink);
			rc = tcon->ses->server->ops->flush(xid, tcon, &wfile->fid);
			cifsFileInfo_put(wfile);
			if (rc)
				goto cifs_setattr_exit;
		} else if (rc != -EBADF)
			goto cifs_setattr_exit;
		else
			rc = 0;
	}

	if (attrs->ia_valid & ATTR_SIZE) {
		rc = cifs_set_file_size(inode, attrs, xid, full_path);
		if (rc != 0)
			goto cifs_setattr_exit;
	}

	if (attrs->ia_valid & ATTR_UID)
		uid = attrs->ia_uid;

	if (attrs->ia_valid & ATTR_GID)
		gid = attrs->ia_gid;

	if ((cifs_sb->mnt_cifs_flags & CIFS_MOUNT_CIFS_ACL) ||
	    (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MODE_FROM_SID)) {
		if (uid_valid(uid) || gid_valid(gid)) {
			mode = NO_CHANGE_64;
			rc = id_mode_to_cifs_acl(inode, full_path, &mode,
							uid, gid);
			if (rc) {
				cifs_dbg(FYI, "%s: Setting id failed with error: %d\n",
					 __func__, rc);
				goto cifs_setattr_exit;
			}
		}
	} else
	if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SET_UID))
		attrs->ia_valid &= ~(ATTR_UID | ATTR_GID);

	 
	if (attrs->ia_valid & (ATTR_KILL_SUID|ATTR_KILL_SGID))
		attrs->ia_valid &= ~ATTR_MODE;

	if (attrs->ia_valid & ATTR_MODE) {
		mode = attrs->ia_mode;
		rc = 0;
		if ((cifs_sb->mnt_cifs_flags & CIFS_MOUNT_CIFS_ACL) ||
		    (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MODE_FROM_SID)) {
			rc = id_mode_to_cifs_acl(inode, full_path, &mode,
						INVALID_UID, INVALID_GID);
			if (rc) {
				cifs_dbg(FYI, "%s: Setting ACL failed with error: %d\n",
					 __func__, rc);
				goto cifs_setattr_exit;
			}

			 
			if (mode != attrs->ia_mode)
				attrs->ia_mode = mode;
		} else
		if (((mode & S_IWUGO) == 0) &&
		    (cifsInode->cifsAttrs & ATTR_READONLY) == 0) {

			dosattr = cifsInode->cifsAttrs | ATTR_READONLY;

			 
			if ((cifs_sb->mnt_cifs_flags & CIFS_MOUNT_DYNPERM) == 0)
				attrs->ia_mode = inode->i_mode & ~S_IWUGO;
		} else if ((mode & S_IWUGO) &&
			   (cifsInode->cifsAttrs & ATTR_READONLY)) {

			dosattr = cifsInode->cifsAttrs & ~ATTR_READONLY;
			 
			if (dosattr == 0)
				dosattr |= ATTR_NORMAL;

			 
			if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_DYNPERM)) {
				attrs->ia_mode &= ~(S_IALLUGO);
				if (S_ISDIR(inode->i_mode))
					attrs->ia_mode |=
						cifs_sb->ctx->dir_mode;
				else
					attrs->ia_mode |=
						cifs_sb->ctx->file_mode;
			}
		} else if (!(cifs_sb->mnt_cifs_flags & CIFS_MOUNT_DYNPERM)) {
			 
			attrs->ia_valid &= ~ATTR_MODE;
		}
	}

	if (attrs->ia_valid & (ATTR_MTIME|ATTR_ATIME|ATTR_CTIME) ||
	    ((attrs->ia_valid & ATTR_MODE) && dosattr)) {
		rc = cifs_set_file_info(inode, attrs, xid, full_path, dosattr);
		 

		 
		if ((rc) && (attrs->ia_valid &
				(ATTR_MODE | ATTR_GID | ATTR_UID | ATTR_SIZE)))
			rc = 0;
	}

	 
	if (rc)
		goto cifs_setattr_exit;

	if ((attrs->ia_valid & ATTR_SIZE) &&
	    attrs->ia_size != i_size_read(inode)) {
		truncate_setsize(inode, attrs->ia_size);
		fscache_resize_cookie(cifs_inode_cookie(inode), attrs->ia_size);
	}

	setattr_copy(&nop_mnt_idmap, inode, attrs);
	mark_inode_dirty(inode);

cifs_setattr_exit:
	free_xid(xid);
	free_dentry_path(page);
	return rc;
}

int
cifs_setattr(struct mnt_idmap *idmap, struct dentry *direntry,
	     struct iattr *attrs)
{
	struct cifs_sb_info *cifs_sb = CIFS_SB(direntry->d_sb);
	int rc, retries = 0;
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
	struct cifs_tcon *pTcon = cifs_sb_master_tcon(cifs_sb);
#endif  

	if (unlikely(cifs_forced_shutdown(cifs_sb)))
		return -EIO;

	do {
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
		if (pTcon->unix_ext)
			rc = cifs_setattr_unix(direntry, attrs);
		else
#endif  
			rc = cifs_setattr_nounix(direntry, attrs);
		retries++;
	} while (is_retryable_error(rc) && retries < 2);

	 
	return rc;
}
