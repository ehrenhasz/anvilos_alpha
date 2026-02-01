
 

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/module.h>

#include <linux/nfs_fs.h>
#include <linux/nfs_page.h>
#include <linux/sunrpc/clnt.h>

#include <linux/uaccess.h>
#include <linux/atomic.h>

#include "internal.h"
#include "iostat.h"
#include "pnfs.h"
#include "fscache.h"
#include "nfstrace.h"

#define NFSDBG_FACILITY		NFSDBG_VFS

static struct kmem_cache *nfs_direct_cachep;

static const struct nfs_pgio_completion_ops nfs_direct_write_completion_ops;
static const struct nfs_commit_completion_ops nfs_direct_commit_completion_ops;
static void nfs_direct_write_complete(struct nfs_direct_req *dreq);
static void nfs_direct_write_schedule_work(struct work_struct *work);

static inline void get_dreq(struct nfs_direct_req *dreq)
{
	atomic_inc(&dreq->io_count);
}

static inline int put_dreq(struct nfs_direct_req *dreq)
{
	return atomic_dec_and_test(&dreq->io_count);
}

static void
nfs_direct_handle_truncated(struct nfs_direct_req *dreq,
			    const struct nfs_pgio_header *hdr,
			    ssize_t dreq_len)
{
	if (!(test_bit(NFS_IOHDR_ERROR, &hdr->flags) ||
	      test_bit(NFS_IOHDR_EOF, &hdr->flags)))
		return;
	if (dreq->max_count >= dreq_len) {
		dreq->max_count = dreq_len;
		if (dreq->count > dreq_len)
			dreq->count = dreq_len;
	}

	if (test_bit(NFS_IOHDR_ERROR, &hdr->flags) && !dreq->error)
		dreq->error = hdr->error;
}

static void
nfs_direct_count_bytes(struct nfs_direct_req *dreq,
		       const struct nfs_pgio_header *hdr)
{
	loff_t hdr_end = hdr->io_start + hdr->good_bytes;
	ssize_t dreq_len = 0;

	if (hdr_end > dreq->io_start)
		dreq_len = hdr_end - dreq->io_start;

	nfs_direct_handle_truncated(dreq, hdr, dreq_len);

	if (dreq_len > dreq->max_count)
		dreq_len = dreq->max_count;

	if (dreq->count < dreq_len)
		dreq->count = dreq_len;
}

static void nfs_direct_truncate_request(struct nfs_direct_req *dreq,
					struct nfs_page *req)
{
	loff_t offs = req_offset(req);
	size_t req_start = (size_t)(offs - dreq->io_start);

	if (req_start < dreq->max_count)
		dreq->max_count = req_start;
	if (req_start < dreq->count)
		dreq->count = req_start;
}

 
int nfs_swap_rw(struct kiocb *iocb, struct iov_iter *iter)
{
	ssize_t ret;

	VM_BUG_ON(iov_iter_count(iter) != PAGE_SIZE);

	if (iov_iter_rw(iter) == READ)
		ret = nfs_file_direct_read(iocb, iter, true);
	else
		ret = nfs_file_direct_write(iocb, iter, true);
	if (ret < 0)
		return ret;
	return 0;
}

static void nfs_direct_release_pages(struct page **pages, unsigned int npages)
{
	unsigned int i;
	for (i = 0; i < npages; i++)
		put_page(pages[i]);
}

void nfs_init_cinfo_from_dreq(struct nfs_commit_info *cinfo,
			      struct nfs_direct_req *dreq)
{
	cinfo->inode = dreq->inode;
	cinfo->mds = &dreq->mds_cinfo;
	cinfo->ds = &dreq->ds_cinfo;
	cinfo->dreq = dreq;
	cinfo->completion_ops = &nfs_direct_commit_completion_ops;
}

static inline struct nfs_direct_req *nfs_direct_req_alloc(void)
{
	struct nfs_direct_req *dreq;

	dreq = kmem_cache_zalloc(nfs_direct_cachep, GFP_KERNEL);
	if (!dreq)
		return NULL;

	kref_init(&dreq->kref);
	kref_get(&dreq->kref);
	init_completion(&dreq->completion);
	INIT_LIST_HEAD(&dreq->mds_cinfo.list);
	pnfs_init_ds_commit_info(&dreq->ds_cinfo);
	INIT_WORK(&dreq->work, nfs_direct_write_schedule_work);
	spin_lock_init(&dreq->lock);

	return dreq;
}

static void nfs_direct_req_free(struct kref *kref)
{
	struct nfs_direct_req *dreq = container_of(kref, struct nfs_direct_req, kref);

	pnfs_release_ds_info(&dreq->ds_cinfo, dreq->inode);
	if (dreq->l_ctx != NULL)
		nfs_put_lock_context(dreq->l_ctx);
	if (dreq->ctx != NULL)
		put_nfs_open_context(dreq->ctx);
	kmem_cache_free(nfs_direct_cachep, dreq);
}

static void nfs_direct_req_release(struct nfs_direct_req *dreq)
{
	kref_put(&dreq->kref, nfs_direct_req_free);
}

ssize_t nfs_dreq_bytes_left(struct nfs_direct_req *dreq, loff_t offset)
{
	loff_t start = offset - dreq->io_start;
	return dreq->max_count - start;
}
EXPORT_SYMBOL_GPL(nfs_dreq_bytes_left);

 
static ssize_t nfs_direct_wait(struct nfs_direct_req *dreq)
{
	ssize_t result = -EIOCBQUEUED;

	 
	if (dreq->iocb)
		goto out;

	result = wait_for_completion_killable(&dreq->completion);

	if (!result) {
		result = dreq->count;
		WARN_ON_ONCE(dreq->count < 0);
	}
	if (!result)
		result = dreq->error;

out:
	return (ssize_t) result;
}

 
static void nfs_direct_complete(struct nfs_direct_req *dreq)
{
	struct inode *inode = dreq->inode;

	inode_dio_end(inode);

	if (dreq->iocb) {
		long res = (long) dreq->error;
		if (dreq->count != 0) {
			res = (long) dreq->count;
			WARN_ON_ONCE(dreq->count < 0);
		}
		dreq->iocb->ki_complete(dreq->iocb, res);
	}

	complete(&dreq->completion);

	nfs_direct_req_release(dreq);
}

static void nfs_direct_read_completion(struct nfs_pgio_header *hdr)
{
	unsigned long bytes = 0;
	struct nfs_direct_req *dreq = hdr->dreq;

	spin_lock(&dreq->lock);
	if (test_bit(NFS_IOHDR_REDO, &hdr->flags)) {
		spin_unlock(&dreq->lock);
		goto out_put;
	}

	nfs_direct_count_bytes(dreq, hdr);
	spin_unlock(&dreq->lock);

	while (!list_empty(&hdr->pages)) {
		struct nfs_page *req = nfs_list_entry(hdr->pages.next);
		struct page *page = req->wb_page;

		if (!PageCompound(page) && bytes < hdr->good_bytes &&
		    (dreq->flags == NFS_ODIRECT_SHOULD_DIRTY))
			set_page_dirty(page);
		bytes += req->wb_bytes;
		nfs_list_remove_request(req);
		nfs_release_request(req);
	}
out_put:
	if (put_dreq(dreq))
		nfs_direct_complete(dreq);
	hdr->release(hdr);
}

static void nfs_read_sync_pgio_error(struct list_head *head, int error)
{
	struct nfs_page *req;

	while (!list_empty(head)) {
		req = nfs_list_entry(head->next);
		nfs_list_remove_request(req);
		nfs_release_request(req);
	}
}

static void nfs_direct_pgio_init(struct nfs_pgio_header *hdr)
{
	get_dreq(hdr->dreq);
}

static const struct nfs_pgio_completion_ops nfs_direct_read_completion_ops = {
	.error_cleanup = nfs_read_sync_pgio_error,
	.init_hdr = nfs_direct_pgio_init,
	.completion = nfs_direct_read_completion,
};

 

static ssize_t nfs_direct_read_schedule_iovec(struct nfs_direct_req *dreq,
					      struct iov_iter *iter,
					      loff_t pos)
{
	struct nfs_pageio_descriptor desc;
	struct inode *inode = dreq->inode;
	ssize_t result = -EINVAL;
	size_t requested_bytes = 0;
	size_t rsize = max_t(size_t, NFS_SERVER(inode)->rsize, PAGE_SIZE);

	nfs_pageio_init_read(&desc, dreq->inode, false,
			     &nfs_direct_read_completion_ops);
	get_dreq(dreq);
	desc.pg_dreq = dreq;
	inode_dio_begin(inode);

	while (iov_iter_count(iter)) {
		struct page **pagevec;
		size_t bytes;
		size_t pgbase;
		unsigned npages, i;

		result = iov_iter_get_pages_alloc2(iter, &pagevec,
						  rsize, &pgbase);
		if (result < 0)
			break;
	
		bytes = result;
		npages = (result + pgbase + PAGE_SIZE - 1) / PAGE_SIZE;
		for (i = 0; i < npages; i++) {
			struct nfs_page *req;
			unsigned int req_len = min_t(size_t, bytes, PAGE_SIZE - pgbase);
			 
			req = nfs_page_create_from_page(dreq->ctx, pagevec[i],
							pgbase, pos, req_len);
			if (IS_ERR(req)) {
				result = PTR_ERR(req);
				break;
			}
			if (!nfs_pageio_add_request(&desc, req)) {
				result = desc.pg_error;
				nfs_release_request(req);
				break;
			}
			pgbase = 0;
			bytes -= req_len;
			requested_bytes += req_len;
			pos += req_len;
			dreq->bytes_left -= req_len;
		}
		nfs_direct_release_pages(pagevec, npages);
		kvfree(pagevec);
		if (result < 0)
			break;
	}

	nfs_pageio_complete(&desc);

	 
	if (requested_bytes == 0) {
		inode_dio_end(inode);
		nfs_direct_req_release(dreq);
		return result < 0 ? result : -EIO;
	}

	if (put_dreq(dreq))
		nfs_direct_complete(dreq);
	return requested_bytes;
}

 
ssize_t nfs_file_direct_read(struct kiocb *iocb, struct iov_iter *iter,
			     bool swap)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	struct nfs_direct_req *dreq;
	struct nfs_lock_context *l_ctx;
	ssize_t result, requested;
	size_t count = iov_iter_count(iter);
	nfs_add_stats(mapping->host, NFSIOS_DIRECTREADBYTES, count);

	dfprintk(FILE, "NFS: direct read(%pD2, %zd@%Ld)\n",
		file, count, (long long) iocb->ki_pos);

	result = 0;
	if (!count)
		goto out;

	task_io_account_read(count);

	result = -ENOMEM;
	dreq = nfs_direct_req_alloc();
	if (dreq == NULL)
		goto out;

	dreq->inode = inode;
	dreq->bytes_left = dreq->max_count = count;
	dreq->io_start = iocb->ki_pos;
	dreq->ctx = get_nfs_open_context(nfs_file_open_context(iocb->ki_filp));
	l_ctx = nfs_get_lock_context(dreq->ctx);
	if (IS_ERR(l_ctx)) {
		result = PTR_ERR(l_ctx);
		nfs_direct_req_release(dreq);
		goto out_release;
	}
	dreq->l_ctx = l_ctx;
	if (!is_sync_kiocb(iocb))
		dreq->iocb = iocb;

	if (user_backed_iter(iter))
		dreq->flags = NFS_ODIRECT_SHOULD_DIRTY;

	if (!swap)
		nfs_start_io_direct(inode);

	NFS_I(inode)->read_io += count;
	requested = nfs_direct_read_schedule_iovec(dreq, iter, iocb->ki_pos);

	if (!swap)
		nfs_end_io_direct(inode);

	if (requested > 0) {
		result = nfs_direct_wait(dreq);
		if (result > 0) {
			requested -= result;
			iocb->ki_pos += result;
		}
		iov_iter_revert(iter, requested);
	} else {
		result = requested;
	}

out_release:
	nfs_direct_req_release(dreq);
out:
	return result;
}

static void nfs_direct_add_page_head(struct list_head *list,
				     struct nfs_page *req)
{
	struct nfs_page *head = req->wb_head;

	if (!list_empty(&head->wb_list) || !nfs_lock_request(head))
		return;
	if (!list_empty(&head->wb_list)) {
		nfs_unlock_request(head);
		return;
	}
	list_add(&head->wb_list, list);
	kref_get(&head->wb_kref);
	kref_get(&head->wb_kref);
}

static void nfs_direct_join_group(struct list_head *list,
				  struct nfs_commit_info *cinfo,
				  struct inode *inode)
{
	struct nfs_page *req, *subreq;

	list_for_each_entry(req, list, wb_list) {
		if (req->wb_head != req) {
			nfs_direct_add_page_head(&req->wb_list, req);
			continue;
		}
		subreq = req->wb_this_page;
		if (subreq == req)
			continue;
		do {
			 
			if (!list_empty(&subreq->wb_list)) {
				nfs_list_remove_request(subreq);
				nfs_release_request(subreq);
			}
		} while ((subreq = subreq->wb_this_page) != req);
		nfs_join_page_group(req, cinfo, inode);
	}
}

static void
nfs_direct_write_scan_commit_list(struct inode *inode,
				  struct list_head *list,
				  struct nfs_commit_info *cinfo)
{
	mutex_lock(&NFS_I(cinfo->inode)->commit_mutex);
	pnfs_recover_commit_reqs(list, cinfo);
	nfs_scan_commit_list(&cinfo->mds->list, list, cinfo, 0);
	mutex_unlock(&NFS_I(cinfo->inode)->commit_mutex);
}

static void nfs_direct_write_reschedule(struct nfs_direct_req *dreq)
{
	struct nfs_pageio_descriptor desc;
	struct nfs_page *req;
	LIST_HEAD(reqs);
	struct nfs_commit_info cinfo;

	nfs_init_cinfo_from_dreq(&cinfo, dreq);
	nfs_direct_write_scan_commit_list(dreq->inode, &reqs, &cinfo);

	nfs_direct_join_group(&reqs, &cinfo, dreq->inode);

	nfs_clear_pnfs_ds_commit_verifiers(&dreq->ds_cinfo);
	get_dreq(dreq);

	nfs_pageio_init_write(&desc, dreq->inode, FLUSH_STABLE, false,
			      &nfs_direct_write_completion_ops);
	desc.pg_dreq = dreq;

	while (!list_empty(&reqs)) {
		req = nfs_list_entry(reqs.next);
		 
		req->wb_nio++;
		if (!nfs_pageio_add_request(&desc, req)) {
			spin_lock(&dreq->lock);
			if (dreq->error < 0) {
				desc.pg_error = dreq->error;
			} else if (desc.pg_error != -EAGAIN) {
				dreq->flags = 0;
				if (!desc.pg_error)
					desc.pg_error = -EIO;
				dreq->error = desc.pg_error;
			} else
				dreq->flags = NFS_ODIRECT_RESCHED_WRITES;
			spin_unlock(&dreq->lock);
			break;
		}
		nfs_release_request(req);
	}
	nfs_pageio_complete(&desc);

	while (!list_empty(&reqs)) {
		req = nfs_list_entry(reqs.next);
		nfs_list_remove_request(req);
		nfs_unlock_and_release_request(req);
		if (desc.pg_error == -EAGAIN) {
			nfs_mark_request_commit(req, NULL, &cinfo, 0);
		} else {
			spin_lock(&dreq->lock);
			nfs_direct_truncate_request(dreq, req);
			spin_unlock(&dreq->lock);
			nfs_release_request(req);
		}
	}

	if (put_dreq(dreq))
		nfs_direct_write_complete(dreq);
}

static void nfs_direct_commit_complete(struct nfs_commit_data *data)
{
	const struct nfs_writeverf *verf = data->res.verf;
	struct nfs_direct_req *dreq = data->dreq;
	struct nfs_commit_info cinfo;
	struct nfs_page *req;
	int status = data->task.tk_status;

	trace_nfs_direct_commit_complete(dreq);

	if (status < 0) {
		 
		dreq->error = status;
		dreq->flags = NFS_ODIRECT_DONE;
	} else {
		status = dreq->error;
	}

	nfs_init_cinfo_from_dreq(&cinfo, dreq);

	while (!list_empty(&data->pages)) {
		req = nfs_list_entry(data->pages.next);
		nfs_list_remove_request(req);
		if (status < 0) {
			spin_lock(&dreq->lock);
			nfs_direct_truncate_request(dreq, req);
			spin_unlock(&dreq->lock);
			nfs_release_request(req);
		} else if (!nfs_write_match_verf(verf, req)) {
			dreq->flags = NFS_ODIRECT_RESCHED_WRITES;
			 
			req->wb_nio = 0;
			nfs_mark_request_commit(req, NULL, &cinfo, 0);
		} else
			nfs_release_request(req);
		nfs_unlock_and_release_request(req);
	}

	if (nfs_commit_end(cinfo.mds))
		nfs_direct_write_complete(dreq);
}

static void nfs_direct_resched_write(struct nfs_commit_info *cinfo,
		struct nfs_page *req)
{
	struct nfs_direct_req *dreq = cinfo->dreq;

	trace_nfs_direct_resched_write(dreq);

	spin_lock(&dreq->lock);
	if (dreq->flags != NFS_ODIRECT_DONE)
		dreq->flags = NFS_ODIRECT_RESCHED_WRITES;
	spin_unlock(&dreq->lock);
	nfs_mark_request_commit(req, NULL, cinfo, 0);
}

static const struct nfs_commit_completion_ops nfs_direct_commit_completion_ops = {
	.completion = nfs_direct_commit_complete,
	.resched_write = nfs_direct_resched_write,
};

static void nfs_direct_commit_schedule(struct nfs_direct_req *dreq)
{
	int res;
	struct nfs_commit_info cinfo;
	LIST_HEAD(mds_list);

	nfs_init_cinfo_from_dreq(&cinfo, dreq);
	nfs_scan_commit(dreq->inode, &mds_list, &cinfo);
	res = nfs_generic_commit_list(dreq->inode, &mds_list, 0, &cinfo);
	if (res < 0)  
		nfs_direct_write_reschedule(dreq);
}

static void nfs_direct_write_clear_reqs(struct nfs_direct_req *dreq)
{
	struct nfs_commit_info cinfo;
	struct nfs_page *req;
	LIST_HEAD(reqs);

	nfs_init_cinfo_from_dreq(&cinfo, dreq);
	nfs_direct_write_scan_commit_list(dreq->inode, &reqs, &cinfo);

	while (!list_empty(&reqs)) {
		req = nfs_list_entry(reqs.next);
		nfs_list_remove_request(req);
		nfs_direct_truncate_request(dreq, req);
		nfs_release_request(req);
		nfs_unlock_and_release_request(req);
	}
}

static void nfs_direct_write_schedule_work(struct work_struct *work)
{
	struct nfs_direct_req *dreq = container_of(work, struct nfs_direct_req, work);
	int flags = dreq->flags;

	dreq->flags = 0;
	switch (flags) {
		case NFS_ODIRECT_DO_COMMIT:
			nfs_direct_commit_schedule(dreq);
			break;
		case NFS_ODIRECT_RESCHED_WRITES:
			nfs_direct_write_reschedule(dreq);
			break;
		default:
			nfs_direct_write_clear_reqs(dreq);
			nfs_zap_mapping(dreq->inode, dreq->inode->i_mapping);
			nfs_direct_complete(dreq);
	}
}

static void nfs_direct_write_complete(struct nfs_direct_req *dreq)
{
	trace_nfs_direct_write_complete(dreq);
	queue_work(nfsiod_workqueue, &dreq->work);  
}

static void nfs_direct_write_completion(struct nfs_pgio_header *hdr)
{
	struct nfs_direct_req *dreq = hdr->dreq;
	struct nfs_commit_info cinfo;
	struct nfs_page *req = nfs_list_entry(hdr->pages.next);
	int flags = NFS_ODIRECT_DONE;

	trace_nfs_direct_write_completion(dreq);

	nfs_init_cinfo_from_dreq(&cinfo, dreq);

	spin_lock(&dreq->lock);
	if (test_bit(NFS_IOHDR_REDO, &hdr->flags)) {
		spin_unlock(&dreq->lock);
		goto out_put;
	}

	nfs_direct_count_bytes(dreq, hdr);
	if (test_bit(NFS_IOHDR_UNSTABLE_WRITES, &hdr->flags) &&
	    !test_bit(NFS_IOHDR_ERROR, &hdr->flags)) {
		if (!dreq->flags)
			dreq->flags = NFS_ODIRECT_DO_COMMIT;
		flags = dreq->flags;
	}
	spin_unlock(&dreq->lock);

	while (!list_empty(&hdr->pages)) {

		req = nfs_list_entry(hdr->pages.next);
		nfs_list_remove_request(req);
		if (flags == NFS_ODIRECT_DO_COMMIT) {
			kref_get(&req->wb_kref);
			memcpy(&req->wb_verf, &hdr->verf.verifier,
			       sizeof(req->wb_verf));
			nfs_mark_request_commit(req, hdr->lseg, &cinfo,
				hdr->ds_commit_idx);
		} else if (flags == NFS_ODIRECT_RESCHED_WRITES) {
			kref_get(&req->wb_kref);
			nfs_mark_request_commit(req, NULL, &cinfo, 0);
		}
		nfs_unlock_and_release_request(req);
	}

out_put:
	if (put_dreq(dreq))
		nfs_direct_write_complete(dreq);
	hdr->release(hdr);
}

static void nfs_write_sync_pgio_error(struct list_head *head, int error)
{
	struct nfs_page *req;

	while (!list_empty(head)) {
		req = nfs_list_entry(head->next);
		nfs_list_remove_request(req);
		nfs_unlock_and_release_request(req);
	}
}

static void nfs_direct_write_reschedule_io(struct nfs_pgio_header *hdr)
{
	struct nfs_direct_req *dreq = hdr->dreq;
	struct nfs_page *req;
	struct nfs_commit_info cinfo;

	trace_nfs_direct_write_reschedule_io(dreq);

	nfs_init_cinfo_from_dreq(&cinfo, dreq);
	spin_lock(&dreq->lock);
	if (dreq->error == 0)
		dreq->flags = NFS_ODIRECT_RESCHED_WRITES;
	set_bit(NFS_IOHDR_REDO, &hdr->flags);
	spin_unlock(&dreq->lock);
	while (!list_empty(&hdr->pages)) {
		req = nfs_list_entry(hdr->pages.next);
		nfs_list_remove_request(req);
		nfs_unlock_request(req);
		nfs_mark_request_commit(req, NULL, &cinfo, 0);
	}
}

static const struct nfs_pgio_completion_ops nfs_direct_write_completion_ops = {
	.error_cleanup = nfs_write_sync_pgio_error,
	.init_hdr = nfs_direct_pgio_init,
	.completion = nfs_direct_write_completion,
	.reschedule_io = nfs_direct_write_reschedule_io,
};


 
 
static ssize_t nfs_direct_write_schedule_iovec(struct nfs_direct_req *dreq,
					       struct iov_iter *iter,
					       loff_t pos, int ioflags)
{
	struct nfs_pageio_descriptor desc;
	struct inode *inode = dreq->inode;
	struct nfs_commit_info cinfo;
	ssize_t result = 0;
	size_t requested_bytes = 0;
	size_t wsize = max_t(size_t, NFS_SERVER(inode)->wsize, PAGE_SIZE);
	bool defer = false;

	trace_nfs_direct_write_schedule_iovec(dreq);

	nfs_pageio_init_write(&desc, inode, ioflags, false,
			      &nfs_direct_write_completion_ops);
	desc.pg_dreq = dreq;
	get_dreq(dreq);
	inode_dio_begin(inode);

	NFS_I(inode)->write_io += iov_iter_count(iter);
	while (iov_iter_count(iter)) {
		struct page **pagevec;
		size_t bytes;
		size_t pgbase;
		unsigned npages, i;

		result = iov_iter_get_pages_alloc2(iter, &pagevec,
						  wsize, &pgbase);
		if (result < 0)
			break;

		bytes = result;
		npages = (result + pgbase + PAGE_SIZE - 1) / PAGE_SIZE;
		for (i = 0; i < npages; i++) {
			struct nfs_page *req;
			unsigned int req_len = min_t(size_t, bytes, PAGE_SIZE - pgbase);

			req = nfs_page_create_from_page(dreq->ctx, pagevec[i],
							pgbase, pos, req_len);
			if (IS_ERR(req)) {
				result = PTR_ERR(req);
				break;
			}

			if (desc.pg_error < 0) {
				nfs_free_request(req);
				result = desc.pg_error;
				break;
			}

			pgbase = 0;
			bytes -= req_len;
			requested_bytes += req_len;
			pos += req_len;
			dreq->bytes_left -= req_len;

			if (defer) {
				nfs_mark_request_commit(req, NULL, &cinfo, 0);
				continue;
			}

			nfs_lock_request(req);
			if (nfs_pageio_add_request(&desc, req))
				continue;

			 
			if (desc.pg_error < 0 && desc.pg_error != -EAGAIN) {
				result = desc.pg_error;
				nfs_unlock_and_release_request(req);
				break;
			}

			 
			nfs_init_cinfo_from_dreq(&cinfo, dreq);
			spin_lock(&dreq->lock);
			dreq->flags = NFS_ODIRECT_RESCHED_WRITES;
			spin_unlock(&dreq->lock);
			nfs_unlock_request(req);
			nfs_mark_request_commit(req, NULL, &cinfo, 0);
			desc.pg_error = 0;
			defer = true;
		}
		nfs_direct_release_pages(pagevec, npages);
		kvfree(pagevec);
		if (result < 0)
			break;
	}
	nfs_pageio_complete(&desc);

	 
	if (requested_bytes == 0) {
		inode_dio_end(inode);
		nfs_direct_req_release(dreq);
		return result < 0 ? result : -EIO;
	}

	if (put_dreq(dreq))
		nfs_direct_write_complete(dreq);
	return requested_bytes;
}

 
ssize_t nfs_file_direct_write(struct kiocb *iocb, struct iov_iter *iter,
			      bool swap)
{
	ssize_t result, requested;
	size_t count;
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	struct nfs_direct_req *dreq;
	struct nfs_lock_context *l_ctx;
	loff_t pos, end;

	dfprintk(FILE, "NFS: direct write(%pD2, %zd@%Ld)\n",
		file, iov_iter_count(iter), (long long) iocb->ki_pos);

	if (swap)
		 
		result =  iov_iter_count(iter);
	else
		result = generic_write_checks(iocb, iter);
	if (result <= 0)
		return result;
	count = result;
	nfs_add_stats(mapping->host, NFSIOS_DIRECTWRITTENBYTES, count);

	pos = iocb->ki_pos;
	end = (pos + iov_iter_count(iter) - 1) >> PAGE_SHIFT;

	task_io_account_write(count);

	result = -ENOMEM;
	dreq = nfs_direct_req_alloc();
	if (!dreq)
		goto out;

	dreq->inode = inode;
	dreq->bytes_left = dreq->max_count = count;
	dreq->io_start = pos;
	dreq->ctx = get_nfs_open_context(nfs_file_open_context(iocb->ki_filp));
	l_ctx = nfs_get_lock_context(dreq->ctx);
	if (IS_ERR(l_ctx)) {
		result = PTR_ERR(l_ctx);
		nfs_direct_req_release(dreq);
		goto out_release;
	}
	dreq->l_ctx = l_ctx;
	if (!is_sync_kiocb(iocb))
		dreq->iocb = iocb;
	pnfs_init_ds_commit_info_ops(&dreq->ds_cinfo, inode);

	if (swap) {
		requested = nfs_direct_write_schedule_iovec(dreq, iter, pos,
							    FLUSH_STABLE);
	} else {
		nfs_start_io_direct(inode);

		requested = nfs_direct_write_schedule_iovec(dreq, iter, pos,
							    FLUSH_COND_STABLE);

		if (mapping->nrpages) {
			invalidate_inode_pages2_range(mapping,
						      pos >> PAGE_SHIFT, end);
		}

		nfs_end_io_direct(inode);
	}

	if (requested > 0) {
		result = nfs_direct_wait(dreq);
		if (result > 0) {
			requested -= result;
			iocb->ki_pos = pos + result;
			 
			generic_write_sync(iocb, result);
		}
		iov_iter_revert(iter, requested);
	} else {
		result = requested;
	}
	nfs_fscache_invalidate(inode, FSCACHE_INVAL_DIO_WRITE);
out_release:
	nfs_direct_req_release(dreq);
out:
	return result;
}

 
int __init nfs_init_directcache(void)
{
	nfs_direct_cachep = kmem_cache_create("nfs_direct_cache",
						sizeof(struct nfs_direct_req),
						0, (SLAB_RECLAIM_ACCOUNT|
							SLAB_MEM_SPREAD),
						NULL);
	if (nfs_direct_cachep == NULL)
		return -ENOMEM;

	return 0;
}

 
void nfs_destroy_directcache(void)
{
	kmem_cache_destroy(nfs_direct_cachep);
}
