
 

#include <linux/bsearch.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sort.h>
#include <linux/mount.h>
#include <linux/xattr.h>
#include <linux/posix_acl_xattr.h>
#include <linux/radix-tree.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/compat.h>
#include <linux/crc32c.h>
#include <linux/fsverity.h>

#include "send.h"
#include "ctree.h"
#include "backref.h"
#include "locking.h"
#include "disk-io.h"
#include "btrfs_inode.h"
#include "transaction.h"
#include "compression.h"
#include "xattr.h"
#include "print-tree.h"
#include "accessors.h"
#include "dir-item.h"
#include "file-item.h"
#include "ioctl.h"
#include "verity.h"
#include "lru_cache.h"

 
#define SEND_MAX_EXTENT_REFS	1024

 
struct fs_path {
	union {
		struct {
			char *start;
			char *end;

			char *buf;
			unsigned short buf_len:15;
			unsigned short reversed:1;
			char inline_buf[];
		};
		 
		char pad[256];
	};
};
#define FS_PATH_INLINE_SIZE \
	(sizeof(struct fs_path) - offsetof(struct fs_path, inline_buf))


 
struct clone_root {
	struct btrfs_root *root;
	u64 ino;
	u64 offset;
	u64 num_bytes;
	bool found_ref;
};

#define SEND_MAX_NAME_CACHE_SIZE			256

 
#define SEND_MAX_BACKREF_CACHE_ROOTS			17

 
#define SEND_MAX_BACKREF_CACHE_SIZE 128

 
struct backref_cache_entry {
	struct btrfs_lru_cache_entry entry;
	u64 root_ids[SEND_MAX_BACKREF_CACHE_ROOTS];
	 
	int num_roots;
};

 
static_assert(offsetof(struct backref_cache_entry, entry) == 0);

 
#define SEND_MAX_DIR_CREATED_CACHE_SIZE			64

 
#define SEND_MAX_DIR_UTIMES_CACHE_SIZE			64

struct send_ctx {
	struct file *send_filp;
	loff_t send_off;
	char *send_buf;
	u32 send_size;
	u32 send_max_size;
	 
	bool put_data;
	struct page **send_buf_pages;
	u64 flags;	 
	 
	u32 proto;

	struct btrfs_root *send_root;
	struct btrfs_root *parent_root;
	struct clone_root *clone_roots;
	int clone_roots_cnt;

	 
	struct btrfs_path *left_path;
	struct btrfs_path *right_path;
	struct btrfs_key *cmp_key;

	 
	u64 last_reloc_trans;

	 
	u64 cur_ino;
	u64 cur_inode_gen;
	u64 cur_inode_size;
	u64 cur_inode_mode;
	u64 cur_inode_rdev;
	u64 cur_inode_last_extent;
	u64 cur_inode_next_write_offset;
	bool cur_inode_new;
	bool cur_inode_new_gen;
	bool cur_inode_deleted;
	bool ignore_cur_inode;
	bool cur_inode_needs_verity;
	void *verity_descriptor;

	u64 send_progress;

	struct list_head new_refs;
	struct list_head deleted_refs;

	struct btrfs_lru_cache name_cache;

	 
	struct inode *cur_inode;
	struct file_ra_state ra;
	u64 page_cache_clear_start;
	bool clean_page_cache;

	 

	 
	struct rb_root pending_dir_moves;

	 
	struct rb_root waiting_dir_moves;

	 
	struct rb_root orphan_dirs;

	struct rb_root rbtree_new_refs;
	struct rb_root rbtree_deleted_refs;

	struct btrfs_lru_cache backref_cache;
	u64 backref_cache_last_reloc_trans;

	struct btrfs_lru_cache dir_created_cache;
	struct btrfs_lru_cache dir_utimes_cache;
};

struct pending_dir_move {
	struct rb_node node;
	struct list_head list;
	u64 parent_ino;
	u64 ino;
	u64 gen;
	struct list_head update_refs;
};

struct waiting_dir_move {
	struct rb_node node;
	u64 ino;
	 
	u64 rmdir_ino;
	u64 rmdir_gen;
	bool orphanized;
};

struct orphan_dir_info {
	struct rb_node node;
	u64 ino;
	u64 gen;
	u64 last_dir_index_offset;
	u64 dir_high_seq_ino;
};

struct name_cache_entry {
	 
	struct btrfs_lru_cache_entry entry;
	u64 parent_ino;
	u64 parent_gen;
	int ret;
	int need_later_update;
	int name_len;
	char name[];
};

 
static_assert(offsetof(struct name_cache_entry, entry) == 0);

#define ADVANCE							1
#define ADVANCE_ONLY_NEXT					-1

enum btrfs_compare_tree_result {
	BTRFS_COMPARE_TREE_NEW,
	BTRFS_COMPARE_TREE_DELETED,
	BTRFS_COMPARE_TREE_CHANGED,
	BTRFS_COMPARE_TREE_SAME,
};

__cold
static void inconsistent_snapshot_error(struct send_ctx *sctx,
					enum btrfs_compare_tree_result result,
					const char *what)
{
	const char *result_string;

	switch (result) {
	case BTRFS_COMPARE_TREE_NEW:
		result_string = "new";
		break;
	case BTRFS_COMPARE_TREE_DELETED:
		result_string = "deleted";
		break;
	case BTRFS_COMPARE_TREE_CHANGED:
		result_string = "updated";
		break;
	case BTRFS_COMPARE_TREE_SAME:
		ASSERT(0);
		result_string = "unchanged";
		break;
	default:
		ASSERT(0);
		result_string = "unexpected";
	}

	btrfs_err(sctx->send_root->fs_info,
		  "Send: inconsistent snapshot, found %s %s for inode %llu without updated inode item, send root is %llu, parent root is %llu",
		  result_string, what, sctx->cmp_key->objectid,
		  sctx->send_root->root_key.objectid,
		  (sctx->parent_root ?
		   sctx->parent_root->root_key.objectid : 0));
}

__maybe_unused
static bool proto_cmd_ok(const struct send_ctx *sctx, int cmd)
{
	switch (sctx->proto) {
	case 1:	 return cmd <= BTRFS_SEND_C_MAX_V1;
	case 2:	 return cmd <= BTRFS_SEND_C_MAX_V2;
	case 3:	 return cmd <= BTRFS_SEND_C_MAX_V3;
	default: return false;
	}
}

static int is_waiting_for_move(struct send_ctx *sctx, u64 ino);

static struct waiting_dir_move *
get_waiting_dir_move(struct send_ctx *sctx, u64 ino);

static int is_waiting_for_rm(struct send_ctx *sctx, u64 dir_ino, u64 gen);

static int need_send_hole(struct send_ctx *sctx)
{
	return (sctx->parent_root && !sctx->cur_inode_new &&
		!sctx->cur_inode_new_gen && !sctx->cur_inode_deleted &&
		S_ISREG(sctx->cur_inode_mode));
}

static void fs_path_reset(struct fs_path *p)
{
	if (p->reversed) {
		p->start = p->buf + p->buf_len - 1;
		p->end = p->start;
		*p->start = 0;
	} else {
		p->start = p->buf;
		p->end = p->start;
		*p->start = 0;
	}
}

static struct fs_path *fs_path_alloc(void)
{
	struct fs_path *p;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;
	p->reversed = 0;
	p->buf = p->inline_buf;
	p->buf_len = FS_PATH_INLINE_SIZE;
	fs_path_reset(p);
	return p;
}

static struct fs_path *fs_path_alloc_reversed(void)
{
	struct fs_path *p;

	p = fs_path_alloc();
	if (!p)
		return NULL;
	p->reversed = 1;
	fs_path_reset(p);
	return p;
}

static void fs_path_free(struct fs_path *p)
{
	if (!p)
		return;
	if (p->buf != p->inline_buf)
		kfree(p->buf);
	kfree(p);
}

static int fs_path_len(struct fs_path *p)
{
	return p->end - p->start;
}

static int fs_path_ensure_buf(struct fs_path *p, int len)
{
	char *tmp_buf;
	int path_len;
	int old_buf_len;

	len++;

	if (p->buf_len >= len)
		return 0;

	if (len > PATH_MAX) {
		WARN_ON(1);
		return -ENOMEM;
	}

	path_len = p->end - p->start;
	old_buf_len = p->buf_len;

	 
	len = kmalloc_size_roundup(len);
	 
	if (p->buf == p->inline_buf) {
		tmp_buf = kmalloc(len, GFP_KERNEL);
		if (tmp_buf)
			memcpy(tmp_buf, p->buf, old_buf_len);
	} else {
		tmp_buf = krealloc(p->buf, len, GFP_KERNEL);
	}
	if (!tmp_buf)
		return -ENOMEM;
	p->buf = tmp_buf;
	p->buf_len = len;

	if (p->reversed) {
		tmp_buf = p->buf + old_buf_len - path_len - 1;
		p->end = p->buf + p->buf_len - 1;
		p->start = p->end - path_len;
		memmove(p->start, tmp_buf, path_len + 1);
	} else {
		p->start = p->buf;
		p->end = p->start + path_len;
	}
	return 0;
}

static int fs_path_prepare_for_add(struct fs_path *p, int name_len,
				   char **prepared)
{
	int ret;
	int new_len;

	new_len = p->end - p->start + name_len;
	if (p->start != p->end)
		new_len++;
	ret = fs_path_ensure_buf(p, new_len);
	if (ret < 0)
		goto out;

	if (p->reversed) {
		if (p->start != p->end)
			*--p->start = '/';
		p->start -= name_len;
		*prepared = p->start;
	} else {
		if (p->start != p->end)
			*p->end++ = '/';
		*prepared = p->end;
		p->end += name_len;
		*p->end = 0;
	}

out:
	return ret;
}

static int fs_path_add(struct fs_path *p, const char *name, int name_len)
{
	int ret;
	char *prepared;

	ret = fs_path_prepare_for_add(p, name_len, &prepared);
	if (ret < 0)
		goto out;
	memcpy(prepared, name, name_len);

out:
	return ret;
}

static int fs_path_add_path(struct fs_path *p, struct fs_path *p2)
{
	int ret;
	char *prepared;

	ret = fs_path_prepare_for_add(p, p2->end - p2->start, &prepared);
	if (ret < 0)
		goto out;
	memcpy(prepared, p2->start, p2->end - p2->start);

out:
	return ret;
}

static int fs_path_add_from_extent_buffer(struct fs_path *p,
					  struct extent_buffer *eb,
					  unsigned long off, int len)
{
	int ret;
	char *prepared;

	ret = fs_path_prepare_for_add(p, len, &prepared);
	if (ret < 0)
		goto out;

	read_extent_buffer(eb, prepared, off, len);

out:
	return ret;
}

static int fs_path_copy(struct fs_path *p, struct fs_path *from)
{
	p->reversed = from->reversed;
	fs_path_reset(p);

	return fs_path_add_path(p, from);
}

static void fs_path_unreverse(struct fs_path *p)
{
	char *tmp;
	int len;

	if (!p->reversed)
		return;

	tmp = p->start;
	len = p->end - p->start;
	p->start = p->buf;
	p->end = p->start + len;
	memmove(p->start, tmp, len + 1);
	p->reversed = 0;
}

static struct btrfs_path *alloc_path_for_send(void)
{
	struct btrfs_path *path;

	path = btrfs_alloc_path();
	if (!path)
		return NULL;
	path->search_commit_root = 1;
	path->skip_locking = 1;
	path->need_commit_sem = 1;
	return path;
}

static int write_buf(struct file *filp, const void *buf, u32 len, loff_t *off)
{
	int ret;
	u32 pos = 0;

	while (pos < len) {
		ret = kernel_write(filp, buf + pos, len - pos, off);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return -EIO;
		pos += ret;
	}

	return 0;
}

static int tlv_put(struct send_ctx *sctx, u16 attr, const void *data, int len)
{
	struct btrfs_tlv_header *hdr;
	int total_len = sizeof(*hdr) + len;
	int left = sctx->send_max_size - sctx->send_size;

	if (WARN_ON_ONCE(sctx->put_data))
		return -EINVAL;

	if (unlikely(left < total_len))
		return -EOVERFLOW;

	hdr = (struct btrfs_tlv_header *) (sctx->send_buf + sctx->send_size);
	put_unaligned_le16(attr, &hdr->tlv_type);
	put_unaligned_le16(len, &hdr->tlv_len);
	memcpy(hdr + 1, data, len);
	sctx->send_size += total_len;

	return 0;
}

#define TLV_PUT_DEFINE_INT(bits) \
	static int tlv_put_u##bits(struct send_ctx *sctx,	 	\
			u##bits attr, u##bits value)			\
	{								\
		__le##bits __tmp = cpu_to_le##bits(value);		\
		return tlv_put(sctx, attr, &__tmp, sizeof(__tmp));	\
	}

TLV_PUT_DEFINE_INT(8)
TLV_PUT_DEFINE_INT(32)
TLV_PUT_DEFINE_INT(64)

static int tlv_put_string(struct send_ctx *sctx, u16 attr,
			  const char *str, int len)
{
	if (len == -1)
		len = strlen(str);
	return tlv_put(sctx, attr, str, len);
}

static int tlv_put_uuid(struct send_ctx *sctx, u16 attr,
			const u8 *uuid)
{
	return tlv_put(sctx, attr, uuid, BTRFS_UUID_SIZE);
}

static int tlv_put_btrfs_timespec(struct send_ctx *sctx, u16 attr,
				  struct extent_buffer *eb,
				  struct btrfs_timespec *ts)
{
	struct btrfs_timespec bts;
	read_extent_buffer(eb, &bts, (unsigned long)ts, sizeof(bts));
	return tlv_put(sctx, attr, &bts, sizeof(bts));
}


#define TLV_PUT(sctx, attrtype, data, attrlen) \
	do { \
		ret = tlv_put(sctx, attrtype, data, attrlen); \
		if (ret < 0) \
			goto tlv_put_failure; \
	} while (0)

#define TLV_PUT_INT(sctx, attrtype, bits, value) \
	do { \
		ret = tlv_put_u##bits(sctx, attrtype, value); \
		if (ret < 0) \
			goto tlv_put_failure; \
	} while (0)

#define TLV_PUT_U8(sctx, attrtype, data) TLV_PUT_INT(sctx, attrtype, 8, data)
#define TLV_PUT_U16(sctx, attrtype, data) TLV_PUT_INT(sctx, attrtype, 16, data)
#define TLV_PUT_U32(sctx, attrtype, data) TLV_PUT_INT(sctx, attrtype, 32, data)
#define TLV_PUT_U64(sctx, attrtype, data) TLV_PUT_INT(sctx, attrtype, 64, data)
#define TLV_PUT_STRING(sctx, attrtype, str, len) \
	do { \
		ret = tlv_put_string(sctx, attrtype, str, len); \
		if (ret < 0) \
			goto tlv_put_failure; \
	} while (0)
#define TLV_PUT_PATH(sctx, attrtype, p) \
	do { \
		ret = tlv_put_string(sctx, attrtype, p->start, \
			p->end - p->start); \
		if (ret < 0) \
			goto tlv_put_failure; \
	} while(0)
#define TLV_PUT_UUID(sctx, attrtype, uuid) \
	do { \
		ret = tlv_put_uuid(sctx, attrtype, uuid); \
		if (ret < 0) \
			goto tlv_put_failure; \
	} while (0)
#define TLV_PUT_BTRFS_TIMESPEC(sctx, attrtype, eb, ts) \
	do { \
		ret = tlv_put_btrfs_timespec(sctx, attrtype, eb, ts); \
		if (ret < 0) \
			goto tlv_put_failure; \
	} while (0)

static int send_header(struct send_ctx *sctx)
{
	struct btrfs_stream_header hdr;

	strcpy(hdr.magic, BTRFS_SEND_STREAM_MAGIC);
	hdr.version = cpu_to_le32(sctx->proto);
	return write_buf(sctx->send_filp, &hdr, sizeof(hdr),
					&sctx->send_off);
}

 
static int begin_cmd(struct send_ctx *sctx, int cmd)
{
	struct btrfs_cmd_header *hdr;

	if (WARN_ON(!sctx->send_buf))
		return -EINVAL;

	BUG_ON(sctx->send_size);

	sctx->send_size += sizeof(*hdr);
	hdr = (struct btrfs_cmd_header *)sctx->send_buf;
	put_unaligned_le16(cmd, &hdr->cmd);

	return 0;
}

static int send_cmd(struct send_ctx *sctx)
{
	int ret;
	struct btrfs_cmd_header *hdr;
	u32 crc;

	hdr = (struct btrfs_cmd_header *)sctx->send_buf;
	put_unaligned_le32(sctx->send_size - sizeof(*hdr), &hdr->len);
	put_unaligned_le32(0, &hdr->crc);

	crc = btrfs_crc32c(0, (unsigned char *)sctx->send_buf, sctx->send_size);
	put_unaligned_le32(crc, &hdr->crc);

	ret = write_buf(sctx->send_filp, sctx->send_buf, sctx->send_size,
					&sctx->send_off);

	sctx->send_size = 0;
	sctx->put_data = false;

	return ret;
}

 
static int send_rename(struct send_ctx *sctx,
		     struct fs_path *from, struct fs_path *to)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret;

	btrfs_debug(fs_info, "send_rename %s -> %s", from->start, to->start);

	ret = begin_cmd(sctx, BTRFS_SEND_C_RENAME);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, from);
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH_TO, to);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

 
static int send_link(struct send_ctx *sctx,
		     struct fs_path *path, struct fs_path *lnk)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret;

	btrfs_debug(fs_info, "send_link %s -> %s", path->start, lnk->start);

	ret = begin_cmd(sctx, BTRFS_SEND_C_LINK);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, path);
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH_LINK, lnk);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

 
static int send_unlink(struct send_ctx *sctx, struct fs_path *path)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret;

	btrfs_debug(fs_info, "send_unlink %s", path->start);

	ret = begin_cmd(sctx, BTRFS_SEND_C_UNLINK);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, path);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

 
static int send_rmdir(struct send_ctx *sctx, struct fs_path *path)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret;

	btrfs_debug(fs_info, "send_rmdir %s", path->start);

	ret = begin_cmd(sctx, BTRFS_SEND_C_RMDIR);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, path);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

struct btrfs_inode_info {
	u64 size;
	u64 gen;
	u64 mode;
	u64 uid;
	u64 gid;
	u64 rdev;
	u64 fileattr;
	u64 nlink;
};

 
static int get_inode_info(struct btrfs_root *root, u64 ino,
			  struct btrfs_inode_info *info)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_inode_item *ii;
	struct btrfs_key key;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = ino;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto out;
	}

	if (!info)
		goto out;

	ii = btrfs_item_ptr(path->nodes[0], path->slots[0],
			struct btrfs_inode_item);
	info->size = btrfs_inode_size(path->nodes[0], ii);
	info->gen = btrfs_inode_generation(path->nodes[0], ii);
	info->mode = btrfs_inode_mode(path->nodes[0], ii);
	info->uid = btrfs_inode_uid(path->nodes[0], ii);
	info->gid = btrfs_inode_gid(path->nodes[0], ii);
	info->rdev = btrfs_inode_rdev(path->nodes[0], ii);
	info->nlink = btrfs_inode_nlink(path->nodes[0], ii);
	 
	info->fileattr = btrfs_inode_flags(path->nodes[0], ii);

out:
	btrfs_free_path(path);
	return ret;
}

static int get_inode_gen(struct btrfs_root *root, u64 ino, u64 *gen)
{
	int ret;
	struct btrfs_inode_info info = { 0 };

	ASSERT(gen);

	ret = get_inode_info(root, ino, &info);
	*gen = info.gen;
	return ret;
}

typedef int (*iterate_inode_ref_t)(int num, u64 dir, int index,
				   struct fs_path *p,
				   void *ctx);

 
static int iterate_inode_ref(struct btrfs_root *root, struct btrfs_path *path,
			     struct btrfs_key *found_key, int resolve,
			     iterate_inode_ref_t iterate, void *ctx)
{
	struct extent_buffer *eb = path->nodes[0];
	struct btrfs_inode_ref *iref;
	struct btrfs_inode_extref *extref;
	struct btrfs_path *tmp_path;
	struct fs_path *p;
	u32 cur = 0;
	u32 total;
	int slot = path->slots[0];
	u32 name_len;
	char *start;
	int ret = 0;
	int num = 0;
	int index;
	u64 dir;
	unsigned long name_off;
	unsigned long elem_size;
	unsigned long ptr;

	p = fs_path_alloc_reversed();
	if (!p)
		return -ENOMEM;

	tmp_path = alloc_path_for_send();
	if (!tmp_path) {
		fs_path_free(p);
		return -ENOMEM;
	}


	if (found_key->type == BTRFS_INODE_REF_KEY) {
		ptr = (unsigned long)btrfs_item_ptr(eb, slot,
						    struct btrfs_inode_ref);
		total = btrfs_item_size(eb, slot);
		elem_size = sizeof(*iref);
	} else {
		ptr = btrfs_item_ptr_offset(eb, slot);
		total = btrfs_item_size(eb, slot);
		elem_size = sizeof(*extref);
	}

	while (cur < total) {
		fs_path_reset(p);

		if (found_key->type == BTRFS_INODE_REF_KEY) {
			iref = (struct btrfs_inode_ref *)(ptr + cur);
			name_len = btrfs_inode_ref_name_len(eb, iref);
			name_off = (unsigned long)(iref + 1);
			index = btrfs_inode_ref_index(eb, iref);
			dir = found_key->offset;
		} else {
			extref = (struct btrfs_inode_extref *)(ptr + cur);
			name_len = btrfs_inode_extref_name_len(eb, extref);
			name_off = (unsigned long)&extref->name;
			index = btrfs_inode_extref_index(eb, extref);
			dir = btrfs_inode_extref_parent(eb, extref);
		}

		if (resolve) {
			start = btrfs_ref_to_path(root, tmp_path, name_len,
						  name_off, eb, dir,
						  p->buf, p->buf_len);
			if (IS_ERR(start)) {
				ret = PTR_ERR(start);
				goto out;
			}
			if (start < p->buf) {
				 
				ret = fs_path_ensure_buf(p,
						p->buf_len + p->buf - start);
				if (ret < 0)
					goto out;
				start = btrfs_ref_to_path(root, tmp_path,
							  name_len, name_off,
							  eb, dir,
							  p->buf, p->buf_len);
				if (IS_ERR(start)) {
					ret = PTR_ERR(start);
					goto out;
				}
				BUG_ON(start < p->buf);
			}
			p->start = start;
		} else {
			ret = fs_path_add_from_extent_buffer(p, eb, name_off,
							     name_len);
			if (ret < 0)
				goto out;
		}

		cur += elem_size + name_len;
		ret = iterate(num, dir, index, p, ctx);
		if (ret)
			goto out;
		num++;
	}

out:
	btrfs_free_path(tmp_path);
	fs_path_free(p);
	return ret;
}

typedef int (*iterate_dir_item_t)(int num, struct btrfs_key *di_key,
				  const char *name, int name_len,
				  const char *data, int data_len,
				  void *ctx);

 
static int iterate_dir_item(struct btrfs_root *root, struct btrfs_path *path,
			    iterate_dir_item_t iterate, void *ctx)
{
	int ret = 0;
	struct extent_buffer *eb;
	struct btrfs_dir_item *di;
	struct btrfs_key di_key;
	char *buf = NULL;
	int buf_len;
	u32 name_len;
	u32 data_len;
	u32 cur;
	u32 len;
	u32 total;
	int slot;
	int num;

	 
	buf_len = PATH_MAX;
	buf = kmalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	eb = path->nodes[0];
	slot = path->slots[0];
	di = btrfs_item_ptr(eb, slot, struct btrfs_dir_item);
	cur = 0;
	len = 0;
	total = btrfs_item_size(eb, slot);

	num = 0;
	while (cur < total) {
		name_len = btrfs_dir_name_len(eb, di);
		data_len = btrfs_dir_data_len(eb, di);
		btrfs_dir_item_key_to_cpu(eb, di, &di_key);

		if (btrfs_dir_ftype(eb, di) == BTRFS_FT_XATTR) {
			if (name_len > XATTR_NAME_MAX) {
				ret = -ENAMETOOLONG;
				goto out;
			}
			if (name_len + data_len >
					BTRFS_MAX_XATTR_SIZE(root->fs_info)) {
				ret = -E2BIG;
				goto out;
			}
		} else {
			 
			if (name_len + data_len > PATH_MAX) {
				ret = -ENAMETOOLONG;
				goto out;
			}
		}

		if (name_len + data_len > buf_len) {
			buf_len = name_len + data_len;
			if (is_vmalloc_addr(buf)) {
				vfree(buf);
				buf = NULL;
			} else {
				char *tmp = krealloc(buf, buf_len,
						GFP_KERNEL | __GFP_NOWARN);

				if (!tmp)
					kfree(buf);
				buf = tmp;
			}
			if (!buf) {
				buf = kvmalloc(buf_len, GFP_KERNEL);
				if (!buf) {
					ret = -ENOMEM;
					goto out;
				}
			}
		}

		read_extent_buffer(eb, buf, (unsigned long)(di + 1),
				name_len + data_len);

		len = sizeof(*di) + name_len + data_len;
		di = (struct btrfs_dir_item *)((char *)di + len);
		cur += len;

		ret = iterate(num, &di_key, buf, name_len, buf + name_len,
			      data_len, ctx);
		if (ret < 0)
			goto out;
		if (ret) {
			ret = 0;
			goto out;
		}

		num++;
	}

out:
	kvfree(buf);
	return ret;
}

static int __copy_first_ref(int num, u64 dir, int index,
			    struct fs_path *p, void *ctx)
{
	int ret;
	struct fs_path *pt = ctx;

	ret = fs_path_copy(pt, p);
	if (ret < 0)
		return ret;

	 
	return 1;
}

 
static int get_inode_path(struct btrfs_root *root,
			  u64 ino, struct fs_path *path)
{
	int ret;
	struct btrfs_key key, found_key;
	struct btrfs_path *p;

	p = alloc_path_for_send();
	if (!p)
		return -ENOMEM;

	fs_path_reset(path);

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = 0;

	ret = btrfs_search_slot_for_read(root, &key, p, 1, 0);
	if (ret < 0)
		goto out;
	if (ret) {
		ret = 1;
		goto out;
	}
	btrfs_item_key_to_cpu(p->nodes[0], &found_key, p->slots[0]);
	if (found_key.objectid != ino ||
	    (found_key.type != BTRFS_INODE_REF_KEY &&
	     found_key.type != BTRFS_INODE_EXTREF_KEY)) {
		ret = -ENOENT;
		goto out;
	}

	ret = iterate_inode_ref(root, p, &found_key, 1,
				__copy_first_ref, path);
	if (ret < 0)
		goto out;
	ret = 0;

out:
	btrfs_free_path(p);
	return ret;
}

struct backref_ctx {
	struct send_ctx *sctx;

	 
	u64 found;

	 
	u64 cur_objectid;
	u64 cur_offset;

	 
	u64 extent_len;

	 
	u64 bytenr;
	 
	u64 backref_owner;
	 
	u64 backref_offset;
};

static int __clone_root_cmp_bsearch(const void *key, const void *elt)
{
	u64 root = (u64)(uintptr_t)key;
	const struct clone_root *cr = elt;

	if (root < cr->root->root_key.objectid)
		return -1;
	if (root > cr->root->root_key.objectid)
		return 1;
	return 0;
}

static int __clone_root_cmp_sort(const void *e1, const void *e2)
{
	const struct clone_root *cr1 = e1;
	const struct clone_root *cr2 = e2;

	if (cr1->root->root_key.objectid < cr2->root->root_key.objectid)
		return -1;
	if (cr1->root->root_key.objectid > cr2->root->root_key.objectid)
		return 1;
	return 0;
}

 
static int iterate_backrefs(u64 ino, u64 offset, u64 num_bytes, u64 root_id,
			    void *ctx_)
{
	struct backref_ctx *bctx = ctx_;
	struct clone_root *clone_root;

	 
	clone_root = bsearch((void *)(uintptr_t)root_id, bctx->sctx->clone_roots,
			     bctx->sctx->clone_roots_cnt,
			     sizeof(struct clone_root),
			     __clone_root_cmp_bsearch);
	if (!clone_root)
		return 0;

	 
	if (clone_root->root == bctx->sctx->send_root &&
	    ino == bctx->cur_objectid &&
	    offset == bctx->cur_offset)
		return 0;

	 
	if (clone_root->root == bctx->sctx->send_root) {
		 
		if (ino > bctx->cur_objectid)
			return 0;
		 
		if (ino == bctx->cur_objectid &&
		    offset + bctx->extent_len >
		    bctx->sctx->cur_inode_next_write_offset)
			return 0;
	}

	bctx->found++;
	clone_root->found_ref = true;

	 
	if (num_bytes > clone_root->num_bytes) {
		clone_root->ino = ino;
		clone_root->offset = offset;
		clone_root->num_bytes = num_bytes;

		 
		if (num_bytes >= bctx->extent_len)
			return BTRFS_ITERATE_EXTENT_INODES_STOP;
	}

	return 0;
}

static bool lookup_backref_cache(u64 leaf_bytenr, void *ctx,
				 const u64 **root_ids_ret, int *root_count_ret)
{
	struct backref_ctx *bctx = ctx;
	struct send_ctx *sctx = bctx->sctx;
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	const u64 key = leaf_bytenr >> fs_info->sectorsize_bits;
	struct btrfs_lru_cache_entry *raw_entry;
	struct backref_cache_entry *entry;

	if (btrfs_lru_cache_size(&sctx->backref_cache) == 0)
		return false;

	 
	if (fs_info->last_reloc_trans > sctx->backref_cache_last_reloc_trans) {
		btrfs_lru_cache_clear(&sctx->backref_cache);
		return false;
	}

	raw_entry = btrfs_lru_cache_lookup(&sctx->backref_cache, key, 0);
	if (!raw_entry)
		return false;

	entry = container_of(raw_entry, struct backref_cache_entry, entry);
	*root_ids_ret = entry->root_ids;
	*root_count_ret = entry->num_roots;

	return true;
}

static void store_backref_cache(u64 leaf_bytenr, const struct ulist *root_ids,
				void *ctx)
{
	struct backref_ctx *bctx = ctx;
	struct send_ctx *sctx = bctx->sctx;
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	struct backref_cache_entry *new_entry;
	struct ulist_iterator uiter;
	struct ulist_node *node;
	int ret;

	 
	new_entry = kmalloc(sizeof(struct backref_cache_entry), GFP_NOFS);
	 
	if (!new_entry)
		return;

	new_entry->entry.key = leaf_bytenr >> fs_info->sectorsize_bits;
	new_entry->entry.gen = 0;
	new_entry->num_roots = 0;
	ULIST_ITER_INIT(&uiter);
	while ((node = ulist_next(root_ids, &uiter)) != NULL) {
		const u64 root_id = node->val;
		struct clone_root *root;

		root = bsearch((void *)(uintptr_t)root_id, sctx->clone_roots,
			       sctx->clone_roots_cnt, sizeof(struct clone_root),
			       __clone_root_cmp_bsearch);
		if (!root)
			continue;

		 
		if (new_entry->num_roots >= SEND_MAX_BACKREF_CACHE_ROOTS) {
			kfree(new_entry);
			return;
		}

		new_entry->root_ids[new_entry->num_roots] = root_id;
		new_entry->num_roots++;
	}

	 
	ret = btrfs_lru_cache_store(&sctx->backref_cache, &new_entry->entry,
				    GFP_NOFS);
	ASSERT(ret == 0 || ret == -ENOMEM);
	if (ret) {
		 
		kfree(new_entry);
		return;
	}

	 
	if (btrfs_lru_cache_size(&sctx->backref_cache) == 1)
		sctx->backref_cache_last_reloc_trans = fs_info->last_reloc_trans;
}

static int check_extent_item(u64 bytenr, const struct btrfs_extent_item *ei,
			     const struct extent_buffer *leaf, void *ctx)
{
	const u64 refs = btrfs_extent_refs(leaf, ei);
	const struct backref_ctx *bctx = ctx;
	const struct send_ctx *sctx = bctx->sctx;

	if (bytenr == bctx->bytenr) {
		const u64 flags = btrfs_extent_flags(leaf, ei);

		if (WARN_ON(flags & BTRFS_EXTENT_FLAG_TREE_BLOCK))
			return -EUCLEAN;

		 
		if (refs == 1 && sctx->clone_roots_cnt == 1)
			return -ENOENT;
	}

	 
	if (refs > SEND_MAX_EXTENT_REFS)
		return -ENOENT;

	return 0;
}

static bool skip_self_data_ref(u64 root, u64 ino, u64 offset, void *ctx)
{
	const struct backref_ctx *bctx = ctx;

	if (ino == bctx->cur_objectid &&
	    root == bctx->backref_owner &&
	    offset == bctx->backref_offset)
		return true;

	return false;
}

 
static int find_extent_clone(struct send_ctx *sctx,
			     struct btrfs_path *path,
			     u64 ino, u64 data_offset,
			     u64 ino_size,
			     struct clone_root **found)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret;
	int extent_type;
	u64 logical;
	u64 disk_byte;
	u64 num_bytes;
	struct btrfs_file_extent_item *fi;
	struct extent_buffer *eb = path->nodes[0];
	struct backref_ctx backref_ctx = { 0 };
	struct btrfs_backref_walk_ctx backref_walk_ctx = { 0 };
	struct clone_root *cur_clone_root;
	int compressed;
	u32 i;

	 
	if (data_offset >= ino_size)
		return 0;

	fi = btrfs_item_ptr(eb, path->slots[0], struct btrfs_file_extent_item);
	extent_type = btrfs_file_extent_type(eb, fi);
	if (extent_type == BTRFS_FILE_EXTENT_INLINE)
		return -ENOENT;

	disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);
	if (disk_byte == 0)
		return -ENOENT;

	compressed = btrfs_file_extent_compression(eb, fi);
	num_bytes = btrfs_file_extent_num_bytes(eb, fi);
	logical = disk_byte + btrfs_file_extent_offset(eb, fi);

	 
	for (i = 0; i < sctx->clone_roots_cnt; i++) {
		cur_clone_root = sctx->clone_roots + i;
		cur_clone_root->ino = (u64)-1;
		cur_clone_root->offset = 0;
		cur_clone_root->num_bytes = 0;
		cur_clone_root->found_ref = false;
	}

	backref_ctx.sctx = sctx;
	backref_ctx.cur_objectid = ino;
	backref_ctx.cur_offset = data_offset;
	backref_ctx.bytenr = disk_byte;
	 
	backref_ctx.backref_owner = btrfs_header_owner(eb);
	backref_ctx.backref_offset = data_offset - btrfs_file_extent_offset(eb, fi);

	 
	if (data_offset + num_bytes >= ino_size)
		backref_ctx.extent_len = ino_size - data_offset;
	else
		backref_ctx.extent_len = num_bytes;

	 
	backref_walk_ctx.bytenr = disk_byte;
	if (compressed == BTRFS_COMPRESS_NONE)
		backref_walk_ctx.extent_item_pos = btrfs_file_extent_offset(eb, fi);
	backref_walk_ctx.fs_info = fs_info;
	backref_walk_ctx.cache_lookup = lookup_backref_cache;
	backref_walk_ctx.cache_store = store_backref_cache;
	backref_walk_ctx.indirect_ref_iterator = iterate_backrefs;
	backref_walk_ctx.check_extent_item = check_extent_item;
	backref_walk_ctx.user_ctx = &backref_ctx;

	 
	if (sctx->clone_roots_cnt == 1)
		backref_walk_ctx.skip_data_ref = skip_self_data_ref;

	ret = iterate_extent_inodes(&backref_walk_ctx, true, iterate_backrefs,
				    &backref_ctx);
	if (ret < 0)
		return ret;

	down_read(&fs_info->commit_root_sem);
	if (fs_info->last_reloc_trans > sctx->last_reloc_trans) {
		 
		up_read(&fs_info->commit_root_sem);
		return -ENOENT;
	}
	up_read(&fs_info->commit_root_sem);

	btrfs_debug(fs_info,
		    "find_extent_clone: data_offset=%llu, ino=%llu, num_bytes=%llu, logical=%llu",
		    data_offset, ino, num_bytes, logical);

	if (!backref_ctx.found) {
		btrfs_debug(fs_info, "no clones found");
		return -ENOENT;
	}

	cur_clone_root = NULL;
	for (i = 0; i < sctx->clone_roots_cnt; i++) {
		struct clone_root *clone_root = &sctx->clone_roots[i];

		if (!clone_root->found_ref)
			continue;

		 
		if (!cur_clone_root ||
		    clone_root->num_bytes > cur_clone_root->num_bytes) {
			cur_clone_root = clone_root;

			 
			if (clone_root->num_bytes >= backref_ctx.extent_len)
				break;
		}
	}

	if (cur_clone_root) {
		*found = cur_clone_root;
		ret = 0;
	} else {
		ret = -ENOENT;
	}

	return ret;
}

static int read_symlink(struct btrfs_root *root,
			u64 ino,
			struct fs_path *dest)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_file_extent_item *ei;
	u8 type;
	u8 compression;
	unsigned long off;
	int len;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = 0;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret) {
		 
		btrfs_err(root->fs_info,
			  "Found empty symlink inode %llu at root %llu",
			  ino, root->root_key.objectid);
		ret = -EIO;
		goto out;
	}

	ei = btrfs_item_ptr(path->nodes[0], path->slots[0],
			struct btrfs_file_extent_item);
	type = btrfs_file_extent_type(path->nodes[0], ei);
	if (unlikely(type != BTRFS_FILE_EXTENT_INLINE)) {
		ret = -EUCLEAN;
		btrfs_crit(root->fs_info,
"send: found symlink extent that is not inline, ino %llu root %llu extent type %d",
			   ino, btrfs_root_id(root), type);
		goto out;
	}
	compression = btrfs_file_extent_compression(path->nodes[0], ei);
	if (unlikely(compression != BTRFS_COMPRESS_NONE)) {
		ret = -EUCLEAN;
		btrfs_crit(root->fs_info,
"send: found symlink extent with compression, ino %llu root %llu compression type %d",
			   ino, btrfs_root_id(root), compression);
		goto out;
	}

	off = btrfs_file_extent_inline_start(ei);
	len = btrfs_file_extent_ram_bytes(path->nodes[0], ei);

	ret = fs_path_add_from_extent_buffer(dest, path->nodes[0], off, len);

out:
	btrfs_free_path(path);
	return ret;
}

 
static int gen_unique_name(struct send_ctx *sctx,
			   u64 ino, u64 gen,
			   struct fs_path *dest)
{
	int ret = 0;
	struct btrfs_path *path;
	struct btrfs_dir_item *di;
	char tmp[64];
	int len;
	u64 idx = 0;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	while (1) {
		struct fscrypt_str tmp_name;

		len = snprintf(tmp, sizeof(tmp), "o%llu-%llu-%llu",
				ino, gen, idx);
		ASSERT(len < sizeof(tmp));
		tmp_name.name = tmp;
		tmp_name.len = strlen(tmp);

		di = btrfs_lookup_dir_item(NULL, sctx->send_root,
				path, BTRFS_FIRST_FREE_OBJECTID,
				&tmp_name, 0);
		btrfs_release_path(path);
		if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out;
		}
		if (di) {
			 
			idx++;
			continue;
		}

		if (!sctx->parent_root) {
			 
			ret = 0;
			break;
		}

		di = btrfs_lookup_dir_item(NULL, sctx->parent_root,
				path, BTRFS_FIRST_FREE_OBJECTID,
				&tmp_name, 0);
		btrfs_release_path(path);
		if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out;
		}
		if (di) {
			 
			idx++;
			continue;
		}
		 
		break;
	}

	ret = fs_path_add(dest, tmp, strlen(tmp));

out:
	btrfs_free_path(path);
	return ret;
}

enum inode_state {
	inode_state_no_change,
	inode_state_will_create,
	inode_state_did_create,
	inode_state_will_delete,
	inode_state_did_delete,
};

static int get_cur_inode_state(struct send_ctx *sctx, u64 ino, u64 gen,
			       u64 *send_gen, u64 *parent_gen)
{
	int ret;
	int left_ret;
	int right_ret;
	u64 left_gen;
	u64 right_gen = 0;
	struct btrfs_inode_info info;

	ret = get_inode_info(sctx->send_root, ino, &info);
	if (ret < 0 && ret != -ENOENT)
		goto out;
	left_ret = (info.nlink == 0) ? -ENOENT : ret;
	left_gen = info.gen;
	if (send_gen)
		*send_gen = ((left_ret == -ENOENT) ? 0 : info.gen);

	if (!sctx->parent_root) {
		right_ret = -ENOENT;
	} else {
		ret = get_inode_info(sctx->parent_root, ino, &info);
		if (ret < 0 && ret != -ENOENT)
			goto out;
		right_ret = (info.nlink == 0) ? -ENOENT : ret;
		right_gen = info.gen;
		if (parent_gen)
			*parent_gen = ((right_ret == -ENOENT) ? 0 : info.gen);
	}

	if (!left_ret && !right_ret) {
		if (left_gen == gen && right_gen == gen) {
			ret = inode_state_no_change;
		} else if (left_gen == gen) {
			if (ino < sctx->send_progress)
				ret = inode_state_did_create;
			else
				ret = inode_state_will_create;
		} else if (right_gen == gen) {
			if (ino < sctx->send_progress)
				ret = inode_state_did_delete;
			else
				ret = inode_state_will_delete;
		} else  {
			ret = -ENOENT;
		}
	} else if (!left_ret) {
		if (left_gen == gen) {
			if (ino < sctx->send_progress)
				ret = inode_state_did_create;
			else
				ret = inode_state_will_create;
		} else {
			ret = -ENOENT;
		}
	} else if (!right_ret) {
		if (right_gen == gen) {
			if (ino < sctx->send_progress)
				ret = inode_state_did_delete;
			else
				ret = inode_state_will_delete;
		} else {
			ret = -ENOENT;
		}
	} else {
		ret = -ENOENT;
	}

out:
	return ret;
}

static int is_inode_existent(struct send_ctx *sctx, u64 ino, u64 gen,
			     u64 *send_gen, u64 *parent_gen)
{
	int ret;

	if (ino == BTRFS_FIRST_FREE_OBJECTID)
		return 1;

	ret = get_cur_inode_state(sctx, ino, gen, send_gen, parent_gen);
	if (ret < 0)
		goto out;

	if (ret == inode_state_no_change ||
	    ret == inode_state_did_create ||
	    ret == inode_state_will_delete)
		ret = 1;
	else
		ret = 0;

out:
	return ret;
}

 
static int lookup_dir_item_inode(struct btrfs_root *root,
				 u64 dir, const char *name, int name_len,
				 u64 *found_inode)
{
	int ret = 0;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	struct btrfs_path *path;
	struct fscrypt_str name_str = FSTR_INIT((char *)name, name_len);

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	di = btrfs_lookup_dir_item(NULL, root, path, dir, &name_str, 0);
	if (IS_ERR_OR_NULL(di)) {
		ret = di ? PTR_ERR(di) : -ENOENT;
		goto out;
	}
	btrfs_dir_item_key_to_cpu(path->nodes[0], di, &key);
	if (key.type == BTRFS_ROOT_ITEM_KEY) {
		ret = -ENOENT;
		goto out;
	}
	*found_inode = key.objectid;

out:
	btrfs_free_path(path);
	return ret;
}

 
static int get_first_ref(struct btrfs_root *root, u64 ino,
			 u64 *dir, u64 *dir_gen, struct fs_path *name)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_path *path;
	int len;
	u64 parent_dir;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = 0;

	ret = btrfs_search_slot_for_read(root, &key, path, 1, 0);
	if (ret < 0)
		goto out;
	if (!ret)
		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				path->slots[0]);
	if (ret || found_key.objectid != ino ||
	    (found_key.type != BTRFS_INODE_REF_KEY &&
	     found_key.type != BTRFS_INODE_EXTREF_KEY)) {
		ret = -ENOENT;
		goto out;
	}

	if (found_key.type == BTRFS_INODE_REF_KEY) {
		struct btrfs_inode_ref *iref;
		iref = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_inode_ref);
		len = btrfs_inode_ref_name_len(path->nodes[0], iref);
		ret = fs_path_add_from_extent_buffer(name, path->nodes[0],
						     (unsigned long)(iref + 1),
						     len);
		parent_dir = found_key.offset;
	} else {
		struct btrfs_inode_extref *extref;
		extref = btrfs_item_ptr(path->nodes[0], path->slots[0],
					struct btrfs_inode_extref);
		len = btrfs_inode_extref_name_len(path->nodes[0], extref);
		ret = fs_path_add_from_extent_buffer(name, path->nodes[0],
					(unsigned long)&extref->name, len);
		parent_dir = btrfs_inode_extref_parent(path->nodes[0], extref);
	}
	if (ret < 0)
		goto out;
	btrfs_release_path(path);

	if (dir_gen) {
		ret = get_inode_gen(root, parent_dir, dir_gen);
		if (ret < 0)
			goto out;
	}

	*dir = parent_dir;

out:
	btrfs_free_path(path);
	return ret;
}

static int is_first_ref(struct btrfs_root *root,
			u64 ino, u64 dir,
			const char *name, int name_len)
{
	int ret;
	struct fs_path *tmp_name;
	u64 tmp_dir;

	tmp_name = fs_path_alloc();
	if (!tmp_name)
		return -ENOMEM;

	ret = get_first_ref(root, ino, &tmp_dir, NULL, tmp_name);
	if (ret < 0)
		goto out;

	if (dir != tmp_dir || name_len != fs_path_len(tmp_name)) {
		ret = 0;
		goto out;
	}

	ret = !memcmp(tmp_name->start, name, name_len);

out:
	fs_path_free(tmp_name);
	return ret;
}

 
static int will_overwrite_ref(struct send_ctx *sctx, u64 dir, u64 dir_gen,
			      const char *name, int name_len,
			      u64 *who_ino, u64 *who_gen, u64 *who_mode)
{
	int ret;
	u64 parent_root_dir_gen;
	u64 other_inode = 0;
	struct btrfs_inode_info info;

	if (!sctx->parent_root)
		return 0;

	ret = is_inode_existent(sctx, dir, dir_gen, NULL, &parent_root_dir_gen);
	if (ret <= 0)
		return 0;

	 
	if (sctx->parent_root && dir != BTRFS_FIRST_FREE_OBJECTID &&
	    parent_root_dir_gen != dir_gen)
		return 0;

	ret = lookup_dir_item_inode(sctx->parent_root, dir, name, name_len,
				    &other_inode);
	if (ret == -ENOENT)
		return 0;
	else if (ret < 0)
		return ret;

	 
	if (other_inode > sctx->send_progress ||
	    is_waiting_for_move(sctx, other_inode)) {
		ret = get_inode_info(sctx->parent_root, other_inode, &info);
		if (ret < 0)
			return ret;

		*who_ino = other_inode;
		*who_gen = info.gen;
		*who_mode = info.mode;
		return 1;
	}

	return 0;
}

 
static int did_overwrite_ref(struct send_ctx *sctx,
			    u64 dir, u64 dir_gen,
			    u64 ino, u64 ino_gen,
			    const char *name, int name_len)
{
	int ret;
	u64 ow_inode;
	u64 ow_gen = 0;
	u64 send_root_dir_gen;

	if (!sctx->parent_root)
		return 0;

	ret = is_inode_existent(sctx, dir, dir_gen, &send_root_dir_gen, NULL);
	if (ret <= 0)
		return ret;

	 
	if (dir != BTRFS_FIRST_FREE_OBJECTID && send_root_dir_gen != dir_gen)
		return 0;

	 
	ret = lookup_dir_item_inode(sctx->send_root, dir, name, name_len,
				    &ow_inode);
	if (ret == -ENOENT) {
		 
		return 0;
	} else if (ret < 0) {
		return ret;
	}

	if (ow_inode == ino) {
		ret = get_inode_gen(sctx->send_root, ow_inode, &ow_gen);
		if (ret < 0)
			return ret;

		 
		if (ow_gen == ino_gen)
			return 0;
	}

	 
	if (ow_inode < sctx->send_progress)
		return 1;

	if (ino != sctx->cur_ino && ow_inode == sctx->cur_ino) {
		if (ow_gen == 0) {
			ret = get_inode_gen(sctx->send_root, ow_inode, &ow_gen);
			if (ret < 0)
				return ret;
		}
		if (ow_gen == sctx->cur_inode_gen)
			return 1;
	}

	return 0;
}

 
static int did_overwrite_first_ref(struct send_ctx *sctx, u64 ino, u64 gen)
{
	int ret = 0;
	struct fs_path *name = NULL;
	u64 dir;
	u64 dir_gen;

	if (!sctx->parent_root)
		goto out;

	name = fs_path_alloc();
	if (!name)
		return -ENOMEM;

	ret = get_first_ref(sctx->parent_root, ino, &dir, &dir_gen, name);
	if (ret < 0)
		goto out;

	ret = did_overwrite_ref(sctx, dir, dir_gen, ino, gen,
			name->start, fs_path_len(name));

out:
	fs_path_free(name);
	return ret;
}

static inline struct name_cache_entry *name_cache_search(struct send_ctx *sctx,
							 u64 ino, u64 gen)
{
	struct btrfs_lru_cache_entry *entry;

	entry = btrfs_lru_cache_lookup(&sctx->name_cache, ino, gen);
	if (!entry)
		return NULL;

	return container_of(entry, struct name_cache_entry, entry);
}

 
static int __get_cur_name_and_parent(struct send_ctx *sctx,
				     u64 ino, u64 gen,
				     u64 *parent_ino,
				     u64 *parent_gen,
				     struct fs_path *dest)
{
	int ret;
	int nce_ret;
	struct name_cache_entry *nce;

	 
	nce = name_cache_search(sctx, ino, gen);
	if (nce) {
		if (ino < sctx->send_progress && nce->need_later_update) {
			btrfs_lru_cache_remove(&sctx->name_cache, &nce->entry);
			nce = NULL;
		} else {
			*parent_ino = nce->parent_ino;
			*parent_gen = nce->parent_gen;
			ret = fs_path_add(dest, nce->name, nce->name_len);
			if (ret < 0)
				goto out;
			ret = nce->ret;
			goto out;
		}
	}

	 
	ret = is_inode_existent(sctx, ino, gen, NULL, NULL);
	if (ret < 0)
		goto out;

	if (!ret) {
		ret = gen_unique_name(sctx, ino, gen, dest);
		if (ret < 0)
			goto out;
		ret = 1;
		goto out_cache;
	}

	 
	if (ino < sctx->send_progress)
		ret = get_first_ref(sctx->send_root, ino,
				    parent_ino, parent_gen, dest);
	else
		ret = get_first_ref(sctx->parent_root, ino,
				    parent_ino, parent_gen, dest);
	if (ret < 0)
		goto out;

	 
	ret = did_overwrite_ref(sctx, *parent_ino, *parent_gen, ino, gen,
			dest->start, dest->end - dest->start);
	if (ret < 0)
		goto out;
	if (ret) {
		fs_path_reset(dest);
		ret = gen_unique_name(sctx, ino, gen, dest);
		if (ret < 0)
			goto out;
		ret = 1;
	}

out_cache:
	 
	nce = kmalloc(sizeof(*nce) + fs_path_len(dest) + 1, GFP_KERNEL);
	if (!nce) {
		ret = -ENOMEM;
		goto out;
	}

	nce->entry.key = ino;
	nce->entry.gen = gen;
	nce->parent_ino = *parent_ino;
	nce->parent_gen = *parent_gen;
	nce->name_len = fs_path_len(dest);
	nce->ret = ret;
	strcpy(nce->name, dest->start);

	if (ino < sctx->send_progress)
		nce->need_later_update = 0;
	else
		nce->need_later_update = 1;

	nce_ret = btrfs_lru_cache_store(&sctx->name_cache, &nce->entry, GFP_KERNEL);
	if (nce_ret < 0) {
		kfree(nce);
		ret = nce_ret;
	}

out:
	return ret;
}

 
static int get_cur_path(struct send_ctx *sctx, u64 ino, u64 gen,
			struct fs_path *dest)
{
	int ret = 0;
	struct fs_path *name = NULL;
	u64 parent_inode = 0;
	u64 parent_gen = 0;
	int stop = 0;

	name = fs_path_alloc();
	if (!name) {
		ret = -ENOMEM;
		goto out;
	}

	dest->reversed = 1;
	fs_path_reset(dest);

	while (!stop && ino != BTRFS_FIRST_FREE_OBJECTID) {
		struct waiting_dir_move *wdm;

		fs_path_reset(name);

		if (is_waiting_for_rm(sctx, ino, gen)) {
			ret = gen_unique_name(sctx, ino, gen, name);
			if (ret < 0)
				goto out;
			ret = fs_path_add_path(dest, name);
			break;
		}

		wdm = get_waiting_dir_move(sctx, ino);
		if (wdm && wdm->orphanized) {
			ret = gen_unique_name(sctx, ino, gen, name);
			stop = 1;
		} else if (wdm) {
			ret = get_first_ref(sctx->parent_root, ino,
					    &parent_inode, &parent_gen, name);
		} else {
			ret = __get_cur_name_and_parent(sctx, ino, gen,
							&parent_inode,
							&parent_gen, name);
			if (ret)
				stop = 1;
		}

		if (ret < 0)
			goto out;

		ret = fs_path_add_path(dest, name);
		if (ret < 0)
			goto out;

		ino = parent_inode;
		gen = parent_gen;
	}

out:
	fs_path_free(name);
	if (!ret)
		fs_path_unreverse(dest);
	return ret;
}

 
static int send_subvol_begin(struct send_ctx *sctx)
{
	int ret;
	struct btrfs_root *send_root = sctx->send_root;
	struct btrfs_root *parent_root = sctx->parent_root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_root_ref *ref;
	struct extent_buffer *leaf;
	char *name = NULL;
	int namelen;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	name = kmalloc(BTRFS_PATH_NAME_MAX, GFP_KERNEL);
	if (!name) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	key.objectid = send_root->root_key.objectid;
	key.type = BTRFS_ROOT_BACKREF_KEY;
	key.offset = 0;

	ret = btrfs_search_slot_for_read(send_root->fs_info->tree_root,
				&key, path, 1, 0);
	if (ret < 0)
		goto out;
	if (ret) {
		ret = -ENOENT;
		goto out;
	}

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	if (key.type != BTRFS_ROOT_BACKREF_KEY ||
	    key.objectid != send_root->root_key.objectid) {
		ret = -ENOENT;
		goto out;
	}
	ref = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_root_ref);
	namelen = btrfs_root_ref_name_len(leaf, ref);
	read_extent_buffer(leaf, name, (unsigned long)(ref + 1), namelen);
	btrfs_release_path(path);

	if (parent_root) {
		ret = begin_cmd(sctx, BTRFS_SEND_C_SNAPSHOT);
		if (ret < 0)
			goto out;
	} else {
		ret = begin_cmd(sctx, BTRFS_SEND_C_SUBVOL);
		if (ret < 0)
			goto out;
	}

	TLV_PUT_STRING(sctx, BTRFS_SEND_A_PATH, name, namelen);

	if (!btrfs_is_empty_uuid(sctx->send_root->root_item.received_uuid))
		TLV_PUT_UUID(sctx, BTRFS_SEND_A_UUID,
			    sctx->send_root->root_item.received_uuid);
	else
		TLV_PUT_UUID(sctx, BTRFS_SEND_A_UUID,
			    sctx->send_root->root_item.uuid);

	TLV_PUT_U64(sctx, BTRFS_SEND_A_CTRANSID,
		    btrfs_root_ctransid(&sctx->send_root->root_item));
	if (parent_root) {
		if (!btrfs_is_empty_uuid(parent_root->root_item.received_uuid))
			TLV_PUT_UUID(sctx, BTRFS_SEND_A_CLONE_UUID,
				     parent_root->root_item.received_uuid);
		else
			TLV_PUT_UUID(sctx, BTRFS_SEND_A_CLONE_UUID,
				     parent_root->root_item.uuid);
		TLV_PUT_U64(sctx, BTRFS_SEND_A_CLONE_CTRANSID,
			    btrfs_root_ctransid(&sctx->parent_root->root_item));
	}

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	btrfs_free_path(path);
	kfree(name);
	return ret;
}

static int send_truncate(struct send_ctx *sctx, u64 ino, u64 gen, u64 size)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p;

	btrfs_debug(fs_info, "send_truncate %llu size=%llu", ino, size);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = begin_cmd(sctx, BTRFS_SEND_C_TRUNCATE);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, ino, gen, p);
	if (ret < 0)
		goto out;
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_SIZE, size);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

static int send_chmod(struct send_ctx *sctx, u64 ino, u64 gen, u64 mode)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p;

	btrfs_debug(fs_info, "send_chmod %llu mode=%llu", ino, mode);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = begin_cmd(sctx, BTRFS_SEND_C_CHMOD);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, ino, gen, p);
	if (ret < 0)
		goto out;
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_MODE, mode & 07777);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

static int send_fileattr(struct send_ctx *sctx, u64 ino, u64 gen, u64 fileattr)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p;

	if (sctx->proto < 2)
		return 0;

	btrfs_debug(fs_info, "send_fileattr %llu fileattr=%llu", ino, fileattr);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = begin_cmd(sctx, BTRFS_SEND_C_FILEATTR);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, ino, gen, p);
	if (ret < 0)
		goto out;
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_FILEATTR, fileattr);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

static int send_chown(struct send_ctx *sctx, u64 ino, u64 gen, u64 uid, u64 gid)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p;

	btrfs_debug(fs_info, "send_chown %llu uid=%llu, gid=%llu",
		    ino, uid, gid);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = begin_cmd(sctx, BTRFS_SEND_C_CHOWN);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, ino, gen, p);
	if (ret < 0)
		goto out;
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UID, uid);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_GID, gid);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

static int send_utimes(struct send_ctx *sctx, u64 ino, u64 gen)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p = NULL;
	struct btrfs_inode_item *ii;
	struct btrfs_path *path = NULL;
	struct extent_buffer *eb;
	struct btrfs_key key;
	int slot;

	btrfs_debug(fs_info, "send_utimes %llu", ino);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	path = alloc_path_for_send();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	key.objectid = ino;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;
	ret = btrfs_search_slot(NULL, sctx->send_root, &key, path, 0, 0);
	if (ret > 0)
		ret = -ENOENT;
	if (ret < 0)
		goto out;

	eb = path->nodes[0];
	slot = path->slots[0];
	ii = btrfs_item_ptr(eb, slot, struct btrfs_inode_item);

	ret = begin_cmd(sctx, BTRFS_SEND_C_UTIMES);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, ino, gen, p);
	if (ret < 0)
		goto out;
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_BTRFS_TIMESPEC(sctx, BTRFS_SEND_A_ATIME, eb, &ii->atime);
	TLV_PUT_BTRFS_TIMESPEC(sctx, BTRFS_SEND_A_MTIME, eb, &ii->mtime);
	TLV_PUT_BTRFS_TIMESPEC(sctx, BTRFS_SEND_A_CTIME, eb, &ii->ctime);
	if (sctx->proto >= 2)
		TLV_PUT_BTRFS_TIMESPEC(sctx, BTRFS_SEND_A_OTIME, eb, &ii->otime);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	btrfs_free_path(path);
	return ret;
}

 
static int cache_dir_utimes(struct send_ctx *sctx, u64 dir, u64 gen)
{
	struct btrfs_lru_cache_entry *entry;
	int ret;

	entry = btrfs_lru_cache_lookup(&sctx->dir_utimes_cache, dir, gen);
	if (entry != NULL)
		return 0;

	 
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return send_utimes(sctx, dir, gen);

	entry->key = dir;
	entry->gen = gen;

	ret = btrfs_lru_cache_store(&sctx->dir_utimes_cache, entry, GFP_KERNEL);
	ASSERT(ret != -EEXIST);
	if (ret) {
		kfree(entry);
		return send_utimes(sctx, dir, gen);
	}

	return 0;
}

static int trim_dir_utimes_cache(struct send_ctx *sctx)
{
	while (btrfs_lru_cache_size(&sctx->dir_utimes_cache) >
	       SEND_MAX_DIR_UTIMES_CACHE_SIZE) {
		struct btrfs_lru_cache_entry *lru;
		int ret;

		lru = btrfs_lru_cache_lru_entry(&sctx->dir_utimes_cache);
		ASSERT(lru != NULL);

		ret = send_utimes(sctx, lru->key, lru->gen);
		if (ret)
			return ret;

		btrfs_lru_cache_remove(&sctx->dir_utimes_cache, lru);
	}

	return 0;
}

 
static int send_create_inode(struct send_ctx *sctx, u64 ino)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p;
	int cmd;
	struct btrfs_inode_info info;
	u64 gen;
	u64 mode;
	u64 rdev;

	btrfs_debug(fs_info, "send_create_inode %llu", ino);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	if (ino != sctx->cur_ino) {
		ret = get_inode_info(sctx->send_root, ino, &info);
		if (ret < 0)
			goto out;
		gen = info.gen;
		mode = info.mode;
		rdev = info.rdev;
	} else {
		gen = sctx->cur_inode_gen;
		mode = sctx->cur_inode_mode;
		rdev = sctx->cur_inode_rdev;
	}

	if (S_ISREG(mode)) {
		cmd = BTRFS_SEND_C_MKFILE;
	} else if (S_ISDIR(mode)) {
		cmd = BTRFS_SEND_C_MKDIR;
	} else if (S_ISLNK(mode)) {
		cmd = BTRFS_SEND_C_SYMLINK;
	} else if (S_ISCHR(mode) || S_ISBLK(mode)) {
		cmd = BTRFS_SEND_C_MKNOD;
	} else if (S_ISFIFO(mode)) {
		cmd = BTRFS_SEND_C_MKFIFO;
	} else if (S_ISSOCK(mode)) {
		cmd = BTRFS_SEND_C_MKSOCK;
	} else {
		btrfs_warn(sctx->send_root->fs_info, "unexpected inode type %o",
				(int)(mode & S_IFMT));
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = begin_cmd(sctx, cmd);
	if (ret < 0)
		goto out;

	ret = gen_unique_name(sctx, ino, gen, p);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_INO, ino);

	if (S_ISLNK(mode)) {
		fs_path_reset(p);
		ret = read_symlink(sctx->send_root, ino, p);
		if (ret < 0)
			goto out;
		TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH_LINK, p);
	} else if (S_ISCHR(mode) || S_ISBLK(mode) ||
		   S_ISFIFO(mode) || S_ISSOCK(mode)) {
		TLV_PUT_U64(sctx, BTRFS_SEND_A_RDEV, new_encode_dev(rdev));
		TLV_PUT_U64(sctx, BTRFS_SEND_A_MODE, mode);
	}

	ret = send_cmd(sctx);
	if (ret < 0)
		goto out;


tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

static void cache_dir_created(struct send_ctx *sctx, u64 dir)
{
	struct btrfs_lru_cache_entry *entry;
	int ret;

	 
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;

	entry->key = dir;
	entry->gen = 0;
	ret = btrfs_lru_cache_store(&sctx->dir_created_cache, entry, GFP_KERNEL);
	if (ret < 0)
		kfree(entry);
}

 
static int did_create_dir(struct send_ctx *sctx, u64 dir)
{
	int ret = 0;
	int iter_ret = 0;
	struct btrfs_path *path = NULL;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_key di_key;
	struct btrfs_dir_item *di;

	if (btrfs_lru_cache_lookup(&sctx->dir_created_cache, dir, 0))
		return 1;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = dir;
	key.type = BTRFS_DIR_INDEX_KEY;
	key.offset = 0;

	btrfs_for_each_slot(sctx->send_root, &key, &found_key, path, iter_ret) {
		struct extent_buffer *eb = path->nodes[0];

		if (found_key.objectid != key.objectid ||
		    found_key.type != key.type) {
			ret = 0;
			break;
		}

		di = btrfs_item_ptr(eb, path->slots[0], struct btrfs_dir_item);
		btrfs_dir_item_key_to_cpu(eb, di, &di_key);

		if (di_key.type != BTRFS_ROOT_ITEM_KEY &&
		    di_key.objectid < sctx->send_progress) {
			ret = 1;
			cache_dir_created(sctx, dir);
			break;
		}
	}
	 
	if (iter_ret < 0)
		ret = iter_ret;

	btrfs_free_path(path);
	return ret;
}

 
static int send_create_inode_if_needed(struct send_ctx *sctx)
{
	int ret;

	if (S_ISDIR(sctx->cur_inode_mode)) {
		ret = did_create_dir(sctx, sctx->cur_ino);
		if (ret < 0)
			return ret;
		else if (ret > 0)
			return 0;
	}

	ret = send_create_inode(sctx, sctx->cur_ino);

	if (ret == 0 && S_ISDIR(sctx->cur_inode_mode))
		cache_dir_created(sctx, sctx->cur_ino);

	return ret;
}

struct recorded_ref {
	struct list_head list;
	char *name;
	struct fs_path *full_path;
	u64 dir;
	u64 dir_gen;
	int name_len;
	struct rb_node node;
	struct rb_root *root;
};

static struct recorded_ref *recorded_ref_alloc(void)
{
	struct recorded_ref *ref;

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return NULL;
	RB_CLEAR_NODE(&ref->node);
	INIT_LIST_HEAD(&ref->list);
	return ref;
}

static void recorded_ref_free(struct recorded_ref *ref)
{
	if (!ref)
		return;
	if (!RB_EMPTY_NODE(&ref->node))
		rb_erase(&ref->node, ref->root);
	list_del(&ref->list);
	fs_path_free(ref->full_path);
	kfree(ref);
}

static void set_ref_path(struct recorded_ref *ref, struct fs_path *path)
{
	ref->full_path = path;
	ref->name = (char *)kbasename(ref->full_path->start);
	ref->name_len = ref->full_path->end - ref->name;
}

static int dup_ref(struct recorded_ref *ref, struct list_head *list)
{
	struct recorded_ref *new;

	new = recorded_ref_alloc();
	if (!new)
		return -ENOMEM;

	new->dir = ref->dir;
	new->dir_gen = ref->dir_gen;
	list_add_tail(&new->list, list);
	return 0;
}

static void __free_recorded_refs(struct list_head *head)
{
	struct recorded_ref *cur;

	while (!list_empty(head)) {
		cur = list_entry(head->next, struct recorded_ref, list);
		recorded_ref_free(cur);
	}
}

static void free_recorded_refs(struct send_ctx *sctx)
{
	__free_recorded_refs(&sctx->new_refs);
	__free_recorded_refs(&sctx->deleted_refs);
}

 
static int orphanize_inode(struct send_ctx *sctx, u64 ino, u64 gen,
			  struct fs_path *path)
{
	int ret;
	struct fs_path *orphan;

	orphan = fs_path_alloc();
	if (!orphan)
		return -ENOMEM;

	ret = gen_unique_name(sctx, ino, gen, orphan);
	if (ret < 0)
		goto out;

	ret = send_rename(sctx, path, orphan);

out:
	fs_path_free(orphan);
	return ret;
}

static struct orphan_dir_info *add_orphan_dir_info(struct send_ctx *sctx,
						   u64 dir_ino, u64 dir_gen)
{
	struct rb_node **p = &sctx->orphan_dirs.rb_node;
	struct rb_node *parent = NULL;
	struct orphan_dir_info *entry, *odi;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct orphan_dir_info, node);
		if (dir_ino < entry->ino)
			p = &(*p)->rb_left;
		else if (dir_ino > entry->ino)
			p = &(*p)->rb_right;
		else if (dir_gen < entry->gen)
			p = &(*p)->rb_left;
		else if (dir_gen > entry->gen)
			p = &(*p)->rb_right;
		else
			return entry;
	}

	odi = kmalloc(sizeof(*odi), GFP_KERNEL);
	if (!odi)
		return ERR_PTR(-ENOMEM);
	odi->ino = dir_ino;
	odi->gen = dir_gen;
	odi->last_dir_index_offset = 0;
	odi->dir_high_seq_ino = 0;

	rb_link_node(&odi->node, parent, p);
	rb_insert_color(&odi->node, &sctx->orphan_dirs);
	return odi;
}

static struct orphan_dir_info *get_orphan_dir_info(struct send_ctx *sctx,
						   u64 dir_ino, u64 gen)
{
	struct rb_node *n = sctx->orphan_dirs.rb_node;
	struct orphan_dir_info *entry;

	while (n) {
		entry = rb_entry(n, struct orphan_dir_info, node);
		if (dir_ino < entry->ino)
			n = n->rb_left;
		else if (dir_ino > entry->ino)
			n = n->rb_right;
		else if (gen < entry->gen)
			n = n->rb_left;
		else if (gen > entry->gen)
			n = n->rb_right;
		else
			return entry;
	}
	return NULL;
}

static int is_waiting_for_rm(struct send_ctx *sctx, u64 dir_ino, u64 gen)
{
	struct orphan_dir_info *odi = get_orphan_dir_info(sctx, dir_ino, gen);

	return odi != NULL;
}

static void free_orphan_dir_info(struct send_ctx *sctx,
				 struct orphan_dir_info *odi)
{
	if (!odi)
		return;
	rb_erase(&odi->node, &sctx->orphan_dirs);
	kfree(odi);
}

 
static int can_rmdir(struct send_ctx *sctx, u64 dir, u64 dir_gen)
{
	int ret = 0;
	int iter_ret = 0;
	struct btrfs_root *root = sctx->parent_root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_key loc;
	struct btrfs_dir_item *di;
	struct orphan_dir_info *odi = NULL;
	u64 dir_high_seq_ino = 0;
	u64 last_dir_index_offset = 0;

	 
	if (dir == BTRFS_FIRST_FREE_OBJECTID)
		return 0;

	odi = get_orphan_dir_info(sctx, dir, dir_gen);
	if (odi && sctx->cur_ino < odi->dir_high_seq_ino)
		return 0;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	if (!odi) {
		 
		key.objectid = dir;
		key.type = BTRFS_DIR_INDEX_KEY;
		key.offset = (u64)-1;

		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0) {
			goto out;
		} else if (ret > 0) {
			 
			ASSERT(path->slots[0] > 0);
			if (WARN_ON(path->slots[0] == 0)) {
				ret = -EUCLEAN;
				goto out;
			}
			path->slots[0]--;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.objectid != dir || key.type != BTRFS_DIR_INDEX_KEY) {
			 
			ret = 1;
			goto out;
		}

		di = btrfs_item_ptr(path->nodes[0], path->slots[0],
				    struct btrfs_dir_item);
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &loc);
		dir_high_seq_ino = loc.objectid;
		if (sctx->cur_ino < dir_high_seq_ino) {
			ret = 0;
			goto out;
		}

		btrfs_release_path(path);
	}

	key.objectid = dir;
	key.type = BTRFS_DIR_INDEX_KEY;
	key.offset = (odi ? odi->last_dir_index_offset : 0);

	btrfs_for_each_slot(root, &key, &found_key, path, iter_ret) {
		struct waiting_dir_move *dm;

		if (found_key.objectid != key.objectid ||
		    found_key.type != key.type)
			break;

		di = btrfs_item_ptr(path->nodes[0], path->slots[0],
				struct btrfs_dir_item);
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &loc);

		dir_high_seq_ino = max(dir_high_seq_ino, loc.objectid);
		last_dir_index_offset = found_key.offset;

		dm = get_waiting_dir_move(sctx, loc.objectid);
		if (dm) {
			dm->rmdir_ino = dir;
			dm->rmdir_gen = dir_gen;
			ret = 0;
			goto out;
		}

		if (loc.objectid > sctx->cur_ino) {
			ret = 0;
			goto out;
		}
	}
	if (iter_ret < 0) {
		ret = iter_ret;
		goto out;
	}
	free_orphan_dir_info(sctx, odi);

	ret = 1;

out:
	btrfs_free_path(path);

	if (ret)
		return ret;

	if (!odi) {
		odi = add_orphan_dir_info(sctx, dir, dir_gen);
		if (IS_ERR(odi))
			return PTR_ERR(odi);

		odi->gen = dir_gen;
	}

	odi->last_dir_index_offset = last_dir_index_offset;
	odi->dir_high_seq_ino = max(odi->dir_high_seq_ino, dir_high_seq_ino);

	return 0;
}

static int is_waiting_for_move(struct send_ctx *sctx, u64 ino)
{
	struct waiting_dir_move *entry = get_waiting_dir_move(sctx, ino);

	return entry != NULL;
}

static int add_waiting_dir_move(struct send_ctx *sctx, u64 ino, bool orphanized)
{
	struct rb_node **p = &sctx->waiting_dir_moves.rb_node;
	struct rb_node *parent = NULL;
	struct waiting_dir_move *entry, *dm;

	dm = kmalloc(sizeof(*dm), GFP_KERNEL);
	if (!dm)
		return -ENOMEM;
	dm->ino = ino;
	dm->rmdir_ino = 0;
	dm->rmdir_gen = 0;
	dm->orphanized = orphanized;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct waiting_dir_move, node);
		if (ino < entry->ino) {
			p = &(*p)->rb_left;
		} else if (ino > entry->ino) {
			p = &(*p)->rb_right;
		} else {
			kfree(dm);
			return -EEXIST;
		}
	}

	rb_link_node(&dm->node, parent, p);
	rb_insert_color(&dm->node, &sctx->waiting_dir_moves);
	return 0;
}

static struct waiting_dir_move *
get_waiting_dir_move(struct send_ctx *sctx, u64 ino)
{
	struct rb_node *n = sctx->waiting_dir_moves.rb_node;
	struct waiting_dir_move *entry;

	while (n) {
		entry = rb_entry(n, struct waiting_dir_move, node);
		if (ino < entry->ino)
			n = n->rb_left;
		else if (ino > entry->ino)
			n = n->rb_right;
		else
			return entry;
	}
	return NULL;
}

static void free_waiting_dir_move(struct send_ctx *sctx,
				  struct waiting_dir_move *dm)
{
	if (!dm)
		return;
	rb_erase(&dm->node, &sctx->waiting_dir_moves);
	kfree(dm);
}

static int add_pending_dir_move(struct send_ctx *sctx,
				u64 ino,
				u64 ino_gen,
				u64 parent_ino,
				struct list_head *new_refs,
				struct list_head *deleted_refs,
				const bool is_orphan)
{
	struct rb_node **p = &sctx->pending_dir_moves.rb_node;
	struct rb_node *parent = NULL;
	struct pending_dir_move *entry = NULL, *pm;
	struct recorded_ref *cur;
	int exists = 0;
	int ret;

	pm = kmalloc(sizeof(*pm), GFP_KERNEL);
	if (!pm)
		return -ENOMEM;
	pm->parent_ino = parent_ino;
	pm->ino = ino;
	pm->gen = ino_gen;
	INIT_LIST_HEAD(&pm->list);
	INIT_LIST_HEAD(&pm->update_refs);
	RB_CLEAR_NODE(&pm->node);

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct pending_dir_move, node);
		if (parent_ino < entry->parent_ino) {
			p = &(*p)->rb_left;
		} else if (parent_ino > entry->parent_ino) {
			p = &(*p)->rb_right;
		} else {
			exists = 1;
			break;
		}
	}

	list_for_each_entry(cur, deleted_refs, list) {
		ret = dup_ref(cur, &pm->update_refs);
		if (ret < 0)
			goto out;
	}
	list_for_each_entry(cur, new_refs, list) {
		ret = dup_ref(cur, &pm->update_refs);
		if (ret < 0)
			goto out;
	}

	ret = add_waiting_dir_move(sctx, pm->ino, is_orphan);
	if (ret)
		goto out;

	if (exists) {
		list_add_tail(&pm->list, &entry->list);
	} else {
		rb_link_node(&pm->node, parent, p);
		rb_insert_color(&pm->node, &sctx->pending_dir_moves);
	}
	ret = 0;
out:
	if (ret) {
		__free_recorded_refs(&pm->update_refs);
		kfree(pm);
	}
	return ret;
}

static struct pending_dir_move *get_pending_dir_moves(struct send_ctx *sctx,
						      u64 parent_ino)
{
	struct rb_node *n = sctx->pending_dir_moves.rb_node;
	struct pending_dir_move *entry;

	while (n) {
		entry = rb_entry(n, struct pending_dir_move, node);
		if (parent_ino < entry->parent_ino)
			n = n->rb_left;
		else if (parent_ino > entry->parent_ino)
			n = n->rb_right;
		else
			return entry;
	}
	return NULL;
}

static int path_loop(struct send_ctx *sctx, struct fs_path *name,
		     u64 ino, u64 gen, u64 *ancestor_ino)
{
	int ret = 0;
	u64 parent_inode = 0;
	u64 parent_gen = 0;
	u64 start_ino = ino;

	*ancestor_ino = 0;
	while (ino != BTRFS_FIRST_FREE_OBJECTID) {
		fs_path_reset(name);

		if (is_waiting_for_rm(sctx, ino, gen))
			break;
		if (is_waiting_for_move(sctx, ino)) {
			if (*ancestor_ino == 0)
				*ancestor_ino = ino;
			ret = get_first_ref(sctx->parent_root, ino,
					    &parent_inode, &parent_gen, name);
		} else {
			ret = __get_cur_name_and_parent(sctx, ino, gen,
							&parent_inode,
							&parent_gen, name);
			if (ret > 0) {
				ret = 0;
				break;
			}
		}
		if (ret < 0)
			break;
		if (parent_inode == start_ino) {
			ret = 1;
			if (*ancestor_ino == 0)
				*ancestor_ino = ino;
			break;
		}
		ino = parent_inode;
		gen = parent_gen;
	}
	return ret;
}

static int apply_dir_move(struct send_ctx *sctx, struct pending_dir_move *pm)
{
	struct fs_path *from_path = NULL;
	struct fs_path *to_path = NULL;
	struct fs_path *name = NULL;
	u64 orig_progress = sctx->send_progress;
	struct recorded_ref *cur;
	u64 parent_ino, parent_gen;
	struct waiting_dir_move *dm = NULL;
	u64 rmdir_ino = 0;
	u64 rmdir_gen;
	u64 ancestor;
	bool is_orphan;
	int ret;

	name = fs_path_alloc();
	from_path = fs_path_alloc();
	if (!name || !from_path) {
		ret = -ENOMEM;
		goto out;
	}

	dm = get_waiting_dir_move(sctx, pm->ino);
	ASSERT(dm);
	rmdir_ino = dm->rmdir_ino;
	rmdir_gen = dm->rmdir_gen;
	is_orphan = dm->orphanized;
	free_waiting_dir_move(sctx, dm);

	if (is_orphan) {
		ret = gen_unique_name(sctx, pm->ino,
				      pm->gen, from_path);
	} else {
		ret = get_first_ref(sctx->parent_root, pm->ino,
				    &parent_ino, &parent_gen, name);
		if (ret < 0)
			goto out;
		ret = get_cur_path(sctx, parent_ino, parent_gen,
				   from_path);
		if (ret < 0)
			goto out;
		ret = fs_path_add_path(from_path, name);
	}
	if (ret < 0)
		goto out;

	sctx->send_progress = sctx->cur_ino + 1;
	ret = path_loop(sctx, name, pm->ino, pm->gen, &ancestor);
	if (ret < 0)
		goto out;
	if (ret) {
		LIST_HEAD(deleted_refs);
		ASSERT(ancestor > BTRFS_FIRST_FREE_OBJECTID);
		ret = add_pending_dir_move(sctx, pm->ino, pm->gen, ancestor,
					   &pm->update_refs, &deleted_refs,
					   is_orphan);
		if (ret < 0)
			goto out;
		if (rmdir_ino) {
			dm = get_waiting_dir_move(sctx, pm->ino);
			ASSERT(dm);
			dm->rmdir_ino = rmdir_ino;
			dm->rmdir_gen = rmdir_gen;
		}
		goto out;
	}
	fs_path_reset(name);
	to_path = name;
	name = NULL;
	ret = get_cur_path(sctx, pm->ino, pm->gen, to_path);
	if (ret < 0)
		goto out;

	ret = send_rename(sctx, from_path, to_path);
	if (ret < 0)
		goto out;

	if (rmdir_ino) {
		struct orphan_dir_info *odi;
		u64 gen;

		odi = get_orphan_dir_info(sctx, rmdir_ino, rmdir_gen);
		if (!odi) {
			 
			goto finish;
		}
		gen = odi->gen;

		ret = can_rmdir(sctx, rmdir_ino, gen);
		if (ret < 0)
			goto out;
		if (!ret)
			goto finish;

		name = fs_path_alloc();
		if (!name) {
			ret = -ENOMEM;
			goto out;
		}
		ret = get_cur_path(sctx, rmdir_ino, gen, name);
		if (ret < 0)
			goto out;
		ret = send_rmdir(sctx, name);
		if (ret < 0)
			goto out;
	}

finish:
	ret = cache_dir_utimes(sctx, pm->ino, pm->gen);
	if (ret < 0)
		goto out;

	 
	list_for_each_entry(cur, &pm->update_refs, list) {
		 
		ret = get_inode_info(sctx->send_root, cur->dir, NULL);
		if (ret == -ENOENT) {
			ret = 0;
			continue;
		}
		if (ret < 0)
			goto out;

		ret = cache_dir_utimes(sctx, cur->dir, cur->dir_gen);
		if (ret < 0)
			goto out;
	}

out:
	fs_path_free(name);
	fs_path_free(from_path);
	fs_path_free(to_path);
	sctx->send_progress = orig_progress;

	return ret;
}

static void free_pending_move(struct send_ctx *sctx, struct pending_dir_move *m)
{
	if (!list_empty(&m->list))
		list_del(&m->list);
	if (!RB_EMPTY_NODE(&m->node))
		rb_erase(&m->node, &sctx->pending_dir_moves);
	__free_recorded_refs(&m->update_refs);
	kfree(m);
}

static void tail_append_pending_moves(struct send_ctx *sctx,
				      struct pending_dir_move *moves,
				      struct list_head *stack)
{
	if (list_empty(&moves->list)) {
		list_add_tail(&moves->list, stack);
	} else {
		LIST_HEAD(list);
		list_splice_init(&moves->list, &list);
		list_add_tail(&moves->list, stack);
		list_splice_tail(&list, stack);
	}
	if (!RB_EMPTY_NODE(&moves->node)) {
		rb_erase(&moves->node, &sctx->pending_dir_moves);
		RB_CLEAR_NODE(&moves->node);
	}
}

static int apply_children_dir_moves(struct send_ctx *sctx)
{
	struct pending_dir_move *pm;
	LIST_HEAD(stack);
	u64 parent_ino = sctx->cur_ino;
	int ret = 0;

	pm = get_pending_dir_moves(sctx, parent_ino);
	if (!pm)
		return 0;

	tail_append_pending_moves(sctx, pm, &stack);

	while (!list_empty(&stack)) {
		pm = list_first_entry(&stack, struct pending_dir_move, list);
		parent_ino = pm->ino;
		ret = apply_dir_move(sctx, pm);
		free_pending_move(sctx, pm);
		if (ret)
			goto out;
		pm = get_pending_dir_moves(sctx, parent_ino);
		if (pm)
			tail_append_pending_moves(sctx, pm, &stack);
	}
	return 0;

out:
	while (!list_empty(&stack)) {
		pm = list_first_entry(&stack, struct pending_dir_move, list);
		free_pending_move(sctx, pm);
	}
	return ret;
}

 
static int wait_for_dest_dir_move(struct send_ctx *sctx,
				  struct recorded_ref *parent_ref,
				  const bool is_orphan)
{
	struct btrfs_fs_info *fs_info = sctx->parent_root->fs_info;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key di_key;
	struct btrfs_dir_item *di;
	u64 left_gen;
	u64 right_gen;
	int ret = 0;
	struct waiting_dir_move *wdm;

	if (RB_EMPTY_ROOT(&sctx->waiting_dir_moves))
		return 0;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = parent_ref->dir;
	key.type = BTRFS_DIR_ITEM_KEY;
	key.offset = btrfs_name_hash(parent_ref->name, parent_ref->name_len);

	ret = btrfs_search_slot(NULL, sctx->parent_root, &key, path, 0, 0);
	if (ret < 0) {
		goto out;
	} else if (ret > 0) {
		ret = 0;
		goto out;
	}

	di = btrfs_match_dir_item_name(fs_info, path, parent_ref->name,
				       parent_ref->name_len);
	if (!di) {
		ret = 0;
		goto out;
	}
	 
	btrfs_dir_item_key_to_cpu(path->nodes[0], di, &di_key);
	if (di_key.type != BTRFS_INODE_ITEM_KEY) {
		ret = 0;
		goto out;
	}

	ret = get_inode_gen(sctx->parent_root, di_key.objectid, &left_gen);
	if (ret < 0)
		goto out;
	ret = get_inode_gen(sctx->send_root, di_key.objectid, &right_gen);
	if (ret < 0) {
		if (ret == -ENOENT)
			ret = 0;
		goto out;
	}

	 
	if (right_gen != left_gen) {
		ret = 0;
		goto out;
	}

	wdm = get_waiting_dir_move(sctx, di_key.objectid);
	if (wdm && !wdm->orphanized) {
		ret = add_pending_dir_move(sctx,
					   sctx->cur_ino,
					   sctx->cur_inode_gen,
					   di_key.objectid,
					   &sctx->new_refs,
					   &sctx->deleted_refs,
					   is_orphan);
		if (!ret)
			ret = 1;
	}
out:
	btrfs_free_path(path);
	return ret;
}

 
static int check_ino_in_path(struct btrfs_root *root,
			     const u64 ino1,
			     const u64 ino1_gen,
			     const u64 ino2,
			     const u64 ino2_gen,
			     struct fs_path *fs_path)
{
	u64 ino = ino2;

	if (ino1 == ino2)
		return ino1_gen == ino2_gen;

	while (ino > BTRFS_FIRST_FREE_OBJECTID) {
		u64 parent;
		u64 parent_gen;
		int ret;

		fs_path_reset(fs_path);
		ret = get_first_ref(root, ino, &parent, &parent_gen, fs_path);
		if (ret < 0)
			return ret;
		if (parent == ino1)
			return parent_gen == ino1_gen;
		ino = parent;
	}
	return 0;
}

 
static int is_ancestor(struct btrfs_root *root,
		       const u64 ino1,
		       const u64 ino1_gen,
		       const u64 ino2,
		       struct fs_path *fs_path)
{
	bool free_fs_path = false;
	int ret = 0;
	int iter_ret = 0;
	struct btrfs_path *path = NULL;
	struct btrfs_key key;

	if (!fs_path) {
		fs_path = fs_path_alloc();
		if (!fs_path)
			return -ENOMEM;
		free_fs_path = true;
	}

	path = alloc_path_for_send();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	key.objectid = ino2;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = 0;

	btrfs_for_each_slot(root, &key, &key, path, iter_ret) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		u32 cur_offset = 0;
		u32 item_size;

		if (key.objectid != ino2)
			break;
		if (key.type != BTRFS_INODE_REF_KEY &&
		    key.type != BTRFS_INODE_EXTREF_KEY)
			break;

		item_size = btrfs_item_size(leaf, slot);
		while (cur_offset < item_size) {
			u64 parent;
			u64 parent_gen;

			if (key.type == BTRFS_INODE_EXTREF_KEY) {
				unsigned long ptr;
				struct btrfs_inode_extref *extref;

				ptr = btrfs_item_ptr_offset(leaf, slot);
				extref = (struct btrfs_inode_extref *)
					(ptr + cur_offset);
				parent = btrfs_inode_extref_parent(leaf,
								   extref);
				cur_offset += sizeof(*extref);
				cur_offset += btrfs_inode_extref_name_len(leaf,
								  extref);
			} else {
				parent = key.offset;
				cur_offset = item_size;
			}

			ret = get_inode_gen(root, parent, &parent_gen);
			if (ret < 0)
				goto out;
			ret = check_ino_in_path(root, ino1, ino1_gen,
						parent, parent_gen, fs_path);
			if (ret)
				goto out;
		}
	}
	ret = 0;
	if (iter_ret < 0)
		ret = iter_ret;

out:
	btrfs_free_path(path);
	if (free_fs_path)
		fs_path_free(fs_path);
	return ret;
}

static int wait_for_parent_move(struct send_ctx *sctx,
				struct recorded_ref *parent_ref,
				const bool is_orphan)
{
	int ret = 0;
	u64 ino = parent_ref->dir;
	u64 ino_gen = parent_ref->dir_gen;
	u64 parent_ino_before, parent_ino_after;
	struct fs_path *path_before = NULL;
	struct fs_path *path_after = NULL;
	int len1, len2;

	path_after = fs_path_alloc();
	path_before = fs_path_alloc();
	if (!path_after || !path_before) {
		ret = -ENOMEM;
		goto out;
	}

	 
	while (ino > BTRFS_FIRST_FREE_OBJECTID) {
		u64 parent_ino_after_gen;

		if (is_waiting_for_move(sctx, ino)) {
			 
			ret = is_ancestor(sctx->parent_root,
					  sctx->cur_ino, sctx->cur_inode_gen,
					  ino, path_before);
			if (ret)
				break;
		}

		fs_path_reset(path_before);
		fs_path_reset(path_after);

		ret = get_first_ref(sctx->send_root, ino, &parent_ino_after,
				    &parent_ino_after_gen, path_after);
		if (ret < 0)
			goto out;
		ret = get_first_ref(sctx->parent_root, ino, &parent_ino_before,
				    NULL, path_before);
		if (ret < 0 && ret != -ENOENT) {
			goto out;
		} else if (ret == -ENOENT) {
			ret = 0;
			break;
		}

		len1 = fs_path_len(path_before);
		len2 = fs_path_len(path_after);
		if (ino > sctx->cur_ino &&
		    (parent_ino_before != parent_ino_after || len1 != len2 ||
		     memcmp(path_before->start, path_after->start, len1))) {
			u64 parent_ino_gen;

			ret = get_inode_gen(sctx->parent_root, ino, &parent_ino_gen);
			if (ret < 0)
				goto out;
			if (ino_gen == parent_ino_gen) {
				ret = 1;
				break;
			}
		}
		ino = parent_ino_after;
		ino_gen = parent_ino_after_gen;
	}

out:
	fs_path_free(path_before);
	fs_path_free(path_after);

	if (ret == 1) {
		ret = add_pending_dir_move(sctx,
					   sctx->cur_ino,
					   sctx->cur_inode_gen,
					   ino,
					   &sctx->new_refs,
					   &sctx->deleted_refs,
					   is_orphan);
		if (!ret)
			ret = 1;
	}

	return ret;
}

static int update_ref_path(struct send_ctx *sctx, struct recorded_ref *ref)
{
	int ret;
	struct fs_path *new_path;

	 
	new_path = fs_path_alloc();
	if (!new_path)
		return -ENOMEM;

	ret = get_cur_path(sctx, ref->dir, ref->dir_gen, new_path);
	if (ret < 0) {
		fs_path_free(new_path);
		return ret;
	}
	ret = fs_path_add(new_path, ref->name, ref->name_len);
	if (ret < 0) {
		fs_path_free(new_path);
		return ret;
	}

	fs_path_free(ref->full_path);
	set_ref_path(ref, new_path);

	return 0;
}

 
static int refresh_ref_path(struct send_ctx *sctx, struct recorded_ref *ref)
{
	char *name;
	int ret;

	name = kmemdup(ref->name, ref->name_len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	fs_path_reset(ref->full_path);
	ret = get_cur_path(sctx, ref->dir, ref->dir_gen, ref->full_path);
	if (ret < 0)
		goto out;

	ret = fs_path_add(ref->full_path, name, ref->name_len);
	if (ret < 0)
		goto out;

	 
	set_ref_path(ref, ref->full_path);
out:
	kfree(name);
	return ret;
}

 
static int process_recorded_refs(struct send_ctx *sctx, int *pending_move)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct recorded_ref *cur;
	struct recorded_ref *cur2;
	LIST_HEAD(check_dirs);
	struct fs_path *valid_path = NULL;
	u64 ow_inode = 0;
	u64 ow_gen;
	u64 ow_mode;
	int did_overwrite = 0;
	int is_orphan = 0;
	u64 last_dir_ino_rm = 0;
	bool can_rename = true;
	bool orphanized_dir = false;
	bool orphanized_ancestor = false;

	btrfs_debug(fs_info, "process_recorded_refs %llu", sctx->cur_ino);

	 
	BUG_ON(sctx->cur_ino <= BTRFS_FIRST_FREE_OBJECTID);

	valid_path = fs_path_alloc();
	if (!valid_path) {
		ret = -ENOMEM;
		goto out;
	}

	 
	if (!sctx->cur_inode_new) {
		ret = did_overwrite_first_ref(sctx, sctx->cur_ino,
				sctx->cur_inode_gen);
		if (ret < 0)
			goto out;
		if (ret)
			did_overwrite = 1;
	}
	if (sctx->cur_inode_new || did_overwrite) {
		ret = gen_unique_name(sctx, sctx->cur_ino,
				sctx->cur_inode_gen, valid_path);
		if (ret < 0)
			goto out;
		is_orphan = 1;
	} else {
		ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen,
				valid_path);
		if (ret < 0)
			goto out;
	}

	 
	list_for_each_entry(cur, &sctx->new_refs, list) {
		ret = get_cur_inode_state(sctx, cur->dir, cur->dir_gen, NULL, NULL);
		if (ret < 0)
			goto out;
		if (ret == inode_state_will_create)
			continue;

		 
		ret = will_overwrite_ref(sctx, cur->dir, cur->dir_gen,
				cur->name, cur->name_len,
				&ow_inode, &ow_gen, &ow_mode);
		if (ret < 0)
			goto out;
		if (ret) {
			ret = is_first_ref(sctx->parent_root,
					   ow_inode, cur->dir, cur->name,
					   cur->name_len);
			if (ret < 0)
				goto out;
			if (ret) {
				struct name_cache_entry *nce;
				struct waiting_dir_move *wdm;

				if (orphanized_dir) {
					ret = refresh_ref_path(sctx, cur);
					if (ret < 0)
						goto out;
				}

				ret = orphanize_inode(sctx, ow_inode, ow_gen,
						cur->full_path);
				if (ret < 0)
					goto out;
				if (S_ISDIR(ow_mode))
					orphanized_dir = true;

				 
				wdm = get_waiting_dir_move(sctx, ow_inode);
				if (wdm)
					wdm->orphanized = true;

				 
				nce = name_cache_search(sctx, ow_inode, ow_gen);
				if (nce)
					btrfs_lru_cache_remove(&sctx->name_cache,
							       &nce->entry);

				 
				ret = is_ancestor(sctx->parent_root,
						  ow_inode, ow_gen,
						  sctx->cur_ino, NULL);
				if (ret > 0) {
					orphanized_ancestor = true;
					fs_path_reset(valid_path);
					ret = get_cur_path(sctx, sctx->cur_ino,
							   sctx->cur_inode_gen,
							   valid_path);
				}
				if (ret < 0)
					goto out;
			} else {
				 
				if (orphanized_dir) {
					ret = refresh_ref_path(sctx, cur);
					if (ret < 0)
						goto out;
				}
				ret = send_unlink(sctx, cur->full_path);
				if (ret < 0)
					goto out;
			}
		}

	}

	list_for_each_entry(cur, &sctx->new_refs, list) {
		 
		ret = get_cur_inode_state(sctx, cur->dir, cur->dir_gen, NULL, NULL);
		if (ret < 0)
			goto out;
		if (ret == inode_state_will_create) {
			ret = 0;
			 
			list_for_each_entry(cur2, &sctx->new_refs, list) {
				if (cur == cur2)
					break;
				if (cur2->dir == cur->dir) {
					ret = 1;
					break;
				}
			}

			 
			if (!ret)
				ret = did_create_dir(sctx, cur->dir);
			if (ret < 0)
				goto out;
			if (!ret) {
				ret = send_create_inode(sctx, cur->dir);
				if (ret < 0)
					goto out;
				cache_dir_created(sctx, cur->dir);
			}
		}

		if (S_ISDIR(sctx->cur_inode_mode) && sctx->parent_root) {
			ret = wait_for_dest_dir_move(sctx, cur, is_orphan);
			if (ret < 0)
				goto out;
			if (ret == 1) {
				can_rename = false;
				*pending_move = 1;
			}
		}

		if (S_ISDIR(sctx->cur_inode_mode) && sctx->parent_root &&
		    can_rename) {
			ret = wait_for_parent_move(sctx, cur, is_orphan);
			if (ret < 0)
				goto out;
			if (ret == 1) {
				can_rename = false;
				*pending_move = 1;
			}
		}

		 
		if (is_orphan && can_rename) {
			ret = send_rename(sctx, valid_path, cur->full_path);
			if (ret < 0)
				goto out;
			is_orphan = 0;
			ret = fs_path_copy(valid_path, cur->full_path);
			if (ret < 0)
				goto out;
		} else if (can_rename) {
			if (S_ISDIR(sctx->cur_inode_mode)) {
				 
				ret = send_rename(sctx, valid_path,
						  cur->full_path);
				if (!ret)
					ret = fs_path_copy(valid_path,
							   cur->full_path);
				if (ret < 0)
					goto out;
			} else {
				 
				if (orphanized_dir) {
					ret = update_ref_path(sctx, cur);
					if (ret < 0)
						goto out;
				}
				ret = send_link(sctx, cur->full_path,
						valid_path);
				if (ret < 0)
					goto out;
			}
		}
		ret = dup_ref(cur, &check_dirs);
		if (ret < 0)
			goto out;
	}

	if (S_ISDIR(sctx->cur_inode_mode) && sctx->cur_inode_deleted) {
		 
		ret = can_rmdir(sctx, sctx->cur_ino, sctx->cur_inode_gen);
		if (ret < 0)
			goto out;
		if (ret) {
			ret = send_rmdir(sctx, valid_path);
			if (ret < 0)
				goto out;
		} else if (!is_orphan) {
			ret = orphanize_inode(sctx, sctx->cur_ino,
					sctx->cur_inode_gen, valid_path);
			if (ret < 0)
				goto out;
			is_orphan = 1;
		}

		list_for_each_entry(cur, &sctx->deleted_refs, list) {
			ret = dup_ref(cur, &check_dirs);
			if (ret < 0)
				goto out;
		}
	} else if (S_ISDIR(sctx->cur_inode_mode) &&
		   !list_empty(&sctx->deleted_refs)) {
		 
		cur = list_entry(sctx->deleted_refs.next, struct recorded_ref,
				list);
		ret = dup_ref(cur, &check_dirs);
		if (ret < 0)
			goto out;
	} else if (!S_ISDIR(sctx->cur_inode_mode)) {
		 
		list_for_each_entry(cur, &sctx->deleted_refs, list) {
			ret = did_overwrite_ref(sctx, cur->dir, cur->dir_gen,
					sctx->cur_ino, sctx->cur_inode_gen,
					cur->name, cur->name_len);
			if (ret < 0)
				goto out;
			if (!ret) {
				 
				if (orphanized_ancestor) {
					ret = update_ref_path(sctx, cur);
					if (ret < 0)
						goto out;
				}
				ret = send_unlink(sctx, cur->full_path);
				if (ret < 0)
					goto out;
			}
			ret = dup_ref(cur, &check_dirs);
			if (ret < 0)
				goto out;
		}
		 
		if (is_orphan) {
			ret = send_unlink(sctx, valid_path);
			if (ret < 0)
				goto out;
		}
	}

	 
	list_for_each_entry(cur, &check_dirs, list) {
		 
		if (cur->dir > sctx->cur_ino)
			continue;

		ret = get_cur_inode_state(sctx, cur->dir, cur->dir_gen, NULL, NULL);
		if (ret < 0)
			goto out;

		if (ret == inode_state_did_create ||
		    ret == inode_state_no_change) {
			ret = cache_dir_utimes(sctx, cur->dir, cur->dir_gen);
			if (ret < 0)
				goto out;
		} else if (ret == inode_state_did_delete &&
			   cur->dir != last_dir_ino_rm) {
			ret = can_rmdir(sctx, cur->dir, cur->dir_gen);
			if (ret < 0)
				goto out;
			if (ret) {
				ret = get_cur_path(sctx, cur->dir,
						   cur->dir_gen, valid_path);
				if (ret < 0)
					goto out;
				ret = send_rmdir(sctx, valid_path);
				if (ret < 0)
					goto out;
				last_dir_ino_rm = cur->dir;
			}
		}
	}

	ret = 0;

out:
	__free_recorded_refs(&check_dirs);
	free_recorded_refs(sctx);
	fs_path_free(valid_path);
	return ret;
}

static int rbtree_ref_comp(const void *k, const struct rb_node *node)
{
	const struct recorded_ref *data = k;
	const struct recorded_ref *ref = rb_entry(node, struct recorded_ref, node);
	int result;

	if (data->dir > ref->dir)
		return 1;
	if (data->dir < ref->dir)
		return -1;
	if (data->dir_gen > ref->dir_gen)
		return 1;
	if (data->dir_gen < ref->dir_gen)
		return -1;
	if (data->name_len > ref->name_len)
		return 1;
	if (data->name_len < ref->name_len)
		return -1;
	result = strcmp(data->name, ref->name);
	if (result > 0)
		return 1;
	if (result < 0)
		return -1;
	return 0;
}

static bool rbtree_ref_less(struct rb_node *node, const struct rb_node *parent)
{
	const struct recorded_ref *entry = rb_entry(node, struct recorded_ref, node);

	return rbtree_ref_comp(entry, parent) < 0;
}

static int record_ref_in_tree(struct rb_root *root, struct list_head *refs,
			      struct fs_path *name, u64 dir, u64 dir_gen,
			      struct send_ctx *sctx)
{
	int ret = 0;
	struct fs_path *path = NULL;
	struct recorded_ref *ref = NULL;

	path = fs_path_alloc();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	ref = recorded_ref_alloc();
	if (!ref) {
		ret = -ENOMEM;
		goto out;
	}

	ret = get_cur_path(sctx, dir, dir_gen, path);
	if (ret < 0)
		goto out;
	ret = fs_path_add_path(path, name);
	if (ret < 0)
		goto out;

	ref->dir = dir;
	ref->dir_gen = dir_gen;
	set_ref_path(ref, path);
	list_add_tail(&ref->list, refs);
	rb_add(&ref->node, root, rbtree_ref_less);
	ref->root = root;
out:
	if (ret) {
		if (path && (!ref || !ref->full_path))
			fs_path_free(path);
		recorded_ref_free(ref);
	}
	return ret;
}

static int record_new_ref_if_needed(int num, u64 dir, int index,
				    struct fs_path *name, void *ctx)
{
	int ret = 0;
	struct send_ctx *sctx = ctx;
	struct rb_node *node = NULL;
	struct recorded_ref data;
	struct recorded_ref *ref;
	u64 dir_gen;

	ret = get_inode_gen(sctx->send_root, dir, &dir_gen);
	if (ret < 0)
		goto out;

	data.dir = dir;
	data.dir_gen = dir_gen;
	set_ref_path(&data, name);
	node = rb_find(&data, &sctx->rbtree_deleted_refs, rbtree_ref_comp);
	if (node) {
		ref = rb_entry(node, struct recorded_ref, node);
		recorded_ref_free(ref);
	} else {
		ret = record_ref_in_tree(&sctx->rbtree_new_refs,
					 &sctx->new_refs, name, dir, dir_gen,
					 sctx);
	}
out:
	return ret;
}

static int record_deleted_ref_if_needed(int num, u64 dir, int index,
					struct fs_path *name, void *ctx)
{
	int ret = 0;
	struct send_ctx *sctx = ctx;
	struct rb_node *node = NULL;
	struct recorded_ref data;
	struct recorded_ref *ref;
	u64 dir_gen;

	ret = get_inode_gen(sctx->parent_root, dir, &dir_gen);
	if (ret < 0)
		goto out;

	data.dir = dir;
	data.dir_gen = dir_gen;
	set_ref_path(&data, name);
	node = rb_find(&data, &sctx->rbtree_new_refs, rbtree_ref_comp);
	if (node) {
		ref = rb_entry(node, struct recorded_ref, node);
		recorded_ref_free(ref);
	} else {
		ret = record_ref_in_tree(&sctx->rbtree_deleted_refs,
					 &sctx->deleted_refs, name, dir,
					 dir_gen, sctx);
	}
out:
	return ret;
}

static int record_new_ref(struct send_ctx *sctx)
{
	int ret;

	ret = iterate_inode_ref(sctx->send_root, sctx->left_path,
				sctx->cmp_key, 0, record_new_ref_if_needed, sctx);
	if (ret < 0)
		goto out;
	ret = 0;

out:
	return ret;
}

static int record_deleted_ref(struct send_ctx *sctx)
{
	int ret;

	ret = iterate_inode_ref(sctx->parent_root, sctx->right_path,
				sctx->cmp_key, 0, record_deleted_ref_if_needed,
				sctx);
	if (ret < 0)
		goto out;
	ret = 0;

out:
	return ret;
}

static int record_changed_ref(struct send_ctx *sctx)
{
	int ret = 0;

	ret = iterate_inode_ref(sctx->send_root, sctx->left_path,
			sctx->cmp_key, 0, record_new_ref_if_needed, sctx);
	if (ret < 0)
		goto out;
	ret = iterate_inode_ref(sctx->parent_root, sctx->right_path,
			sctx->cmp_key, 0, record_deleted_ref_if_needed, sctx);
	if (ret < 0)
		goto out;
	ret = 0;

out:
	return ret;
}

 
static int process_all_refs(struct send_ctx *sctx,
			    enum btrfs_compare_tree_result cmd)
{
	int ret = 0;
	int iter_ret = 0;
	struct btrfs_root *root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key found_key;
	iterate_inode_ref_t cb;
	int pending_move = 0;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	if (cmd == BTRFS_COMPARE_TREE_NEW) {
		root = sctx->send_root;
		cb = record_new_ref_if_needed;
	} else if (cmd == BTRFS_COMPARE_TREE_DELETED) {
		root = sctx->parent_root;
		cb = record_deleted_ref_if_needed;
	} else {
		btrfs_err(sctx->send_root->fs_info,
				"Wrong command %d in process_all_refs", cmd);
		ret = -EINVAL;
		goto out;
	}

	key.objectid = sctx->cmp_key->objectid;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = 0;
	btrfs_for_each_slot(root, &key, &found_key, path, iter_ret) {
		if (found_key.objectid != key.objectid ||
		    (found_key.type != BTRFS_INODE_REF_KEY &&
		     found_key.type != BTRFS_INODE_EXTREF_KEY))
			break;

		ret = iterate_inode_ref(root, path, &found_key, 0, cb, sctx);
		if (ret < 0)
			goto out;
	}
	 
	if (iter_ret < 0) {
		ret = iter_ret;
		goto out;
	}
	btrfs_release_path(path);

	 
	ret = process_recorded_refs(sctx, &pending_move);
out:
	btrfs_free_path(path);
	return ret;
}

static int send_set_xattr(struct send_ctx *sctx,
			  struct fs_path *path,
			  const char *name, int name_len,
			  const char *data, int data_len)
{
	int ret = 0;

	ret = begin_cmd(sctx, BTRFS_SEND_C_SET_XATTR);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, path);
	TLV_PUT_STRING(sctx, BTRFS_SEND_A_XATTR_NAME, name, name_len);
	TLV_PUT(sctx, BTRFS_SEND_A_XATTR_DATA, data, data_len);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

static int send_remove_xattr(struct send_ctx *sctx,
			  struct fs_path *path,
			  const char *name, int name_len)
{
	int ret = 0;

	ret = begin_cmd(sctx, BTRFS_SEND_C_REMOVE_XATTR);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, path);
	TLV_PUT_STRING(sctx, BTRFS_SEND_A_XATTR_NAME, name, name_len);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

static int __process_new_xattr(int num, struct btrfs_key *di_key,
			       const char *name, int name_len, const char *data,
			       int data_len, void *ctx)
{
	int ret;
	struct send_ctx *sctx = ctx;
	struct fs_path *p;
	struct posix_acl_xattr_header dummy_acl;

	 
	if (!strncmp(name, XATTR_NAME_CAPS, name_len))
		return 0;

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	 
	if (!strncmp(name, XATTR_NAME_POSIX_ACL_ACCESS, name_len) ||
	    !strncmp(name, XATTR_NAME_POSIX_ACL_DEFAULT, name_len)) {
		if (data_len == 0) {
			dummy_acl.a_version =
					cpu_to_le32(POSIX_ACL_XATTR_VERSION);
			data = (char *)&dummy_acl;
			data_len = sizeof(dummy_acl);
		}
	}

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto out;

	ret = send_set_xattr(sctx, p, name, name_len, data, data_len);

out:
	fs_path_free(p);
	return ret;
}

static int __process_deleted_xattr(int num, struct btrfs_key *di_key,
				   const char *name, int name_len,
				   const char *data, int data_len, void *ctx)
{
	int ret;
	struct send_ctx *sctx = ctx;
	struct fs_path *p;

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto out;

	ret = send_remove_xattr(sctx, p, name, name_len);

out:
	fs_path_free(p);
	return ret;
}

static int process_new_xattr(struct send_ctx *sctx)
{
	int ret = 0;

	ret = iterate_dir_item(sctx->send_root, sctx->left_path,
			       __process_new_xattr, sctx);

	return ret;
}

static int process_deleted_xattr(struct send_ctx *sctx)
{
	return iterate_dir_item(sctx->parent_root, sctx->right_path,
				__process_deleted_xattr, sctx);
}

struct find_xattr_ctx {
	const char *name;
	int name_len;
	int found_idx;
	char *found_data;
	int found_data_len;
};

static int __find_xattr(int num, struct btrfs_key *di_key, const char *name,
			int name_len, const char *data, int data_len, void *vctx)
{
	struct find_xattr_ctx *ctx = vctx;

	if (name_len == ctx->name_len &&
	    strncmp(name, ctx->name, name_len) == 0) {
		ctx->found_idx = num;
		ctx->found_data_len = data_len;
		ctx->found_data = kmemdup(data, data_len, GFP_KERNEL);
		if (!ctx->found_data)
			return -ENOMEM;
		return 1;
	}
	return 0;
}

static int find_xattr(struct btrfs_root *root,
		      struct btrfs_path *path,
		      struct btrfs_key *key,
		      const char *name, int name_len,
		      char **data, int *data_len)
{
	int ret;
	struct find_xattr_ctx ctx;

	ctx.name = name;
	ctx.name_len = name_len;
	ctx.found_idx = -1;
	ctx.found_data = NULL;
	ctx.found_data_len = 0;

	ret = iterate_dir_item(root, path, __find_xattr, &ctx);
	if (ret < 0)
		return ret;

	if (ctx.found_idx == -1)
		return -ENOENT;
	if (data) {
		*data = ctx.found_data;
		*data_len = ctx.found_data_len;
	} else {
		kfree(ctx.found_data);
	}
	return ctx.found_idx;
}


static int __process_changed_new_xattr(int num, struct btrfs_key *di_key,
				       const char *name, int name_len,
				       const char *data, int data_len,
				       void *ctx)
{
	int ret;
	struct send_ctx *sctx = ctx;
	char *found_data = NULL;
	int found_data_len  = 0;

	ret = find_xattr(sctx->parent_root, sctx->right_path,
			 sctx->cmp_key, name, name_len, &found_data,
			 &found_data_len);
	if (ret == -ENOENT) {
		ret = __process_new_xattr(num, di_key, name, name_len, data,
					  data_len, ctx);
	} else if (ret >= 0) {
		if (data_len != found_data_len ||
		    memcmp(data, found_data, data_len)) {
			ret = __process_new_xattr(num, di_key, name, name_len,
						  data, data_len, ctx);
		} else {
			ret = 0;
		}
	}

	kfree(found_data);
	return ret;
}

static int __process_changed_deleted_xattr(int num, struct btrfs_key *di_key,
					   const char *name, int name_len,
					   const char *data, int data_len,
					   void *ctx)
{
	int ret;
	struct send_ctx *sctx = ctx;

	ret = find_xattr(sctx->send_root, sctx->left_path, sctx->cmp_key,
			 name, name_len, NULL, NULL);
	if (ret == -ENOENT)
		ret = __process_deleted_xattr(num, di_key, name, name_len, data,
					      data_len, ctx);
	else if (ret >= 0)
		ret = 0;

	return ret;
}

static int process_changed_xattr(struct send_ctx *sctx)
{
	int ret = 0;

	ret = iterate_dir_item(sctx->send_root, sctx->left_path,
			__process_changed_new_xattr, sctx);
	if (ret < 0)
		goto out;
	ret = iterate_dir_item(sctx->parent_root, sctx->right_path,
			__process_changed_deleted_xattr, sctx);

out:
	return ret;
}

static int process_all_new_xattrs(struct send_ctx *sctx)
{
	int ret = 0;
	int iter_ret = 0;
	struct btrfs_root *root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key found_key;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	root = sctx->send_root;

	key.objectid = sctx->cmp_key->objectid;
	key.type = BTRFS_XATTR_ITEM_KEY;
	key.offset = 0;
	btrfs_for_each_slot(root, &key, &found_key, path, iter_ret) {
		if (found_key.objectid != key.objectid ||
		    found_key.type != key.type) {
			ret = 0;
			break;
		}

		ret = iterate_dir_item(root, path, __process_new_xattr, sctx);
		if (ret < 0)
			break;
	}
	 
	if (iter_ret < 0)
		ret = iter_ret;

	btrfs_free_path(path);
	return ret;
}

static int send_verity(struct send_ctx *sctx, struct fs_path *path,
		       struct fsverity_descriptor *desc)
{
	int ret;

	ret = begin_cmd(sctx, BTRFS_SEND_C_ENABLE_VERITY);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, path);
	TLV_PUT_U8(sctx, BTRFS_SEND_A_VERITY_ALGORITHM,
			le8_to_cpu(desc->hash_algorithm));
	TLV_PUT_U32(sctx, BTRFS_SEND_A_VERITY_BLOCK_SIZE,
			1U << le8_to_cpu(desc->log_blocksize));
	TLV_PUT(sctx, BTRFS_SEND_A_VERITY_SALT_DATA, desc->salt,
			le8_to_cpu(desc->salt_size));
	TLV_PUT(sctx, BTRFS_SEND_A_VERITY_SIG_DATA, desc->signature,
			le32_to_cpu(desc->sig_size));

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	return ret;
}

static int process_verity(struct send_ctx *sctx)
{
	int ret = 0;
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	struct inode *inode;
	struct fs_path *p;

	inode = btrfs_iget(fs_info->sb, sctx->cur_ino, sctx->send_root);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	ret = btrfs_get_verity_descriptor(inode, NULL, 0);
	if (ret < 0)
		goto iput;

	if (ret > FS_VERITY_MAX_DESCRIPTOR_SIZE) {
		ret = -EMSGSIZE;
		goto iput;
	}
	if (!sctx->verity_descriptor) {
		sctx->verity_descriptor = kvmalloc(FS_VERITY_MAX_DESCRIPTOR_SIZE,
						   GFP_KERNEL);
		if (!sctx->verity_descriptor) {
			ret = -ENOMEM;
			goto iput;
		}
	}

	ret = btrfs_get_verity_descriptor(inode, sctx->verity_descriptor, ret);
	if (ret < 0)
		goto iput;

	p = fs_path_alloc();
	if (!p) {
		ret = -ENOMEM;
		goto iput;
	}
	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto free_path;

	ret = send_verity(sctx, p, sctx->verity_descriptor);
	if (ret < 0)
		goto free_path;

free_path:
	fs_path_free(p);
iput:
	iput(inode);
	return ret;
}

static inline u64 max_send_read_size(const struct send_ctx *sctx)
{
	return sctx->send_max_size - SZ_16K;
}

static int put_data_header(struct send_ctx *sctx, u32 len)
{
	if (WARN_ON_ONCE(sctx->put_data))
		return -EINVAL;
	sctx->put_data = true;
	if (sctx->proto >= 2) {
		 
		if (sctx->send_max_size - sctx->send_size < sizeof(__le16) + len)
			return -EOVERFLOW;
		put_unaligned_le16(BTRFS_SEND_A_DATA, sctx->send_buf + sctx->send_size);
		sctx->send_size += sizeof(__le16);
	} else {
		struct btrfs_tlv_header *hdr;

		if (sctx->send_max_size - sctx->send_size < sizeof(*hdr) + len)
			return -EOVERFLOW;
		hdr = (struct btrfs_tlv_header *)(sctx->send_buf + sctx->send_size);
		put_unaligned_le16(BTRFS_SEND_A_DATA, &hdr->tlv_type);
		put_unaligned_le16(len, &hdr->tlv_len);
		sctx->send_size += sizeof(*hdr);
	}
	return 0;
}

static int put_file_data(struct send_ctx *sctx, u64 offset, u32 len)
{
	struct btrfs_root *root = sctx->send_root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct page *page;
	pgoff_t index = offset >> PAGE_SHIFT;
	pgoff_t last_index;
	unsigned pg_offset = offset_in_page(offset);
	int ret;

	ret = put_data_header(sctx, len);
	if (ret)
		return ret;

	last_index = (offset + len - 1) >> PAGE_SHIFT;

	while (index <= last_index) {
		unsigned cur_len = min_t(unsigned, len,
					 PAGE_SIZE - pg_offset);

		page = find_lock_page(sctx->cur_inode->i_mapping, index);
		if (!page) {
			page_cache_sync_readahead(sctx->cur_inode->i_mapping,
						  &sctx->ra, NULL, index,
						  last_index + 1 - index);

			page = find_or_create_page(sctx->cur_inode->i_mapping,
						   index, GFP_KERNEL);
			if (!page) {
				ret = -ENOMEM;
				break;
			}
		}

		if (PageReadahead(page))
			page_cache_async_readahead(sctx->cur_inode->i_mapping,
						   &sctx->ra, NULL, page_folio(page),
						   index, last_index + 1 - index);

		if (!PageUptodate(page)) {
			btrfs_read_folio(NULL, page_folio(page));
			lock_page(page);
			if (!PageUptodate(page)) {
				unlock_page(page);
				btrfs_err(fs_info,
			"send: IO error at offset %llu for inode %llu root %llu",
					page_offset(page), sctx->cur_ino,
					sctx->send_root->root_key.objectid);
				put_page(page);
				ret = -EIO;
				break;
			}
		}

		memcpy_from_page(sctx->send_buf + sctx->send_size, page,
				 pg_offset, cur_len);
		unlock_page(page);
		put_page(page);
		index++;
		pg_offset = 0;
		len -= cur_len;
		sctx->send_size += cur_len;
	}

	return ret;
}

 
static int send_write(struct send_ctx *sctx, u64 offset, u32 len)
{
	struct btrfs_fs_info *fs_info = sctx->send_root->fs_info;
	int ret = 0;
	struct fs_path *p;

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	btrfs_debug(fs_info, "send_write offset=%llu, len=%d", offset, len);

	ret = begin_cmd(sctx, BTRFS_SEND_C_WRITE);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_FILE_OFFSET, offset);
	ret = put_file_data(sctx, offset, len);
	if (ret < 0)
		goto out;

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

 
static int send_clone(struct send_ctx *sctx,
		      u64 offset, u32 len,
		      struct clone_root *clone_root)
{
	int ret = 0;
	struct fs_path *p;
	u64 gen;

	btrfs_debug(sctx->send_root->fs_info,
		    "send_clone offset=%llu, len=%d, clone_root=%llu, clone_inode=%llu, clone_offset=%llu",
		    offset, len, clone_root->root->root_key.objectid,
		    clone_root->ino, clone_root->offset);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = begin_cmd(sctx, BTRFS_SEND_C_CLONE);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto out;

	TLV_PUT_U64(sctx, BTRFS_SEND_A_FILE_OFFSET, offset);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_CLONE_LEN, len);
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);

	if (clone_root->root == sctx->send_root) {
		ret = get_inode_gen(sctx->send_root, clone_root->ino, &gen);
		if (ret < 0)
			goto out;
		ret = get_cur_path(sctx, clone_root->ino, gen, p);
	} else {
		ret = get_inode_path(clone_root->root, clone_root->ino, p);
	}
	if (ret < 0)
		goto out;

	 
	if (!btrfs_is_empty_uuid(clone_root->root->root_item.received_uuid))
		TLV_PUT_UUID(sctx, BTRFS_SEND_A_CLONE_UUID,
			     clone_root->root->root_item.received_uuid);
	else
		TLV_PUT_UUID(sctx, BTRFS_SEND_A_CLONE_UUID,
			     clone_root->root->root_item.uuid);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_CLONE_CTRANSID,
		    btrfs_root_ctransid(&clone_root->root->root_item));
	TLV_PUT_PATH(sctx, BTRFS_SEND_A_CLONE_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_CLONE_OFFSET,
			clone_root->offset);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

 
static int send_update_extent(struct send_ctx *sctx,
			      u64 offset, u32 len)
{
	int ret = 0;
	struct fs_path *p;

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;

	ret = begin_cmd(sctx, BTRFS_SEND_C_UPDATE_EXTENT);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto out;

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_FILE_OFFSET, offset);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_SIZE, len);

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(p);
	return ret;
}

static int send_hole(struct send_ctx *sctx, u64 end)
{
	struct fs_path *p = NULL;
	u64 read_size = max_send_read_size(sctx);
	u64 offset = sctx->cur_inode_last_extent;
	int ret = 0;

	 
	if (offset >= sctx->cur_inode_size)
		return 0;

	 
	end = min_t(u64, end, sctx->cur_inode_size);

	if (sctx->flags & BTRFS_SEND_FLAG_NO_FILE_DATA)
		return send_update_extent(sctx, offset, end - offset);

	p = fs_path_alloc();
	if (!p)
		return -ENOMEM;
	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, p);
	if (ret < 0)
		goto tlv_put_failure;
	while (offset < end) {
		u64 len = min(end - offset, read_size);

		ret = begin_cmd(sctx, BTRFS_SEND_C_WRITE);
		if (ret < 0)
			break;
		TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, p);
		TLV_PUT_U64(sctx, BTRFS_SEND_A_FILE_OFFSET, offset);
		ret = put_data_header(sctx, len);
		if (ret < 0)
			break;
		memset(sctx->send_buf + sctx->send_size, 0, len);
		sctx->send_size += len;
		ret = send_cmd(sctx);
		if (ret < 0)
			break;
		offset += len;
	}
	sctx->cur_inode_next_write_offset = offset;
tlv_put_failure:
	fs_path_free(p);
	return ret;
}

static int send_encoded_inline_extent(struct send_ctx *sctx,
				      struct btrfs_path *path, u64 offset,
				      u64 len)
{
	struct btrfs_root *root = sctx->send_root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct inode *inode;
	struct fs_path *fspath;
	struct extent_buffer *leaf = path->nodes[0];
	struct btrfs_key key;
	struct btrfs_file_extent_item *ei;
	u64 ram_bytes;
	size_t inline_size;
	int ret;

	inode = btrfs_iget(fs_info->sb, sctx->cur_ino, root);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	fspath = fs_path_alloc();
	if (!fspath) {
		ret = -ENOMEM;
		goto out;
	}

	ret = begin_cmd(sctx, BTRFS_SEND_C_ENCODED_WRITE);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, fspath);
	if (ret < 0)
		goto out;

	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_file_extent_item);
	ram_bytes = btrfs_file_extent_ram_bytes(leaf, ei);
	inline_size = btrfs_file_extent_inline_item_len(leaf, path->slots[0]);

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, fspath);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_FILE_OFFSET, offset);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UNENCODED_FILE_LEN,
		    min(key.offset + ram_bytes - offset, len));
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UNENCODED_LEN, ram_bytes);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UNENCODED_OFFSET, offset - key.offset);
	ret = btrfs_encoded_io_compression_from_extent(fs_info,
				btrfs_file_extent_compression(leaf, ei));
	if (ret < 0)
		goto out;
	TLV_PUT_U32(sctx, BTRFS_SEND_A_COMPRESSION, ret);

	ret = put_data_header(sctx, inline_size);
	if (ret < 0)
		goto out;
	read_extent_buffer(leaf, sctx->send_buf + sctx->send_size,
			   btrfs_file_extent_inline_start(ei), inline_size);
	sctx->send_size += inline_size;

	ret = send_cmd(sctx);

tlv_put_failure:
out:
	fs_path_free(fspath);
	iput(inode);
	return ret;
}

static int send_encoded_extent(struct send_ctx *sctx, struct btrfs_path *path,
			       u64 offset, u64 len)
{
	struct btrfs_root *root = sctx->send_root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct inode *inode;
	struct fs_path *fspath;
	struct extent_buffer *leaf = path->nodes[0];
	struct btrfs_key key;
	struct btrfs_file_extent_item *ei;
	u64 disk_bytenr, disk_num_bytes;
	u32 data_offset;
	struct btrfs_cmd_header *hdr;
	u32 crc;
	int ret;

	inode = btrfs_iget(fs_info->sb, sctx->cur_ino, root);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	fspath = fs_path_alloc();
	if (!fspath) {
		ret = -ENOMEM;
		goto out;
	}

	ret = begin_cmd(sctx, BTRFS_SEND_C_ENCODED_WRITE);
	if (ret < 0)
		goto out;

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, fspath);
	if (ret < 0)
		goto out;

	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_file_extent_item);
	disk_bytenr = btrfs_file_extent_disk_bytenr(leaf, ei);
	disk_num_bytes = btrfs_file_extent_disk_num_bytes(leaf, ei);

	TLV_PUT_PATH(sctx, BTRFS_SEND_A_PATH, fspath);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_FILE_OFFSET, offset);
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UNENCODED_FILE_LEN,
		    min(key.offset + btrfs_file_extent_num_bytes(leaf, ei) - offset,
			len));
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UNENCODED_LEN,
		    btrfs_file_extent_ram_bytes(leaf, ei));
	TLV_PUT_U64(sctx, BTRFS_SEND_A_UNENCODED_OFFSET,
		    offset - key.offset + btrfs_file_extent_offset(leaf, ei));
	ret = btrfs_encoded_io_compression_from_extent(fs_info,
				btrfs_file_extent_compression(leaf, ei));
	if (ret < 0)
		goto out;
	TLV_PUT_U32(sctx, BTRFS_SEND_A_COMPRESSION, ret);
	TLV_PUT_U32(sctx, BTRFS_SEND_A_ENCRYPTION, 0);

	ret = put_data_header(sctx, disk_num_bytes);
	if (ret < 0)
		goto out;

	 
	data_offset = PAGE_ALIGN(sctx->send_size);
	if (data_offset > sctx->send_max_size ||
	    sctx->send_max_size - data_offset < disk_num_bytes) {
		ret = -EOVERFLOW;
		goto out;
	}

	 
	ret = btrfs_encoded_read_regular_fill_pages(BTRFS_I(inode), offset,
						    disk_bytenr, disk_num_bytes,
						    sctx->send_buf_pages +
						    (data_offset >> PAGE_SHIFT));
	if (ret)
		goto out;

	hdr = (struct btrfs_cmd_header *)sctx->send_buf;
	hdr->len = cpu_to_le32(sctx->send_size + disk_num_bytes - sizeof(*hdr));
	hdr->crc = 0;
	crc = btrfs_crc32c(0, sctx->send_buf, sctx->send_size);
	crc = btrfs_crc32c(crc, sctx->send_buf + data_offset, disk_num_bytes);
	hdr->crc = cpu_to_le32(crc);

	ret = write_buf(sctx->send_filp, sctx->send_buf, sctx->send_size,
			&sctx->send_off);
	if (!ret) {
		ret = write_buf(sctx->send_filp, sctx->send_buf + data_offset,
				disk_num_bytes, &sctx->send_off);
	}
	sctx->send_size = 0;
	sctx->put_data = false;

tlv_put_failure:
out:
	fs_path_free(fspath);
	iput(inode);
	return ret;
}

static int send_extent_data(struct send_ctx *sctx, struct btrfs_path *path,
			    const u64 offset, const u64 len)
{
	const u64 end = offset + len;
	struct extent_buffer *leaf = path->nodes[0];
	struct btrfs_file_extent_item *ei;
	u64 read_size = max_send_read_size(sctx);
	u64 sent = 0;

	if (sctx->flags & BTRFS_SEND_FLAG_NO_FILE_DATA)
		return send_update_extent(sctx, offset, len);

	ei = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	if ((sctx->flags & BTRFS_SEND_FLAG_COMPRESSED) &&
	    btrfs_file_extent_compression(leaf, ei) != BTRFS_COMPRESS_NONE) {
		bool is_inline = (btrfs_file_extent_type(leaf, ei) ==
				  BTRFS_FILE_EXTENT_INLINE);

		 
		if (is_inline &&
		    btrfs_file_extent_inline_item_len(leaf,
						      path->slots[0]) <= len) {
			return send_encoded_inline_extent(sctx, path, offset,
							  len);
		} else if (!is_inline &&
			   btrfs_file_extent_disk_num_bytes(leaf, ei) <= len) {
			return send_encoded_extent(sctx, path, offset, len);
		}
	}

	if (sctx->cur_inode == NULL) {
		struct btrfs_root *root = sctx->send_root;

		sctx->cur_inode = btrfs_iget(root->fs_info->sb, sctx->cur_ino, root);
		if (IS_ERR(sctx->cur_inode)) {
			int err = PTR_ERR(sctx->cur_inode);

			sctx->cur_inode = NULL;
			return err;
		}
		memset(&sctx->ra, 0, sizeof(struct file_ra_state));
		file_ra_state_init(&sctx->ra, sctx->cur_inode->i_mapping);

		 
		sctx->clean_page_cache = (sctx->cur_inode->i_mapping->nrpages == 0);
		sctx->page_cache_clear_start = round_down(offset, PAGE_SIZE);
	}

	while (sent < len) {
		u64 size = min(len - sent, read_size);
		int ret;

		ret = send_write(sctx, offset + sent, size);
		if (ret < 0)
			return ret;
		sent += size;
	}

	if (sctx->clean_page_cache && PAGE_ALIGNED(end)) {
		 
		truncate_inode_pages_range(&sctx->cur_inode->i_data,
					   sctx->page_cache_clear_start,
					   end - 1);
		sctx->page_cache_clear_start = end;
	}

	return 0;
}

 
static int send_capabilities(struct send_ctx *sctx)
{
	struct fs_path *fspath = NULL;
	struct btrfs_path *path;
	struct btrfs_dir_item *di;
	struct extent_buffer *leaf;
	unsigned long data_ptr;
	char *buf = NULL;
	int buf_len;
	int ret = 0;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	di = btrfs_lookup_xattr(NULL, sctx->send_root, path, sctx->cur_ino,
				XATTR_NAME_CAPS, strlen(XATTR_NAME_CAPS), 0);
	if (!di) {
		 
		goto out;
	} else if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto out;
	}

	leaf = path->nodes[0];
	buf_len = btrfs_dir_data_len(leaf, di);

	fspath = fs_path_alloc();
	buf = kmalloc(buf_len, GFP_KERNEL);
	if (!fspath || !buf) {
		ret = -ENOMEM;
		goto out;
	}

	ret = get_cur_path(sctx, sctx->cur_ino, sctx->cur_inode_gen, fspath);
	if (ret < 0)
		goto out;

	data_ptr = (unsigned long)(di + 1) + btrfs_dir_name_len(leaf, di);
	read_extent_buffer(leaf, buf, data_ptr, buf_len);

	ret = send_set_xattr(sctx, fspath, XATTR_NAME_CAPS,
			strlen(XATTR_NAME_CAPS), buf, buf_len);
out:
	kfree(buf);
	fs_path_free(fspath);
	btrfs_free_path(path);
	return ret;
}

static int clone_range(struct send_ctx *sctx, struct btrfs_path *dst_path,
		       struct clone_root *clone_root, const u64 disk_byte,
		       u64 data_offset, u64 offset, u64 len)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	int ret;
	struct btrfs_inode_info info;
	u64 clone_src_i_size = 0;

	 
	if (clone_root->offset == 0 &&
	    len == sctx->send_root->fs_info->sectorsize)
		return send_extent_data(sctx, dst_path, offset, len);

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	 
	ret = get_inode_info(clone_root->root, clone_root->ino, &info);
	btrfs_release_path(path);
	if (ret < 0)
		goto out;
	clone_src_i_size = info.size;

	 
	key.objectid = clone_root->ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = clone_root->offset;
	ret = btrfs_search_slot(NULL, clone_root->root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret > 0 && path->slots[0] > 0) {
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0] - 1);
		if (key.objectid == clone_root->ino &&
		    key.type == BTRFS_EXTENT_DATA_KEY)
			path->slots[0]--;
	}

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		struct btrfs_file_extent_item *ei;
		u8 type;
		u64 ext_len;
		u64 clone_len;
		u64 clone_data_offset;
		bool crossed_src_i_size = false;

		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(clone_root->root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);

		 
		if (key.objectid != clone_root->ino ||
		    key.type != BTRFS_EXTENT_DATA_KEY)
			break;

		ei = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);
		type = btrfs_file_extent_type(leaf, ei);
		if (type == BTRFS_FILE_EXTENT_INLINE) {
			ext_len = btrfs_file_extent_ram_bytes(leaf, ei);
			ext_len = PAGE_ALIGN(ext_len);
		} else {
			ext_len = btrfs_file_extent_num_bytes(leaf, ei);
		}

		if (key.offset + ext_len <= clone_root->offset)
			goto next;

		if (key.offset > clone_root->offset) {
			 
			u64 hole_len = key.offset - clone_root->offset;

			if (hole_len > len)
				hole_len = len;
			ret = send_extent_data(sctx, dst_path, offset,
					       hole_len);
			if (ret < 0)
				goto out;

			len -= hole_len;
			if (len == 0)
				break;
			offset += hole_len;
			clone_root->offset += hole_len;
			data_offset += hole_len;
		}

		if (key.offset >= clone_root->offset + len)
			break;

		if (key.offset >= clone_src_i_size)
			break;

		if (key.offset + ext_len > clone_src_i_size) {
			ext_len = clone_src_i_size - key.offset;
			crossed_src_i_size = true;
		}

		clone_data_offset = btrfs_file_extent_offset(leaf, ei);
		if (btrfs_file_extent_disk_bytenr(leaf, ei) == disk_byte) {
			clone_root->offset = key.offset;
			if (clone_data_offset < data_offset &&
				clone_data_offset + ext_len > data_offset) {
				u64 extent_offset;

				extent_offset = data_offset - clone_data_offset;
				ext_len -= extent_offset;
				clone_data_offset += extent_offset;
				clone_root->offset += extent_offset;
			}
		}

		clone_len = min_t(u64, ext_len, len);

		if (btrfs_file_extent_disk_bytenr(leaf, ei) == disk_byte &&
		    clone_data_offset == data_offset) {
			const u64 src_end = clone_root->offset + clone_len;
			const u64 sectorsize = SZ_64K;

			 
			if (src_end == clone_src_i_size &&
			    !IS_ALIGNED(src_end, sectorsize) &&
			    offset + clone_len < sctx->cur_inode_size) {
				u64 slen;

				slen = ALIGN_DOWN(src_end - clone_root->offset,
						  sectorsize);
				if (slen > 0) {
					ret = send_clone(sctx, offset, slen,
							 clone_root);
					if (ret < 0)
						goto out;
				}
				ret = send_extent_data(sctx, dst_path,
						       offset + slen,
						       clone_len - slen);
			} else {
				ret = send_clone(sctx, offset, clone_len,
						 clone_root);
			}
		} else if (crossed_src_i_size && clone_len < len) {
			 
			break;
		} else {
			ret = send_extent_data(sctx, dst_path, offset,
					       clone_len);
		}

		if (ret < 0)
			goto out;

		len -= clone_len;
		if (len == 0)
			break;
		offset += clone_len;
		clone_root->offset += clone_len;

		 
		if (clone_root->root == sctx->send_root &&
		    clone_root->ino == sctx->cur_ino &&
		    clone_root->offset >= sctx->cur_inode_next_write_offset)
			break;

		data_offset += clone_len;
next:
		path->slots[0]++;
	}

	if (len > 0)
		ret = send_extent_data(sctx, dst_path, offset, len);
	else
		ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

static int send_write_or_clone(struct send_ctx *sctx,
			       struct btrfs_path *path,
			       struct btrfs_key *key,
			       struct clone_root *clone_root)
{
	int ret = 0;
	u64 offset = key->offset;
	u64 end;
	u64 bs = sctx->send_root->fs_info->sb->s_blocksize;

	end = min_t(u64, btrfs_file_extent_end(path), sctx->cur_inode_size);
	if (offset >= end)
		return 0;

	if (clone_root && IS_ALIGNED(end, bs)) {
		struct btrfs_file_extent_item *ei;
		u64 disk_byte;
		u64 data_offset;

		ei = btrfs_item_ptr(path->nodes[0], path->slots[0],
				    struct btrfs_file_extent_item);
		disk_byte = btrfs_file_extent_disk_bytenr(path->nodes[0], ei);
		data_offset = btrfs_file_extent_offset(path->nodes[0], ei);
		ret = clone_range(sctx, path, clone_root, disk_byte,
				  data_offset, offset, end - offset);
	} else {
		ret = send_extent_data(sctx, path, offset, end - offset);
	}
	sctx->cur_inode_next_write_offset = end;
	return ret;
}

static int is_extent_unchanged(struct send_ctx *sctx,
			       struct btrfs_path *left_path,
			       struct btrfs_key *ekey)
{
	int ret = 0;
	struct btrfs_key key;
	struct btrfs_path *path = NULL;
	struct extent_buffer *eb;
	int slot;
	struct btrfs_key found_key;
	struct btrfs_file_extent_item *ei;
	u64 left_disknr;
	u64 right_disknr;
	u64 left_offset;
	u64 right_offset;
	u64 left_offset_fixed;
	u64 left_len;
	u64 right_len;
	u64 left_gen;
	u64 right_gen;
	u8 left_type;
	u8 right_type;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	eb = left_path->nodes[0];
	slot = left_path->slots[0];
	ei = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
	left_type = btrfs_file_extent_type(eb, ei);

	if (left_type != BTRFS_FILE_EXTENT_REG) {
		ret = 0;
		goto out;
	}
	left_disknr = btrfs_file_extent_disk_bytenr(eb, ei);
	left_len = btrfs_file_extent_num_bytes(eb, ei);
	left_offset = btrfs_file_extent_offset(eb, ei);
	left_gen = btrfs_file_extent_generation(eb, ei);

	 

	key.objectid = ekey->objectid;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = ekey->offset;
	ret = btrfs_search_slot_for_read(sctx->parent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret) {
		ret = 0;
		goto out;
	}

	 
	eb = path->nodes[0];
	slot = path->slots[0];
	btrfs_item_key_to_cpu(eb, &found_key, slot);
	if (found_key.objectid != key.objectid ||
	    found_key.type != key.type) {
		 
		ret = (left_disknr) ? 0 : 1;
		goto out;
	}

	 
	key = found_key;
	while (key.offset < ekey->offset + left_len) {
		ei = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		right_type = btrfs_file_extent_type(eb, ei);
		if (right_type != BTRFS_FILE_EXTENT_REG &&
		    right_type != BTRFS_FILE_EXTENT_INLINE) {
			ret = 0;
			goto out;
		}

		if (right_type == BTRFS_FILE_EXTENT_INLINE) {
			right_len = btrfs_file_extent_ram_bytes(eb, ei);
			right_len = PAGE_ALIGN(right_len);
		} else {
			right_len = btrfs_file_extent_num_bytes(eb, ei);
		}

		 
		if (found_key.offset + right_len <= ekey->offset) {
			 
			ret = (left_disknr) ? 0 : 1;
			goto out;
		}

		 
		if (right_type == BTRFS_FILE_EXTENT_INLINE) {
			ret = 0;
			goto out;
		}

		right_disknr = btrfs_file_extent_disk_bytenr(eb, ei);
		right_offset = btrfs_file_extent_offset(eb, ei);
		right_gen = btrfs_file_extent_generation(eb, ei);

		left_offset_fixed = left_offset;
		if (key.offset < ekey->offset) {
			 
			right_offset += ekey->offset - key.offset;
		} else {
			 
			left_offset_fixed += key.offset - ekey->offset;
		}

		 
		if (left_disknr != right_disknr ||
		    left_offset_fixed != right_offset ||
		    left_gen != right_gen) {
			ret = 0;
			goto out;
		}

		 
		ret = btrfs_next_item(sctx->parent_root, path);
		if (ret < 0)
			goto out;
		if (!ret) {
			eb = path->nodes[0];
			slot = path->slots[0];
			btrfs_item_key_to_cpu(eb, &found_key, slot);
		}
		if (ret || found_key.objectid != key.objectid ||
		    found_key.type != key.type) {
			key.offset += right_len;
			break;
		}
		if (found_key.offset != key.offset + right_len) {
			ret = 0;
			goto out;
		}
		key = found_key;
	}

	 
	if (key.offset >= ekey->offset + left_len)
		ret = 1;
	else
		ret = 0;


out:
	btrfs_free_path(path);
	return ret;
}

static int get_last_extent(struct send_ctx *sctx, u64 offset)
{
	struct btrfs_path *path;
	struct btrfs_root *root = sctx->send_root;
	struct btrfs_key key;
	int ret;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	sctx->cur_inode_last_extent = 0;

	key.objectid = sctx->cur_ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = offset;
	ret = btrfs_search_slot_for_read(root, &key, path, 0, 1);
	if (ret < 0)
		goto out;
	ret = 0;
	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
	if (key.objectid != sctx->cur_ino || key.type != BTRFS_EXTENT_DATA_KEY)
		goto out;

	sctx->cur_inode_last_extent = btrfs_file_extent_end(path);
out:
	btrfs_free_path(path);
	return ret;
}

static int range_is_hole_in_parent(struct send_ctx *sctx,
				   const u64 start,
				   const u64 end)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_root *root = sctx->parent_root;
	u64 search_start = start;
	int ret;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = sctx->cur_ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = search_start;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret > 0 && path->slots[0] > 0)
		path->slots[0]--;

	while (search_start < end) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		struct btrfs_file_extent_item *fi;
		u64 extent_end;

		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid < sctx->cur_ino ||
		    key.type < BTRFS_EXTENT_DATA_KEY)
			goto next;
		if (key.objectid > sctx->cur_ino ||
		    key.type > BTRFS_EXTENT_DATA_KEY ||
		    key.offset >= end)
			break;

		fi = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);
		extent_end = btrfs_file_extent_end(path);
		if (extent_end <= start)
			goto next;
		if (btrfs_file_extent_disk_bytenr(leaf, fi) == 0) {
			search_start = extent_end;
			goto next;
		}
		ret = 0;
		goto out;
next:
		path->slots[0]++;
	}
	ret = 1;
out:
	btrfs_free_path(path);
	return ret;
}

static int maybe_send_hole(struct send_ctx *sctx, struct btrfs_path *path,
			   struct btrfs_key *key)
{
	int ret = 0;

	if (sctx->cur_ino != key->objectid || !need_send_hole(sctx))
		return 0;

	if (sctx->cur_inode_last_extent == (u64)-1) {
		ret = get_last_extent(sctx, key->offset - 1);
		if (ret)
			return ret;
	}

	if (path->slots[0] == 0 &&
	    sctx->cur_inode_last_extent < key->offset) {
		 
		ret = get_last_extent(sctx, key->offset - 1);
		if (ret)
			return ret;
	}

	if (sctx->cur_inode_last_extent < key->offset) {
		ret = range_is_hole_in_parent(sctx,
					      sctx->cur_inode_last_extent,
					      key->offset);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			ret = send_hole(sctx, key->offset);
		else
			ret = 0;
	}
	sctx->cur_inode_last_extent = btrfs_file_extent_end(path);
	return ret;
}

static int process_extent(struct send_ctx *sctx,
			  struct btrfs_path *path,
			  struct btrfs_key *key)
{
	struct clone_root *found_clone = NULL;
	int ret = 0;

	if (S_ISLNK(sctx->cur_inode_mode))
		return 0;

	if (sctx->parent_root && !sctx->cur_inode_new) {
		ret = is_extent_unchanged(sctx, path, key);
		if (ret < 0)
			goto out;
		if (ret) {
			ret = 0;
			goto out_hole;
		}
	} else {
		struct btrfs_file_extent_item *ei;
		u8 type;

		ei = btrfs_item_ptr(path->nodes[0], path->slots[0],
				    struct btrfs_file_extent_item);
		type = btrfs_file_extent_type(path->nodes[0], ei);
		if (type == BTRFS_FILE_EXTENT_PREALLOC ||
		    type == BTRFS_FILE_EXTENT_REG) {
			 
			if (type == BTRFS_FILE_EXTENT_PREALLOC) {
				ret = 0;
				goto out;
			}

			 
			if (btrfs_file_extent_disk_bytenr(path->nodes[0], ei) == 0) {
				ret = 0;
				goto out;
			}
		}
	}

	ret = find_extent_clone(sctx, path, key->objectid, key->offset,
			sctx->cur_inode_size, &found_clone);
	if (ret != -ENOENT && ret < 0)
		goto out;

	ret = send_write_or_clone(sctx, path, key, found_clone);
	if (ret)
		goto out;
out_hole:
	ret = maybe_send_hole(sctx, path, key);
out:
	return ret;
}

static int process_all_extents(struct send_ctx *sctx)
{
	int ret = 0;
	int iter_ret = 0;
	struct btrfs_root *root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key found_key;

	root = sctx->send_root;
	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;

	key.objectid = sctx->cmp_key->objectid;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = 0;
	btrfs_for_each_slot(root, &key, &found_key, path, iter_ret) {
		if (found_key.objectid != key.objectid ||
		    found_key.type != key.type) {
			ret = 0;
			break;
		}

		ret = process_extent(sctx, path, &found_key);
		if (ret < 0)
			break;
	}
	 
	if (iter_ret < 0)
		ret = iter_ret;

	btrfs_free_path(path);
	return ret;
}

static int process_recorded_refs_if_needed(struct send_ctx *sctx, int at_end,
					   int *pending_move,
					   int *refs_processed)
{
	int ret = 0;

	if (sctx->cur_ino == 0)
		goto out;
	if (!at_end && sctx->cur_ino == sctx->cmp_key->objectid &&
	    sctx->cmp_key->type <= BTRFS_INODE_EXTREF_KEY)
		goto out;
	if (list_empty(&sctx->new_refs) && list_empty(&sctx->deleted_refs))
		goto out;

	ret = process_recorded_refs(sctx, pending_move);
	if (ret < 0)
		goto out;

	*refs_processed = 1;
out:
	return ret;
}

static int finish_inode_if_needed(struct send_ctx *sctx, int at_end)
{
	int ret = 0;
	struct btrfs_inode_info info;
	u64 left_mode;
	u64 left_uid;
	u64 left_gid;
	u64 left_fileattr;
	u64 right_mode;
	u64 right_uid;
	u64 right_gid;
	u64 right_fileattr;
	int need_chmod = 0;
	int need_chown = 0;
	bool need_fileattr = false;
	int need_truncate = 1;
	int pending_move = 0;
	int refs_processed = 0;

	if (sctx->ignore_cur_inode)
		return 0;

	ret = process_recorded_refs_if_needed(sctx, at_end, &pending_move,
					      &refs_processed);
	if (ret < 0)
		goto out;

	 
	if (refs_processed && !pending_move)
		sctx->send_progress = sctx->cur_ino + 1;

	if (sctx->cur_ino == 0 || sctx->cur_inode_deleted)
		goto out;
	if (!at_end && sctx->cmp_key->objectid == sctx->cur_ino)
		goto out;
	ret = get_inode_info(sctx->send_root, sctx->cur_ino, &info);
	if (ret < 0)
		goto out;
	left_mode = info.mode;
	left_uid = info.uid;
	left_gid = info.gid;
	left_fileattr = info.fileattr;

	if (!sctx->parent_root || sctx->cur_inode_new) {
		need_chown = 1;
		if (!S_ISLNK(sctx->cur_inode_mode))
			need_chmod = 1;
		if (sctx->cur_inode_next_write_offset == sctx->cur_inode_size)
			need_truncate = 0;
	} else {
		u64 old_size;

		ret = get_inode_info(sctx->parent_root, sctx->cur_ino, &info);
		if (ret < 0)
			goto out;
		old_size = info.size;
		right_mode = info.mode;
		right_uid = info.uid;
		right_gid = info.gid;
		right_fileattr = info.fileattr;

		if (left_uid != right_uid || left_gid != right_gid)
			need_chown = 1;
		if (!S_ISLNK(sctx->cur_inode_mode) && left_mode != right_mode)
			need_chmod = 1;
		if (!S_ISLNK(sctx->cur_inode_mode) && left_fileattr != right_fileattr)
			need_fileattr = true;
		if ((old_size == sctx->cur_inode_size) ||
		    (sctx->cur_inode_size > old_size &&
		     sctx->cur_inode_next_write_offset == sctx->cur_inode_size))
			need_truncate = 0;
	}

	if (S_ISREG(sctx->cur_inode_mode)) {
		if (need_send_hole(sctx)) {
			if (sctx->cur_inode_last_extent == (u64)-1 ||
			    sctx->cur_inode_last_extent <
			    sctx->cur_inode_size) {
				ret = get_last_extent(sctx, (u64)-1);
				if (ret)
					goto out;
			}
			if (sctx->cur_inode_last_extent <
			    sctx->cur_inode_size) {
				ret = send_hole(sctx, sctx->cur_inode_size);
				if (ret)
					goto out;
			}
		}
		if (need_truncate) {
			ret = send_truncate(sctx, sctx->cur_ino,
					    sctx->cur_inode_gen,
					    sctx->cur_inode_size);
			if (ret < 0)
				goto out;
		}
	}

	if (need_chown) {
		ret = send_chown(sctx, sctx->cur_ino, sctx->cur_inode_gen,
				left_uid, left_gid);
		if (ret < 0)
			goto out;
	}
	if (need_chmod) {
		ret = send_chmod(sctx, sctx->cur_ino, sctx->cur_inode_gen,
				left_mode);
		if (ret < 0)
			goto out;
	}
	if (need_fileattr) {
		ret = send_fileattr(sctx, sctx->cur_ino, sctx->cur_inode_gen,
				    left_fileattr);
		if (ret < 0)
			goto out;
	}

	if (proto_cmd_ok(sctx, BTRFS_SEND_C_ENABLE_VERITY)
	    && sctx->cur_inode_needs_verity) {
		ret = process_verity(sctx);
		if (ret < 0)
			goto out;
	}

	ret = send_capabilities(sctx);
	if (ret < 0)
		goto out;

	 
	if (!is_waiting_for_move(sctx, sctx->cur_ino)) {
		ret = apply_children_dir_moves(sctx);
		if (ret)
			goto out;
		 
		sctx->send_progress = sctx->cur_ino + 1;

		 
		if (S_ISDIR(sctx->cur_inode_mode) && sctx->cur_inode_size > 0)
			ret = cache_dir_utimes(sctx, sctx->cur_ino, sctx->cur_inode_gen);
		else
			ret = send_utimes(sctx, sctx->cur_ino, sctx->cur_inode_gen);

		if (ret < 0)
			goto out;
	}

out:
	if (!ret)
		ret = trim_dir_utimes_cache(sctx);

	return ret;
}

static void close_current_inode(struct send_ctx *sctx)
{
	u64 i_size;

	if (sctx->cur_inode == NULL)
		return;

	i_size = i_size_read(sctx->cur_inode);

	 
	if (sctx->clean_page_cache && sctx->page_cache_clear_start < i_size)
		truncate_inode_pages_range(&sctx->cur_inode->i_data,
					   sctx->page_cache_clear_start,
					   round_up(i_size, PAGE_SIZE) - 1);

	iput(sctx->cur_inode);
	sctx->cur_inode = NULL;
}

static int changed_inode(struct send_ctx *sctx,
			 enum btrfs_compare_tree_result result)
{
	int ret = 0;
	struct btrfs_key *key = sctx->cmp_key;
	struct btrfs_inode_item *left_ii = NULL;
	struct btrfs_inode_item *right_ii = NULL;
	u64 left_gen = 0;
	u64 right_gen = 0;

	close_current_inode(sctx);

	sctx->cur_ino = key->objectid;
	sctx->cur_inode_new_gen = false;
	sctx->cur_inode_last_extent = (u64)-1;
	sctx->cur_inode_next_write_offset = 0;
	sctx->ignore_cur_inode = false;

	 
	sctx->send_progress = sctx->cur_ino;

	if (result == BTRFS_COMPARE_TREE_NEW ||
	    result == BTRFS_COMPARE_TREE_CHANGED) {
		left_ii = btrfs_item_ptr(sctx->left_path->nodes[0],
				sctx->left_path->slots[0],
				struct btrfs_inode_item);
		left_gen = btrfs_inode_generation(sctx->left_path->nodes[0],
				left_ii);
	} else {
		right_ii = btrfs_item_ptr(sctx->right_path->nodes[0],
				sctx->right_path->slots[0],
				struct btrfs_inode_item);
		right_gen = btrfs_inode_generation(sctx->right_path->nodes[0],
				right_ii);
	}
	if (result == BTRFS_COMPARE_TREE_CHANGED) {
		right_ii = btrfs_item_ptr(sctx->right_path->nodes[0],
				sctx->right_path->slots[0],
				struct btrfs_inode_item);

		right_gen = btrfs_inode_generation(sctx->right_path->nodes[0],
				right_ii);

		 
		if (left_gen != right_gen &&
		    sctx->cur_ino != BTRFS_FIRST_FREE_OBJECTID)
			sctx->cur_inode_new_gen = true;
	}

	 
	if (result == BTRFS_COMPARE_TREE_NEW) {
		if (btrfs_inode_nlink(sctx->left_path->nodes[0], left_ii) == 0) {
			sctx->ignore_cur_inode = true;
			goto out;
		}
		sctx->cur_inode_gen = left_gen;
		sctx->cur_inode_new = true;
		sctx->cur_inode_deleted = false;
		sctx->cur_inode_size = btrfs_inode_size(
				sctx->left_path->nodes[0], left_ii);
		sctx->cur_inode_mode = btrfs_inode_mode(
				sctx->left_path->nodes[0], left_ii);
		sctx->cur_inode_rdev = btrfs_inode_rdev(
				sctx->left_path->nodes[0], left_ii);
		if (sctx->cur_ino != BTRFS_FIRST_FREE_OBJECTID)
			ret = send_create_inode_if_needed(sctx);
	} else if (result == BTRFS_COMPARE_TREE_DELETED) {
		sctx->cur_inode_gen = right_gen;
		sctx->cur_inode_new = false;
		sctx->cur_inode_deleted = true;
		sctx->cur_inode_size = btrfs_inode_size(
				sctx->right_path->nodes[0], right_ii);
		sctx->cur_inode_mode = btrfs_inode_mode(
				sctx->right_path->nodes[0], right_ii);
	} else if (result == BTRFS_COMPARE_TREE_CHANGED) {
		u32 new_nlinks, old_nlinks;

		new_nlinks = btrfs_inode_nlink(sctx->left_path->nodes[0], left_ii);
		old_nlinks = btrfs_inode_nlink(sctx->right_path->nodes[0], right_ii);
		if (new_nlinks == 0 && old_nlinks == 0) {
			sctx->ignore_cur_inode = true;
			goto out;
		} else if (new_nlinks == 0 || old_nlinks == 0) {
			sctx->cur_inode_new_gen = 1;
		}
		 
		if (sctx->cur_inode_new_gen) {
			 
			if (old_nlinks > 0) {
				sctx->cur_inode_gen = right_gen;
				sctx->cur_inode_new = false;
				sctx->cur_inode_deleted = true;
				sctx->cur_inode_size = btrfs_inode_size(
						sctx->right_path->nodes[0], right_ii);
				sctx->cur_inode_mode = btrfs_inode_mode(
						sctx->right_path->nodes[0], right_ii);
				ret = process_all_refs(sctx,
						BTRFS_COMPARE_TREE_DELETED);
				if (ret < 0)
					goto out;
			}

			 
			if (new_nlinks > 0) {
				sctx->cur_inode_gen = left_gen;
				sctx->cur_inode_new = true;
				sctx->cur_inode_deleted = false;
				sctx->cur_inode_size = btrfs_inode_size(
						sctx->left_path->nodes[0],
						left_ii);
				sctx->cur_inode_mode = btrfs_inode_mode(
						sctx->left_path->nodes[0],
						left_ii);
				sctx->cur_inode_rdev = btrfs_inode_rdev(
						sctx->left_path->nodes[0],
						left_ii);
				ret = send_create_inode_if_needed(sctx);
				if (ret < 0)
					goto out;

				ret = process_all_refs(sctx, BTRFS_COMPARE_TREE_NEW);
				if (ret < 0)
					goto out;
				 
				sctx->send_progress = sctx->cur_ino + 1;

				 
				ret = process_all_extents(sctx);
				if (ret < 0)
					goto out;
				ret = process_all_new_xattrs(sctx);
				if (ret < 0)
					goto out;
			}
		} else {
			sctx->cur_inode_gen = left_gen;
			sctx->cur_inode_new = false;
			sctx->cur_inode_new_gen = false;
			sctx->cur_inode_deleted = false;
			sctx->cur_inode_size = btrfs_inode_size(
					sctx->left_path->nodes[0], left_ii);
			sctx->cur_inode_mode = btrfs_inode_mode(
					sctx->left_path->nodes[0], left_ii);
		}
	}

out:
	return ret;
}

 
static int changed_ref(struct send_ctx *sctx,
		       enum btrfs_compare_tree_result result)
{
	int ret = 0;

	if (sctx->cur_ino != sctx->cmp_key->objectid) {
		inconsistent_snapshot_error(sctx, result, "reference");
		return -EIO;
	}

	if (!sctx->cur_inode_new_gen &&
	    sctx->cur_ino != BTRFS_FIRST_FREE_OBJECTID) {
		if (result == BTRFS_COMPARE_TREE_NEW)
			ret = record_new_ref(sctx);
		else if (result == BTRFS_COMPARE_TREE_DELETED)
			ret = record_deleted_ref(sctx);
		else if (result == BTRFS_COMPARE_TREE_CHANGED)
			ret = record_changed_ref(sctx);
	}

	return ret;
}

 
static int changed_xattr(struct send_ctx *sctx,
			 enum btrfs_compare_tree_result result)
{
	int ret = 0;

	if (sctx->cur_ino != sctx->cmp_key->objectid) {
		inconsistent_snapshot_error(sctx, result, "xattr");
		return -EIO;
	}

	if (!sctx->cur_inode_new_gen && !sctx->cur_inode_deleted) {
		if (result == BTRFS_COMPARE_TREE_NEW)
			ret = process_new_xattr(sctx);
		else if (result == BTRFS_COMPARE_TREE_DELETED)
			ret = process_deleted_xattr(sctx);
		else if (result == BTRFS_COMPARE_TREE_CHANGED)
			ret = process_changed_xattr(sctx);
	}

	return ret;
}

 
static int changed_extent(struct send_ctx *sctx,
			  enum btrfs_compare_tree_result result)
{
	int ret = 0;

	 
	if (sctx->cur_ino != sctx->cmp_key->objectid)
		return 0;

	if (!sctx->cur_inode_new_gen && !sctx->cur_inode_deleted) {
		if (result != BTRFS_COMPARE_TREE_DELETED)
			ret = process_extent(sctx, sctx->left_path,
					sctx->cmp_key);
	}

	return ret;
}

static int changed_verity(struct send_ctx *sctx, enum btrfs_compare_tree_result result)
{
	int ret = 0;

	if (!sctx->cur_inode_new_gen && !sctx->cur_inode_deleted) {
		if (result == BTRFS_COMPARE_TREE_NEW)
			sctx->cur_inode_needs_verity = true;
	}
	return ret;
}

static int dir_changed(struct send_ctx *sctx, u64 dir)
{
	u64 orig_gen, new_gen;
	int ret;

	ret = get_inode_gen(sctx->send_root, dir, &new_gen);
	if (ret)
		return ret;

	ret = get_inode_gen(sctx->parent_root, dir, &orig_gen);
	if (ret)
		return ret;

	return (orig_gen != new_gen) ? 1 : 0;
}

static int compare_refs(struct send_ctx *sctx, struct btrfs_path *path,
			struct btrfs_key *key)
{
	struct btrfs_inode_extref *extref;
	struct extent_buffer *leaf;
	u64 dirid = 0, last_dirid = 0;
	unsigned long ptr;
	u32 item_size;
	u32 cur_offset = 0;
	int ref_name_len;
	int ret = 0;

	 
	if (key->type == BTRFS_INODE_REF_KEY) {
		dirid = key->offset;

		ret = dir_changed(sctx, dirid);
		goto out;
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size(leaf, path->slots[0]);
	ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
	while (cur_offset < item_size) {
		extref = (struct btrfs_inode_extref *)(ptr +
						       cur_offset);
		dirid = btrfs_inode_extref_parent(leaf, extref);
		ref_name_len = btrfs_inode_extref_name_len(leaf, extref);
		cur_offset += ref_name_len + sizeof(*extref);
		if (dirid == last_dirid)
			continue;
		ret = dir_changed(sctx, dirid);
		if (ret)
			break;
		last_dirid = dirid;
	}
out:
	return ret;
}

 
static int changed_cb(struct btrfs_path *left_path,
		      struct btrfs_path *right_path,
		      struct btrfs_key *key,
		      enum btrfs_compare_tree_result result,
		      struct send_ctx *sctx)
{
	int ret = 0;

	 
	lockdep_assert_not_held(&sctx->send_root->fs_info->commit_root_sem);

	 
	if (left_path->nodes[0])
		ASSERT(test_bit(EXTENT_BUFFER_UNMAPPED,
				&left_path->nodes[0]->bflags));
	 
	if (right_path && right_path->nodes[0])
		ASSERT(test_bit(EXTENT_BUFFER_UNMAPPED,
				&right_path->nodes[0]->bflags));

	if (result == BTRFS_COMPARE_TREE_SAME) {
		if (key->type == BTRFS_INODE_REF_KEY ||
		    key->type == BTRFS_INODE_EXTREF_KEY) {
			ret = compare_refs(sctx, left_path, key);
			if (!ret)
				return 0;
			if (ret < 0)
				return ret;
		} else if (key->type == BTRFS_EXTENT_DATA_KEY) {
			return maybe_send_hole(sctx, left_path, key);
		} else {
			return 0;
		}
		result = BTRFS_COMPARE_TREE_CHANGED;
		ret = 0;
	}

	sctx->left_path = left_path;
	sctx->right_path = right_path;
	sctx->cmp_key = key;

	ret = finish_inode_if_needed(sctx, 0);
	if (ret < 0)
		goto out;

	 
	if (key->objectid == BTRFS_FREE_INO_OBJECTID ||
	    key->objectid == BTRFS_FREE_SPACE_OBJECTID)
		goto out;

	if (key->type == BTRFS_INODE_ITEM_KEY) {
		ret = changed_inode(sctx, result);
	} else if (!sctx->ignore_cur_inode) {
		if (key->type == BTRFS_INODE_REF_KEY ||
		    key->type == BTRFS_INODE_EXTREF_KEY)
			ret = changed_ref(sctx, result);
		else if (key->type == BTRFS_XATTR_ITEM_KEY)
			ret = changed_xattr(sctx, result);
		else if (key->type == BTRFS_EXTENT_DATA_KEY)
			ret = changed_extent(sctx, result);
		else if (key->type == BTRFS_VERITY_DESC_ITEM_KEY &&
			 key->offset == 0)
			ret = changed_verity(sctx, result);
	}

out:
	return ret;
}

static int search_key_again(const struct send_ctx *sctx,
			    struct btrfs_root *root,
			    struct btrfs_path *path,
			    const struct btrfs_key *key)
{
	int ret;

	if (!path->need_commit_sem)
		lockdep_assert_held_read(&root->fs_info->commit_root_sem);

	 
	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	ASSERT(ret <= 0);
	if (ret > 0) {
		btrfs_print_tree(path->nodes[path->lowest_level], false);
		btrfs_err(root->fs_info,
"send: key (%llu %u %llu) not found in %s root %llu, lowest_level %d, slot %d",
			  key->objectid, key->type, key->offset,
			  (root == sctx->parent_root ? "parent" : "send"),
			  root->root_key.objectid, path->lowest_level,
			  path->slots[path->lowest_level]);
		return -EUCLEAN;
	}

	return ret;
}

static int full_send_tree(struct send_ctx *sctx)
{
	int ret;
	struct btrfs_root *send_root = sctx->send_root;
	struct btrfs_key key;
	struct btrfs_fs_info *fs_info = send_root->fs_info;
	struct btrfs_path *path;

	path = alloc_path_for_send();
	if (!path)
		return -ENOMEM;
	path->reada = READA_FORWARD_ALWAYS;

	key.objectid = BTRFS_FIRST_FREE_OBJECTID;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	down_read(&fs_info->commit_root_sem);
	sctx->last_reloc_trans = fs_info->last_reloc_trans;
	up_read(&fs_info->commit_root_sem);

	ret = btrfs_search_slot_for_read(send_root, &key, path, 1, 0);
	if (ret < 0)
		goto out;
	if (ret)
		goto out_finish;

	while (1) {
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

		ret = changed_cb(path, NULL, &key,
				 BTRFS_COMPARE_TREE_NEW, sctx);
		if (ret < 0)
			goto out;

		down_read(&fs_info->commit_root_sem);
		if (fs_info->last_reloc_trans > sctx->last_reloc_trans) {
			sctx->last_reloc_trans = fs_info->last_reloc_trans;
			up_read(&fs_info->commit_root_sem);
			 
			btrfs_release_path(path);
			ret = search_key_again(sctx, send_root, path, &key);
			if (ret < 0)
				goto out;
		} else {
			up_read(&fs_info->commit_root_sem);
		}

		ret = btrfs_next_item(send_root, path);
		if (ret < 0)
			goto out;
		if (ret) {
			ret  = 0;
			break;
		}
	}

out_finish:
	ret = finish_inode_if_needed(sctx, 1);

out:
	btrfs_free_path(path);
	return ret;
}

static int replace_node_with_clone(struct btrfs_path *path, int level)
{
	struct extent_buffer *clone;

	clone = btrfs_clone_extent_buffer(path->nodes[level]);
	if (!clone)
		return -ENOMEM;

	free_extent_buffer(path->nodes[level]);
	path->nodes[level] = clone;

	return 0;
}

static int tree_move_down(struct btrfs_path *path, int *level, u64 reada_min_gen)
{
	struct extent_buffer *eb;
	struct extent_buffer *parent = path->nodes[*level];
	int slot = path->slots[*level];
	const int nritems = btrfs_header_nritems(parent);
	u64 reada_max;
	u64 reada_done = 0;

	lockdep_assert_held_read(&parent->fs_info->commit_root_sem);

	BUG_ON(*level == 0);
	eb = btrfs_read_node_slot(parent, slot);
	if (IS_ERR(eb))
		return PTR_ERR(eb);

	 
	reada_max = (*level == 1 ? SZ_128K : eb->fs_info->nodesize);

	for (slot++; slot < nritems && reada_done < reada_max; slot++) {
		if (btrfs_node_ptr_generation(parent, slot) > reada_min_gen) {
			btrfs_readahead_node_child(parent, slot);
			reada_done += eb->fs_info->nodesize;
		}
	}

	path->nodes[*level - 1] = eb;
	path->slots[*level - 1] = 0;
	(*level)--;

	if (*level == 0)
		return replace_node_with_clone(path, 0);

	return 0;
}

static int tree_move_next_or_upnext(struct btrfs_path *path,
				    int *level, int root_level)
{
	int ret = 0;
	int nritems;
	nritems = btrfs_header_nritems(path->nodes[*level]);

	path->slots[*level]++;

	while (path->slots[*level] >= nritems) {
		if (*level == root_level) {
			path->slots[*level] = nritems - 1;
			return -1;
		}

		 
		path->slots[*level] = 0;
		free_extent_buffer(path->nodes[*level]);
		path->nodes[*level] = NULL;
		(*level)++;
		path->slots[*level]++;

		nritems = btrfs_header_nritems(path->nodes[*level]);
		ret = 1;
	}
	return ret;
}

 
static int tree_advance(struct btrfs_path *path,
			int *level, int root_level,
			int allow_down,
			struct btrfs_key *key,
			u64 reada_min_gen)
{
	int ret;

	if (*level == 0 || !allow_down) {
		ret = tree_move_next_or_upnext(path, level, root_level);
	} else {
		ret = tree_move_down(path, level, reada_min_gen);
	}

	 
	if (*level == 0)
		btrfs_item_key_to_cpu(path->nodes[*level], key,
				      path->slots[*level]);
	else
		btrfs_node_key_to_cpu(path->nodes[*level], key,
				      path->slots[*level]);

	return ret;
}

static int tree_compare_item(struct btrfs_path *left_path,
			     struct btrfs_path *right_path,
			     char *tmp_buf)
{
	int cmp;
	int len1, len2;
	unsigned long off1, off2;

	len1 = btrfs_item_size(left_path->nodes[0], left_path->slots[0]);
	len2 = btrfs_item_size(right_path->nodes[0], right_path->slots[0]);
	if (len1 != len2)
		return 1;

	off1 = btrfs_item_ptr_offset(left_path->nodes[0], left_path->slots[0]);
	off2 = btrfs_item_ptr_offset(right_path->nodes[0],
				right_path->slots[0]);

	read_extent_buffer(left_path->nodes[0], tmp_buf, off1, len1);

	cmp = memcmp_extent_buffer(right_path->nodes[0], tmp_buf, off2, len1);
	if (cmp)
		return 1;
	return 0;
}

 
static int restart_after_relocation(struct btrfs_path *left_path,
				    struct btrfs_path *right_path,
				    const struct btrfs_key *left_key,
				    const struct btrfs_key *right_key,
				    int left_level,
				    int right_level,
				    const struct send_ctx *sctx)
{
	int root_level;
	int ret;

	lockdep_assert_held_read(&sctx->send_root->fs_info->commit_root_sem);

	btrfs_release_path(left_path);
	btrfs_release_path(right_path);

	 
	left_path->lowest_level = left_level;
	ret = search_key_again(sctx, sctx->send_root, left_path, left_key);
	if (ret < 0)
		return ret;

	right_path->lowest_level = right_level;
	ret = search_key_again(sctx, sctx->parent_root, right_path, right_key);
	if (ret < 0)
		return ret;

	 
	if (left_level == 0) {
		ret = replace_node_with_clone(left_path, 0);
		if (ret < 0)
			return ret;
	}

	if (right_level == 0) {
		ret = replace_node_with_clone(right_path, 0);
		if (ret < 0)
			return ret;
	}

	 
	root_level = btrfs_header_level(sctx->send_root->commit_root);
	if (root_level > 0) {
		ret = replace_node_with_clone(left_path, root_level);
		if (ret < 0)
			return ret;
	}

	root_level = btrfs_header_level(sctx->parent_root->commit_root);
	if (root_level > 0) {
		ret = replace_node_with_clone(right_path, root_level);
		if (ret < 0)
			return ret;
	}

	return 0;
}

 
static int btrfs_compare_trees(struct btrfs_root *left_root,
			struct btrfs_root *right_root, struct send_ctx *sctx)
{
	struct btrfs_fs_info *fs_info = left_root->fs_info;
	int ret;
	int cmp;
	struct btrfs_path *left_path = NULL;
	struct btrfs_path *right_path = NULL;
	struct btrfs_key left_key;
	struct btrfs_key right_key;
	char *tmp_buf = NULL;
	int left_root_level;
	int right_root_level;
	int left_level;
	int right_level;
	int left_end_reached = 0;
	int right_end_reached = 0;
	int advance_left = 0;
	int advance_right = 0;
	u64 left_blockptr;
	u64 right_blockptr;
	u64 left_gen;
	u64 right_gen;
	u64 reada_min_gen;

	left_path = btrfs_alloc_path();
	if (!left_path) {
		ret = -ENOMEM;
		goto out;
	}
	right_path = btrfs_alloc_path();
	if (!right_path) {
		ret = -ENOMEM;
		goto out;
	}

	tmp_buf = kvmalloc(fs_info->nodesize, GFP_KERNEL);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto out;
	}

	left_path->search_commit_root = 1;
	left_path->skip_locking = 1;
	right_path->search_commit_root = 1;
	right_path->skip_locking = 1;

	 

	down_read(&fs_info->commit_root_sem);
	left_level = btrfs_header_level(left_root->commit_root);
	left_root_level = left_level;
	 
	left_path->nodes[left_level] =
			btrfs_clone_extent_buffer(left_root->commit_root);
	if (!left_path->nodes[left_level]) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	right_level = btrfs_header_level(right_root->commit_root);
	right_root_level = right_level;
	right_path->nodes[right_level] =
			btrfs_clone_extent_buffer(right_root->commit_root);
	if (!right_path->nodes[right_level]) {
		ret = -ENOMEM;
		goto out_unlock;
	}
	 
	reada_min_gen = btrfs_header_generation(right_root->commit_root);

	if (left_level == 0)
		btrfs_item_key_to_cpu(left_path->nodes[left_level],
				&left_key, left_path->slots[left_level]);
	else
		btrfs_node_key_to_cpu(left_path->nodes[left_level],
				&left_key, left_path->slots[left_level]);
	if (right_level == 0)
		btrfs_item_key_to_cpu(right_path->nodes[right_level],
				&right_key, right_path->slots[right_level]);
	else
		btrfs_node_key_to_cpu(right_path->nodes[right_level],
				&right_key, right_path->slots[right_level]);

	sctx->last_reloc_trans = fs_info->last_reloc_trans;

	while (1) {
		if (need_resched() ||
		    rwsem_is_contended(&fs_info->commit_root_sem)) {
			up_read(&fs_info->commit_root_sem);
			cond_resched();
			down_read(&fs_info->commit_root_sem);
		}

		if (fs_info->last_reloc_trans > sctx->last_reloc_trans) {
			ret = restart_after_relocation(left_path, right_path,
						       &left_key, &right_key,
						       left_level, right_level,
						       sctx);
			if (ret < 0)
				goto out_unlock;
			sctx->last_reloc_trans = fs_info->last_reloc_trans;
		}

		if (advance_left && !left_end_reached) {
			ret = tree_advance(left_path, &left_level,
					left_root_level,
					advance_left != ADVANCE_ONLY_NEXT,
					&left_key, reada_min_gen);
			if (ret == -1)
				left_end_reached = ADVANCE;
			else if (ret < 0)
				goto out_unlock;
			advance_left = 0;
		}
		if (advance_right && !right_end_reached) {
			ret = tree_advance(right_path, &right_level,
					right_root_level,
					advance_right != ADVANCE_ONLY_NEXT,
					&right_key, reada_min_gen);
			if (ret == -1)
				right_end_reached = ADVANCE;
			else if (ret < 0)
				goto out_unlock;
			advance_right = 0;
		}

		if (left_end_reached && right_end_reached) {
			ret = 0;
			goto out_unlock;
		} else if (left_end_reached) {
			if (right_level == 0) {
				up_read(&fs_info->commit_root_sem);
				ret = changed_cb(left_path, right_path,
						&right_key,
						BTRFS_COMPARE_TREE_DELETED,
						sctx);
				if (ret < 0)
					goto out;
				down_read(&fs_info->commit_root_sem);
			}
			advance_right = ADVANCE;
			continue;
		} else if (right_end_reached) {
			if (left_level == 0) {
				up_read(&fs_info->commit_root_sem);
				ret = changed_cb(left_path, right_path,
						&left_key,
						BTRFS_COMPARE_TREE_NEW,
						sctx);
				if (ret < 0)
					goto out;
				down_read(&fs_info->commit_root_sem);
			}
			advance_left = ADVANCE;
			continue;
		}

		if (left_level == 0 && right_level == 0) {
			up_read(&fs_info->commit_root_sem);
			cmp = btrfs_comp_cpu_keys(&left_key, &right_key);
			if (cmp < 0) {
				ret = changed_cb(left_path, right_path,
						&left_key,
						BTRFS_COMPARE_TREE_NEW,
						sctx);
				advance_left = ADVANCE;
			} else if (cmp > 0) {
				ret = changed_cb(left_path, right_path,
						&right_key,
						BTRFS_COMPARE_TREE_DELETED,
						sctx);
				advance_right = ADVANCE;
			} else {
				enum btrfs_compare_tree_result result;

				WARN_ON(!extent_buffer_uptodate(left_path->nodes[0]));
				ret = tree_compare_item(left_path, right_path,
							tmp_buf);
				if (ret)
					result = BTRFS_COMPARE_TREE_CHANGED;
				else
					result = BTRFS_COMPARE_TREE_SAME;
				ret = changed_cb(left_path, right_path,
						 &left_key, result, sctx);
				advance_left = ADVANCE;
				advance_right = ADVANCE;
			}

			if (ret < 0)
				goto out;
			down_read(&fs_info->commit_root_sem);
		} else if (left_level == right_level) {
			cmp = btrfs_comp_cpu_keys(&left_key, &right_key);
			if (cmp < 0) {
				advance_left = ADVANCE;
			} else if (cmp > 0) {
				advance_right = ADVANCE;
			} else {
				left_blockptr = btrfs_node_blockptr(
						left_path->nodes[left_level],
						left_path->slots[left_level]);
				right_blockptr = btrfs_node_blockptr(
						right_path->nodes[right_level],
						right_path->slots[right_level]);
				left_gen = btrfs_node_ptr_generation(
						left_path->nodes[left_level],
						left_path->slots[left_level]);
				right_gen = btrfs_node_ptr_generation(
						right_path->nodes[right_level],
						right_path->slots[right_level]);
				if (left_blockptr == right_blockptr &&
				    left_gen == right_gen) {
					 
					advance_left = ADVANCE_ONLY_NEXT;
					advance_right = ADVANCE_ONLY_NEXT;
				} else {
					advance_left = ADVANCE;
					advance_right = ADVANCE;
				}
			}
		} else if (left_level < right_level) {
			advance_right = ADVANCE;
		} else {
			advance_left = ADVANCE;
		}
	}

out_unlock:
	up_read(&fs_info->commit_root_sem);
out:
	btrfs_free_path(left_path);
	btrfs_free_path(right_path);
	kvfree(tmp_buf);
	return ret;
}

static int send_subvol(struct send_ctx *sctx)
{
	int ret;

	if (!(sctx->flags & BTRFS_SEND_FLAG_OMIT_STREAM_HEADER)) {
		ret = send_header(sctx);
		if (ret < 0)
			goto out;
	}

	ret = send_subvol_begin(sctx);
	if (ret < 0)
		goto out;

	if (sctx->parent_root) {
		ret = btrfs_compare_trees(sctx->send_root, sctx->parent_root, sctx);
		if (ret < 0)
			goto out;
		ret = finish_inode_if_needed(sctx, 1);
		if (ret < 0)
			goto out;
	} else {
		ret = full_send_tree(sctx);
		if (ret < 0)
			goto out;
	}

out:
	free_recorded_refs(sctx);
	return ret;
}

 
static int ensure_commit_roots_uptodate(struct send_ctx *sctx)
{
	int i;
	struct btrfs_trans_handle *trans = NULL;

again:
	if (sctx->parent_root &&
	    sctx->parent_root->node != sctx->parent_root->commit_root)
		goto commit_trans;

	for (i = 0; i < sctx->clone_roots_cnt; i++)
		if (sctx->clone_roots[i].root->node !=
		    sctx->clone_roots[i].root->commit_root)
			goto commit_trans;

	if (trans)
		return btrfs_end_transaction(trans);

	return 0;

commit_trans:
	 
	if (!trans) {
		trans = btrfs_join_transaction(sctx->send_root);
		if (IS_ERR(trans))
			return PTR_ERR(trans);
		goto again;
	}

	return btrfs_commit_transaction(trans);
}

 
static int flush_delalloc_roots(struct send_ctx *sctx)
{
	struct btrfs_root *root = sctx->parent_root;
	int ret;
	int i;

	if (root) {
		ret = btrfs_start_delalloc_snapshot(root, false);
		if (ret)
			return ret;
		btrfs_wait_ordered_extents(root, U64_MAX, 0, U64_MAX);
	}

	for (i = 0; i < sctx->clone_roots_cnt; i++) {
		root = sctx->clone_roots[i].root;
		ret = btrfs_start_delalloc_snapshot(root, false);
		if (ret)
			return ret;
		btrfs_wait_ordered_extents(root, U64_MAX, 0, U64_MAX);
	}

	return 0;
}

static void btrfs_root_dec_send_in_progress(struct btrfs_root* root)
{
	spin_lock(&root->root_item_lock);
	root->send_in_progress--;
	 
	if (root->send_in_progress < 0)
		btrfs_err(root->fs_info,
			  "send_in_progress unbalanced %d root %llu",
			  root->send_in_progress, root->root_key.objectid);
	spin_unlock(&root->root_item_lock);
}

static void dedupe_in_progress_warn(const struct btrfs_root *root)
{
	btrfs_warn_rl(root->fs_info,
"cannot use root %llu for send while deduplications on it are in progress (%d in progress)",
		      root->root_key.objectid, root->dedupe_in_progress);
}

long btrfs_ioctl_send(struct inode *inode, struct btrfs_ioctl_send_args *arg)
{
	int ret = 0;
	struct btrfs_root *send_root = BTRFS_I(inode)->root;
	struct btrfs_fs_info *fs_info = send_root->fs_info;
	struct btrfs_root *clone_root;
	struct send_ctx *sctx = NULL;
	u32 i;
	u64 *clone_sources_tmp = NULL;
	int clone_sources_to_rollback = 0;
	size_t alloc_size;
	int sort_clone_roots = 0;
	struct btrfs_lru_cache_entry *entry;
	struct btrfs_lru_cache_entry *tmp;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	 
	spin_lock(&send_root->root_item_lock);
	if (btrfs_root_readonly(send_root) && send_root->dedupe_in_progress) {
		dedupe_in_progress_warn(send_root);
		spin_unlock(&send_root->root_item_lock);
		return -EAGAIN;
	}
	send_root->send_in_progress++;
	spin_unlock(&send_root->root_item_lock);

	 
	if (!btrfs_root_readonly(send_root)) {
		ret = -EPERM;
		goto out;
	}

	 
	if (arg->clone_sources_count > SZ_8M / sizeof(struct clone_root)) {
		ret = -EINVAL;
		goto out;
	}

	if (arg->flags & ~BTRFS_SEND_FLAG_MASK) {
		ret = -EINVAL;
		goto out;
	}

	sctx = kzalloc(sizeof(struct send_ctx), GFP_KERNEL);
	if (!sctx) {
		ret = -ENOMEM;
		goto out;
	}

	INIT_LIST_HEAD(&sctx->new_refs);
	INIT_LIST_HEAD(&sctx->deleted_refs);

	btrfs_lru_cache_init(&sctx->name_cache, SEND_MAX_NAME_CACHE_SIZE);
	btrfs_lru_cache_init(&sctx->backref_cache, SEND_MAX_BACKREF_CACHE_SIZE);
	btrfs_lru_cache_init(&sctx->dir_created_cache,
			     SEND_MAX_DIR_CREATED_CACHE_SIZE);
	 
	btrfs_lru_cache_init(&sctx->dir_utimes_cache, 0);

	sctx->pending_dir_moves = RB_ROOT;
	sctx->waiting_dir_moves = RB_ROOT;
	sctx->orphan_dirs = RB_ROOT;
	sctx->rbtree_new_refs = RB_ROOT;
	sctx->rbtree_deleted_refs = RB_ROOT;

	sctx->flags = arg->flags;

	if (arg->flags & BTRFS_SEND_FLAG_VERSION) {
		if (arg->version > BTRFS_SEND_STREAM_VERSION) {
			ret = -EPROTO;
			goto out;
		}
		 
		sctx->proto = arg->version ?: BTRFS_SEND_STREAM_VERSION;
	} else {
		sctx->proto = 1;
	}
	if ((arg->flags & BTRFS_SEND_FLAG_COMPRESSED) && sctx->proto < 2) {
		ret = -EINVAL;
		goto out;
	}

	sctx->send_filp = fget(arg->send_fd);
	if (!sctx->send_filp || !(sctx->send_filp->f_mode & FMODE_WRITE)) {
		ret = -EBADF;
		goto out;
	}

	sctx->send_root = send_root;
	 
	if (btrfs_root_dead(sctx->send_root)) {
		ret = -EPERM;
		goto out;
	}

	sctx->clone_roots_cnt = arg->clone_sources_count;

	if (sctx->proto >= 2) {
		u32 send_buf_num_pages;

		sctx->send_max_size = BTRFS_SEND_BUF_SIZE_V2;
		sctx->send_buf = vmalloc(sctx->send_max_size);
		if (!sctx->send_buf) {
			ret = -ENOMEM;
			goto out;
		}
		send_buf_num_pages = sctx->send_max_size >> PAGE_SHIFT;
		sctx->send_buf_pages = kcalloc(send_buf_num_pages,
					       sizeof(*sctx->send_buf_pages),
					       GFP_KERNEL);
		if (!sctx->send_buf_pages) {
			ret = -ENOMEM;
			goto out;
		}
		for (i = 0; i < send_buf_num_pages; i++) {
			sctx->send_buf_pages[i] =
				vmalloc_to_page(sctx->send_buf + (i << PAGE_SHIFT));
		}
	} else {
		sctx->send_max_size = BTRFS_SEND_BUF_SIZE_V1;
		sctx->send_buf = kvmalloc(sctx->send_max_size, GFP_KERNEL);
	}
	if (!sctx->send_buf) {
		ret = -ENOMEM;
		goto out;
	}

	sctx->clone_roots = kvcalloc(sizeof(*sctx->clone_roots),
				     arg->clone_sources_count + 1,
				     GFP_KERNEL);
	if (!sctx->clone_roots) {
		ret = -ENOMEM;
		goto out;
	}

	alloc_size = array_size(sizeof(*arg->clone_sources),
				arg->clone_sources_count);

	if (arg->clone_sources_count) {
		clone_sources_tmp = kvmalloc(alloc_size, GFP_KERNEL);
		if (!clone_sources_tmp) {
			ret = -ENOMEM;
			goto out;
		}

		ret = copy_from_user(clone_sources_tmp, arg->clone_sources,
				alloc_size);
		if (ret) {
			ret = -EFAULT;
			goto out;
		}

		for (i = 0; i < arg->clone_sources_count; i++) {
			clone_root = btrfs_get_fs_root(fs_info,
						clone_sources_tmp[i], true);
			if (IS_ERR(clone_root)) {
				ret = PTR_ERR(clone_root);
				goto out;
			}
			spin_lock(&clone_root->root_item_lock);
			if (!btrfs_root_readonly(clone_root) ||
			    btrfs_root_dead(clone_root)) {
				spin_unlock(&clone_root->root_item_lock);
				btrfs_put_root(clone_root);
				ret = -EPERM;
				goto out;
			}
			if (clone_root->dedupe_in_progress) {
				dedupe_in_progress_warn(clone_root);
				spin_unlock(&clone_root->root_item_lock);
				btrfs_put_root(clone_root);
				ret = -EAGAIN;
				goto out;
			}
			clone_root->send_in_progress++;
			spin_unlock(&clone_root->root_item_lock);

			sctx->clone_roots[i].root = clone_root;
			clone_sources_to_rollback = i + 1;
		}
		kvfree(clone_sources_tmp);
		clone_sources_tmp = NULL;
	}

	if (arg->parent_root) {
		sctx->parent_root = btrfs_get_fs_root(fs_info, arg->parent_root,
						      true);
		if (IS_ERR(sctx->parent_root)) {
			ret = PTR_ERR(sctx->parent_root);
			goto out;
		}

		spin_lock(&sctx->parent_root->root_item_lock);
		sctx->parent_root->send_in_progress++;
		if (!btrfs_root_readonly(sctx->parent_root) ||
				btrfs_root_dead(sctx->parent_root)) {
			spin_unlock(&sctx->parent_root->root_item_lock);
			ret = -EPERM;
			goto out;
		}
		if (sctx->parent_root->dedupe_in_progress) {
			dedupe_in_progress_warn(sctx->parent_root);
			spin_unlock(&sctx->parent_root->root_item_lock);
			ret = -EAGAIN;
			goto out;
		}
		spin_unlock(&sctx->parent_root->root_item_lock);
	}

	 
	sctx->clone_roots[sctx->clone_roots_cnt++].root =
		btrfs_grab_root(sctx->send_root);

	 
	sort(sctx->clone_roots, sctx->clone_roots_cnt,
			sizeof(*sctx->clone_roots), __clone_root_cmp_sort,
			NULL);
	sort_clone_roots = 1;

	ret = flush_delalloc_roots(sctx);
	if (ret)
		goto out;

	ret = ensure_commit_roots_uptodate(sctx);
	if (ret)
		goto out;

	ret = send_subvol(sctx);
	if (ret < 0)
		goto out;

	btrfs_lru_cache_for_each_entry_safe(&sctx->dir_utimes_cache, entry, tmp) {
		ret = send_utimes(sctx, entry->key, entry->gen);
		if (ret < 0)
			goto out;
		btrfs_lru_cache_remove(&sctx->dir_utimes_cache, entry);
	}

	if (!(sctx->flags & BTRFS_SEND_FLAG_OMIT_END_CMD)) {
		ret = begin_cmd(sctx, BTRFS_SEND_C_END);
		if (ret < 0)
			goto out;
		ret = send_cmd(sctx);
		if (ret < 0)
			goto out;
	}

out:
	WARN_ON(sctx && !ret && !RB_EMPTY_ROOT(&sctx->pending_dir_moves));
	while (sctx && !RB_EMPTY_ROOT(&sctx->pending_dir_moves)) {
		struct rb_node *n;
		struct pending_dir_move *pm;

		n = rb_first(&sctx->pending_dir_moves);
		pm = rb_entry(n, struct pending_dir_move, node);
		while (!list_empty(&pm->list)) {
			struct pending_dir_move *pm2;

			pm2 = list_first_entry(&pm->list,
					       struct pending_dir_move, list);
			free_pending_move(sctx, pm2);
		}
		free_pending_move(sctx, pm);
	}

	WARN_ON(sctx && !ret && !RB_EMPTY_ROOT(&sctx->waiting_dir_moves));
	while (sctx && !RB_EMPTY_ROOT(&sctx->waiting_dir_moves)) {
		struct rb_node *n;
		struct waiting_dir_move *dm;

		n = rb_first(&sctx->waiting_dir_moves);
		dm = rb_entry(n, struct waiting_dir_move, node);
		rb_erase(&dm->node, &sctx->waiting_dir_moves);
		kfree(dm);
	}

	WARN_ON(sctx && !ret && !RB_EMPTY_ROOT(&sctx->orphan_dirs));
	while (sctx && !RB_EMPTY_ROOT(&sctx->orphan_dirs)) {
		struct rb_node *n;
		struct orphan_dir_info *odi;

		n = rb_first(&sctx->orphan_dirs);
		odi = rb_entry(n, struct orphan_dir_info, node);
		free_orphan_dir_info(sctx, odi);
	}

	if (sort_clone_roots) {
		for (i = 0; i < sctx->clone_roots_cnt; i++) {
			btrfs_root_dec_send_in_progress(
					sctx->clone_roots[i].root);
			btrfs_put_root(sctx->clone_roots[i].root);
		}
	} else {
		for (i = 0; sctx && i < clone_sources_to_rollback; i++) {
			btrfs_root_dec_send_in_progress(
					sctx->clone_roots[i].root);
			btrfs_put_root(sctx->clone_roots[i].root);
		}

		btrfs_root_dec_send_in_progress(send_root);
	}
	if (sctx && !IS_ERR_OR_NULL(sctx->parent_root)) {
		btrfs_root_dec_send_in_progress(sctx->parent_root);
		btrfs_put_root(sctx->parent_root);
	}

	kvfree(clone_sources_tmp);

	if (sctx) {
		if (sctx->send_filp)
			fput(sctx->send_filp);

		kvfree(sctx->clone_roots);
		kfree(sctx->send_buf_pages);
		kvfree(sctx->send_buf);
		kvfree(sctx->verity_descriptor);

		close_current_inode(sctx);

		btrfs_lru_cache_clear(&sctx->name_cache);
		btrfs_lru_cache_clear(&sctx->backref_cache);
		btrfs_lru_cache_clear(&sctx->dir_created_cache);
		btrfs_lru_cache_clear(&sctx->dir_utimes_cache);

		kfree(sctx);
	}

	return ret;
}
