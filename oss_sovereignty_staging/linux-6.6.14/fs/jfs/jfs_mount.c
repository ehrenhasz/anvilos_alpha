
 

 

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/log2.h>

#include "jfs_incore.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "jfs_metapage.h"
#include "jfs_debug.h"


 
static int chkSuper(struct super_block *);
static int logMOUNT(struct super_block *sb);

 
int jfs_mount(struct super_block *sb)
{
	int rc = 0;		 
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct inode *ipaimap = NULL;
	struct inode *ipaimap2 = NULL;
	struct inode *ipimap = NULL;
	struct inode *ipbmap = NULL;

	 
	if ((rc = chkSuper(sb))) {
		goto out;
	}

	ipaimap = diReadSpecial(sb, AGGREGATE_I, 0);
	if (ipaimap == NULL) {
		jfs_err("jfs_mount: Failed to read AGGREGATE_I");
		rc = -EIO;
		goto out;
	}
	sbi->ipaimap = ipaimap;

	jfs_info("jfs_mount: ipaimap:0x%p", ipaimap);

	 
	if ((rc = diMount(ipaimap))) {
		jfs_err("jfs_mount: diMount(ipaimap) failed w/rc = %d", rc);
		goto err_ipaimap;
	}

	 
	ipbmap = diReadSpecial(sb, BMAP_I, 0);
	if (ipbmap == NULL) {
		rc = -EIO;
		goto err_umount_ipaimap;
	}

	jfs_info("jfs_mount: ipbmap:0x%p", ipbmap);

	sbi->ipbmap = ipbmap;

	 
	if ((rc = dbMount(ipbmap))) {
		jfs_err("jfs_mount: dbMount failed w/rc = %d", rc);
		goto err_ipbmap;
	}

	 
	if ((sbi->mntflag & JFS_BAD_SAIT) == 0) {
		ipaimap2 = diReadSpecial(sb, AGGREGATE_I, 1);
		if (!ipaimap2) {
			jfs_err("jfs_mount: Failed to read AGGREGATE_I");
			rc = -EIO;
			goto err_umount_ipbmap;
		}
		sbi->ipaimap2 = ipaimap2;

		jfs_info("jfs_mount: ipaimap2:0x%p", ipaimap2);

		 
		if ((rc = diMount(ipaimap2))) {
			jfs_err("jfs_mount: diMount(ipaimap2) failed, rc = %d",
				rc);
			goto err_ipaimap2;
		}
	} else
		 
		sbi->ipaimap2 = NULL;

	 
	 
	ipimap = diReadSpecial(sb, FILESYSTEM_I, 0);
	if (ipimap == NULL) {
		jfs_err("jfs_mount: Failed to read FILESYSTEM_I");
		 
		rc = -EIO;
		goto err_umount_ipaimap2;
	}
	jfs_info("jfs_mount: ipimap:0x%p", ipimap);

	 
	sbi->ipimap = ipimap;

	 
	if ((rc = diMount(ipimap))) {
		jfs_err("jfs_mount: diMount failed w/rc = %d", rc);
		goto err_ipimap;
	}

	return rc;

	 
err_ipimap:
	 
	diFreeSpecial(ipimap);
err_umount_ipaimap2:
	 
	if (ipaimap2)
		diUnmount(ipaimap2, 1);
err_ipaimap2:
	 
	if (ipaimap2)
		diFreeSpecial(ipaimap2);
err_umount_ipbmap:	 
	dbUnmount(ipbmap, 1);
err_ipbmap:		 
	diFreeSpecial(ipbmap);
err_umount_ipaimap:	 
	diUnmount(ipaimap, 1);
err_ipaimap:		 
	diFreeSpecial(ipaimap);
out:
	if (rc)
		jfs_err("Mount JFS Failure: %d", rc);

	return rc;
}

 
int jfs_mount_rw(struct super_block *sb, int remount)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	int rc;

	 
	if (remount) {
		if (chkSuper(sb) || (sbi->state != FM_CLEAN))
			return -EINVAL;

		truncate_inode_pages(sbi->ipimap->i_mapping, 0);
		truncate_inode_pages(sbi->ipbmap->i_mapping, 0);

		IWRITE_LOCK(sbi->ipimap, RDWRLOCK_IMAP);
		diUnmount(sbi->ipimap, 1);
		if ((rc = diMount(sbi->ipimap))) {
			IWRITE_UNLOCK(sbi->ipimap);
			jfs_err("jfs_mount_rw: diMount failed!");
			return rc;
		}
		IWRITE_UNLOCK(sbi->ipimap);

		dbUnmount(sbi->ipbmap, 1);
		if ((rc = dbMount(sbi->ipbmap))) {
			jfs_err("jfs_mount_rw: dbMount failed!");
			return rc;
		}
	}

	 
	if ((rc = lmLogOpen(sb)))
		return rc;

	 
	if ((rc = updateSuper(sb, FM_MOUNT))) {
		jfs_err("jfs_mount: updateSuper failed w/rc = %d", rc);
		lmLogClose(sb);
		return rc;
	}

	 
	logMOUNT(sb);

	return rc;
}

 
static int chkSuper(struct super_block *sb)
{
	int rc = 0;
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_superblock *j_sb;
	struct buffer_head *bh;
	int AIM_bytesize, AIT_bytesize;
	int expected_AIM_bytesize, expected_AIT_bytesize;
	s64 AIM_byte_addr, AIT_byte_addr, fsckwsp_addr;
	s64 byte_addr_diff0, byte_addr_diff1;
	s32 bsize;

	if ((rc = readSuper(sb, &bh)))
		return rc;
	j_sb = (struct jfs_superblock *)bh->b_data;

	 
	 
	if (strncmp(j_sb->s_magic, JFS_MAGIC, 4) ||
	    le32_to_cpu(j_sb->s_version) > JFS_VERSION) {
		rc = -EINVAL;
		goto out;
	}

	bsize = le32_to_cpu(j_sb->s_bsize);
	if (bsize != PSIZE) {
		jfs_err("Only 4K block size supported!");
		rc = -EINVAL;
		goto out;
	}

	jfs_info("superblock: flag:0x%08x state:0x%08x size:0x%Lx",
		 le32_to_cpu(j_sb->s_flag), le32_to_cpu(j_sb->s_state),
		 (unsigned long long) le64_to_cpu(j_sb->s_size));

	 
	if ((j_sb->s_flag & cpu_to_le32(JFS_BAD_SAIT)) !=
	    cpu_to_le32(JFS_BAD_SAIT)) {
		expected_AIM_bytesize = 2 * PSIZE;
		AIM_bytesize = lengthPXD(&(j_sb->s_aim2)) * bsize;
		expected_AIT_bytesize = 4 * PSIZE;
		AIT_bytesize = lengthPXD(&(j_sb->s_ait2)) * bsize;
		AIM_byte_addr = addressPXD(&(j_sb->s_aim2)) * bsize;
		AIT_byte_addr = addressPXD(&(j_sb->s_ait2)) * bsize;
		byte_addr_diff0 = AIT_byte_addr - AIM_byte_addr;
		fsckwsp_addr = addressPXD(&(j_sb->s_fsckpxd)) * bsize;
		byte_addr_diff1 = fsckwsp_addr - AIT_byte_addr;
		if ((AIM_bytesize != expected_AIM_bytesize) ||
		    (AIT_bytesize != expected_AIT_bytesize) ||
		    (byte_addr_diff0 != AIM_bytesize) ||
		    (byte_addr_diff1 <= AIT_bytesize))
			j_sb->s_flag |= cpu_to_le32(JFS_BAD_SAIT);
	}

	if ((j_sb->s_flag & cpu_to_le32(JFS_GROUPCOMMIT)) !=
	    cpu_to_le32(JFS_GROUPCOMMIT))
		j_sb->s_flag |= cpu_to_le32(JFS_GROUPCOMMIT);

	 
	if (j_sb->s_state != cpu_to_le32(FM_CLEAN) &&
	    !sb_rdonly(sb)) {
		jfs_err("jfs_mount: Mount Failure: File System Dirty.");
		rc = -EINVAL;
		goto out;
	}

	sbi->state = le32_to_cpu(j_sb->s_state);
	sbi->mntflag = le32_to_cpu(j_sb->s_flag);

	 
	sbi->bsize = bsize;
	sbi->l2bsize = le16_to_cpu(j_sb->s_l2bsize);

	 
	if (sbi->l2bsize != ilog2((u32)bsize) ||
	    j_sb->pad != 0 ||
	    le32_to_cpu(j_sb->s_state) > FM_STATE_MAX) {
		rc = -EINVAL;
		jfs_err("jfs_mount: Mount Failure: superblock is corrupt!");
		goto out;
	}

	 
	sbi->nbperpage = PSIZE >> sbi->l2bsize;
	sbi->l2nbperpage = L2PSIZE - sbi->l2bsize;
	sbi->l2niperblk = sbi->l2bsize - L2DISIZE;
	if (sbi->mntflag & JFS_INLINELOG)
		sbi->logpxd = j_sb->s_logpxd;
	else {
		sbi->logdev = new_decode_dev(le32_to_cpu(j_sb->s_logdev));
		uuid_copy(&sbi->uuid, &j_sb->s_uuid);
		uuid_copy(&sbi->loguuid, &j_sb->s_loguuid);
	}
	sbi->fsckpxd = j_sb->s_fsckpxd;
	sbi->ait2 = j_sb->s_ait2;

      out:
	brelse(bh);
	return rc;
}


 
int updateSuper(struct super_block *sb, uint state)
{
	struct jfs_superblock *j_sb;
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct buffer_head *bh;
	int rc;

	if (sbi->flag & JFS_NOINTEGRITY) {
		if (state == FM_DIRTY) {
			sbi->p_state = state;
			return 0;
		} else if (state == FM_MOUNT) {
			sbi->p_state = sbi->state;
			state = FM_DIRTY;
		} else if (state == FM_CLEAN) {
			state = sbi->p_state;
		} else
			jfs_err("updateSuper: bad state");
	} else if (sbi->state == FM_DIRTY)
		return 0;

	if ((rc = readSuper(sb, &bh)))
		return rc;

	j_sb = (struct jfs_superblock *)bh->b_data;

	j_sb->s_state = cpu_to_le32(state);
	sbi->state = state;

	if (state == FM_MOUNT) {
		 
		j_sb->s_logdev = cpu_to_le32(new_encode_dev(sbi->log->bdev->bd_dev));
		j_sb->s_logserial = cpu_to_le32(sbi->log->serial);
	} else if (state == FM_CLEAN) {
		 
		if (j_sb->s_flag & cpu_to_le32(JFS_DASD_ENABLED))
			j_sb->s_flag |= cpu_to_le32(JFS_DASD_PRIME);
	}

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	return 0;
}


 
int readSuper(struct super_block *sb, struct buffer_head **bpp)
{
	 
	*bpp = sb_bread(sb, SUPER1_OFF >> sb->s_blocksize_bits);
	if (*bpp)
		return 0;

	 
	*bpp = sb_bread(sb, SUPER2_OFF >> sb->s_blocksize_bits);
	if (*bpp)
		return 0;

	return -EIO;
}


 
static int logMOUNT(struct super_block *sb)
{
	struct jfs_log *log = JFS_SBI(sb)->log;
	struct lrd lrd;

	lrd.logtid = 0;
	lrd.backchain = 0;
	lrd.type = cpu_to_le16(LOG_MOUNT);
	lrd.length = 0;
	lrd.aggregate = cpu_to_le32(new_encode_dev(sb->s_bdev->bd_dev));
	lmLog(log, NULL, &lrd, NULL);

	return 0;
}
