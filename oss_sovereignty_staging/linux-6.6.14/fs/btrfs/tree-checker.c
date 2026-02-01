
 

 

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/error-injection.h>
#include "messages.h"
#include "ctree.h"
#include "tree-checker.h"
#include "disk-io.h"
#include "compression.h"
#include "volumes.h"
#include "misc.h"
#include "fs.h"
#include "accessors.h"
#include "file-item.h"
#include "inode-item.h"

 

 
__printf(3, 4)
__cold
static void generic_err(const struct extent_buffer *eb, int slot,
			const char *fmt, ...)
{
	const struct btrfs_fs_info *fs_info = eb->fs_info;
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	btrfs_crit(fs_info,
		"corrupt %s: root=%llu block=%llu slot=%d, %pV",
		btrfs_header_level(eb) == 0 ? "leaf" : "node",
		btrfs_header_owner(eb), btrfs_header_bytenr(eb), slot, &vaf);
	va_end(args);
}

 
__printf(3, 4)
__cold
static void file_extent_err(const struct extent_buffer *eb, int slot,
			    const char *fmt, ...)
{
	const struct btrfs_fs_info *fs_info = eb->fs_info;
	struct btrfs_key key;
	struct va_format vaf;
	va_list args;

	btrfs_item_key_to_cpu(eb, &key, slot);
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	btrfs_crit(fs_info,
	"corrupt %s: root=%llu block=%llu slot=%d ino=%llu file_offset=%llu, %pV",
		btrfs_header_level(eb) == 0 ? "leaf" : "node",
		btrfs_header_owner(eb), btrfs_header_bytenr(eb), slot,
		key.objectid, key.offset, &vaf);
	va_end(args);
}

 
#define CHECK_FE_ALIGNED(leaf, slot, fi, name, alignment)		      \
({									      \
	if (unlikely(!IS_ALIGNED(btrfs_file_extent_##name((leaf), (fi)),      \
				 (alignment))))				      \
		file_extent_err((leaf), (slot),				      \
	"invalid %s for file extent, have %llu, should be aligned to %u",     \
			(#name), btrfs_file_extent_##name((leaf), (fi)),      \
			(alignment));					      \
	(!IS_ALIGNED(btrfs_file_extent_##name((leaf), (fi)), (alignment)));   \
})

static u64 file_extent_end(struct extent_buffer *leaf,
			   struct btrfs_key *key,
			   struct btrfs_file_extent_item *extent)
{
	u64 end;
	u64 len;

	if (btrfs_file_extent_type(leaf, extent) == BTRFS_FILE_EXTENT_INLINE) {
		len = btrfs_file_extent_ram_bytes(leaf, extent);
		end = ALIGN(key->offset + len, leaf->fs_info->sectorsize);
	} else {
		len = btrfs_file_extent_num_bytes(leaf, extent);
		end = key->offset + len;
	}
	return end;
}

 
__printf(3, 4)
__cold
static void dir_item_err(const struct extent_buffer *eb, int slot,
			 const char *fmt, ...)
{
	const struct btrfs_fs_info *fs_info = eb->fs_info;
	struct btrfs_key key;
	struct va_format vaf;
	va_list args;

	btrfs_item_key_to_cpu(eb, &key, slot);
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	btrfs_crit(fs_info,
		"corrupt %s: root=%llu block=%llu slot=%d ino=%llu, %pV",
		btrfs_header_level(eb) == 0 ? "leaf" : "node",
		btrfs_header_owner(eb), btrfs_header_bytenr(eb), slot,
		key.objectid, &vaf);
	va_end(args);
}

 
static bool check_prev_ino(struct extent_buffer *leaf,
			   struct btrfs_key *key, int slot,
			   struct btrfs_key *prev_key)
{
	 
	if (slot == 0)
		return true;

	 
	ASSERT(key->type == BTRFS_XATTR_ITEM_KEY ||
	       key->type == BTRFS_INODE_REF_KEY ||
	       key->type == BTRFS_DIR_INDEX_KEY ||
	       key->type == BTRFS_DIR_ITEM_KEY ||
	       key->type == BTRFS_EXTENT_DATA_KEY);

	 
	if (!is_fstree(btrfs_header_owner(leaf)))
		return true;

	if (key->objectid == prev_key->objectid)
		return true;

	 
	dir_item_err(leaf, slot,
		"invalid previous key objectid, have %llu expect %llu",
		prev_key->objectid, key->objectid);
	return false;
}
static int check_extent_data_item(struct extent_buffer *leaf,
				  struct btrfs_key *key, int slot,
				  struct btrfs_key *prev_key)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_file_extent_item *fi;
	u32 sectorsize = fs_info->sectorsize;
	u32 item_size = btrfs_item_size(leaf, slot);
	u64 extent_end;

	if (unlikely(!IS_ALIGNED(key->offset, sectorsize))) {
		file_extent_err(leaf, slot,
"unaligned file_offset for file extent, have %llu should be aligned to %u",
			key->offset, sectorsize);
		return -EUCLEAN;
	}

	 
	if (unlikely(!check_prev_ino(leaf, key, slot, prev_key)))
		return -EUCLEAN;

	fi = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);

	 
	if (unlikely(item_size < BTRFS_FILE_EXTENT_INLINE_DATA_START)) {
		file_extent_err(leaf, slot,
				"invalid item size, have %u expect [%zu, %u)",
				item_size, BTRFS_FILE_EXTENT_INLINE_DATA_START,
				SZ_4K);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_file_extent_type(leaf, fi) >=
		     BTRFS_NR_FILE_EXTENT_TYPES)) {
		file_extent_err(leaf, slot,
		"invalid type for file extent, have %u expect range [0, %u]",
			btrfs_file_extent_type(leaf, fi),
			BTRFS_NR_FILE_EXTENT_TYPES - 1);
		return -EUCLEAN;
	}

	 
	if (unlikely(btrfs_file_extent_compression(leaf, fi) >=
		     BTRFS_NR_COMPRESS_TYPES)) {
		file_extent_err(leaf, slot,
	"invalid compression for file extent, have %u expect range [0, %u]",
			btrfs_file_extent_compression(leaf, fi),
			BTRFS_NR_COMPRESS_TYPES - 1);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_file_extent_encryption(leaf, fi))) {
		file_extent_err(leaf, slot,
			"invalid encryption for file extent, have %u expect 0",
			btrfs_file_extent_encryption(leaf, fi));
		return -EUCLEAN;
	}
	if (btrfs_file_extent_type(leaf, fi) == BTRFS_FILE_EXTENT_INLINE) {
		 
		if (unlikely(key->offset)) {
			file_extent_err(leaf, slot,
		"invalid file_offset for inline file extent, have %llu expect 0",
				key->offset);
			return -EUCLEAN;
		}

		 
		if (btrfs_file_extent_compression(leaf, fi) !=
		    BTRFS_COMPRESS_NONE)
			return 0;

		 
		if (unlikely(item_size != BTRFS_FILE_EXTENT_INLINE_DATA_START +
					  btrfs_file_extent_ram_bytes(leaf, fi))) {
			file_extent_err(leaf, slot,
	"invalid ram_bytes for uncompressed inline extent, have %u expect %llu",
				item_size, BTRFS_FILE_EXTENT_INLINE_DATA_START +
				btrfs_file_extent_ram_bytes(leaf, fi));
			return -EUCLEAN;
		}
		return 0;
	}

	 
	if (unlikely(item_size != sizeof(*fi))) {
		file_extent_err(leaf, slot,
	"invalid item size for reg/prealloc file extent, have %u expect %zu",
			item_size, sizeof(*fi));
		return -EUCLEAN;
	}
	if (unlikely(CHECK_FE_ALIGNED(leaf, slot, fi, ram_bytes, sectorsize) ||
		     CHECK_FE_ALIGNED(leaf, slot, fi, disk_bytenr, sectorsize) ||
		     CHECK_FE_ALIGNED(leaf, slot, fi, disk_num_bytes, sectorsize) ||
		     CHECK_FE_ALIGNED(leaf, slot, fi, offset, sectorsize) ||
		     CHECK_FE_ALIGNED(leaf, slot, fi, num_bytes, sectorsize)))
		return -EUCLEAN;

	 
	if (unlikely(check_add_overflow(btrfs_file_extent_num_bytes(leaf, fi),
					key->offset, &extent_end))) {
		file_extent_err(leaf, slot,
	"extent end overflow, have file offset %llu extent num bytes %llu",
				key->offset,
				btrfs_file_extent_num_bytes(leaf, fi));
		return -EUCLEAN;
	}

	 
	if (slot > 0 &&
	    prev_key->objectid == key->objectid &&
	    prev_key->type == BTRFS_EXTENT_DATA_KEY) {
		struct btrfs_file_extent_item *prev_fi;
		u64 prev_end;

		prev_fi = btrfs_item_ptr(leaf, slot - 1,
					 struct btrfs_file_extent_item);
		prev_end = file_extent_end(leaf, prev_key, prev_fi);
		if (unlikely(prev_end > key->offset)) {
			file_extent_err(leaf, slot - 1,
"file extent end range (%llu) goes beyond start offset (%llu) of the next file extent",
					prev_end, key->offset);
			return -EUCLEAN;
		}
	}

	return 0;
}

static int check_csum_item(struct extent_buffer *leaf, struct btrfs_key *key,
			   int slot, struct btrfs_key *prev_key)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	u32 sectorsize = fs_info->sectorsize;
	const u32 csumsize = fs_info->csum_size;

	if (unlikely(key->objectid != BTRFS_EXTENT_CSUM_OBJECTID)) {
		generic_err(leaf, slot,
		"invalid key objectid for csum item, have %llu expect %llu",
			key->objectid, BTRFS_EXTENT_CSUM_OBJECTID);
		return -EUCLEAN;
	}
	if (unlikely(!IS_ALIGNED(key->offset, sectorsize))) {
		generic_err(leaf, slot,
	"unaligned key offset for csum item, have %llu should be aligned to %u",
			key->offset, sectorsize);
		return -EUCLEAN;
	}
	if (unlikely(!IS_ALIGNED(btrfs_item_size(leaf, slot), csumsize))) {
		generic_err(leaf, slot,
	"unaligned item size for csum item, have %u should be aligned to %u",
			btrfs_item_size(leaf, slot), csumsize);
		return -EUCLEAN;
	}
	if (slot > 0 && prev_key->type == BTRFS_EXTENT_CSUM_KEY) {
		u64 prev_csum_end;
		u32 prev_item_size;

		prev_item_size = btrfs_item_size(leaf, slot - 1);
		prev_csum_end = (prev_item_size / csumsize) * sectorsize;
		prev_csum_end += prev_key->offset;
		if (unlikely(prev_csum_end > key->offset)) {
			generic_err(leaf, slot - 1,
"csum end range (%llu) goes beyond the start range (%llu) of the next csum item",
				    prev_csum_end, key->offset);
			return -EUCLEAN;
		}
	}
	return 0;
}

 
#define inode_item_err(eb, slot, fmt, ...)			\
	dir_item_err(eb, slot, fmt, __VA_ARGS__)

static int check_inode_key(struct extent_buffer *leaf, struct btrfs_key *key,
			   int slot)
{
	struct btrfs_key item_key;
	bool is_inode_item;

	btrfs_item_key_to_cpu(leaf, &item_key, slot);
	is_inode_item = (item_key.type == BTRFS_INODE_ITEM_KEY);

	 
	if (item_key.type == BTRFS_XATTR_ITEM_KEY) {
		if (unlikely(key->objectid != 0 || key->type != 0 ||
			     key->offset != 0))
			return -EUCLEAN;
		return 0;
	}

	if (unlikely((key->objectid < BTRFS_FIRST_FREE_OBJECTID ||
		      key->objectid > BTRFS_LAST_FREE_OBJECTID) &&
		     key->objectid != BTRFS_ROOT_TREE_DIR_OBJECTID &&
		     key->objectid != BTRFS_FREE_INO_OBJECTID)) {
		if (is_inode_item) {
			generic_err(leaf, slot,
	"invalid key objectid: has %llu expect %llu or [%llu, %llu] or %llu",
				key->objectid, BTRFS_ROOT_TREE_DIR_OBJECTID,
				BTRFS_FIRST_FREE_OBJECTID,
				BTRFS_LAST_FREE_OBJECTID,
				BTRFS_FREE_INO_OBJECTID);
		} else {
			dir_item_err(leaf, slot,
"invalid location key objectid: has %llu expect %llu or [%llu, %llu] or %llu",
				key->objectid, BTRFS_ROOT_TREE_DIR_OBJECTID,
				BTRFS_FIRST_FREE_OBJECTID,
				BTRFS_LAST_FREE_OBJECTID,
				BTRFS_FREE_INO_OBJECTID);
		}
		return -EUCLEAN;
	}
	if (unlikely(key->offset != 0)) {
		if (is_inode_item)
			inode_item_err(leaf, slot,
				       "invalid key offset: has %llu expect 0",
				       key->offset);
		else
			dir_item_err(leaf, slot,
				"invalid location key offset:has %llu expect 0",
				key->offset);
		return -EUCLEAN;
	}
	return 0;
}

static int check_root_key(struct extent_buffer *leaf, struct btrfs_key *key,
			  int slot)
{
	struct btrfs_key item_key;
	bool is_root_item;

	btrfs_item_key_to_cpu(leaf, &item_key, slot);
	is_root_item = (item_key.type == BTRFS_ROOT_ITEM_KEY);

	 
	if (unlikely(is_root_item && key->objectid == BTRFS_TREE_RELOC_OBJECTID &&
		     !is_fstree(key->offset))) {
		generic_err(leaf, slot,
		"invalid reloc tree for root %lld, root id is not a subvolume tree",
			    key->offset);
		return -EUCLEAN;
	}

	 
	if (unlikely(key->objectid == 0)) {
		if (is_root_item)
			generic_err(leaf, slot, "invalid root id 0");
		else
			dir_item_err(leaf, slot,
				     "invalid location key root id 0");
		return -EUCLEAN;
	}

	 
	if (unlikely(!is_fstree(key->objectid) && !is_root_item)) {
		dir_item_err(leaf, slot,
		"invalid location key objectid, have %llu expect [%llu, %llu]",
				key->objectid, BTRFS_FIRST_FREE_OBJECTID,
				BTRFS_LAST_FREE_OBJECTID);
		return -EUCLEAN;
	}

	 
	if (unlikely(key->objectid == BTRFS_TREE_RELOC_OBJECTID &&
		     key->offset == 0)) {
		generic_err(leaf, slot, "invalid root id 0 for reloc tree");
		return -EUCLEAN;
	}
	return 0;
}

static int check_dir_item(struct extent_buffer *leaf,
			  struct btrfs_key *key, struct btrfs_key *prev_key,
			  int slot)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_dir_item *di;
	u32 item_size = btrfs_item_size(leaf, slot);
	u32 cur = 0;

	if (unlikely(!check_prev_ino(leaf, key, slot, prev_key)))
		return -EUCLEAN;

	di = btrfs_item_ptr(leaf, slot, struct btrfs_dir_item);
	while (cur < item_size) {
		struct btrfs_key location_key;
		u32 name_len;
		u32 data_len;
		u32 max_name_len;
		u32 total_size;
		u32 name_hash;
		u8 dir_type;
		int ret;

		 
		if (unlikely(cur + sizeof(*di) > item_size)) {
			dir_item_err(leaf, slot,
		"dir item header crosses item boundary, have %zu boundary %u",
				cur + sizeof(*di), item_size);
			return -EUCLEAN;
		}

		 
		btrfs_dir_item_key_to_cpu(leaf, di, &location_key);
		if (location_key.type == BTRFS_ROOT_ITEM_KEY) {
			ret = check_root_key(leaf, &location_key, slot);
			if (unlikely(ret < 0))
				return ret;
		} else if (location_key.type == BTRFS_INODE_ITEM_KEY ||
			   location_key.type == 0) {
			ret = check_inode_key(leaf, &location_key, slot);
			if (unlikely(ret < 0))
				return ret;
		} else {
			dir_item_err(leaf, slot,
			"invalid location key type, have %u, expect %u or %u",
				     location_key.type, BTRFS_ROOT_ITEM_KEY,
				     BTRFS_INODE_ITEM_KEY);
			return -EUCLEAN;
		}

		 
		dir_type = btrfs_dir_ftype(leaf, di);
		if (unlikely(dir_type >= BTRFS_FT_MAX)) {
			dir_item_err(leaf, slot,
			"invalid dir item type, have %u expect [0, %u)",
				dir_type, BTRFS_FT_MAX);
			return -EUCLEAN;
		}

		if (unlikely(key->type == BTRFS_XATTR_ITEM_KEY &&
			     dir_type != BTRFS_FT_XATTR)) {
			dir_item_err(leaf, slot,
		"invalid dir item type for XATTR key, have %u expect %u",
				dir_type, BTRFS_FT_XATTR);
			return -EUCLEAN;
		}
		if (unlikely(dir_type == BTRFS_FT_XATTR &&
			     key->type != BTRFS_XATTR_ITEM_KEY)) {
			dir_item_err(leaf, slot,
			"xattr dir type found for non-XATTR key");
			return -EUCLEAN;
		}
		if (dir_type == BTRFS_FT_XATTR)
			max_name_len = XATTR_NAME_MAX;
		else
			max_name_len = BTRFS_NAME_LEN;

		 
		name_len = btrfs_dir_name_len(leaf, di);
		data_len = btrfs_dir_data_len(leaf, di);
		if (unlikely(name_len > max_name_len)) {
			dir_item_err(leaf, slot,
			"dir item name len too long, have %u max %u",
				name_len, max_name_len);
			return -EUCLEAN;
		}
		if (unlikely(name_len + data_len > BTRFS_MAX_XATTR_SIZE(fs_info))) {
			dir_item_err(leaf, slot,
			"dir item name and data len too long, have %u max %u",
				name_len + data_len,
				BTRFS_MAX_XATTR_SIZE(fs_info));
			return -EUCLEAN;
		}

		if (unlikely(data_len && dir_type != BTRFS_FT_XATTR)) {
			dir_item_err(leaf, slot,
			"dir item with invalid data len, have %u expect 0",
				data_len);
			return -EUCLEAN;
		}

		total_size = sizeof(*di) + name_len + data_len;

		 
		if (unlikely(cur + total_size > item_size)) {
			dir_item_err(leaf, slot,
		"dir item data crosses item boundary, have %u boundary %u",
				cur + total_size, item_size);
			return -EUCLEAN;
		}

		 
		if (key->type == BTRFS_DIR_ITEM_KEY ||
		    key->type == BTRFS_XATTR_ITEM_KEY) {
			char namebuf[max(BTRFS_NAME_LEN, XATTR_NAME_MAX)];

			read_extent_buffer(leaf, namebuf,
					(unsigned long)(di + 1), name_len);
			name_hash = btrfs_name_hash(namebuf, name_len);
			if (unlikely(key->offset != name_hash)) {
				dir_item_err(leaf, slot,
		"name hash mismatch with key, have 0x%016x expect 0x%016llx",
					name_hash, key->offset);
				return -EUCLEAN;
			}
		}
		cur += total_size;
		di = (struct btrfs_dir_item *)((void *)di + total_size);
	}
	return 0;
}

__printf(3, 4)
__cold
static void block_group_err(const struct extent_buffer *eb, int slot,
			    const char *fmt, ...)
{
	const struct btrfs_fs_info *fs_info = eb->fs_info;
	struct btrfs_key key;
	struct va_format vaf;
	va_list args;

	btrfs_item_key_to_cpu(eb, &key, slot);
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	btrfs_crit(fs_info,
	"corrupt %s: root=%llu block=%llu slot=%d bg_start=%llu bg_len=%llu, %pV",
		btrfs_header_level(eb) == 0 ? "leaf" : "node",
		btrfs_header_owner(eb), btrfs_header_bytenr(eb), slot,
		key.objectid, key.offset, &vaf);
	va_end(args);
}

static int check_block_group_item(struct extent_buffer *leaf,
				  struct btrfs_key *key, int slot)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_block_group_item bgi;
	u32 item_size = btrfs_item_size(leaf, slot);
	u64 chunk_objectid;
	u64 flags;
	u64 type;

	 
	if (unlikely(key->offset == 0)) {
		block_group_err(leaf, slot,
				"invalid block group size 0");
		return -EUCLEAN;
	}

	if (unlikely(item_size != sizeof(bgi))) {
		block_group_err(leaf, slot,
			"invalid item size, have %u expect %zu",
				item_size, sizeof(bgi));
		return -EUCLEAN;
	}

	read_extent_buffer(leaf, &bgi, btrfs_item_ptr_offset(leaf, slot),
			   sizeof(bgi));
	chunk_objectid = btrfs_stack_block_group_chunk_objectid(&bgi);
	if (btrfs_fs_incompat(fs_info, EXTENT_TREE_V2)) {
		 
		if (unlikely(fs_info->nr_global_roots &&
			     chunk_objectid >= fs_info->nr_global_roots)) {
			block_group_err(leaf, slot,
	"invalid block group global root id, have %llu, needs to be <= %llu",
					chunk_objectid,
					fs_info->nr_global_roots);
			return -EUCLEAN;
		}
	} else if (unlikely(chunk_objectid != BTRFS_FIRST_CHUNK_TREE_OBJECTID)) {
		block_group_err(leaf, slot,
		"invalid block group chunk objectid, have %llu expect %llu",
				btrfs_stack_block_group_chunk_objectid(&bgi),
				BTRFS_FIRST_CHUNK_TREE_OBJECTID);
		return -EUCLEAN;
	}

	if (unlikely(btrfs_stack_block_group_used(&bgi) > key->offset)) {
		block_group_err(leaf, slot,
			"invalid block group used, have %llu expect [0, %llu)",
				btrfs_stack_block_group_used(&bgi), key->offset);
		return -EUCLEAN;
	}

	flags = btrfs_stack_block_group_flags(&bgi);
	if (unlikely(hweight64(flags & BTRFS_BLOCK_GROUP_PROFILE_MASK) > 1)) {
		block_group_err(leaf, slot,
"invalid profile flags, have 0x%llx (%lu bits set) expect no more than 1 bit set",
			flags & BTRFS_BLOCK_GROUP_PROFILE_MASK,
			hweight64(flags & BTRFS_BLOCK_GROUP_PROFILE_MASK));
		return -EUCLEAN;
	}

	type = flags & BTRFS_BLOCK_GROUP_TYPE_MASK;
	if (unlikely(type != BTRFS_BLOCK_GROUP_DATA &&
		     type != BTRFS_BLOCK_GROUP_METADATA &&
		     type != BTRFS_BLOCK_GROUP_SYSTEM &&
		     type != (BTRFS_BLOCK_GROUP_METADATA |
			      BTRFS_BLOCK_GROUP_DATA))) {
		block_group_err(leaf, slot,
"invalid type, have 0x%llx (%lu bits set) expect either 0x%llx, 0x%llx, 0x%llx or 0x%llx",
			type, hweight64(type),
			BTRFS_BLOCK_GROUP_DATA, BTRFS_BLOCK_GROUP_METADATA,
			BTRFS_BLOCK_GROUP_SYSTEM,
			BTRFS_BLOCK_GROUP_METADATA | BTRFS_BLOCK_GROUP_DATA);
		return -EUCLEAN;
	}
	return 0;
}

__printf(4, 5)
__cold
static void chunk_err(const struct extent_buffer *leaf,
		      const struct btrfs_chunk *chunk, u64 logical,
		      const char *fmt, ...)
{
	const struct btrfs_fs_info *fs_info = leaf->fs_info;
	bool is_sb;
	struct va_format vaf;
	va_list args;
	int i;
	int slot = -1;

	 
	is_sb = (leaf->start == BTRFS_SUPER_INFO_OFFSET);

	if (!is_sb) {
		 
		for (i = 0; i < btrfs_header_nritems(leaf); i++) {
			if (btrfs_item_ptr_offset(leaf, i) ==
					(unsigned long)chunk) {
				slot = i;
				break;
			}
		}
	}
	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	if (is_sb)
		btrfs_crit(fs_info,
		"corrupt superblock syschunk array: chunk_start=%llu, %pV",
			   logical, &vaf);
	else
		btrfs_crit(fs_info,
	"corrupt leaf: root=%llu block=%llu slot=%d chunk_start=%llu, %pV",
			   BTRFS_CHUNK_TREE_OBJECTID, leaf->start, slot,
			   logical, &vaf);
	va_end(args);
}

 
int btrfs_check_chunk_valid(struct extent_buffer *leaf,
			    struct btrfs_chunk *chunk, u64 logical)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	u64 length;
	u64 chunk_end;
	u64 stripe_len;
	u16 num_stripes;
	u16 sub_stripes;
	u64 type;
	u64 features;
	bool mixed = false;
	int raid_index;
	int nparity;
	int ncopies;

	length = btrfs_chunk_length(leaf, chunk);
	stripe_len = btrfs_chunk_stripe_len(leaf, chunk);
	num_stripes = btrfs_chunk_num_stripes(leaf, chunk);
	sub_stripes = btrfs_chunk_sub_stripes(leaf, chunk);
	type = btrfs_chunk_type(leaf, chunk);
	raid_index = btrfs_bg_flags_to_raid_index(type);
	ncopies = btrfs_raid_array[raid_index].ncopies;
	nparity = btrfs_raid_array[raid_index].nparity;

	if (unlikely(!num_stripes)) {
		chunk_err(leaf, chunk, logical,
			  "invalid chunk num_stripes, have %u", num_stripes);
		return -EUCLEAN;
	}
	if (unlikely(num_stripes < ncopies)) {
		chunk_err(leaf, chunk, logical,
			  "invalid chunk num_stripes < ncopies, have %u < %d",
			  num_stripes, ncopies);
		return -EUCLEAN;
	}
	if (unlikely(nparity && num_stripes == nparity)) {
		chunk_err(leaf, chunk, logical,
			  "invalid chunk num_stripes == nparity, have %u == %d",
			  num_stripes, nparity);
		return -EUCLEAN;
	}
	if (unlikely(!IS_ALIGNED(logical, fs_info->sectorsize))) {
		chunk_err(leaf, chunk, logical,
		"invalid chunk logical, have %llu should aligned to %u",
			  logical, fs_info->sectorsize);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_chunk_sector_size(leaf, chunk) != fs_info->sectorsize)) {
		chunk_err(leaf, chunk, logical,
			  "invalid chunk sectorsize, have %u expect %u",
			  btrfs_chunk_sector_size(leaf, chunk),
			  fs_info->sectorsize);
		return -EUCLEAN;
	}
	if (unlikely(!length || !IS_ALIGNED(length, fs_info->sectorsize))) {
		chunk_err(leaf, chunk, logical,
			  "invalid chunk length, have %llu", length);
		return -EUCLEAN;
	}
	if (unlikely(check_add_overflow(logical, length, &chunk_end))) {
		chunk_err(leaf, chunk, logical,
"invalid chunk logical start and length, have logical start %llu length %llu",
			  logical, length);
		return -EUCLEAN;
	}
	if (unlikely(!is_power_of_2(stripe_len) || stripe_len != BTRFS_STRIPE_LEN)) {
		chunk_err(leaf, chunk, logical,
			  "invalid chunk stripe length: %llu",
			  stripe_len);
		return -EUCLEAN;
	}
	 
	if (unlikely(length >= btrfs_stripe_nr_to_offset(U32_MAX))) {
		chunk_err(leaf, chunk, logical,
			  "chunk length too large: have %llu limit %llu",
			  length, btrfs_stripe_nr_to_offset(U32_MAX));
		return -EUCLEAN;
	}
	if (unlikely(type & ~(BTRFS_BLOCK_GROUP_TYPE_MASK |
			      BTRFS_BLOCK_GROUP_PROFILE_MASK))) {
		chunk_err(leaf, chunk, logical,
			  "unrecognized chunk type: 0x%llx",
			  ~(BTRFS_BLOCK_GROUP_TYPE_MASK |
			    BTRFS_BLOCK_GROUP_PROFILE_MASK) &
			  btrfs_chunk_type(leaf, chunk));
		return -EUCLEAN;
	}

	if (unlikely(!has_single_bit_set(type & BTRFS_BLOCK_GROUP_PROFILE_MASK) &&
		     (type & BTRFS_BLOCK_GROUP_PROFILE_MASK) != 0)) {
		chunk_err(leaf, chunk, logical,
		"invalid chunk profile flag: 0x%llx, expect 0 or 1 bit set",
			  type & BTRFS_BLOCK_GROUP_PROFILE_MASK);
		return -EUCLEAN;
	}
	if (unlikely((type & BTRFS_BLOCK_GROUP_TYPE_MASK) == 0)) {
		chunk_err(leaf, chunk, logical,
	"missing chunk type flag, have 0x%llx one bit must be set in 0x%llx",
			  type, BTRFS_BLOCK_GROUP_TYPE_MASK);
		return -EUCLEAN;
	}

	if (unlikely((type & BTRFS_BLOCK_GROUP_SYSTEM) &&
		     (type & (BTRFS_BLOCK_GROUP_METADATA |
			      BTRFS_BLOCK_GROUP_DATA)))) {
		chunk_err(leaf, chunk, logical,
			  "system chunk with data or metadata type: 0x%llx",
			  type);
		return -EUCLEAN;
	}

	features = btrfs_super_incompat_flags(fs_info->super_copy);
	if (features & BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS)
		mixed = true;

	if (!mixed) {
		if (unlikely((type & BTRFS_BLOCK_GROUP_METADATA) &&
			     (type & BTRFS_BLOCK_GROUP_DATA))) {
			chunk_err(leaf, chunk, logical,
			"mixed chunk type in non-mixed mode: 0x%llx", type);
			return -EUCLEAN;
		}
	}

	if (unlikely((type & BTRFS_BLOCK_GROUP_RAID10 &&
		      sub_stripes != btrfs_raid_array[BTRFS_RAID_RAID10].sub_stripes) ||
		     (type & BTRFS_BLOCK_GROUP_RAID1 &&
		      num_stripes != btrfs_raid_array[BTRFS_RAID_RAID1].devs_min) ||
		     (type & BTRFS_BLOCK_GROUP_RAID1C3 &&
		      num_stripes != btrfs_raid_array[BTRFS_RAID_RAID1C3].devs_min) ||
		     (type & BTRFS_BLOCK_GROUP_RAID1C4 &&
		      num_stripes != btrfs_raid_array[BTRFS_RAID_RAID1C4].devs_min) ||
		     (type & BTRFS_BLOCK_GROUP_RAID5 &&
		      num_stripes < btrfs_raid_array[BTRFS_RAID_RAID5].devs_min) ||
		     (type & BTRFS_BLOCK_GROUP_RAID6 &&
		      num_stripes < btrfs_raid_array[BTRFS_RAID_RAID6].devs_min) ||
		     (type & BTRFS_BLOCK_GROUP_DUP &&
		      num_stripes != btrfs_raid_array[BTRFS_RAID_DUP].dev_stripes) ||
		     ((type & BTRFS_BLOCK_GROUP_PROFILE_MASK) == 0 &&
		      num_stripes != btrfs_raid_array[BTRFS_RAID_SINGLE].dev_stripes))) {
		chunk_err(leaf, chunk, logical,
			"invalid num_stripes:sub_stripes %u:%u for profile %llu",
			num_stripes, sub_stripes,
			type & BTRFS_BLOCK_GROUP_PROFILE_MASK);
		return -EUCLEAN;
	}

	return 0;
}

 
static int check_leaf_chunk_item(struct extent_buffer *leaf,
				 struct btrfs_chunk *chunk,
				 struct btrfs_key *key, int slot)
{
	int num_stripes;

	if (unlikely(btrfs_item_size(leaf, slot) < sizeof(struct btrfs_chunk))) {
		chunk_err(leaf, chunk, key->offset,
			"invalid chunk item size: have %u expect [%zu, %u)",
			btrfs_item_size(leaf, slot),
			sizeof(struct btrfs_chunk),
			BTRFS_LEAF_DATA_SIZE(leaf->fs_info));
		return -EUCLEAN;
	}

	num_stripes = btrfs_chunk_num_stripes(leaf, chunk);
	 
	if (num_stripes == 0)
		goto out;

	if (unlikely(btrfs_chunk_item_size(num_stripes) !=
		     btrfs_item_size(leaf, slot))) {
		chunk_err(leaf, chunk, key->offset,
			"invalid chunk item size: have %u expect %lu",
			btrfs_item_size(leaf, slot),
			btrfs_chunk_item_size(num_stripes));
		return -EUCLEAN;
	}
out:
	return btrfs_check_chunk_valid(leaf, chunk, key->offset);
}

__printf(3, 4)
__cold
static void dev_item_err(const struct extent_buffer *eb, int slot,
			 const char *fmt, ...)
{
	struct btrfs_key key;
	struct va_format vaf;
	va_list args;

	btrfs_item_key_to_cpu(eb, &key, slot);
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	btrfs_crit(eb->fs_info,
	"corrupt %s: root=%llu block=%llu slot=%d devid=%llu %pV",
		btrfs_header_level(eb) == 0 ? "leaf" : "node",
		btrfs_header_owner(eb), btrfs_header_bytenr(eb), slot,
		key.objectid, &vaf);
	va_end(args);
}

static int check_dev_item(struct extent_buffer *leaf,
			  struct btrfs_key *key, int slot)
{
	struct btrfs_dev_item *ditem;
	const u32 item_size = btrfs_item_size(leaf, slot);

	if (unlikely(key->objectid != BTRFS_DEV_ITEMS_OBJECTID)) {
		dev_item_err(leaf, slot,
			     "invalid objectid: has=%llu expect=%llu",
			     key->objectid, BTRFS_DEV_ITEMS_OBJECTID);
		return -EUCLEAN;
	}

	if (unlikely(item_size != sizeof(*ditem))) {
		dev_item_err(leaf, slot, "invalid item size: has %u expect %zu",
			     item_size, sizeof(*ditem));
		return -EUCLEAN;
	}

	ditem = btrfs_item_ptr(leaf, slot, struct btrfs_dev_item);
	if (unlikely(btrfs_device_id(leaf, ditem) != key->offset)) {
		dev_item_err(leaf, slot,
			     "devid mismatch: key has=%llu item has=%llu",
			     key->offset, btrfs_device_id(leaf, ditem));
		return -EUCLEAN;
	}

	 
	if (unlikely(btrfs_device_bytes_used(leaf, ditem) >
		     btrfs_device_total_bytes(leaf, ditem))) {
		dev_item_err(leaf, slot,
			     "invalid bytes used: have %llu expect [0, %llu]",
			     btrfs_device_bytes_used(leaf, ditem),
			     btrfs_device_total_bytes(leaf, ditem));
		return -EUCLEAN;
	}
	 
	return 0;
}

static int check_inode_item(struct extent_buffer *leaf,
			    struct btrfs_key *key, int slot)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_inode_item *iitem;
	u64 super_gen = btrfs_super_generation(fs_info->super_copy);
	u32 valid_mask = (S_IFMT | S_ISUID | S_ISGID | S_ISVTX | 0777);
	const u32 item_size = btrfs_item_size(leaf, slot);
	u32 mode;
	int ret;
	u32 flags;
	u32 ro_flags;

	ret = check_inode_key(leaf, key, slot);
	if (unlikely(ret < 0))
		return ret;

	if (unlikely(item_size != sizeof(*iitem))) {
		generic_err(leaf, slot, "invalid item size: has %u expect %zu",
			    item_size, sizeof(*iitem));
		return -EUCLEAN;
	}

	iitem = btrfs_item_ptr(leaf, slot, struct btrfs_inode_item);

	 
	if (unlikely(btrfs_inode_generation(leaf, iitem) > super_gen + 1)) {
		inode_item_err(leaf, slot,
			"invalid inode generation: has %llu expect (0, %llu]",
			       btrfs_inode_generation(leaf, iitem),
			       super_gen + 1);
		return -EUCLEAN;
	}
	 
	if (unlikely(btrfs_inode_transid(leaf, iitem) > super_gen + 1)) {
		inode_item_err(leaf, slot,
			"invalid inode transid: has %llu expect [0, %llu]",
			       btrfs_inode_transid(leaf, iitem), super_gen + 1);
		return -EUCLEAN;
	}

	 
	mode = btrfs_inode_mode(leaf, iitem);
	if (unlikely(mode & ~valid_mask)) {
		inode_item_err(leaf, slot,
			       "unknown mode bit detected: 0x%x",
			       mode & ~valid_mask);
		return -EUCLEAN;
	}

	 
	if (!has_single_bit_set(mode & S_IFMT)) {
		if (unlikely(!S_ISLNK(mode) && !S_ISBLK(mode) && !S_ISSOCK(mode))) {
			inode_item_err(leaf, slot,
			"invalid mode: has 0%o expect valid S_IF* bit(s)",
				       mode & S_IFMT);
			return -EUCLEAN;
		}
	}
	if (unlikely(S_ISDIR(mode) && btrfs_inode_nlink(leaf, iitem) > 1)) {
		inode_item_err(leaf, slot,
		       "invalid nlink: has %u expect no more than 1 for dir",
			btrfs_inode_nlink(leaf, iitem));
		return -EUCLEAN;
	}
	btrfs_inode_split_flags(btrfs_inode_flags(leaf, iitem), &flags, &ro_flags);
	if (unlikely(flags & ~BTRFS_INODE_FLAG_MASK)) {
		inode_item_err(leaf, slot,
			       "unknown incompat flags detected: 0x%x", flags);
		return -EUCLEAN;
	}
	if (unlikely(!sb_rdonly(fs_info->sb) &&
		     (ro_flags & ~BTRFS_INODE_RO_FLAG_MASK))) {
		inode_item_err(leaf, slot,
			"unknown ro-compat flags detected on writeable mount: 0x%x",
			ro_flags);
		return -EUCLEAN;
	}
	return 0;
}

static int check_root_item(struct extent_buffer *leaf, struct btrfs_key *key,
			   int slot)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_root_item ri = { 0 };
	const u64 valid_root_flags = BTRFS_ROOT_SUBVOL_RDONLY |
				     BTRFS_ROOT_SUBVOL_DEAD;
	int ret;

	ret = check_root_key(leaf, key, slot);
	if (unlikely(ret < 0))
		return ret;

	if (unlikely(btrfs_item_size(leaf, slot) != sizeof(ri) &&
		     btrfs_item_size(leaf, slot) !=
		     btrfs_legacy_root_item_size())) {
		generic_err(leaf, slot,
			    "invalid root item size, have %u expect %zu or %u",
			    btrfs_item_size(leaf, slot), sizeof(ri),
			    btrfs_legacy_root_item_size());
		return -EUCLEAN;
	}

	 
	read_extent_buffer(leaf, &ri, btrfs_item_ptr_offset(leaf, slot),
			   btrfs_item_size(leaf, slot));

	 
	if (unlikely(btrfs_root_generation(&ri) >
		     btrfs_super_generation(fs_info->super_copy) + 1)) {
		generic_err(leaf, slot,
			"invalid root generation, have %llu expect (0, %llu]",
			    btrfs_root_generation(&ri),
			    btrfs_super_generation(fs_info->super_copy) + 1);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_root_generation_v2(&ri) >
		     btrfs_super_generation(fs_info->super_copy) + 1)) {
		generic_err(leaf, slot,
		"invalid root v2 generation, have %llu expect (0, %llu]",
			    btrfs_root_generation_v2(&ri),
			    btrfs_super_generation(fs_info->super_copy) + 1);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_root_last_snapshot(&ri) >
		     btrfs_super_generation(fs_info->super_copy) + 1)) {
		generic_err(leaf, slot,
		"invalid root last_snapshot, have %llu expect (0, %llu]",
			    btrfs_root_last_snapshot(&ri),
			    btrfs_super_generation(fs_info->super_copy) + 1);
		return -EUCLEAN;
	}

	 
	if (unlikely(!IS_ALIGNED(btrfs_root_bytenr(&ri), fs_info->sectorsize))) {
		generic_err(leaf, slot,
		"invalid root bytenr, have %llu expect to be aligned to %u",
			    btrfs_root_bytenr(&ri), fs_info->sectorsize);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_root_level(&ri) >= BTRFS_MAX_LEVEL)) {
		generic_err(leaf, slot,
			    "invalid root level, have %u expect [0, %u]",
			    btrfs_root_level(&ri), BTRFS_MAX_LEVEL - 1);
		return -EUCLEAN;
	}
	if (unlikely(btrfs_root_drop_level(&ri) >= BTRFS_MAX_LEVEL)) {
		generic_err(leaf, slot,
			    "invalid root level, have %u expect [0, %u]",
			    btrfs_root_drop_level(&ri), BTRFS_MAX_LEVEL - 1);
		return -EUCLEAN;
	}

	 
	if (unlikely(btrfs_root_flags(&ri) & ~valid_root_flags)) {
		generic_err(leaf, slot,
			    "invalid root flags, have 0x%llx expect mask 0x%llx",
			    btrfs_root_flags(&ri), valid_root_flags);
		return -EUCLEAN;
	}
	return 0;
}

__printf(3,4)
__cold
static void extent_err(const struct extent_buffer *eb, int slot,
		       const char *fmt, ...)
{
	struct btrfs_key key;
	struct va_format vaf;
	va_list args;
	u64 bytenr;
	u64 len;

	btrfs_item_key_to_cpu(eb, &key, slot);
	bytenr = key.objectid;
	if (key.type == BTRFS_METADATA_ITEM_KEY ||
	    key.type == BTRFS_TREE_BLOCK_REF_KEY ||
	    key.type == BTRFS_SHARED_BLOCK_REF_KEY)
		len = eb->fs_info->nodesize;
	else
		len = key.offset;
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	btrfs_crit(eb->fs_info,
	"corrupt %s: block=%llu slot=%d extent bytenr=%llu len=%llu %pV",
		btrfs_header_level(eb) == 0 ? "leaf" : "node",
		eb->start, slot, bytenr, len, &vaf);
	va_end(args);
}

static int check_extent_item(struct extent_buffer *leaf,
			     struct btrfs_key *key, int slot,
			     struct btrfs_key *prev_key)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_extent_item *ei;
	bool is_tree_block = false;
	unsigned long ptr;	 
	unsigned long end;	 
	const u32 item_size = btrfs_item_size(leaf, slot);
	u64 flags;
	u64 generation;
	u64 total_refs;		 
	u64 inline_refs = 0;	 

	if (unlikely(key->type == BTRFS_METADATA_ITEM_KEY &&
		     !btrfs_fs_incompat(fs_info, SKINNY_METADATA))) {
		generic_err(leaf, slot,
"invalid key type, METADATA_ITEM type invalid when SKINNY_METADATA feature disabled");
		return -EUCLEAN;
	}
	 
	if (unlikely(!IS_ALIGNED(key->objectid, fs_info->sectorsize))) {
		generic_err(leaf, slot,
		"invalid key objectid, have %llu expect to be aligned to %u",
			   key->objectid, fs_info->sectorsize);
		return -EUCLEAN;
	}

	 
	if (unlikely(key->type == BTRFS_METADATA_ITEM_KEY &&
		     key->offset >= BTRFS_MAX_LEVEL)) {
		extent_err(leaf, slot,
			   "invalid tree level, have %llu expect [0, %u]",
			   key->offset, BTRFS_MAX_LEVEL - 1);
		return -EUCLEAN;
	}

	 
	if (unlikely(item_size < sizeof(*ei))) {
		extent_err(leaf, slot,
			   "invalid item size, have %u expect [%zu, %u)",
			   item_size, sizeof(*ei),
			   BTRFS_LEAF_DATA_SIZE(fs_info));
		return -EUCLEAN;
	}
	end = item_size + btrfs_item_ptr_offset(leaf, slot);

	 
	ei = btrfs_item_ptr(leaf, slot, struct btrfs_extent_item);
	flags = btrfs_extent_flags(leaf, ei);
	total_refs = btrfs_extent_refs(leaf, ei);
	generation = btrfs_extent_generation(leaf, ei);
	if (unlikely(generation >
		     btrfs_super_generation(fs_info->super_copy) + 1)) {
		extent_err(leaf, slot,
			   "invalid generation, have %llu expect (0, %llu]",
			   generation,
			   btrfs_super_generation(fs_info->super_copy) + 1);
		return -EUCLEAN;
	}
	if (unlikely(!has_single_bit_set(flags & (BTRFS_EXTENT_FLAG_DATA |
						  BTRFS_EXTENT_FLAG_TREE_BLOCK)))) {
		extent_err(leaf, slot,
		"invalid extent flag, have 0x%llx expect 1 bit set in 0x%llx",
			flags, BTRFS_EXTENT_FLAG_DATA |
			BTRFS_EXTENT_FLAG_TREE_BLOCK);
		return -EUCLEAN;
	}
	is_tree_block = !!(flags & BTRFS_EXTENT_FLAG_TREE_BLOCK);
	if (is_tree_block) {
		if (unlikely(key->type == BTRFS_EXTENT_ITEM_KEY &&
			     key->offset != fs_info->nodesize)) {
			extent_err(leaf, slot,
				   "invalid extent length, have %llu expect %u",
				   key->offset, fs_info->nodesize);
			return -EUCLEAN;
		}
	} else {
		if (unlikely(key->type != BTRFS_EXTENT_ITEM_KEY)) {
			extent_err(leaf, slot,
			"invalid key type, have %u expect %u for data backref",
				   key->type, BTRFS_EXTENT_ITEM_KEY);
			return -EUCLEAN;
		}
		if (unlikely(!IS_ALIGNED(key->offset, fs_info->sectorsize))) {
			extent_err(leaf, slot,
			"invalid extent length, have %llu expect aligned to %u",
				   key->offset, fs_info->sectorsize);
			return -EUCLEAN;
		}
		if (unlikely(flags & BTRFS_BLOCK_FLAG_FULL_BACKREF)) {
			extent_err(leaf, slot,
			"invalid extent flag, data has full backref set");
			return -EUCLEAN;
		}
	}
	ptr = (unsigned long)(struct btrfs_extent_item *)(ei + 1);

	 
	if (is_tree_block && key->type != BTRFS_METADATA_ITEM_KEY) {
		struct btrfs_tree_block_info *info;

		info = (struct btrfs_tree_block_info *)ptr;
		if (unlikely(btrfs_tree_block_level(leaf, info) >= BTRFS_MAX_LEVEL)) {
			extent_err(leaf, slot,
			"invalid tree block info level, have %u expect [0, %u]",
				   btrfs_tree_block_level(leaf, info),
				   BTRFS_MAX_LEVEL - 1);
			return -EUCLEAN;
		}
		ptr = (unsigned long)(struct btrfs_tree_block_info *)(info + 1);
	}

	 
	while (ptr < end) {
		struct btrfs_extent_inline_ref *iref;
		struct btrfs_extent_data_ref *dref;
		struct btrfs_shared_data_ref *sref;
		u64 dref_offset;
		u64 inline_offset;
		u8 inline_type;

		if (unlikely(ptr + sizeof(*iref) > end)) {
			extent_err(leaf, slot,
"inline ref item overflows extent item, ptr %lu iref size %zu end %lu",
				   ptr, sizeof(*iref), end);
			return -EUCLEAN;
		}
		iref = (struct btrfs_extent_inline_ref *)ptr;
		inline_type = btrfs_extent_inline_ref_type(leaf, iref);
		inline_offset = btrfs_extent_inline_ref_offset(leaf, iref);
		if (unlikely(ptr + btrfs_extent_inline_ref_size(inline_type) > end)) {
			extent_err(leaf, slot,
"inline ref item overflows extent item, ptr %lu iref size %u end %lu",
				   ptr, inline_type, end);
			return -EUCLEAN;
		}

		switch (inline_type) {
		 
		case BTRFS_TREE_BLOCK_REF_KEY:
			inline_refs++;
			break;
		 
		case BTRFS_SHARED_BLOCK_REF_KEY:
			if (unlikely(!IS_ALIGNED(inline_offset,
						 fs_info->sectorsize))) {
				extent_err(leaf, slot,
		"invalid tree parent bytenr, have %llu expect aligned to %u",
					   inline_offset, fs_info->sectorsize);
				return -EUCLEAN;
			}
			inline_refs++;
			break;
		 
		case BTRFS_EXTENT_DATA_REF_KEY:
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			dref_offset = btrfs_extent_data_ref_offset(leaf, dref);
			if (unlikely(!IS_ALIGNED(dref_offset,
						 fs_info->sectorsize))) {
				extent_err(leaf, slot,
		"invalid data ref offset, have %llu expect aligned to %u",
					   dref_offset, fs_info->sectorsize);
				return -EUCLEAN;
			}
			inline_refs += btrfs_extent_data_ref_count(leaf, dref);
			break;
		 
		case BTRFS_SHARED_DATA_REF_KEY:
			sref = (struct btrfs_shared_data_ref *)(iref + 1);
			if (unlikely(!IS_ALIGNED(inline_offset,
						 fs_info->sectorsize))) {
				extent_err(leaf, slot,
		"invalid data parent bytenr, have %llu expect aligned to %u",
					   inline_offset, fs_info->sectorsize);
				return -EUCLEAN;
			}
			inline_refs += btrfs_shared_data_ref_count(leaf, sref);
			break;
		default:
			extent_err(leaf, slot, "unknown inline ref type: %u",
				   inline_type);
			return -EUCLEAN;
		}
		ptr += btrfs_extent_inline_ref_size(inline_type);
	}
	 
	if (unlikely(ptr != end)) {
		extent_err(leaf, slot,
			   "invalid extent item size, padding bytes found");
		return -EUCLEAN;
	}

	 
	if (unlikely(inline_refs > total_refs)) {
		extent_err(leaf, slot,
			"invalid extent refs, have %llu expect >= inline %llu",
			   total_refs, inline_refs);
		return -EUCLEAN;
	}

	if ((prev_key->type == BTRFS_EXTENT_ITEM_KEY) ||
	    (prev_key->type == BTRFS_METADATA_ITEM_KEY)) {
		u64 prev_end = prev_key->objectid;

		if (prev_key->type == BTRFS_METADATA_ITEM_KEY)
			prev_end += fs_info->nodesize;
		else
			prev_end += prev_key->offset;

		if (unlikely(prev_end > key->objectid)) {
			extent_err(leaf, slot,
	"previous extent [%llu %u %llu] overlaps current extent [%llu %u %llu]",
				   prev_key->objectid, prev_key->type,
				   prev_key->offset, key->objectid, key->type,
				   key->offset);
			return -EUCLEAN;
		}
	}

	return 0;
}

static int check_simple_keyed_refs(struct extent_buffer *leaf,
				   struct btrfs_key *key, int slot)
{
	u32 expect_item_size = 0;

	if (key->type == BTRFS_SHARED_DATA_REF_KEY)
		expect_item_size = sizeof(struct btrfs_shared_data_ref);

	if (unlikely(btrfs_item_size(leaf, slot) != expect_item_size)) {
		generic_err(leaf, slot,
		"invalid item size, have %u expect %u for key type %u",
			    btrfs_item_size(leaf, slot),
			    expect_item_size, key->type);
		return -EUCLEAN;
	}
	if (unlikely(!IS_ALIGNED(key->objectid, leaf->fs_info->sectorsize))) {
		generic_err(leaf, slot,
"invalid key objectid for shared block ref, have %llu expect aligned to %u",
			    key->objectid, leaf->fs_info->sectorsize);
		return -EUCLEAN;
	}
	if (unlikely(key->type != BTRFS_TREE_BLOCK_REF_KEY &&
		     !IS_ALIGNED(key->offset, leaf->fs_info->sectorsize))) {
		extent_err(leaf, slot,
		"invalid tree parent bytenr, have %llu expect aligned to %u",
			   key->offset, leaf->fs_info->sectorsize);
		return -EUCLEAN;
	}
	return 0;
}

static int check_extent_data_ref(struct extent_buffer *leaf,
				 struct btrfs_key *key, int slot)
{
	struct btrfs_extent_data_ref *dref;
	unsigned long ptr = btrfs_item_ptr_offset(leaf, slot);
	const unsigned long end = ptr + btrfs_item_size(leaf, slot);

	if (unlikely(btrfs_item_size(leaf, slot) % sizeof(*dref) != 0)) {
		generic_err(leaf, slot,
	"invalid item size, have %u expect aligned to %zu for key type %u",
			    btrfs_item_size(leaf, slot),
			    sizeof(*dref), key->type);
		return -EUCLEAN;
	}
	if (unlikely(!IS_ALIGNED(key->objectid, leaf->fs_info->sectorsize))) {
		generic_err(leaf, slot,
"invalid key objectid for shared block ref, have %llu expect aligned to %u",
			    key->objectid, leaf->fs_info->sectorsize);
		return -EUCLEAN;
	}
	for (; ptr < end; ptr += sizeof(*dref)) {
		u64 offset;

		 
		dref = (struct btrfs_extent_data_ref *)ptr;
		offset = btrfs_extent_data_ref_offset(leaf, dref);
		if (unlikely(!IS_ALIGNED(offset, leaf->fs_info->sectorsize))) {
			extent_err(leaf, slot,
	"invalid extent data backref offset, have %llu expect aligned to %u",
				   offset, leaf->fs_info->sectorsize);
			return -EUCLEAN;
		}
	}
	return 0;
}

#define inode_ref_err(eb, slot, fmt, args...)			\
	inode_item_err(eb, slot, fmt, ##args)
static int check_inode_ref(struct extent_buffer *leaf,
			   struct btrfs_key *key, struct btrfs_key *prev_key,
			   int slot)
{
	struct btrfs_inode_ref *iref;
	unsigned long ptr;
	unsigned long end;

	if (unlikely(!check_prev_ino(leaf, key, slot, prev_key)))
		return -EUCLEAN;
	 
	if (unlikely(btrfs_item_size(leaf, slot) <= sizeof(*iref))) {
		inode_ref_err(leaf, slot,
			"invalid item size, have %u expect (%zu, %u)",
			btrfs_item_size(leaf, slot),
			sizeof(*iref), BTRFS_LEAF_DATA_SIZE(leaf->fs_info));
		return -EUCLEAN;
	}

	ptr = btrfs_item_ptr_offset(leaf, slot);
	end = ptr + btrfs_item_size(leaf, slot);
	while (ptr < end) {
		u16 namelen;

		if (unlikely(ptr + sizeof(iref) > end)) {
			inode_ref_err(leaf, slot,
			"inode ref overflow, ptr %lu end %lu inode_ref_size %zu",
				ptr, end, sizeof(iref));
			return -EUCLEAN;
		}

		iref = (struct btrfs_inode_ref *)ptr;
		namelen = btrfs_inode_ref_name_len(leaf, iref);
		if (unlikely(ptr + sizeof(*iref) + namelen > end)) {
			inode_ref_err(leaf, slot,
				"inode ref overflow, ptr %lu end %lu namelen %u",
				ptr, end, namelen);
			return -EUCLEAN;
		}

		 
		ptr += sizeof(*iref) + namelen;
	}
	return 0;
}

 
static enum btrfs_tree_block_status check_leaf_item(struct extent_buffer *leaf,
						    struct btrfs_key *key,
						    int slot,
						    struct btrfs_key *prev_key)
{
	int ret = 0;
	struct btrfs_chunk *chunk;

	switch (key->type) {
	case BTRFS_EXTENT_DATA_KEY:
		ret = check_extent_data_item(leaf, key, slot, prev_key);
		break;
	case BTRFS_EXTENT_CSUM_KEY:
		ret = check_csum_item(leaf, key, slot, prev_key);
		break;
	case BTRFS_DIR_ITEM_KEY:
	case BTRFS_DIR_INDEX_KEY:
	case BTRFS_XATTR_ITEM_KEY:
		ret = check_dir_item(leaf, key, prev_key, slot);
		break;
	case BTRFS_INODE_REF_KEY:
		ret = check_inode_ref(leaf, key, prev_key, slot);
		break;
	case BTRFS_BLOCK_GROUP_ITEM_KEY:
		ret = check_block_group_item(leaf, key, slot);
		break;
	case BTRFS_CHUNK_ITEM_KEY:
		chunk = btrfs_item_ptr(leaf, slot, struct btrfs_chunk);
		ret = check_leaf_chunk_item(leaf, chunk, key, slot);
		break;
	case BTRFS_DEV_ITEM_KEY:
		ret = check_dev_item(leaf, key, slot);
		break;
	case BTRFS_INODE_ITEM_KEY:
		ret = check_inode_item(leaf, key, slot);
		break;
	case BTRFS_ROOT_ITEM_KEY:
		ret = check_root_item(leaf, key, slot);
		break;
	case BTRFS_EXTENT_ITEM_KEY:
	case BTRFS_METADATA_ITEM_KEY:
		ret = check_extent_item(leaf, key, slot, prev_key);
		break;
	case BTRFS_TREE_BLOCK_REF_KEY:
	case BTRFS_SHARED_DATA_REF_KEY:
	case BTRFS_SHARED_BLOCK_REF_KEY:
		ret = check_simple_keyed_refs(leaf, key, slot);
		break;
	case BTRFS_EXTENT_DATA_REF_KEY:
		ret = check_extent_data_ref(leaf, key, slot);
		break;
	}

	if (ret)
		return BTRFS_TREE_BLOCK_INVALID_ITEM;
	return BTRFS_TREE_BLOCK_CLEAN;
}

enum btrfs_tree_block_status __btrfs_check_leaf(struct extent_buffer *leaf)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	 
	struct btrfs_key prev_key = {0, 0, 0};
	struct btrfs_key key;
	u32 nritems = btrfs_header_nritems(leaf);
	int slot;

	if (unlikely(btrfs_header_level(leaf) != 0)) {
		generic_err(leaf, 0,
			"invalid level for leaf, have %d expect 0",
			btrfs_header_level(leaf));
		return BTRFS_TREE_BLOCK_INVALID_LEVEL;
	}

	 
	if (nritems == 0 && !btrfs_header_flag(leaf, BTRFS_HEADER_FLAG_RELOC)) {
		u64 owner = btrfs_header_owner(leaf);

		 
		if (unlikely(owner == BTRFS_ROOT_TREE_OBJECTID ||
			     owner == BTRFS_CHUNK_TREE_OBJECTID ||
			     owner == BTRFS_DEV_TREE_OBJECTID ||
			     owner == BTRFS_FS_TREE_OBJECTID ||
			     owner == BTRFS_DATA_RELOC_TREE_OBJECTID)) {
			generic_err(leaf, 0,
			"invalid root, root %llu must never be empty",
				    owner);
			return BTRFS_TREE_BLOCK_INVALID_NRITEMS;
		}

		 
		if (unlikely(owner == 0)) {
			generic_err(leaf, 0,
				"invalid owner, root 0 is not defined");
			return BTRFS_TREE_BLOCK_INVALID_OWNER;
		}

		 
		if (btrfs_fs_incompat(fs_info, EXTENT_TREE_V2))
			return BTRFS_TREE_BLOCK_CLEAN;

		if (unlikely(owner == BTRFS_EXTENT_TREE_OBJECTID)) {
			generic_err(leaf, 0,
			"invalid root, root %llu must never be empty",
				    owner);
			return BTRFS_TREE_BLOCK_INVALID_NRITEMS;
		}

		return BTRFS_TREE_BLOCK_CLEAN;
	}

	if (unlikely(nritems == 0))
		return BTRFS_TREE_BLOCK_CLEAN;

	 
	for (slot = 0; slot < nritems; slot++) {
		u32 item_end_expected;
		u64 item_data_end;

		btrfs_item_key_to_cpu(leaf, &key, slot);

		 
		if (unlikely(btrfs_comp_cpu_keys(&prev_key, &key) >= 0)) {
			generic_err(leaf, slot,
	"bad key order, prev (%llu %u %llu) current (%llu %u %llu)",
				prev_key.objectid, prev_key.type,
				prev_key.offset, key.objectid, key.type,
				key.offset);
			return BTRFS_TREE_BLOCK_BAD_KEY_ORDER;
		}

		item_data_end = (u64)btrfs_item_offset(leaf, slot) +
				btrfs_item_size(leaf, slot);
		 
		if (slot == 0)
			item_end_expected = BTRFS_LEAF_DATA_SIZE(fs_info);
		else
			item_end_expected = btrfs_item_offset(leaf,
								 slot - 1);
		if (unlikely(item_data_end != item_end_expected)) {
			generic_err(leaf, slot,
				"unexpected item end, have %llu expect %u",
				item_data_end, item_end_expected);
			return BTRFS_TREE_BLOCK_INVALID_OFFSETS;
		}

		 
		if (unlikely(item_data_end > BTRFS_LEAF_DATA_SIZE(fs_info))) {
			generic_err(leaf, slot,
			"slot end outside of leaf, have %llu expect range [0, %u]",
				item_data_end, BTRFS_LEAF_DATA_SIZE(fs_info));
			return BTRFS_TREE_BLOCK_INVALID_OFFSETS;
		}

		 
		if (unlikely(btrfs_item_ptr_offset(leaf, slot) <
			     btrfs_item_nr_offset(leaf, slot) + sizeof(struct btrfs_item))) {
			generic_err(leaf, slot,
		"slot overlaps with its data, item end %lu data start %lu",
				btrfs_item_nr_offset(leaf, slot) +
				sizeof(struct btrfs_item),
				btrfs_item_ptr_offset(leaf, slot));
			return BTRFS_TREE_BLOCK_INVALID_OFFSETS;
		}

		 
		if (btrfs_header_flag(leaf, BTRFS_HEADER_FLAG_WRITTEN)) {
			enum btrfs_tree_block_status ret;

			 
			ret = check_leaf_item(leaf, &key, slot, &prev_key);
			if (unlikely(ret != BTRFS_TREE_BLOCK_CLEAN))
				return ret;
		}

		prev_key.objectid = key.objectid;
		prev_key.type = key.type;
		prev_key.offset = key.offset;
	}

	return BTRFS_TREE_BLOCK_CLEAN;
}

int btrfs_check_leaf(struct extent_buffer *leaf)
{
	enum btrfs_tree_block_status ret;

	ret = __btrfs_check_leaf(leaf);
	if (unlikely(ret != BTRFS_TREE_BLOCK_CLEAN))
		return -EUCLEAN;
	return 0;
}
ALLOW_ERROR_INJECTION(btrfs_check_leaf, ERRNO);

enum btrfs_tree_block_status __btrfs_check_node(struct extent_buffer *node)
{
	struct btrfs_fs_info *fs_info = node->fs_info;
	unsigned long nr = btrfs_header_nritems(node);
	struct btrfs_key key, next_key;
	int slot;
	int level = btrfs_header_level(node);
	u64 bytenr;

	if (unlikely(level <= 0 || level >= BTRFS_MAX_LEVEL)) {
		generic_err(node, 0,
			"invalid level for node, have %d expect [1, %d]",
			level, BTRFS_MAX_LEVEL - 1);
		return BTRFS_TREE_BLOCK_INVALID_LEVEL;
	}
	if (unlikely(nr == 0 || nr > BTRFS_NODEPTRS_PER_BLOCK(fs_info))) {
		btrfs_crit(fs_info,
"corrupt node: root=%llu block=%llu, nritems too %s, have %lu expect range [1,%u]",
			   btrfs_header_owner(node), node->start,
			   nr == 0 ? "small" : "large", nr,
			   BTRFS_NODEPTRS_PER_BLOCK(fs_info));
		return BTRFS_TREE_BLOCK_INVALID_NRITEMS;
	}

	for (slot = 0; slot < nr - 1; slot++) {
		bytenr = btrfs_node_blockptr(node, slot);
		btrfs_node_key_to_cpu(node, &key, slot);
		btrfs_node_key_to_cpu(node, &next_key, slot + 1);

		if (unlikely(!bytenr)) {
			generic_err(node, slot,
				"invalid NULL node pointer");
			return BTRFS_TREE_BLOCK_INVALID_BLOCKPTR;
		}
		if (unlikely(!IS_ALIGNED(bytenr, fs_info->sectorsize))) {
			generic_err(node, slot,
			"unaligned pointer, have %llu should be aligned to %u",
				bytenr, fs_info->sectorsize);
			return BTRFS_TREE_BLOCK_INVALID_BLOCKPTR;
		}

		if (unlikely(btrfs_comp_cpu_keys(&key, &next_key) >= 0)) {
			generic_err(node, slot,
	"bad key order, current (%llu %u %llu) next (%llu %u %llu)",
				key.objectid, key.type, key.offset,
				next_key.objectid, next_key.type,
				next_key.offset);
			return BTRFS_TREE_BLOCK_BAD_KEY_ORDER;
		}
	}
	return BTRFS_TREE_BLOCK_CLEAN;
}

int btrfs_check_node(struct extent_buffer *node)
{
	enum btrfs_tree_block_status ret;

	ret = __btrfs_check_node(node);
	if (unlikely(ret != BTRFS_TREE_BLOCK_CLEAN))
		return -EUCLEAN;
	return 0;
}
ALLOW_ERROR_INJECTION(btrfs_check_node, ERRNO);

int btrfs_check_eb_owner(const struct extent_buffer *eb, u64 root_owner)
{
	const bool is_subvol = is_fstree(root_owner);
	const u64 eb_owner = btrfs_header_owner(eb);

	 
	if (test_bit(BTRFS_FS_STATE_DUMMY_FS_INFO, &eb->fs_info->fs_state))
		return 0;
	 
	if (root_owner == 0)
		return 0;
	 
	if (root_owner == BTRFS_TREE_LOG_OBJECTID ||
	    root_owner == BTRFS_TREE_RELOC_OBJECTID)
		return 0;

	if (!is_subvol) {
		 
		if (unlikely(root_owner != eb_owner)) {
			btrfs_crit(eb->fs_info,
"corrupted %s, root=%llu block=%llu owner mismatch, have %llu expect %llu",
				btrfs_header_level(eb) == 0 ? "leaf" : "node",
				root_owner, btrfs_header_bytenr(eb), eb_owner,
				root_owner);
			return -EUCLEAN;
		}
		return 0;
	}

	 
	if (unlikely(is_subvol != is_fstree(eb_owner))) {
		btrfs_crit(eb->fs_info,
"corrupted %s, root=%llu block=%llu owner mismatch, have %llu expect [%llu, %llu]",
			btrfs_header_level(eb) == 0 ? "leaf" : "node",
			root_owner, btrfs_header_bytenr(eb), eb_owner,
			BTRFS_FIRST_FREE_OBJECTID, BTRFS_LAST_FREE_OBJECTID);
		return -EUCLEAN;
	}
	return 0;
}

int btrfs_verify_level_key(struct extent_buffer *eb, int level,
			   struct btrfs_key *first_key, u64 parent_transid)
{
	struct btrfs_fs_info *fs_info = eb->fs_info;
	int found_level;
	struct btrfs_key found_key;
	int ret;

	found_level = btrfs_header_level(eb);
	if (found_level != level) {
		WARN(IS_ENABLED(CONFIG_BTRFS_DEBUG),
		     KERN_ERR "BTRFS: tree level check failed\n");
		btrfs_err(fs_info,
"tree level mismatch detected, bytenr=%llu level expected=%u has=%u",
			  eb->start, level, found_level);
		return -EIO;
	}

	if (!first_key)
		return 0;

	 
	if (btrfs_header_generation(eb) > fs_info->last_trans_committed)
		return 0;

	 
	if (btrfs_header_nritems(eb) == 0) {
		btrfs_err(fs_info,
		"invalid tree nritems, bytenr=%llu nritems=0 expect >0",
			  eb->start);
		WARN_ON(IS_ENABLED(CONFIG_BTRFS_DEBUG));
		return -EUCLEAN;
	}

	if (found_level)
		btrfs_node_key_to_cpu(eb, &found_key, 0);
	else
		btrfs_item_key_to_cpu(eb, &found_key, 0);
	ret = btrfs_comp_cpu_keys(first_key, &found_key);

	if (ret) {
		WARN(IS_ENABLED(CONFIG_BTRFS_DEBUG),
		     KERN_ERR "BTRFS: tree first key check failed\n");
		btrfs_err(fs_info,
"tree first key mismatch detected, bytenr=%llu parent_transid=%llu key expected=(%llu,%u,%llu) has=(%llu,%u,%llu)",
			  eb->start, parent_transid, first_key->objectid,
			  first_key->type, first_key->offset,
			  found_key.objectid, found_key.type,
			  found_key.offset);
	}
	return ret;
}
