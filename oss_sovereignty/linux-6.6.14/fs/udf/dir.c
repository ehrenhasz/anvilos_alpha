
 

#include "udfdecl.h"

#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/iversion.h>

#include "udf_i.h"
#include "udf_sb.h"

static int udf_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *dir = file_inode(file);
	loff_t nf_pos, emit_pos = 0;
	int flen;
	unsigned char *fname = NULL;
	int ret = 0;
	struct super_block *sb = dir->i_sb;
	bool pos_valid = false;
	struct udf_fileident_iter iter;

	if (ctx->pos == 0) {
		if (!dir_emit_dot(file, ctx))
			return 0;
		ctx->pos = 1;
	}
	nf_pos = (ctx->pos - 1) << 2;
	if (nf_pos >= dir->i_size)
		goto out;

	 
	if (!inode_eq_iversion(dir, file->f_version)) {
		emit_pos = nf_pos;
		nf_pos = 0;
	} else {
		pos_valid = true;
	}

	fname = kmalloc(UDF_NAME_LEN, GFP_NOFS);
	if (!fname) {
		ret = -ENOMEM;
		goto out;
	}

	for (ret = udf_fiiter_init(&iter, dir, nf_pos);
	     !ret && iter.pos < dir->i_size;
	     ret = udf_fiiter_advance(&iter)) {
		struct kernel_lb_addr tloc;
		udf_pblk_t iblock;

		 
		if (iter.pos < emit_pos)
			continue;

		 
		pos_valid = true;
		ctx->pos = (iter.pos >> 2) + 1;

		if (iter.fi.fileCharacteristics & FID_FILE_CHAR_DELETED) {
			if (!UDF_QUERY_FLAG(sb, UDF_FLAG_UNDELETE))
				continue;
		}

		if (iter.fi.fileCharacteristics & FID_FILE_CHAR_HIDDEN) {
			if (!UDF_QUERY_FLAG(sb, UDF_FLAG_UNHIDE))
				continue;
		}

		if (iter.fi.fileCharacteristics & FID_FILE_CHAR_PARENT) {
			if (!dir_emit_dotdot(file, ctx))
				goto out_iter;
			continue;
		}

		flen = udf_get_filename(sb, iter.name,
				iter.fi.lengthFileIdent, fname, UDF_NAME_LEN);
		if (flen < 0)
			continue;

		tloc = lelb_to_cpu(iter.fi.icb.extLocation);
		iblock = udf_get_lb_pblock(sb, &tloc, 0);
		if (!dir_emit(ctx, fname, flen, iblock, DT_UNKNOWN))
			goto out_iter;
	}

	if (!ret) {
		ctx->pos = (iter.pos >> 2) + 1;
		pos_valid = true;
	}
out_iter:
	udf_fiiter_release(&iter);
out:
	if (pos_valid)
		file->f_version = inode_query_iversion(dir);
	kfree(fname);

	return ret;
}

 
const struct file_operations udf_dir_operations = {
	.llseek			= generic_file_llseek,
	.read			= generic_read_dir,
	.iterate_shared		= udf_readdir,
	.unlocked_ioctl		= udf_ioctl,
	.fsync			= generic_file_fsync,
};
