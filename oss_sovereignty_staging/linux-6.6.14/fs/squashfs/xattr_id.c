
 

 

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/slab.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs.h"
#include "xattr.h"

 
int squashfs_xattr_lookup(struct super_block *sb, unsigned int index,
		int *count, unsigned int *size, unsigned long long *xattr)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	int block = SQUASHFS_XATTR_BLOCK(index);
	int offset = SQUASHFS_XATTR_BLOCK_OFFSET(index);
	u64 start_block;
	struct squashfs_xattr_id id;
	int err;

	if (index >= msblk->xattr_ids)
		return -EINVAL;

	start_block = le64_to_cpu(msblk->xattr_id_table[block]);

	err = squashfs_read_metadata(sb, &id, &start_block, &offset,
							sizeof(id));
	if (err < 0)
		return err;

	*xattr = le64_to_cpu(id.xattr);
	*size = le32_to_cpu(id.size);
	*count = le32_to_cpu(id.count);
	return 0;
}


 
__le64 *squashfs_read_xattr_id_table(struct super_block *sb, u64 table_start,
		u64 *xattr_table_start, unsigned int *xattr_ids)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	unsigned int len, indexes;
	struct squashfs_xattr_id_table *id_table;
	__le64 *table;
	u64 start, end;
	int n;

	id_table = squashfs_read_table(sb, table_start, sizeof(*id_table));
	if (IS_ERR(id_table))
		return (__le64 *) id_table;

	*xattr_table_start = le64_to_cpu(id_table->xattr_table_start);
	*xattr_ids = le32_to_cpu(id_table->xattr_ids);
	kfree(id_table);

	 

	 
	if (*xattr_ids == 0)
		return ERR_PTR(-EINVAL);

	len = SQUASHFS_XATTR_BLOCK_BYTES(*xattr_ids);
	indexes = SQUASHFS_XATTR_BLOCKS(*xattr_ids);

	 
	start = table_start + sizeof(*id_table);
	end = msblk->bytes_used;

	if (len != (end - start))
		return ERR_PTR(-EINVAL);

	table = squashfs_read_table(sb, start, len);
	if (IS_ERR(table))
		return table;

	 
	for (n = 0; n < (indexes - 1); n++) {
		start = le64_to_cpu(table[n]);
		end = le64_to_cpu(table[n + 1]);

		if (start >= end || (end - start) >
				(SQUASHFS_METADATA_SIZE + SQUASHFS_BLOCK_OFFSET)) {
			kfree(table);
			return ERR_PTR(-EINVAL);
		}
	}

	start = le64_to_cpu(table[indexes - 1]);
	if (start >= table_start || (table_start - start) >
				(SQUASHFS_METADATA_SIZE + SQUASHFS_BLOCK_OFFSET)) {
		kfree(table);
		return ERR_PTR(-EINVAL);
	}

	if (*xattr_table_start >= le64_to_cpu(table[0])) {
		kfree(table);
		return ERR_PTR(-EINVAL);
	}

	return table;
}
