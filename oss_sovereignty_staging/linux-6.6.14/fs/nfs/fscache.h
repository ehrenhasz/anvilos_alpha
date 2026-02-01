 
 

#ifndef _NFS_FSCACHE_H
#define _NFS_FSCACHE_H

#include <linux/swap.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/nfs4_mount.h>
#include <linux/fscache.h>
#include <linux/iversion.h>

#ifdef CONFIG_NFS_FSCACHE

 
struct nfs_fscache_inode_auxdata {
	s64	mtime_sec;
	s64	mtime_nsec;
	s64	ctime_sec;
	s64	ctime_nsec;
	u64	change_attr;
};

struct nfs_netfs_io_data {
	 
	refcount_t			refcount;
	struct netfs_io_subrequest	*sreq;

	 
	atomic64_t	transferred;
	int		error;
};

static inline void nfs_netfs_get(struct nfs_netfs_io_data *netfs)
{
	refcount_inc(&netfs->refcount);
}

static inline void nfs_netfs_put(struct nfs_netfs_io_data *netfs)
{
	ssize_t final_len;

	 
	if (!refcount_dec_and_test(&netfs->refcount))
		return;

	 
	final_len = min_t(s64, netfs->sreq->len, atomic64_read(&netfs->transferred));
	netfs_subreq_terminated(netfs->sreq, netfs->error ?: final_len, false);
	kfree(netfs);
}
static inline void nfs_netfs_inode_init(struct nfs_inode *nfsi)
{
	netfs_inode_init(&nfsi->netfs, &nfs_netfs_ops);
}
extern void nfs_netfs_initiate_read(struct nfs_pgio_header *hdr);
extern void nfs_netfs_read_completion(struct nfs_pgio_header *hdr);
extern int nfs_netfs_folio_unlock(struct folio *folio);

 
extern int nfs_fscache_get_super_cookie(struct super_block *, const char *, int);
extern void nfs_fscache_release_super_cookie(struct super_block *);

extern void nfs_fscache_init_inode(struct inode *);
extern void nfs_fscache_clear_inode(struct inode *);
extern void nfs_fscache_open_file(struct inode *, struct file *);
extern void nfs_fscache_release_file(struct inode *, struct file *);
extern int nfs_netfs_readahead(struct readahead_control *ractl);
extern int nfs_netfs_read_folio(struct file *file, struct folio *folio);

static inline bool nfs_fscache_release_folio(struct folio *folio, gfp_t gfp)
{
	if (folio_test_fscache(folio)) {
		if (current_is_kswapd() || !(gfp & __GFP_FS))
			return false;
		folio_wait_fscache(folio);
	}
	fscache_note_page_release(netfs_i_cookie(netfs_inode(folio->mapping->host)));
	return true;
}

static inline void nfs_fscache_update_auxdata(struct nfs_fscache_inode_auxdata *auxdata,
					      struct inode *inode)
{
	memset(auxdata, 0, sizeof(*auxdata));
	auxdata->mtime_sec  = inode->i_mtime.tv_sec;
	auxdata->mtime_nsec = inode->i_mtime.tv_nsec;
	auxdata->ctime_sec  = inode_get_ctime(inode).tv_sec;
	auxdata->ctime_nsec = inode_get_ctime(inode).tv_nsec;

	if (NFS_SERVER(inode)->nfs_client->rpc_ops->version == 4)
		auxdata->change_attr = inode_peek_iversion_raw(inode);
}

 
static inline void nfs_fscache_invalidate(struct inode *inode, int flags)
{
	struct nfs_fscache_inode_auxdata auxdata;
	struct fscache_cookie *cookie =  netfs_i_cookie(&NFS_I(inode)->netfs);

	nfs_fscache_update_auxdata(&auxdata, inode);
	fscache_invalidate(cookie, &auxdata, i_size_read(inode), flags);
}

 
static inline const char *nfs_server_fscache_state(struct nfs_server *server)
{
	if (server->fscache)
		return "yes";
	return "no ";
}

static inline void nfs_netfs_set_pgio_header(struct nfs_pgio_header *hdr,
					     struct nfs_pageio_descriptor *desc)
{
	hdr->netfs = desc->pg_netfs;
}
static inline void nfs_netfs_set_pageio_descriptor(struct nfs_pageio_descriptor *desc,
						   struct nfs_pgio_header *hdr)
{
	desc->pg_netfs = hdr->netfs;
}
static inline void nfs_netfs_reset_pageio_descriptor(struct nfs_pageio_descriptor *desc)
{
	desc->pg_netfs = NULL;
}
#else  
static inline void nfs_netfs_inode_init(struct nfs_inode *nfsi) {}
static inline void nfs_netfs_initiate_read(struct nfs_pgio_header *hdr) {}
static inline void nfs_netfs_read_completion(struct nfs_pgio_header *hdr) {}
static inline int nfs_netfs_folio_unlock(struct folio *folio)
{
	return 1;
}
static inline void nfs_fscache_release_super_cookie(struct super_block *sb) {}

static inline void nfs_fscache_init_inode(struct inode *inode) {}
static inline void nfs_fscache_clear_inode(struct inode *inode) {}
static inline void nfs_fscache_open_file(struct inode *inode,
					 struct file *filp) {}
static inline void nfs_fscache_release_file(struct inode *inode, struct file *file) {}
static inline int nfs_netfs_readahead(struct readahead_control *ractl)
{
	return -ENOBUFS;
}
static inline int nfs_netfs_read_folio(struct file *file, struct folio *folio)
{
	return -ENOBUFS;
}

static inline bool nfs_fscache_release_folio(struct folio *folio, gfp_t gfp)
{
	return true;  
}
static inline void nfs_fscache_invalidate(struct inode *inode, int flags) {}

static inline const char *nfs_server_fscache_state(struct nfs_server *server)
{
	return "no ";
}
static inline void nfs_netfs_set_pgio_header(struct nfs_pgio_header *hdr,
					     struct nfs_pageio_descriptor *desc) {}
static inline void nfs_netfs_set_pageio_descriptor(struct nfs_pageio_descriptor *desc,
						   struct nfs_pgio_header *hdr) {}
static inline void nfs_netfs_reset_pageio_descriptor(struct nfs_pageio_descriptor *desc) {}
#endif  
#endif  
