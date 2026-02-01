
 

#include <linux/mtd/super.h>
#include <linux/namei.h>
#include <linux/export.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/backing-dev.h>
#include <linux/blkdev.h>
#include <linux/fs_context.h>
#include "mtdcore.h"

 
static int mtd_get_sb(struct fs_context *fc,
		      struct mtd_info *mtd,
		      int (*fill_super)(struct super_block *,
					struct fs_context *))
{
	struct super_block *sb;
	int ret;

	sb = sget_dev(fc, MKDEV(MTD_BLOCK_MAJOR, mtd->index));
	if (IS_ERR(sb))
		return PTR_ERR(sb);

	if (sb->s_root) {
		 
		pr_debug("MTDSB: Device %d (\"%s\") is already mounted\n",
			 mtd->index, mtd->name);
		put_mtd_device(mtd);
	} else {
		 
		pr_debug("MTDSB: New superblock for device %d (\"%s\")\n",
			 mtd->index, mtd->name);

		 
		sb->s_mtd = mtd;
		sb->s_bdi = bdi_get(mtd_bdi);

		ret = fill_super(sb, fc);
		if (ret < 0)
			goto error_sb;

		sb->s_flags |= SB_ACTIVE;
	}

	BUG_ON(fc->root);
	fc->root = dget(sb->s_root);
	return 0;

error_sb:
	deactivate_locked_super(sb);
	return ret;
}

 
static int mtd_get_sb_by_nr(struct fs_context *fc, int mtdnr,
			    int (*fill_super)(struct super_block *,
					      struct fs_context *))
{
	struct mtd_info *mtd;

	mtd = get_mtd_device(NULL, mtdnr);
	if (IS_ERR(mtd)) {
		errorf(fc, "MTDSB: Device #%u doesn't appear to exist\n", mtdnr);
		return PTR_ERR(mtd);
	}

	return mtd_get_sb(fc, mtd, fill_super);
}

 
int get_tree_mtd(struct fs_context *fc,
	      int (*fill_super)(struct super_block *sb,
				struct fs_context *fc))
{
#ifdef CONFIG_BLOCK
	dev_t dev;
	int ret;
#endif
	int mtdnr;

	if (!fc->source)
		return invalf(fc, "No source specified");

	pr_debug("MTDSB: dev_name \"%s\"\n", fc->source);

	 
	if (fc->source[0] == 'm' &&
	    fc->source[1] == 't' &&
	    fc->source[2] == 'd') {
		if (fc->source[3] == ':') {
			struct mtd_info *mtd;

			 
			pr_debug("MTDSB: mtd:%%s, name \"%s\"\n",
				 fc->source + 4);

			mtd = get_mtd_device_nm(fc->source + 4);
			if (!IS_ERR(mtd))
				return mtd_get_sb(fc, mtd, fill_super);

			errorf(fc, "MTD: MTD device with name \"%s\" not found",
			       fc->source + 4);

		} else if (isdigit(fc->source[3])) {
			 
			char *endptr;

			mtdnr = simple_strtoul(fc->source + 3, &endptr, 0);
			if (!*endptr) {
				 
				pr_debug("MTDSB: mtd%%d, mtdnr %d\n", mtdnr);
				return mtd_get_sb_by_nr(fc, mtdnr, fill_super);
			}
		}
	}

#ifdef CONFIG_BLOCK
	 
	ret = lookup_bdev(fc->source, &dev);
	if (ret) {
		errorf(fc, "MTD: Couldn't look up '%s': %d", fc->source, ret);
		return ret;
	}
	pr_debug("MTDSB: lookup_bdev() returned 0\n");

	if (MAJOR(dev) == MTD_BLOCK_MAJOR)
		return mtd_get_sb_by_nr(fc, MINOR(dev), fill_super);

#endif  

	if (!(fc->sb_flags & SB_SILENT))
		errorf(fc, "MTD: Attempt to mount non-MTD device \"%s\"",
		       fc->source);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(get_tree_mtd);

 
void kill_mtd_super(struct super_block *sb)
{
	generic_shutdown_super(sb);
	put_mtd_device(sb->s_mtd);
	sb->s_mtd = NULL;
}

EXPORT_SYMBOL_GPL(kill_mtd_super);
