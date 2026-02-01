
 

#include <crypto/hash.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/blk-cgroup.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/writeback.h>
#include <linux/compat.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/falloc.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/btrfs.h>
#include <linux/blkdev.h>
#include <linux/posix_acl_xattr.h>
#include <linux/uio.h>
#include <linux/magic.h>
#include <linux/iversion.h>
#include <linux/swap.h>
#include <linux/migrate.h>
#include <linux/sched/mm.h>
#include <linux/iomap.h>
#include <asm/unaligned.h>
#include <linux/fsverity.h>
#include "misc.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "print-tree.h"
#include "ordered-data.h"
#include "xattr.h"
#include "tree-log.h"
#include "bio.h"
#include "compression.h"
#include "locking.h"
#include "free-space-cache.h"
#include "props.h"
#include "qgroup.h"
#include "delalloc-space.h"
#include "block-group.h"
#include "space-info.h"
#include "zoned.h"
#include "subpage.h"
#include "inode-item.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"
#include "root-tree.h"
#include "defrag.h"
#include "dir-item.h"
#include "file-item.h"
#include "uuid-tree.h"
#include "ioctl.h"
#include "file.h"
#include "acl.h"
#include "relocation.h"
#include "verity.h"
#include "super.h"
#include "orphan.h"
#include "backref.h"

struct btrfs_iget_args {
	u64 ino;
	struct btrfs_root *root;
};

struct btrfs_dio_data {
	ssize_t submitted;
	struct extent_changeset *data_reserved;
	struct btrfs_ordered_extent *ordered;
	bool data_space_reserved;
	bool nocow_done;
};

struct btrfs_dio_private {
	 
	u64 file_offset;
	u32 bytes;

	 
	struct btrfs_bio bbio;
};

static struct bio_set btrfs_dio_bioset;

struct btrfs_rename_ctx {
	 
	u64 index;
};

 
struct data_reloc_warn {
	struct btrfs_path path;
	struct btrfs_fs_info *fs_info;
	u64 extent_item_size;
	u64 logical;
	int mirror_num;
};

static const struct inode_operations btrfs_dir_inode_operations;
static const struct inode_operations btrfs_symlink_inode_operations;
static const struct inode_operations btrfs_special_inode_operations;
static const struct inode_operations btrfs_file_inode_operations;
static const struct address_space_operations btrfs_aops;
static const struct file_operations btrfs_dir_file_operations;

static struct kmem_cache *btrfs_inode_cachep;

static int btrfs_setsize(struct inode *inode, struct iattr *attr);
static int btrfs_truncate(struct btrfs_inode *inode, bool skip_writeback);

static noinline int run_delalloc_cow(struct btrfs_inode *inode,
				     struct page *locked_page, u64 start,
				     u64 end, struct writeback_control *wbc,
				     bool pages_dirty);
static struct extent_map *create_io_em(struct btrfs_inode *inode, u64 start,
				       u64 len, u64 orig_start, u64 block_start,
				       u64 block_len, u64 orig_block_len,
				       u64 ram_bytes, int compress_type,
				       int type);

static int data_reloc_print_warning_inode(u64 inum, u64 offset, u64 num_bytes,
					  u64 root, void *warn_ctx)
{
	struct data_reloc_warn *warn = warn_ctx;
	struct btrfs_fs_info *fs_info = warn->fs_info;
	struct extent_buffer *eb;
	struct btrfs_inode_item *inode_item;
	struct inode_fs_paths *ipath = NULL;
	struct btrfs_root *local_root;
	struct btrfs_key key;
	unsigned int nofs_flag;
	u32 nlink;
	int ret;

	local_root = btrfs_get_fs_root(fs_info, root, true);
	if (IS_ERR(local_root)) {
		ret = PTR_ERR(local_root);
		goto err;
	}

	 
	key.objectid = inum;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, local_root, &key, &warn->path, 0, 0);
	if (ret) {
		btrfs_put_root(local_root);
		btrfs_release_path(&warn->path);
		goto err;
	}

	eb = warn->path.nodes[0];
	inode_item = btrfs_item_ptr(eb, warn->path.slots[0], struct btrfs_inode_item);
	nlink = btrfs_inode_nlink(eb, inode_item);
	btrfs_release_path(&warn->path);

	nofs_flag = memalloc_nofs_save();
	ipath = init_ipath(4096, local_root, &warn->path);
	memalloc_nofs_restore(nofs_flag);
	if (IS_ERR(ipath)) {
		btrfs_put_root(local_root);
		ret = PTR_ERR(ipath);
		ipath = NULL;
		 
		btrfs_warn(fs_info,
"checksum error at logical %llu mirror %u root %llu, inode %llu offset %llu",
			   warn->logical, warn->mirror_num, root, inum, offset);
		return ret;
	}
	ret = paths_from_inode(inum, ipath);
	if (ret < 0)
		goto err;

	 
	for (int i = 0; i < ipath->fspath->elem_cnt; i++) {
		btrfs_warn(fs_info,
"checksum error at logical %llu mirror %u root %llu inode %llu offset %llu length %u links %u (path: %s)",
			   warn->logical, warn->mirror_num, root, inum, offset,
			   fs_info->sectorsize, nlink,
			   (char *)(unsigned long)ipath->fspath->val[i]);
	}

	btrfs_put_root(local_root);
	free_ipath(ipath);
	return 0;

err:
	btrfs_warn(fs_info,
"checksum error at logical %llu mirror %u root %llu inode %llu offset %llu, path resolving failed with ret=%d",
		   warn->logical, warn->mirror_num, root, inum, offset, ret);

	free_ipath(ipath);
	return ret;
}

 
static void print_data_reloc_error(const struct btrfs_inode *inode, u64 file_off,
				   const u8 *csum, const u8 *csum_expected,
				   int mirror_num)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct btrfs_path path = { 0 };
	struct btrfs_key found_key = { 0 };
	struct extent_buffer *eb;
	struct btrfs_extent_item *ei;
	const u32 csum_size = fs_info->csum_size;
	u64 logical;
	u64 flags;
	u32 item_size;
	int ret;

	mutex_lock(&fs_info->reloc_mutex);
	logical = btrfs_get_reloc_bg_bytenr(fs_info);
	mutex_unlock(&fs_info->reloc_mutex);

	if (logical == U64_MAX) {
		btrfs_warn_rl(fs_info, "has data reloc tree but no running relocation");
		btrfs_warn_rl(fs_info,
"csum failed root %lld ino %llu off %llu csum " CSUM_FMT " expected csum " CSUM_FMT " mirror %d",
			inode->root->root_key.objectid, btrfs_ino(inode), file_off,
			CSUM_FMT_VALUE(csum_size, csum),
			CSUM_FMT_VALUE(csum_size, csum_expected),
			mirror_num);
		return;
	}

	logical += file_off;
	btrfs_warn_rl(fs_info,
"csum failed root %lld ino %llu off %llu logical %llu csum " CSUM_FMT " expected csum " CSUM_FMT " mirror %d",
			inode->root->root_key.objectid,
			btrfs_ino(inode), file_off, logical,
			CSUM_FMT_VALUE(csum_size, csum),
			CSUM_FMT_VALUE(csum_size, csum_expected),
			mirror_num);

	ret = extent_from_logical(fs_info, logical, &path, &found_key, &flags);
	if (ret < 0) {
		btrfs_err_rl(fs_info, "failed to lookup extent item for logical %llu: %d",
			     logical, ret);
		return;
	}
	eb = path.nodes[0];
	ei = btrfs_item_ptr(eb, path.slots[0], struct btrfs_extent_item);
	item_size = btrfs_item_size(eb, path.slots[0]);
	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		unsigned long ptr = 0;
		u64 ref_root;
		u8 ref_level;

		while (true) {
			ret = tree_backref_for_extent(&ptr, eb, &found_key, ei,
						      item_size, &ref_root,
						      &ref_level);
			if (ret < 0) {
				btrfs_warn_rl(fs_info,
				"failed to resolve tree backref for logical %llu: %d",
					      logical, ret);
				break;
			}
			if (ret > 0)
				break;

			btrfs_warn_rl(fs_info,
"csum error at logical %llu mirror %u: metadata %s (level %d) in tree %llu",
				logical, mirror_num,
				(ref_level ? "node" : "leaf"),
				ref_level, ref_root);
		}
		btrfs_release_path(&path);
	} else {
		struct btrfs_backref_walk_ctx ctx = { 0 };
		struct data_reloc_warn reloc_warn = { 0 };

		btrfs_release_path(&path);

		ctx.bytenr = found_key.objectid;
		ctx.extent_item_pos = logical - found_key.objectid;
		ctx.fs_info = fs_info;

		reloc_warn.logical = logical;
		reloc_warn.extent_item_size = found_key.offset;
		reloc_warn.mirror_num = mirror_num;
		reloc_warn.fs_info = fs_info;

		iterate_extent_inodes(&ctx, true,
				      data_reloc_print_warning_inode, &reloc_warn);
	}
}

static void __cold btrfs_print_data_csum_error(struct btrfs_inode *inode,
		u64 logical_start, u8 *csum, u8 *csum_expected, int mirror_num)
{
	struct btrfs_root *root = inode->root;
	const u32 csum_size = root->fs_info->csum_size;

	 
	if (root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID)
		return print_data_reloc_error(inode, logical_start, csum,
					      csum_expected, mirror_num);

	 
	if (root->root_key.objectid >= BTRFS_LAST_FREE_OBJECTID) {
		btrfs_warn_rl(root->fs_info,
"csum failed root %lld ino %lld off %llu csum " CSUM_FMT " expected csum " CSUM_FMT " mirror %d",
			root->root_key.objectid, btrfs_ino(inode),
			logical_start,
			CSUM_FMT_VALUE(csum_size, csum),
			CSUM_FMT_VALUE(csum_size, csum_expected),
			mirror_num);
	} else {
		btrfs_warn_rl(root->fs_info,
"csum failed root %llu ino %llu off %llu csum " CSUM_FMT " expected csum " CSUM_FMT " mirror %d",
			root->root_key.objectid, btrfs_ino(inode),
			logical_start,
			CSUM_FMT_VALUE(csum_size, csum),
			CSUM_FMT_VALUE(csum_size, csum_expected),
			mirror_num);
	}
}

 
int btrfs_inode_lock(struct btrfs_inode *inode, unsigned int ilock_flags)
{
	if (ilock_flags & BTRFS_ILOCK_SHARED) {
		if (ilock_flags & BTRFS_ILOCK_TRY) {
			if (!inode_trylock_shared(&inode->vfs_inode))
				return -EAGAIN;
			else
				return 0;
		}
		inode_lock_shared(&inode->vfs_inode);
	} else {
		if (ilock_flags & BTRFS_ILOCK_TRY) {
			if (!inode_trylock(&inode->vfs_inode))
				return -EAGAIN;
			else
				return 0;
		}
		inode_lock(&inode->vfs_inode);
	}
	if (ilock_flags & BTRFS_ILOCK_MMAP)
		down_write(&inode->i_mmap_lock);
	return 0;
}

 
void btrfs_inode_unlock(struct btrfs_inode *inode, unsigned int ilock_flags)
{
	if (ilock_flags & BTRFS_ILOCK_MMAP)
		up_write(&inode->i_mmap_lock);
	if (ilock_flags & BTRFS_ILOCK_SHARED)
		inode_unlock_shared(&inode->vfs_inode);
	else
		inode_unlock(&inode->vfs_inode);
}

 
static inline void btrfs_cleanup_ordered_extents(struct btrfs_inode *inode,
						 struct page *locked_page,
						 u64 offset, u64 bytes)
{
	unsigned long index = offset >> PAGE_SHIFT;
	unsigned long end_index = (offset + bytes - 1) >> PAGE_SHIFT;
	u64 page_start = 0, page_end = 0;
	struct page *page;

	if (locked_page) {
		page_start = page_offset(locked_page);
		page_end = page_start + PAGE_SIZE - 1;
	}

	while (index <= end_index) {
		 
		if (locked_page && index == (page_start >> PAGE_SHIFT)) {
			index++;
			continue;
		}
		page = find_get_page(inode->vfs_inode.i_mapping, index);
		index++;
		if (!page)
			continue;

		 
		btrfs_page_clamp_clear_ordered(inode->root->fs_info, page,
					       offset, bytes);
		put_page(page);
	}

	if (locked_page) {
		 
		if (bytes + offset <= page_start + PAGE_SIZE)
			return;
		 
		if (page_start >= offset && page_end <= (offset + bytes - 1)) {
			bytes = offset + bytes - page_offset(locked_page) - PAGE_SIZE;
			offset = page_offset(locked_page) + PAGE_SIZE;
		}
	}

	return btrfs_mark_ordered_io_finished(inode, NULL, offset, bytes, false);
}

static int btrfs_dirty_inode(struct btrfs_inode *inode);

static int btrfs_init_inode_security(struct btrfs_trans_handle *trans,
				     struct btrfs_new_inode_args *args)
{
	int err;

	if (args->default_acl) {
		err = __btrfs_set_acl(trans, args->inode, args->default_acl,
				      ACL_TYPE_DEFAULT);
		if (err)
			return err;
	}
	if (args->acl) {
		err = __btrfs_set_acl(trans, args->inode, args->acl, ACL_TYPE_ACCESS);
		if (err)
			return err;
	}
	if (!args->default_acl && !args->acl)
		cache_no_acl(args->inode);
	return btrfs_xattr_security_init(trans, args->inode, args->dir,
					 &args->dentry->d_name);
}

 
static int insert_inline_extent(struct btrfs_trans_handle *trans,
				struct btrfs_path *path,
				struct btrfs_inode *inode, bool extent_inserted,
				size_t size, size_t compressed_size,
				int compress_type,
				struct page **compressed_pages,
				bool update_i_size)
{
	struct btrfs_root *root = inode->root;
	struct extent_buffer *leaf;
	struct page *page = NULL;
	char *kaddr;
	unsigned long ptr;
	struct btrfs_file_extent_item *ei;
	int ret;
	size_t cur_size = size;
	u64 i_size;

	ASSERT((compressed_size > 0 && compressed_pages) ||
	       (compressed_size == 0 && !compressed_pages));

	if (compressed_size && compressed_pages)
		cur_size = compressed_size;

	if (!extent_inserted) {
		struct btrfs_key key;
		size_t datasize;

		key.objectid = btrfs_ino(inode);
		key.offset = 0;
		key.type = BTRFS_EXTENT_DATA_KEY;

		datasize = btrfs_file_extent_calc_inline_size(cur_size);
		ret = btrfs_insert_empty_item(trans, root, path, &key,
					      datasize);
		if (ret)
			goto fail;
	}
	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, ei, trans->transid);
	btrfs_set_file_extent_type(leaf, ei, BTRFS_FILE_EXTENT_INLINE);
	btrfs_set_file_extent_encryption(leaf, ei, 0);
	btrfs_set_file_extent_other_encoding(leaf, ei, 0);
	btrfs_set_file_extent_ram_bytes(leaf, ei, size);
	ptr = btrfs_file_extent_inline_start(ei);

	if (compress_type != BTRFS_COMPRESS_NONE) {
		struct page *cpage;
		int i = 0;
		while (compressed_size > 0) {
			cpage = compressed_pages[i];
			cur_size = min_t(unsigned long, compressed_size,
				       PAGE_SIZE);

			kaddr = kmap_local_page(cpage);
			write_extent_buffer(leaf, kaddr, ptr, cur_size);
			kunmap_local(kaddr);

			i++;
			ptr += cur_size;
			compressed_size -= cur_size;
		}
		btrfs_set_file_extent_compression(leaf, ei,
						  compress_type);
	} else {
		page = find_get_page(inode->vfs_inode.i_mapping, 0);
		btrfs_set_file_extent_compression(leaf, ei, 0);
		kaddr = kmap_local_page(page);
		write_extent_buffer(leaf, kaddr, ptr, size);
		kunmap_local(kaddr);
		put_page(page);
	}
	btrfs_mark_buffer_dirty(trans, leaf);
	btrfs_release_path(path);

	 
	ret = btrfs_inode_set_file_extent_range(inode, 0,
					ALIGN(size, root->fs_info->sectorsize));
	if (ret)
		goto fail;

	 
	i_size = i_size_read(&inode->vfs_inode);
	if (update_i_size && size > i_size) {
		i_size_write(&inode->vfs_inode, size);
		i_size = size;
	}
	inode->disk_i_size = i_size;

fail:
	return ret;
}


 
static noinline int cow_file_range_inline(struct btrfs_inode *inode, u64 size,
					  size_t compressed_size,
					  int compress_type,
					  struct page **compressed_pages,
					  bool update_i_size)
{
	struct btrfs_drop_extents_args drop_args = { 0 };
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans;
	u64 data_len = (compressed_size ?: size);
	int ret;
	struct btrfs_path *path;

	 
	if (size < i_size_read(&inode->vfs_inode) ||
	    size > fs_info->sectorsize ||
	    data_len > BTRFS_MAX_INLINE_DATA_SIZE(fs_info) ||
	    data_len > fs_info->max_inline)
		return 1;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans)) {
		btrfs_free_path(path);
		return PTR_ERR(trans);
	}
	trans->block_rsv = &inode->block_rsv;

	drop_args.path = path;
	drop_args.start = 0;
	drop_args.end = fs_info->sectorsize;
	drop_args.drop_cache = true;
	drop_args.replace_extent = true;
	drop_args.extent_item_size = btrfs_file_extent_calc_inline_size(data_len);
	ret = btrfs_drop_extents(trans, root, inode, &drop_args);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}

	ret = insert_inline_extent(trans, path, inode, drop_args.extent_inserted,
				   size, compressed_size, compress_type,
				   compressed_pages, update_i_size);
	if (ret && ret != -ENOSPC) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	} else if (ret == -ENOSPC) {
		ret = 1;
		goto out;
	}

	btrfs_update_inode_bytes(inode, size, drop_args.bytes_found);
	ret = btrfs_update_inode(trans, root, inode);
	if (ret && ret != -ENOSPC) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	} else if (ret == -ENOSPC) {
		ret = 1;
		goto out;
	}

	btrfs_set_inode_full_sync(inode);
out:
	 
	btrfs_qgroup_free_data(inode, NULL, 0, PAGE_SIZE, NULL);
	btrfs_free_path(path);
	btrfs_end_transaction(trans);
	return ret;
}

struct async_extent {
	u64 start;
	u64 ram_size;
	u64 compressed_size;
	struct page **pages;
	unsigned long nr_pages;
	int compress_type;
	struct list_head list;
};

struct async_chunk {
	struct btrfs_inode *inode;
	struct page *locked_page;
	u64 start;
	u64 end;
	blk_opf_t write_flags;
	struct list_head extents;
	struct cgroup_subsys_state *blkcg_css;
	struct btrfs_work work;
	struct async_cow *async_cow;
};

struct async_cow {
	atomic_t num_chunks;
	struct async_chunk chunks[];
};

static noinline int add_async_extent(struct async_chunk *cow,
				     u64 start, u64 ram_size,
				     u64 compressed_size,
				     struct page **pages,
				     unsigned long nr_pages,
				     int compress_type)
{
	struct async_extent *async_extent;

	async_extent = kmalloc(sizeof(*async_extent), GFP_NOFS);
	BUG_ON(!async_extent);  
	async_extent->start = start;
	async_extent->ram_size = ram_size;
	async_extent->compressed_size = compressed_size;
	async_extent->pages = pages;
	async_extent->nr_pages = nr_pages;
	async_extent->compress_type = compress_type;
	list_add_tail(&async_extent->list, &cow->extents);
	return 0;
}

 
static inline int inode_need_compress(struct btrfs_inode *inode, u64 start,
				      u64 end)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;

	if (!btrfs_inode_can_compress(inode)) {
		WARN(IS_ENABLED(CONFIG_BTRFS_DEBUG),
			KERN_ERR "BTRFS: unexpected compression for ino %llu\n",
			btrfs_ino(inode));
		return 0;
	}
	 
	if (fs_info->sectorsize < PAGE_SIZE) {
		if (!PAGE_ALIGNED(start) ||
		    !PAGE_ALIGNED(end + 1))
			return 0;
	}

	 
	if (btrfs_test_opt(fs_info, FORCE_COMPRESS))
		return 1;
	 
	if (inode->defrag_compress)
		return 1;
	 
	if (inode->flags & BTRFS_INODE_NOCOMPRESS)
		return 0;
	if (btrfs_test_opt(fs_info, COMPRESS) ||
	    inode->flags & BTRFS_INODE_COMPRESS ||
	    inode->prop_compress)
		return btrfs_compress_heuristic(&inode->vfs_inode, start, end);
	return 0;
}

static inline void inode_should_defrag(struct btrfs_inode *inode,
		u64 start, u64 end, u64 num_bytes, u32 small_write)
{
	 
	if (num_bytes < small_write &&
	    (start > 0 || end + 1 < inode->disk_i_size))
		btrfs_add_inode_defrag(NULL, inode, small_write);
}

 
static void compress_file_range(struct btrfs_work *work)
{
	struct async_chunk *async_chunk =
		container_of(work, struct async_chunk, work);
	struct btrfs_inode *inode = async_chunk->inode;
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct address_space *mapping = inode->vfs_inode.i_mapping;
	u64 blocksize = fs_info->sectorsize;
	u64 start = async_chunk->start;
	u64 end = async_chunk->end;
	u64 actual_end;
	u64 i_size;
	int ret = 0;
	struct page **pages;
	unsigned long nr_pages;
	unsigned long total_compressed = 0;
	unsigned long total_in = 0;
	unsigned int poff;
	int i;
	int compress_type = fs_info->compress_type;

	inode_should_defrag(inode, start, end, end - start + 1, SZ_16K);

	 
	extent_range_clear_dirty_for_io(&inode->vfs_inode, start, end);

	 
	barrier();
	i_size = i_size_read(&inode->vfs_inode);
	barrier();
	actual_end = min_t(u64, i_size, end + 1);
again:
	pages = NULL;
	nr_pages = (end >> PAGE_SHIFT) - (start >> PAGE_SHIFT) + 1;
	nr_pages = min_t(unsigned long, nr_pages, BTRFS_MAX_COMPRESSED_PAGES);

	 
	if (actual_end <= start)
		goto cleanup_and_bail_uncompressed;

	total_compressed = actual_end - start;

	 
	if (total_compressed <= blocksize &&
	   (start > 0 || end + 1 < inode->disk_i_size))
		goto cleanup_and_bail_uncompressed;

	 
	if (blocksize < PAGE_SIZE) {
		if (!PAGE_ALIGNED(start) ||
		    !PAGE_ALIGNED(round_up(actual_end, blocksize)))
			goto cleanup_and_bail_uncompressed;
	}

	total_compressed = min_t(unsigned long, total_compressed,
			BTRFS_MAX_UNCOMPRESSED);
	total_in = 0;
	ret = 0;

	 
	if (!inode_need_compress(inode, start, end))
		goto cleanup_and_bail_uncompressed;

	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_NOFS);
	if (!pages) {
		 
		goto cleanup_and_bail_uncompressed;
	}

	if (inode->defrag_compress)
		compress_type = inode->defrag_compress;
	else if (inode->prop_compress)
		compress_type = inode->prop_compress;

	 
	ret = btrfs_compress_pages(compress_type | (fs_info->compress_level << 4),
				   mapping, start, pages, &nr_pages, &total_in,
				   &total_compressed);
	if (ret)
		goto mark_incompressible;

	 
	poff = offset_in_page(total_compressed);
	if (poff)
		memzero_page(pages[nr_pages - 1], poff, PAGE_SIZE - poff);

	 
	if (start == 0 && fs_info->sectorsize == PAGE_SIZE) {
		if (total_in < actual_end) {
			ret = cow_file_range_inline(inode, actual_end, 0,
						    BTRFS_COMPRESS_NONE, NULL,
						    false);
		} else {
			ret = cow_file_range_inline(inode, actual_end,
						    total_compressed,
						    compress_type, pages,
						    false);
		}
		if (ret <= 0) {
			unsigned long clear_flags = EXTENT_DELALLOC |
				EXTENT_DELALLOC_NEW | EXTENT_DEFRAG |
				EXTENT_DO_ACCOUNTING;

			if (ret < 0)
				mapping_set_error(mapping, -EIO);

			 
			extent_clear_unlock_delalloc(inode, start, end,
						     NULL,
						     clear_flags,
						     PAGE_UNLOCK |
						     PAGE_START_WRITEBACK |
						     PAGE_END_WRITEBACK);
			goto free_pages;
		}
	}

	 
	total_compressed = ALIGN(total_compressed, blocksize);

	 
	total_in = round_up(total_in, fs_info->sectorsize);
	if (total_compressed + blocksize > total_in)
		goto mark_incompressible;

	 
	add_async_extent(async_chunk, start, total_in, total_compressed, pages,
			 nr_pages, compress_type);
	if (start + total_in < end) {
		start += total_in;
		cond_resched();
		goto again;
	}
	return;

mark_incompressible:
	if (!btrfs_test_opt(fs_info, FORCE_COMPRESS) && !inode->prop_compress)
		inode->flags |= BTRFS_INODE_NOCOMPRESS;
cleanup_and_bail_uncompressed:
	add_async_extent(async_chunk, start, end - start + 1, 0, NULL, 0,
			 BTRFS_COMPRESS_NONE);
free_pages:
	if (pages) {
		for (i = 0; i < nr_pages; i++) {
			WARN_ON(pages[i]->mapping);
			put_page(pages[i]);
		}
		kfree(pages);
	}
}

static void free_async_extent_pages(struct async_extent *async_extent)
{
	int i;

	if (!async_extent->pages)
		return;

	for (i = 0; i < async_extent->nr_pages; i++) {
		WARN_ON(async_extent->pages[i]->mapping);
		put_page(async_extent->pages[i]);
	}
	kfree(async_extent->pages);
	async_extent->nr_pages = 0;
	async_extent->pages = NULL;
}

static void submit_uncompressed_range(struct btrfs_inode *inode,
				      struct async_extent *async_extent,
				      struct page *locked_page)
{
	u64 start = async_extent->start;
	u64 end = async_extent->start + async_extent->ram_size - 1;
	int ret;
	struct writeback_control wbc = {
		.sync_mode		= WB_SYNC_ALL,
		.range_start		= start,
		.range_end		= end,
		.no_cgroup_owner	= 1,
	};

	wbc_attach_fdatawrite_inode(&wbc, &inode->vfs_inode);
	ret = run_delalloc_cow(inode, locked_page, start, end, &wbc, false);
	wbc_detach_inode(&wbc);
	if (ret < 0) {
		btrfs_cleanup_ordered_extents(inode, locked_page, start, end - start + 1);
		if (locked_page) {
			const u64 page_start = page_offset(locked_page);

			set_page_writeback(locked_page);
			end_page_writeback(locked_page);
			btrfs_mark_ordered_io_finished(inode, locked_page,
						       page_start, PAGE_SIZE,
						       !ret);
			mapping_set_error(locked_page->mapping, ret);
			unlock_page(locked_page);
		}
	}
}

static void submit_one_async_extent(struct async_chunk *async_chunk,
				    struct async_extent *async_extent,
				    u64 *alloc_hint)
{
	struct btrfs_inode *inode = async_chunk->inode;
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_ordered_extent *ordered;
	struct btrfs_key ins;
	struct page *locked_page = NULL;
	struct extent_map *em;
	int ret = 0;
	u64 start = async_extent->start;
	u64 end = async_extent->start + async_extent->ram_size - 1;

	if (async_chunk->blkcg_css)
		kthread_associate_blkcg(async_chunk->blkcg_css);

	 
	if (async_chunk->locked_page) {
		u64 locked_page_start = page_offset(async_chunk->locked_page);
		u64 locked_page_end = locked_page_start + PAGE_SIZE - 1;

		if (!(start >= locked_page_end || end <= locked_page_start))
			locked_page = async_chunk->locked_page;
	}
	lock_extent(io_tree, start, end, NULL);

	if (async_extent->compress_type == BTRFS_COMPRESS_NONE) {
		submit_uncompressed_range(inode, async_extent, locked_page);
		goto done;
	}

	ret = btrfs_reserve_extent(root, async_extent->ram_size,
				   async_extent->compressed_size,
				   async_extent->compressed_size,
				   0, *alloc_hint, &ins, 1, 1);
	if (ret) {
		 
		goto out_free;
	}

	 
	em = create_io_em(inode, start,
			  async_extent->ram_size,	 
			  start,			 
			  ins.objectid,			 
			  ins.offset,			 
			  ins.offset,			 
			  async_extent->ram_size,	 
			  async_extent->compress_type,
			  BTRFS_ORDERED_COMPRESSED);
	if (IS_ERR(em)) {
		ret = PTR_ERR(em);
		goto out_free_reserve;
	}
	free_extent_map(em);

	ordered = btrfs_alloc_ordered_extent(inode, start,	 
				       async_extent->ram_size,	 
				       async_extent->ram_size,	 
				       ins.objectid,		 
				       ins.offset,		 
				       0,			 
				       1 << BTRFS_ORDERED_COMPRESSED,
				       async_extent->compress_type);
	if (IS_ERR(ordered)) {
		btrfs_drop_extent_map_range(inode, start, end, false);
		ret = PTR_ERR(ordered);
		goto out_free_reserve;
	}
	btrfs_dec_block_group_reservations(fs_info, ins.objectid);

	 
	extent_clear_unlock_delalloc(inode, start, end,
			NULL, EXTENT_LOCKED | EXTENT_DELALLOC,
			PAGE_UNLOCK | PAGE_START_WRITEBACK);
	btrfs_submit_compressed_write(ordered,
			    async_extent->pages,	 
			    async_extent->nr_pages,
			    async_chunk->write_flags, true);
	*alloc_hint = ins.objectid + ins.offset;
done:
	if (async_chunk->blkcg_css)
		kthread_associate_blkcg(NULL);
	kfree(async_extent);
	return;

out_free_reserve:
	btrfs_dec_block_group_reservations(fs_info, ins.objectid);
	btrfs_free_reserved_extent(fs_info, ins.objectid, ins.offset, 1);
out_free:
	mapping_set_error(inode->vfs_inode.i_mapping, -EIO);
	extent_clear_unlock_delalloc(inode, start, end,
				     NULL, EXTENT_LOCKED | EXTENT_DELALLOC |
				     EXTENT_DELALLOC_NEW |
				     EXTENT_DEFRAG | EXTENT_DO_ACCOUNTING,
				     PAGE_UNLOCK | PAGE_START_WRITEBACK |
				     PAGE_END_WRITEBACK);
	free_async_extent_pages(async_extent);
	if (async_chunk->blkcg_css)
		kthread_associate_blkcg(NULL);
	btrfs_debug(fs_info,
"async extent submission failed root=%lld inode=%llu start=%llu len=%llu ret=%d",
		    root->root_key.objectid, btrfs_ino(inode), start,
		    async_extent->ram_size, ret);
	kfree(async_extent);
}

static u64 get_extent_allocation_hint(struct btrfs_inode *inode, u64 start,
				      u64 num_bytes)
{
	struct extent_map_tree *em_tree = &inode->extent_tree;
	struct extent_map *em;
	u64 alloc_hint = 0;

	read_lock(&em_tree->lock);
	em = search_extent_mapping(em_tree, start, num_bytes);
	if (em) {
		 
		if (em->block_start >= EXTENT_MAP_LAST_BYTE) {
			free_extent_map(em);
			em = search_extent_mapping(em_tree, 0, 0);
			if (em && em->block_start < EXTENT_MAP_LAST_BYTE)
				alloc_hint = em->block_start;
			if (em)
				free_extent_map(em);
		} else {
			alloc_hint = em->block_start;
			free_extent_map(em);
		}
	}
	read_unlock(&em_tree->lock);

	return alloc_hint;
}

 
static noinline int cow_file_range(struct btrfs_inode *inode,
				   struct page *locked_page, u64 start, u64 end,
				   u64 *done_offset,
				   bool keep_locked, bool no_inline)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 alloc_hint = 0;
	u64 orig_start = start;
	u64 num_bytes;
	unsigned long ram_size;
	u64 cur_alloc_size = 0;
	u64 min_alloc_size;
	u64 blocksize = fs_info->sectorsize;
	struct btrfs_key ins;
	struct extent_map *em;
	unsigned clear_bits;
	unsigned long page_ops;
	bool extent_reserved = false;
	int ret = 0;

	if (btrfs_is_free_space_inode(inode)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	num_bytes = ALIGN(end - start + 1, blocksize);
	num_bytes = max(blocksize,  num_bytes);
	ASSERT(num_bytes <= btrfs_super_total_bytes(fs_info->super_copy));

	inode_should_defrag(inode, start, end, num_bytes, SZ_64K);

	 
	if (start == 0 && fs_info->sectorsize == PAGE_SIZE && !no_inline) {
		u64 actual_end = min_t(u64, i_size_read(&inode->vfs_inode),
				       end + 1);

		 
		ret = cow_file_range_inline(inode, actual_end, 0,
					    BTRFS_COMPRESS_NONE, NULL, false);
		if (ret == 0) {
			 
			extent_clear_unlock_delalloc(inode, start, end,
				     locked_page,
				     EXTENT_LOCKED | EXTENT_DELALLOC |
				     EXTENT_DELALLOC_NEW | EXTENT_DEFRAG |
				     EXTENT_DO_ACCOUNTING, PAGE_UNLOCK |
				     PAGE_START_WRITEBACK | PAGE_END_WRITEBACK);
			 
			unlock_page(locked_page);
			ret = 1;
			goto done;
		} else if (ret < 0) {
			goto out_unlock;
		}
	}

	alloc_hint = get_extent_allocation_hint(inode, start, num_bytes);

	 
	if (btrfs_is_data_reloc_root(root))
		min_alloc_size = num_bytes;
	else
		min_alloc_size = fs_info->sectorsize;

	while (num_bytes > 0) {
		struct btrfs_ordered_extent *ordered;

		cur_alloc_size = num_bytes;
		ret = btrfs_reserve_extent(root, cur_alloc_size, cur_alloc_size,
					   min_alloc_size, 0, alloc_hint,
					   &ins, 1, 1);
		if (ret == -EAGAIN) {
			 
			ASSERT(btrfs_is_zoned(fs_info));
			if (start == orig_start) {
				wait_on_bit_io(&inode->root->fs_info->flags,
					       BTRFS_FS_NEED_ZONE_FINISH,
					       TASK_UNINTERRUPTIBLE);
				continue;
			}
			if (done_offset) {
				*done_offset = start - 1;
				return 0;
			}
			ret = -ENOSPC;
		}
		if (ret < 0)
			goto out_unlock;
		cur_alloc_size = ins.offset;
		extent_reserved = true;

		ram_size = ins.offset;
		em = create_io_em(inode, start, ins.offset,  
				  start,  
				  ins.objectid,  
				  ins.offset,  
				  ins.offset,  
				  ram_size,  
				  BTRFS_COMPRESS_NONE,  
				  BTRFS_ORDERED_REGULAR  );
		if (IS_ERR(em)) {
			ret = PTR_ERR(em);
			goto out_reserve;
		}
		free_extent_map(em);

		ordered = btrfs_alloc_ordered_extent(inode, start, ram_size,
					ram_size, ins.objectid, cur_alloc_size,
					0, 1 << BTRFS_ORDERED_REGULAR,
					BTRFS_COMPRESS_NONE);
		if (IS_ERR(ordered)) {
			ret = PTR_ERR(ordered);
			goto out_drop_extent_cache;
		}

		if (btrfs_is_data_reloc_root(root)) {
			ret = btrfs_reloc_clone_csums(ordered);

			 
			if (ret)
				btrfs_drop_extent_map_range(inode, start,
							    start + ram_size - 1,
							    false);
		}
		btrfs_put_ordered_extent(ordered);

		btrfs_dec_block_group_reservations(fs_info, ins.objectid);

		 
		page_ops = (keep_locked ? 0 : PAGE_UNLOCK);
		page_ops |= PAGE_SET_ORDERED;

		extent_clear_unlock_delalloc(inode, start, start + ram_size - 1,
					     locked_page,
					     EXTENT_LOCKED | EXTENT_DELALLOC,
					     page_ops);
		if (num_bytes < cur_alloc_size)
			num_bytes = 0;
		else
			num_bytes -= cur_alloc_size;
		alloc_hint = ins.objectid + ins.offset;
		start += cur_alloc_size;
		extent_reserved = false;

		 
		if (ret)
			goto out_unlock;
	}
done:
	if (done_offset)
		*done_offset = end;
	return ret;

out_drop_extent_cache:
	btrfs_drop_extent_map_range(inode, start, start + ram_size - 1, false);
out_reserve:
	btrfs_dec_block_group_reservations(fs_info, ins.objectid);
	btrfs_free_reserved_extent(fs_info, ins.objectid, ins.offset, 1);
out_unlock:
	 

	clear_bits = EXTENT_LOCKED | EXTENT_DELALLOC | EXTENT_DELALLOC_NEW |
		EXTENT_DEFRAG | EXTENT_CLEAR_META_RESV;
	page_ops = PAGE_UNLOCK | PAGE_START_WRITEBACK | PAGE_END_WRITEBACK;

	 
	if (keep_locked && orig_start < start) {
		if (!locked_page)
			mapping_set_error(inode->vfs_inode.i_mapping, ret);
		extent_clear_unlock_delalloc(inode, orig_start, start - 1,
					     locked_page, 0, page_ops);
	}

	 
	if (extent_reserved) {
		extent_clear_unlock_delalloc(inode, start,
					     start + cur_alloc_size - 1,
					     locked_page,
					     clear_bits,
					     page_ops);
		start += cur_alloc_size;
	}

	 
	if (start < end) {
		clear_bits |= EXTENT_CLEAR_DATA_RESV;
		extent_clear_unlock_delalloc(inode, start, end, locked_page,
					     clear_bits, page_ops);
	}
	return ret;
}

 
static noinline void submit_compressed_extents(struct btrfs_work *work)
{
	struct async_chunk *async_chunk = container_of(work, struct async_chunk,
						     work);
	struct btrfs_fs_info *fs_info = btrfs_work_owner(work);
	struct async_extent *async_extent;
	unsigned long nr_pages;
	u64 alloc_hint = 0;

	nr_pages = (async_chunk->end - async_chunk->start + PAGE_SIZE) >>
		PAGE_SHIFT;

	while (!list_empty(&async_chunk->extents)) {
		async_extent = list_entry(async_chunk->extents.next,
					  struct async_extent, list);
		list_del(&async_extent->list);
		submit_one_async_extent(async_chunk, async_extent, &alloc_hint);
	}

	 
	if (atomic_sub_return(nr_pages, &fs_info->async_delalloc_pages) <
	    5 * SZ_1M)
		cond_wake_up_nomb(&fs_info->async_submit_wait);
}

static noinline void async_cow_free(struct btrfs_work *work)
{
	struct async_chunk *async_chunk;
	struct async_cow *async_cow;

	async_chunk = container_of(work, struct async_chunk, work);
	btrfs_add_delayed_iput(async_chunk->inode);
	if (async_chunk->blkcg_css)
		css_put(async_chunk->blkcg_css);

	async_cow = async_chunk->async_cow;
	if (atomic_dec_and_test(&async_cow->num_chunks))
		kvfree(async_cow);
}

static bool run_delalloc_compressed(struct btrfs_inode *inode,
				    struct page *locked_page, u64 start,
				    u64 end, struct writeback_control *wbc)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct cgroup_subsys_state *blkcg_css = wbc_blkcg_css(wbc);
	struct async_cow *ctx;
	struct async_chunk *async_chunk;
	unsigned long nr_pages;
	u64 num_chunks = DIV_ROUND_UP(end - start, SZ_512K);
	int i;
	unsigned nofs_flag;
	const blk_opf_t write_flags = wbc_to_write_flags(wbc);

	nofs_flag = memalloc_nofs_save();
	ctx = kvmalloc(struct_size(ctx, chunks, num_chunks), GFP_KERNEL);
	memalloc_nofs_restore(nofs_flag);
	if (!ctx)
		return false;

	unlock_extent(&inode->io_tree, start, end, NULL);
	set_bit(BTRFS_INODE_HAS_ASYNC_EXTENT, &inode->runtime_flags);

	async_chunk = ctx->chunks;
	atomic_set(&ctx->num_chunks, num_chunks);

	for (i = 0; i < num_chunks; i++) {
		u64 cur_end = min(end, start + SZ_512K - 1);

		 
		ihold(&inode->vfs_inode);
		async_chunk[i].async_cow = ctx;
		async_chunk[i].inode = inode;
		async_chunk[i].start = start;
		async_chunk[i].end = cur_end;
		async_chunk[i].write_flags = write_flags;
		INIT_LIST_HEAD(&async_chunk[i].extents);

		 
		if (locked_page) {
			 
			wbc_account_cgroup_owner(wbc, locked_page,
						 cur_end - start);
			async_chunk[i].locked_page = locked_page;
			locked_page = NULL;
		} else {
			async_chunk[i].locked_page = NULL;
		}

		if (blkcg_css != blkcg_root_css) {
			css_get(blkcg_css);
			async_chunk[i].blkcg_css = blkcg_css;
			async_chunk[i].write_flags |= REQ_BTRFS_CGROUP_PUNT;
		} else {
			async_chunk[i].blkcg_css = NULL;
		}

		btrfs_init_work(&async_chunk[i].work, compress_file_range,
				submit_compressed_extents, async_cow_free);

		nr_pages = DIV_ROUND_UP(cur_end - start, PAGE_SIZE);
		atomic_add(nr_pages, &fs_info->async_delalloc_pages);

		btrfs_queue_work(fs_info->delalloc_workers, &async_chunk[i].work);

		start = cur_end + 1;
	}
	return true;
}

 
static noinline int run_delalloc_cow(struct btrfs_inode *inode,
				     struct page *locked_page, u64 start,
				     u64 end, struct writeback_control *wbc,
				     bool pages_dirty)
{
	u64 done_offset = end;
	int ret;

	while (start <= end) {
		ret = cow_file_range(inode, locked_page, start, end, &done_offset,
				     true, false);
		if (ret)
			return ret;
		extent_write_locked_range(&inode->vfs_inode, locked_page, start,
					  done_offset, wbc, pages_dirty);
		start = done_offset + 1;
	}

	return 1;
}

static noinline int csum_exist_in_range(struct btrfs_fs_info *fs_info,
					u64 bytenr, u64 num_bytes, bool nowait)
{
	struct btrfs_root *csum_root = btrfs_csum_root(fs_info, bytenr);
	struct btrfs_ordered_sum *sums;
	int ret;
	LIST_HEAD(list);

	ret = btrfs_lookup_csums_list(csum_root, bytenr, bytenr + num_bytes - 1,
				      &list, 0, nowait);
	if (ret == 0 && list_empty(&list))
		return 0;

	while (!list_empty(&list)) {
		sums = list_entry(list.next, struct btrfs_ordered_sum, list);
		list_del(&sums->list);
		kfree(sums);
	}
	if (ret < 0)
		return ret;
	return 1;
}

static int fallback_to_cow(struct btrfs_inode *inode, struct page *locked_page,
			   const u64 start, const u64 end)
{
	const bool is_space_ino = btrfs_is_free_space_inode(inode);
	const bool is_reloc_ino = btrfs_is_data_reloc_root(inode->root);
	const u64 range_bytes = end + 1 - start;
	struct extent_io_tree *io_tree = &inode->io_tree;
	u64 range_start = start;
	u64 count;
	int ret;

	 
	count = count_range_bits(io_tree, &range_start, end, range_bytes,
				 EXTENT_NORESERVE, 0, NULL);
	if (count > 0 || is_space_ino || is_reloc_ino) {
		u64 bytes = count;
		struct btrfs_fs_info *fs_info = inode->root->fs_info;
		struct btrfs_space_info *sinfo = fs_info->data_sinfo;

		if (is_space_ino || is_reloc_ino)
			bytes = range_bytes;

		spin_lock(&sinfo->lock);
		btrfs_space_info_update_bytes_may_use(fs_info, sinfo, bytes);
		spin_unlock(&sinfo->lock);

		if (count > 0)
			clear_extent_bit(io_tree, start, end, EXTENT_NORESERVE,
					 NULL);
	}

	 
	ret = cow_file_range(inode, locked_page, start, end, NULL, false, true);
	ASSERT(ret != 1);
	return ret;
}

struct can_nocow_file_extent_args {
	 

	 
	u64 start;
	 
	u64 end;
	bool writeback_path;
	bool strict;
	 
	bool free_path;

	 

	u64 disk_bytenr;
	u64 disk_num_bytes;
	u64 extent_offset;
	 
	u64 num_bytes;
};

 
static int can_nocow_file_extent(struct btrfs_path *path,
				 struct btrfs_key *key,
				 struct btrfs_inode *inode,
				 struct can_nocow_file_extent_args *args)
{
	const bool is_freespace_inode = btrfs_is_free_space_inode(inode);
	struct extent_buffer *leaf = path->nodes[0];
	struct btrfs_root *root = inode->root;
	struct btrfs_file_extent_item *fi;
	u64 extent_end;
	u8 extent_type;
	int can_nocow = 0;
	int ret = 0;
	bool nowait = path->nowait;

	fi = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_file_extent_item);
	extent_type = btrfs_file_extent_type(leaf, fi);

	if (extent_type == BTRFS_FILE_EXTENT_INLINE)
		goto out;

	 
	args->disk_bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
	args->disk_num_bytes = btrfs_file_extent_disk_num_bytes(leaf, fi);
	args->extent_offset = btrfs_file_extent_offset(leaf, fi);

	if (!(inode->flags & BTRFS_INODE_NODATACOW) &&
	    extent_type == BTRFS_FILE_EXTENT_REG)
		goto out;

	 
	if (!args->strict &&
	    btrfs_file_extent_generation(leaf, fi) <=
	    btrfs_root_last_snapshot(&root->root_item))
		goto out;

	 
	if (args->disk_bytenr == 0)
		goto out;

	 
	if (btrfs_file_extent_compression(leaf, fi) ||
	    btrfs_file_extent_encryption(leaf, fi) ||
	    btrfs_file_extent_other_encoding(leaf, fi))
		goto out;

	extent_end = btrfs_file_extent_end(path);

	 
	btrfs_release_path(path);

	ret = btrfs_cross_ref_exist(root, btrfs_ino(inode),
				    key->offset - args->extent_offset,
				    args->disk_bytenr, args->strict, path);
	WARN_ON_ONCE(ret > 0 && is_freespace_inode);
	if (ret != 0)
		goto out;

	if (args->free_path) {
		 
		btrfs_free_path(path);
		path = NULL;
	}

	 
	if (args->writeback_path && !is_freespace_inode &&
	    atomic_read(&root->snapshot_force_cow))
		goto out;

	args->disk_bytenr += args->extent_offset;
	args->disk_bytenr += args->start - key->offset;
	args->num_bytes = min(args->end + 1, extent_end) - args->start;

	 
	ret = csum_exist_in_range(root->fs_info, args->disk_bytenr, args->num_bytes,
				  nowait);
	WARN_ON_ONCE(ret > 0 && is_freespace_inode);
	if (ret != 0)
		goto out;

	can_nocow = 1;
 out:
	if (args->free_path && path)
		btrfs_free_path(path);

	return ret < 0 ? ret : can_nocow;
}

 
static noinline int run_delalloc_nocow(struct btrfs_inode *inode,
				       struct page *locked_page,
				       const u64 start, const u64 end)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct btrfs_root *root = inode->root;
	struct btrfs_path *path;
	u64 cow_start = (u64)-1;
	u64 cur_offset = start;
	int ret;
	bool check_prev = true;
	u64 ino = btrfs_ino(inode);
	struct can_nocow_file_extent_args nocow_args = { 0 };

	 
	ASSERT(!btrfs_is_zoned(fs_info) || btrfs_is_data_reloc_root(root));

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto error;
	}

	nocow_args.end = end;
	nocow_args.writeback_path = true;

	while (1) {
		struct btrfs_block_group *nocow_bg = NULL;
		struct btrfs_ordered_extent *ordered;
		struct btrfs_key found_key;
		struct btrfs_file_extent_item *fi;
		struct extent_buffer *leaf;
		u64 extent_end;
		u64 ram_bytes;
		u64 nocow_end;
		int extent_type;
		bool is_prealloc;

		ret = btrfs_lookup_file_extent(NULL, root, path, ino,
					       cur_offset, 0);
		if (ret < 0)
			goto error;

		 
		if (ret > 0 && path->slots[0] > 0 && check_prev) {
			leaf = path->nodes[0];
			btrfs_item_key_to_cpu(leaf, &found_key,
					      path->slots[0] - 1);
			if (found_key.objectid == ino &&
			    found_key.type == BTRFS_EXTENT_DATA_KEY)
				path->slots[0]--;
		}
		check_prev = false;
next_slot:
		 
		leaf = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto error;
			if (ret > 0)
				break;
			leaf = path->nodes[0];
		}

		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		 
		if (found_key.objectid > ino)
			break;
		 
		if (WARN_ON_ONCE(found_key.objectid < ino) ||
		    found_key.type < BTRFS_EXTENT_DATA_KEY) {
			path->slots[0]++;
			goto next_slot;
		}

		 
		if (found_key.type > BTRFS_EXTENT_DATA_KEY ||
		    found_key.offset > end)
			break;

		 
		if (found_key.offset > cur_offset) {
			extent_end = found_key.offset;
			extent_type = 0;
			goto must_cow;
		}

		 
		fi = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(leaf, fi);
		 
		ASSERT(extent_type < BTRFS_NR_FILE_EXTENT_TYPES);
		if (WARN_ON(extent_type >= BTRFS_NR_FILE_EXTENT_TYPES)) {
			ret = -EUCLEAN;
			goto error;
		}
		ram_bytes = btrfs_file_extent_ram_bytes(leaf, fi);
		extent_end = btrfs_file_extent_end(path);

		 
		if (extent_end <= cur_offset) {
			path->slots[0]++;
			goto next_slot;
		}

		nocow_args.start = cur_offset;
		ret = can_nocow_file_extent(path, &found_key, inode, &nocow_args);
		if (ret < 0)
			goto error;
		if (ret == 0)
			goto must_cow;

		ret = 0;
		nocow_bg = btrfs_inc_nocow_writers(fs_info, nocow_args.disk_bytenr);
		if (!nocow_bg) {
must_cow:
			 
			if (cow_start == (u64)-1)
				cow_start = cur_offset;
			cur_offset = extent_end;
			if (cur_offset > end)
				break;
			if (!path->nodes[0])
				continue;
			path->slots[0]++;
			goto next_slot;
		}

		 
		if (cow_start != (u64)-1) {
			ret = fallback_to_cow(inode, locked_page,
					      cow_start, found_key.offset - 1);
			cow_start = (u64)-1;
			if (ret) {
				btrfs_dec_nocow_writers(nocow_bg);
				goto error;
			}
		}

		nocow_end = cur_offset + nocow_args.num_bytes - 1;
		is_prealloc = extent_type == BTRFS_FILE_EXTENT_PREALLOC;
		if (is_prealloc) {
			u64 orig_start = found_key.offset - nocow_args.extent_offset;
			struct extent_map *em;

			em = create_io_em(inode, cur_offset, nocow_args.num_bytes,
					  orig_start,
					  nocow_args.disk_bytenr,  
					  nocow_args.num_bytes,  
					  nocow_args.disk_num_bytes,  
					  ram_bytes, BTRFS_COMPRESS_NONE,
					  BTRFS_ORDERED_PREALLOC);
			if (IS_ERR(em)) {
				btrfs_dec_nocow_writers(nocow_bg);
				ret = PTR_ERR(em);
				goto error;
			}
			free_extent_map(em);
		}

		ordered = btrfs_alloc_ordered_extent(inode, cur_offset,
				nocow_args.num_bytes, nocow_args.num_bytes,
				nocow_args.disk_bytenr, nocow_args.num_bytes, 0,
				is_prealloc
				? (1 << BTRFS_ORDERED_PREALLOC)
				: (1 << BTRFS_ORDERED_NOCOW),
				BTRFS_COMPRESS_NONE);
		btrfs_dec_nocow_writers(nocow_bg);
		if (IS_ERR(ordered)) {
			if (is_prealloc) {
				btrfs_drop_extent_map_range(inode, cur_offset,
							    nocow_end, false);
			}
			ret = PTR_ERR(ordered);
			goto error;
		}

		if (btrfs_is_data_reloc_root(root))
			 
			ret = btrfs_reloc_clone_csums(ordered);
		btrfs_put_ordered_extent(ordered);

		extent_clear_unlock_delalloc(inode, cur_offset, nocow_end,
					     locked_page, EXTENT_LOCKED |
					     EXTENT_DELALLOC |
					     EXTENT_CLEAR_DATA_RESV,
					     PAGE_UNLOCK | PAGE_SET_ORDERED);

		cur_offset = extent_end;

		 
		if (ret)
			goto error;
		if (cur_offset > end)
			break;
	}
	btrfs_release_path(path);

	if (cur_offset <= end && cow_start == (u64)-1)
		cow_start = cur_offset;

	if (cow_start != (u64)-1) {
		cur_offset = end;
		ret = fallback_to_cow(inode, locked_page, cow_start, end);
		cow_start = (u64)-1;
		if (ret)
			goto error;
	}

	btrfs_free_path(path);
	return 0;

error:
	 
	if (cow_start != (u64)-1)
		cur_offset = cow_start;
	if (cur_offset < end)
		extent_clear_unlock_delalloc(inode, cur_offset, end,
					     locked_page, EXTENT_LOCKED |
					     EXTENT_DELALLOC | EXTENT_DEFRAG |
					     EXTENT_DO_ACCOUNTING, PAGE_UNLOCK |
					     PAGE_START_WRITEBACK |
					     PAGE_END_WRITEBACK);
	btrfs_free_path(path);
	return ret;
}

static bool should_nocow(struct btrfs_inode *inode, u64 start, u64 end)
{
	if (inode->flags & (BTRFS_INODE_NODATACOW | BTRFS_INODE_PREALLOC)) {
		if (inode->defrag_bytes &&
		    test_range_bit(&inode->io_tree, start, end, EXTENT_DEFRAG,
				   0, NULL))
			return false;
		return true;
	}
	return false;
}

 
int btrfs_run_delalloc_range(struct btrfs_inode *inode, struct page *locked_page,
			     u64 start, u64 end, struct writeback_control *wbc)
{
	const bool zoned = btrfs_is_zoned(inode->root->fs_info);
	int ret;

	 
	ASSERT(!(end <= page_offset(locked_page) ||
		 start >= page_offset(locked_page) + PAGE_SIZE));

	if (should_nocow(inode, start, end)) {
		ret = run_delalloc_nocow(inode, locked_page, start, end);
		goto out;
	}

	if (btrfs_inode_can_compress(inode) &&
	    inode_need_compress(inode, start, end) &&
	    run_delalloc_compressed(inode, locked_page, start, end, wbc))
		return 1;

	if (zoned)
		ret = run_delalloc_cow(inode, locked_page, start, end, wbc,
				       true);
	else
		ret = cow_file_range(inode, locked_page, start, end, NULL,
				     false, false);

out:
	if (ret < 0)
		btrfs_cleanup_ordered_extents(inode, locked_page, start,
					      end - start + 1);
	return ret;
}

void btrfs_split_delalloc_extent(struct btrfs_inode *inode,
				 struct extent_state *orig, u64 split)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	u64 size;

	 
	if (!(orig->state & EXTENT_DELALLOC))
		return;

	size = orig->end - orig->start + 1;
	if (size > fs_info->max_extent_size) {
		u32 num_extents;
		u64 new_size;

		 
		new_size = orig->end - split + 1;
		num_extents = count_max_extents(fs_info, new_size);
		new_size = split - orig->start;
		num_extents += count_max_extents(fs_info, new_size);
		if (count_max_extents(fs_info, size) >= num_extents)
			return;
	}

	spin_lock(&inode->lock);
	btrfs_mod_outstanding_extents(inode, 1);
	spin_unlock(&inode->lock);
}

 
void btrfs_merge_delalloc_extent(struct btrfs_inode *inode, struct extent_state *new,
				 struct extent_state *other)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	u64 new_size, old_size;
	u32 num_extents;

	 
	if (!(other->state & EXTENT_DELALLOC))
		return;

	if (new->start > other->start)
		new_size = new->end - other->start + 1;
	else
		new_size = other->end - new->start + 1;

	 
	if (new_size <= fs_info->max_extent_size) {
		spin_lock(&inode->lock);
		btrfs_mod_outstanding_extents(inode, -1);
		spin_unlock(&inode->lock);
		return;
	}

	 
	old_size = other->end - other->start + 1;
	num_extents = count_max_extents(fs_info, old_size);
	old_size = new->end - new->start + 1;
	num_extents += count_max_extents(fs_info, old_size);
	if (count_max_extents(fs_info, new_size) >= num_extents)
		return;

	spin_lock(&inode->lock);
	btrfs_mod_outstanding_extents(inode, -1);
	spin_unlock(&inode->lock);
}

static void btrfs_add_delalloc_inodes(struct btrfs_root *root,
				      struct btrfs_inode *inode)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;

	spin_lock(&root->delalloc_lock);
	if (list_empty(&inode->delalloc_inodes)) {
		list_add_tail(&inode->delalloc_inodes, &root->delalloc_inodes);
		set_bit(BTRFS_INODE_IN_DELALLOC_LIST, &inode->runtime_flags);
		root->nr_delalloc_inodes++;
		if (root->nr_delalloc_inodes == 1) {
			spin_lock(&fs_info->delalloc_root_lock);
			BUG_ON(!list_empty(&root->delalloc_root));
			list_add_tail(&root->delalloc_root,
				      &fs_info->delalloc_roots);
			spin_unlock(&fs_info->delalloc_root_lock);
		}
	}
	spin_unlock(&root->delalloc_lock);
}

void __btrfs_del_delalloc_inode(struct btrfs_root *root,
				struct btrfs_inode *inode)
{
	struct btrfs_fs_info *fs_info = root->fs_info;

	if (!list_empty(&inode->delalloc_inodes)) {
		list_del_init(&inode->delalloc_inodes);
		clear_bit(BTRFS_INODE_IN_DELALLOC_LIST,
			  &inode->runtime_flags);
		root->nr_delalloc_inodes--;
		if (!root->nr_delalloc_inodes) {
			ASSERT(list_empty(&root->delalloc_inodes));
			spin_lock(&fs_info->delalloc_root_lock);
			BUG_ON(list_empty(&root->delalloc_root));
			list_del_init(&root->delalloc_root);
			spin_unlock(&fs_info->delalloc_root_lock);
		}
	}
}

static void btrfs_del_delalloc_inode(struct btrfs_root *root,
				     struct btrfs_inode *inode)
{
	spin_lock(&root->delalloc_lock);
	__btrfs_del_delalloc_inode(root, inode);
	spin_unlock(&root->delalloc_lock);
}

 
void btrfs_set_delalloc_extent(struct btrfs_inode *inode, struct extent_state *state,
			       u32 bits)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;

	if ((bits & EXTENT_DEFRAG) && !(bits & EXTENT_DELALLOC))
		WARN_ON(1);
	 
	if (!(state->state & EXTENT_DELALLOC) && (bits & EXTENT_DELALLOC)) {
		struct btrfs_root *root = inode->root;
		u64 len = state->end + 1 - state->start;
		u32 num_extents = count_max_extents(fs_info, len);
		bool do_list = !btrfs_is_free_space_inode(inode);

		spin_lock(&inode->lock);
		btrfs_mod_outstanding_extents(inode, num_extents);
		spin_unlock(&inode->lock);

		 
		if (btrfs_is_testing(fs_info))
			return;

		percpu_counter_add_batch(&fs_info->delalloc_bytes, len,
					 fs_info->delalloc_batch);
		spin_lock(&inode->lock);
		inode->delalloc_bytes += len;
		if (bits & EXTENT_DEFRAG)
			inode->defrag_bytes += len;
		if (do_list && !test_bit(BTRFS_INODE_IN_DELALLOC_LIST,
					 &inode->runtime_flags))
			btrfs_add_delalloc_inodes(root, inode);
		spin_unlock(&inode->lock);
	}

	if (!(state->state & EXTENT_DELALLOC_NEW) &&
	    (bits & EXTENT_DELALLOC_NEW)) {
		spin_lock(&inode->lock);
		inode->new_delalloc_bytes += state->end + 1 - state->start;
		spin_unlock(&inode->lock);
	}
}

 
void btrfs_clear_delalloc_extent(struct btrfs_inode *inode,
				 struct extent_state *state, u32 bits)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	u64 len = state->end + 1 - state->start;
	u32 num_extents = count_max_extents(fs_info, len);

	if ((state->state & EXTENT_DEFRAG) && (bits & EXTENT_DEFRAG)) {
		spin_lock(&inode->lock);
		inode->defrag_bytes -= len;
		spin_unlock(&inode->lock);
	}

	 
	if ((state->state & EXTENT_DELALLOC) && (bits & EXTENT_DELALLOC)) {
		struct btrfs_root *root = inode->root;
		bool do_list = !btrfs_is_free_space_inode(inode);

		spin_lock(&inode->lock);
		btrfs_mod_outstanding_extents(inode, -num_extents);
		spin_unlock(&inode->lock);

		 
		if (bits & EXTENT_CLEAR_META_RESV &&
		    root != fs_info->tree_root)
			btrfs_delalloc_release_metadata(inode, len, false);

		 
		if (btrfs_is_testing(fs_info))
			return;

		if (!btrfs_is_data_reloc_root(root) &&
		    do_list && !(state->state & EXTENT_NORESERVE) &&
		    (bits & EXTENT_CLEAR_DATA_RESV))
			btrfs_free_reserved_data_space_noquota(fs_info, len);

		percpu_counter_add_batch(&fs_info->delalloc_bytes, -len,
					 fs_info->delalloc_batch);
		spin_lock(&inode->lock);
		inode->delalloc_bytes -= len;
		if (do_list && inode->delalloc_bytes == 0 &&
		    test_bit(BTRFS_INODE_IN_DELALLOC_LIST,
					&inode->runtime_flags))
			btrfs_del_delalloc_inode(root, inode);
		spin_unlock(&inode->lock);
	}

	if ((state->state & EXTENT_DELALLOC_NEW) &&
	    (bits & EXTENT_DELALLOC_NEW)) {
		spin_lock(&inode->lock);
		ASSERT(inode->new_delalloc_bytes >= len);
		inode->new_delalloc_bytes -= len;
		if (bits & EXTENT_ADD_INODE_BYTES)
			inode_add_bytes(&inode->vfs_inode, len);
		spin_unlock(&inode->lock);
	}
}

static int btrfs_extract_ordered_extent(struct btrfs_bio *bbio,
					struct btrfs_ordered_extent *ordered)
{
	u64 start = (u64)bbio->bio.bi_iter.bi_sector << SECTOR_SHIFT;
	u64 len = bbio->bio.bi_iter.bi_size;
	struct btrfs_ordered_extent *new;
	int ret;

	 
	if (WARN_ON_ONCE(start != ordered->disk_bytenr))
		return -EINVAL;

	 
	if (ordered->disk_num_bytes == len) {
		refcount_inc(&ordered->refs);
		bbio->ordered = ordered;
		return 0;
	}

	 
	if (!test_bit(BTRFS_ORDERED_NOCOW, &ordered->flags)) {
		ret = split_extent_map(bbio->inode, bbio->file_offset,
				       ordered->num_bytes, len,
				       ordered->disk_bytenr);
		if (ret)
			return ret;
	}

	new = btrfs_split_ordered_extent(ordered, len);
	if (IS_ERR(new))
		return PTR_ERR(new);
	bbio->ordered = new;
	return 0;
}

 
static int add_pending_csums(struct btrfs_trans_handle *trans,
			     struct list_head *list)
{
	struct btrfs_ordered_sum *sum;
	struct btrfs_root *csum_root = NULL;
	int ret;

	list_for_each_entry(sum, list, list) {
		trans->adding_csums = true;
		if (!csum_root)
			csum_root = btrfs_csum_root(trans->fs_info,
						    sum->logical);
		ret = btrfs_csum_file_blocks(trans, csum_root, sum);
		trans->adding_csums = false;
		if (ret)
			return ret;
	}
	return 0;
}

static int btrfs_find_new_delalloc_bytes(struct btrfs_inode *inode,
					 const u64 start,
					 const u64 len,
					 struct extent_state **cached_state)
{
	u64 search_start = start;
	const u64 end = start + len - 1;

	while (search_start < end) {
		const u64 search_len = end - search_start + 1;
		struct extent_map *em;
		u64 em_len;
		int ret = 0;

		em = btrfs_get_extent(inode, NULL, 0, search_start, search_len);
		if (IS_ERR(em))
			return PTR_ERR(em);

		if (em->block_start != EXTENT_MAP_HOLE)
			goto next;

		em_len = em->len;
		if (em->start < search_start)
			em_len -= search_start - em->start;
		if (em_len > search_len)
			em_len = search_len;

		ret = set_extent_bit(&inode->io_tree, search_start,
				     search_start + em_len - 1,
				     EXTENT_DELALLOC_NEW, cached_state);
next:
		search_start = extent_map_end(em);
		free_extent_map(em);
		if (ret)
			return ret;
	}
	return 0;
}

int btrfs_set_extent_delalloc(struct btrfs_inode *inode, u64 start, u64 end,
			      unsigned int extra_bits,
			      struct extent_state **cached_state)
{
	WARN_ON(PAGE_ALIGNED(end));

	if (start >= i_size_read(&inode->vfs_inode) &&
	    !(inode->flags & BTRFS_INODE_PREALLOC)) {
		 
		extra_bits |= EXTENT_DELALLOC_NEW;
	} else {
		int ret;

		ret = btrfs_find_new_delalloc_bytes(inode, start,
						    end + 1 - start,
						    cached_state);
		if (ret)
			return ret;
	}

	return set_extent_bit(&inode->io_tree, start, end,
			      EXTENT_DELALLOC | extra_bits, cached_state);
}

 
struct btrfs_writepage_fixup {
	struct page *page;
	struct btrfs_inode *inode;
	struct btrfs_work work;
};

static void btrfs_writepage_fixup_worker(struct btrfs_work *work)
{
	struct btrfs_writepage_fixup *fixup =
		container_of(work, struct btrfs_writepage_fixup, work);
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	struct extent_changeset *data_reserved = NULL;
	struct page *page = fixup->page;
	struct btrfs_inode *inode = fixup->inode;
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	u64 page_start = page_offset(page);
	u64 page_end = page_offset(page) + PAGE_SIZE - 1;
	int ret = 0;
	bool free_delalloc_space = true;

	 
	ret = btrfs_delalloc_reserve_space(inode, &data_reserved, page_start,
					   PAGE_SIZE);
again:
	lock_page(page);

	 
	if (!page->mapping || !PageDirty(page) || !PageChecked(page)) {
		 
		if (!ret) {
			btrfs_delalloc_release_extents(inode, PAGE_SIZE);
			btrfs_delalloc_release_space(inode, data_reserved,
						     page_start, PAGE_SIZE,
						     true);
		}
		ret = 0;
		goto out_page;
	}

	 
	if (ret)
		goto out_page;

	lock_extent(&inode->io_tree, page_start, page_end, &cached_state);

	 
	if (PageOrdered(page))
		goto out_reserved;

	ordered = btrfs_lookup_ordered_range(inode, page_start, PAGE_SIZE);
	if (ordered) {
		unlock_extent(&inode->io_tree, page_start, page_end,
			      &cached_state);
		unlock_page(page);
		btrfs_start_ordered_extent(ordered);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	ret = btrfs_set_extent_delalloc(inode, page_start, page_end, 0,
					&cached_state);
	if (ret)
		goto out_reserved;

	 
	BUG_ON(!PageDirty(page));
	free_delalloc_space = false;
out_reserved:
	btrfs_delalloc_release_extents(inode, PAGE_SIZE);
	if (free_delalloc_space)
		btrfs_delalloc_release_space(inode, data_reserved, page_start,
					     PAGE_SIZE, true);
	unlock_extent(&inode->io_tree, page_start, page_end, &cached_state);
out_page:
	if (ret) {
		 
		mapping_set_error(page->mapping, ret);
		btrfs_mark_ordered_io_finished(inode, page, page_start,
					       PAGE_SIZE, !ret);
		clear_page_dirty_for_io(page);
	}
	btrfs_page_clear_checked(fs_info, page, page_start, PAGE_SIZE);
	unlock_page(page);
	put_page(page);
	kfree(fixup);
	extent_changeset_free(data_reserved);
	 
	btrfs_add_delayed_iput(inode);
}

 
int btrfs_writepage_cow_fixup(struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct btrfs_writepage_fixup *fixup;

	 
	if (PageOrdered(page))
		return 0;

	 
	if (PageChecked(page))
		return -EAGAIN;

	fixup = kzalloc(sizeof(*fixup), GFP_NOFS);
	if (!fixup)
		return -EAGAIN;

	 
	ihold(inode);
	btrfs_page_set_checked(fs_info, page, page_offset(page), PAGE_SIZE);
	get_page(page);
	btrfs_init_work(&fixup->work, btrfs_writepage_fixup_worker, NULL, NULL);
	fixup->page = page;
	fixup->inode = BTRFS_I(inode);
	btrfs_queue_work(fs_info->fixup_workers, &fixup->work);

	return -EAGAIN;
}

static int insert_reserved_file_extent(struct btrfs_trans_handle *trans,
				       struct btrfs_inode *inode, u64 file_pos,
				       struct btrfs_file_extent_item *stack_fi,
				       const bool update_inode_bytes,
				       u64 qgroup_reserved)
{
	struct btrfs_root *root = inode->root;
	const u64 sectorsize = root->fs_info->sectorsize;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key ins;
	u64 disk_num_bytes = btrfs_stack_file_extent_disk_num_bytes(stack_fi);
	u64 disk_bytenr = btrfs_stack_file_extent_disk_bytenr(stack_fi);
	u64 offset = btrfs_stack_file_extent_offset(stack_fi);
	u64 num_bytes = btrfs_stack_file_extent_num_bytes(stack_fi);
	u64 ram_bytes = btrfs_stack_file_extent_ram_bytes(stack_fi);
	struct btrfs_drop_extents_args drop_args = { 0 };
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	 
	drop_args.path = path;
	drop_args.start = file_pos;
	drop_args.end = file_pos + num_bytes;
	drop_args.replace_extent = true;
	drop_args.extent_item_size = sizeof(*stack_fi);
	ret = btrfs_drop_extents(trans, root, inode, &drop_args);
	if (ret)
		goto out;

	if (!drop_args.extent_inserted) {
		ins.objectid = btrfs_ino(inode);
		ins.offset = file_pos;
		ins.type = BTRFS_EXTENT_DATA_KEY;

		ret = btrfs_insert_empty_item(trans, root, path, &ins,
					      sizeof(*stack_fi));
		if (ret)
			goto out;
	}
	leaf = path->nodes[0];
	btrfs_set_stack_file_extent_generation(stack_fi, trans->transid);
	write_extent_buffer(leaf, stack_fi,
			btrfs_item_ptr_offset(leaf, path->slots[0]),
			sizeof(struct btrfs_file_extent_item));

	btrfs_mark_buffer_dirty(trans, leaf);
	btrfs_release_path(path);

	 
	if (file_pos == 0 && !IS_ALIGNED(drop_args.bytes_found, sectorsize)) {
		u64 inline_size = round_down(drop_args.bytes_found, sectorsize);

		inline_size = drop_args.bytes_found - inline_size;
		btrfs_update_inode_bytes(inode, sectorsize, inline_size);
		drop_args.bytes_found -= inline_size;
		num_bytes -= sectorsize;
	}

	if (update_inode_bytes)
		btrfs_update_inode_bytes(inode, num_bytes, drop_args.bytes_found);

	ins.objectid = disk_bytenr;
	ins.offset = disk_num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;

	ret = btrfs_inode_set_file_extent_range(inode, file_pos, ram_bytes);
	if (ret)
		goto out;

	ret = btrfs_alloc_reserved_file_extent(trans, root, btrfs_ino(inode),
					       file_pos - offset,
					       qgroup_reserved, &ins);
out:
	btrfs_free_path(path);

	return ret;
}

static void btrfs_release_delalloc_bytes(struct btrfs_fs_info *fs_info,
					 u64 start, u64 len)
{
	struct btrfs_block_group *cache;

	cache = btrfs_lookup_block_group(fs_info, start);
	ASSERT(cache);

	spin_lock(&cache->lock);
	cache->delalloc_bytes -= len;
	spin_unlock(&cache->lock);

	btrfs_put_block_group(cache);
}

static int insert_ordered_extent_file_extent(struct btrfs_trans_handle *trans,
					     struct btrfs_ordered_extent *oe)
{
	struct btrfs_file_extent_item stack_fi;
	bool update_inode_bytes;
	u64 num_bytes = oe->num_bytes;
	u64 ram_bytes = oe->ram_bytes;

	memset(&stack_fi, 0, sizeof(stack_fi));
	btrfs_set_stack_file_extent_type(&stack_fi, BTRFS_FILE_EXTENT_REG);
	btrfs_set_stack_file_extent_disk_bytenr(&stack_fi, oe->disk_bytenr);
	btrfs_set_stack_file_extent_disk_num_bytes(&stack_fi,
						   oe->disk_num_bytes);
	btrfs_set_stack_file_extent_offset(&stack_fi, oe->offset);
	if (test_bit(BTRFS_ORDERED_TRUNCATED, &oe->flags)) {
		num_bytes = oe->truncated_len;
		ram_bytes = num_bytes;
	}
	btrfs_set_stack_file_extent_num_bytes(&stack_fi, num_bytes);
	btrfs_set_stack_file_extent_ram_bytes(&stack_fi, ram_bytes);
	btrfs_set_stack_file_extent_compression(&stack_fi, oe->compress_type);
	 

	 
	update_inode_bytes = test_bit(BTRFS_ORDERED_DIRECT, &oe->flags) ||
			     test_bit(BTRFS_ORDERED_ENCODED, &oe->flags) ||
			     test_bit(BTRFS_ORDERED_TRUNCATED, &oe->flags);

	return insert_reserved_file_extent(trans, BTRFS_I(oe->inode),
					   oe->file_offset, &stack_fi,
					   update_inode_bytes, oe->qgroup_rsv);
}

 
int btrfs_finish_one_ordered(struct btrfs_ordered_extent *ordered_extent)
{
	struct btrfs_inode *inode = BTRFS_I(ordered_extent->inode);
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans = NULL;
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct extent_state *cached_state = NULL;
	u64 start, end;
	int compress_type = 0;
	int ret = 0;
	u64 logical_len = ordered_extent->num_bytes;
	bool freespace_inode;
	bool truncated = false;
	bool clear_reserved_extent = true;
	unsigned int clear_bits = EXTENT_DEFRAG;

	start = ordered_extent->file_offset;
	end = start + ordered_extent->num_bytes - 1;

	if (!test_bit(BTRFS_ORDERED_NOCOW, &ordered_extent->flags) &&
	    !test_bit(BTRFS_ORDERED_PREALLOC, &ordered_extent->flags) &&
	    !test_bit(BTRFS_ORDERED_DIRECT, &ordered_extent->flags) &&
	    !test_bit(BTRFS_ORDERED_ENCODED, &ordered_extent->flags))
		clear_bits |= EXTENT_DELALLOC_NEW;

	freespace_inode = btrfs_is_free_space_inode(inode);
	if (!freespace_inode)
		btrfs_lockdep_acquire(fs_info, btrfs_ordered_extent);

	if (test_bit(BTRFS_ORDERED_IOERR, &ordered_extent->flags)) {
		ret = -EIO;
		goto out;
	}

	if (btrfs_is_zoned(fs_info))
		btrfs_zone_finish_endio(fs_info, ordered_extent->disk_bytenr,
					ordered_extent->disk_num_bytes);

	if (test_bit(BTRFS_ORDERED_TRUNCATED, &ordered_extent->flags)) {
		truncated = true;
		logical_len = ordered_extent->truncated_len;
		 
		if (!logical_len)
			goto out;
	}

	if (test_bit(BTRFS_ORDERED_NOCOW, &ordered_extent->flags)) {
		BUG_ON(!list_empty(&ordered_extent->list));  

		btrfs_inode_safe_disk_i_size_write(inode, 0);
		if (freespace_inode)
			trans = btrfs_join_transaction_spacecache(root);
		else
			trans = btrfs_join_transaction(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			trans = NULL;
			goto out;
		}
		trans->block_rsv = &inode->block_rsv;
		ret = btrfs_update_inode_fallback(trans, root, inode);
		if (ret)  
			btrfs_abort_transaction(trans, ret);
		goto out;
	}

	clear_bits |= EXTENT_LOCKED;
	lock_extent(io_tree, start, end, &cached_state);

	if (freespace_inode)
		trans = btrfs_join_transaction_spacecache(root);
	else
		trans = btrfs_join_transaction(root);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		trans = NULL;
		goto out;
	}

	trans->block_rsv = &inode->block_rsv;

	if (test_bit(BTRFS_ORDERED_COMPRESSED, &ordered_extent->flags))
		compress_type = ordered_extent->compress_type;
	if (test_bit(BTRFS_ORDERED_PREALLOC, &ordered_extent->flags)) {
		BUG_ON(compress_type);
		ret = btrfs_mark_extent_written(trans, inode,
						ordered_extent->file_offset,
						ordered_extent->file_offset +
						logical_len);
		btrfs_zoned_release_data_reloc_bg(fs_info, ordered_extent->disk_bytenr,
						  ordered_extent->disk_num_bytes);
	} else {
		BUG_ON(root == fs_info->tree_root);
		ret = insert_ordered_extent_file_extent(trans, ordered_extent);
		if (!ret) {
			clear_reserved_extent = false;
			btrfs_release_delalloc_bytes(fs_info,
						ordered_extent->disk_bytenr,
						ordered_extent->disk_num_bytes);
		}
	}
	unpin_extent_cache(&inode->extent_tree, ordered_extent->file_offset,
			   ordered_extent->num_bytes, trans->transid);
	if (ret < 0) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}

	ret = add_pending_csums(trans, &ordered_extent->list);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}

	 
	if ((clear_bits & EXTENT_DELALLOC_NEW) &&
	    !test_bit(BTRFS_ORDERED_TRUNCATED, &ordered_extent->flags))
		clear_extent_bit(&inode->io_tree, start, end,
				 EXTENT_DELALLOC_NEW | EXTENT_ADD_INODE_BYTES,
				 &cached_state);

	btrfs_inode_safe_disk_i_size_write(inode, 0);
	ret = btrfs_update_inode_fallback(trans, root, inode);
	if (ret) {  
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
	ret = 0;
out:
	clear_extent_bit(&inode->io_tree, start, end, clear_bits,
			 &cached_state);

	if (trans)
		btrfs_end_transaction(trans);

	if (ret || truncated) {
		u64 unwritten_start = start;

		 
		if (ret && !test_and_set_bit(BTRFS_ORDERED_IOERR,
					     &ordered_extent->flags))
			mapping_set_error(ordered_extent->inode->i_mapping, -EIO);

		if (truncated)
			unwritten_start += logical_len;
		clear_extent_uptodate(io_tree, unwritten_start, end, NULL);

		 
		btrfs_drop_extent_map_range(inode, unwritten_start, end, false);

		 
		if ((ret || !logical_len) &&
		    clear_reserved_extent &&
		    !test_bit(BTRFS_ORDERED_NOCOW, &ordered_extent->flags) &&
		    !test_bit(BTRFS_ORDERED_PREALLOC, &ordered_extent->flags)) {
			 
			if (ret && btrfs_test_opt(fs_info, DISCARD_SYNC))
				btrfs_discard_extent(fs_info,
						ordered_extent->disk_bytenr,
						ordered_extent->disk_num_bytes,
						NULL);
			btrfs_free_reserved_extent(fs_info,
					ordered_extent->disk_bytenr,
					ordered_extent->disk_num_bytes, 1);
			 
			btrfs_qgroup_free_refroot(fs_info, inode->root->root_key.objectid,
						  ordered_extent->qgroup_rsv,
						  BTRFS_QGROUP_RSV_DATA);
		}
	}

	 
	btrfs_remove_ordered_extent(inode, ordered_extent);

	 
	btrfs_put_ordered_extent(ordered_extent);
	 
	btrfs_put_ordered_extent(ordered_extent);

	return ret;
}

int btrfs_finish_ordered_io(struct btrfs_ordered_extent *ordered)
{
	if (btrfs_is_zoned(btrfs_sb(ordered->inode->i_sb)) &&
	    !test_bit(BTRFS_ORDERED_IOERR, &ordered->flags))
		btrfs_finish_ordered_zoned(ordered);
	return btrfs_finish_one_ordered(ordered);
}

 
int btrfs_check_sector_csum(struct btrfs_fs_info *fs_info, struct page *page,
			    u32 pgoff, u8 *csum, const u8 * const csum_expected)
{
	SHASH_DESC_ON_STACK(shash, fs_info->csum_shash);
	char *kaddr;

	ASSERT(pgoff + fs_info->sectorsize <= PAGE_SIZE);

	shash->tfm = fs_info->csum_shash;

	kaddr = kmap_local_page(page) + pgoff;
	crypto_shash_digest(shash, kaddr, fs_info->sectorsize, csum);
	kunmap_local(kaddr);

	if (memcmp(csum, csum_expected, fs_info->csum_size))
		return -EIO;
	return 0;
}

 
bool btrfs_data_csum_ok(struct btrfs_bio *bbio, struct btrfs_device *dev,
			u32 bio_offset, struct bio_vec *bv)
{
	struct btrfs_inode *inode = bbio->inode;
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	u64 file_offset = bbio->file_offset + bio_offset;
	u64 end = file_offset + bv->bv_len - 1;
	u8 *csum_expected;
	u8 csum[BTRFS_CSUM_SIZE];

	ASSERT(bv->bv_len == fs_info->sectorsize);

	if (!bbio->csum)
		return true;

	if (btrfs_is_data_reloc_root(inode->root) &&
	    test_range_bit(&inode->io_tree, file_offset, end, EXTENT_NODATASUM,
			   1, NULL)) {
		 
		clear_extent_bits(&inode->io_tree, file_offset, end,
				  EXTENT_NODATASUM);
		return true;
	}

	csum_expected = bbio->csum + (bio_offset >> fs_info->sectorsize_bits) *
				fs_info->csum_size;
	if (btrfs_check_sector_csum(fs_info, bv->bv_page, bv->bv_offset, csum,
				    csum_expected))
		goto zeroit;
	return true;

zeroit:
	btrfs_print_data_csum_error(inode, file_offset, csum, csum_expected,
				    bbio->mirror_num);
	if (dev)
		btrfs_dev_stat_inc_and_print(dev, BTRFS_DEV_STAT_CORRUPTION_ERRS);
	memzero_bvec(bv);
	return false;
}

 
void btrfs_add_delayed_iput(struct btrfs_inode *inode)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	unsigned long flags;

	if (atomic_add_unless(&inode->vfs_inode.i_count, -1, 1))
		return;

	atomic_inc(&fs_info->nr_delayed_iputs);
	 
	spin_lock_irqsave(&fs_info->delayed_iput_lock, flags);
	ASSERT(list_empty(&inode->delayed_iput));
	list_add_tail(&inode->delayed_iput, &fs_info->delayed_iputs);
	spin_unlock_irqrestore(&fs_info->delayed_iput_lock, flags);
	if (!test_bit(BTRFS_FS_CLEANER_RUNNING, &fs_info->flags))
		wake_up_process(fs_info->cleaner_kthread);
}

static void run_delayed_iput_locked(struct btrfs_fs_info *fs_info,
				    struct btrfs_inode *inode)
{
	list_del_init(&inode->delayed_iput);
	spin_unlock_irq(&fs_info->delayed_iput_lock);
	iput(&inode->vfs_inode);
	if (atomic_dec_and_test(&fs_info->nr_delayed_iputs))
		wake_up(&fs_info->delayed_iputs_wait);
	spin_lock_irq(&fs_info->delayed_iput_lock);
}

static void btrfs_run_delayed_iput(struct btrfs_fs_info *fs_info,
				   struct btrfs_inode *inode)
{
	if (!list_empty(&inode->delayed_iput)) {
		spin_lock_irq(&fs_info->delayed_iput_lock);
		if (!list_empty(&inode->delayed_iput))
			run_delayed_iput_locked(fs_info, inode);
		spin_unlock_irq(&fs_info->delayed_iput_lock);
	}
}

void btrfs_run_delayed_iputs(struct btrfs_fs_info *fs_info)
{
	 
	spin_lock_irq(&fs_info->delayed_iput_lock);
	while (!list_empty(&fs_info->delayed_iputs)) {
		struct btrfs_inode *inode;

		inode = list_first_entry(&fs_info->delayed_iputs,
				struct btrfs_inode, delayed_iput);
		run_delayed_iput_locked(fs_info, inode);
		if (need_resched()) {
			spin_unlock_irq(&fs_info->delayed_iput_lock);
			cond_resched();
			spin_lock_irq(&fs_info->delayed_iput_lock);
		}
	}
	spin_unlock_irq(&fs_info->delayed_iput_lock);
}

 
int btrfs_wait_on_delayed_iputs(struct btrfs_fs_info *fs_info)
{
	int ret = wait_event_killable(fs_info->delayed_iputs_wait,
			atomic_read(&fs_info->nr_delayed_iputs) == 0);
	if (ret)
		return -EINTR;
	return 0;
}

 
int btrfs_orphan_add(struct btrfs_trans_handle *trans,
		     struct btrfs_inode *inode)
{
	int ret;

	ret = btrfs_insert_orphan_item(trans, inode->root, btrfs_ino(inode));
	if (ret && ret != -EEXIST) {
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	return 0;
}

 
static int btrfs_orphan_del(struct btrfs_trans_handle *trans,
			    struct btrfs_inode *inode)
{
	return btrfs_del_orphan_item(trans, inode->root, btrfs_ino(inode));
}

 
int btrfs_orphan_cleanup(struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key key, found_key;
	struct btrfs_trans_handle *trans;
	struct inode *inode;
	u64 last_objectid = 0;
	int ret = 0, nr_unlink = 0;

	if (test_and_set_bit(BTRFS_ROOT_ORPHAN_CLEANUP, &root->state))
		return 0;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}
	path->reada = READA_BACK;

	key.objectid = BTRFS_ORPHAN_OBJECTID;
	key.type = BTRFS_ORPHAN_ITEM_KEY;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0)
			goto out;

		 
		if (ret > 0) {
			ret = 0;
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		 
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		 
		if (found_key.objectid != BTRFS_ORPHAN_OBJECTID)
			break;
		if (found_key.type != BTRFS_ORPHAN_ITEM_KEY)
			break;

		 
		btrfs_release_path(path);

		 

		if (found_key.offset == last_objectid) {
			 
			btrfs_err(fs_info,
				  "Error removing orphan entry, stopping orphan cleanup");
			ret = BTRFS_FS_ERROR(fs_info) ?: -EINVAL;
			goto out;
		}

		last_objectid = found_key.offset;

		found_key.objectid = found_key.offset;
		found_key.type = BTRFS_INODE_ITEM_KEY;
		found_key.offset = 0;
		inode = btrfs_iget(fs_info->sb, last_objectid, root);
		if (IS_ERR(inode)) {
			ret = PTR_ERR(inode);
			inode = NULL;
			if (ret != -ENOENT)
				goto out;
		}

		if (!inode && root == fs_info->tree_root) {
			struct btrfs_root *dead_root;
			int is_dead_root = 0;

			 

			spin_lock(&fs_info->fs_roots_radix_lock);
			dead_root = radix_tree_lookup(&fs_info->fs_roots_radix,
							 (unsigned long)found_key.objectid);
			if (dead_root && btrfs_root_refs(&dead_root->root_item) == 0)
				is_dead_root = 1;
			spin_unlock(&fs_info->fs_roots_radix_lock);

			if (is_dead_root) {
				 
				key.offset = found_key.objectid - 1;
				continue;
			}

		}

		 
		if (!inode || inode->i_nlink) {
			if (inode) {
				ret = btrfs_drop_verity_items(BTRFS_I(inode));
				iput(inode);
				inode = NULL;
				if (ret)
					goto out;
			}
			trans = btrfs_start_transaction(root, 1);
			if (IS_ERR(trans)) {
				ret = PTR_ERR(trans);
				goto out;
			}
			btrfs_debug(fs_info, "auto deleting %Lu",
				    found_key.objectid);
			ret = btrfs_del_orphan_item(trans, root,
						    found_key.objectid);
			btrfs_end_transaction(trans);
			if (ret)
				goto out;
			continue;
		}

		nr_unlink++;

		 
		iput(inode);
	}
	 
	btrfs_release_path(path);

	if (test_bit(BTRFS_ROOT_ORPHAN_ITEM_INSERTED, &root->state)) {
		trans = btrfs_join_transaction(root);
		if (!IS_ERR(trans))
			btrfs_end_transaction(trans);
	}

	if (nr_unlink)
		btrfs_debug(fs_info, "unlinked %d orphans", nr_unlink);

out:
	if (ret)
		btrfs_err(fs_info, "could not do orphan cleanup %d", ret);
	btrfs_free_path(path);
	return ret;
}

 
static noinline int acls_after_inode_item(struct extent_buffer *leaf,
					  int slot, u64 objectid,
					  int *first_xattr_slot)
{
	u32 nritems = btrfs_header_nritems(leaf);
	struct btrfs_key found_key;
	static u64 xattr_access = 0;
	static u64 xattr_default = 0;
	int scanned = 0;

	if (!xattr_access) {
		xattr_access = btrfs_name_hash(XATTR_NAME_POSIX_ACL_ACCESS,
					strlen(XATTR_NAME_POSIX_ACL_ACCESS));
		xattr_default = btrfs_name_hash(XATTR_NAME_POSIX_ACL_DEFAULT,
					strlen(XATTR_NAME_POSIX_ACL_DEFAULT));
	}

	slot++;
	*first_xattr_slot = -1;
	while (slot < nritems) {
		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		 
		if (found_key.objectid != objectid)
			return 0;

		 
		if (found_key.type == BTRFS_XATTR_ITEM_KEY) {
			if (*first_xattr_slot == -1)
				*first_xattr_slot = slot;
			if (found_key.offset == xattr_access ||
			    found_key.offset == xattr_default)
				return 1;
		}

		 
		if (found_key.type > BTRFS_XATTR_ITEM_KEY)
			return 0;

		slot++;
		scanned++;

		 
		if (scanned >= 8)
			break;
	}
	 
	if (*first_xattr_slot == -1)
		*first_xattr_slot = slot;
	return 1;
}

 
static int btrfs_read_locked_inode(struct inode *inode,
				   struct btrfs_path *in_path)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct btrfs_path *path = in_path;
	struct extent_buffer *leaf;
	struct btrfs_inode_item *inode_item;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key location;
	unsigned long ptr;
	int maybe_acls;
	u32 rdev;
	int ret;
	bool filled = false;
	int first_xattr_slot;

	ret = btrfs_fill_inode(inode, &rdev);
	if (!ret)
		filled = true;

	if (!path) {
		path = btrfs_alloc_path();
		if (!path)
			return -ENOMEM;
	}

	memcpy(&location, &BTRFS_I(inode)->location, sizeof(location));

	ret = btrfs_lookup_inode(NULL, root, path, &location, 0);
	if (ret) {
		if (path != in_path)
			btrfs_free_path(path);
		return ret;
	}

	leaf = path->nodes[0];

	if (filled)
		goto cache_index;

	inode_item = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_inode_item);
	inode->i_mode = btrfs_inode_mode(leaf, inode_item);
	set_nlink(inode, btrfs_inode_nlink(leaf, inode_item));
	i_uid_write(inode, btrfs_inode_uid(leaf, inode_item));
	i_gid_write(inode, btrfs_inode_gid(leaf, inode_item));
	btrfs_i_size_write(BTRFS_I(inode), btrfs_inode_size(leaf, inode_item));
	btrfs_inode_set_file_extent_range(BTRFS_I(inode), 0,
			round_up(i_size_read(inode), fs_info->sectorsize));

	inode->i_atime.tv_sec = btrfs_timespec_sec(leaf, &inode_item->atime);
	inode->i_atime.tv_nsec = btrfs_timespec_nsec(leaf, &inode_item->atime);

	inode->i_mtime.tv_sec = btrfs_timespec_sec(leaf, &inode_item->mtime);
	inode->i_mtime.tv_nsec = btrfs_timespec_nsec(leaf, &inode_item->mtime);

	inode_set_ctime(inode, btrfs_timespec_sec(leaf, &inode_item->ctime),
			btrfs_timespec_nsec(leaf, &inode_item->ctime));

	BTRFS_I(inode)->i_otime.tv_sec =
		btrfs_timespec_sec(leaf, &inode_item->otime);
	BTRFS_I(inode)->i_otime.tv_nsec =
		btrfs_timespec_nsec(leaf, &inode_item->otime);

	inode_set_bytes(inode, btrfs_inode_nbytes(leaf, inode_item));
	BTRFS_I(inode)->generation = btrfs_inode_generation(leaf, inode_item);
	BTRFS_I(inode)->last_trans = btrfs_inode_transid(leaf, inode_item);

	inode_set_iversion_queried(inode,
				   btrfs_inode_sequence(leaf, inode_item));
	inode->i_generation = BTRFS_I(inode)->generation;
	inode->i_rdev = 0;
	rdev = btrfs_inode_rdev(leaf, inode_item);

	BTRFS_I(inode)->index_cnt = (u64)-1;
	btrfs_inode_split_flags(btrfs_inode_flags(leaf, inode_item),
				&BTRFS_I(inode)->flags, &BTRFS_I(inode)->ro_flags);

cache_index:
	 
	if (BTRFS_I(inode)->last_trans == fs_info->generation)
		set_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
			&BTRFS_I(inode)->runtime_flags);

	 
	BTRFS_I(inode)->last_unlink_trans = BTRFS_I(inode)->last_trans;

	 
	BTRFS_I(inode)->last_reflink_trans = BTRFS_I(inode)->last_trans;

	path->slots[0]++;
	if (inode->i_nlink != 1 ||
	    path->slots[0] >= btrfs_header_nritems(leaf))
		goto cache_acl;

	btrfs_item_key_to_cpu(leaf, &location, path->slots[0]);
	if (location.objectid != btrfs_ino(BTRFS_I(inode)))
		goto cache_acl;

	ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
	if (location.type == BTRFS_INODE_REF_KEY) {
		struct btrfs_inode_ref *ref;

		ref = (struct btrfs_inode_ref *)ptr;
		BTRFS_I(inode)->dir_index = btrfs_inode_ref_index(leaf, ref);
	} else if (location.type == BTRFS_INODE_EXTREF_KEY) {
		struct btrfs_inode_extref *extref;

		extref = (struct btrfs_inode_extref *)ptr;
		BTRFS_I(inode)->dir_index = btrfs_inode_extref_index(leaf,
								     extref);
	}
cache_acl:
	 
	maybe_acls = acls_after_inode_item(leaf, path->slots[0],
			btrfs_ino(BTRFS_I(inode)), &first_xattr_slot);
	if (first_xattr_slot != -1) {
		path->slots[0] = first_xattr_slot;
		ret = btrfs_load_inode_props(inode, path);
		if (ret)
			btrfs_err(fs_info,
				  "error loading props for ino %llu (root %llu): %d",
				  btrfs_ino(BTRFS_I(inode)),
				  root->root_key.objectid, ret);
	}
	if (path != in_path)
		btrfs_free_path(path);

	if (!maybe_acls)
		cache_no_acl(inode);

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_fop = &btrfs_file_operations;
		inode->i_op = &btrfs_file_inode_operations;
		break;
	case S_IFDIR:
		inode->i_fop = &btrfs_dir_file_operations;
		inode->i_op = &btrfs_dir_inode_operations;
		break;
	case S_IFLNK:
		inode->i_op = &btrfs_symlink_inode_operations;
		inode_nohighmem(inode);
		inode->i_mapping->a_ops = &btrfs_aops;
		break;
	default:
		inode->i_op = &btrfs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, rdev);
		break;
	}

	btrfs_sync_inode_flags_to_i_flags(inode);
	return 0;
}

 
static void fill_inode_item(struct btrfs_trans_handle *trans,
			    struct extent_buffer *leaf,
			    struct btrfs_inode_item *item,
			    struct inode *inode)
{
	struct btrfs_map_token token;
	u64 flags;

	btrfs_init_map_token(&token, leaf);

	btrfs_set_token_inode_uid(&token, item, i_uid_read(inode));
	btrfs_set_token_inode_gid(&token, item, i_gid_read(inode));
	btrfs_set_token_inode_size(&token, item, BTRFS_I(inode)->disk_i_size);
	btrfs_set_token_inode_mode(&token, item, inode->i_mode);
	btrfs_set_token_inode_nlink(&token, item, inode->i_nlink);

	btrfs_set_token_timespec_sec(&token, &item->atime,
				     inode->i_atime.tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->atime,
				      inode->i_atime.tv_nsec);

	btrfs_set_token_timespec_sec(&token, &item->mtime,
				     inode->i_mtime.tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->mtime,
				      inode->i_mtime.tv_nsec);

	btrfs_set_token_timespec_sec(&token, &item->ctime,
				     inode_get_ctime(inode).tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->ctime,
				      inode_get_ctime(inode).tv_nsec);

	btrfs_set_token_timespec_sec(&token, &item->otime,
				     BTRFS_I(inode)->i_otime.tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->otime,
				      BTRFS_I(inode)->i_otime.tv_nsec);

	btrfs_set_token_inode_nbytes(&token, item, inode_get_bytes(inode));
	btrfs_set_token_inode_generation(&token, item,
					 BTRFS_I(inode)->generation);
	btrfs_set_token_inode_sequence(&token, item, inode_peek_iversion(inode));
	btrfs_set_token_inode_transid(&token, item, trans->transid);
	btrfs_set_token_inode_rdev(&token, item, inode->i_rdev);
	flags = btrfs_inode_combine_flags(BTRFS_I(inode)->flags,
					  BTRFS_I(inode)->ro_flags);
	btrfs_set_token_inode_flags(&token, item, flags);
	btrfs_set_token_inode_block_group(&token, item, 0);
}

 
static noinline int btrfs_update_inode_item(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_inode *inode)
{
	struct btrfs_inode_item *inode_item;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_lookup_inode(trans, root, path, &inode->location, 1);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto failed;
	}

	leaf = path->nodes[0];
	inode_item = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_inode_item);

	fill_inode_item(trans, leaf, inode_item, &inode->vfs_inode);
	btrfs_mark_buffer_dirty(trans, leaf);
	btrfs_set_inode_last_trans(trans, inode);
	ret = 0;
failed:
	btrfs_free_path(path);
	return ret;
}

 
noinline int btrfs_update_inode(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_inode *inode)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret;

	 
	if (!btrfs_is_free_space_inode(inode)
	    && !btrfs_is_data_reloc_root(root)
	    && !test_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags)) {
		btrfs_update_root_times(trans, root);

		ret = btrfs_delayed_update_inode(trans, root, inode);
		if (!ret)
			btrfs_set_inode_last_trans(trans, inode);
		return ret;
	}

	return btrfs_update_inode_item(trans, root, inode);
}

int btrfs_update_inode_fallback(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct btrfs_inode *inode)
{
	int ret;

	ret = btrfs_update_inode(trans, root, inode);
	if (ret == -ENOSPC)
		return btrfs_update_inode_item(trans, root, inode);
	return ret;
}

 
static int __btrfs_unlink_inode(struct btrfs_trans_handle *trans,
				struct btrfs_inode *dir,
				struct btrfs_inode *inode,
				const struct fscrypt_str *name,
				struct btrfs_rename_ctx *rename_ctx)
{
	struct btrfs_root *root = dir->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path;
	int ret = 0;
	struct btrfs_dir_item *di;
	u64 index;
	u64 ino = btrfs_ino(inode);
	u64 dir_ino = btrfs_ino(dir);

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	di = btrfs_lookup_dir_item(trans, root, path, dir_ino, name, -1);
	if (IS_ERR_OR_NULL(di)) {
		ret = di ? PTR_ERR(di) : -ENOENT;
		goto err;
	}
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	if (ret)
		goto err;
	btrfs_release_path(path);

	 
	if (inode->dir_index) {
		ret = btrfs_delayed_delete_inode_ref(inode);
		if (!ret) {
			index = inode->dir_index;
			goto skip_backref;
		}
	}

	ret = btrfs_del_inode_ref(trans, root, name, ino, dir_ino, &index);
	if (ret) {
		btrfs_info(fs_info,
			"failed to delete reference to %.*s, inode %llu parent %llu",
			name->len, name->name, ino, dir_ino);
		btrfs_abort_transaction(trans, ret);
		goto err;
	}
skip_backref:
	if (rename_ctx)
		rename_ctx->index = index;

	ret = btrfs_delete_delayed_dir_index(trans, dir, index);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto err;
	}

	 
	if (!rename_ctx) {
		btrfs_del_inode_ref_in_log(trans, root, name, inode, dir_ino);
		btrfs_del_dir_entries_in_log(trans, root, name, dir, index);
	}

	 
	btrfs_run_delayed_iput(fs_info, inode);
err:
	btrfs_free_path(path);
	if (ret)
		goto out;

	btrfs_i_size_write(dir, dir->vfs_inode.i_size - name->len * 2);
	inode_inc_iversion(&inode->vfs_inode);
	inode_inc_iversion(&dir->vfs_inode);
	inode_set_ctime_current(&inode->vfs_inode);
	dir->vfs_inode.i_mtime = inode_set_ctime_current(&dir->vfs_inode);
	ret = btrfs_update_inode(trans, root, dir);
out:
	return ret;
}

int btrfs_unlink_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_inode *dir, struct btrfs_inode *inode,
		       const struct fscrypt_str *name)
{
	int ret;

	ret = __btrfs_unlink_inode(trans, dir, inode, name, NULL);
	if (!ret) {
		drop_nlink(&inode->vfs_inode);
		ret = btrfs_update_inode(trans, inode->root, inode);
	}
	return ret;
}

 
static struct btrfs_trans_handle *__unlink_start_trans(struct btrfs_inode *dir)
{
	struct btrfs_root *root = dir->root;

	return btrfs_start_transaction_fallback_global_rsv(root,
						   BTRFS_UNLINK_METADATA_UNITS);
}

static int btrfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct btrfs_trans_handle *trans;
	struct inode *inode = d_inode(dentry);
	int ret;
	struct fscrypt_name fname;

	ret = fscrypt_setup_filename(dir, &dentry->d_name, 1, &fname);
	if (ret)
		return ret;

	 

	trans = __unlink_start_trans(BTRFS_I(dir));
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto fscrypt_free;
	}

	btrfs_record_unlink_dir(trans, BTRFS_I(dir), BTRFS_I(d_inode(dentry)),
				false);

	ret = btrfs_unlink_inode(trans, BTRFS_I(dir), BTRFS_I(d_inode(dentry)),
				 &fname.disk_name);
	if (ret)
		goto end_trans;

	if (inode->i_nlink == 0) {
		ret = btrfs_orphan_add(trans, BTRFS_I(inode));
		if (ret)
			goto end_trans;
	}

end_trans:
	btrfs_end_transaction(trans);
	btrfs_btree_balance_dirty(BTRFS_I(dir)->root->fs_info);
fscrypt_free:
	fscrypt_free_filename(&fname);
	return ret;
}

static int btrfs_unlink_subvol(struct btrfs_trans_handle *trans,
			       struct btrfs_inode *dir, struct dentry *dentry)
{
	struct btrfs_root *root = dir->root;
	struct btrfs_inode *inode = BTRFS_I(d_inode(dentry));
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	u64 index;
	int ret;
	u64 objectid;
	u64 dir_ino = btrfs_ino(dir);
	struct fscrypt_name fname;

	ret = fscrypt_setup_filename(&dir->vfs_inode, &dentry->d_name, 1, &fname);
	if (ret)
		return ret;

	 

	if (btrfs_ino(inode) == BTRFS_FIRST_FREE_OBJECTID) {
		objectid = inode->root->root_key.objectid;
	} else if (btrfs_ino(inode) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID) {
		objectid = inode->location.objectid;
	} else {
		WARN_ON(1);
		fscrypt_free_filename(&fname);
		return -EINVAL;
	}

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	di = btrfs_lookup_dir_item(trans, root, path, dir_ino,
				   &fname.disk_name, -1);
	if (IS_ERR_OR_NULL(di)) {
		ret = di ? PTR_ERR(di) : -ENOENT;
		goto out;
	}

	leaf = path->nodes[0];
	btrfs_dir_item_key_to_cpu(leaf, di, &key);
	WARN_ON(key.type != BTRFS_ROOT_ITEM_KEY || key.objectid != objectid);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
	btrfs_release_path(path);

	 
	if (btrfs_ino(inode) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID) {
		di = btrfs_search_dir_index_item(root, path, dir_ino, &fname.disk_name);
		if (IS_ERR_OR_NULL(di)) {
			if (!di)
				ret = -ENOENT;
			else
				ret = PTR_ERR(di);
			btrfs_abort_transaction(trans, ret);
			goto out;
		}

		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		index = key.offset;
		btrfs_release_path(path);
	} else {
		ret = btrfs_del_root_ref(trans, objectid,
					 root->root_key.objectid, dir_ino,
					 &index, &fname.disk_name);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out;
		}
	}

	ret = btrfs_delete_delayed_dir_index(trans, dir, index);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}

	btrfs_i_size_write(dir, dir->vfs_inode.i_size - fname.disk_name.len * 2);
	inode_inc_iversion(&dir->vfs_inode);
	dir->vfs_inode.i_mtime = inode_set_ctime_current(&dir->vfs_inode);
	ret = btrfs_update_inode_fallback(trans, root, dir);
	if (ret)
		btrfs_abort_transaction(trans, ret);
out:
	btrfs_free_path(path);
	fscrypt_free_filename(&fname);
	return ret;
}

 
static noinline int may_destroy_subvol(struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	struct fscrypt_str name = FSTR_INIT("default", 7);
	u64 dir_id;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	 
	dir_id = btrfs_super_root_dir(fs_info->super_copy);
	di = btrfs_lookup_dir_item(NULL, fs_info->tree_root, path,
				   dir_id, &name, 0);
	if (di && !IS_ERR(di)) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &key);
		if (key.objectid == root->root_key.objectid) {
			ret = -EPERM;
			btrfs_err(fs_info,
				  "deleting default subvolume %llu is not allowed",
				  key.objectid);
			goto out;
		}
		btrfs_release_path(path);
	}

	key.objectid = root->root_key.objectid;
	key.type = BTRFS_ROOT_REF_KEY;
	key.offset = (u64)-1;

	ret = btrfs_search_slot(NULL, fs_info->tree_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	BUG_ON(ret == 0);

	ret = 0;
	if (path->slots[0] > 0) {
		path->slots[0]--;
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.objectid == root->root_key.objectid &&
		    key.type == BTRFS_ROOT_REF_KEY)
			ret = -ENOTEMPTY;
	}
out:
	btrfs_free_path(path);
	return ret;
}

 
static void btrfs_prune_dentries(struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct rb_node *node;
	struct rb_node *prev;
	struct btrfs_inode *entry;
	struct inode *inode;
	u64 objectid = 0;

	if (!BTRFS_FS_ERROR(fs_info))
		WARN_ON(btrfs_root_refs(&root->root_item) != 0);

	spin_lock(&root->inode_lock);
again:
	node = root->inode_tree.rb_node;
	prev = NULL;
	while (node) {
		prev = node;
		entry = rb_entry(node, struct btrfs_inode, rb_node);

		if (objectid < btrfs_ino(entry))
			node = node->rb_left;
		else if (objectid > btrfs_ino(entry))
			node = node->rb_right;
		else
			break;
	}
	if (!node) {
		while (prev) {
			entry = rb_entry(prev, struct btrfs_inode, rb_node);
			if (objectid <= btrfs_ino(entry)) {
				node = prev;
				break;
			}
			prev = rb_next(prev);
		}
	}
	while (node) {
		entry = rb_entry(node, struct btrfs_inode, rb_node);
		objectid = btrfs_ino(entry) + 1;
		inode = igrab(&entry->vfs_inode);
		if (inode) {
			spin_unlock(&root->inode_lock);
			if (atomic_read(&inode->i_count) > 1)
				d_prune_aliases(inode);
			 
			iput(inode);
			cond_resched();
			spin_lock(&root->inode_lock);
			goto again;
		}

		if (cond_resched_lock(&root->inode_lock))
			goto again;

		node = rb_next(node);
	}
	spin_unlock(&root->inode_lock);
}

int btrfs_delete_subvolume(struct btrfs_inode *dir, struct dentry *dentry)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(dentry->d_sb);
	struct btrfs_root *root = dir->root;
	struct inode *inode = d_inode(dentry);
	struct btrfs_root *dest = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct btrfs_block_rsv block_rsv;
	u64 root_flags;
	int ret;

	 
	spin_lock(&dest->root_item_lock);
	if (dest->send_in_progress) {
		spin_unlock(&dest->root_item_lock);
		btrfs_warn(fs_info,
			   "attempt to delete subvolume %llu during send",
			   dest->root_key.objectid);
		return -EPERM;
	}
	if (atomic_read(&dest->nr_swapfiles)) {
		spin_unlock(&dest->root_item_lock);
		btrfs_warn(fs_info,
			   "attempt to delete subvolume %llu with active swapfile",
			   root->root_key.objectid);
		return -EPERM;
	}
	root_flags = btrfs_root_flags(&dest->root_item);
	btrfs_set_root_flags(&dest->root_item,
			     root_flags | BTRFS_ROOT_SUBVOL_DEAD);
	spin_unlock(&dest->root_item_lock);

	down_write(&fs_info->subvol_sem);

	ret = may_destroy_subvol(dest);
	if (ret)
		goto out_up_write;

	btrfs_init_block_rsv(&block_rsv, BTRFS_BLOCK_RSV_TEMP);
	 
	ret = btrfs_subvolume_reserve_metadata(root, &block_rsv, 5, true);
	if (ret)
		goto out_up_write;

	trans = btrfs_start_transaction(root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_release;
	}
	trans->block_rsv = &block_rsv;
	trans->bytes_reserved = block_rsv.size;

	btrfs_record_snapshot_destroy(trans, dir);

	ret = btrfs_unlink_subvol(trans, dir, dentry);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_end_trans;
	}

	ret = btrfs_record_root_in_trans(trans, dest);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_end_trans;
	}

	memset(&dest->root_item.drop_progress, 0,
		sizeof(dest->root_item.drop_progress));
	btrfs_set_root_drop_level(&dest->root_item, 0);
	btrfs_set_root_refs(&dest->root_item, 0);

	if (!test_and_set_bit(BTRFS_ROOT_ORPHAN_ITEM_INSERTED, &dest->state)) {
		ret = btrfs_insert_orphan_item(trans,
					fs_info->tree_root,
					dest->root_key.objectid);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out_end_trans;
		}
	}

	ret = btrfs_uuid_tree_remove(trans, dest->root_item.uuid,
				  BTRFS_UUID_KEY_SUBVOL,
				  dest->root_key.objectid);
	if (ret && ret != -ENOENT) {
		btrfs_abort_transaction(trans, ret);
		goto out_end_trans;
	}
	if (!btrfs_is_empty_uuid(dest->root_item.received_uuid)) {
		ret = btrfs_uuid_tree_remove(trans,
					  dest->root_item.received_uuid,
					  BTRFS_UUID_KEY_RECEIVED_SUBVOL,
					  dest->root_key.objectid);
		if (ret && ret != -ENOENT) {
			btrfs_abort_transaction(trans, ret);
			goto out_end_trans;
		}
	}

	free_anon_bdev(dest->anon_dev);
	dest->anon_dev = 0;
out_end_trans:
	trans->block_rsv = NULL;
	trans->bytes_reserved = 0;
	ret = btrfs_end_transaction(trans);
	inode->i_flags |= S_DEAD;
out_release:
	btrfs_subvolume_release_metadata(root, &block_rsv);
out_up_write:
	up_write(&fs_info->subvol_sem);
	if (ret) {
		spin_lock(&dest->root_item_lock);
		root_flags = btrfs_root_flags(&dest->root_item);
		btrfs_set_root_flags(&dest->root_item,
				root_flags & ~BTRFS_ROOT_SUBVOL_DEAD);
		spin_unlock(&dest->root_item_lock);
	} else {
		d_invalidate(dentry);
		btrfs_prune_dentries(dest);
		ASSERT(dest->send_in_progress == 0);
	}

	return ret;
}

static int btrfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct btrfs_fs_info *fs_info = BTRFS_I(inode)->root->fs_info;
	int err = 0;
	struct btrfs_trans_handle *trans;
	u64 last_unlink_trans;
	struct fscrypt_name fname;

	if (inode->i_size > BTRFS_EMPTY_DIR_SIZE)
		return -ENOTEMPTY;
	if (btrfs_ino(BTRFS_I(inode)) == BTRFS_FIRST_FREE_OBJECTID) {
		if (unlikely(btrfs_fs_incompat(fs_info, EXTENT_TREE_V2))) {
			btrfs_err(fs_info,
			"extent tree v2 doesn't support snapshot deletion yet");
			return -EOPNOTSUPP;
		}
		return btrfs_delete_subvolume(BTRFS_I(dir), dentry);
	}

	err = fscrypt_setup_filename(dir, &dentry->d_name, 1, &fname);
	if (err)
		return err;

	 

	trans = __unlink_start_trans(BTRFS_I(dir));
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out_notrans;
	}

	if (unlikely(btrfs_ino(BTRFS_I(inode)) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)) {
		err = btrfs_unlink_subvol(trans, BTRFS_I(dir), dentry);
		goto out;
	}

	err = btrfs_orphan_add(trans, BTRFS_I(inode));
	if (err)
		goto out;

	last_unlink_trans = BTRFS_I(inode)->last_unlink_trans;

	 
	err = btrfs_unlink_inode(trans, BTRFS_I(dir), BTRFS_I(d_inode(dentry)),
				 &fname.disk_name);
	if (!err) {
		btrfs_i_size_write(BTRFS_I(inode), 0);
		 
		if (last_unlink_trans >= trans->transid)
			BTRFS_I(dir)->last_unlink_trans = last_unlink_trans;
	}
out:
	btrfs_end_transaction(trans);
out_notrans:
	btrfs_btree_balance_dirty(fs_info);
	fscrypt_free_filename(&fname);

	return err;
}

 
int btrfs_truncate_block(struct btrfs_inode *inode, loff_t from, loff_t len,
			 int front)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct address_space *mapping = inode->vfs_inode.i_mapping;
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	struct extent_changeset *data_reserved = NULL;
	bool only_release_metadata = false;
	u32 blocksize = fs_info->sectorsize;
	pgoff_t index = from >> PAGE_SHIFT;
	unsigned offset = from & (blocksize - 1);
	struct page *page;
	gfp_t mask = btrfs_alloc_write_mask(mapping);
	size_t write_bytes = blocksize;
	int ret = 0;
	u64 block_start;
	u64 block_end;

	if (IS_ALIGNED(offset, blocksize) &&
	    (!len || IS_ALIGNED(len, blocksize)))
		goto out;

	block_start = round_down(from, blocksize);
	block_end = block_start + blocksize - 1;

	ret = btrfs_check_data_free_space(inode, &data_reserved, block_start,
					  blocksize, false);
	if (ret < 0) {
		if (btrfs_check_nocow_lock(inode, block_start, &write_bytes, false) > 0) {
			 
			only_release_metadata = true;
		} else {
			goto out;
		}
	}
	ret = btrfs_delalloc_reserve_metadata(inode, blocksize, blocksize, false);
	if (ret < 0) {
		if (!only_release_metadata)
			btrfs_free_reserved_data_space(inode, data_reserved,
						       block_start, blocksize);
		goto out;
	}
again:
	page = find_or_create_page(mapping, index, mask);
	if (!page) {
		btrfs_delalloc_release_space(inode, data_reserved, block_start,
					     blocksize, true);
		btrfs_delalloc_release_extents(inode, blocksize);
		ret = -ENOMEM;
		goto out;
	}

	if (!PageUptodate(page)) {
		ret = btrfs_read_folio(NULL, page_folio(page));
		lock_page(page);
		if (page->mapping != mapping) {
			unlock_page(page);
			put_page(page);
			goto again;
		}
		if (!PageUptodate(page)) {
			ret = -EIO;
			goto out_unlock;
		}
	}

	 
	ret = set_page_extent_mapped(page);
	if (ret < 0)
		goto out_unlock;

	wait_on_page_writeback(page);

	lock_extent(io_tree, block_start, block_end, &cached_state);

	ordered = btrfs_lookup_ordered_extent(inode, block_start);
	if (ordered) {
		unlock_extent(io_tree, block_start, block_end, &cached_state);
		unlock_page(page);
		put_page(page);
		btrfs_start_ordered_extent(ordered);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	clear_extent_bit(&inode->io_tree, block_start, block_end,
			 EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING | EXTENT_DEFRAG,
			 &cached_state);

	ret = btrfs_set_extent_delalloc(inode, block_start, block_end, 0,
					&cached_state);
	if (ret) {
		unlock_extent(io_tree, block_start, block_end, &cached_state);
		goto out_unlock;
	}

	if (offset != blocksize) {
		if (!len)
			len = blocksize - offset;
		if (front)
			memzero_page(page, (block_start - page_offset(page)),
				     offset);
		else
			memzero_page(page, (block_start - page_offset(page)) + offset,
				     len);
	}
	btrfs_page_clear_checked(fs_info, page, block_start,
				 block_end + 1 - block_start);
	btrfs_page_set_dirty(fs_info, page, block_start, block_end + 1 - block_start);
	unlock_extent(io_tree, block_start, block_end, &cached_state);

	if (only_release_metadata)
		set_extent_bit(&inode->io_tree, block_start, block_end,
			       EXTENT_NORESERVE, NULL);

out_unlock:
	if (ret) {
		if (only_release_metadata)
			btrfs_delalloc_release_metadata(inode, blocksize, true);
		else
			btrfs_delalloc_release_space(inode, data_reserved,
					block_start, blocksize, true);
	}
	btrfs_delalloc_release_extents(inode, blocksize);
	unlock_page(page);
	put_page(page);
out:
	if (only_release_metadata)
		btrfs_check_nocow_unlock(inode);
	extent_changeset_free(data_reserved);
	return ret;
}

static int maybe_insert_hole(struct btrfs_root *root, struct btrfs_inode *inode,
			     u64 offset, u64 len)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans;
	struct btrfs_drop_extents_args drop_args = { 0 };
	int ret;

	 
	if (btrfs_fs_incompat(fs_info, NO_HOLES))
		return 0;

	 
	trans = btrfs_start_transaction(root, 3);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	drop_args.start = offset;
	drop_args.end = offset + len;
	drop_args.drop_cache = true;

	ret = btrfs_drop_extents(trans, root, inode, &drop_args);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		btrfs_end_transaction(trans);
		return ret;
	}

	ret = btrfs_insert_hole_extent(trans, root, btrfs_ino(inode), offset, len);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
	} else {
		btrfs_update_inode_bytes(inode, 0, drop_args.bytes_found);
		btrfs_update_inode(trans, root, inode);
	}
	btrfs_end_transaction(trans);
	return ret;
}

 
int btrfs_cont_expand(struct btrfs_inode *inode, loff_t oldsize, loff_t size)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct extent_map *em = NULL;
	struct extent_state *cached_state = NULL;
	u64 hole_start = ALIGN(oldsize, fs_info->sectorsize);
	u64 block_end = ALIGN(size, fs_info->sectorsize);
	u64 last_byte;
	u64 cur_offset;
	u64 hole_size;
	int err = 0;

	 
	err = btrfs_truncate_block(inode, oldsize, 0, 0);
	if (err)
		return err;

	if (size <= hole_start)
		return 0;

	btrfs_lock_and_flush_ordered_range(inode, hole_start, block_end - 1,
					   &cached_state);
	cur_offset = hole_start;
	while (1) {
		em = btrfs_get_extent(inode, NULL, 0, cur_offset,
				      block_end - cur_offset);
		if (IS_ERR(em)) {
			err = PTR_ERR(em);
			em = NULL;
			break;
		}
		last_byte = min(extent_map_end(em), block_end);
		last_byte = ALIGN(last_byte, fs_info->sectorsize);
		hole_size = last_byte - cur_offset;

		if (!test_bit(EXTENT_FLAG_PREALLOC, &em->flags)) {
			struct extent_map *hole_em;

			err = maybe_insert_hole(root, inode, cur_offset,
						hole_size);
			if (err)
				break;

			err = btrfs_inode_set_file_extent_range(inode,
							cur_offset, hole_size);
			if (err)
				break;

			hole_em = alloc_extent_map();
			if (!hole_em) {
				btrfs_drop_extent_map_range(inode, cur_offset,
						    cur_offset + hole_size - 1,
						    false);
				btrfs_set_inode_full_sync(inode);
				goto next;
			}
			hole_em->start = cur_offset;
			hole_em->len = hole_size;
			hole_em->orig_start = cur_offset;

			hole_em->block_start = EXTENT_MAP_HOLE;
			hole_em->block_len = 0;
			hole_em->orig_block_len = 0;
			hole_em->ram_bytes = hole_size;
			hole_em->compress_type = BTRFS_COMPRESS_NONE;
			hole_em->generation = fs_info->generation;

			err = btrfs_replace_extent_map_range(inode, hole_em, true);
			free_extent_map(hole_em);
		} else {
			err = btrfs_inode_set_file_extent_range(inode,
							cur_offset, hole_size);
			if (err)
				break;
		}
next:
		free_extent_map(em);
		em = NULL;
		cur_offset = last_byte;
		if (cur_offset >= block_end)
			break;
	}
	free_extent_map(em);
	unlock_extent(io_tree, hole_start, block_end - 1, &cached_state);
	return err;
}

static int btrfs_setsize(struct inode *inode, struct iattr *attr)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	loff_t oldsize = i_size_read(inode);
	loff_t newsize = attr->ia_size;
	int mask = attr->ia_valid;
	int ret;

	 
	if (newsize != oldsize) {
		inode_inc_iversion(inode);
		if (!(mask & (ATTR_CTIME | ATTR_MTIME))) {
			inode->i_mtime = inode_set_ctime_current(inode);
		}
	}

	if (newsize > oldsize) {
		 
		btrfs_drew_write_lock(&root->snapshot_lock);
		ret = btrfs_cont_expand(BTRFS_I(inode), oldsize, newsize);
		if (ret) {
			btrfs_drew_write_unlock(&root->snapshot_lock);
			return ret;
		}

		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans)) {
			btrfs_drew_write_unlock(&root->snapshot_lock);
			return PTR_ERR(trans);
		}

		i_size_write(inode, newsize);
		btrfs_inode_safe_disk_i_size_write(BTRFS_I(inode), 0);
		pagecache_isize_extended(inode, oldsize, newsize);
		ret = btrfs_update_inode(trans, root, BTRFS_I(inode));
		btrfs_drew_write_unlock(&root->snapshot_lock);
		btrfs_end_transaction(trans);
	} else {
		struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);

		if (btrfs_is_zoned(fs_info)) {
			ret = btrfs_wait_ordered_range(inode,
					ALIGN(newsize, fs_info->sectorsize),
					(u64)-1);
			if (ret)
				return ret;
		}

		 
		if (newsize == 0)
			set_bit(BTRFS_INODE_FLUSH_ON_CLOSE,
				&BTRFS_I(inode)->runtime_flags);

		truncate_setsize(inode, newsize);

		inode_dio_wait(inode);

		ret = btrfs_truncate(BTRFS_I(inode), newsize == oldsize);
		if (ret && inode->i_nlink) {
			int err;

			 
			err = btrfs_wait_ordered_range(inode, 0, (u64)-1);
			if (err)
				return err;
			i_size_write(inode, BTRFS_I(inode)->disk_i_size);
		}
	}

	return ret;
}

static int btrfs_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
			 struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int err;

	if (btrfs_root_readonly(root))
		return -EROFS;

	err = setattr_prepare(idmap, dentry, attr);
	if (err)
		return err;

	if (S_ISREG(inode->i_mode) && (attr->ia_valid & ATTR_SIZE)) {
		err = btrfs_setsize(inode, attr);
		if (err)
			return err;
	}

	if (attr->ia_valid) {
		setattr_copy(idmap, inode, attr);
		inode_inc_iversion(inode);
		err = btrfs_dirty_inode(BTRFS_I(inode));

		if (!err && attr->ia_valid & ATTR_MODE)
			err = posix_acl_chmod(idmap, dentry, inode->i_mode);
	}

	return err;
}

 
static void evict_inode_truncate_pages(struct inode *inode)
{
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct rb_node *node;

	ASSERT(inode->i_state & I_FREEING);
	truncate_inode_pages_final(&inode->i_data);

	btrfs_drop_extent_map_range(BTRFS_I(inode), 0, (u64)-1, false);

	 
	spin_lock(&io_tree->lock);
	while (!RB_EMPTY_ROOT(&io_tree->state)) {
		struct extent_state *state;
		struct extent_state *cached_state = NULL;
		u64 start;
		u64 end;
		unsigned state_flags;

		node = rb_first(&io_tree->state);
		state = rb_entry(node, struct extent_state, rb_node);
		start = state->start;
		end = state->end;
		state_flags = state->state;
		spin_unlock(&io_tree->lock);

		lock_extent(io_tree, start, end, &cached_state);

		 
		if (state_flags & EXTENT_DELALLOC)
			btrfs_qgroup_free_data(BTRFS_I(inode), NULL, start,
					       end - start + 1, NULL);

		clear_extent_bit(io_tree, start, end,
				 EXTENT_CLEAR_ALL_BITS | EXTENT_DO_ACCOUNTING,
				 &cached_state);

		cond_resched();
		spin_lock(&io_tree->lock);
	}
	spin_unlock(&io_tree->lock);
}

static struct btrfs_trans_handle *evict_refill_and_join(struct btrfs_root *root,
							struct btrfs_block_rsv *rsv)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans;
	u64 delayed_refs_extra = btrfs_calc_delayed_ref_bytes(fs_info, 1);
	int ret;

	 
	ret = btrfs_block_rsv_refill(fs_info, rsv, rsv->size + delayed_refs_extra,
				     BTRFS_RESERVE_FLUSH_EVICT);
	if (ret) {
		ret = btrfs_block_rsv_refill(fs_info, rsv, rsv->size,
					     BTRFS_RESERVE_FLUSH_EVICT);
		if (ret) {
			btrfs_warn(fs_info,
				   "could not allocate space for delete; will truncate on mount");
			return ERR_PTR(-ENOSPC);
		}
		delayed_refs_extra = 0;
	}

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return trans;

	if (delayed_refs_extra) {
		trans->block_rsv = &fs_info->trans_block_rsv;
		trans->bytes_reserved = delayed_refs_extra;
		btrfs_block_rsv_migrate(rsv, trans->block_rsv,
					delayed_refs_extra, true);
	}
	return trans;
}

void btrfs_evict_inode(struct inode *inode)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_block_rsv *rsv = NULL;
	int ret;

	trace_btrfs_inode_evict(inode);

	if (!root) {
		fsverity_cleanup_inode(inode);
		clear_inode(inode);
		return;
	}

	evict_inode_truncate_pages(inode);

	if (inode->i_nlink &&
	    ((btrfs_root_refs(&root->root_item) != 0 &&
	      root->root_key.objectid != BTRFS_ROOT_TREE_OBJECTID) ||
	     btrfs_is_free_space_inode(BTRFS_I(inode))))
		goto out;

	if (is_bad_inode(inode))
		goto out;

	if (test_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags))
		goto out;

	if (inode->i_nlink > 0) {
		BUG_ON(btrfs_root_refs(&root->root_item) != 0 &&
		       root->root_key.objectid != BTRFS_ROOT_TREE_OBJECTID);
		goto out;
	}

	 
	ret = btrfs_commit_inode_delayed_inode(BTRFS_I(inode));
	if (ret)
		goto out;

	 
	btrfs_kill_delayed_inode_items(BTRFS_I(inode));

	rsv = btrfs_alloc_block_rsv(fs_info, BTRFS_BLOCK_RSV_TEMP);
	if (!rsv)
		goto out;
	rsv->size = btrfs_calc_metadata_size(fs_info, 1);
	rsv->failfast = true;

	btrfs_i_size_write(BTRFS_I(inode), 0);

	while (1) {
		struct btrfs_truncate_control control = {
			.inode = BTRFS_I(inode),
			.ino = btrfs_ino(BTRFS_I(inode)),
			.new_size = 0,
			.min_type = 0,
		};

		trans = evict_refill_and_join(root, rsv);
		if (IS_ERR(trans))
			goto out;

		trans->block_rsv = rsv;

		ret = btrfs_truncate_inode_items(trans, root, &control);
		trans->block_rsv = &fs_info->trans_block_rsv;
		btrfs_end_transaction(trans);
		 
		btrfs_btree_balance_dirty_nodelay(fs_info);
		if (ret && ret != -ENOSPC && ret != -EAGAIN)
			goto out;
		else if (!ret)
			break;
	}

	 
	trans = evict_refill_and_join(root, rsv);
	if (!IS_ERR(trans)) {
		trans->block_rsv = rsv;
		btrfs_orphan_del(trans, BTRFS_I(inode));
		trans->block_rsv = &fs_info->trans_block_rsv;
		btrfs_end_transaction(trans);
	}

out:
	btrfs_free_block_rsv(fs_info, rsv);
	 
	btrfs_remove_delayed_node(BTRFS_I(inode));
	fsverity_cleanup_inode(inode);
	clear_inode(inode);
}

 
static int btrfs_inode_by_name(struct btrfs_inode *dir, struct dentry *dentry,
			       struct btrfs_key *location, u8 *type)
{
	struct btrfs_dir_item *di;
	struct btrfs_path *path;
	struct btrfs_root *root = dir->root;
	int ret = 0;
	struct fscrypt_name fname;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = fscrypt_setup_filename(&dir->vfs_inode, &dentry->d_name, 1, &fname);
	if (ret < 0)
		goto out;
	 
	ASSERT(ret == 0);

	 

	di = btrfs_lookup_dir_item(NULL, root, path, btrfs_ino(dir),
				   &fname.disk_name, 0);
	if (IS_ERR_OR_NULL(di)) {
		ret = di ? PTR_ERR(di) : -ENOENT;
		goto out;
	}

	btrfs_dir_item_key_to_cpu(path->nodes[0], di, location);
	if (location->type != BTRFS_INODE_ITEM_KEY &&
	    location->type != BTRFS_ROOT_ITEM_KEY) {
		ret = -EUCLEAN;
		btrfs_warn(root->fs_info,
"%s gets something invalid in DIR_ITEM (name %s, directory ino %llu, location(%llu %u %llu))",
			   __func__, fname.disk_name.name, btrfs_ino(dir),
			   location->objectid, location->type, location->offset);
	}
	if (!ret)
		*type = btrfs_dir_ftype(path->nodes[0], di);
out:
	fscrypt_free_filename(&fname);
	btrfs_free_path(path);
	return ret;
}

 
static int fixup_tree_root_location(struct btrfs_fs_info *fs_info,
				    struct btrfs_inode *dir,
				    struct dentry *dentry,
				    struct btrfs_key *location,
				    struct btrfs_root **sub_root)
{
	struct btrfs_path *path;
	struct btrfs_root *new_root;
	struct btrfs_root_ref *ref;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	int ret;
	int err = 0;
	struct fscrypt_name fname;

	ret = fscrypt_setup_filename(&dir->vfs_inode, &dentry->d_name, 0, &fname);
	if (ret)
		return ret;

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		goto out;
	}

	err = -ENOENT;
	key.objectid = dir->root->root_key.objectid;
	key.type = BTRFS_ROOT_REF_KEY;
	key.offset = location->objectid;

	ret = btrfs_search_slot(NULL, fs_info->tree_root, &key, path, 0, 0);
	if (ret) {
		if (ret < 0)
			err = ret;
		goto out;
	}

	leaf = path->nodes[0];
	ref = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_root_ref);
	if (btrfs_root_ref_dirid(leaf, ref) != btrfs_ino(dir) ||
	    btrfs_root_ref_name_len(leaf, ref) != fname.disk_name.len)
		goto out;

	ret = memcmp_extent_buffer(leaf, fname.disk_name.name,
				   (unsigned long)(ref + 1), fname.disk_name.len);
	if (ret)
		goto out;

	btrfs_release_path(path);

	new_root = btrfs_get_fs_root(fs_info, location->objectid, true);
	if (IS_ERR(new_root)) {
		err = PTR_ERR(new_root);
		goto out;
	}

	*sub_root = new_root;
	location->objectid = btrfs_root_dirid(&new_root->root_item);
	location->type = BTRFS_INODE_ITEM_KEY;
	location->offset = 0;
	err = 0;
out:
	btrfs_free_path(path);
	fscrypt_free_filename(&fname);
	return err;
}

static void inode_tree_add(struct btrfs_inode *inode)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_inode *entry;
	struct rb_node **p;
	struct rb_node *parent;
	struct rb_node *new = &inode->rb_node;
	u64 ino = btrfs_ino(inode);

	if (inode_unhashed(&inode->vfs_inode))
		return;
	parent = NULL;
	spin_lock(&root->inode_lock);
	p = &root->inode_tree.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct btrfs_inode, rb_node);

		if (ino < btrfs_ino(entry))
			p = &parent->rb_left;
		else if (ino > btrfs_ino(entry))
			p = &parent->rb_right;
		else {
			WARN_ON(!(entry->vfs_inode.i_state &
				  (I_WILL_FREE | I_FREEING)));
			rb_replace_node(parent, new, &root->inode_tree);
			RB_CLEAR_NODE(parent);
			spin_unlock(&root->inode_lock);
			return;
		}
	}
	rb_link_node(new, parent, p);
	rb_insert_color(new, &root->inode_tree);
	spin_unlock(&root->inode_lock);
}

static void inode_tree_del(struct btrfs_inode *inode)
{
	struct btrfs_root *root = inode->root;
	int empty = 0;

	spin_lock(&root->inode_lock);
	if (!RB_EMPTY_NODE(&inode->rb_node)) {
		rb_erase(&inode->rb_node, &root->inode_tree);
		RB_CLEAR_NODE(&inode->rb_node);
		empty = RB_EMPTY_ROOT(&root->inode_tree);
	}
	spin_unlock(&root->inode_lock);

	if (empty && btrfs_root_refs(&root->root_item) == 0) {
		spin_lock(&root->inode_lock);
		empty = RB_EMPTY_ROOT(&root->inode_tree);
		spin_unlock(&root->inode_lock);
		if (empty)
			btrfs_add_dead_root(root);
	}
}


static int btrfs_init_locked_inode(struct inode *inode, void *p)
{
	struct btrfs_iget_args *args = p;

	inode->i_ino = args->ino;
	BTRFS_I(inode)->location.objectid = args->ino;
	BTRFS_I(inode)->location.type = BTRFS_INODE_ITEM_KEY;
	BTRFS_I(inode)->location.offset = 0;
	BTRFS_I(inode)->root = btrfs_grab_root(args->root);
	BUG_ON(args->root && !BTRFS_I(inode)->root);

	if (args->root && args->root == args->root->fs_info->tree_root &&
	    args->ino != BTRFS_BTREE_INODE_OBJECTID)
		set_bit(BTRFS_INODE_FREE_SPACE_INODE,
			&BTRFS_I(inode)->runtime_flags);
	return 0;
}

static int btrfs_find_actor(struct inode *inode, void *opaque)
{
	struct btrfs_iget_args *args = opaque;

	return args->ino == BTRFS_I(inode)->location.objectid &&
		args->root == BTRFS_I(inode)->root;
}

static struct inode *btrfs_iget_locked(struct super_block *s, u64 ino,
				       struct btrfs_root *root)
{
	struct inode *inode;
	struct btrfs_iget_args args;
	unsigned long hashval = btrfs_inode_hash(ino, root);

	args.ino = ino;
	args.root = root;

	inode = iget5_locked(s, hashval, btrfs_find_actor,
			     btrfs_init_locked_inode,
			     (void *)&args);
	return inode;
}

 
struct inode *btrfs_iget_path(struct super_block *s, u64 ino,
			      struct btrfs_root *root, struct btrfs_path *path)
{
	struct inode *inode;

	inode = btrfs_iget_locked(s, ino, root);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		int ret;

		ret = btrfs_read_locked_inode(inode, path);
		if (!ret) {
			inode_tree_add(BTRFS_I(inode));
			unlock_new_inode(inode);
		} else {
			iget_failed(inode);
			 
			if (ret > 0)
				ret = -ENOENT;
			inode = ERR_PTR(ret);
		}
	}

	return inode;
}

struct inode *btrfs_iget(struct super_block *s, u64 ino, struct btrfs_root *root)
{
	return btrfs_iget_path(s, ino, root, NULL);
}

static struct inode *new_simple_dir(struct inode *dir,
				    struct btrfs_key *key,
				    struct btrfs_root *root)
{
	struct inode *inode = new_inode(dir->i_sb);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	BTRFS_I(inode)->root = btrfs_grab_root(root);
	memcpy(&BTRFS_I(inode)->location, key, sizeof(*key));
	set_bit(BTRFS_INODE_DUMMY, &BTRFS_I(inode)->runtime_flags);

	inode->i_ino = BTRFS_EMPTY_SUBVOL_DIR_OBJECTID;
	 
	inode->i_op = &simple_dir_inode_operations;
	inode->i_opflags &= ~IOP_XATTR;
	inode->i_fop = &simple_dir_operations;
	inode->i_mode = S_IFDIR | S_IRUGO | S_IWUSR | S_IXUGO;
	inode->i_mtime = inode_set_ctime_current(inode);
	inode->i_atime = dir->i_atime;
	BTRFS_I(inode)->i_otime = inode->i_mtime;
	inode->i_uid = dir->i_uid;
	inode->i_gid = dir->i_gid;

	return inode;
}

static_assert(BTRFS_FT_UNKNOWN == FT_UNKNOWN);
static_assert(BTRFS_FT_REG_FILE == FT_REG_FILE);
static_assert(BTRFS_FT_DIR == FT_DIR);
static_assert(BTRFS_FT_CHRDEV == FT_CHRDEV);
static_assert(BTRFS_FT_BLKDEV == FT_BLKDEV);
static_assert(BTRFS_FT_FIFO == FT_FIFO);
static_assert(BTRFS_FT_SOCK == FT_SOCK);
static_assert(BTRFS_FT_SYMLINK == FT_SYMLINK);

static inline u8 btrfs_inode_type(struct inode *inode)
{
	return fs_umode_to_ftype(inode->i_mode);
}

struct inode *btrfs_lookup_dentry(struct inode *dir, struct dentry *dentry)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(dir->i_sb);
	struct inode *inode;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_root *sub_root = root;
	struct btrfs_key location;
	u8 di_type = 0;
	int ret = 0;

	if (dentry->d_name.len > BTRFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	ret = btrfs_inode_by_name(BTRFS_I(dir), dentry, &location, &di_type);
	if (ret < 0)
		return ERR_PTR(ret);

	if (location.type == BTRFS_INODE_ITEM_KEY) {
		inode = btrfs_iget(dir->i_sb, location.objectid, root);
		if (IS_ERR(inode))
			return inode;

		 
		if (btrfs_inode_type(inode) != di_type) {
			btrfs_crit(fs_info,
"inode mode mismatch with dir: inode mode=0%o btrfs type=%u dir type=%u",
				  inode->i_mode, btrfs_inode_type(inode),
				  di_type);
			iput(inode);
			return ERR_PTR(-EUCLEAN);
		}
		return inode;
	}

	ret = fixup_tree_root_location(fs_info, BTRFS_I(dir), dentry,
				       &location, &sub_root);
	if (ret < 0) {
		if (ret != -ENOENT)
			inode = ERR_PTR(ret);
		else
			inode = new_simple_dir(dir, &location, root);
	} else {
		inode = btrfs_iget(dir->i_sb, location.objectid, sub_root);
		btrfs_put_root(sub_root);

		if (IS_ERR(inode))
			return inode;

		down_read(&fs_info->cleanup_work_sem);
		if (!sb_rdonly(inode->i_sb))
			ret = btrfs_orphan_cleanup(sub_root);
		up_read(&fs_info->cleanup_work_sem);
		if (ret) {
			iput(inode);
			inode = ERR_PTR(ret);
		}
	}

	return inode;
}

static int btrfs_dentry_delete(const struct dentry *dentry)
{
	struct btrfs_root *root;
	struct inode *inode = d_inode(dentry);

	if (!inode && !IS_ROOT(dentry))
		inode = d_inode(dentry->d_parent);

	if (inode) {
		root = BTRFS_I(inode)->root;
		if (btrfs_root_refs(&root->root_item) == 0)
			return 1;

		if (btrfs_ino(BTRFS_I(inode)) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)
			return 1;
	}
	return 0;
}

static struct dentry *btrfs_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags)
{
	struct inode *inode = btrfs_lookup_dentry(dir, dentry);

	if (inode == ERR_PTR(-ENOENT))
		inode = NULL;
	return d_splice_alias(inode, dentry);
}

 
static int btrfs_set_inode_index_count(struct btrfs_inode *inode)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_key key, found_key;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int ret;

	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_DIR_INDEX_KEY;
	key.offset = (u64)-1;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	 
	if (ret == 0)
		goto out;
	ret = 0;

	if (path->slots[0] == 0) {
		inode->index_cnt = BTRFS_DIR_START_INDEX;
		goto out;
	}

	path->slots[0]--;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

	if (found_key.objectid != btrfs_ino(inode) ||
	    found_key.type != BTRFS_DIR_INDEX_KEY) {
		inode->index_cnt = BTRFS_DIR_START_INDEX;
		goto out;
	}

	inode->index_cnt = found_key.offset + 1;
out:
	btrfs_free_path(path);
	return ret;
}

static int btrfs_get_dir_last_index(struct btrfs_inode *dir, u64 *index)
{
	int ret = 0;

	btrfs_inode_lock(dir, 0);
	if (dir->index_cnt == (u64)-1) {
		ret = btrfs_inode_delayed_dir_index_count(dir);
		if (ret) {
			ret = btrfs_set_inode_index_count(dir);
			if (ret)
				goto out;
		}
	}

	 
	*index = dir->index_cnt - 1;
out:
	btrfs_inode_unlock(dir, 0);

	return ret;
}

 
static int btrfs_opendir(struct inode *inode, struct file *file)
{
	struct btrfs_file_private *private;
	u64 last_index;
	int ret;

	ret = btrfs_get_dir_last_index(BTRFS_I(inode), &last_index);
	if (ret)
		return ret;

	private = kzalloc(sizeof(struct btrfs_file_private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;
	private->last_index = last_index;
	private->filldir_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!private->filldir_buf) {
		kfree(private);
		return -ENOMEM;
	}
	file->private_data = private;
	return 0;
}

static loff_t btrfs_dir_llseek(struct file *file, loff_t offset, int whence)
{
	struct btrfs_file_private *private = file->private_data;
	int ret;

	ret = btrfs_get_dir_last_index(BTRFS_I(file_inode(file)),
				       &private->last_index);
	if (ret)
		return ret;

	return generic_file_llseek(file, offset, whence);
}

struct dir_entry {
	u64 ino;
	u64 offset;
	unsigned type;
	int name_len;
};

static int btrfs_filldir(void *addr, int entries, struct dir_context *ctx)
{
	while (entries--) {
		struct dir_entry *entry = addr;
		char *name = (char *)(entry + 1);

		ctx->pos = get_unaligned(&entry->offset);
		if (!dir_emit(ctx, name, get_unaligned(&entry->name_len),
					 get_unaligned(&entry->ino),
					 get_unaligned(&entry->type)))
			return 1;
		addr += sizeof(struct dir_entry) +
			get_unaligned(&entry->name_len);
		ctx->pos++;
	}
	return 0;
}

static int btrfs_real_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_private *private = file->private_data;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_path *path;
	void *addr;
	LIST_HEAD(ins_list);
	LIST_HEAD(del_list);
	int ret;
	char *name_ptr;
	int name_len;
	int entries = 0;
	int total_len = 0;
	bool put = false;
	struct btrfs_key location;

	if (!dir_emit_dots(file, ctx))
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	addr = private->filldir_buf;
	path->reada = READA_FORWARD;

	put = btrfs_readdir_get_delayed_items(inode, private->last_index,
					      &ins_list, &del_list);

again:
	key.type = BTRFS_DIR_INDEX_KEY;
	key.offset = ctx->pos;
	key.objectid = btrfs_ino(BTRFS_I(inode));

	btrfs_for_each_slot(root, &key, &found_key, path, ret) {
		struct dir_entry *entry;
		struct extent_buffer *leaf = path->nodes[0];
		u8 ftype;

		if (found_key.objectid != key.objectid)
			break;
		if (found_key.type != BTRFS_DIR_INDEX_KEY)
			break;
		if (found_key.offset < ctx->pos)
			continue;
		if (found_key.offset > private->last_index)
			break;
		if (btrfs_should_delete_dir_index(&del_list, found_key.offset))
			continue;
		di = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_dir_item);
		name_len = btrfs_dir_name_len(leaf, di);
		if ((total_len + sizeof(struct dir_entry) + name_len) >=
		    PAGE_SIZE) {
			btrfs_release_path(path);
			ret = btrfs_filldir(private->filldir_buf, entries, ctx);
			if (ret)
				goto nopos;
			addr = private->filldir_buf;
			entries = 0;
			total_len = 0;
			goto again;
		}

		ftype = btrfs_dir_flags_to_ftype(btrfs_dir_flags(leaf, di));
		entry = addr;
		name_ptr = (char *)(entry + 1);
		read_extent_buffer(leaf, name_ptr,
				   (unsigned long)(di + 1), name_len);
		put_unaligned(name_len, &entry->name_len);
		put_unaligned(fs_ftype_to_dtype(ftype), &entry->type);
		btrfs_dir_item_key_to_cpu(leaf, di, &location);
		put_unaligned(location.objectid, &entry->ino);
		put_unaligned(found_key.offset, &entry->offset);
		entries++;
		addr += sizeof(struct dir_entry) + name_len;
		total_len += sizeof(struct dir_entry) + name_len;
	}
	 
	if (ret < 0)
		goto err;

	btrfs_release_path(path);

	ret = btrfs_filldir(private->filldir_buf, entries, ctx);
	if (ret)
		goto nopos;

	ret = btrfs_readdir_delayed_dir_index(ctx, &ins_list);
	if (ret)
		goto nopos;

	 
	if (ctx->pos >= INT_MAX)
		ctx->pos = LLONG_MAX;
	else
		ctx->pos = INT_MAX;
nopos:
	ret = 0;
err:
	if (put)
		btrfs_readdir_put_delayed_items(inode, &ins_list, &del_list);
	btrfs_free_path(path);
	return ret;
}

 
static int btrfs_dirty_inode(struct btrfs_inode *inode)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans;
	int ret;

	if (test_bit(BTRFS_INODE_DUMMY, &inode->runtime_flags))
		return 0;

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	ret = btrfs_update_inode(trans, root, inode);
	if (ret && (ret == -ENOSPC || ret == -EDQUOT)) {
		 
		btrfs_end_transaction(trans);
		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans))
			return PTR_ERR(trans);

		ret = btrfs_update_inode(trans, root, inode);
	}
	btrfs_end_transaction(trans);
	if (inode->delayed_node)
		btrfs_balance_delayed_items(fs_info);

	return ret;
}

 
static int btrfs_update_time(struct inode *inode, int flags)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	bool dirty = flags & ~S_VERSION;

	if (btrfs_root_readonly(root))
		return -EROFS;

	dirty = inode_update_timestamps(inode, flags);
	return dirty ? btrfs_dirty_inode(BTRFS_I(inode)) : 0;
}

 
int btrfs_set_inode_index(struct btrfs_inode *dir, u64 *index)
{
	int ret = 0;

	if (dir->index_cnt == (u64)-1) {
		ret = btrfs_inode_delayed_dir_index_count(dir);
		if (ret) {
			ret = btrfs_set_inode_index_count(dir);
			if (ret)
				return ret;
		}
	}

	*index = dir->index_cnt;
	dir->index_cnt++;

	return ret;
}

static int btrfs_insert_inode_locked(struct inode *inode)
{
	struct btrfs_iget_args args;

	args.ino = BTRFS_I(inode)->location.objectid;
	args.root = BTRFS_I(inode)->root;

	return insert_inode_locked4(inode,
		   btrfs_inode_hash(inode->i_ino, BTRFS_I(inode)->root),
		   btrfs_find_actor, &args);
}

int btrfs_new_inode_prepare(struct btrfs_new_inode_args *args,
			    unsigned int *trans_num_items)
{
	struct inode *dir = args->dir;
	struct inode *inode = args->inode;
	int ret;

	if (!args->orphan) {
		ret = fscrypt_setup_filename(dir, &args->dentry->d_name, 0,
					     &args->fname);
		if (ret)
			return ret;
	}

	ret = posix_acl_create(dir, &inode->i_mode, &args->default_acl, &args->acl);
	if (ret) {
		fscrypt_free_filename(&args->fname);
		return ret;
	}

	 
	*trans_num_items = 1;
	 
	if (BTRFS_I(dir)->prop_compress)
		(*trans_num_items)++;
	 
	if (args->default_acl)
		(*trans_num_items)++;
	 
	if (args->acl)
		(*trans_num_items)++;
#ifdef CONFIG_SECURITY
	 
	if (dir->i_security)
		(*trans_num_items)++;
#endif
	if (args->orphan) {
		 
		(*trans_num_items)++;
	} else {
		 
		*trans_num_items += 3;
	}
	return 0;
}

void btrfs_new_inode_args_destroy(struct btrfs_new_inode_args *args)
{
	posix_acl_release(args->acl);
	posix_acl_release(args->default_acl);
	fscrypt_free_filename(&args->fname);
}

 
static void btrfs_inherit_iflags(struct btrfs_inode *inode, struct btrfs_inode *dir)
{
	unsigned int flags;

	flags = dir->flags;

	if (flags & BTRFS_INODE_NOCOMPRESS) {
		inode->flags &= ~BTRFS_INODE_COMPRESS;
		inode->flags |= BTRFS_INODE_NOCOMPRESS;
	} else if (flags & BTRFS_INODE_COMPRESS) {
		inode->flags &= ~BTRFS_INODE_NOCOMPRESS;
		inode->flags |= BTRFS_INODE_COMPRESS;
	}

	if (flags & BTRFS_INODE_NODATACOW) {
		inode->flags |= BTRFS_INODE_NODATACOW;
		if (S_ISREG(inode->vfs_inode.i_mode))
			inode->flags |= BTRFS_INODE_NODATASUM;
	}

	btrfs_sync_inode_flags_to_i_flags(&inode->vfs_inode);
}

int btrfs_create_new_inode(struct btrfs_trans_handle *trans,
			   struct btrfs_new_inode_args *args)
{
	struct inode *dir = args->dir;
	struct inode *inode = args->inode;
	const struct fscrypt_str *name = args->orphan ? NULL : &args->fname.disk_name;
	struct btrfs_fs_info *fs_info = btrfs_sb(dir->i_sb);
	struct btrfs_root *root;
	struct btrfs_inode_item *inode_item;
	struct btrfs_key *location;
	struct btrfs_path *path;
	u64 objectid;
	struct btrfs_inode_ref *ref;
	struct btrfs_key key[2];
	u32 sizes[2];
	struct btrfs_item_batch batch;
	unsigned long ptr;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	if (!args->subvol)
		BTRFS_I(inode)->root = btrfs_grab_root(BTRFS_I(dir)->root);
	root = BTRFS_I(inode)->root;

	ret = btrfs_get_free_objectid(root, &objectid);
	if (ret)
		goto out;
	inode->i_ino = objectid;

	if (args->orphan) {
		 
		set_nlink(inode, 0);
	} else {
		trace_btrfs_inode_request(dir);

		ret = btrfs_set_inode_index(BTRFS_I(dir), &BTRFS_I(inode)->dir_index);
		if (ret)
			goto out;
	}
	 
	BTRFS_I(inode)->index_cnt = BTRFS_DIR_START_INDEX;
	BTRFS_I(inode)->generation = trans->transid;
	inode->i_generation = BTRFS_I(inode)->generation;

	 
	if (!args->subvol)
		btrfs_inherit_iflags(BTRFS_I(inode), BTRFS_I(dir));

	if (S_ISREG(inode->i_mode)) {
		if (btrfs_test_opt(fs_info, NODATASUM))
			BTRFS_I(inode)->flags |= BTRFS_INODE_NODATASUM;
		if (btrfs_test_opt(fs_info, NODATACOW))
			BTRFS_I(inode)->flags |= BTRFS_INODE_NODATACOW |
				BTRFS_INODE_NODATASUM;
	}

	location = &BTRFS_I(inode)->location;
	location->objectid = objectid;
	location->offset = 0;
	location->type = BTRFS_INODE_ITEM_KEY;

	ret = btrfs_insert_inode_locked(inode);
	if (ret < 0) {
		if (!args->orphan)
			BTRFS_I(dir)->index_cnt--;
		goto out;
	}

	 
	btrfs_set_inode_full_sync(BTRFS_I(inode));

	key[0].objectid = objectid;
	key[0].type = BTRFS_INODE_ITEM_KEY;
	key[0].offset = 0;

	sizes[0] = sizeof(struct btrfs_inode_item);

	if (!args->orphan) {
		 
		key[1].objectid = objectid;
		key[1].type = BTRFS_INODE_REF_KEY;
		if (args->subvol) {
			key[1].offset = objectid;
			sizes[1] = 2 + sizeof(*ref);
		} else {
			key[1].offset = btrfs_ino(BTRFS_I(dir));
			sizes[1] = name->len + sizeof(*ref);
		}
	}

	batch.keys = &key[0];
	batch.data_sizes = &sizes[0];
	batch.total_data_size = sizes[0] + (args->orphan ? 0 : sizes[1]);
	batch.nr = args->orphan ? 1 : 2;
	ret = btrfs_insert_empty_items(trans, root, path, &batch);
	if (ret != 0) {
		btrfs_abort_transaction(trans, ret);
		goto discard;
	}

	inode->i_mtime = inode_set_ctime_current(inode);
	inode->i_atime = inode->i_mtime;
	BTRFS_I(inode)->i_otime = inode->i_mtime;

	 

	inode_item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				  struct btrfs_inode_item);
	memzero_extent_buffer(path->nodes[0], (unsigned long)inode_item,
			     sizeof(*inode_item));
	fill_inode_item(trans, path->nodes[0], inode_item, inode);

	if (!args->orphan) {
		ref = btrfs_item_ptr(path->nodes[0], path->slots[0] + 1,
				     struct btrfs_inode_ref);
		ptr = (unsigned long)(ref + 1);
		if (args->subvol) {
			btrfs_set_inode_ref_name_len(path->nodes[0], ref, 2);
			btrfs_set_inode_ref_index(path->nodes[0], ref, 0);
			write_extent_buffer(path->nodes[0], "..", ptr, 2);
		} else {
			btrfs_set_inode_ref_name_len(path->nodes[0], ref,
						     name->len);
			btrfs_set_inode_ref_index(path->nodes[0], ref,
						  BTRFS_I(inode)->dir_index);
			write_extent_buffer(path->nodes[0], name->name, ptr,
					    name->len);
		}
	}

	btrfs_mark_buffer_dirty(trans, path->nodes[0]);
	 
	btrfs_free_path(path);
	path = NULL;

	if (args->subvol) {
		struct inode *parent;

		 
		parent = btrfs_iget(fs_info->sb, BTRFS_FIRST_FREE_OBJECTID,
				    BTRFS_I(dir)->root);
		if (IS_ERR(parent)) {
			ret = PTR_ERR(parent);
		} else {
			ret = btrfs_inode_inherit_props(trans, inode, parent);
			iput(parent);
		}
	} else {
		ret = btrfs_inode_inherit_props(trans, inode, dir);
	}
	if (ret) {
		btrfs_err(fs_info,
			  "error inheriting props for ino %llu (root %llu): %d",
			  btrfs_ino(BTRFS_I(inode)), root->root_key.objectid,
			  ret);
	}

	 
	if (!args->subvol) {
		ret = btrfs_init_inode_security(trans, args);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto discard;
		}
	}

	inode_tree_add(BTRFS_I(inode));

	trace_btrfs_inode_new(inode);
	btrfs_set_inode_last_trans(trans, BTRFS_I(inode));

	btrfs_update_root_times(trans, root);

	if (args->orphan) {
		ret = btrfs_orphan_add(trans, BTRFS_I(inode));
	} else {
		ret = btrfs_add_link(trans, BTRFS_I(dir), BTRFS_I(inode), name,
				     0, BTRFS_I(inode)->dir_index);
	}
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto discard;
	}

	return 0;

discard:
	 
	ihold(inode);
	discard_new_inode(inode);
out:
	btrfs_free_path(path);
	return ret;
}

 
int btrfs_add_link(struct btrfs_trans_handle *trans,
		   struct btrfs_inode *parent_inode, struct btrfs_inode *inode,
		   const struct fscrypt_str *name, int add_backref, u64 index)
{
	int ret = 0;
	struct btrfs_key key;
	struct btrfs_root *root = parent_inode->root;
	u64 ino = btrfs_ino(inode);
	u64 parent_ino = btrfs_ino(parent_inode);

	if (unlikely(ino == BTRFS_FIRST_FREE_OBJECTID)) {
		memcpy(&key, &inode->root->root_key, sizeof(key));
	} else {
		key.objectid = ino;
		key.type = BTRFS_INODE_ITEM_KEY;
		key.offset = 0;
	}

	if (unlikely(ino == BTRFS_FIRST_FREE_OBJECTID)) {
		ret = btrfs_add_root_ref(trans, key.objectid,
					 root->root_key.objectid, parent_ino,
					 index, name);
	} else if (add_backref) {
		ret = btrfs_insert_inode_ref(trans, root, name,
					     ino, parent_ino, index);
	}

	 
	if (ret)
		return ret;

	ret = btrfs_insert_dir_item(trans, name, parent_inode, &key,
				    btrfs_inode_type(&inode->vfs_inode), index);
	if (ret == -EEXIST || ret == -EOVERFLOW)
		goto fail_dir_item;
	else if (ret) {
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	btrfs_i_size_write(parent_inode, parent_inode->vfs_inode.i_size +
			   name->len * 2);
	inode_inc_iversion(&parent_inode->vfs_inode);
	 
	if (!test_bit(BTRFS_FS_LOG_RECOVERING, &root->fs_info->flags))
		parent_inode->vfs_inode.i_mtime =
			inode_set_ctime_current(&parent_inode->vfs_inode);

	ret = btrfs_update_inode(trans, root, parent_inode);
	if (ret)
		btrfs_abort_transaction(trans, ret);
	return ret;

fail_dir_item:
	if (unlikely(ino == BTRFS_FIRST_FREE_OBJECTID)) {
		u64 local_index;
		int err;
		err = btrfs_del_root_ref(trans, key.objectid,
					 root->root_key.objectid, parent_ino,
					 &local_index, name);
		if (err)
			btrfs_abort_transaction(trans, err);
	} else if (add_backref) {
		u64 local_index;
		int err;

		err = btrfs_del_inode_ref(trans, root, name, ino, parent_ino,
					  &local_index);
		if (err)
			btrfs_abort_transaction(trans, err);
	}

	 
	return ret;
}

static int btrfs_create_common(struct inode *dir, struct dentry *dentry,
			       struct inode *inode)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(dir->i_sb);
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_new_inode_args new_inode_args = {
		.dir = dir,
		.dentry = dentry,
		.inode = inode,
	};
	unsigned int trans_num_items;
	struct btrfs_trans_handle *trans;
	int err;

	err = btrfs_new_inode_prepare(&new_inode_args, &trans_num_items);
	if (err)
		goto out_inode;

	trans = btrfs_start_transaction(root, trans_num_items);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out_new_inode_args;
	}

	err = btrfs_create_new_inode(trans, &new_inode_args);
	if (!err)
		d_instantiate_new(dentry, inode);

	btrfs_end_transaction(trans);
	btrfs_btree_balance_dirty(fs_info);
out_new_inode_args:
	btrfs_new_inode_args_destroy(&new_inode_args);
out_inode:
	if (err)
		iput(inode);
	return err;
}

static int btrfs_mknod(struct mnt_idmap *idmap, struct inode *dir,
		       struct dentry *dentry, umode_t mode, dev_t rdev)
{
	struct inode *inode;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;
	inode_init_owner(idmap, inode, dir, mode);
	inode->i_op = &btrfs_special_inode_operations;
	init_special_inode(inode, inode->i_mode, rdev);
	return btrfs_create_common(dir, dentry, inode);
}

static int btrfs_create(struct mnt_idmap *idmap, struct inode *dir,
			struct dentry *dentry, umode_t mode, bool excl)
{
	struct inode *inode;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;
	inode_init_owner(idmap, inode, dir, mode);
	inode->i_fop = &btrfs_file_operations;
	inode->i_op = &btrfs_file_inode_operations;
	inode->i_mapping->a_ops = &btrfs_aops;
	return btrfs_create_common(dir, dentry, inode);
}

static int btrfs_link(struct dentry *old_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct btrfs_trans_handle *trans = NULL;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode = d_inode(old_dentry);
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct fscrypt_name fname;
	u64 index;
	int err;
	int drop_inode = 0;

	 
	if (root->root_key.objectid != BTRFS_I(inode)->root->root_key.objectid)
		return -EXDEV;

	if (inode->i_nlink >= BTRFS_LINK_MAX)
		return -EMLINK;

	err = fscrypt_setup_filename(dir, &dentry->d_name, 0, &fname);
	if (err)
		goto fail;

	err = btrfs_set_inode_index(BTRFS_I(dir), &index);
	if (err)
		goto fail;

	 
	trans = btrfs_start_transaction(root, inode->i_nlink ? 5 : 6);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		trans = NULL;
		goto fail;
	}

	 
	BTRFS_I(inode)->dir_index = 0ULL;
	inc_nlink(inode);
	inode_inc_iversion(inode);
	inode_set_ctime_current(inode);
	ihold(inode);
	set_bit(BTRFS_INODE_COPY_EVERYTHING, &BTRFS_I(inode)->runtime_flags);

	err = btrfs_add_link(trans, BTRFS_I(dir), BTRFS_I(inode),
			     &fname.disk_name, 1, index);

	if (err) {
		drop_inode = 1;
	} else {
		struct dentry *parent = dentry->d_parent;

		err = btrfs_update_inode(trans, root, BTRFS_I(inode));
		if (err)
			goto fail;
		if (inode->i_nlink == 1) {
			 
			err = btrfs_orphan_del(trans, BTRFS_I(inode));
			if (err)
				goto fail;
		}
		d_instantiate(dentry, inode);
		btrfs_log_new_name(trans, old_dentry, NULL, 0, parent);
	}

fail:
	fscrypt_free_filename(&fname);
	if (trans)
		btrfs_end_transaction(trans);
	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(fs_info);
	return err;
}

static int btrfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		       struct dentry *dentry, umode_t mode)
{
	struct inode *inode;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;
	inode_init_owner(idmap, inode, dir, S_IFDIR | mode);
	inode->i_op = &btrfs_dir_inode_operations;
	inode->i_fop = &btrfs_dir_file_operations;
	return btrfs_create_common(dir, dentry, inode);
}

static noinline int uncompress_inline(struct btrfs_path *path,
				      struct page *page,
				      struct btrfs_file_extent_item *item)
{
	int ret;
	struct extent_buffer *leaf = path->nodes[0];
	char *tmp;
	size_t max_size;
	unsigned long inline_size;
	unsigned long ptr;
	int compress_type;

	compress_type = btrfs_file_extent_compression(leaf, item);
	max_size = btrfs_file_extent_ram_bytes(leaf, item);
	inline_size = btrfs_file_extent_inline_item_len(leaf, path->slots[0]);
	tmp = kmalloc(inline_size, GFP_NOFS);
	if (!tmp)
		return -ENOMEM;
	ptr = btrfs_file_extent_inline_start(item);

	read_extent_buffer(leaf, tmp, ptr, inline_size);

	max_size = min_t(unsigned long, PAGE_SIZE, max_size);
	ret = btrfs_decompress(compress_type, tmp, page, 0, inline_size, max_size);

	 

	if (max_size < PAGE_SIZE)
		memzero_page(page, max_size, PAGE_SIZE - max_size);
	kfree(tmp);
	return ret;
}

static int read_inline_extent(struct btrfs_inode *inode, struct btrfs_path *path,
			      struct page *page)
{
	struct btrfs_file_extent_item *fi;
	void *kaddr;
	size_t copy_size;

	if (!page || PageUptodate(page))
		return 0;

	ASSERT(page_offset(page) == 0);

	fi = btrfs_item_ptr(path->nodes[0], path->slots[0],
			    struct btrfs_file_extent_item);
	if (btrfs_file_extent_compression(path->nodes[0], fi) != BTRFS_COMPRESS_NONE)
		return uncompress_inline(path, page, fi);

	copy_size = min_t(u64, PAGE_SIZE,
			  btrfs_file_extent_ram_bytes(path->nodes[0], fi));
	kaddr = kmap_local_page(page);
	read_extent_buffer(path->nodes[0], kaddr,
			   btrfs_file_extent_inline_start(fi), copy_size);
	kunmap_local(kaddr);
	if (copy_size < PAGE_SIZE)
		memzero_page(page, copy_size, PAGE_SIZE - copy_size);
	return 0;
}

 
struct extent_map *btrfs_get_extent(struct btrfs_inode *inode,
				    struct page *page, size_t pg_offset,
				    u64 start, u64 len)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	int ret = 0;
	u64 extent_start = 0;
	u64 extent_end = 0;
	u64 objectid = btrfs_ino(inode);
	int extent_type = -1;
	struct btrfs_path *path = NULL;
	struct btrfs_root *root = inode->root;
	struct btrfs_file_extent_item *item;
	struct extent_buffer *leaf;
	struct btrfs_key found_key;
	struct extent_map *em = NULL;
	struct extent_map_tree *em_tree = &inode->extent_tree;

	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree, start, len);
	read_unlock(&em_tree->lock);

	if (em) {
		if (em->start > start || em->start + em->len <= start)
			free_extent_map(em);
		else if (em->block_start == EXTENT_MAP_INLINE && page)
			free_extent_map(em);
		else
			goto out;
	}
	em = alloc_extent_map();
	if (!em) {
		ret = -ENOMEM;
		goto out;
	}
	em->start = EXTENT_MAP_HOLE;
	em->orig_start = EXTENT_MAP_HOLE;
	em->len = (u64)-1;
	em->block_len = (u64)-1;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	 
	path->reada = READA_FORWARD;

	 
	if (btrfs_is_free_space_inode(inode)) {
		path->search_commit_root = 1;
		path->skip_locking = 1;
	}

	ret = btrfs_lookup_file_extent(NULL, root, path, objectid, start, 0);
	if (ret < 0) {
		goto out;
	} else if (ret > 0) {
		if (path->slots[0] == 0)
			goto not_found;
		path->slots[0]--;
		ret = 0;
	}

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0],
			      struct btrfs_file_extent_item);
	btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
	if (found_key.objectid != objectid ||
	    found_key.type != BTRFS_EXTENT_DATA_KEY) {
		 
		extent_end = start;
		goto next;
	}

	extent_type = btrfs_file_extent_type(leaf, item);
	extent_start = found_key.offset;
	extent_end = btrfs_file_extent_end(path);
	if (extent_type == BTRFS_FILE_EXTENT_REG ||
	    extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
		 
		if (!S_ISREG(inode->vfs_inode.i_mode)) {
			ret = -EUCLEAN;
			btrfs_crit(fs_info,
		"regular/prealloc extent found for non-regular inode %llu",
				   btrfs_ino(inode));
			goto out;
		}
		trace_btrfs_get_extent_show_fi_regular(inode, leaf, item,
						       extent_start);
	} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
		trace_btrfs_get_extent_show_fi_inline(inode, leaf, item,
						      path->slots[0],
						      extent_start);
	}
next:
	if (start >= extent_end) {
		path->slots[0]++;
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				goto not_found;

			leaf = path->nodes[0];
		}
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		if (found_key.objectid != objectid ||
		    found_key.type != BTRFS_EXTENT_DATA_KEY)
			goto not_found;
		if (start + len <= found_key.offset)
			goto not_found;
		if (start > found_key.offset)
			goto next;

		 
		em->start = start;
		em->orig_start = start;
		em->len = found_key.offset - start;
		em->block_start = EXTENT_MAP_HOLE;
		goto insert;
	}

	btrfs_extent_item_to_extent_map(inode, path, item, em);

	if (extent_type == BTRFS_FILE_EXTENT_REG ||
	    extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
		goto insert;
	} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
		 
		ASSERT(pg_offset == 0);
		ASSERT(extent_start == 0);
		ASSERT(em->start == 0);

		 
		ASSERT(em->block_start == EXTENT_MAP_INLINE);
		ASSERT(em->len == fs_info->sectorsize);

		ret = read_inline_extent(inode, path, page);
		if (ret < 0)
			goto out;
		goto insert;
	}
not_found:
	em->start = start;
	em->orig_start = start;
	em->len = len;
	em->block_start = EXTENT_MAP_HOLE;
insert:
	ret = 0;
	btrfs_release_path(path);
	if (em->start > start || extent_map_end(em) <= start) {
		btrfs_err(fs_info,
			  "bad extent! em: [%llu %llu] passed [%llu %llu]",
			  em->start, em->len, start, len);
		ret = -EIO;
		goto out;
	}

	write_lock(&em_tree->lock);
	ret = btrfs_add_extent_mapping(fs_info, em_tree, &em, start, len);
	write_unlock(&em_tree->lock);
out:
	btrfs_free_path(path);

	trace_btrfs_get_extent(root, inode, em);

	if (ret) {
		free_extent_map(em);
		return ERR_PTR(ret);
	}
	return em;
}

static struct extent_map *btrfs_create_dio_extent(struct btrfs_inode *inode,
						  struct btrfs_dio_data *dio_data,
						  const u64 start,
						  const u64 len,
						  const u64 orig_start,
						  const u64 block_start,
						  const u64 block_len,
						  const u64 orig_block_len,
						  const u64 ram_bytes,
						  const int type)
{
	struct extent_map *em = NULL;
	struct btrfs_ordered_extent *ordered;

	if (type != BTRFS_ORDERED_NOCOW) {
		em = create_io_em(inode, start, len, orig_start, block_start,
				  block_len, orig_block_len, ram_bytes,
				  BTRFS_COMPRESS_NONE,  
				  type);
		if (IS_ERR(em))
			goto out;
	}
	ordered = btrfs_alloc_ordered_extent(inode, start, len, len,
					     block_start, block_len, 0,
					     (1 << type) |
					     (1 << BTRFS_ORDERED_DIRECT),
					     BTRFS_COMPRESS_NONE);
	if (IS_ERR(ordered)) {
		if (em) {
			free_extent_map(em);
			btrfs_drop_extent_map_range(inode, start,
						    start + len - 1, false);
		}
		em = ERR_CAST(ordered);
	} else {
		ASSERT(!dio_data->ordered);
		dio_data->ordered = ordered;
	}
 out:

	return em;
}

static struct extent_map *btrfs_new_extent_direct(struct btrfs_inode *inode,
						  struct btrfs_dio_data *dio_data,
						  u64 start, u64 len)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_map *em;
	struct btrfs_key ins;
	u64 alloc_hint;
	int ret;

	alloc_hint = get_extent_allocation_hint(inode, start, len);
again:
	ret = btrfs_reserve_extent(root, len, len, fs_info->sectorsize,
				   0, alloc_hint, &ins, 1, 1);
	if (ret == -EAGAIN) {
		ASSERT(btrfs_is_zoned(fs_info));
		wait_on_bit_io(&inode->root->fs_info->flags, BTRFS_FS_NEED_ZONE_FINISH,
			       TASK_UNINTERRUPTIBLE);
		goto again;
	}
	if (ret)
		return ERR_PTR(ret);

	em = btrfs_create_dio_extent(inode, dio_data, start, ins.offset, start,
				     ins.objectid, ins.offset, ins.offset,
				     ins.offset, BTRFS_ORDERED_REGULAR);
	btrfs_dec_block_group_reservations(fs_info, ins.objectid);
	if (IS_ERR(em))
		btrfs_free_reserved_extent(fs_info, ins.objectid, ins.offset,
					   1);

	return em;
}

static bool btrfs_extent_readonly(struct btrfs_fs_info *fs_info, u64 bytenr)
{
	struct btrfs_block_group *block_group;
	bool readonly = false;

	block_group = btrfs_lookup_block_group(fs_info, bytenr);
	if (!block_group || block_group->ro)
		readonly = true;
	if (block_group)
		btrfs_put_block_group(block_group);
	return readonly;
}

 
noinline int can_nocow_extent(struct inode *inode, u64 offset, u64 *len,
			      u64 *orig_start, u64 *orig_block_len,
			      u64 *ram_bytes, bool nowait, bool strict)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct can_nocow_file_extent_args nocow_args = { 0 };
	struct btrfs_path *path;
	int ret;
	struct extent_buffer *leaf;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_file_extent_item *fi;
	struct btrfs_key key;
	int found_type;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->nowait = nowait;

	ret = btrfs_lookup_file_extent(NULL, root, path,
			btrfs_ino(BTRFS_I(inode)), offset, 0);
	if (ret < 0)
		goto out;

	if (ret == 1) {
		if (path->slots[0] == 0) {
			 
			ret = 0;
			goto out;
		}
		path->slots[0]--;
	}
	ret = 0;
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	if (key.objectid != btrfs_ino(BTRFS_I(inode)) ||
	    key.type != BTRFS_EXTENT_DATA_KEY) {
		 
		goto out;
	}

	if (key.offset > offset) {
		 
		goto out;
	}

	if (btrfs_file_extent_end(path) <= offset)
		goto out;

	fi = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_file_extent_item);
	found_type = btrfs_file_extent_type(leaf, fi);
	if (ram_bytes)
		*ram_bytes = btrfs_file_extent_ram_bytes(leaf, fi);

	nocow_args.start = offset;
	nocow_args.end = offset + *len - 1;
	nocow_args.strict = strict;
	nocow_args.free_path = true;

	ret = can_nocow_file_extent(path, &key, BTRFS_I(inode), &nocow_args);
	 
	path = NULL;

	if (ret != 1) {
		 
		ret = 0;
		goto out;
	}

	ret = 0;
	if (btrfs_extent_readonly(fs_info, nocow_args.disk_bytenr))
		goto out;

	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NODATACOW) &&
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		u64 range_end;

		range_end = round_up(offset + nocow_args.num_bytes,
				     root->fs_info->sectorsize) - 1;
		ret = test_range_bit(io_tree, offset, range_end,
				     EXTENT_DELALLOC, 0, NULL);
		if (ret) {
			ret = -EAGAIN;
			goto out;
		}
	}

	if (orig_start)
		*orig_start = key.offset - nocow_args.extent_offset;
	if (orig_block_len)
		*orig_block_len = nocow_args.disk_num_bytes;

	*len = nocow_args.num_bytes;
	ret = 1;
out:
	btrfs_free_path(path);
	return ret;
}

static int lock_extent_direct(struct inode *inode, u64 lockstart, u64 lockend,
			      struct extent_state **cached_state,
			      unsigned int iomap_flags)
{
	const bool writing = (iomap_flags & IOMAP_WRITE);
	const bool nowait = (iomap_flags & IOMAP_NOWAIT);
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_ordered_extent *ordered;
	int ret = 0;

	while (1) {
		if (nowait) {
			if (!try_lock_extent(io_tree, lockstart, lockend,
					     cached_state))
				return -EAGAIN;
		} else {
			lock_extent(io_tree, lockstart, lockend, cached_state);
		}
		 
		ordered = btrfs_lookup_ordered_range(BTRFS_I(inode), lockstart,
						     lockend - lockstart + 1);

		 
		if (!ordered &&
		    (!writing || !filemap_range_has_page(inode->i_mapping,
							 lockstart, lockend)))
			break;

		unlock_extent(io_tree, lockstart, lockend, cached_state);

		if (ordered) {
			if (nowait) {
				btrfs_put_ordered_extent(ordered);
				ret = -EAGAIN;
				break;
			}
			 
			if (writing ||
			    test_bit(BTRFS_ORDERED_DIRECT, &ordered->flags))
				btrfs_start_ordered_extent(ordered);
			else
				ret = nowait ? -EAGAIN : -ENOTBLK;
			btrfs_put_ordered_extent(ordered);
		} else {
			 
			ret = nowait ? -EAGAIN : -ENOTBLK;
		}

		if (ret)
			break;

		cond_resched();
	}

	return ret;
}

 
static struct extent_map *create_io_em(struct btrfs_inode *inode, u64 start,
				       u64 len, u64 orig_start, u64 block_start,
				       u64 block_len, u64 orig_block_len,
				       u64 ram_bytes, int compress_type,
				       int type)
{
	struct extent_map *em;
	int ret;

	ASSERT(type == BTRFS_ORDERED_PREALLOC ||
	       type == BTRFS_ORDERED_COMPRESSED ||
	       type == BTRFS_ORDERED_NOCOW ||
	       type == BTRFS_ORDERED_REGULAR);

	em = alloc_extent_map();
	if (!em)
		return ERR_PTR(-ENOMEM);

	em->start = start;
	em->orig_start = orig_start;
	em->len = len;
	em->block_len = block_len;
	em->block_start = block_start;
	em->orig_block_len = orig_block_len;
	em->ram_bytes = ram_bytes;
	em->generation = -1;
	set_bit(EXTENT_FLAG_PINNED, &em->flags);
	if (type == BTRFS_ORDERED_PREALLOC) {
		set_bit(EXTENT_FLAG_FILLING, &em->flags);
	} else if (type == BTRFS_ORDERED_COMPRESSED) {
		set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);
		em->compress_type = compress_type;
	}

	ret = btrfs_replace_extent_map_range(inode, em, true);
	if (ret) {
		free_extent_map(em);
		return ERR_PTR(ret);
	}

	 
	return em;
}


static int btrfs_get_blocks_direct_write(struct extent_map **map,
					 struct inode *inode,
					 struct btrfs_dio_data *dio_data,
					 u64 start, u64 *lenp,
					 unsigned int iomap_flags)
{
	const bool nowait = (iomap_flags & IOMAP_NOWAIT);
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct extent_map *em = *map;
	int type;
	u64 block_start, orig_start, orig_block_len, ram_bytes;
	struct btrfs_block_group *bg;
	bool can_nocow = false;
	bool space_reserved = false;
	u64 len = *lenp;
	u64 prev_len;
	int ret = 0;

	 
	if (test_bit(EXTENT_FLAG_PREALLOC, &em->flags) ||
	    ((BTRFS_I(inode)->flags & BTRFS_INODE_NODATACOW) &&
	     em->block_start != EXTENT_MAP_HOLE)) {
		if (test_bit(EXTENT_FLAG_PREALLOC, &em->flags))
			type = BTRFS_ORDERED_PREALLOC;
		else
			type = BTRFS_ORDERED_NOCOW;
		len = min(len, em->len - (start - em->start));
		block_start = em->block_start + (start - em->start);

		if (can_nocow_extent(inode, start, &len, &orig_start,
				     &orig_block_len, &ram_bytes, false, false) == 1) {
			bg = btrfs_inc_nocow_writers(fs_info, block_start);
			if (bg)
				can_nocow = true;
		}
	}

	prev_len = len;
	if (can_nocow) {
		struct extent_map *em2;

		 
		ret = btrfs_delalloc_reserve_metadata(BTRFS_I(inode), len, len,
						      nowait);
		if (ret < 0) {
			 
			free_extent_map(em);
			*map = NULL;
			btrfs_dec_nocow_writers(bg);
			if (nowait && (ret == -ENOSPC || ret == -EDQUOT))
				ret = -EAGAIN;
			goto out;
		}
		space_reserved = true;

		em2 = btrfs_create_dio_extent(BTRFS_I(inode), dio_data, start, len,
					      orig_start, block_start,
					      len, orig_block_len,
					      ram_bytes, type);
		btrfs_dec_nocow_writers(bg);
		if (type == BTRFS_ORDERED_PREALLOC) {
			free_extent_map(em);
			*map = em2;
			em = em2;
		}

		if (IS_ERR(em2)) {
			ret = PTR_ERR(em2);
			goto out;
		}

		dio_data->nocow_done = true;
	} else {
		 
		free_extent_map(em);
		*map = NULL;

		if (nowait) {
			ret = -EAGAIN;
			goto out;
		}

		 
		if (!dio_data->data_space_reserved) {
			ret = -ENOSPC;
			goto out;
		}

		 
		ret = btrfs_delalloc_reserve_metadata(BTRFS_I(inode), len, len,
						      false);
		if (ret < 0)
			goto out;
		space_reserved = true;

		em = btrfs_new_extent_direct(BTRFS_I(inode), dio_data, start, len);
		if (IS_ERR(em)) {
			ret = PTR_ERR(em);
			goto out;
		}
		*map = em;
		len = min(len, em->len - (start - em->start));
		if (len < prev_len)
			btrfs_delalloc_release_metadata(BTRFS_I(inode),
							prev_len - len, true);
	}

	 
	btrfs_delalloc_release_extents(BTRFS_I(inode), prev_len);

	 
	if (start + len > i_size_read(inode))
		i_size_write(inode, start + len);
out:
	if (ret && space_reserved) {
		btrfs_delalloc_release_extents(BTRFS_I(inode), len);
		btrfs_delalloc_release_metadata(BTRFS_I(inode), len, true);
	}
	*lenp = len;
	return ret;
}

static int btrfs_dio_iomap_begin(struct inode *inode, loff_t start,
		loff_t length, unsigned int flags, struct iomap *iomap,
		struct iomap *srcmap)
{
	struct iomap_iter *iter = container_of(iomap, struct iomap_iter, iomap);
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct extent_map *em;
	struct extent_state *cached_state = NULL;
	struct btrfs_dio_data *dio_data = iter->private;
	u64 lockstart, lockend;
	const bool write = !!(flags & IOMAP_WRITE);
	int ret = 0;
	u64 len = length;
	const u64 data_alloc_len = length;
	bool unlock_extents = false;

	 
	if (!write && (flags & IOMAP_NOWAIT) && length > PAGE_SIZE)
		return -EAGAIN;

	 
	if (!write)
		len = min_t(u64, len, fs_info->sectorsize * BTRFS_MAX_BIO_SECTORS);

	lockstart = start;
	lockend = start + len - 1;

	 
	if (test_bit(BTRFS_INODE_HAS_ASYNC_EXTENT,
		     &BTRFS_I(inode)->runtime_flags)) {
		if (flags & IOMAP_NOWAIT) {
			if (filemap_range_needs_writeback(inode->i_mapping,
							  lockstart, lockend))
				return -EAGAIN;
		} else {
			ret = filemap_fdatawrite_range(inode->i_mapping, start,
						       start + length - 1);
			if (ret)
				return ret;
		}
	}

	memset(dio_data, 0, sizeof(*dio_data));

	 
	if (write && !(flags & IOMAP_NOWAIT)) {
		ret = btrfs_check_data_free_space(BTRFS_I(inode),
						  &dio_data->data_reserved,
						  start, data_alloc_len, false);
		if (!ret)
			dio_data->data_space_reserved = true;
		else if (ret && !(BTRFS_I(inode)->flags &
				  (BTRFS_INODE_NODATACOW | BTRFS_INODE_PREALLOC)))
			goto err;
	}

	 
	ret = lock_extent_direct(inode, lockstart, lockend, &cached_state, flags);
	if (ret < 0)
		goto err;

	em = btrfs_get_extent(BTRFS_I(inode), NULL, 0, start, len);
	if (IS_ERR(em)) {
		ret = PTR_ERR(em);
		goto unlock_err;
	}

	 
	if (test_bit(EXTENT_FLAG_COMPRESSED, &em->flags) ||
	    em->block_start == EXTENT_MAP_INLINE) {
		free_extent_map(em);
		 
		ret = (flags & IOMAP_NOWAIT) ? -EAGAIN : -ENOTBLK;
		goto unlock_err;
	}

	len = min(len, em->len - (start - em->start));

	 
	if ((flags & IOMAP_NOWAIT) && len < length) {
		free_extent_map(em);
		ret = -EAGAIN;
		goto unlock_err;
	}

	if (write) {
		ret = btrfs_get_blocks_direct_write(&em, inode, dio_data,
						    start, &len, flags);
		if (ret < 0)
			goto unlock_err;
		unlock_extents = true;
		 
		len = min(len, em->len - (start - em->start));
		if (dio_data->data_space_reserved) {
			u64 release_offset;
			u64 release_len = 0;

			if (dio_data->nocow_done) {
				release_offset = start;
				release_len = data_alloc_len;
			} else if (len < data_alloc_len) {
				release_offset = start + len;
				release_len = data_alloc_len - len;
			}

			if (release_len > 0)
				btrfs_free_reserved_data_space(BTRFS_I(inode),
							       dio_data->data_reserved,
							       release_offset,
							       release_len);
		}
	} else {
		 
		lockstart = start + len;
		if (lockstart < lockend)
			unlock_extents = true;
	}

	if (unlock_extents)
		unlock_extent(&BTRFS_I(inode)->io_tree, lockstart, lockend,
			      &cached_state);
	else
		free_extent_state(cached_state);

	 
	if ((em->block_start == EXTENT_MAP_HOLE) ||
	    (test_bit(EXTENT_FLAG_PREALLOC, &em->flags) && !write)) {
		iomap->addr = IOMAP_NULL_ADDR;
		iomap->type = IOMAP_HOLE;
	} else {
		iomap->addr = em->block_start + (start - em->start);
		iomap->type = IOMAP_MAPPED;
	}
	iomap->offset = start;
	iomap->bdev = fs_info->fs_devices->latest_dev->bdev;
	iomap->length = len;
	free_extent_map(em);

	return 0;

unlock_err:
	unlock_extent(&BTRFS_I(inode)->io_tree, lockstart, lockend,
		      &cached_state);
err:
	if (dio_data->data_space_reserved) {
		btrfs_free_reserved_data_space(BTRFS_I(inode),
					       dio_data->data_reserved,
					       start, data_alloc_len);
		extent_changeset_free(dio_data->data_reserved);
	}

	return ret;
}

static int btrfs_dio_iomap_end(struct inode *inode, loff_t pos, loff_t length,
		ssize_t written, unsigned int flags, struct iomap *iomap)
{
	struct iomap_iter *iter = container_of(iomap, struct iomap_iter, iomap);
	struct btrfs_dio_data *dio_data = iter->private;
	size_t submitted = dio_data->submitted;
	const bool write = !!(flags & IOMAP_WRITE);
	int ret = 0;

	if (!write && (iomap->type == IOMAP_HOLE)) {
		 
		unlock_extent(&BTRFS_I(inode)->io_tree, pos, pos + length - 1,
			      NULL);
		return 0;
	}

	if (submitted < length) {
		pos += submitted;
		length -= submitted;
		if (write)
			btrfs_finish_ordered_extent(dio_data->ordered, NULL,
						    pos, length, false);
		else
			unlock_extent(&BTRFS_I(inode)->io_tree, pos,
				      pos + length - 1, NULL);
		ret = -ENOTBLK;
	}
	if (write) {
		btrfs_put_ordered_extent(dio_data->ordered);
		dio_data->ordered = NULL;
	}

	if (write)
		extent_changeset_free(dio_data->data_reserved);
	return ret;
}

static void btrfs_dio_end_io(struct btrfs_bio *bbio)
{
	struct btrfs_dio_private *dip =
		container_of(bbio, struct btrfs_dio_private, bbio);
	struct btrfs_inode *inode = bbio->inode;
	struct bio *bio = &bbio->bio;

	if (bio->bi_status) {
		btrfs_warn(inode->root->fs_info,
		"direct IO failed ino %llu op 0x%0x offset %#llx len %u err no %d",
			   btrfs_ino(inode), bio->bi_opf,
			   dip->file_offset, dip->bytes, bio->bi_status);
	}

	if (btrfs_op(bio) == BTRFS_MAP_WRITE) {
		btrfs_finish_ordered_extent(bbio->ordered, NULL,
					    dip->file_offset, dip->bytes,
					    !bio->bi_status);
	} else {
		unlock_extent(&inode->io_tree, dip->file_offset,
			      dip->file_offset + dip->bytes - 1, NULL);
	}

	bbio->bio.bi_private = bbio->private;
	iomap_dio_bio_end_io(bio);
}

static void btrfs_dio_submit_io(const struct iomap_iter *iter, struct bio *bio,
				loff_t file_offset)
{
	struct btrfs_bio *bbio = btrfs_bio(bio);
	struct btrfs_dio_private *dip =
		container_of(bbio, struct btrfs_dio_private, bbio);
	struct btrfs_dio_data *dio_data = iter->private;

	btrfs_bio_init(bbio, BTRFS_I(iter->inode)->root->fs_info,
		       btrfs_dio_end_io, bio->bi_private);
	bbio->inode = BTRFS_I(iter->inode);
	bbio->file_offset = file_offset;

	dip->file_offset = file_offset;
	dip->bytes = bio->bi_iter.bi_size;

	dio_data->submitted += bio->bi_iter.bi_size;

	 
	if (iter->flags & IOMAP_WRITE) {
		int ret;

		ret = btrfs_extract_ordered_extent(bbio, dio_data->ordered);
		if (ret) {
			btrfs_finish_ordered_extent(dio_data->ordered, NULL,
						    file_offset, dip->bytes,
						    !ret);
			bio->bi_status = errno_to_blk_status(ret);
			iomap_dio_bio_end_io(bio);
			return;
		}
	}

	btrfs_submit_bio(bbio, 0);
}

static const struct iomap_ops btrfs_dio_iomap_ops = {
	.iomap_begin            = btrfs_dio_iomap_begin,
	.iomap_end              = btrfs_dio_iomap_end,
};

static const struct iomap_dio_ops btrfs_dio_ops = {
	.submit_io		= btrfs_dio_submit_io,
	.bio_set		= &btrfs_dio_bioset,
};

ssize_t btrfs_dio_read(struct kiocb *iocb, struct iov_iter *iter, size_t done_before)
{
	struct btrfs_dio_data data = { 0 };

	return iomap_dio_rw(iocb, iter, &btrfs_dio_iomap_ops, &btrfs_dio_ops,
			    IOMAP_DIO_PARTIAL, &data, done_before);
}

struct iomap_dio *btrfs_dio_write(struct kiocb *iocb, struct iov_iter *iter,
				  size_t done_before)
{
	struct btrfs_dio_data data = { 0 };

	return __iomap_dio_rw(iocb, iter, &btrfs_dio_iomap_ops, &btrfs_dio_ops,
			    IOMAP_DIO_PARTIAL, &data, done_before);
}

static int btrfs_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
			u64 start, u64 len)
{
	int	ret;

	ret = fiemap_prep(inode, fieinfo, start, &len, 0);
	if (ret)
		return ret;

	 
	if (fieinfo->fi_flags & FIEMAP_FLAG_SYNC) {
		ret = btrfs_wait_ordered_range(inode, 0, LLONG_MAX);
		if (ret)
			return ret;
	}

	return extent_fiemap(BTRFS_I(inode), fieinfo, start, len);
}

static int btrfs_writepages(struct address_space *mapping,
			    struct writeback_control *wbc)
{
	return extent_writepages(mapping, wbc);
}

static void btrfs_readahead(struct readahead_control *rac)
{
	extent_readahead(rac);
}

 
static void wait_subpage_spinlock(struct page *page)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(page->mapping->host->i_sb);
	struct btrfs_subpage *subpage;

	if (!btrfs_is_subpage(fs_info, page))
		return;

	ASSERT(PagePrivate(page) && page->private);
	subpage = (struct btrfs_subpage *)page->private;

	 
	spin_lock_irq(&subpage->lock);
	spin_unlock_irq(&subpage->lock);
}

static bool __btrfs_release_folio(struct folio *folio, gfp_t gfp_flags)
{
	int ret = try_release_extent_mapping(&folio->page, gfp_flags);

	if (ret == 1) {
		wait_subpage_spinlock(&folio->page);
		clear_page_extent_mapped(&folio->page);
	}
	return ret;
}

static bool btrfs_release_folio(struct folio *folio, gfp_t gfp_flags)
{
	if (folio_test_writeback(folio) || folio_test_dirty(folio))
		return false;
	return __btrfs_release_folio(folio, gfp_flags);
}

#ifdef CONFIG_MIGRATION
static int btrfs_migrate_folio(struct address_space *mapping,
			     struct folio *dst, struct folio *src,
			     enum migrate_mode mode)
{
	int ret = filemap_migrate_folio(mapping, dst, src, mode);

	if (ret != MIGRATEPAGE_SUCCESS)
		return ret;

	if (folio_test_ordered(src)) {
		folio_clear_ordered(src);
		folio_set_ordered(dst);
	}

	return MIGRATEPAGE_SUCCESS;
}
#else
#define btrfs_migrate_folio NULL
#endif

static void btrfs_invalidate_folio(struct folio *folio, size_t offset,
				 size_t length)
{
	struct btrfs_inode *inode = BTRFS_I(folio->mapping->host);
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct extent_io_tree *tree = &inode->io_tree;
	struct extent_state *cached_state = NULL;
	u64 page_start = folio_pos(folio);
	u64 page_end = page_start + folio_size(folio) - 1;
	u64 cur;
	int inode_evicting = inode->vfs_inode.i_state & I_FREEING;

	 
	folio_wait_writeback(folio);
	wait_subpage_spinlock(&folio->page);

	 
	if (!(offset == 0 && length == folio_size(folio))) {
		btrfs_release_folio(folio, GFP_NOFS);
		return;
	}

	if (!inode_evicting)
		lock_extent(tree, page_start, page_end, &cached_state);

	cur = page_start;
	while (cur < page_end) {
		struct btrfs_ordered_extent *ordered;
		u64 range_end;
		u32 range_len;
		u32 extra_flags = 0;

		ordered = btrfs_lookup_first_ordered_range(inode, cur,
							   page_end + 1 - cur);
		if (!ordered) {
			range_end = page_end;
			 
			extra_flags = EXTENT_CLEAR_ALL_BITS;
			goto next;
		}
		if (ordered->file_offset > cur) {
			 
			range_end = ordered->file_offset - 1;
			extra_flags = EXTENT_CLEAR_ALL_BITS;
			goto next;
		}

		range_end = min(ordered->file_offset + ordered->num_bytes - 1,
				page_end);
		ASSERT(range_end + 1 - cur < U32_MAX);
		range_len = range_end + 1 - cur;
		if (!btrfs_page_test_ordered(fs_info, &folio->page, cur, range_len)) {
			 
			goto next;
		}
		btrfs_page_clear_ordered(fs_info, &folio->page, cur, range_len);

		 
		if (!inode_evicting)
			clear_extent_bit(tree, cur, range_end,
					 EXTENT_DELALLOC |
					 EXTENT_LOCKED | EXTENT_DO_ACCOUNTING |
					 EXTENT_DEFRAG, &cached_state);

		spin_lock_irq(&inode->ordered_tree.lock);
		set_bit(BTRFS_ORDERED_TRUNCATED, &ordered->flags);
		ordered->truncated_len = min(ordered->truncated_len,
					     cur - ordered->file_offset);
		spin_unlock_irq(&inode->ordered_tree.lock);

		 
		if (btrfs_dec_test_ordered_pending(inode, &ordered,
						   cur, range_end + 1 - cur)) {
			btrfs_finish_ordered_io(ordered);
			 
			extra_flags = EXTENT_CLEAR_ALL_BITS;
		}
next:
		if (ordered)
			btrfs_put_ordered_extent(ordered);
		 
		btrfs_qgroup_free_data(inode, NULL, cur, range_end + 1 - cur, NULL);
		if (!inode_evicting) {
			clear_extent_bit(tree, cur, range_end, EXTENT_LOCKED |
				 EXTENT_DELALLOC | EXTENT_UPTODATE |
				 EXTENT_DO_ACCOUNTING | EXTENT_DEFRAG |
				 extra_flags, &cached_state);
		}
		cur = range_end + 1;
	}
	 
	ASSERT(!folio_test_ordered(folio));
	btrfs_page_clear_checked(fs_info, &folio->page, folio_pos(folio), folio_size(folio));
	if (!inode_evicting)
		__btrfs_release_folio(folio, GFP_NOFS);
	clear_page_extent_mapped(&folio->page);
}

 
vm_fault_t btrfs_page_mkwrite(struct vm_fault *vmf)
{
	struct page *page = vmf->page;
	struct inode *inode = file_inode(vmf->vma->vm_file);
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	struct extent_changeset *data_reserved = NULL;
	unsigned long zero_start;
	loff_t size;
	vm_fault_t ret;
	int ret2;
	int reserved = 0;
	u64 reserved_space;
	u64 page_start;
	u64 page_end;
	u64 end;

	reserved_space = PAGE_SIZE;

	sb_start_pagefault(inode->i_sb);
	page_start = page_offset(page);
	page_end = page_start + PAGE_SIZE - 1;
	end = page_end;

	 
	ret2 = btrfs_delalloc_reserve_space(BTRFS_I(inode), &data_reserved,
					    page_start, reserved_space);
	if (!ret2) {
		ret2 = file_update_time(vmf->vma->vm_file);
		reserved = 1;
	}
	if (ret2) {
		ret = vmf_error(ret2);
		if (reserved)
			goto out;
		goto out_noreserve;
	}

	ret = VM_FAULT_NOPAGE;  
again:
	down_read(&BTRFS_I(inode)->i_mmap_lock);
	lock_page(page);
	size = i_size_read(inode);

	if ((page->mapping != inode->i_mapping) ||
	    (page_start >= size)) {
		 
		goto out_unlock;
	}
	wait_on_page_writeback(page);

	lock_extent(io_tree, page_start, page_end, &cached_state);
	ret2 = set_page_extent_mapped(page);
	if (ret2 < 0) {
		ret = vmf_error(ret2);
		unlock_extent(io_tree, page_start, page_end, &cached_state);
		goto out_unlock;
	}

	 
	ordered = btrfs_lookup_ordered_range(BTRFS_I(inode), page_start,
			PAGE_SIZE);
	if (ordered) {
		unlock_extent(io_tree, page_start, page_end, &cached_state);
		unlock_page(page);
		up_read(&BTRFS_I(inode)->i_mmap_lock);
		btrfs_start_ordered_extent(ordered);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	if (page->index == ((size - 1) >> PAGE_SHIFT)) {
		reserved_space = round_up(size - page_start,
					  fs_info->sectorsize);
		if (reserved_space < PAGE_SIZE) {
			end = page_start + reserved_space - 1;
			btrfs_delalloc_release_space(BTRFS_I(inode),
					data_reserved, page_start,
					PAGE_SIZE - reserved_space, true);
		}
	}

	 
	clear_extent_bit(&BTRFS_I(inode)->io_tree, page_start, end,
			  EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING |
			  EXTENT_DEFRAG, &cached_state);

	ret2 = btrfs_set_extent_delalloc(BTRFS_I(inode), page_start, end, 0,
					&cached_state);
	if (ret2) {
		unlock_extent(io_tree, page_start, page_end, &cached_state);
		ret = VM_FAULT_SIGBUS;
		goto out_unlock;
	}

	 
	if (page_start + PAGE_SIZE > size)
		zero_start = offset_in_page(size);
	else
		zero_start = PAGE_SIZE;

	if (zero_start != PAGE_SIZE)
		memzero_page(page, zero_start, PAGE_SIZE - zero_start);

	btrfs_page_clear_checked(fs_info, page, page_start, PAGE_SIZE);
	btrfs_page_set_dirty(fs_info, page, page_start, end + 1 - page_start);
	btrfs_page_set_uptodate(fs_info, page, page_start, end + 1 - page_start);

	btrfs_set_inode_last_sub_trans(BTRFS_I(inode));

	unlock_extent(io_tree, page_start, page_end, &cached_state);
	up_read(&BTRFS_I(inode)->i_mmap_lock);

	btrfs_delalloc_release_extents(BTRFS_I(inode), PAGE_SIZE);
	sb_end_pagefault(inode->i_sb);
	extent_changeset_free(data_reserved);
	return VM_FAULT_LOCKED;

out_unlock:
	unlock_page(page);
	up_read(&BTRFS_I(inode)->i_mmap_lock);
out:
	btrfs_delalloc_release_extents(BTRFS_I(inode), PAGE_SIZE);
	btrfs_delalloc_release_space(BTRFS_I(inode), data_reserved, page_start,
				     reserved_space, (ret != 0));
out_noreserve:
	sb_end_pagefault(inode->i_sb);
	extent_changeset_free(data_reserved);
	return ret;
}

static int btrfs_truncate(struct btrfs_inode *inode, bool skip_writeback)
{
	struct btrfs_truncate_control control = {
		.inode = inode,
		.ino = btrfs_ino(inode),
		.min_type = BTRFS_EXTENT_DATA_KEY,
		.clear_extent_range = true,
	};
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_rsv *rsv;
	int ret;
	struct btrfs_trans_handle *trans;
	u64 mask = fs_info->sectorsize - 1;
	const u64 min_size = btrfs_calc_metadata_size(fs_info, 1);

	if (!skip_writeback) {
		ret = btrfs_wait_ordered_range(&inode->vfs_inode,
					       inode->vfs_inode.i_size & (~mask),
					       (u64)-1);
		if (ret)
			return ret;
	}

	 
	rsv = btrfs_alloc_block_rsv(fs_info, BTRFS_BLOCK_RSV_TEMP);
	if (!rsv)
		return -ENOMEM;
	rsv->size = min_size;
	rsv->failfast = true;

	 
	trans = btrfs_start_transaction(root, 2);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out;
	}

	 
	ret = btrfs_block_rsv_migrate(&fs_info->trans_block_rsv, rsv,
				      min_size, false);
	 
	if (WARN_ON(ret)) {
		btrfs_end_transaction(trans);
		goto out;
	}

	trans->block_rsv = rsv;

	while (1) {
		struct extent_state *cached_state = NULL;
		const u64 new_size = inode->vfs_inode.i_size;
		const u64 lock_start = ALIGN_DOWN(new_size, fs_info->sectorsize);

		control.new_size = new_size;
		lock_extent(&inode->io_tree, lock_start, (u64)-1, &cached_state);
		 
		btrfs_drop_extent_map_range(inode,
					    ALIGN(new_size, fs_info->sectorsize),
					    (u64)-1, false);

		ret = btrfs_truncate_inode_items(trans, root, &control);

		inode_sub_bytes(&inode->vfs_inode, control.sub_bytes);
		btrfs_inode_safe_disk_i_size_write(inode, control.last_size);

		unlock_extent(&inode->io_tree, lock_start, (u64)-1, &cached_state);

		trans->block_rsv = &fs_info->trans_block_rsv;
		if (ret != -ENOSPC && ret != -EAGAIN)
			break;

		ret = btrfs_update_inode(trans, root, inode);
		if (ret)
			break;

		btrfs_end_transaction(trans);
		btrfs_btree_balance_dirty(fs_info);

		trans = btrfs_start_transaction(root, 2);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			trans = NULL;
			break;
		}

		btrfs_block_rsv_release(fs_info, rsv, -1, NULL);
		ret = btrfs_block_rsv_migrate(&fs_info->trans_block_rsv,
					      rsv, min_size, false);
		 
		if (WARN_ON(ret))
			break;

		trans->block_rsv = rsv;
	}

	 
	if (ret == BTRFS_NEED_TRUNCATE_BLOCK) {
		btrfs_end_transaction(trans);
		btrfs_btree_balance_dirty(fs_info);

		ret = btrfs_truncate_block(inode, inode->vfs_inode.i_size, 0, 0);
		if (ret)
			goto out;
		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			goto out;
		}
		btrfs_inode_safe_disk_i_size_write(inode, 0);
	}

	if (trans) {
		int ret2;

		trans->block_rsv = &fs_info->trans_block_rsv;
		ret2 = btrfs_update_inode(trans, root, inode);
		if (ret2 && !ret)
			ret = ret2;

		ret2 = btrfs_end_transaction(trans);
		if (ret2 && !ret)
			ret = ret2;
		btrfs_btree_balance_dirty(fs_info);
	}
out:
	btrfs_free_block_rsv(fs_info, rsv);
	 
	if (control.extents_found > 0)
		btrfs_set_inode_full_sync(inode);

	return ret;
}

struct inode *btrfs_new_subvol_inode(struct mnt_idmap *idmap,
				     struct inode *dir)
{
	struct inode *inode;

	inode = new_inode(dir->i_sb);
	if (inode) {
		 
		inode_init_owner(idmap, inode, NULL,
				 S_IFDIR | (~current_umask() & S_IRWXUGO));
		inode->i_op = &btrfs_dir_inode_operations;
		inode->i_fop = &btrfs_dir_file_operations;
	}
	return inode;
}

struct inode *btrfs_alloc_inode(struct super_block *sb)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(sb);
	struct btrfs_inode *ei;
	struct inode *inode;

	ei = alloc_inode_sb(sb, btrfs_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;

	ei->root = NULL;
	ei->generation = 0;
	ei->last_trans = 0;
	ei->last_sub_trans = 0;
	ei->logged_trans = 0;
	ei->delalloc_bytes = 0;
	ei->new_delalloc_bytes = 0;
	ei->defrag_bytes = 0;
	ei->disk_i_size = 0;
	ei->flags = 0;
	ei->ro_flags = 0;
	ei->csum_bytes = 0;
	ei->index_cnt = (u64)-1;
	ei->dir_index = 0;
	ei->last_unlink_trans = 0;
	ei->last_reflink_trans = 0;
	ei->last_log_commit = 0;

	spin_lock_init(&ei->lock);
	ei->outstanding_extents = 0;
	if (sb->s_magic != BTRFS_TEST_MAGIC)
		btrfs_init_metadata_block_rsv(fs_info, &ei->block_rsv,
					      BTRFS_BLOCK_RSV_DELALLOC);
	ei->runtime_flags = 0;
	ei->prop_compress = BTRFS_COMPRESS_NONE;
	ei->defrag_compress = BTRFS_COMPRESS_NONE;

	ei->delayed_node = NULL;

	ei->i_otime.tv_sec = 0;
	ei->i_otime.tv_nsec = 0;

	inode = &ei->vfs_inode;
	extent_map_tree_init(&ei->extent_tree);
	extent_io_tree_init(fs_info, &ei->io_tree, IO_TREE_INODE_IO);
	ei->io_tree.inode = ei;
	extent_io_tree_init(fs_info, &ei->file_extent_tree,
			    IO_TREE_INODE_FILE_EXTENT);
	mutex_init(&ei->log_mutex);
	btrfs_ordered_inode_tree_init(&ei->ordered_tree);
	INIT_LIST_HEAD(&ei->delalloc_inodes);
	INIT_LIST_HEAD(&ei->delayed_iput);
	RB_CLEAR_NODE(&ei->rb_node);
	init_rwsem(&ei->i_mmap_lock);

	return inode;
}

#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
void btrfs_test_destroy_inode(struct inode *inode)
{
	btrfs_drop_extent_map_range(BTRFS_I(inode), 0, (u64)-1, false);
	kmem_cache_free(btrfs_inode_cachep, BTRFS_I(inode));
}
#endif

void btrfs_free_inode(struct inode *inode)
{
	kmem_cache_free(btrfs_inode_cachep, BTRFS_I(inode));
}

void btrfs_destroy_inode(struct inode *vfs_inode)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_inode *inode = BTRFS_I(vfs_inode);
	struct btrfs_root *root = inode->root;
	bool freespace_inode;

	WARN_ON(!hlist_empty(&vfs_inode->i_dentry));
	WARN_ON(vfs_inode->i_data.nrpages);
	WARN_ON(inode->block_rsv.reserved);
	WARN_ON(inode->block_rsv.size);
	WARN_ON(inode->outstanding_extents);
	if (!S_ISDIR(vfs_inode->i_mode)) {
		WARN_ON(inode->delalloc_bytes);
		WARN_ON(inode->new_delalloc_bytes);
	}
	WARN_ON(inode->csum_bytes);
	WARN_ON(inode->defrag_bytes);

	 
	if (!root)
		return;

	 
	freespace_inode = btrfs_is_free_space_inode(inode);

	while (1) {
		ordered = btrfs_lookup_first_ordered_extent(inode, (u64)-1);
		if (!ordered)
			break;
		else {
			btrfs_err(root->fs_info,
				  "found ordered extent %llu %llu on inode cleanup",
				  ordered->file_offset, ordered->num_bytes);

			if (!freespace_inode)
				btrfs_lockdep_acquire(root->fs_info, btrfs_ordered_extent);

			btrfs_remove_ordered_extent(inode, ordered);
			btrfs_put_ordered_extent(ordered);
			btrfs_put_ordered_extent(ordered);
		}
	}
	btrfs_qgroup_check_reserved_leak(inode);
	inode_tree_del(inode);
	btrfs_drop_extent_map_range(inode, 0, (u64)-1, false);
	btrfs_inode_clear_file_extent_range(inode, 0, (u64)-1);
	btrfs_put_root(inode->root);
}

int btrfs_drop_inode(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;

	if (root == NULL)
		return 1;

	 
	if (btrfs_root_refs(&root->root_item) == 0)
		return 1;
	else
		return generic_drop_inode(inode);
}

static void init_once(void *foo)
{
	struct btrfs_inode *ei = foo;

	inode_init_once(&ei->vfs_inode);
}

void __cold btrfs_destroy_cachep(void)
{
	 
	rcu_barrier();
	bioset_exit(&btrfs_dio_bioset);
	kmem_cache_destroy(btrfs_inode_cachep);
}

int __init btrfs_init_cachep(void)
{
	btrfs_inode_cachep = kmem_cache_create("btrfs_inode",
			sizeof(struct btrfs_inode), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD | SLAB_ACCOUNT,
			init_once);
	if (!btrfs_inode_cachep)
		goto fail;

	if (bioset_init(&btrfs_dio_bioset, BIO_POOL_SIZE,
			offsetof(struct btrfs_dio_private, bbio.bio),
			BIOSET_NEED_BVECS))
		goto fail;

	return 0;
fail:
	btrfs_destroy_cachep();
	return -ENOMEM;
}

static int btrfs_getattr(struct mnt_idmap *idmap,
			 const struct path *path, struct kstat *stat,
			 u32 request_mask, unsigned int flags)
{
	u64 delalloc_bytes;
	u64 inode_bytes;
	struct inode *inode = d_inode(path->dentry);
	u32 blocksize = inode->i_sb->s_blocksize;
	u32 bi_flags = BTRFS_I(inode)->flags;
	u32 bi_ro_flags = BTRFS_I(inode)->ro_flags;

	stat->result_mask |= STATX_BTIME;
	stat->btime.tv_sec = BTRFS_I(inode)->i_otime.tv_sec;
	stat->btime.tv_nsec = BTRFS_I(inode)->i_otime.tv_nsec;
	if (bi_flags & BTRFS_INODE_APPEND)
		stat->attributes |= STATX_ATTR_APPEND;
	if (bi_flags & BTRFS_INODE_COMPRESS)
		stat->attributes |= STATX_ATTR_COMPRESSED;
	if (bi_flags & BTRFS_INODE_IMMUTABLE)
		stat->attributes |= STATX_ATTR_IMMUTABLE;
	if (bi_flags & BTRFS_INODE_NODUMP)
		stat->attributes |= STATX_ATTR_NODUMP;
	if (bi_ro_flags & BTRFS_INODE_RO_VERITY)
		stat->attributes |= STATX_ATTR_VERITY;

	stat->attributes_mask |= (STATX_ATTR_APPEND |
				  STATX_ATTR_COMPRESSED |
				  STATX_ATTR_IMMUTABLE |
				  STATX_ATTR_NODUMP);

	generic_fillattr(idmap, request_mask, inode, stat);
	stat->dev = BTRFS_I(inode)->root->anon_dev;

	spin_lock(&BTRFS_I(inode)->lock);
	delalloc_bytes = BTRFS_I(inode)->new_delalloc_bytes;
	inode_bytes = inode_get_bytes(inode);
	spin_unlock(&BTRFS_I(inode)->lock);
	stat->blocks = (ALIGN(inode_bytes, blocksize) +
			ALIGN(delalloc_bytes, blocksize)) >> SECTOR_SHIFT;
	return 0;
}

static int btrfs_rename_exchange(struct inode *old_dir,
			      struct dentry *old_dentry,
			      struct inode *new_dir,
			      struct dentry *new_dentry)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(old_dir->i_sb);
	struct btrfs_trans_handle *trans;
	unsigned int trans_num_items;
	struct btrfs_root *root = BTRFS_I(old_dir)->root;
	struct btrfs_root *dest = BTRFS_I(new_dir)->root;
	struct inode *new_inode = new_dentry->d_inode;
	struct inode *old_inode = old_dentry->d_inode;
	struct btrfs_rename_ctx old_rename_ctx;
	struct btrfs_rename_ctx new_rename_ctx;
	u64 old_ino = btrfs_ino(BTRFS_I(old_inode));
	u64 new_ino = btrfs_ino(BTRFS_I(new_inode));
	u64 old_idx = 0;
	u64 new_idx = 0;
	int ret;
	int ret2;
	bool need_abort = false;
	struct fscrypt_name old_fname, new_fname;
	struct fscrypt_str *old_name, *new_name;

	 
	if (root != dest &&
	    (old_ino != BTRFS_FIRST_FREE_OBJECTID ||
	     new_ino != BTRFS_FIRST_FREE_OBJECTID))
		return -EXDEV;

	ret = fscrypt_setup_filename(old_dir, &old_dentry->d_name, 0, &old_fname);
	if (ret)
		return ret;

	ret = fscrypt_setup_filename(new_dir, &new_dentry->d_name, 0, &new_fname);
	if (ret) {
		fscrypt_free_filename(&old_fname);
		return ret;
	}

	old_name = &old_fname.disk_name;
	new_name = &new_fname.disk_name;

	 
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID ||
	    new_ino == BTRFS_FIRST_FREE_OBJECTID)
		down_read(&fs_info->subvol_sem);

	 
	trans_num_items = (old_dir == new_dir ? 9 : 10);
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID) {
		 
		trans_num_items += 4;
	} else {
		 
		trans_num_items += 3;
	}
	if (new_ino == BTRFS_FIRST_FREE_OBJECTID)
		trans_num_items += 4;
	else
		trans_num_items += 3;
	trans = btrfs_start_transaction(root, trans_num_items);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_notrans;
	}

	if (dest != root) {
		ret = btrfs_record_root_in_trans(trans, dest);
		if (ret)
			goto out_fail;
	}

	 
	ret = btrfs_set_inode_index(BTRFS_I(new_dir), &old_idx);
	if (ret)
		goto out_fail;
	ret = btrfs_set_inode_index(BTRFS_I(old_dir), &new_idx);
	if (ret)
		goto out_fail;

	BTRFS_I(old_inode)->dir_index = 0ULL;
	BTRFS_I(new_inode)->dir_index = 0ULL;

	 
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID) {
		 
		btrfs_set_log_full_commit(trans);
	} else {
		ret = btrfs_insert_inode_ref(trans, dest, new_name, old_ino,
					     btrfs_ino(BTRFS_I(new_dir)),
					     old_idx);
		if (ret)
			goto out_fail;
		need_abort = true;
	}

	 
	if (new_ino == BTRFS_FIRST_FREE_OBJECTID) {
		 
		btrfs_set_log_full_commit(trans);
	} else {
		ret = btrfs_insert_inode_ref(trans, root, old_name, new_ino,
					     btrfs_ino(BTRFS_I(old_dir)),
					     new_idx);
		if (ret) {
			if (need_abort)
				btrfs_abort_transaction(trans, ret);
			goto out_fail;
		}
	}

	 
	inode_inc_iversion(old_dir);
	inode_inc_iversion(new_dir);
	inode_inc_iversion(old_inode);
	inode_inc_iversion(new_inode);
	simple_rename_timestamp(old_dir, old_dentry, new_dir, new_dentry);

	if (old_dentry->d_parent != new_dentry->d_parent) {
		btrfs_record_unlink_dir(trans, BTRFS_I(old_dir),
					BTRFS_I(old_inode), true);
		btrfs_record_unlink_dir(trans, BTRFS_I(new_dir),
					BTRFS_I(new_inode), true);
	}

	 
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID) {
		ret = btrfs_unlink_subvol(trans, BTRFS_I(old_dir), old_dentry);
	} else {  
		ret = __btrfs_unlink_inode(trans, BTRFS_I(old_dir),
					   BTRFS_I(old_dentry->d_inode),
					   old_name, &old_rename_ctx);
		if (!ret)
			ret = btrfs_update_inode(trans, root, BTRFS_I(old_inode));
	}
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_fail;
	}

	 
	if (new_ino == BTRFS_FIRST_FREE_OBJECTID) {
		ret = btrfs_unlink_subvol(trans, BTRFS_I(new_dir), new_dentry);
	} else {  
		ret = __btrfs_unlink_inode(trans, BTRFS_I(new_dir),
					   BTRFS_I(new_dentry->d_inode),
					   new_name, &new_rename_ctx);
		if (!ret)
			ret = btrfs_update_inode(trans, dest, BTRFS_I(new_inode));
	}
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_fail;
	}

	ret = btrfs_add_link(trans, BTRFS_I(new_dir), BTRFS_I(old_inode),
			     new_name, 0, old_idx);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_fail;
	}

	ret = btrfs_add_link(trans, BTRFS_I(old_dir), BTRFS_I(new_inode),
			     old_name, 0, new_idx);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_fail;
	}

	if (old_inode->i_nlink == 1)
		BTRFS_I(old_inode)->dir_index = old_idx;
	if (new_inode->i_nlink == 1)
		BTRFS_I(new_inode)->dir_index = new_idx;

	 
	if (old_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_pin_log_trans(root);
	if (new_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_pin_log_trans(dest);

	 
	if (old_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_log_new_name(trans, old_dentry, BTRFS_I(old_dir),
				   old_rename_ctx.index, new_dentry->d_parent);
	if (new_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_log_new_name(trans, new_dentry, BTRFS_I(new_dir),
				   new_rename_ctx.index, old_dentry->d_parent);

	 
	if (old_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_end_log_trans(root);
	if (new_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_end_log_trans(dest);
out_fail:
	ret2 = btrfs_end_transaction(trans);
	ret = ret ? ret : ret2;
out_notrans:
	if (new_ino == BTRFS_FIRST_FREE_OBJECTID ||
	    old_ino == BTRFS_FIRST_FREE_OBJECTID)
		up_read(&fs_info->subvol_sem);

	fscrypt_free_filename(&new_fname);
	fscrypt_free_filename(&old_fname);
	return ret;
}

static struct inode *new_whiteout_inode(struct mnt_idmap *idmap,
					struct inode *dir)
{
	struct inode *inode;

	inode = new_inode(dir->i_sb);
	if (inode) {
		inode_init_owner(idmap, inode, dir,
				 S_IFCHR | WHITEOUT_MODE);
		inode->i_op = &btrfs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, WHITEOUT_DEV);
	}
	return inode;
}

static int btrfs_rename(struct mnt_idmap *idmap,
			struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry,
			unsigned int flags)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(old_dir->i_sb);
	struct btrfs_new_inode_args whiteout_args = {
		.dir = old_dir,
		.dentry = old_dentry,
	};
	struct btrfs_trans_handle *trans;
	unsigned int trans_num_items;
	struct btrfs_root *root = BTRFS_I(old_dir)->root;
	struct btrfs_root *dest = BTRFS_I(new_dir)->root;
	struct inode *new_inode = d_inode(new_dentry);
	struct inode *old_inode = d_inode(old_dentry);
	struct btrfs_rename_ctx rename_ctx;
	u64 index = 0;
	int ret;
	int ret2;
	u64 old_ino = btrfs_ino(BTRFS_I(old_inode));
	struct fscrypt_name old_fname, new_fname;

	if (btrfs_ino(BTRFS_I(new_dir)) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)
		return -EPERM;

	 
	if (old_ino != BTRFS_FIRST_FREE_OBJECTID && root != dest)
		return -EXDEV;

	if (old_ino == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID ||
	    (new_inode && btrfs_ino(BTRFS_I(new_inode)) == BTRFS_FIRST_FREE_OBJECTID))
		return -ENOTEMPTY;

	if (S_ISDIR(old_inode->i_mode) && new_inode &&
	    new_inode->i_size > BTRFS_EMPTY_DIR_SIZE)
		return -ENOTEMPTY;

	ret = fscrypt_setup_filename(old_dir, &old_dentry->d_name, 0, &old_fname);
	if (ret)
		return ret;

	ret = fscrypt_setup_filename(new_dir, &new_dentry->d_name, 0, &new_fname);
	if (ret) {
		fscrypt_free_filename(&old_fname);
		return ret;
	}

	 
	ret = btrfs_check_dir_item_collision(dest, new_dir->i_ino, &new_fname.disk_name);
	if (ret) {
		if (ret == -EEXIST) {
			 
			if (WARN_ON(!new_inode)) {
				goto out_fscrypt_names;
			}
		} else {
			 
			goto out_fscrypt_names;
		}
	}
	ret = 0;

	 
	if (new_inode && S_ISREG(old_inode->i_mode) && new_inode->i_size)
		filemap_flush(old_inode->i_mapping);

	if (flags & RENAME_WHITEOUT) {
		whiteout_args.inode = new_whiteout_inode(idmap, old_dir);
		if (!whiteout_args.inode) {
			ret = -ENOMEM;
			goto out_fscrypt_names;
		}
		ret = btrfs_new_inode_prepare(&whiteout_args, &trans_num_items);
		if (ret)
			goto out_whiteout_inode;
	} else {
		 
		trans_num_items = 1;
	}

	if (old_ino == BTRFS_FIRST_FREE_OBJECTID) {
		 
		down_read(&fs_info->subvol_sem);
		 
		trans_num_items += 4;
	} else {
		 
		trans_num_items += 3;
	}
	 
	trans_num_items += 4;
	 
	if (new_dir != old_dir)
		trans_num_items++;
	if (new_inode) {
		 
		trans_num_items += 5;
	}
	trans = btrfs_start_transaction(root, trans_num_items);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_notrans;
	}

	if (dest != root) {
		ret = btrfs_record_root_in_trans(trans, dest);
		if (ret)
			goto out_fail;
	}

	ret = btrfs_set_inode_index(BTRFS_I(new_dir), &index);
	if (ret)
		goto out_fail;

	BTRFS_I(old_inode)->dir_index = 0ULL;
	if (unlikely(old_ino == BTRFS_FIRST_FREE_OBJECTID)) {
		 
		btrfs_set_log_full_commit(trans);
	} else {
		ret = btrfs_insert_inode_ref(trans, dest, &new_fname.disk_name,
					     old_ino, btrfs_ino(BTRFS_I(new_dir)),
					     index);
		if (ret)
			goto out_fail;
	}

	inode_inc_iversion(old_dir);
	inode_inc_iversion(new_dir);
	inode_inc_iversion(old_inode);
	simple_rename_timestamp(old_dir, old_dentry, new_dir, new_dentry);

	if (old_dentry->d_parent != new_dentry->d_parent)
		btrfs_record_unlink_dir(trans, BTRFS_I(old_dir),
					BTRFS_I(old_inode), true);

	if (unlikely(old_ino == BTRFS_FIRST_FREE_OBJECTID)) {
		ret = btrfs_unlink_subvol(trans, BTRFS_I(old_dir), old_dentry);
	} else {
		ret = __btrfs_unlink_inode(trans, BTRFS_I(old_dir),
					   BTRFS_I(d_inode(old_dentry)),
					   &old_fname.disk_name, &rename_ctx);
		if (!ret)
			ret = btrfs_update_inode(trans, root, BTRFS_I(old_inode));
	}
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_fail;
	}

	if (new_inode) {
		inode_inc_iversion(new_inode);
		if (unlikely(btrfs_ino(BTRFS_I(new_inode)) ==
			     BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)) {
			ret = btrfs_unlink_subvol(trans, BTRFS_I(new_dir), new_dentry);
			BUG_ON(new_inode->i_nlink == 0);
		} else {
			ret = btrfs_unlink_inode(trans, BTRFS_I(new_dir),
						 BTRFS_I(d_inode(new_dentry)),
						 &new_fname.disk_name);
		}
		if (!ret && new_inode->i_nlink == 0)
			ret = btrfs_orphan_add(trans,
					BTRFS_I(d_inode(new_dentry)));
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out_fail;
		}
	}

	ret = btrfs_add_link(trans, BTRFS_I(new_dir), BTRFS_I(old_inode),
			     &new_fname.disk_name, 0, index);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out_fail;
	}

	if (old_inode->i_nlink == 1)
		BTRFS_I(old_inode)->dir_index = index;

	if (old_ino != BTRFS_FIRST_FREE_OBJECTID)
		btrfs_log_new_name(trans, old_dentry, BTRFS_I(old_dir),
				   rename_ctx.index, new_dentry->d_parent);

	if (flags & RENAME_WHITEOUT) {
		ret = btrfs_create_new_inode(trans, &whiteout_args);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out_fail;
		} else {
			unlock_new_inode(whiteout_args.inode);
			iput(whiteout_args.inode);
			whiteout_args.inode = NULL;
		}
	}
out_fail:
	ret2 = btrfs_end_transaction(trans);
	ret = ret ? ret : ret2;
out_notrans:
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID)
		up_read(&fs_info->subvol_sem);
	if (flags & RENAME_WHITEOUT)
		btrfs_new_inode_args_destroy(&whiteout_args);
out_whiteout_inode:
	if (flags & RENAME_WHITEOUT)
		iput(whiteout_args.inode);
out_fscrypt_names:
	fscrypt_free_filename(&old_fname);
	fscrypt_free_filename(&new_fname);
	return ret;
}

static int btrfs_rename2(struct mnt_idmap *idmap, struct inode *old_dir,
			 struct dentry *old_dentry, struct inode *new_dir,
			 struct dentry *new_dentry, unsigned int flags)
{
	int ret;

	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE | RENAME_WHITEOUT))
		return -EINVAL;

	if (flags & RENAME_EXCHANGE)
		ret = btrfs_rename_exchange(old_dir, old_dentry, new_dir,
					    new_dentry);
	else
		ret = btrfs_rename(idmap, old_dir, old_dentry, new_dir,
				   new_dentry, flags);

	btrfs_btree_balance_dirty(BTRFS_I(new_dir)->root->fs_info);

	return ret;
}

struct btrfs_delalloc_work {
	struct inode *inode;
	struct completion completion;
	struct list_head list;
	struct btrfs_work work;
};

static void btrfs_run_delalloc_work(struct btrfs_work *work)
{
	struct btrfs_delalloc_work *delalloc_work;
	struct inode *inode;

	delalloc_work = container_of(work, struct btrfs_delalloc_work,
				     work);
	inode = delalloc_work->inode;
	filemap_flush(inode->i_mapping);
	if (test_bit(BTRFS_INODE_HAS_ASYNC_EXTENT,
				&BTRFS_I(inode)->runtime_flags))
		filemap_flush(inode->i_mapping);

	iput(inode);
	complete(&delalloc_work->completion);
}

static struct btrfs_delalloc_work *btrfs_alloc_delalloc_work(struct inode *inode)
{
	struct btrfs_delalloc_work *work;

	work = kmalloc(sizeof(*work), GFP_NOFS);
	if (!work)
		return NULL;

	init_completion(&work->completion);
	INIT_LIST_HEAD(&work->list);
	work->inode = inode;
	btrfs_init_work(&work->work, btrfs_run_delalloc_work, NULL, NULL);

	return work;
}

 
static int start_delalloc_inodes(struct btrfs_root *root,
				 struct writeback_control *wbc, bool snapshot,
				 bool in_reclaim_context)
{
	struct btrfs_inode *binode;
	struct inode *inode;
	struct btrfs_delalloc_work *work, *next;
	LIST_HEAD(works);
	LIST_HEAD(splice);
	int ret = 0;
	bool full_flush = wbc->nr_to_write == LONG_MAX;

	mutex_lock(&root->delalloc_mutex);
	spin_lock(&root->delalloc_lock);
	list_splice_init(&root->delalloc_inodes, &splice);
	while (!list_empty(&splice)) {
		binode = list_entry(splice.next, struct btrfs_inode,
				    delalloc_inodes);

		list_move_tail(&binode->delalloc_inodes,
			       &root->delalloc_inodes);

		if (in_reclaim_context &&
		    test_bit(BTRFS_INODE_NO_DELALLOC_FLUSH, &binode->runtime_flags))
			continue;

		inode = igrab(&binode->vfs_inode);
		if (!inode) {
			cond_resched_lock(&root->delalloc_lock);
			continue;
		}
		spin_unlock(&root->delalloc_lock);

		if (snapshot)
			set_bit(BTRFS_INODE_SNAPSHOT_FLUSH,
				&binode->runtime_flags);
		if (full_flush) {
			work = btrfs_alloc_delalloc_work(inode);
			if (!work) {
				iput(inode);
				ret = -ENOMEM;
				goto out;
			}
			list_add_tail(&work->list, &works);
			btrfs_queue_work(root->fs_info->flush_workers,
					 &work->work);
		} else {
			ret = filemap_fdatawrite_wbc(inode->i_mapping, wbc);
			btrfs_add_delayed_iput(BTRFS_I(inode));
			if (ret || wbc->nr_to_write <= 0)
				goto out;
		}
		cond_resched();
		spin_lock(&root->delalloc_lock);
	}
	spin_unlock(&root->delalloc_lock);

out:
	list_for_each_entry_safe(work, next, &works, list) {
		list_del_init(&work->list);
		wait_for_completion(&work->completion);
		kfree(work);
	}

	if (!list_empty(&splice)) {
		spin_lock(&root->delalloc_lock);
		list_splice_tail(&splice, &root->delalloc_inodes);
		spin_unlock(&root->delalloc_lock);
	}
	mutex_unlock(&root->delalloc_mutex);
	return ret;
}

int btrfs_start_delalloc_snapshot(struct btrfs_root *root, bool in_reclaim_context)
{
	struct writeback_control wbc = {
		.nr_to_write = LONG_MAX,
		.sync_mode = WB_SYNC_NONE,
		.range_start = 0,
		.range_end = LLONG_MAX,
	};
	struct btrfs_fs_info *fs_info = root->fs_info;

	if (BTRFS_FS_ERROR(fs_info))
		return -EROFS;

	return start_delalloc_inodes(root, &wbc, true, in_reclaim_context);
}

int btrfs_start_delalloc_roots(struct btrfs_fs_info *fs_info, long nr,
			       bool in_reclaim_context)
{
	struct writeback_control wbc = {
		.nr_to_write = nr,
		.sync_mode = WB_SYNC_NONE,
		.range_start = 0,
		.range_end = LLONG_MAX,
	};
	struct btrfs_root *root;
	LIST_HEAD(splice);
	int ret;

	if (BTRFS_FS_ERROR(fs_info))
		return -EROFS;

	mutex_lock(&fs_info->delalloc_root_mutex);
	spin_lock(&fs_info->delalloc_root_lock);
	list_splice_init(&fs_info->delalloc_roots, &splice);
	while (!list_empty(&splice)) {
		 
		if (nr == LONG_MAX)
			wbc.nr_to_write = LONG_MAX;

		root = list_first_entry(&splice, struct btrfs_root,
					delalloc_root);
		root = btrfs_grab_root(root);
		BUG_ON(!root);
		list_move_tail(&root->delalloc_root,
			       &fs_info->delalloc_roots);
		spin_unlock(&fs_info->delalloc_root_lock);

		ret = start_delalloc_inodes(root, &wbc, false, in_reclaim_context);
		btrfs_put_root(root);
		if (ret < 0 || wbc.nr_to_write <= 0)
			goto out;
		spin_lock(&fs_info->delalloc_root_lock);
	}
	spin_unlock(&fs_info->delalloc_root_lock);

	ret = 0;
out:
	if (!list_empty(&splice)) {
		spin_lock(&fs_info->delalloc_root_lock);
		list_splice_tail(&splice, &fs_info->delalloc_roots);
		spin_unlock(&fs_info->delalloc_root_lock);
	}
	mutex_unlock(&fs_info->delalloc_root_mutex);
	return ret;
}

static int btrfs_symlink(struct mnt_idmap *idmap, struct inode *dir,
			 struct dentry *dentry, const char *symname)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(dir->i_sb);
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct inode *inode;
	struct btrfs_new_inode_args new_inode_args = {
		.dir = dir,
		.dentry = dentry,
	};
	unsigned int trans_num_items;
	int err;
	int name_len;
	int datasize;
	unsigned long ptr;
	struct btrfs_file_extent_item *ei;
	struct extent_buffer *leaf;

	name_len = strlen(symname);
	if (name_len > BTRFS_MAX_INLINE_DATA_SIZE(fs_info))
		return -ENAMETOOLONG;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;
	inode_init_owner(idmap, inode, dir, S_IFLNK | S_IRWXUGO);
	inode->i_op = &btrfs_symlink_inode_operations;
	inode_nohighmem(inode);
	inode->i_mapping->a_ops = &btrfs_aops;
	btrfs_i_size_write(BTRFS_I(inode), name_len);
	inode_set_bytes(inode, name_len);

	new_inode_args.inode = inode;
	err = btrfs_new_inode_prepare(&new_inode_args, &trans_num_items);
	if (err)
		goto out_inode;
	 
	trans_num_items++;

	trans = btrfs_start_transaction(root, trans_num_items);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out_new_inode_args;
	}

	err = btrfs_create_new_inode(trans, &new_inode_args);
	if (err)
		goto out;

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		btrfs_abort_transaction(trans, err);
		discard_new_inode(inode);
		inode = NULL;
		goto out;
	}
	key.objectid = btrfs_ino(BTRFS_I(inode));
	key.offset = 0;
	key.type = BTRFS_EXTENT_DATA_KEY;
	datasize = btrfs_file_extent_calc_inline_size(name_len);
	err = btrfs_insert_empty_item(trans, root, path, &key,
				      datasize);
	if (err) {
		btrfs_abort_transaction(trans, err);
		btrfs_free_path(path);
		discard_new_inode(inode);
		inode = NULL;
		goto out;
	}
	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, ei, trans->transid);
	btrfs_set_file_extent_type(leaf, ei,
				   BTRFS_FILE_EXTENT_INLINE);
	btrfs_set_file_extent_encryption(leaf, ei, 0);
	btrfs_set_file_extent_compression(leaf, ei, 0);
	btrfs_set_file_extent_other_encoding(leaf, ei, 0);
	btrfs_set_file_extent_ram_bytes(leaf, ei, name_len);

	ptr = btrfs_file_extent_inline_start(ei);
	write_extent_buffer(leaf, symname, ptr, name_len);
	btrfs_mark_buffer_dirty(trans, leaf);
	btrfs_free_path(path);

	d_instantiate_new(dentry, inode);
	err = 0;
out:
	btrfs_end_transaction(trans);
	btrfs_btree_balance_dirty(fs_info);
out_new_inode_args:
	btrfs_new_inode_args_destroy(&new_inode_args);
out_inode:
	if (err)
		iput(inode);
	return err;
}

static struct btrfs_trans_handle *insert_prealloc_file_extent(
				       struct btrfs_trans_handle *trans_in,
				       struct btrfs_inode *inode,
				       struct btrfs_key *ins,
				       u64 file_offset)
{
	struct btrfs_file_extent_item stack_fi;
	struct btrfs_replace_extent_info extent_info;
	struct btrfs_trans_handle *trans = trans_in;
	struct btrfs_path *path;
	u64 start = ins->objectid;
	u64 len = ins->offset;
	u64 qgroup_released = 0;
	int ret;

	memset(&stack_fi, 0, sizeof(stack_fi));

	btrfs_set_stack_file_extent_type(&stack_fi, BTRFS_FILE_EXTENT_PREALLOC);
	btrfs_set_stack_file_extent_disk_bytenr(&stack_fi, start);
	btrfs_set_stack_file_extent_disk_num_bytes(&stack_fi, len);
	btrfs_set_stack_file_extent_num_bytes(&stack_fi, len);
	btrfs_set_stack_file_extent_ram_bytes(&stack_fi, len);
	btrfs_set_stack_file_extent_compression(&stack_fi, BTRFS_COMPRESS_NONE);
	 

	ret = btrfs_qgroup_release_data(inode, file_offset, len, &qgroup_released);
	if (ret < 0)
		return ERR_PTR(ret);

	if (trans) {
		ret = insert_reserved_file_extent(trans, inode,
						  file_offset, &stack_fi,
						  true, qgroup_released);
		if (ret)
			goto free_qgroup;
		return trans;
	}

	extent_info.disk_offset = start;
	extent_info.disk_len = len;
	extent_info.data_offset = 0;
	extent_info.data_len = len;
	extent_info.file_offset = file_offset;
	extent_info.extent_buf = (char *)&stack_fi;
	extent_info.is_new_extent = true;
	extent_info.update_times = true;
	extent_info.qgroup_reserved = qgroup_released;
	extent_info.insertions = 0;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto free_qgroup;
	}

	ret = btrfs_replace_file_extents(inode, path, file_offset,
				     file_offset + len - 1, &extent_info,
				     &trans);
	btrfs_free_path(path);
	if (ret)
		goto free_qgroup;
	return trans;

free_qgroup:
	 
	btrfs_qgroup_free_refroot(inode->root->fs_info,
			inode->root->root_key.objectid, qgroup_released,
			BTRFS_QGROUP_RSV_DATA);
	return ERR_PTR(ret);
}

static int __btrfs_prealloc_file_range(struct inode *inode, int mode,
				       u64 start, u64 num_bytes, u64 min_size,
				       loff_t actual_len, u64 *alloc_hint,
				       struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct extent_map *em;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key ins;
	u64 cur_offset = start;
	u64 clear_offset = start;
	u64 i_size;
	u64 cur_bytes;
	u64 last_alloc = (u64)-1;
	int ret = 0;
	bool own_trans = true;
	u64 end = start + num_bytes - 1;

	if (trans)
		own_trans = false;
	while (num_bytes > 0) {
		cur_bytes = min_t(u64, num_bytes, SZ_256M);
		cur_bytes = max(cur_bytes, min_size);
		 
		cur_bytes = min(cur_bytes, last_alloc);
		ret = btrfs_reserve_extent(root, cur_bytes, cur_bytes,
				min_size, 0, *alloc_hint, &ins, 1, 0);
		if (ret)
			break;

		 
		clear_offset += ins.offset;

		last_alloc = ins.offset;
		trans = insert_prealloc_file_extent(trans, BTRFS_I(inode),
						    &ins, cur_offset);
		 
		btrfs_dec_block_group_reservations(fs_info, ins.objectid);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			btrfs_free_reserved_extent(fs_info, ins.objectid,
						   ins.offset, 0);
			break;
		}

		em = alloc_extent_map();
		if (!em) {
			btrfs_drop_extent_map_range(BTRFS_I(inode), cur_offset,
					    cur_offset + ins.offset - 1, false);
			btrfs_set_inode_full_sync(BTRFS_I(inode));
			goto next;
		}

		em->start = cur_offset;
		em->orig_start = cur_offset;
		em->len = ins.offset;
		em->block_start = ins.objectid;
		em->block_len = ins.offset;
		em->orig_block_len = ins.offset;
		em->ram_bytes = ins.offset;
		set_bit(EXTENT_FLAG_PREALLOC, &em->flags);
		em->generation = trans->transid;

		ret = btrfs_replace_extent_map_range(BTRFS_I(inode), em, true);
		free_extent_map(em);
next:
		num_bytes -= ins.offset;
		cur_offset += ins.offset;
		*alloc_hint = ins.objectid + ins.offset;

		inode_inc_iversion(inode);
		inode_set_ctime_current(inode);
		BTRFS_I(inode)->flags |= BTRFS_INODE_PREALLOC;
		if (!(mode & FALLOC_FL_KEEP_SIZE) &&
		    (actual_len > inode->i_size) &&
		    (cur_offset > inode->i_size)) {
			if (cur_offset > actual_len)
				i_size = actual_len;
			else
				i_size = cur_offset;
			i_size_write(inode, i_size);
			btrfs_inode_safe_disk_i_size_write(BTRFS_I(inode), 0);
		}

		ret = btrfs_update_inode(trans, root, BTRFS_I(inode));

		if (ret) {
			btrfs_abort_transaction(trans, ret);
			if (own_trans)
				btrfs_end_transaction(trans);
			break;
		}

		if (own_trans) {
			btrfs_end_transaction(trans);
			trans = NULL;
		}
	}
	if (clear_offset < end)
		btrfs_free_reserved_data_space(BTRFS_I(inode), NULL, clear_offset,
			end - clear_offset + 1);
	return ret;
}

int btrfs_prealloc_file_range(struct inode *inode, int mode,
			      u64 start, u64 num_bytes, u64 min_size,
			      loff_t actual_len, u64 *alloc_hint)
{
	return __btrfs_prealloc_file_range(inode, mode, start, num_bytes,
					   min_size, actual_len, alloc_hint,
					   NULL);
}

int btrfs_prealloc_file_range_trans(struct inode *inode,
				    struct btrfs_trans_handle *trans, int mode,
				    u64 start, u64 num_bytes, u64 min_size,
				    loff_t actual_len, u64 *alloc_hint)
{
	return __btrfs_prealloc_file_range(inode, mode, start, num_bytes,
					   min_size, actual_len, alloc_hint, trans);
}

static int btrfs_permission(struct mnt_idmap *idmap,
			    struct inode *inode, int mask)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	umode_t mode = inode->i_mode;

	if (mask & MAY_WRITE &&
	    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode))) {
		if (btrfs_root_readonly(root))
			return -EROFS;
		if (BTRFS_I(inode)->flags & BTRFS_INODE_READONLY)
			return -EACCES;
	}
	return generic_permission(idmap, inode, mask);
}

static int btrfs_tmpfile(struct mnt_idmap *idmap, struct inode *dir,
			 struct file *file, umode_t mode)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(dir->i_sb);
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode;
	struct btrfs_new_inode_args new_inode_args = {
		.dir = dir,
		.dentry = file->f_path.dentry,
		.orphan = true,
	};
	unsigned int trans_num_items;
	int ret;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;
	inode_init_owner(idmap, inode, dir, mode);
	inode->i_fop = &btrfs_file_operations;
	inode->i_op = &btrfs_file_inode_operations;
	inode->i_mapping->a_ops = &btrfs_aops;

	new_inode_args.inode = inode;
	ret = btrfs_new_inode_prepare(&new_inode_args, &trans_num_items);
	if (ret)
		goto out_inode;

	trans = btrfs_start_transaction(root, trans_num_items);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_new_inode_args;
	}

	ret = btrfs_create_new_inode(trans, &new_inode_args);

	 
	set_nlink(inode, 1);

	if (!ret) {
		d_tmpfile(file, inode);
		unlock_new_inode(inode);
		mark_inode_dirty(inode);
	}

	btrfs_end_transaction(trans);
	btrfs_btree_balance_dirty(fs_info);
out_new_inode_args:
	btrfs_new_inode_args_destroy(&new_inode_args);
out_inode:
	if (ret)
		iput(inode);
	return finish_open_simple(file, ret);
}

void btrfs_set_range_writeback(struct btrfs_inode *inode, u64 start, u64 end)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	unsigned long index = start >> PAGE_SHIFT;
	unsigned long end_index = end >> PAGE_SHIFT;
	struct page *page;
	u32 len;

	ASSERT(end + 1 - start <= U32_MAX);
	len = end + 1 - start;
	while (index <= end_index) {
		page = find_get_page(inode->vfs_inode.i_mapping, index);
		ASSERT(page);  

		btrfs_page_set_writeback(fs_info, page, start, len);
		put_page(page);
		index++;
	}
}

int btrfs_encoded_io_compression_from_extent(struct btrfs_fs_info *fs_info,
					     int compress_type)
{
	switch (compress_type) {
	case BTRFS_COMPRESS_NONE:
		return BTRFS_ENCODED_IO_COMPRESSION_NONE;
	case BTRFS_COMPRESS_ZLIB:
		return BTRFS_ENCODED_IO_COMPRESSION_ZLIB;
	case BTRFS_COMPRESS_LZO:
		 
		if (fs_info->sectorsize < SZ_4K || fs_info->sectorsize > SZ_64K)
			return -EINVAL;
		return BTRFS_ENCODED_IO_COMPRESSION_LZO_4K +
		       (fs_info->sectorsize_bits - 12);
	case BTRFS_COMPRESS_ZSTD:
		return BTRFS_ENCODED_IO_COMPRESSION_ZSTD;
	default:
		return -EUCLEAN;
	}
}

static ssize_t btrfs_encoded_read_inline(
				struct kiocb *iocb,
				struct iov_iter *iter, u64 start,
				u64 lockend,
				struct extent_state **cached_state,
				u64 extent_start, size_t count,
				struct btrfs_ioctl_encoded_io_args *encoded,
				bool *unlocked)
{
	struct btrfs_inode *inode = BTRFS_I(file_inode(iocb->ki_filp));
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_file_extent_item *item;
	u64 ram_bytes;
	unsigned long ptr;
	void *tmp;
	ssize_t ret;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}
	ret = btrfs_lookup_file_extent(NULL, root, path, btrfs_ino(inode),
				       extent_start, 0);
	if (ret) {
		if (ret > 0) {
			 
			ret = -EIO;
		}
		goto out;
	}
	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_file_extent_item);

	ram_bytes = btrfs_file_extent_ram_bytes(leaf, item);
	ptr = btrfs_file_extent_inline_start(item);

	encoded->len = min_t(u64, extent_start + ram_bytes,
			     inode->vfs_inode.i_size) - iocb->ki_pos;
	ret = btrfs_encoded_io_compression_from_extent(fs_info,
				 btrfs_file_extent_compression(leaf, item));
	if (ret < 0)
		goto out;
	encoded->compression = ret;
	if (encoded->compression) {
		size_t inline_size;

		inline_size = btrfs_file_extent_inline_item_len(leaf,
								path->slots[0]);
		if (inline_size > count) {
			ret = -ENOBUFS;
			goto out;
		}
		count = inline_size;
		encoded->unencoded_len = ram_bytes;
		encoded->unencoded_offset = iocb->ki_pos - extent_start;
	} else {
		count = min_t(u64, count, encoded->len);
		encoded->len = count;
		encoded->unencoded_len = count;
		ptr += iocb->ki_pos - extent_start;
	}

	tmp = kmalloc(count, GFP_NOFS);
	if (!tmp) {
		ret = -ENOMEM;
		goto out;
	}
	read_extent_buffer(leaf, tmp, ptr, count);
	btrfs_release_path(path);
	unlock_extent(io_tree, start, lockend, cached_state);
	btrfs_inode_unlock(inode, BTRFS_ILOCK_SHARED);
	*unlocked = true;

	ret = copy_to_iter(tmp, count, iter);
	if (ret != count)
		ret = -EFAULT;
	kfree(tmp);
out:
	btrfs_free_path(path);
	return ret;
}

struct btrfs_encoded_read_private {
	wait_queue_head_t wait;
	atomic_t pending;
	blk_status_t status;
};

static void btrfs_encoded_read_endio(struct btrfs_bio *bbio)
{
	struct btrfs_encoded_read_private *priv = bbio->private;

	if (bbio->bio.bi_status) {
		 
		WRITE_ONCE(priv->status, bbio->bio.bi_status);
	}
	if (!atomic_dec_return(&priv->pending))
		wake_up(&priv->wait);
	bio_put(&bbio->bio);
}

int btrfs_encoded_read_regular_fill_pages(struct btrfs_inode *inode,
					  u64 file_offset, u64 disk_bytenr,
					  u64 disk_io_size, struct page **pages)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct btrfs_encoded_read_private priv = {
		.pending = ATOMIC_INIT(1),
	};
	unsigned long i = 0;
	struct btrfs_bio *bbio;

	init_waitqueue_head(&priv.wait);

	bbio = btrfs_bio_alloc(BIO_MAX_VECS, REQ_OP_READ, fs_info,
			       btrfs_encoded_read_endio, &priv);
	bbio->bio.bi_iter.bi_sector = disk_bytenr >> SECTOR_SHIFT;
	bbio->inode = inode;

	do {
		size_t bytes = min_t(u64, disk_io_size, PAGE_SIZE);

		if (bio_add_page(&bbio->bio, pages[i], bytes, 0) < bytes) {
			atomic_inc(&priv.pending);
			btrfs_submit_bio(bbio, 0);

			bbio = btrfs_bio_alloc(BIO_MAX_VECS, REQ_OP_READ, fs_info,
					       btrfs_encoded_read_endio, &priv);
			bbio->bio.bi_iter.bi_sector = disk_bytenr >> SECTOR_SHIFT;
			bbio->inode = inode;
			continue;
		}

		i++;
		disk_bytenr += bytes;
		disk_io_size -= bytes;
	} while (disk_io_size);

	atomic_inc(&priv.pending);
	btrfs_submit_bio(bbio, 0);

	if (atomic_dec_return(&priv.pending))
		io_wait_event(priv.wait, !atomic_read(&priv.pending));
	 
	return blk_status_to_errno(READ_ONCE(priv.status));
}

static ssize_t btrfs_encoded_read_regular(struct kiocb *iocb,
					  struct iov_iter *iter,
					  u64 start, u64 lockend,
					  struct extent_state **cached_state,
					  u64 disk_bytenr, u64 disk_io_size,
					  size_t count, bool compressed,
					  bool *unlocked)
{
	struct btrfs_inode *inode = BTRFS_I(file_inode(iocb->ki_filp));
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct page **pages;
	unsigned long nr_pages, i;
	u64 cur;
	size_t page_offset;
	ssize_t ret;

	nr_pages = DIV_ROUND_UP(disk_io_size, PAGE_SIZE);
	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_NOFS);
	if (!pages)
		return -ENOMEM;
	ret = btrfs_alloc_page_array(nr_pages, pages);
	if (ret) {
		ret = -ENOMEM;
		goto out;
		}

	ret = btrfs_encoded_read_regular_fill_pages(inode, start, disk_bytenr,
						    disk_io_size, pages);
	if (ret)
		goto out;

	unlock_extent(io_tree, start, lockend, cached_state);
	btrfs_inode_unlock(inode, BTRFS_ILOCK_SHARED);
	*unlocked = true;

	if (compressed) {
		i = 0;
		page_offset = 0;
	} else {
		i = (iocb->ki_pos - start) >> PAGE_SHIFT;
		page_offset = (iocb->ki_pos - start) & (PAGE_SIZE - 1);
	}
	cur = 0;
	while (cur < count) {
		size_t bytes = min_t(size_t, count - cur,
				     PAGE_SIZE - page_offset);

		if (copy_page_to_iter(pages[i], page_offset, bytes,
				      iter) != bytes) {
			ret = -EFAULT;
			goto out;
		}
		i++;
		cur += bytes;
		page_offset = 0;
	}
	ret = count;
out:
	for (i = 0; i < nr_pages; i++) {
		if (pages[i])
			__free_page(pages[i]);
	}
	kfree(pages);
	return ret;
}

ssize_t btrfs_encoded_read(struct kiocb *iocb, struct iov_iter *iter,
			   struct btrfs_ioctl_encoded_io_args *encoded)
{
	struct btrfs_inode *inode = BTRFS_I(file_inode(iocb->ki_filp));
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct extent_io_tree *io_tree = &inode->io_tree;
	ssize_t ret;
	size_t count = iov_iter_count(iter);
	u64 start, lockend, disk_bytenr, disk_io_size;
	struct extent_state *cached_state = NULL;
	struct extent_map *em;
	bool unlocked = false;

	file_accessed(iocb->ki_filp);

	btrfs_inode_lock(inode, BTRFS_ILOCK_SHARED);

	if (iocb->ki_pos >= inode->vfs_inode.i_size) {
		btrfs_inode_unlock(inode, BTRFS_ILOCK_SHARED);
		return 0;
	}
	start = ALIGN_DOWN(iocb->ki_pos, fs_info->sectorsize);
	 
	lockend = start + BTRFS_MAX_UNCOMPRESSED - 1;

	for (;;) {
		struct btrfs_ordered_extent *ordered;

		ret = btrfs_wait_ordered_range(&inode->vfs_inode, start,
					       lockend - start + 1);
		if (ret)
			goto out_unlock_inode;
		lock_extent(io_tree, start, lockend, &cached_state);
		ordered = btrfs_lookup_ordered_range(inode, start,
						     lockend - start + 1);
		if (!ordered)
			break;
		btrfs_put_ordered_extent(ordered);
		unlock_extent(io_tree, start, lockend, &cached_state);
		cond_resched();
	}

	em = btrfs_get_extent(inode, NULL, 0, start, lockend - start + 1);
	if (IS_ERR(em)) {
		ret = PTR_ERR(em);
		goto out_unlock_extent;
	}

	if (em->block_start == EXTENT_MAP_INLINE) {
		u64 extent_start = em->start;

		 
		free_extent_map(em);
		em = NULL;
		ret = btrfs_encoded_read_inline(iocb, iter, start, lockend,
						&cached_state, extent_start,
						count, encoded, &unlocked);
		goto out;
	}

	 
	encoded->len = min_t(u64, extent_map_end(em),
			     inode->vfs_inode.i_size) - iocb->ki_pos;
	if (em->block_start == EXTENT_MAP_HOLE ||
	    test_bit(EXTENT_FLAG_PREALLOC, &em->flags)) {
		disk_bytenr = EXTENT_MAP_HOLE;
		count = min_t(u64, count, encoded->len);
		encoded->len = count;
		encoded->unencoded_len = count;
	} else if (test_bit(EXTENT_FLAG_COMPRESSED, &em->flags)) {
		disk_bytenr = em->block_start;
		 
		if (em->block_len > count) {
			ret = -ENOBUFS;
			goto out_em;
		}
		disk_io_size = em->block_len;
		count = em->block_len;
		encoded->unencoded_len = em->ram_bytes;
		encoded->unencoded_offset = iocb->ki_pos - em->orig_start;
		ret = btrfs_encoded_io_compression_from_extent(fs_info,
							     em->compress_type);
		if (ret < 0)
			goto out_em;
		encoded->compression = ret;
	} else {
		disk_bytenr = em->block_start + (start - em->start);
		if (encoded->len > count)
			encoded->len = count;
		 
		disk_io_size = min(lockend + 1, iocb->ki_pos + encoded->len) - start;
		count = start + disk_io_size - iocb->ki_pos;
		encoded->len = count;
		encoded->unencoded_len = count;
		disk_io_size = ALIGN(disk_io_size, fs_info->sectorsize);
	}
	free_extent_map(em);
	em = NULL;

	if (disk_bytenr == EXTENT_MAP_HOLE) {
		unlock_extent(io_tree, start, lockend, &cached_state);
		btrfs_inode_unlock(inode, BTRFS_ILOCK_SHARED);
		unlocked = true;
		ret = iov_iter_zero(count, iter);
		if (ret != count)
			ret = -EFAULT;
	} else {
		ret = btrfs_encoded_read_regular(iocb, iter, start, lockend,
						 &cached_state, disk_bytenr,
						 disk_io_size, count,
						 encoded->compression,
						 &unlocked);
	}

out:
	if (ret >= 0)
		iocb->ki_pos += encoded->len;
out_em:
	free_extent_map(em);
out_unlock_extent:
	if (!unlocked)
		unlock_extent(io_tree, start, lockend, &cached_state);
out_unlock_inode:
	if (!unlocked)
		btrfs_inode_unlock(inode, BTRFS_ILOCK_SHARED);
	return ret;
}

ssize_t btrfs_do_encoded_write(struct kiocb *iocb, struct iov_iter *from,
			       const struct btrfs_ioctl_encoded_io_args *encoded)
{
	struct btrfs_inode *inode = BTRFS_I(file_inode(iocb->ki_filp));
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *io_tree = &inode->io_tree;
	struct extent_changeset *data_reserved = NULL;
	struct extent_state *cached_state = NULL;
	struct btrfs_ordered_extent *ordered;
	int compression;
	size_t orig_count;
	u64 start, end;
	u64 num_bytes, ram_bytes, disk_num_bytes;
	unsigned long nr_pages, i;
	struct page **pages;
	struct btrfs_key ins;
	bool extent_reserved = false;
	struct extent_map *em;
	ssize_t ret;

	switch (encoded->compression) {
	case BTRFS_ENCODED_IO_COMPRESSION_ZLIB:
		compression = BTRFS_COMPRESS_ZLIB;
		break;
	case BTRFS_ENCODED_IO_COMPRESSION_ZSTD:
		compression = BTRFS_COMPRESS_ZSTD;
		break;
	case BTRFS_ENCODED_IO_COMPRESSION_LZO_4K:
	case BTRFS_ENCODED_IO_COMPRESSION_LZO_8K:
	case BTRFS_ENCODED_IO_COMPRESSION_LZO_16K:
	case BTRFS_ENCODED_IO_COMPRESSION_LZO_32K:
	case BTRFS_ENCODED_IO_COMPRESSION_LZO_64K:
		 
		if (encoded->compression -
		    BTRFS_ENCODED_IO_COMPRESSION_LZO_4K + 12 !=
		    fs_info->sectorsize_bits)
			return -EINVAL;
		compression = BTRFS_COMPRESS_LZO;
		break;
	default:
		return -EINVAL;
	}
	if (encoded->encryption != BTRFS_ENCODED_IO_ENCRYPTION_NONE)
		return -EINVAL;

	orig_count = iov_iter_count(from);

	 
	if (encoded->unencoded_len > BTRFS_MAX_UNCOMPRESSED ||
	    orig_count > BTRFS_MAX_COMPRESSED || orig_count == 0)
		return -EINVAL;

	 
	if (orig_count >= encoded->unencoded_len)
		return -EINVAL;

	 
	start = iocb->ki_pos;
	if (!IS_ALIGNED(start, fs_info->sectorsize))
		return -EINVAL;

	 
	if (start + encoded->len < inode->vfs_inode.i_size &&
	    !IS_ALIGNED(start + encoded->len, fs_info->sectorsize))
		return -EINVAL;

	 
	if (!IS_ALIGNED(encoded->unencoded_offset, fs_info->sectorsize))
		return -EINVAL;

	num_bytes = ALIGN(encoded->len, fs_info->sectorsize);
	ram_bytes = ALIGN(encoded->unencoded_len, fs_info->sectorsize);
	end = start + num_bytes - 1;

	 
	disk_num_bytes = ALIGN(orig_count, fs_info->sectorsize);
	nr_pages = DIV_ROUND_UP(disk_num_bytes, PAGE_SIZE);
	pages = kvcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL_ACCOUNT);
	if (!pages)
		return -ENOMEM;
	for (i = 0; i < nr_pages; i++) {
		size_t bytes = min_t(size_t, PAGE_SIZE, iov_iter_count(from));
		char *kaddr;

		pages[i] = alloc_page(GFP_KERNEL_ACCOUNT);
		if (!pages[i]) {
			ret = -ENOMEM;
			goto out_pages;
		}
		kaddr = kmap_local_page(pages[i]);
		if (copy_from_iter(kaddr, bytes, from) != bytes) {
			kunmap_local(kaddr);
			ret = -EFAULT;
			goto out_pages;
		}
		if (bytes < PAGE_SIZE)
			memset(kaddr + bytes, 0, PAGE_SIZE - bytes);
		kunmap_local(kaddr);
	}

	for (;;) {
		struct btrfs_ordered_extent *ordered;

		ret = btrfs_wait_ordered_range(&inode->vfs_inode, start, num_bytes);
		if (ret)
			goto out_pages;
		ret = invalidate_inode_pages2_range(inode->vfs_inode.i_mapping,
						    start >> PAGE_SHIFT,
						    end >> PAGE_SHIFT);
		if (ret)
			goto out_pages;
		lock_extent(io_tree, start, end, &cached_state);
		ordered = btrfs_lookup_ordered_range(inode, start, num_bytes);
		if (!ordered &&
		    !filemap_range_has_page(inode->vfs_inode.i_mapping, start, end))
			break;
		if (ordered)
			btrfs_put_ordered_extent(ordered);
		unlock_extent(io_tree, start, end, &cached_state);
		cond_resched();
	}

	 
	ret = btrfs_alloc_data_chunk_ondemand(inode, disk_num_bytes);
	if (ret)
		goto out_unlock;
	ret = btrfs_qgroup_reserve_data(inode, &data_reserved, start, num_bytes);
	if (ret)
		goto out_free_data_space;
	ret = btrfs_delalloc_reserve_metadata(inode, num_bytes, disk_num_bytes,
					      false);
	if (ret)
		goto out_qgroup_free_data;

	 
	if (start == 0 && encoded->unencoded_len == encoded->len &&
	    encoded->unencoded_offset == 0) {
		ret = cow_file_range_inline(inode, encoded->len, orig_count,
					    compression, pages, true);
		if (ret <= 0) {
			if (ret == 0)
				ret = orig_count;
			goto out_delalloc_release;
		}
	}

	ret = btrfs_reserve_extent(root, disk_num_bytes, disk_num_bytes,
				   disk_num_bytes, 0, 0, &ins, 1, 1);
	if (ret)
		goto out_delalloc_release;
	extent_reserved = true;

	em = create_io_em(inode, start, num_bytes,
			  start - encoded->unencoded_offset, ins.objectid,
			  ins.offset, ins.offset, ram_bytes, compression,
			  BTRFS_ORDERED_COMPRESSED);
	if (IS_ERR(em)) {
		ret = PTR_ERR(em);
		goto out_free_reserved;
	}
	free_extent_map(em);

	ordered = btrfs_alloc_ordered_extent(inode, start, num_bytes, ram_bytes,
				       ins.objectid, ins.offset,
				       encoded->unencoded_offset,
				       (1 << BTRFS_ORDERED_ENCODED) |
				       (1 << BTRFS_ORDERED_COMPRESSED),
				       compression);
	if (IS_ERR(ordered)) {
		btrfs_drop_extent_map_range(inode, start, end, false);
		ret = PTR_ERR(ordered);
		goto out_free_reserved;
	}
	btrfs_dec_block_group_reservations(fs_info, ins.objectid);

	if (start + encoded->len > inode->vfs_inode.i_size)
		i_size_write(&inode->vfs_inode, start + encoded->len);

	unlock_extent(io_tree, start, end, &cached_state);

	btrfs_delalloc_release_extents(inode, num_bytes);

	btrfs_submit_compressed_write(ordered, pages, nr_pages, 0, false);
	ret = orig_count;
	goto out;

out_free_reserved:
	btrfs_dec_block_group_reservations(fs_info, ins.objectid);
	btrfs_free_reserved_extent(fs_info, ins.objectid, ins.offset, 1);
out_delalloc_release:
	btrfs_delalloc_release_extents(inode, num_bytes);
	btrfs_delalloc_release_metadata(inode, disk_num_bytes, ret < 0);
out_qgroup_free_data:
	if (ret < 0)
		btrfs_qgroup_free_data(inode, data_reserved, start, num_bytes, NULL);
out_free_data_space:
	 
	if (!extent_reserved)
		btrfs_free_reserved_data_space_noquota(fs_info, disk_num_bytes);
out_unlock:
	unlock_extent(io_tree, start, end, &cached_state);
out_pages:
	for (i = 0; i < nr_pages; i++) {
		if (pages[i])
			__free_page(pages[i]);
	}
	kvfree(pages);
out:
	if (ret >= 0)
		iocb->ki_pos += encoded->len;
	return ret;
}

#ifdef CONFIG_SWAP
 
static int btrfs_add_swapfile_pin(struct inode *inode, void *ptr,
				  bool is_block_group)
{
	struct btrfs_fs_info *fs_info = BTRFS_I(inode)->root->fs_info;
	struct btrfs_swapfile_pin *sp, *entry;
	struct rb_node **p;
	struct rb_node *parent = NULL;

	sp = kmalloc(sizeof(*sp), GFP_NOFS);
	if (!sp)
		return -ENOMEM;
	sp->ptr = ptr;
	sp->inode = inode;
	sp->is_block_group = is_block_group;
	sp->bg_extent_count = 1;

	spin_lock(&fs_info->swapfile_pins_lock);
	p = &fs_info->swapfile_pins.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct btrfs_swapfile_pin, node);
		if (sp->ptr < entry->ptr ||
		    (sp->ptr == entry->ptr && sp->inode < entry->inode)) {
			p = &(*p)->rb_left;
		} else if (sp->ptr > entry->ptr ||
			   (sp->ptr == entry->ptr && sp->inode > entry->inode)) {
			p = &(*p)->rb_right;
		} else {
			if (is_block_group)
				entry->bg_extent_count++;
			spin_unlock(&fs_info->swapfile_pins_lock);
			kfree(sp);
			return 1;
		}
	}
	rb_link_node(&sp->node, parent, p);
	rb_insert_color(&sp->node, &fs_info->swapfile_pins);
	spin_unlock(&fs_info->swapfile_pins_lock);
	return 0;
}

 
static void btrfs_free_swapfile_pins(struct inode *inode)
{
	struct btrfs_fs_info *fs_info = BTRFS_I(inode)->root->fs_info;
	struct btrfs_swapfile_pin *sp;
	struct rb_node *node, *next;

	spin_lock(&fs_info->swapfile_pins_lock);
	node = rb_first(&fs_info->swapfile_pins);
	while (node) {
		next = rb_next(node);
		sp = rb_entry(node, struct btrfs_swapfile_pin, node);
		if (sp->inode == inode) {
			rb_erase(&sp->node, &fs_info->swapfile_pins);
			if (sp->is_block_group) {
				btrfs_dec_block_group_swap_extents(sp->ptr,
							   sp->bg_extent_count);
				btrfs_put_block_group(sp->ptr);
			}
			kfree(sp);
		}
		node = next;
	}
	spin_unlock(&fs_info->swapfile_pins_lock);
}

struct btrfs_swap_info {
	u64 start;
	u64 block_start;
	u64 block_len;
	u64 lowest_ppage;
	u64 highest_ppage;
	unsigned long nr_pages;
	int nr_extents;
};

static int btrfs_add_swap_extent(struct swap_info_struct *sis,
				 struct btrfs_swap_info *bsi)
{
	unsigned long nr_pages;
	unsigned long max_pages;
	u64 first_ppage, first_ppage_reported, next_ppage;
	int ret;

	 
	if (bsi->nr_pages >= sis->max)
		return 0;

	max_pages = sis->max - bsi->nr_pages;
	first_ppage = PAGE_ALIGN(bsi->block_start) >> PAGE_SHIFT;
	next_ppage = PAGE_ALIGN_DOWN(bsi->block_start + bsi->block_len) >> PAGE_SHIFT;

	if (first_ppage >= next_ppage)
		return 0;
	nr_pages = next_ppage - first_ppage;
	nr_pages = min(nr_pages, max_pages);

	first_ppage_reported = first_ppage;
	if (bsi->start == 0)
		first_ppage_reported++;
	if (bsi->lowest_ppage > first_ppage_reported)
		bsi->lowest_ppage = first_ppage_reported;
	if (bsi->highest_ppage < (next_ppage - 1))
		bsi->highest_ppage = next_ppage - 1;

	ret = add_swap_extent(sis, bsi->nr_pages, nr_pages, first_ppage);
	if (ret < 0)
		return ret;
	bsi->nr_extents += ret;
	bsi->nr_pages += nr_pages;
	return 0;
}

static void btrfs_swap_deactivate(struct file *file)
{
	struct inode *inode = file_inode(file);

	btrfs_free_swapfile_pins(inode);
	atomic_dec(&BTRFS_I(inode)->root->nr_swapfiles);
}

static int btrfs_swap_activate(struct swap_info_struct *sis, struct file *file,
			       sector_t *span)
{
	struct inode *inode = file_inode(file);
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct extent_state *cached_state = NULL;
	struct extent_map *em = NULL;
	struct btrfs_device *device = NULL;
	struct btrfs_swap_info bsi = {
		.lowest_ppage = (sector_t)-1ULL,
	};
	int ret = 0;
	u64 isize;
	u64 start;

	 
	ret = btrfs_wait_ordered_range(inode, 0, (u64)-1);
	if (ret)
		return ret;

	 
	if (BTRFS_I(inode)->flags & BTRFS_INODE_COMPRESS) {
		btrfs_warn(fs_info, "swapfile must not be compressed");
		return -EINVAL;
	}
	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NODATACOW)) {
		btrfs_warn(fs_info, "swapfile must not be copy-on-write");
		return -EINVAL;
	}
	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)) {
		btrfs_warn(fs_info, "swapfile must not be checksummed");
		return -EINVAL;
	}

	 
	if (!btrfs_exclop_start(fs_info, BTRFS_EXCLOP_SWAP_ACTIVATE)) {
		btrfs_warn(fs_info,
	   "cannot activate swapfile while exclusive operation is running");
		return -EBUSY;
	}

	 
	if (!btrfs_drew_try_write_lock(&root->snapshot_lock)) {
		btrfs_exclop_finish(fs_info);
		btrfs_warn(fs_info,
	   "cannot activate swapfile because snapshot creation is in progress");
		return -EINVAL;
	}
	 
	spin_lock(&root->root_item_lock);
	if (btrfs_root_dead(root)) {
		spin_unlock(&root->root_item_lock);

		btrfs_exclop_finish(fs_info);
		btrfs_warn(fs_info,
		"cannot activate swapfile because subvolume %llu is being deleted",
			root->root_key.objectid);
		return -EPERM;
	}
	atomic_inc(&root->nr_swapfiles);
	spin_unlock(&root->root_item_lock);

	isize = ALIGN_DOWN(inode->i_size, fs_info->sectorsize);

	lock_extent(io_tree, 0, isize - 1, &cached_state);
	start = 0;
	while (start < isize) {
		u64 logical_block_start, physical_block_start;
		struct btrfs_block_group *bg;
		u64 len = isize - start;

		em = btrfs_get_extent(BTRFS_I(inode), NULL, 0, start, len);
		if (IS_ERR(em)) {
			ret = PTR_ERR(em);
			goto out;
		}

		if (em->block_start == EXTENT_MAP_HOLE) {
			btrfs_warn(fs_info, "swapfile must not have holes");
			ret = -EINVAL;
			goto out;
		}
		if (em->block_start == EXTENT_MAP_INLINE) {
			 
			btrfs_warn(fs_info, "swapfile must not be inline");
			ret = -EINVAL;
			goto out;
		}
		if (test_bit(EXTENT_FLAG_COMPRESSED, &em->flags)) {
			btrfs_warn(fs_info, "swapfile must not be compressed");
			ret = -EINVAL;
			goto out;
		}

		logical_block_start = em->block_start + (start - em->start);
		len = min(len, em->len - (start - em->start));
		free_extent_map(em);
		em = NULL;

		ret = can_nocow_extent(inode, start, &len, NULL, NULL, NULL, false, true);
		if (ret < 0) {
			goto out;
		} else if (ret) {
			ret = 0;
		} else {
			btrfs_warn(fs_info,
				   "swapfile must not be copy-on-write");
			ret = -EINVAL;
			goto out;
		}

		em = btrfs_get_chunk_map(fs_info, logical_block_start, len);
		if (IS_ERR(em)) {
			ret = PTR_ERR(em);
			goto out;
		}

		if (em->map_lookup->type & BTRFS_BLOCK_GROUP_PROFILE_MASK) {
			btrfs_warn(fs_info,
				   "swapfile must have single data profile");
			ret = -EINVAL;
			goto out;
		}

		if (device == NULL) {
			device = em->map_lookup->stripes[0].dev;
			ret = btrfs_add_swapfile_pin(inode, device, false);
			if (ret == 1)
				ret = 0;
			else if (ret)
				goto out;
		} else if (device != em->map_lookup->stripes[0].dev) {
			btrfs_warn(fs_info, "swapfile must be on one device");
			ret = -EINVAL;
			goto out;
		}

		physical_block_start = (em->map_lookup->stripes[0].physical +
					(logical_block_start - em->start));
		len = min(len, em->len - (logical_block_start - em->start));
		free_extent_map(em);
		em = NULL;

		bg = btrfs_lookup_block_group(fs_info, logical_block_start);
		if (!bg) {
			btrfs_warn(fs_info,
			   "could not find block group containing swapfile");
			ret = -EINVAL;
			goto out;
		}

		if (!btrfs_inc_block_group_swap_extents(bg)) {
			btrfs_warn(fs_info,
			   "block group for swapfile at %llu is read-only%s",
			   bg->start,
			   atomic_read(&fs_info->scrubs_running) ?
				       " (scrub running)" : "");
			btrfs_put_block_group(bg);
			ret = -EINVAL;
			goto out;
		}

		ret = btrfs_add_swapfile_pin(inode, bg, true);
		if (ret) {
			btrfs_put_block_group(bg);
			if (ret == 1)
				ret = 0;
			else
				goto out;
		}

		if (bsi.block_len &&
		    bsi.block_start + bsi.block_len == physical_block_start) {
			bsi.block_len += len;
		} else {
			if (bsi.block_len) {
				ret = btrfs_add_swap_extent(sis, &bsi);
				if (ret)
					goto out;
			}
			bsi.start = start;
			bsi.block_start = physical_block_start;
			bsi.block_len = len;
		}

		start += len;
	}

	if (bsi.block_len)
		ret = btrfs_add_swap_extent(sis, &bsi);

out:
	if (!IS_ERR_OR_NULL(em))
		free_extent_map(em);

	unlock_extent(io_tree, 0, isize - 1, &cached_state);

	if (ret)
		btrfs_swap_deactivate(file);

	btrfs_drew_write_unlock(&root->snapshot_lock);

	btrfs_exclop_finish(fs_info);

	if (ret)
		return ret;

	if (device)
		sis->bdev = device->bdev;
	*span = bsi.highest_ppage - bsi.lowest_ppage + 1;
	sis->max = bsi.nr_pages;
	sis->pages = bsi.nr_pages - 1;
	sis->highest_bit = bsi.nr_pages - 1;
	return bsi.nr_extents;
}
#else
static void btrfs_swap_deactivate(struct file *file)
{
}

static int btrfs_swap_activate(struct swap_info_struct *sis, struct file *file,
			       sector_t *span)
{
	return -EOPNOTSUPP;
}
#endif

 
void btrfs_update_inode_bytes(struct btrfs_inode *inode,
			      const u64 add_bytes,
			      const u64 del_bytes)
{
	if (add_bytes == del_bytes)
		return;

	spin_lock(&inode->lock);
	if (del_bytes > 0)
		inode_sub_bytes(&inode->vfs_inode, del_bytes);
	if (add_bytes > 0)
		inode_add_bytes(&inode->vfs_inode, add_bytes);
	spin_unlock(&inode->lock);
}

 
void btrfs_assert_inode_range_clean(struct btrfs_inode *inode, u64 start, u64 end)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_ordered_extent *ordered;

	if (!IS_ENABLED(CONFIG_BTRFS_ASSERT))
		return;

	ordered = btrfs_lookup_first_ordered_range(inode, start, end + 1 - start);
	if (ordered) {
		btrfs_err(root->fs_info,
"found unexpected ordered extent in file range [%llu, %llu] for inode %llu root %llu (ordered range [%llu, %llu])",
			  start, end, btrfs_ino(inode), root->root_key.objectid,
			  ordered->file_offset,
			  ordered->file_offset + ordered->num_bytes - 1);
		btrfs_put_ordered_extent(ordered);
	}

	ASSERT(ordered == NULL);
}

static const struct inode_operations btrfs_dir_inode_operations = {
	.getattr	= btrfs_getattr,
	.lookup		= btrfs_lookup,
	.create		= btrfs_create,
	.unlink		= btrfs_unlink,
	.link		= btrfs_link,
	.mkdir		= btrfs_mkdir,
	.rmdir		= btrfs_rmdir,
	.rename		= btrfs_rename2,
	.symlink	= btrfs_symlink,
	.setattr	= btrfs_setattr,
	.mknod		= btrfs_mknod,
	.listxattr	= btrfs_listxattr,
	.permission	= btrfs_permission,
	.get_inode_acl	= btrfs_get_acl,
	.set_acl	= btrfs_set_acl,
	.update_time	= btrfs_update_time,
	.tmpfile        = btrfs_tmpfile,
	.fileattr_get	= btrfs_fileattr_get,
	.fileattr_set	= btrfs_fileattr_set,
};

static const struct file_operations btrfs_dir_file_operations = {
	.llseek		= btrfs_dir_llseek,
	.read		= generic_read_dir,
	.iterate_shared	= btrfs_real_readdir,
	.open		= btrfs_opendir,
	.unlocked_ioctl	= btrfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= btrfs_compat_ioctl,
#endif
	.release        = btrfs_release_file,
	.fsync		= btrfs_sync_file,
};

 
static const struct address_space_operations btrfs_aops = {
	.read_folio	= btrfs_read_folio,
	.writepages	= btrfs_writepages,
	.readahead	= btrfs_readahead,
	.invalidate_folio = btrfs_invalidate_folio,
	.release_folio	= btrfs_release_folio,
	.migrate_folio	= btrfs_migrate_folio,
	.dirty_folio	= filemap_dirty_folio,
	.error_remove_page = generic_error_remove_page,
	.swap_activate	= btrfs_swap_activate,
	.swap_deactivate = btrfs_swap_deactivate,
};

static const struct inode_operations btrfs_file_inode_operations = {
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
	.listxattr      = btrfs_listxattr,
	.permission	= btrfs_permission,
	.fiemap		= btrfs_fiemap,
	.get_inode_acl	= btrfs_get_acl,
	.set_acl	= btrfs_set_acl,
	.update_time	= btrfs_update_time,
	.fileattr_get	= btrfs_fileattr_get,
	.fileattr_set	= btrfs_fileattr_set,
};
static const struct inode_operations btrfs_special_inode_operations = {
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
	.permission	= btrfs_permission,
	.listxattr	= btrfs_listxattr,
	.get_inode_acl	= btrfs_get_acl,
	.set_acl	= btrfs_set_acl,
	.update_time	= btrfs_update_time,
};
static const struct inode_operations btrfs_symlink_inode_operations = {
	.get_link	= page_get_link,
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
	.permission	= btrfs_permission,
	.listxattr	= btrfs_listxattr,
	.update_time	= btrfs_update_time,
};

const struct dentry_operations btrfs_dentry_operations = {
	.d_delete	= btrfs_dentry_delete,
};
