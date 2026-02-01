
 

 

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/slab.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs.h"

 
int squashfs_get_id(struct super_block *sb, unsigned int index,
					unsigned int *id)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	int block = SQUASHFS_ID_BLOCK(index);
	int offset = SQUASHFS_ID_BLOCK_OFFSET(index);
	u64 start_block;
	__le32 disk_id;
	int err;

	if (index >= msblk->ids)
		return -EINVAL;

	start_block = le64_to_cpu(msblk->id_table[block]);

	err = squashfs_read_metadata(sb, &disk_id, &start_block, &offset,
							sizeof(disk_id));
	if (err < 0)
		return err;

	*id = le32_to_cpu(disk_id);
	return 0;
}


 
__le64 *squashfs_read_id_index_table(struct super_block *sb,
		u64 id_table_start, u64 next_table, unsigned short no_ids)
{
	unsigned int length = SQUASHFS_ID_BLOCK_BYTES(no_ids);
	unsigned int indexes = SQUASHFS_ID_BLOCKS(no_ids);
	int n;
	__le64 *table;
	u64 start, end;

	TRACE("In read_id_index_table, length %d\n", length);

	 

	 
	if (no_ids == 0)
		return ERR_PTR(-EINVAL);

	 
	if (length != (next_table - id_table_start))
		return ERR_PTR(-EINVAL);

	table = squashfs_read_table(sb, id_table_start, length);
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
	if (start >= id_table_start || (id_table_start - start) >
				(SQUASHFS_METADATA_SIZE + SQUASHFS_BLOCK_OFFSET)) {
		kfree(table);
		return ERR_PTR(-EINVAL);
	}

	return table;
}
