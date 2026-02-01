
 

#include <linux/compat.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/swap.h>
#include <linux/sched.h>
#include <linux/kmemleak.h>
#include <linux/xattr.h>
#include <linux/hash.h>

#include "delegation.h"
#include "iostat.h"
#include "internal.h"
#include "fscache.h"

#include "nfstrace.h"

 

static int nfs_opendir(struct inode *, struct file *);
static int nfs_closedir(struct inode *, struct file *);
static int nfs_readdir(struct file *, struct dir_context *);
static int nfs_fsync_dir(struct file *, loff_t, loff_t, int);
static loff_t nfs_llseek_dir(struct file *, loff_t, int);
static void nfs_readdir_clear_array(struct folio *);

const struct file_operations nfs_dir_operations = {
	.llseek		= nfs_llseek_dir,
	.read		= generic_read_dir,
	.iterate_shared	= nfs_readdir,
	.open		= nfs_opendir,
	.release	= nfs_closedir,
	.fsync		= nfs_fsync_dir,
};

const struct address_space_operations nfs_dir_aops = {
	.free_folio = nfs_readdir_clear_array,
};

#define NFS_INIT_DTSIZE PAGE_SIZE

static struct nfs_open_dir_context *
alloc_nfs_open_dir_context(struct inode *dir)
{
	struct nfs_inode *nfsi = NFS_I(dir);
	struct nfs_open_dir_context *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL_ACCOUNT);
	if (ctx != NULL) {
		ctx->attr_gencount = nfsi->attr_gencount;
		ctx->dtsize = NFS_INIT_DTSIZE;
		spin_lock(&dir->i_lock);
		if (list_empty(&nfsi->open_files) &&
		    (nfsi->cache_validity & NFS_INO_DATA_INVAL_DEFER))
			nfs_set_cache_invalid(dir,
					      NFS_INO_INVALID_DATA |
						      NFS_INO_REVAL_FORCED);
		list_add_tail_rcu(&ctx->list, &nfsi->open_files);
		memcpy(ctx->verf, nfsi->cookieverf, sizeof(ctx->verf));
		spin_unlock(&dir->i_lock);
		return ctx;
	}
	return  ERR_PTR(-ENOMEM);
}

static void put_nfs_open_dir_context(struct inode *dir, struct nfs_open_dir_context *ctx)
{
	spin_lock(&dir->i_lock);
	list_del_rcu(&ctx->list);
	spin_unlock(&dir->i_lock);
	kfree_rcu(ctx, rcu_head);
}

 
static int
nfs_opendir(struct inode *inode, struct file *filp)
{
	int res = 0;
	struct nfs_open_dir_context *ctx;

	dfprintk(FILE, "NFS: open dir(%pD2)\n", filp);

	nfs_inc_stats(inode, NFSIOS_VFSOPEN);

	ctx = alloc_nfs_open_dir_context(inode);
	if (IS_ERR(ctx)) {
		res = PTR_ERR(ctx);
		goto out;
	}
	filp->private_data = ctx;
out:
	return res;
}

static int
nfs_closedir(struct inode *inode, struct file *filp)
{
	put_nfs_open_dir_context(file_inode(filp), filp->private_data);
	return 0;
}

struct nfs_cache_array_entry {
	u64 cookie;
	u64 ino;
	const char *name;
	unsigned int name_len;
	unsigned char d_type;
};

struct nfs_cache_array {
	u64 change_attr;
	u64 last_cookie;
	unsigned int size;
	unsigned char folio_full : 1,
		      folio_is_eof : 1,
		      cookies_are_ordered : 1;
	struct nfs_cache_array_entry array[];
};

struct nfs_readdir_descriptor {
	struct file	*file;
	struct folio	*folio;
	struct dir_context *ctx;
	pgoff_t		folio_index;
	pgoff_t		folio_index_max;
	u64		dir_cookie;
	u64		last_cookie;
	loff_t		current_index;

	__be32		verf[NFS_DIR_VERIFIER_SIZE];
	unsigned long	dir_verifier;
	unsigned long	timestamp;
	unsigned long	gencount;
	unsigned long	attr_gencount;
	unsigned int	cache_entry_index;
	unsigned int	buffer_fills;
	unsigned int	dtsize;
	bool clear_cache;
	bool plus;
	bool eob;
	bool eof;
};

static void nfs_set_dtsize(struct nfs_readdir_descriptor *desc, unsigned int sz)
{
	struct nfs_server *server = NFS_SERVER(file_inode(desc->file));
	unsigned int maxsize = server->dtsize;

	if (sz > maxsize)
		sz = maxsize;
	if (sz < NFS_MIN_FILE_IO_SIZE)
		sz = NFS_MIN_FILE_IO_SIZE;
	desc->dtsize = sz;
}

static void nfs_shrink_dtsize(struct nfs_readdir_descriptor *desc)
{
	nfs_set_dtsize(desc, desc->dtsize >> 1);
}

static void nfs_grow_dtsize(struct nfs_readdir_descriptor *desc)
{
	nfs_set_dtsize(desc, desc->dtsize << 1);
}

static void nfs_readdir_folio_init_array(struct folio *folio, u64 last_cookie,
					 u64 change_attr)
{
	struct nfs_cache_array *array;

	array = kmap_local_folio(folio, 0);
	array->change_attr = change_attr;
	array->last_cookie = last_cookie;
	array->size = 0;
	array->folio_full = 0;
	array->folio_is_eof = 0;
	array->cookies_are_ordered = 1;
	kunmap_local(array);
}

 
static void nfs_readdir_clear_array(struct folio *folio)
{
	struct nfs_cache_array *array;
	unsigned int i;

	array = kmap_local_folio(folio, 0);
	for (i = 0; i < array->size; i++)
		kfree(array->array[i].name);
	array->size = 0;
	kunmap_local(array);
}

static void nfs_readdir_folio_reinit_array(struct folio *folio, u64 last_cookie,
					   u64 change_attr)
{
	nfs_readdir_clear_array(folio);
	nfs_readdir_folio_init_array(folio, last_cookie, change_attr);
}

static struct folio *
nfs_readdir_folio_array_alloc(u64 last_cookie, gfp_t gfp_flags)
{
	struct folio *folio = folio_alloc(gfp_flags, 0);
	if (folio)
		nfs_readdir_folio_init_array(folio, last_cookie, 0);
	return folio;
}

static void nfs_readdir_folio_array_free(struct folio *folio)
{
	if (folio) {
		nfs_readdir_clear_array(folio);
		folio_put(folio);
	}
}

static u64 nfs_readdir_array_index_cookie(struct nfs_cache_array *array)
{
	return array->size == 0 ? array->last_cookie : array->array[0].cookie;
}

static void nfs_readdir_array_set_eof(struct nfs_cache_array *array)
{
	array->folio_is_eof = 1;
	array->folio_full = 1;
}

static bool nfs_readdir_array_is_full(struct nfs_cache_array *array)
{
	return array->folio_full;
}

 
static const char *nfs_readdir_copy_name(const char *name, unsigned int len)
{
	const char *ret = kmemdup_nul(name, len, GFP_KERNEL);

	 
	if (ret != NULL)
		kmemleak_not_leak(ret);
	return ret;
}

static size_t nfs_readdir_array_maxentries(void)
{
	return (PAGE_SIZE - sizeof(struct nfs_cache_array)) /
	       sizeof(struct nfs_cache_array_entry);
}

 
static int nfs_readdir_array_can_expand(struct nfs_cache_array *array)
{
	if (array->folio_full)
		return -ENOSPC;
	if (array->size == nfs_readdir_array_maxentries()) {
		array->folio_full = 1;
		return -ENOSPC;
	}
	return 0;
}

static int nfs_readdir_folio_array_append(struct folio *folio,
					  const struct nfs_entry *entry,
					  u64 *cookie)
{
	struct nfs_cache_array *array;
	struct nfs_cache_array_entry *cache_entry;
	const char *name;
	int ret = -ENOMEM;

	name = nfs_readdir_copy_name(entry->name, entry->len);

	array = kmap_local_folio(folio, 0);
	if (!name)
		goto out;
	ret = nfs_readdir_array_can_expand(array);
	if (ret) {
		kfree(name);
		goto out;
	}

	cache_entry = &array->array[array->size];
	cache_entry->cookie = array->last_cookie;
	cache_entry->ino = entry->ino;
	cache_entry->d_type = entry->d_type;
	cache_entry->name_len = entry->len;
	cache_entry->name = name;
	array->last_cookie = entry->cookie;
	if (array->last_cookie <= cache_entry->cookie)
		array->cookies_are_ordered = 0;
	array->size++;
	if (entry->eof != 0)
		nfs_readdir_array_set_eof(array);
out:
	*cookie = array->last_cookie;
	kunmap_local(array);
	return ret;
}

#define NFS_READDIR_COOKIE_MASK (U32_MAX >> 14)
 
static pgoff_t nfs_readdir_folio_cookie_hash(u64 cookie)
{
	if (cookie == 0)
		return 0;
	return hash_64(cookie, 18);
}

static bool nfs_readdir_folio_validate(struct folio *folio, u64 last_cookie,
				       u64 change_attr)
{
	struct nfs_cache_array *array = kmap_local_folio(folio, 0);
	int ret = true;

	if (array->change_attr != change_attr)
		ret = false;
	if (nfs_readdir_array_index_cookie(array) != last_cookie)
		ret = false;
	kunmap_local(array);
	return ret;
}

static void nfs_readdir_folio_unlock_and_put(struct folio *folio)
{
	folio_unlock(folio);
	folio_put(folio);
}

static void nfs_readdir_folio_init_and_validate(struct folio *folio, u64 cookie,
						u64 change_attr)
{
	if (folio_test_uptodate(folio)) {
		if (nfs_readdir_folio_validate(folio, cookie, change_attr))
			return;
		nfs_readdir_clear_array(folio);
	}
	nfs_readdir_folio_init_array(folio, cookie, change_attr);
	folio_mark_uptodate(folio);
}

static struct folio *nfs_readdir_folio_get_locked(struct address_space *mapping,
						  u64 cookie, u64 change_attr)
{
	pgoff_t index = nfs_readdir_folio_cookie_hash(cookie);
	struct folio *folio;

	folio = filemap_grab_folio(mapping, index);
	if (IS_ERR(folio))
		return NULL;
	nfs_readdir_folio_init_and_validate(folio, cookie, change_attr);
	return folio;
}

static u64 nfs_readdir_folio_last_cookie(struct folio *folio)
{
	struct nfs_cache_array *array;
	u64 ret;

	array = kmap_local_folio(folio, 0);
	ret = array->last_cookie;
	kunmap_local(array);
	return ret;
}

static bool nfs_readdir_folio_needs_filling(struct folio *folio)
{
	struct nfs_cache_array *array;
	bool ret;

	array = kmap_local_folio(folio, 0);
	ret = !nfs_readdir_array_is_full(array);
	kunmap_local(array);
	return ret;
}

static void nfs_readdir_folio_set_eof(struct folio *folio)
{
	struct nfs_cache_array *array;

	array = kmap_local_folio(folio, 0);
	nfs_readdir_array_set_eof(array);
	kunmap_local(array);
}

static struct folio *nfs_readdir_folio_get_next(struct address_space *mapping,
						u64 cookie, u64 change_attr)
{
	pgoff_t index = nfs_readdir_folio_cookie_hash(cookie);
	struct folio *folio;

	folio = __filemap_get_folio(mapping, index,
			FGP_LOCK|FGP_CREAT|FGP_NOFS|FGP_NOWAIT,
			mapping_gfp_mask(mapping));
	if (IS_ERR(folio))
		return NULL;
	nfs_readdir_folio_init_and_validate(folio, cookie, change_attr);
	if (nfs_readdir_folio_last_cookie(folio) != cookie)
		nfs_readdir_folio_reinit_array(folio, cookie, change_attr);
	return folio;
}

static inline
int is_32bit_api(void)
{
#ifdef CONFIG_COMPAT
	return in_compat_syscall();
#else
	return (BITS_PER_LONG == 32);
#endif
}

static
bool nfs_readdir_use_cookie(const struct file *filp)
{
	if ((filp->f_mode & FMODE_32BITHASH) ||
	    (!(filp->f_mode & FMODE_64BITHASH) && is_32bit_api()))
		return false;
	return true;
}

static void nfs_readdir_seek_next_array(struct nfs_cache_array *array,
					struct nfs_readdir_descriptor *desc)
{
	if (array->folio_full) {
		desc->last_cookie = array->last_cookie;
		desc->current_index += array->size;
		desc->cache_entry_index = 0;
		desc->folio_index++;
	} else
		desc->last_cookie = nfs_readdir_array_index_cookie(array);
}

static void nfs_readdir_rewind_search(struct nfs_readdir_descriptor *desc)
{
	desc->current_index = 0;
	desc->last_cookie = 0;
	desc->folio_index = 0;
}

static int nfs_readdir_search_for_pos(struct nfs_cache_array *array,
				      struct nfs_readdir_descriptor *desc)
{
	loff_t diff = desc->ctx->pos - desc->current_index;
	unsigned int index;

	if (diff < 0)
		goto out_eof;
	if (diff >= array->size) {
		if (array->folio_is_eof)
			goto out_eof;
		nfs_readdir_seek_next_array(array, desc);
		return -EAGAIN;
	}

	index = (unsigned int)diff;
	desc->dir_cookie = array->array[index].cookie;
	desc->cache_entry_index = index;
	return 0;
out_eof:
	desc->eof = true;
	return -EBADCOOKIE;
}

static bool nfs_readdir_array_cookie_in_range(struct nfs_cache_array *array,
					      u64 cookie)
{
	if (!array->cookies_are_ordered)
		return true;
	 
	if (cookie >= array->last_cookie)
		return false;
	if (array->size && cookie < array->array[0].cookie)
		return false;
	return true;
}

static int nfs_readdir_search_for_cookie(struct nfs_cache_array *array,
					 struct nfs_readdir_descriptor *desc)
{
	unsigned int i;
	int status = -EAGAIN;

	if (!nfs_readdir_array_cookie_in_range(array, desc->dir_cookie))
		goto check_eof;

	for (i = 0; i < array->size; i++) {
		if (array->array[i].cookie == desc->dir_cookie) {
			if (nfs_readdir_use_cookie(desc->file))
				desc->ctx->pos = desc->dir_cookie;
			else
				desc->ctx->pos = desc->current_index + i;
			desc->cache_entry_index = i;
			return 0;
		}
	}
check_eof:
	if (array->folio_is_eof) {
		status = -EBADCOOKIE;
		if (desc->dir_cookie == array->last_cookie)
			desc->eof = true;
	} else
		nfs_readdir_seek_next_array(array, desc);
	return status;
}

static int nfs_readdir_search_array(struct nfs_readdir_descriptor *desc)
{
	struct nfs_cache_array *array;
	int status;

	array = kmap_local_folio(desc->folio, 0);

	if (desc->dir_cookie == 0)
		status = nfs_readdir_search_for_pos(array, desc);
	else
		status = nfs_readdir_search_for_cookie(array, desc);

	kunmap_local(array);
	return status;
}

 
static int nfs_readdir_xdr_filler(struct nfs_readdir_descriptor *desc,
				  __be32 *verf, u64 cookie,
				  struct page **pages, size_t bufsize,
				  __be32 *verf_res)
{
	struct inode *inode = file_inode(desc->file);
	struct nfs_readdir_arg arg = {
		.dentry = file_dentry(desc->file),
		.cred = desc->file->f_cred,
		.verf = verf,
		.cookie = cookie,
		.pages = pages,
		.page_len = bufsize,
		.plus = desc->plus,
	};
	struct nfs_readdir_res res = {
		.verf = verf_res,
	};
	unsigned long	timestamp, gencount;
	int		error;

 again:
	timestamp = jiffies;
	gencount = nfs_inc_attr_generation_counter();
	desc->dir_verifier = nfs_save_change_attribute(inode);
	error = NFS_PROTO(inode)->readdir(&arg, &res);
	if (error < 0) {
		 
		if (error == -ENOTSUPP && desc->plus) {
			NFS_SERVER(inode)->caps &= ~NFS_CAP_READDIRPLUS;
			desc->plus = arg.plus = false;
			goto again;
		}
		goto error;
	}
	desc->timestamp = timestamp;
	desc->gencount = gencount;
error:
	return error;
}

static int xdr_decode(struct nfs_readdir_descriptor *desc,
		      struct nfs_entry *entry, struct xdr_stream *xdr)
{
	struct inode *inode = file_inode(desc->file);
	int error;

	error = NFS_PROTO(inode)->decode_dirent(xdr, entry, desc->plus);
	if (error)
		return error;
	entry->fattr->time_start = desc->timestamp;
	entry->fattr->gencount = desc->gencount;
	return 0;
}

 
static
int nfs_same_file(struct dentry *dentry, struct nfs_entry *entry)
{
	struct inode *inode;
	struct nfs_inode *nfsi;

	if (d_really_is_negative(dentry))
		return 0;

	inode = d_inode(dentry);
	if (is_bad_inode(inode) || NFS_STALE(inode))
		return 0;

	nfsi = NFS_I(inode);
	if (entry->fattr->fileid != nfsi->fileid)
		return 0;
	if (entry->fh->size && nfs_compare_fh(entry->fh, &nfsi->fh) != 0)
		return 0;
	return 1;
}

#define NFS_READDIR_CACHE_USAGE_THRESHOLD (8UL)

static bool nfs_use_readdirplus(struct inode *dir, struct dir_context *ctx,
				unsigned int cache_hits,
				unsigned int cache_misses)
{
	if (!nfs_server_capable(dir, NFS_CAP_READDIRPLUS))
		return false;
	if (ctx->pos == 0 ||
	    cache_hits + cache_misses > NFS_READDIR_CACHE_USAGE_THRESHOLD)
		return true;
	return false;
}

 
void nfs_readdir_record_entry_cache_hit(struct inode *dir)
{
	struct nfs_inode *nfsi = NFS_I(dir);
	struct nfs_open_dir_context *ctx;

	if (nfs_server_capable(dir, NFS_CAP_READDIRPLUS) &&
	    S_ISDIR(dir->i_mode)) {
		rcu_read_lock();
		list_for_each_entry_rcu (ctx, &nfsi->open_files, list)
			atomic_inc(&ctx->cache_hits);
		rcu_read_unlock();
	}
}

 
void nfs_readdir_record_entry_cache_miss(struct inode *dir)
{
	struct nfs_inode *nfsi = NFS_I(dir);
	struct nfs_open_dir_context *ctx;

	if (nfs_server_capable(dir, NFS_CAP_READDIRPLUS) &&
	    S_ISDIR(dir->i_mode)) {
		rcu_read_lock();
		list_for_each_entry_rcu (ctx, &nfsi->open_files, list)
			atomic_inc(&ctx->cache_misses);
		rcu_read_unlock();
	}
}

static void nfs_lookup_advise_force_readdirplus(struct inode *dir,
						unsigned int flags)
{
	if (nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE))
		return;
	if (flags & (LOOKUP_EXCL | LOOKUP_PARENT | LOOKUP_REVAL))
		return;
	nfs_readdir_record_entry_cache_miss(dir);
}

static
void nfs_prime_dcache(struct dentry *parent, struct nfs_entry *entry,
		unsigned long dir_verifier)
{
	struct qstr filename = QSTR_INIT(entry->name, entry->len);
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	struct dentry *dentry;
	struct dentry *alias;
	struct inode *inode;
	int status;

	if (!(entry->fattr->valid & NFS_ATTR_FATTR_FILEID))
		return;
	if (!(entry->fattr->valid & NFS_ATTR_FATTR_FSID))
		return;
	if (filename.len == 0)
		return;
	 
	if (strnlen(filename.name, filename.len) != filename.len)
		return;
	 
	if (strnchr(filename.name, filename.len, '/'))
		return;
	if (filename.name[0] == '.') {
		if (filename.len == 1)
			return;
		if (filename.len == 2 && filename.name[1] == '.')
			return;
	}
	filename.hash = full_name_hash(parent, filename.name, filename.len);

	dentry = d_lookup(parent, &filename);
again:
	if (!dentry) {
		dentry = d_alloc_parallel(parent, &filename, &wq);
		if (IS_ERR(dentry))
			return;
	}
	if (!d_in_lookup(dentry)) {
		 
		if (!nfs_fsid_equal(&NFS_SB(dentry->d_sb)->fsid,
					&entry->fattr->fsid))
			goto out;
		if (nfs_same_file(dentry, entry)) {
			if (!entry->fh->size)
				goto out;
			nfs_set_verifier(dentry, dir_verifier);
			status = nfs_refresh_inode(d_inode(dentry), entry->fattr);
			if (!status)
				nfs_setsecurity(d_inode(dentry), entry->fattr);
			trace_nfs_readdir_lookup_revalidate(d_inode(parent),
							    dentry, 0, status);
			goto out;
		} else {
			trace_nfs_readdir_lookup_revalidate_failed(
				d_inode(parent), dentry, 0);
			d_invalidate(dentry);
			dput(dentry);
			dentry = NULL;
			goto again;
		}
	}
	if (!entry->fh->size) {
		d_lookup_done(dentry);
		goto out;
	}

	inode = nfs_fhget(dentry->d_sb, entry->fh, entry->fattr);
	alias = d_splice_alias(inode, dentry);
	d_lookup_done(dentry);
	if (alias) {
		if (IS_ERR(alias))
			goto out;
		dput(dentry);
		dentry = alias;
	}
	nfs_set_verifier(dentry, dir_verifier);
	trace_nfs_readdir_lookup(d_inode(parent), dentry, 0);
out:
	dput(dentry);
}

static int nfs_readdir_entry_decode(struct nfs_readdir_descriptor *desc,
				    struct nfs_entry *entry,
				    struct xdr_stream *stream)
{
	int ret;

	if (entry->fattr->label)
		entry->fattr->label->len = NFS4_MAXLABELLEN;
	ret = xdr_decode(desc, entry, stream);
	if (ret || !desc->plus)
		return ret;
	nfs_prime_dcache(file_dentry(desc->file), entry, desc->dir_verifier);
	return 0;
}

 
static int nfs_readdir_folio_filler(struct nfs_readdir_descriptor *desc,
				    struct nfs_entry *entry,
				    struct page **xdr_pages, unsigned int buflen,
				    struct folio **arrays, size_t narrays,
				    u64 change_attr)
{
	struct address_space *mapping = desc->file->f_mapping;
	struct folio *new, *folio = *arrays;
	struct xdr_stream stream;
	struct page *scratch;
	struct xdr_buf buf;
	u64 cookie;
	int status;

	scratch = alloc_page(GFP_KERNEL);
	if (scratch == NULL)
		return -ENOMEM;

	xdr_init_decode_pages(&stream, &buf, xdr_pages, buflen);
	xdr_set_scratch_page(&stream, scratch);

	do {
		status = nfs_readdir_entry_decode(desc, entry, &stream);
		if (status != 0)
			break;

		status = nfs_readdir_folio_array_append(folio, entry, &cookie);
		if (status != -ENOSPC)
			continue;

		if (folio->mapping != mapping) {
			if (!--narrays)
				break;
			new = nfs_readdir_folio_array_alloc(cookie, GFP_KERNEL);
			if (!new)
				break;
			arrays++;
			*arrays = folio = new;
		} else {
			new = nfs_readdir_folio_get_next(mapping, cookie,
							 change_attr);
			if (!new)
				break;
			if (folio != *arrays)
				nfs_readdir_folio_unlock_and_put(folio);
			folio = new;
		}
		desc->folio_index_max++;
		status = nfs_readdir_folio_array_append(folio, entry, &cookie);
	} while (!status && !entry->eof);

	switch (status) {
	case -EBADCOOKIE:
		if (!entry->eof)
			break;
		nfs_readdir_folio_set_eof(folio);
		fallthrough;
	case -EAGAIN:
		status = 0;
		break;
	case -ENOSPC:
		status = 0;
		if (!desc->plus)
			break;
		while (!nfs_readdir_entry_decode(desc, entry, &stream))
			;
	}

	if (folio != *arrays)
		nfs_readdir_folio_unlock_and_put(folio);

	put_page(scratch);
	return status;
}

static void nfs_readdir_free_pages(struct page **pages, size_t npages)
{
	while (npages--)
		put_page(pages[npages]);
	kfree(pages);
}

 
static struct page **nfs_readdir_alloc_pages(size_t npages)
{
	struct page **pages;
	size_t i;

	pages = kmalloc_array(npages, sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		struct page *page = alloc_page(GFP_KERNEL);
		if (page == NULL)
			goto out_freepages;
		pages[i] = page;
	}
	return pages;

out_freepages:
	nfs_readdir_free_pages(pages, i);
	return NULL;
}

static int nfs_readdir_xdr_to_array(struct nfs_readdir_descriptor *desc,
				    __be32 *verf_arg, __be32 *verf_res,
				    struct folio **arrays, size_t narrays)
{
	u64 change_attr;
	struct page **pages;
	struct folio *folio = *arrays;
	struct nfs_entry *entry;
	size_t array_size;
	struct inode *inode = file_inode(desc->file);
	unsigned int dtsize = desc->dtsize;
	unsigned int pglen;
	int status = -ENOMEM;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;
	entry->cookie = nfs_readdir_folio_last_cookie(folio);
	entry->fh = nfs_alloc_fhandle();
	entry->fattr = nfs_alloc_fattr_with_label(NFS_SERVER(inode));
	entry->server = NFS_SERVER(inode);
	if (entry->fh == NULL || entry->fattr == NULL)
		goto out;

	array_size = (dtsize + PAGE_SIZE - 1) >> PAGE_SHIFT;
	pages = nfs_readdir_alloc_pages(array_size);
	if (!pages)
		goto out;

	change_attr = inode_peek_iversion_raw(inode);
	status = nfs_readdir_xdr_filler(desc, verf_arg, entry->cookie, pages,
					dtsize, verf_res);
	if (status < 0)
		goto free_pages;

	pglen = status;
	if (pglen != 0)
		status = nfs_readdir_folio_filler(desc, entry, pages, pglen,
						  arrays, narrays, change_attr);
	else
		nfs_readdir_folio_set_eof(folio);
	desc->buffer_fills++;

free_pages:
	nfs_readdir_free_pages(pages, array_size);
out:
	nfs_free_fattr(entry->fattr);
	nfs_free_fhandle(entry->fh);
	kfree(entry);
	return status;
}

static void nfs_readdir_folio_put(struct nfs_readdir_descriptor *desc)
{
	folio_put(desc->folio);
	desc->folio = NULL;
}

static void
nfs_readdir_folio_unlock_and_put_cached(struct nfs_readdir_descriptor *desc)
{
	folio_unlock(desc->folio);
	nfs_readdir_folio_put(desc);
}

static struct folio *
nfs_readdir_folio_get_cached(struct nfs_readdir_descriptor *desc)
{
	struct address_space *mapping = desc->file->f_mapping;
	u64 change_attr = inode_peek_iversion_raw(mapping->host);
	u64 cookie = desc->last_cookie;
	struct folio *folio;

	folio = nfs_readdir_folio_get_locked(mapping, cookie, change_attr);
	if (!folio)
		return NULL;
	if (desc->clear_cache && !nfs_readdir_folio_needs_filling(folio))
		nfs_readdir_folio_reinit_array(folio, cookie, change_attr);
	return folio;
}

 
static int find_and_lock_cache_page(struct nfs_readdir_descriptor *desc)
{
	struct inode *inode = file_inode(desc->file);
	struct nfs_inode *nfsi = NFS_I(inode);
	__be32 verf[NFS_DIR_VERIFIER_SIZE];
	int res;

	desc->folio = nfs_readdir_folio_get_cached(desc);
	if (!desc->folio)
		return -ENOMEM;
	if (nfs_readdir_folio_needs_filling(desc->folio)) {
		 
		if (desc->folio_index == desc->folio_index_max)
			nfs_grow_dtsize(desc);
		desc->folio_index_max = desc->folio_index;
		trace_nfs_readdir_cache_fill(desc->file, nfsi->cookieverf,
					     desc->last_cookie,
					     desc->folio->index, desc->dtsize);
		res = nfs_readdir_xdr_to_array(desc, nfsi->cookieverf, verf,
					       &desc->folio, 1);
		if (res < 0) {
			nfs_readdir_folio_unlock_and_put_cached(desc);
			trace_nfs_readdir_cache_fill_done(inode, res);
			if (res == -EBADCOOKIE || res == -ENOTSYNC) {
				invalidate_inode_pages2(desc->file->f_mapping);
				nfs_readdir_rewind_search(desc);
				trace_nfs_readdir_invalidate_cache_range(
					inode, 0, MAX_LFS_FILESIZE);
				return -EAGAIN;
			}
			return res;
		}
		 
		if (desc->last_cookie == 0 &&
		    memcmp(nfsi->cookieverf, verf, sizeof(nfsi->cookieverf))) {
			memcpy(nfsi->cookieverf, verf,
			       sizeof(nfsi->cookieverf));
			invalidate_inode_pages2_range(desc->file->f_mapping, 1,
						      -1);
			trace_nfs_readdir_invalidate_cache_range(
				inode, 1, MAX_LFS_FILESIZE);
		}
		desc->clear_cache = false;
	}
	res = nfs_readdir_search_array(desc);
	if (res == 0)
		return 0;
	nfs_readdir_folio_unlock_and_put_cached(desc);
	return res;
}

 
static int readdir_search_pagecache(struct nfs_readdir_descriptor *desc)
{
	int res;

	do {
		res = find_and_lock_cache_page(desc);
	} while (res == -EAGAIN);
	return res;
}

#define NFS_READDIR_CACHE_MISS_THRESHOLD (16UL)

 
static void nfs_do_filldir(struct nfs_readdir_descriptor *desc,
			   const __be32 *verf)
{
	struct file	*file = desc->file;
	struct nfs_cache_array *array;
	unsigned int i;
	bool first_emit = !desc->dir_cookie;

	array = kmap_local_folio(desc->folio, 0);
	for (i = desc->cache_entry_index; i < array->size; i++) {
		struct nfs_cache_array_entry *ent;

		 
		if (first_emit && i > NFS_READDIR_CACHE_MISS_THRESHOLD + 2) {
			desc->eob = true;
			break;
		}

		ent = &array->array[i];
		if (!dir_emit(desc->ctx, ent->name, ent->name_len,
		    nfs_compat_user_ino64(ent->ino), ent->d_type)) {
			desc->eob = true;
			break;
		}
		memcpy(desc->verf, verf, sizeof(desc->verf));
		if (i == array->size - 1) {
			desc->dir_cookie = array->last_cookie;
			nfs_readdir_seek_next_array(array, desc);
		} else {
			desc->dir_cookie = array->array[i + 1].cookie;
			desc->last_cookie = array->array[0].cookie;
		}
		if (nfs_readdir_use_cookie(file))
			desc->ctx->pos = desc->dir_cookie;
		else
			desc->ctx->pos++;
	}
	if (array->folio_is_eof)
		desc->eof = !desc->eob;

	kunmap_local(array);
	dfprintk(DIRCACHE, "NFS: nfs_do_filldir() filling ended @ cookie %llu\n",
			(unsigned long long)desc->dir_cookie);
}

 
static int uncached_readdir(struct nfs_readdir_descriptor *desc)
{
	struct folio	**arrays;
	size_t		i, sz = 512;
	__be32		verf[NFS_DIR_VERIFIER_SIZE];
	int		status = -ENOMEM;

	dfprintk(DIRCACHE, "NFS: uncached_readdir() searching for cookie %llu\n",
			(unsigned long long)desc->dir_cookie);

	arrays = kcalloc(sz, sizeof(*arrays), GFP_KERNEL);
	if (!arrays)
		goto out;
	arrays[0] = nfs_readdir_folio_array_alloc(desc->dir_cookie, GFP_KERNEL);
	if (!arrays[0])
		goto out;

	desc->folio_index = 0;
	desc->cache_entry_index = 0;
	desc->last_cookie = desc->dir_cookie;
	desc->folio_index_max = 0;

	trace_nfs_readdir_uncached(desc->file, desc->verf, desc->last_cookie,
				   -1, desc->dtsize);

	status = nfs_readdir_xdr_to_array(desc, desc->verf, verf, arrays, sz);
	if (status < 0) {
		trace_nfs_readdir_uncached_done(file_inode(desc->file), status);
		goto out_free;
	}

	for (i = 0; !desc->eob && i < sz && arrays[i]; i++) {
		desc->folio = arrays[i];
		nfs_do_filldir(desc, verf);
	}
	desc->folio = NULL;

	 
	if (!desc->eof) {
		if (!desc->eob)
			nfs_grow_dtsize(desc);
		else if (desc->buffer_fills == 1 &&
			 i < (desc->folio_index_max >> 1))
			nfs_shrink_dtsize(desc);
	}
out_free:
	for (i = 0; i < sz && arrays[i]; i++)
		nfs_readdir_folio_array_free(arrays[i]);
out:
	if (!nfs_readdir_use_cookie(desc->file))
		nfs_readdir_rewind_search(desc);
	desc->folio_index_max = -1;
	kfree(arrays);
	dfprintk(DIRCACHE, "NFS: %s: returns %d\n", __func__, status);
	return status;
}

static bool nfs_readdir_handle_cache_misses(struct inode *inode,
					    struct nfs_readdir_descriptor *desc,
					    unsigned int cache_misses,
					    bool force_clear)
{
	if (desc->ctx->pos == 0 || !desc->plus)
		return false;
	if (cache_misses <= NFS_READDIR_CACHE_MISS_THRESHOLD && !force_clear)
		return false;
	trace_nfs_readdir_force_readdirplus(inode);
	return true;
}

 
static int nfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct dentry	*dentry = file_dentry(file);
	struct inode	*inode = d_inode(dentry);
	struct nfs_inode *nfsi = NFS_I(inode);
	struct nfs_open_dir_context *dir_ctx = file->private_data;
	struct nfs_readdir_descriptor *desc;
	unsigned int cache_hits, cache_misses;
	bool force_clear;
	int res;

	dfprintk(FILE, "NFS: readdir(%pD2) starting at cookie %llu\n",
			file, (long long)ctx->pos);
	nfs_inc_stats(inode, NFSIOS_VFSGETDENTS);

	 
	nfs_revalidate_mapping(inode, file->f_mapping);

	res = -ENOMEM;
	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		goto out;
	desc->file = file;
	desc->ctx = ctx;
	desc->folio_index_max = -1;

	spin_lock(&file->f_lock);
	desc->dir_cookie = dir_ctx->dir_cookie;
	desc->folio_index = dir_ctx->page_index;
	desc->last_cookie = dir_ctx->last_cookie;
	desc->attr_gencount = dir_ctx->attr_gencount;
	desc->eof = dir_ctx->eof;
	nfs_set_dtsize(desc, dir_ctx->dtsize);
	memcpy(desc->verf, dir_ctx->verf, sizeof(desc->verf));
	cache_hits = atomic_xchg(&dir_ctx->cache_hits, 0);
	cache_misses = atomic_xchg(&dir_ctx->cache_misses, 0);
	force_clear = dir_ctx->force_clear;
	spin_unlock(&file->f_lock);

	if (desc->eof) {
		res = 0;
		goto out_free;
	}

	desc->plus = nfs_use_readdirplus(inode, ctx, cache_hits, cache_misses);
	force_clear = nfs_readdir_handle_cache_misses(inode, desc, cache_misses,
						      force_clear);
	desc->clear_cache = force_clear;

	do {
		res = readdir_search_pagecache(desc);

		if (res == -EBADCOOKIE) {
			res = 0;
			 
			if (desc->dir_cookie && !desc->eof) {
				 
				res = uncached_readdir(desc);
				if (res == 0)
					continue;
				if (res == -EBADCOOKIE || res == -ENOTSYNC)
					res = 0;
			}
			break;
		}
		if (res == -ETOOSMALL && desc->plus) {
			nfs_zap_caches(inode);
			desc->plus = false;
			desc->eof = false;
			continue;
		}
		if (res < 0)
			break;

		nfs_do_filldir(desc, nfsi->cookieverf);
		nfs_readdir_folio_unlock_and_put_cached(desc);
		if (desc->folio_index == desc->folio_index_max)
			desc->clear_cache = force_clear;
	} while (!desc->eob && !desc->eof);

	spin_lock(&file->f_lock);
	dir_ctx->dir_cookie = desc->dir_cookie;
	dir_ctx->last_cookie = desc->last_cookie;
	dir_ctx->attr_gencount = desc->attr_gencount;
	dir_ctx->page_index = desc->folio_index;
	dir_ctx->force_clear = force_clear;
	dir_ctx->eof = desc->eof;
	dir_ctx->dtsize = desc->dtsize;
	memcpy(dir_ctx->verf, desc->verf, sizeof(dir_ctx->verf));
	spin_unlock(&file->f_lock);
out_free:
	kfree(desc);

out:
	dfprintk(FILE, "NFS: readdir(%pD2) returns %d\n", file, res);
	return res;
}

static loff_t nfs_llseek_dir(struct file *filp, loff_t offset, int whence)
{
	struct nfs_open_dir_context *dir_ctx = filp->private_data;

	dfprintk(FILE, "NFS: llseek dir(%pD2, %lld, %d)\n",
			filp, offset, whence);

	switch (whence) {
	default:
		return -EINVAL;
	case SEEK_SET:
		if (offset < 0)
			return -EINVAL;
		spin_lock(&filp->f_lock);
		break;
	case SEEK_CUR:
		if (offset == 0)
			return filp->f_pos;
		spin_lock(&filp->f_lock);
		offset += filp->f_pos;
		if (offset < 0) {
			spin_unlock(&filp->f_lock);
			return -EINVAL;
		}
	}
	if (offset != filp->f_pos) {
		filp->f_pos = offset;
		dir_ctx->page_index = 0;
		if (!nfs_readdir_use_cookie(filp)) {
			dir_ctx->dir_cookie = 0;
			dir_ctx->last_cookie = 0;
		} else {
			dir_ctx->dir_cookie = offset;
			dir_ctx->last_cookie = offset;
		}
		dir_ctx->eof = false;
	}
	spin_unlock(&filp->f_lock);
	return offset;
}

 
static int nfs_fsync_dir(struct file *filp, loff_t start, loff_t end,
			 int datasync)
{
	dfprintk(FILE, "NFS: fsync dir(%pD2) datasync %d\n", filp, datasync);

	nfs_inc_stats(file_inode(filp), NFSIOS_VFSFSYNC);
	return 0;
}

 
void nfs_force_lookup_revalidate(struct inode *dir)
{
	NFS_I(dir)->cache_change_attribute += 2;
}
EXPORT_SYMBOL_GPL(nfs_force_lookup_revalidate);

 
static bool nfs_verify_change_attribute(struct inode *dir, unsigned long verf)
{
	return (verf & ~1UL) == nfs_save_change_attribute(dir);
}

static void nfs_set_verifier_delegated(unsigned long *verf)
{
	*verf |= 1UL;
}

#if IS_ENABLED(CONFIG_NFS_V4)
static void nfs_unset_verifier_delegated(unsigned long *verf)
{
	*verf &= ~1UL;
}
#endif  

static bool nfs_test_verifier_delegated(unsigned long verf)
{
	return verf & 1;
}

static bool nfs_verifier_is_delegated(struct dentry *dentry)
{
	return nfs_test_verifier_delegated(dentry->d_time);
}

static void nfs_set_verifier_locked(struct dentry *dentry, unsigned long verf)
{
	struct inode *inode = d_inode(dentry);
	struct inode *dir = d_inode(dentry->d_parent);

	if (!nfs_verify_change_attribute(dir, verf))
		return;
	if (inode && NFS_PROTO(inode)->have_delegation(inode, FMODE_READ))
		nfs_set_verifier_delegated(&verf);
	dentry->d_time = verf;
}

 
void nfs_set_verifier(struct dentry *dentry, unsigned long verf)
{

	spin_lock(&dentry->d_lock);
	nfs_set_verifier_locked(dentry, verf);
	spin_unlock(&dentry->d_lock);
}
EXPORT_SYMBOL_GPL(nfs_set_verifier);

#if IS_ENABLED(CONFIG_NFS_V4)
 
void nfs_clear_verifier_delegated(struct inode *inode)
{
	struct dentry *alias;

	if (!inode)
		return;
	spin_lock(&inode->i_lock);
	hlist_for_each_entry(alias, &inode->i_dentry, d_u.d_alias) {
		spin_lock(&alias->d_lock);
		nfs_unset_verifier_delegated(&alias->d_time);
		spin_unlock(&alias->d_lock);
	}
	spin_unlock(&inode->i_lock);
}
EXPORT_SYMBOL_GPL(nfs_clear_verifier_delegated);
#endif  

static int nfs_dentry_verify_change(struct inode *dir, struct dentry *dentry)
{
	if (nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE) &&
	    d_really_is_negative(dentry))
		return dentry->d_time == inode_peek_iversion_raw(dir);
	return nfs_verify_change_attribute(dir, dentry->d_time);
}

 
static int nfs_check_verifier(struct inode *dir, struct dentry *dentry,
			      int rcu_walk)
{
	if (IS_ROOT(dentry))
		return 1;
	if (NFS_SERVER(dir)->flags & NFS_MOUNT_LOOKUP_CACHE_NONE)
		return 0;
	if (!nfs_dentry_verify_change(dir, dentry))
		return 0;
	 
	if (nfs_mapping_need_revalidate_inode(dir)) {
		if (rcu_walk)
			return 0;
		if (__nfs_revalidate_inode(NFS_SERVER(dir), dir) < 0)
			return 0;
	}
	if (!nfs_dentry_verify_change(dir, dentry))
		return 0;
	return 1;
}

 
static int nfs_is_exclusive_create(struct inode *dir, unsigned int flags)
{
	if (NFS_PROTO(dir)->version == 2)
		return 0;
	return flags & LOOKUP_EXCL;
}

 
static
int nfs_lookup_verify_inode(struct inode *inode, unsigned int flags)
{
	struct nfs_server *server = NFS_SERVER(inode);
	int ret;

	if (IS_AUTOMOUNT(inode))
		return 0;

	if (flags & LOOKUP_OPEN) {
		switch (inode->i_mode & S_IFMT) {
		case S_IFREG:
			 
			if (server->caps & NFS_CAP_ATOMIC_OPEN)
				goto out;
			fallthrough;
		case S_IFDIR:
			if (server->flags & NFS_MOUNT_NOCTO)
				break;
			 
			goto out_force;
		}
	}

	 
	if (flags & LOOKUP_REVAL)
		goto out_force;
out:
	if (inode->i_nlink > 0 ||
	    (inode->i_nlink == 0 &&
	     test_bit(NFS_INO_PRESERVE_UNLINKED, &NFS_I(inode)->flags)))
		return 0;
	else
		return -ESTALE;
out_force:
	if (flags & LOOKUP_RCU)
		return -ECHILD;
	ret = __nfs_revalidate_inode(server, inode);
	if (ret != 0)
		return ret;
	goto out;
}

static void nfs_mark_dir_for_revalidate(struct inode *inode)
{
	spin_lock(&inode->i_lock);
	nfs_set_cache_invalid(inode, NFS_INO_INVALID_CHANGE);
	spin_unlock(&inode->i_lock);
}

 
static inline
int nfs_neg_need_reval(struct inode *dir, struct dentry *dentry,
		       unsigned int flags)
{
	if (flags & (LOOKUP_CREATE | LOOKUP_RENAME_TARGET))
		return 0;
	if (NFS_SERVER(dir)->flags & NFS_MOUNT_LOOKUP_CACHE_NONEG)
		return 1;
	 
	if (nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE))
		return 1;
	return !nfs_check_verifier(dir, dentry, flags & LOOKUP_RCU);
}

static int
nfs_lookup_revalidate_done(struct inode *dir, struct dentry *dentry,
			   struct inode *inode, int error)
{
	switch (error) {
	case 1:
		break;
	case 0:
		 
		if (inode && IS_ROOT(dentry))
			error = 1;
		break;
	}
	trace_nfs_lookup_revalidate_exit(dir, dentry, 0, error);
	return error;
}

static int
nfs_lookup_revalidate_negative(struct inode *dir, struct dentry *dentry,
			       unsigned int flags)
{
	int ret = 1;
	if (nfs_neg_need_reval(dir, dentry, flags)) {
		if (flags & LOOKUP_RCU)
			return -ECHILD;
		ret = 0;
	}
	return nfs_lookup_revalidate_done(dir, dentry, NULL, ret);
}

static int
nfs_lookup_revalidate_delegated(struct inode *dir, struct dentry *dentry,
				struct inode *inode)
{
	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
	return nfs_lookup_revalidate_done(dir, dentry, inode, 1);
}

static int nfs_lookup_revalidate_dentry(struct inode *dir,
					struct dentry *dentry,
					struct inode *inode, unsigned int flags)
{
	struct nfs_fh *fhandle;
	struct nfs_fattr *fattr;
	unsigned long dir_verifier;
	int ret;

	trace_nfs_lookup_revalidate_enter(dir, dentry, flags);

	ret = -ENOMEM;
	fhandle = nfs_alloc_fhandle();
	fattr = nfs_alloc_fattr_with_label(NFS_SERVER(inode));
	if (fhandle == NULL || fattr == NULL)
		goto out;

	dir_verifier = nfs_save_change_attribute(dir);
	ret = NFS_PROTO(dir)->lookup(dir, dentry, fhandle, fattr);
	if (ret < 0) {
		switch (ret) {
		case -ESTALE:
		case -ENOENT:
			ret = 0;
			break;
		case -ETIMEDOUT:
			if (NFS_SERVER(inode)->flags & NFS_MOUNT_SOFTREVAL)
				ret = 1;
		}
		goto out;
	}

	 
	nfs_lookup_advise_force_readdirplus(dir, flags);

	ret = 0;
	if (nfs_compare_fh(NFS_FH(inode), fhandle))
		goto out;
	if (nfs_refresh_inode(inode, fattr) < 0)
		goto out;

	nfs_setsecurity(inode, fattr);
	nfs_set_verifier(dentry, dir_verifier);

	ret = 1;
out:
	nfs_free_fattr(fattr);
	nfs_free_fhandle(fhandle);

	 
	if (!ret && nfs_dentry_verify_change(dir, dentry))
		nfs_mark_dir_for_revalidate(dir);
	return nfs_lookup_revalidate_done(dir, dentry, inode, ret);
}

 
static int
nfs_do_lookup_revalidate(struct inode *dir, struct dentry *dentry,
			 unsigned int flags)
{
	struct inode *inode;
	int error;

	nfs_inc_stats(dir, NFSIOS_DENTRYREVALIDATE);
	inode = d_inode(dentry);

	if (!inode)
		return nfs_lookup_revalidate_negative(dir, dentry, flags);

	if (is_bad_inode(inode)) {
		dfprintk(LOOKUPCACHE, "%s: %pd2 has dud inode\n",
				__func__, dentry);
		goto out_bad;
	}

	if ((flags & LOOKUP_RENAME_TARGET) && d_count(dentry) < 2 &&
	    nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE))
		goto out_bad;

	if (nfs_verifier_is_delegated(dentry))
		return nfs_lookup_revalidate_delegated(dir, dentry, inode);

	 
	if (!(flags & (LOOKUP_EXCL | LOOKUP_REVAL)) &&
	    nfs_check_verifier(dir, dentry, flags & LOOKUP_RCU)) {
		error = nfs_lookup_verify_inode(inode, flags);
		if (error) {
			if (error == -ESTALE)
				nfs_mark_dir_for_revalidate(dir);
			goto out_bad;
		}
		goto out_valid;
	}

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	if (NFS_STALE(inode))
		goto out_bad;

	return nfs_lookup_revalidate_dentry(dir, dentry, inode, flags);
out_valid:
	return nfs_lookup_revalidate_done(dir, dentry, inode, 1);
out_bad:
	if (flags & LOOKUP_RCU)
		return -ECHILD;
	return nfs_lookup_revalidate_done(dir, dentry, inode, 0);
}

static int
__nfs_lookup_revalidate(struct dentry *dentry, unsigned int flags,
			int (*reval)(struct inode *, struct dentry *, unsigned int))
{
	struct dentry *parent;
	struct inode *dir;
	int ret;

	if (flags & LOOKUP_RCU) {
		if (dentry->d_fsdata == NFS_FSDATA_BLOCKED)
			return -ECHILD;
		parent = READ_ONCE(dentry->d_parent);
		dir = d_inode_rcu(parent);
		if (!dir)
			return -ECHILD;
		ret = reval(dir, dentry, flags);
		if (parent != READ_ONCE(dentry->d_parent))
			return -ECHILD;
	} else {
		 
		wait_var_event(&dentry->d_fsdata,
			       dentry->d_fsdata != NFS_FSDATA_BLOCKED);
		parent = dget_parent(dentry);
		ret = reval(d_inode(parent), dentry, flags);
		dput(parent);
	}
	return ret;
}

static int nfs_lookup_revalidate(struct dentry *dentry, unsigned int flags)
{
	return __nfs_lookup_revalidate(dentry, flags, nfs_do_lookup_revalidate);
}

 
static int nfs_weak_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct inode *inode = d_inode(dentry);
	int error = 0;

	 
	if (!inode) {
		dfprintk(LOOKUPCACHE, "%s: %pd2 has negative inode\n",
				__func__, dentry);
		return 1;
	}

	if (is_bad_inode(inode)) {
		dfprintk(LOOKUPCACHE, "%s: %pd2 has dud inode\n",
				__func__, dentry);
		return 0;
	}

	error = nfs_lookup_verify_inode(inode, flags);
	dfprintk(LOOKUPCACHE, "NFS: %s: inode %lu is %s\n",
			__func__, inode->i_ino, error ? "invalid" : "valid");
	return !error;
}

 
static int nfs_dentry_delete(const struct dentry *dentry)
{
	dfprintk(VFS, "NFS: dentry_delete(%pd2, %x)\n",
		dentry, dentry->d_flags);

	 
	if (d_really_is_positive(dentry) && NFS_STALE(d_inode(dentry)))
		return 1;

	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		 
		return 1;
	}
	if (!(dentry->d_sb->s_flags & SB_ACTIVE)) {
		 
		return 1;
	}
	return 0;

}

 
static void nfs_drop_nlink(struct inode *inode)
{
	spin_lock(&inode->i_lock);
	 
	if (inode->i_nlink > 0)
		drop_nlink(inode);
	NFS_I(inode)->attr_gencount = nfs_inc_attr_generation_counter();
	nfs_set_cache_invalid(
		inode, NFS_INO_INVALID_CHANGE | NFS_INO_INVALID_CTIME |
			       NFS_INO_INVALID_NLINK);
	spin_unlock(&inode->i_lock);
}

 
static void nfs_dentry_iput(struct dentry *dentry, struct inode *inode)
{
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		nfs_complete_unlink(dentry, inode);
		nfs_drop_nlink(inode);
	}
	iput(inode);
}

static void nfs_d_release(struct dentry *dentry)
{
	 
	if (unlikely(dentry->d_fsdata)) {
		if (dentry->d_flags & DCACHE_NFSFS_RENAMED)
			WARN_ON(1);
		else
			kfree(dentry->d_fsdata);
	}
}

const struct dentry_operations nfs_dentry_operations = {
	.d_revalidate	= nfs_lookup_revalidate,
	.d_weak_revalidate	= nfs_weak_revalidate,
	.d_delete	= nfs_dentry_delete,
	.d_iput		= nfs_dentry_iput,
	.d_automount	= nfs_d_automount,
	.d_release	= nfs_d_release,
};
EXPORT_SYMBOL_GPL(nfs_dentry_operations);

struct dentry *nfs_lookup(struct inode *dir, struct dentry * dentry, unsigned int flags)
{
	struct dentry *res;
	struct inode *inode = NULL;
	struct nfs_fh *fhandle = NULL;
	struct nfs_fattr *fattr = NULL;
	unsigned long dir_verifier;
	int error;

	dfprintk(VFS, "NFS: lookup(%pd2)\n", dentry);
	nfs_inc_stats(dir, NFSIOS_VFSLOOKUP);

	if (unlikely(dentry->d_name.len > NFS_SERVER(dir)->namelen))
		return ERR_PTR(-ENAMETOOLONG);

	 
	if (nfs_is_exclusive_create(dir, flags) || flags & LOOKUP_RENAME_TARGET)
		return NULL;

	res = ERR_PTR(-ENOMEM);
	fhandle = nfs_alloc_fhandle();
	fattr = nfs_alloc_fattr_with_label(NFS_SERVER(dir));
	if (fhandle == NULL || fattr == NULL)
		goto out;

	dir_verifier = nfs_save_change_attribute(dir);
	trace_nfs_lookup_enter(dir, dentry, flags);
	error = NFS_PROTO(dir)->lookup(dir, dentry, fhandle, fattr);
	if (error == -ENOENT) {
		if (nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE))
			dir_verifier = inode_peek_iversion_raw(dir);
		goto no_entry;
	}
	if (error < 0) {
		res = ERR_PTR(error);
		goto out;
	}
	inode = nfs_fhget(dentry->d_sb, fhandle, fattr);
	res = ERR_CAST(inode);
	if (IS_ERR(res))
		goto out;

	 
	nfs_lookup_advise_force_readdirplus(dir, flags);

no_entry:
	res = d_splice_alias(inode, dentry);
	if (res != NULL) {
		if (IS_ERR(res))
			goto out;
		dentry = res;
	}
	nfs_set_verifier(dentry, dir_verifier);
out:
	trace_nfs_lookup_exit(dir, dentry, flags, PTR_ERR_OR_ZERO(res));
	nfs_free_fattr(fattr);
	nfs_free_fhandle(fhandle);
	return res;
}
EXPORT_SYMBOL_GPL(nfs_lookup);

void nfs_d_prune_case_insensitive_aliases(struct inode *inode)
{
	 
	if (inode && nfs_server_capable(inode, NFS_CAP_CASE_INSENSITIVE))
		d_prune_aliases(inode);
}
EXPORT_SYMBOL_GPL(nfs_d_prune_case_insensitive_aliases);

#if IS_ENABLED(CONFIG_NFS_V4)
static int nfs4_lookup_revalidate(struct dentry *, unsigned int);

const struct dentry_operations nfs4_dentry_operations = {
	.d_revalidate	= nfs4_lookup_revalidate,
	.d_weak_revalidate	= nfs_weak_revalidate,
	.d_delete	= nfs_dentry_delete,
	.d_iput		= nfs_dentry_iput,
	.d_automount	= nfs_d_automount,
	.d_release	= nfs_d_release,
};
EXPORT_SYMBOL_GPL(nfs4_dentry_operations);

static struct nfs_open_context *create_nfs_open_context(struct dentry *dentry, int open_flags, struct file *filp)
{
	return alloc_nfs_open_context(dentry, flags_to_mode(open_flags), filp);
}

static int do_open(struct inode *inode, struct file *filp)
{
	nfs_fscache_open_file(inode, filp);
	return 0;
}

static int nfs_finish_open(struct nfs_open_context *ctx,
			   struct dentry *dentry,
			   struct file *file, unsigned open_flags)
{
	int err;

	err = finish_open(file, dentry, do_open);
	if (err)
		goto out;
	if (S_ISREG(file_inode(file)->i_mode))
		nfs_file_set_open_context(file, ctx);
	else
		err = -EOPENSTALE;
out:
	return err;
}

int nfs_atomic_open(struct inode *dir, struct dentry *dentry,
		    struct file *file, unsigned open_flags,
		    umode_t mode)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	struct nfs_open_context *ctx;
	struct dentry *res;
	struct iattr attr = { .ia_valid = ATTR_OPEN };
	struct inode *inode;
	unsigned int lookup_flags = 0;
	unsigned long dir_verifier;
	bool switched = false;
	int created = 0;
	int err;

	 
	BUG_ON(d_inode(dentry));

	dfprintk(VFS, "NFS: atomic_open(%s/%lu), %pd\n",
			dir->i_sb->s_id, dir->i_ino, dentry);

	err = nfs_check_flags(open_flags);
	if (err)
		return err;

	 
	if ((open_flags & O_DIRECTORY)) {
		if (!d_in_lookup(dentry)) {
			 
			return -ENOENT;
		}
		lookup_flags = LOOKUP_OPEN|LOOKUP_DIRECTORY;
		goto no_open;
	}

	if (dentry->d_name.len > NFS_SERVER(dir)->namelen)
		return -ENAMETOOLONG;

	if (open_flags & O_CREAT) {
		struct nfs_server *server = NFS_SERVER(dir);

		if (!(server->attr_bitmask[2] & FATTR4_WORD2_MODE_UMASK))
			mode &= ~current_umask();

		attr.ia_valid |= ATTR_MODE;
		attr.ia_mode = mode;
	}
	if (open_flags & O_TRUNC) {
		attr.ia_valid |= ATTR_SIZE;
		attr.ia_size = 0;
	}

	if (!(open_flags & O_CREAT) && !d_in_lookup(dentry)) {
		d_drop(dentry);
		switched = true;
		dentry = d_alloc_parallel(dentry->d_parent,
					  &dentry->d_name, &wq);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);
		if (unlikely(!d_in_lookup(dentry)))
			return finish_no_open(file, dentry);
	}

	ctx = create_nfs_open_context(dentry, open_flags, file);
	err = PTR_ERR(ctx);
	if (IS_ERR(ctx))
		goto out;

	trace_nfs_atomic_open_enter(dir, ctx, open_flags);
	inode = NFS_PROTO(dir)->open_context(dir, ctx, open_flags, &attr, &created);
	if (created)
		file->f_mode |= FMODE_CREATED;
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		trace_nfs_atomic_open_exit(dir, ctx, open_flags, err);
		put_nfs_open_context(ctx);
		d_drop(dentry);
		switch (err) {
		case -ENOENT:
			d_splice_alias(NULL, dentry);
			if (nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE))
				dir_verifier = inode_peek_iversion_raw(dir);
			else
				dir_verifier = nfs_save_change_attribute(dir);
			nfs_set_verifier(dentry, dir_verifier);
			break;
		case -EISDIR:
		case -ENOTDIR:
			goto no_open;
		case -ELOOP:
			if (!(open_flags & O_NOFOLLOW))
				goto no_open;
			break;
			 
		default:
			break;
		}
		goto out;
	}
	file->f_mode |= FMODE_CAN_ODIRECT;

	err = nfs_finish_open(ctx, ctx->dentry, file, open_flags);
	trace_nfs_atomic_open_exit(dir, ctx, open_flags, err);
	put_nfs_open_context(ctx);
out:
	if (unlikely(switched)) {
		d_lookup_done(dentry);
		dput(dentry);
	}
	return err;

no_open:
	res = nfs_lookup(dir, dentry, lookup_flags);
	if (!res) {
		inode = d_inode(dentry);
		if ((lookup_flags & LOOKUP_DIRECTORY) && inode &&
		    !(S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
			res = ERR_PTR(-ENOTDIR);
		else if (inode && S_ISREG(inode->i_mode))
			res = ERR_PTR(-EOPENSTALE);
	} else if (!IS_ERR(res)) {
		inode = d_inode(res);
		if ((lookup_flags & LOOKUP_DIRECTORY) && inode &&
		    !(S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode))) {
			dput(res);
			res = ERR_PTR(-ENOTDIR);
		} else if (inode && S_ISREG(inode->i_mode)) {
			dput(res);
			res = ERR_PTR(-EOPENSTALE);
		}
	}
	if (switched) {
		d_lookup_done(dentry);
		if (!res)
			res = dentry;
		else
			dput(dentry);
	}
	if (IS_ERR(res))
		return PTR_ERR(res);
	return finish_no_open(file, res);
}
EXPORT_SYMBOL_GPL(nfs_atomic_open);

static int
nfs4_do_lookup_revalidate(struct inode *dir, struct dentry *dentry,
			  unsigned int flags)
{
	struct inode *inode;

	if (!(flags & LOOKUP_OPEN) || (flags & LOOKUP_DIRECTORY))
		goto full_reval;
	if (d_mountpoint(dentry))
		goto full_reval;

	inode = d_inode(dentry);

	 
	if (inode == NULL)
		goto full_reval;

	if (nfs_verifier_is_delegated(dentry))
		return nfs_lookup_revalidate_delegated(dir, dentry, inode);

	 
	if (!S_ISREG(inode->i_mode))
		goto full_reval;

	 
	if (flags & (LOOKUP_EXCL | LOOKUP_REVAL))
		goto reval_dentry;

	 
	if (!nfs_check_verifier(dir, dentry, flags & LOOKUP_RCU))
		goto reval_dentry;

	 
	return 1;
reval_dentry:
	if (flags & LOOKUP_RCU)
		return -ECHILD;
	return nfs_lookup_revalidate_dentry(dir, dentry, inode, flags);

full_reval:
	return nfs_do_lookup_revalidate(dir, dentry, flags);
}

static int nfs4_lookup_revalidate(struct dentry *dentry, unsigned int flags)
{
	return __nfs_lookup_revalidate(dentry, flags,
			nfs4_do_lookup_revalidate);
}

#endif  

struct dentry *
nfs_add_or_obtain(struct dentry *dentry, struct nfs_fh *fhandle,
				struct nfs_fattr *fattr)
{
	struct dentry *parent = dget_parent(dentry);
	struct inode *dir = d_inode(parent);
	struct inode *inode;
	struct dentry *d;
	int error;

	d_drop(dentry);

	if (fhandle->size == 0) {
		error = NFS_PROTO(dir)->lookup(dir, dentry, fhandle, fattr);
		if (error)
			goto out_error;
	}
	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
	if (!(fattr->valid & NFS_ATTR_FATTR)) {
		struct nfs_server *server = NFS_SB(dentry->d_sb);
		error = server->nfs_client->rpc_ops->getattr(server, fhandle,
				fattr, NULL);
		if (error < 0)
			goto out_error;
	}
	inode = nfs_fhget(dentry->d_sb, fhandle, fattr);
	d = d_splice_alias(inode, dentry);
out:
	dput(parent);
	return d;
out_error:
	d = ERR_PTR(error);
	goto out;
}
EXPORT_SYMBOL_GPL(nfs_add_or_obtain);

 
int nfs_instantiate(struct dentry *dentry, struct nfs_fh *fhandle,
				struct nfs_fattr *fattr)
{
	struct dentry *d;

	d = nfs_add_or_obtain(dentry, fhandle, fattr);
	if (IS_ERR(d))
		return PTR_ERR(d);

	 
	dput(d);
	return 0;
}
EXPORT_SYMBOL_GPL(nfs_instantiate);

 
int nfs_create(struct mnt_idmap *idmap, struct inode *dir,
	       struct dentry *dentry, umode_t mode, bool excl)
{
	struct iattr attr;
	int open_flags = excl ? O_CREAT | O_EXCL : O_CREAT;
	int error;

	dfprintk(VFS, "NFS: create(%s/%lu), %pd\n",
			dir->i_sb->s_id, dir->i_ino, dentry);

	attr.ia_mode = mode;
	attr.ia_valid = ATTR_MODE;

	trace_nfs_create_enter(dir, dentry, open_flags);
	error = NFS_PROTO(dir)->create(dir, dentry, &attr, open_flags);
	trace_nfs_create_exit(dir, dentry, open_flags, error);
	if (error != 0)
		goto out_err;
	return 0;
out_err:
	d_drop(dentry);
	return error;
}
EXPORT_SYMBOL_GPL(nfs_create);

 
int
nfs_mknod(struct mnt_idmap *idmap, struct inode *dir,
	  struct dentry *dentry, umode_t mode, dev_t rdev)
{
	struct iattr attr;
	int status;

	dfprintk(VFS, "NFS: mknod(%s/%lu), %pd\n",
			dir->i_sb->s_id, dir->i_ino, dentry);

	attr.ia_mode = mode;
	attr.ia_valid = ATTR_MODE;

	trace_nfs_mknod_enter(dir, dentry);
	status = NFS_PROTO(dir)->mknod(dir, dentry, &attr, rdev);
	trace_nfs_mknod_exit(dir, dentry, status);
	if (status != 0)
		goto out_err;
	return 0;
out_err:
	d_drop(dentry);
	return status;
}
EXPORT_SYMBOL_GPL(nfs_mknod);

 
int nfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
	      struct dentry *dentry, umode_t mode)
{
	struct iattr attr;
	int error;

	dfprintk(VFS, "NFS: mkdir(%s/%lu), %pd\n",
			dir->i_sb->s_id, dir->i_ino, dentry);

	attr.ia_valid = ATTR_MODE;
	attr.ia_mode = mode | S_IFDIR;

	trace_nfs_mkdir_enter(dir, dentry);
	error = NFS_PROTO(dir)->mkdir(dir, dentry, &attr);
	trace_nfs_mkdir_exit(dir, dentry, error);
	if (error != 0)
		goto out_err;
	return 0;
out_err:
	d_drop(dentry);
	return error;
}
EXPORT_SYMBOL_GPL(nfs_mkdir);

static void nfs_dentry_handle_enoent(struct dentry *dentry)
{
	if (simple_positive(dentry))
		d_delete(dentry);
}

static void nfs_dentry_remove_handle_error(struct inode *dir,
					   struct dentry *dentry, int error)
{
	switch (error) {
	case -ENOENT:
		if (d_really_is_positive(dentry))
			d_delete(dentry);
		nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
		break;
	case 0:
		nfs_d_prune_case_insensitive_aliases(d_inode(dentry));
		nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
	}
}

int nfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;

	dfprintk(VFS, "NFS: rmdir(%s/%lu), %pd\n",
			dir->i_sb->s_id, dir->i_ino, dentry);

	trace_nfs_rmdir_enter(dir, dentry);
	if (d_really_is_positive(dentry)) {
		down_write(&NFS_I(d_inode(dentry))->rmdir_sem);
		error = NFS_PROTO(dir)->rmdir(dir, &dentry->d_name);
		 
		switch (error) {
		case 0:
			clear_nlink(d_inode(dentry));
			break;
		case -ENOENT:
			nfs_dentry_handle_enoent(dentry);
		}
		up_write(&NFS_I(d_inode(dentry))->rmdir_sem);
	} else
		error = NFS_PROTO(dir)->rmdir(dir, &dentry->d_name);
	nfs_dentry_remove_handle_error(dir, dentry, error);
	trace_nfs_rmdir_exit(dir, dentry, error);

	return error;
}
EXPORT_SYMBOL_GPL(nfs_rmdir);

 
static int nfs_safe_remove(struct dentry *dentry)
{
	struct inode *dir = d_inode(dentry->d_parent);
	struct inode *inode = d_inode(dentry);
	int error = -EBUSY;
		
	dfprintk(VFS, "NFS: safe_remove(%pd2)\n", dentry);

	 
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		error = 0;
		goto out;
	}

	trace_nfs_remove_enter(dir, dentry);
	if (inode != NULL) {
		error = NFS_PROTO(dir)->remove(dir, dentry);
		if (error == 0)
			nfs_drop_nlink(inode);
	} else
		error = NFS_PROTO(dir)->remove(dir, dentry);
	if (error == -ENOENT)
		nfs_dentry_handle_enoent(dentry);
	trace_nfs_remove_exit(dir, dentry, error);
out:
	return error;
}

 
int nfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int error;

	dfprintk(VFS, "NFS: unlink(%s/%lu, %pd)\n", dir->i_sb->s_id,
		dir->i_ino, dentry);

	trace_nfs_unlink_enter(dir, dentry);
	spin_lock(&dentry->d_lock);
	if (d_count(dentry) > 1 && !test_bit(NFS_INO_PRESERVE_UNLINKED,
					     &NFS_I(d_inode(dentry))->flags)) {
		spin_unlock(&dentry->d_lock);
		 
		write_inode_now(d_inode(dentry), 0);
		error = nfs_sillyrename(dir, dentry);
		goto out;
	}
	 
	error = -ETXTBSY;
	if (WARN_ON(dentry->d_flags & DCACHE_NFSFS_RENAMED) ||
	    WARN_ON(dentry->d_fsdata == NFS_FSDATA_BLOCKED)) {
		spin_unlock(&dentry->d_lock);
		goto out;
	}
	 
	kfree(dentry->d_fsdata);
	dentry->d_fsdata = NFS_FSDATA_BLOCKED;

	spin_unlock(&dentry->d_lock);
	error = nfs_safe_remove(dentry);
	nfs_dentry_remove_handle_error(dir, dentry, error);
	dentry->d_fsdata = NULL;
	wake_up_var(&dentry->d_fsdata);
out:
	trace_nfs_unlink_exit(dir, dentry, error);
	return error;
}
EXPORT_SYMBOL_GPL(nfs_unlink);

 
int nfs_symlink(struct mnt_idmap *idmap, struct inode *dir,
		struct dentry *dentry, const char *symname)
{
	struct page *page;
	char *kaddr;
	struct iattr attr;
	unsigned int pathlen = strlen(symname);
	int error;

	dfprintk(VFS, "NFS: symlink(%s/%lu, %pd, %s)\n", dir->i_sb->s_id,
		dir->i_ino, dentry, symname);

	if (pathlen > PAGE_SIZE)
		return -ENAMETOOLONG;

	attr.ia_mode = S_IFLNK | S_IRWXUGO;
	attr.ia_valid = ATTR_MODE;

	page = alloc_page(GFP_USER);
	if (!page)
		return -ENOMEM;

	kaddr = page_address(page);
	memcpy(kaddr, symname, pathlen);
	if (pathlen < PAGE_SIZE)
		memset(kaddr + pathlen, 0, PAGE_SIZE - pathlen);

	trace_nfs_symlink_enter(dir, dentry);
	error = NFS_PROTO(dir)->symlink(dir, dentry, page, pathlen, &attr);
	trace_nfs_symlink_exit(dir, dentry, error);
	if (error != 0) {
		dfprintk(VFS, "NFS: symlink(%s/%lu, %pd, %s) error %d\n",
			dir->i_sb->s_id, dir->i_ino,
			dentry, symname, error);
		d_drop(dentry);
		__free_page(page);
		return error;
	}

	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));

	 
	if (!add_to_page_cache_lru(page, d_inode(dentry)->i_mapping, 0,
							GFP_KERNEL)) {
		SetPageUptodate(page);
		unlock_page(page);
		 
		put_page(page);
	} else
		__free_page(page);

	return 0;
}
EXPORT_SYMBOL_GPL(nfs_symlink);

int
nfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(old_dentry);
	int error;

	dfprintk(VFS, "NFS: link(%pd2 -> %pd2)\n",
		old_dentry, dentry);

	trace_nfs_link_enter(inode, dir, dentry);
	d_drop(dentry);
	if (S_ISREG(inode->i_mode))
		nfs_sync_inode(inode);
	error = NFS_PROTO(dir)->link(inode, dir, &dentry->d_name);
	if (error == 0) {
		nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
		ihold(inode);
		d_add(dentry, inode);
	}
	trace_nfs_link_exit(inode, dir, dentry, error);
	return error;
}
EXPORT_SYMBOL_GPL(nfs_link);

static void
nfs_unblock_rename(struct rpc_task *task, struct nfs_renamedata *data)
{
	struct dentry *new_dentry = data->new_dentry;

	new_dentry->d_fsdata = NULL;
	wake_up_var(&new_dentry->d_fsdata);
}

 
int nfs_rename(struct mnt_idmap *idmap, struct inode *old_dir,
	       struct dentry *old_dentry, struct inode *new_dir,
	       struct dentry *new_dentry, unsigned int flags)
{
	struct inode *old_inode = d_inode(old_dentry);
	struct inode *new_inode = d_inode(new_dentry);
	struct dentry *dentry = NULL;
	struct rpc_task *task;
	bool must_unblock = false;
	int error = -EBUSY;

	if (flags)
		return -EINVAL;

	dfprintk(VFS, "NFS: rename(%pd2 -> %pd2, ct=%d)\n",
		 old_dentry, new_dentry,
		 d_count(new_dentry));

	trace_nfs_rename_enter(old_dir, old_dentry, new_dir, new_dentry);
	 
	if (new_inode && !S_ISDIR(new_inode->i_mode)) {
		 
		error = -ETXTBSY;
		if (WARN_ON(new_dentry->d_flags & DCACHE_NFSFS_RENAMED) ||
		    WARN_ON(new_dentry->d_fsdata == NFS_FSDATA_BLOCKED))
			goto out;
		if (new_dentry->d_fsdata) {
			 
			kfree(new_dentry->d_fsdata);
			new_dentry->d_fsdata = NULL;
		}

		spin_lock(&new_dentry->d_lock);
		if (d_count(new_dentry) > 2) {
			int err;

			spin_unlock(&new_dentry->d_lock);

			 
			dentry = d_alloc(new_dentry->d_parent,
					 &new_dentry->d_name);
			if (!dentry)
				goto out;

			 
			err = nfs_sillyrename(new_dir, new_dentry);
			if (err)
				goto out;

			new_dentry = dentry;
			new_inode = NULL;
		} else {
			new_dentry->d_fsdata = NFS_FSDATA_BLOCKED;
			must_unblock = true;
			spin_unlock(&new_dentry->d_lock);
		}

	}

	if (S_ISREG(old_inode->i_mode))
		nfs_sync_inode(old_inode);
	task = nfs_async_rename(old_dir, new_dir, old_dentry, new_dentry,
				must_unblock ? nfs_unblock_rename : NULL);
	if (IS_ERR(task)) {
		error = PTR_ERR(task);
		goto out;
	}

	error = rpc_wait_for_completion_task(task);
	if (error != 0) {
		((struct nfs_renamedata *)task->tk_calldata)->cancelled = 1;
		 
		smp_wmb();
	} else
		error = task->tk_status;
	rpc_put_task(task);
	 
	if (error == 0) {
		spin_lock(&old_inode->i_lock);
		NFS_I(old_inode)->attr_gencount = nfs_inc_attr_generation_counter();
		nfs_set_cache_invalid(old_inode, NFS_INO_INVALID_CHANGE |
							 NFS_INO_INVALID_CTIME |
							 NFS_INO_REVAL_FORCED);
		spin_unlock(&old_inode->i_lock);
	}
out:
	trace_nfs_rename_exit(old_dir, old_dentry,
			new_dir, new_dentry, error);
	if (!error) {
		if (new_inode != NULL)
			nfs_drop_nlink(new_inode);
		 
		d_move(old_dentry, new_dentry);
		nfs_set_verifier(old_dentry,
					nfs_save_change_attribute(new_dir));
	} else if (error == -ENOENT)
		nfs_dentry_handle_enoent(old_dentry);

	 
	if (dentry)
		dput(dentry);
	return error;
}
EXPORT_SYMBOL_GPL(nfs_rename);

static DEFINE_SPINLOCK(nfs_access_lru_lock);
static LIST_HEAD(nfs_access_lru_list);
static atomic_long_t nfs_access_nr_entries;

static unsigned long nfs_access_max_cachesize = 4*1024*1024;
module_param(nfs_access_max_cachesize, ulong, 0644);
MODULE_PARM_DESC(nfs_access_max_cachesize, "NFS access maximum total cache length");

static void nfs_access_free_entry(struct nfs_access_entry *entry)
{
	put_group_info(entry->group_info);
	kfree_rcu(entry, rcu_head);
	smp_mb__before_atomic();
	atomic_long_dec(&nfs_access_nr_entries);
	smp_mb__after_atomic();
}

static void nfs_access_free_list(struct list_head *head)
{
	struct nfs_access_entry *cache;

	while (!list_empty(head)) {
		cache = list_entry(head->next, struct nfs_access_entry, lru);
		list_del(&cache->lru);
		nfs_access_free_entry(cache);
	}
}

static unsigned long
nfs_do_access_cache_scan(unsigned int nr_to_scan)
{
	LIST_HEAD(head);
	struct nfs_inode *nfsi, *next;
	struct nfs_access_entry *cache;
	long freed = 0;

	spin_lock(&nfs_access_lru_lock);
	list_for_each_entry_safe(nfsi, next, &nfs_access_lru_list, access_cache_inode_lru) {
		struct inode *inode;

		if (nr_to_scan-- == 0)
			break;
		inode = &nfsi->vfs_inode;
		spin_lock(&inode->i_lock);
		if (list_empty(&nfsi->access_cache_entry_lru))
			goto remove_lru_entry;
		cache = list_entry(nfsi->access_cache_entry_lru.next,
				struct nfs_access_entry, lru);
		list_move(&cache->lru, &head);
		rb_erase(&cache->rb_node, &nfsi->access_cache);
		freed++;
		if (!list_empty(&nfsi->access_cache_entry_lru))
			list_move_tail(&nfsi->access_cache_inode_lru,
					&nfs_access_lru_list);
		else {
remove_lru_entry:
			list_del_init(&nfsi->access_cache_inode_lru);
			smp_mb__before_atomic();
			clear_bit(NFS_INO_ACL_LRU_SET, &nfsi->flags);
			smp_mb__after_atomic();
		}
		spin_unlock(&inode->i_lock);
	}
	spin_unlock(&nfs_access_lru_lock);
	nfs_access_free_list(&head);
	return freed;
}

unsigned long
nfs_access_cache_scan(struct shrinker *shrink, struct shrink_control *sc)
{
	int nr_to_scan = sc->nr_to_scan;
	gfp_t gfp_mask = sc->gfp_mask;

	if ((gfp_mask & GFP_KERNEL) != GFP_KERNEL)
		return SHRINK_STOP;
	return nfs_do_access_cache_scan(nr_to_scan);
}


unsigned long
nfs_access_cache_count(struct shrinker *shrink, struct shrink_control *sc)
{
	return vfs_pressure_ratio(atomic_long_read(&nfs_access_nr_entries));
}

static void
nfs_access_cache_enforce_limit(void)
{
	long nr_entries = atomic_long_read(&nfs_access_nr_entries);
	unsigned long diff;
	unsigned int nr_to_scan;

	if (nr_entries < 0 || nr_entries <= nfs_access_max_cachesize)
		return;
	nr_to_scan = 100;
	diff = nr_entries - nfs_access_max_cachesize;
	if (diff < nr_to_scan)
		nr_to_scan = diff;
	nfs_do_access_cache_scan(nr_to_scan);
}

static void __nfs_access_zap_cache(struct nfs_inode *nfsi, struct list_head *head)
{
	struct rb_root *root_node = &nfsi->access_cache;
	struct rb_node *n;
	struct nfs_access_entry *entry;

	 
	while ((n = rb_first(root_node)) != NULL) {
		entry = rb_entry(n, struct nfs_access_entry, rb_node);
		rb_erase(n, root_node);
		list_move(&entry->lru, head);
	}
	nfsi->cache_validity &= ~NFS_INO_INVALID_ACCESS;
}

void nfs_access_zap_cache(struct inode *inode)
{
	LIST_HEAD(head);

	if (test_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags) == 0)
		return;
	 
	spin_lock(&nfs_access_lru_lock);
	if (test_and_clear_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags))
		list_del_init(&NFS_I(inode)->access_cache_inode_lru);

	spin_lock(&inode->i_lock);
	__nfs_access_zap_cache(NFS_I(inode), &head);
	spin_unlock(&inode->i_lock);
	spin_unlock(&nfs_access_lru_lock);
	nfs_access_free_list(&head);
}
EXPORT_SYMBOL_GPL(nfs_access_zap_cache);

static int access_cmp(const struct cred *a, const struct nfs_access_entry *b)
{
	struct group_info *ga, *gb;
	int g;

	if (uid_lt(a->fsuid, b->fsuid))
		return -1;
	if (uid_gt(a->fsuid, b->fsuid))
		return 1;

	if (gid_lt(a->fsgid, b->fsgid))
		return -1;
	if (gid_gt(a->fsgid, b->fsgid))
		return 1;

	ga = a->group_info;
	gb = b->group_info;
	if (ga == gb)
		return 0;
	if (ga == NULL)
		return -1;
	if (gb == NULL)
		return 1;
	if (ga->ngroups < gb->ngroups)
		return -1;
	if (ga->ngroups > gb->ngroups)
		return 1;

	for (g = 0; g < ga->ngroups; g++) {
		if (gid_lt(ga->gid[g], gb->gid[g]))
			return -1;
		if (gid_gt(ga->gid[g], gb->gid[g]))
			return 1;
	}
	return 0;
}

static struct nfs_access_entry *nfs_access_search_rbtree(struct inode *inode, const struct cred *cred)
{
	struct rb_node *n = NFS_I(inode)->access_cache.rb_node;

	while (n != NULL) {
		struct nfs_access_entry *entry =
			rb_entry(n, struct nfs_access_entry, rb_node);
		int cmp = access_cmp(cred, entry);

		if (cmp < 0)
			n = n->rb_left;
		else if (cmp > 0)
			n = n->rb_right;
		else
			return entry;
	}
	return NULL;
}

static u64 nfs_access_login_time(const struct task_struct *task,
				 const struct cred *cred)
{
	const struct task_struct *parent;
	const struct cred *pcred;
	u64 ret;

	rcu_read_lock();
	for (;;) {
		parent = rcu_dereference(task->real_parent);
		pcred = __task_cred(parent);
		if (parent == task || cred_fscmp(pcred, cred) != 0)
			break;
		task = parent;
	}
	ret = task->start_time;
	rcu_read_unlock();
	return ret;
}

static int nfs_access_get_cached_locked(struct inode *inode, const struct cred *cred, u32 *mask, bool may_block)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	u64 login_time = nfs_access_login_time(current, cred);
	struct nfs_access_entry *cache;
	bool retry = true;
	int err;

	spin_lock(&inode->i_lock);
	for(;;) {
		if (nfsi->cache_validity & NFS_INO_INVALID_ACCESS)
			goto out_zap;
		cache = nfs_access_search_rbtree(inode, cred);
		err = -ENOENT;
		if (cache == NULL)
			goto out;
		 
		if (!nfs_check_cache_invalid(inode, NFS_INO_INVALID_ACCESS))
			break;
		if (!retry)
			break;
		err = -ECHILD;
		if (!may_block)
			goto out;
		spin_unlock(&inode->i_lock);
		err = __nfs_revalidate_inode(NFS_SERVER(inode), inode);
		if (err)
			return err;
		spin_lock(&inode->i_lock);
		retry = false;
	}
	err = -ENOENT;
	if ((s64)(login_time - cache->timestamp) > 0)
		goto out;
	*mask = cache->mask;
	list_move_tail(&cache->lru, &nfsi->access_cache_entry_lru);
	err = 0;
out:
	spin_unlock(&inode->i_lock);
	return err;
out_zap:
	spin_unlock(&inode->i_lock);
	nfs_access_zap_cache(inode);
	return -ENOENT;
}

static int nfs_access_get_cached_rcu(struct inode *inode, const struct cred *cred, u32 *mask)
{
	 
	struct nfs_inode *nfsi = NFS_I(inode);
	u64 login_time = nfs_access_login_time(current, cred);
	struct nfs_access_entry *cache;
	int err = -ECHILD;
	struct list_head *lh;

	rcu_read_lock();
	if (nfsi->cache_validity & NFS_INO_INVALID_ACCESS)
		goto out;
	lh = rcu_dereference(list_tail_rcu(&nfsi->access_cache_entry_lru));
	cache = list_entry(lh, struct nfs_access_entry, lru);
	if (lh == &nfsi->access_cache_entry_lru ||
	    access_cmp(cred, cache) != 0)
		cache = NULL;
	if (cache == NULL)
		goto out;
	if ((s64)(login_time - cache->timestamp) > 0)
		goto out;
	if (nfs_check_cache_invalid(inode, NFS_INO_INVALID_ACCESS))
		goto out;
	*mask = cache->mask;
	err = 0;
out:
	rcu_read_unlock();
	return err;
}

int nfs_access_get_cached(struct inode *inode, const struct cred *cred,
			  u32 *mask, bool may_block)
{
	int status;

	status = nfs_access_get_cached_rcu(inode, cred, mask);
	if (status != 0)
		status = nfs_access_get_cached_locked(inode, cred, mask,
		    may_block);

	return status;
}
EXPORT_SYMBOL_GPL(nfs_access_get_cached);

static void nfs_access_add_rbtree(struct inode *inode,
				  struct nfs_access_entry *set,
				  const struct cred *cred)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	struct rb_root *root_node = &nfsi->access_cache;
	struct rb_node **p = &root_node->rb_node;
	struct rb_node *parent = NULL;
	struct nfs_access_entry *entry;
	int cmp;

	spin_lock(&inode->i_lock);
	while (*p != NULL) {
		parent = *p;
		entry = rb_entry(parent, struct nfs_access_entry, rb_node);
		cmp = access_cmp(cred, entry);

		if (cmp < 0)
			p = &parent->rb_left;
		else if (cmp > 0)
			p = &parent->rb_right;
		else
			goto found;
	}
	rb_link_node(&set->rb_node, parent, p);
	rb_insert_color(&set->rb_node, root_node);
	list_add_tail(&set->lru, &nfsi->access_cache_entry_lru);
	spin_unlock(&inode->i_lock);
	return;
found:
	rb_replace_node(parent, &set->rb_node, root_node);
	list_add_tail(&set->lru, &nfsi->access_cache_entry_lru);
	list_del(&entry->lru);
	spin_unlock(&inode->i_lock);
	nfs_access_free_entry(entry);
}

void nfs_access_add_cache(struct inode *inode, struct nfs_access_entry *set,
			  const struct cred *cred)
{
	struct nfs_access_entry *cache = kmalloc(sizeof(*cache), GFP_KERNEL);
	if (cache == NULL)
		return;
	RB_CLEAR_NODE(&cache->rb_node);
	cache->fsuid = cred->fsuid;
	cache->fsgid = cred->fsgid;
	cache->group_info = get_group_info(cred->group_info);
	cache->mask = set->mask;
	cache->timestamp = ktime_get_ns();

	 
	smp_wmb();
	nfs_access_add_rbtree(inode, cache, cred);

	 
	smp_mb__before_atomic();
	atomic_long_inc(&nfs_access_nr_entries);
	smp_mb__after_atomic();

	 
	if (!test_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags)) {
		spin_lock(&nfs_access_lru_lock);
		if (!test_and_set_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags))
			list_add_tail(&NFS_I(inode)->access_cache_inode_lru,
					&nfs_access_lru_list);
		spin_unlock(&nfs_access_lru_lock);
	}
	nfs_access_cache_enforce_limit();
}
EXPORT_SYMBOL_GPL(nfs_access_add_cache);

#define NFS_MAY_READ (NFS_ACCESS_READ)
#define NFS_MAY_WRITE (NFS_ACCESS_MODIFY | \
		NFS_ACCESS_EXTEND | \
		NFS_ACCESS_DELETE)
#define NFS_FILE_MAY_WRITE (NFS_ACCESS_MODIFY | \
		NFS_ACCESS_EXTEND)
#define NFS_DIR_MAY_WRITE NFS_MAY_WRITE
#define NFS_MAY_LOOKUP (NFS_ACCESS_LOOKUP)
#define NFS_MAY_EXECUTE (NFS_ACCESS_EXECUTE)
static int
nfs_access_calc_mask(u32 access_result, umode_t umode)
{
	int mask = 0;

	if (access_result & NFS_MAY_READ)
		mask |= MAY_READ;
	if (S_ISDIR(umode)) {
		if ((access_result & NFS_DIR_MAY_WRITE) == NFS_DIR_MAY_WRITE)
			mask |= MAY_WRITE;
		if ((access_result & NFS_MAY_LOOKUP) == NFS_MAY_LOOKUP)
			mask |= MAY_EXEC;
	} else if (S_ISREG(umode)) {
		if ((access_result & NFS_FILE_MAY_WRITE) == NFS_FILE_MAY_WRITE)
			mask |= MAY_WRITE;
		if ((access_result & NFS_MAY_EXECUTE) == NFS_MAY_EXECUTE)
			mask |= MAY_EXEC;
	} else if (access_result & NFS_MAY_WRITE)
			mask |= MAY_WRITE;
	return mask;
}

void nfs_access_set_mask(struct nfs_access_entry *entry, u32 access_result)
{
	entry->mask = access_result;
}
EXPORT_SYMBOL_GPL(nfs_access_set_mask);

static int nfs_do_access(struct inode *inode, const struct cred *cred, int mask)
{
	struct nfs_access_entry cache;
	bool may_block = (mask & MAY_NOT_BLOCK) == 0;
	int cache_mask = -1;
	int status;

	trace_nfs_access_enter(inode);

	status = nfs_access_get_cached(inode, cred, &cache.mask, may_block);
	if (status == 0)
		goto out_cached;

	status = -ECHILD;
	if (!may_block)
		goto out;

	 
	cache.mask = NFS_ACCESS_READ | NFS_ACCESS_MODIFY | NFS_ACCESS_EXTEND |
		     nfs_access_xattr_mask(NFS_SERVER(inode));
	if (S_ISDIR(inode->i_mode))
		cache.mask |= NFS_ACCESS_DELETE | NFS_ACCESS_LOOKUP;
	else
		cache.mask |= NFS_ACCESS_EXECUTE;
	status = NFS_PROTO(inode)->access(inode, &cache, cred);
	if (status != 0) {
		if (status == -ESTALE) {
			if (!S_ISDIR(inode->i_mode))
				nfs_set_inode_stale(inode);
			else
				nfs_zap_caches(inode);
		}
		goto out;
	}
	nfs_access_add_cache(inode, &cache, cred);
out_cached:
	cache_mask = nfs_access_calc_mask(cache.mask, inode->i_mode);
	if ((mask & ~cache_mask & (MAY_READ | MAY_WRITE | MAY_EXEC)) != 0)
		status = -EACCES;
out:
	trace_nfs_access_exit(inode, mask, cache_mask, status);
	return status;
}

static int nfs_open_permission_mask(int openflags)
{
	int mask = 0;

	if (openflags & __FMODE_EXEC) {
		 
		mask = MAY_EXEC;
	} else {
		if ((openflags & O_ACCMODE) != O_WRONLY)
			mask |= MAY_READ;
		if ((openflags & O_ACCMODE) != O_RDONLY)
			mask |= MAY_WRITE;
	}

	return mask;
}

int nfs_may_open(struct inode *inode, const struct cred *cred, int openflags)
{
	return nfs_do_access(inode, cred, nfs_open_permission_mask(openflags));
}
EXPORT_SYMBOL_GPL(nfs_may_open);

static int nfs_execute_ok(struct inode *inode, int mask)
{
	struct nfs_server *server = NFS_SERVER(inode);
	int ret = 0;

	if (S_ISDIR(inode->i_mode))
		return 0;
	if (nfs_check_cache_invalid(inode, NFS_INO_INVALID_MODE)) {
		if (mask & MAY_NOT_BLOCK)
			return -ECHILD;
		ret = __nfs_revalidate_inode(server, inode);
	}
	if (ret == 0 && !execute_ok(inode))
		ret = -EACCES;
	return ret;
}

int nfs_permission(struct mnt_idmap *idmap,
		   struct inode *inode,
		   int mask)
{
	const struct cred *cred = current_cred();
	int res = 0;

	nfs_inc_stats(inode, NFSIOS_VFSACCESS);

	if ((mask & (MAY_READ | MAY_WRITE | MAY_EXEC)) == 0)
		goto out;
	 
	if (mask & (MAY_ACCESS | MAY_CHDIR))
		goto force_lookup;

	switch (inode->i_mode & S_IFMT) {
		case S_IFLNK:
			goto out;
		case S_IFREG:
			if ((mask & MAY_OPEN) &&
			   nfs_server_capable(inode, NFS_CAP_ATOMIC_OPEN))
				return 0;
			break;
		case S_IFDIR:
			 
			if ((mask & MAY_WRITE) && !(mask & MAY_READ))
				goto out;
	}

force_lookup:
	if (!NFS_PROTO(inode)->access)
		goto out_notsup;

	res = nfs_do_access(inode, cred, mask);
out:
	if (!res && (mask & MAY_EXEC))
		res = nfs_execute_ok(inode, mask);

	dfprintk(VFS, "NFS: permission(%s/%lu), mask=0x%x, res=%d\n",
		inode->i_sb->s_id, inode->i_ino, mask, res);
	return res;
out_notsup:
	if (mask & MAY_NOT_BLOCK)
		return -ECHILD;

	res = nfs_revalidate_inode(inode, NFS_INO_INVALID_MODE |
						  NFS_INO_INVALID_OTHER);
	if (res == 0)
		res = generic_permission(&nop_mnt_idmap, inode, mask);
	goto out;
}
EXPORT_SYMBOL_GPL(nfs_permission);
