
 

#include <linux/buffer_head.h>

#include "befs.h"
#include "io.h"

 

struct buffer_head *
befs_bread_iaddr(struct super_block *sb, befs_inode_addr iaddr)
{
	struct buffer_head *bh;
	befs_blocknr_t block;
	struct befs_sb_info *befs_sb = BEFS_SB(sb);

	befs_debug(sb, "---> Enter %s "
		   "[%u, %hu, %hu]", __func__, iaddr.allocation_group,
		   iaddr.start, iaddr.len);

	if (iaddr.allocation_group > befs_sb->num_ags) {
		befs_error(sb, "BEFS: Invalid allocation group %u, max is %u",
			   iaddr.allocation_group, befs_sb->num_ags);
		goto error;
	}

	block = iaddr2blockno(sb, &iaddr);

	befs_debug(sb, "%s: offset = %lu", __func__, (unsigned long)block);

	bh = sb_bread(sb, block);

	if (bh == NULL) {
		befs_error(sb, "Failed to read block %lu",
			   (unsigned long)block);
		goto error;
	}

	befs_debug(sb, "<--- %s", __func__);
	return bh;

error:
	befs_debug(sb, "<--- %s ERROR", __func__);
	return NULL;
}
