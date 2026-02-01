
 

#include <linux/module.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/sched/mm.h>
#include <linux/task_io_accounting_ops.h>
#include "internal.h"

 
static void netfs_clear_unread(struct netfs_io_subrequest *subreq)
{
	struct iov_iter iter;

	iov_iter_xarray(&iter, ITER_DEST, &subreq->rreq->mapping->i_pages,
			subreq->start + subreq->transferred,
			subreq->len   - subreq->transferred);
	iov_iter_zero(iov_iter_count(&iter), &iter);
}

static void netfs_cache_read_terminated(void *priv, ssize_t transferred_or_error,
					bool was_async)
{
	struct netfs_io_subrequest *subreq = priv;

	netfs_subreq_terminated(subreq, transferred_or_error, was_async);
}

 
static void netfs_read_from_cache(struct netfs_io_request *rreq,
				  struct netfs_io_subrequest *subreq,
				  enum netfs_read_from_hole read_hole)
{
	struct netfs_cache_resources *cres = &rreq->cache_resources;
	struct iov_iter iter;

	netfs_stat(&netfs_n_rh_read);
	iov_iter_xarray(&iter, ITER_DEST, &rreq->mapping->i_pages,
			subreq->start + subreq->transferred,
			subreq->len   - subreq->transferred);

	cres->ops->read(cres, subreq->start, &iter, read_hole,
			netfs_cache_read_terminated, subreq);
}

 
static void netfs_fill_with_zeroes(struct netfs_io_request *rreq,
				   struct netfs_io_subrequest *subreq)
{
	netfs_stat(&netfs_n_rh_zero);
	__set_bit(NETFS_SREQ_CLEAR_TAIL, &subreq->flags);
	netfs_subreq_terminated(subreq, 0, false);
}

 
static void netfs_read_from_server(struct netfs_io_request *rreq,
				   struct netfs_io_subrequest *subreq)
{
	netfs_stat(&netfs_n_rh_download);
	rreq->netfs_ops->issue_read(subreq);
}

 
static void netfs_rreq_completed(struct netfs_io_request *rreq, bool was_async)
{
	trace_netfs_rreq(rreq, netfs_rreq_trace_done);
	netfs_clear_subrequests(rreq, was_async);
	netfs_put_request(rreq, was_async, netfs_rreq_trace_put_complete);
}

 
static void netfs_rreq_unmark_after_write(struct netfs_io_request *rreq,
					  bool was_async)
{
	struct netfs_io_subrequest *subreq;
	struct folio *folio;
	pgoff_t unlocked = 0;
	bool have_unlocked = false;

	rcu_read_lock();

	list_for_each_entry(subreq, &rreq->subrequests, rreq_link) {
		XA_STATE(xas, &rreq->mapping->i_pages, subreq->start / PAGE_SIZE);

		xas_for_each(&xas, folio, (subreq->start + subreq->len - 1) / PAGE_SIZE) {
			if (xas_retry(&xas, folio))
				continue;

			 
			if (have_unlocked && folio_index(folio) <= unlocked)
				continue;
			unlocked = folio_index(folio);
			folio_end_fscache(folio);
			have_unlocked = true;
		}
	}

	rcu_read_unlock();
	netfs_rreq_completed(rreq, was_async);
}

static void netfs_rreq_copy_terminated(void *priv, ssize_t transferred_or_error,
				       bool was_async)
{
	struct netfs_io_subrequest *subreq = priv;
	struct netfs_io_request *rreq = subreq->rreq;

	if (IS_ERR_VALUE(transferred_or_error)) {
		netfs_stat(&netfs_n_rh_write_failed);
		trace_netfs_failure(rreq, subreq, transferred_or_error,
				    netfs_fail_copy_to_cache);
	} else {
		netfs_stat(&netfs_n_rh_write_done);
	}

	trace_netfs_sreq(subreq, netfs_sreq_trace_write_term);

	 
	if (atomic_dec_and_test(&rreq->nr_copy_ops))
		netfs_rreq_unmark_after_write(rreq, was_async);

	netfs_put_subrequest(subreq, was_async, netfs_sreq_trace_put_terminated);
}

 
static void netfs_rreq_do_write_to_cache(struct netfs_io_request *rreq)
{
	struct netfs_cache_resources *cres = &rreq->cache_resources;
	struct netfs_io_subrequest *subreq, *next, *p;
	struct iov_iter iter;
	int ret;

	trace_netfs_rreq(rreq, netfs_rreq_trace_copy);

	 
	atomic_inc(&rreq->nr_copy_ops);

	list_for_each_entry_safe(subreq, p, &rreq->subrequests, rreq_link) {
		if (!test_bit(NETFS_SREQ_COPY_TO_CACHE, &subreq->flags)) {
			list_del_init(&subreq->rreq_link);
			netfs_put_subrequest(subreq, false,
					     netfs_sreq_trace_put_no_copy);
		}
	}

	list_for_each_entry(subreq, &rreq->subrequests, rreq_link) {
		 
		while (!list_is_last(&subreq->rreq_link, &rreq->subrequests)) {
			next = list_next_entry(subreq, rreq_link);
			if (next->start != subreq->start + subreq->len)
				break;
			subreq->len += next->len;
			list_del_init(&next->rreq_link);
			netfs_put_subrequest(next, false,
					     netfs_sreq_trace_put_merged);
		}

		ret = cres->ops->prepare_write(cres, &subreq->start, &subreq->len,
					       rreq->i_size, true);
		if (ret < 0) {
			trace_netfs_failure(rreq, subreq, ret, netfs_fail_prepare_write);
			trace_netfs_sreq(subreq, netfs_sreq_trace_write_skip);
			continue;
		}

		iov_iter_xarray(&iter, ITER_SOURCE, &rreq->mapping->i_pages,
				subreq->start, subreq->len);

		atomic_inc(&rreq->nr_copy_ops);
		netfs_stat(&netfs_n_rh_write);
		netfs_get_subrequest(subreq, netfs_sreq_trace_get_copy_to_cache);
		trace_netfs_sreq(subreq, netfs_sreq_trace_write);
		cres->ops->write(cres, subreq->start, &iter,
				 netfs_rreq_copy_terminated, subreq);
	}

	 
	if (atomic_dec_and_test(&rreq->nr_copy_ops))
		netfs_rreq_unmark_after_write(rreq, false);
}

static void netfs_rreq_write_to_cache_work(struct work_struct *work)
{
	struct netfs_io_request *rreq =
		container_of(work, struct netfs_io_request, work);

	netfs_rreq_do_write_to_cache(rreq);
}

static void netfs_rreq_write_to_cache(struct netfs_io_request *rreq)
{
	rreq->work.func = netfs_rreq_write_to_cache_work;
	if (!queue_work(system_unbound_wq, &rreq->work))
		BUG();
}

 
static void netfs_rreq_short_read(struct netfs_io_request *rreq,
				  struct netfs_io_subrequest *subreq)
{
	__clear_bit(NETFS_SREQ_SHORT_IO, &subreq->flags);
	__set_bit(NETFS_SREQ_SEEK_DATA_READ, &subreq->flags);

	netfs_stat(&netfs_n_rh_short_read);
	trace_netfs_sreq(subreq, netfs_sreq_trace_resubmit_short);

	netfs_get_subrequest(subreq, netfs_sreq_trace_get_short_read);
	atomic_inc(&rreq->nr_outstanding);
	if (subreq->source == NETFS_READ_FROM_CACHE)
		netfs_read_from_cache(rreq, subreq, NETFS_READ_HOLE_CLEAR);
	else
		netfs_read_from_server(rreq, subreq);
}

 
static bool netfs_rreq_perform_resubmissions(struct netfs_io_request *rreq)
{
	struct netfs_io_subrequest *subreq;

	WARN_ON(in_interrupt());

	trace_netfs_rreq(rreq, netfs_rreq_trace_resubmit);

	 
	atomic_inc(&rreq->nr_outstanding);

	__clear_bit(NETFS_RREQ_INCOMPLETE_IO, &rreq->flags);
	list_for_each_entry(subreq, &rreq->subrequests, rreq_link) {
		if (subreq->error) {
			if (subreq->source != NETFS_READ_FROM_CACHE)
				break;
			subreq->source = NETFS_DOWNLOAD_FROM_SERVER;
			subreq->error = 0;
			netfs_stat(&netfs_n_rh_download_instead);
			trace_netfs_sreq(subreq, netfs_sreq_trace_download_instead);
			netfs_get_subrequest(subreq, netfs_sreq_trace_get_resubmit);
			atomic_inc(&rreq->nr_outstanding);
			netfs_read_from_server(rreq, subreq);
		} else if (test_bit(NETFS_SREQ_SHORT_IO, &subreq->flags)) {
			netfs_rreq_short_read(rreq, subreq);
		}
	}

	 
	if (atomic_dec_and_test(&rreq->nr_outstanding))
		return true;

	wake_up_var(&rreq->nr_outstanding);
	return false;
}

 
static void netfs_rreq_is_still_valid(struct netfs_io_request *rreq)
{
	struct netfs_io_subrequest *subreq;

	if (!rreq->netfs_ops->is_still_valid ||
	    rreq->netfs_ops->is_still_valid(rreq))
		return;

	list_for_each_entry(subreq, &rreq->subrequests, rreq_link) {
		if (subreq->source == NETFS_READ_FROM_CACHE) {
			subreq->error = -ESTALE;
			__set_bit(NETFS_RREQ_INCOMPLETE_IO, &rreq->flags);
		}
	}
}

 
static void netfs_rreq_assess(struct netfs_io_request *rreq, bool was_async)
{
	trace_netfs_rreq(rreq, netfs_rreq_trace_assess);

again:
	netfs_rreq_is_still_valid(rreq);

	if (!test_bit(NETFS_RREQ_FAILED, &rreq->flags) &&
	    test_bit(NETFS_RREQ_INCOMPLETE_IO, &rreq->flags)) {
		if (netfs_rreq_perform_resubmissions(rreq))
			goto again;
		return;
	}

	netfs_rreq_unlock_folios(rreq);

	clear_bit_unlock(NETFS_RREQ_IN_PROGRESS, &rreq->flags);
	wake_up_bit(&rreq->flags, NETFS_RREQ_IN_PROGRESS);

	if (test_bit(NETFS_RREQ_COPY_TO_CACHE, &rreq->flags))
		return netfs_rreq_write_to_cache(rreq);

	netfs_rreq_completed(rreq, was_async);
}

static void netfs_rreq_work(struct work_struct *work)
{
	struct netfs_io_request *rreq =
		container_of(work, struct netfs_io_request, work);
	netfs_rreq_assess(rreq, false);
}

 
static void netfs_rreq_terminated(struct netfs_io_request *rreq,
				  bool was_async)
{
	if (test_bit(NETFS_RREQ_INCOMPLETE_IO, &rreq->flags) &&
	    was_async) {
		if (!queue_work(system_unbound_wq, &rreq->work))
			BUG();
	} else {
		netfs_rreq_assess(rreq, was_async);
	}
}

 
void netfs_subreq_terminated(struct netfs_io_subrequest *subreq,
			     ssize_t transferred_or_error,
			     bool was_async)
{
	struct netfs_io_request *rreq = subreq->rreq;
	int u;

	_enter("[%u]{%llx,%lx},%zd",
	       subreq->debug_index, subreq->start, subreq->flags,
	       transferred_or_error);

	switch (subreq->source) {
	case NETFS_READ_FROM_CACHE:
		netfs_stat(&netfs_n_rh_read_done);
		break;
	case NETFS_DOWNLOAD_FROM_SERVER:
		netfs_stat(&netfs_n_rh_download_done);
		break;
	default:
		break;
	}

	if (IS_ERR_VALUE(transferred_or_error)) {
		subreq->error = transferred_or_error;
		trace_netfs_failure(rreq, subreq, transferred_or_error,
				    netfs_fail_read);
		goto failed;
	}

	if (WARN(transferred_or_error > subreq->len - subreq->transferred,
		 "Subreq overread: R%x[%x] %zd > %zu - %zu",
		 rreq->debug_id, subreq->debug_index,
		 transferred_or_error, subreq->len, subreq->transferred))
		transferred_or_error = subreq->len - subreq->transferred;

	subreq->error = 0;
	subreq->transferred += transferred_or_error;
	if (subreq->transferred < subreq->len)
		goto incomplete;

complete:
	__clear_bit(NETFS_SREQ_NO_PROGRESS, &subreq->flags);
	if (test_bit(NETFS_SREQ_COPY_TO_CACHE, &subreq->flags))
		set_bit(NETFS_RREQ_COPY_TO_CACHE, &rreq->flags);

out:
	trace_netfs_sreq(subreq, netfs_sreq_trace_terminated);

	 
	u = atomic_dec_return(&rreq->nr_outstanding);
	if (u == 0)
		netfs_rreq_terminated(rreq, was_async);
	else if (u == 1)
		wake_up_var(&rreq->nr_outstanding);

	netfs_put_subrequest(subreq, was_async, netfs_sreq_trace_put_terminated);
	return;

incomplete:
	if (test_bit(NETFS_SREQ_CLEAR_TAIL, &subreq->flags)) {
		netfs_clear_unread(subreq);
		subreq->transferred = subreq->len;
		goto complete;
	}

	if (transferred_or_error == 0) {
		if (__test_and_set_bit(NETFS_SREQ_NO_PROGRESS, &subreq->flags)) {
			subreq->error = -ENODATA;
			goto failed;
		}
	} else {
		__clear_bit(NETFS_SREQ_NO_PROGRESS, &subreq->flags);
	}

	__set_bit(NETFS_SREQ_SHORT_IO, &subreq->flags);
	set_bit(NETFS_RREQ_INCOMPLETE_IO, &rreq->flags);
	goto out;

failed:
	if (subreq->source == NETFS_READ_FROM_CACHE) {
		netfs_stat(&netfs_n_rh_read_failed);
		set_bit(NETFS_RREQ_INCOMPLETE_IO, &rreq->flags);
	} else {
		netfs_stat(&netfs_n_rh_download_failed);
		set_bit(NETFS_RREQ_FAILED, &rreq->flags);
		rreq->error = subreq->error;
	}
	goto out;
}
EXPORT_SYMBOL(netfs_subreq_terminated);

static enum netfs_io_source netfs_cache_prepare_read(struct netfs_io_subrequest *subreq,
						       loff_t i_size)
{
	struct netfs_io_request *rreq = subreq->rreq;
	struct netfs_cache_resources *cres = &rreq->cache_resources;

	if (cres->ops)
		return cres->ops->prepare_read(subreq, i_size);
	if (subreq->start >= rreq->i_size)
		return NETFS_FILL_WITH_ZEROES;
	return NETFS_DOWNLOAD_FROM_SERVER;
}

 
static enum netfs_io_source
netfs_rreq_prepare_read(struct netfs_io_request *rreq,
			struct netfs_io_subrequest *subreq)
{
	enum netfs_io_source source;

	_enter("%llx-%llx,%llx", subreq->start, subreq->start + subreq->len, rreq->i_size);

	source = netfs_cache_prepare_read(subreq, rreq->i_size);
	if (source == NETFS_INVALID_READ)
		goto out;

	if (source == NETFS_DOWNLOAD_FROM_SERVER) {
		 
		if (subreq->len > rreq->i_size - subreq->start)
			subreq->len = rreq->i_size - subreq->start;

		if (rreq->netfs_ops->clamp_length &&
		    !rreq->netfs_ops->clamp_length(subreq)) {
			source = NETFS_INVALID_READ;
			goto out;
		}
	}

	if (WARN_ON(subreq->len == 0))
		source = NETFS_INVALID_READ;

out:
	subreq->source = source;
	trace_netfs_sreq(subreq, netfs_sreq_trace_prepare);
	return source;
}

 
static bool netfs_rreq_submit_slice(struct netfs_io_request *rreq,
				    unsigned int *_debug_index)
{
	struct netfs_io_subrequest *subreq;
	enum netfs_io_source source;

	subreq = netfs_alloc_subrequest(rreq);
	if (!subreq)
		return false;

	subreq->debug_index	= (*_debug_index)++;
	subreq->start		= rreq->start + rreq->submitted;
	subreq->len		= rreq->len   - rreq->submitted;

	_debug("slice %llx,%zx,%zx", subreq->start, subreq->len, rreq->submitted);
	list_add_tail(&subreq->rreq_link, &rreq->subrequests);

	 
	source = netfs_rreq_prepare_read(rreq, subreq);
	if (source == NETFS_INVALID_READ)
		goto subreq_failed;

	atomic_inc(&rreq->nr_outstanding);

	rreq->submitted += subreq->len;

	trace_netfs_sreq(subreq, netfs_sreq_trace_submit);
	switch (source) {
	case NETFS_FILL_WITH_ZEROES:
		netfs_fill_with_zeroes(rreq, subreq);
		break;
	case NETFS_DOWNLOAD_FROM_SERVER:
		netfs_read_from_server(rreq, subreq);
		break;
	case NETFS_READ_FROM_CACHE:
		netfs_read_from_cache(rreq, subreq, NETFS_READ_HOLE_IGNORE);
		break;
	default:
		BUG();
	}

	return true;

subreq_failed:
	rreq->error = subreq->error;
	netfs_put_subrequest(subreq, false, netfs_sreq_trace_put_failed);
	return false;
}

 
int netfs_begin_read(struct netfs_io_request *rreq, bool sync)
{
	unsigned int debug_index = 0;
	int ret;

	_enter("R=%x %llx-%llx",
	       rreq->debug_id, rreq->start, rreq->start + rreq->len - 1);

	if (rreq->len == 0) {
		pr_err("Zero-sized read [R=%x]\n", rreq->debug_id);
		netfs_put_request(rreq, false, netfs_rreq_trace_put_zero_len);
		return -EIO;
	}

	INIT_WORK(&rreq->work, netfs_rreq_work);

	if (sync)
		netfs_get_request(rreq, netfs_rreq_trace_get_hold);

	 
	atomic_set(&rreq->nr_outstanding, 1);
	do {
		if (!netfs_rreq_submit_slice(rreq, &debug_index))
			break;

	} while (rreq->submitted < rreq->len);

	if (sync) {
		 
		for (;;) {
			wait_var_event(&rreq->nr_outstanding,
				       atomic_read(&rreq->nr_outstanding) == 1);
			netfs_rreq_assess(rreq, false);
			if (!test_bit(NETFS_RREQ_IN_PROGRESS, &rreq->flags))
				break;
			cond_resched();
		}

		ret = rreq->error;
		if (ret == 0 && rreq->submitted < rreq->len) {
			trace_netfs_failure(rreq, NULL, ret, netfs_fail_short_read);
			ret = -EIO;
		}
		netfs_put_request(rreq, false, netfs_rreq_trace_put_hold);
	} else {
		 
		if (atomic_dec_and_test(&rreq->nr_outstanding))
			netfs_rreq_assess(rreq, false);
		ret = 0;
	}
	return ret;
}
