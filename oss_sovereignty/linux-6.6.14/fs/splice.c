
 
#include <linux/bvec.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/splice.h>
#include <linux/memcontrol.h>
#include <linux/mm_inline.h>
#include <linux/swap.h>
#include <linux/writeback.h>
#include <linux/export.h>
#include <linux/syscalls.h>
#include <linux/uio.h>
#include <linux/fsnotify.h>
#include <linux/security.h>
#include <linux/gfp.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/sched/signal.h>

#include "internal.h"

 
static noinline void noinline pipe_clear_nowait(struct file *file)
{
	fmode_t fmode = READ_ONCE(file->f_mode);

	do {
		if (!(fmode & FMODE_NOWAIT))
			break;
	} while (!try_cmpxchg(&file->f_mode, &fmode, fmode & ~FMODE_NOWAIT));
}

 
static bool page_cache_pipe_buf_try_steal(struct pipe_inode_info *pipe,
		struct pipe_buffer *buf)
{
	struct folio *folio = page_folio(buf->page);
	struct address_space *mapping;

	folio_lock(folio);

	mapping = folio_mapping(folio);
	if (mapping) {
		WARN_ON(!folio_test_uptodate(folio));

		 
		folio_wait_writeback(folio);

		if (!filemap_release_folio(folio, GFP_KERNEL))
			goto out_unlock;

		 
		if (remove_mapping(mapping, folio)) {
			buf->flags |= PIPE_BUF_FLAG_LRU;
			return true;
		}
	}

	 
out_unlock:
	folio_unlock(folio);
	return false;
}

static void page_cache_pipe_buf_release(struct pipe_inode_info *pipe,
					struct pipe_buffer *buf)
{
	put_page(buf->page);
	buf->flags &= ~PIPE_BUF_FLAG_LRU;
}

 
static int page_cache_pipe_buf_confirm(struct pipe_inode_info *pipe,
				       struct pipe_buffer *buf)
{
	struct folio *folio = page_folio(buf->page);
	int err;

	if (!folio_test_uptodate(folio)) {
		folio_lock(folio);

		 
		if (!folio->mapping) {
			err = -ENODATA;
			goto error;
		}

		 
		if (!folio_test_uptodate(folio)) {
			err = -EIO;
			goto error;
		}

		 
		folio_unlock(folio);
	}

	return 0;
error:
	folio_unlock(folio);
	return err;
}

const struct pipe_buf_operations page_cache_pipe_buf_ops = {
	.confirm	= page_cache_pipe_buf_confirm,
	.release	= page_cache_pipe_buf_release,
	.try_steal	= page_cache_pipe_buf_try_steal,
	.get		= generic_pipe_buf_get,
};

static bool user_page_pipe_buf_try_steal(struct pipe_inode_info *pipe,
		struct pipe_buffer *buf)
{
	if (!(buf->flags & PIPE_BUF_FLAG_GIFT))
		return false;

	buf->flags |= PIPE_BUF_FLAG_LRU;
	return generic_pipe_buf_try_steal(pipe, buf);
}

static const struct pipe_buf_operations user_page_pipe_buf_ops = {
	.release	= page_cache_pipe_buf_release,
	.try_steal	= user_page_pipe_buf_try_steal,
	.get		= generic_pipe_buf_get,
};

static void wakeup_pipe_readers(struct pipe_inode_info *pipe)
{
	smp_mb();
	if (waitqueue_active(&pipe->rd_wait))
		wake_up_interruptible(&pipe->rd_wait);
	kill_fasync(&pipe->fasync_readers, SIGIO, POLL_IN);
}

 
ssize_t splice_to_pipe(struct pipe_inode_info *pipe,
		       struct splice_pipe_desc *spd)
{
	unsigned int spd_pages = spd->nr_pages;
	unsigned int tail = pipe->tail;
	unsigned int head = pipe->head;
	unsigned int mask = pipe->ring_size - 1;
	int ret = 0, page_nr = 0;

	if (!spd_pages)
		return 0;

	if (unlikely(!pipe->readers)) {
		send_sig(SIGPIPE, current, 0);
		ret = -EPIPE;
		goto out;
	}

	while (!pipe_full(head, tail, pipe->max_usage)) {
		struct pipe_buffer *buf = &pipe->bufs[head & mask];

		buf->page = spd->pages[page_nr];
		buf->offset = spd->partial[page_nr].offset;
		buf->len = spd->partial[page_nr].len;
		buf->private = spd->partial[page_nr].private;
		buf->ops = spd->ops;
		buf->flags = 0;

		head++;
		pipe->head = head;
		page_nr++;
		ret += buf->len;

		if (!--spd->nr_pages)
			break;
	}

	if (!ret)
		ret = -EAGAIN;

out:
	while (page_nr < spd_pages)
		spd->spd_release(spd, page_nr++);

	return ret;
}
EXPORT_SYMBOL_GPL(splice_to_pipe);

ssize_t add_to_pipe(struct pipe_inode_info *pipe, struct pipe_buffer *buf)
{
	unsigned int head = pipe->head;
	unsigned int tail = pipe->tail;
	unsigned int mask = pipe->ring_size - 1;
	int ret;

	if (unlikely(!pipe->readers)) {
		send_sig(SIGPIPE, current, 0);
		ret = -EPIPE;
	} else if (pipe_full(head, tail, pipe->max_usage)) {
		ret = -EAGAIN;
	} else {
		pipe->bufs[head & mask] = *buf;
		pipe->head = head + 1;
		return buf->len;
	}
	pipe_buf_release(pipe, buf);
	return ret;
}
EXPORT_SYMBOL(add_to_pipe);

 
int splice_grow_spd(const struct pipe_inode_info *pipe, struct splice_pipe_desc *spd)
{
	unsigned int max_usage = READ_ONCE(pipe->max_usage);

	spd->nr_pages_max = max_usage;
	if (max_usage <= PIPE_DEF_BUFFERS)
		return 0;

	spd->pages = kmalloc_array(max_usage, sizeof(struct page *), GFP_KERNEL);
	spd->partial = kmalloc_array(max_usage, sizeof(struct partial_page),
				     GFP_KERNEL);

	if (spd->pages && spd->partial)
		return 0;

	kfree(spd->pages);
	kfree(spd->partial);
	return -ENOMEM;
}

void splice_shrink_spd(struct splice_pipe_desc *spd)
{
	if (spd->nr_pages_max <= PIPE_DEF_BUFFERS)
		return;

	kfree(spd->pages);
	kfree(spd->partial);
}

 
ssize_t copy_splice_read(struct file *in, loff_t *ppos,
			 struct pipe_inode_info *pipe,
			 size_t len, unsigned int flags)
{
	struct iov_iter to;
	struct bio_vec *bv;
	struct kiocb kiocb;
	struct page **pages;
	ssize_t ret;
	size_t used, npages, chunk, remain, keep = 0;
	int i;

	 
	used = pipe_occupancy(pipe->head, pipe->tail);
	npages = max_t(ssize_t, pipe->max_usage - used, 0);
	len = min_t(size_t, len, npages * PAGE_SIZE);
	npages = DIV_ROUND_UP(len, PAGE_SIZE);

	bv = kzalloc(array_size(npages, sizeof(bv[0])) +
		     array_size(npages, sizeof(struct page *)), GFP_KERNEL);
	if (!bv)
		return -ENOMEM;

	pages = (struct page **)(bv + npages);
	npages = alloc_pages_bulk_array(GFP_USER, npages, pages);
	if (!npages) {
		kfree(bv);
		return -ENOMEM;
	}

	remain = len = min_t(size_t, len, npages * PAGE_SIZE);

	for (i = 0; i < npages; i++) {
		chunk = min_t(size_t, PAGE_SIZE, remain);
		bv[i].bv_page = pages[i];
		bv[i].bv_offset = 0;
		bv[i].bv_len = chunk;
		remain -= chunk;
	}

	 
	iov_iter_bvec(&to, ITER_DEST, bv, npages, len);
	init_sync_kiocb(&kiocb, in);
	kiocb.ki_pos = *ppos;
	ret = call_read_iter(in, &kiocb, &to);

	if (ret > 0) {
		keep = DIV_ROUND_UP(ret, PAGE_SIZE);
		*ppos = kiocb.ki_pos;
	}

	 
	if (ret == -EFAULT)
		ret = -EAGAIN;

	 
	if (keep < npages)
		release_pages(pages + keep, npages - keep);

	 
	remain = ret;
	for (i = 0; i < keep; i++) {
		struct pipe_buffer *buf = pipe_head_buf(pipe);

		chunk = min_t(size_t, remain, PAGE_SIZE);
		*buf = (struct pipe_buffer) {
			.ops	= &default_pipe_buf_ops,
			.page	= bv[i].bv_page,
			.offset	= 0,
			.len	= chunk,
		};
		pipe->head++;
		remain -= chunk;
	}

	kfree(bv);
	return ret;
}
EXPORT_SYMBOL(copy_splice_read);

const struct pipe_buf_operations default_pipe_buf_ops = {
	.release	= generic_pipe_buf_release,
	.try_steal	= generic_pipe_buf_try_steal,
	.get		= generic_pipe_buf_get,
};

 
const struct pipe_buf_operations nosteal_pipe_buf_ops = {
	.release	= generic_pipe_buf_release,
	.get		= generic_pipe_buf_get,
};
EXPORT_SYMBOL(nosteal_pipe_buf_ops);

static void wakeup_pipe_writers(struct pipe_inode_info *pipe)
{
	smp_mb();
	if (waitqueue_active(&pipe->wr_wait))
		wake_up_interruptible(&pipe->wr_wait);
	kill_fasync(&pipe->fasync_writers, SIGIO, POLL_OUT);
}

 
static int splice_from_pipe_feed(struct pipe_inode_info *pipe, struct splice_desc *sd,
			  splice_actor *actor)
{
	unsigned int head = pipe->head;
	unsigned int tail = pipe->tail;
	unsigned int mask = pipe->ring_size - 1;
	int ret;

	while (!pipe_empty(head, tail)) {
		struct pipe_buffer *buf = &pipe->bufs[tail & mask];

		sd->len = buf->len;
		if (sd->len > sd->total_len)
			sd->len = sd->total_len;

		ret = pipe_buf_confirm(pipe, buf);
		if (unlikely(ret)) {
			if (ret == -ENODATA)
				ret = 0;
			return ret;
		}

		ret = actor(pipe, buf, sd);
		if (ret <= 0)
			return ret;

		buf->offset += ret;
		buf->len -= ret;

		sd->num_spliced += ret;
		sd->len -= ret;
		sd->pos += ret;
		sd->total_len -= ret;

		if (!buf->len) {
			pipe_buf_release(pipe, buf);
			tail++;
			pipe->tail = tail;
			if (pipe->files)
				sd->need_wakeup = true;
		}

		if (!sd->total_len)
			return 0;
	}

	return 1;
}

 
static inline bool eat_empty_buffer(struct pipe_inode_info *pipe)
{
	unsigned int tail = pipe->tail;
	unsigned int mask = pipe->ring_size - 1;
	struct pipe_buffer *buf = &pipe->bufs[tail & mask];

	if (unlikely(!buf->len)) {
		pipe_buf_release(pipe, buf);
		pipe->tail = tail+1;
		return true;
	}

	return false;
}

 
static int splice_from_pipe_next(struct pipe_inode_info *pipe, struct splice_desc *sd)
{
	 
	if (signal_pending(current))
		return -ERESTARTSYS;

repeat:
	while (pipe_empty(pipe->head, pipe->tail)) {
		if (!pipe->writers)
			return 0;

		if (sd->num_spliced)
			return 0;

		if (sd->flags & SPLICE_F_NONBLOCK)
			return -EAGAIN;

		if (signal_pending(current))
			return -ERESTARTSYS;

		if (sd->need_wakeup) {
			wakeup_pipe_writers(pipe);
			sd->need_wakeup = false;
		}

		pipe_wait_readable(pipe);
	}

	if (eat_empty_buffer(pipe))
		goto repeat;

	return 1;
}

 
static void splice_from_pipe_begin(struct splice_desc *sd)
{
	sd->num_spliced = 0;
	sd->need_wakeup = false;
}

 
static void splice_from_pipe_end(struct pipe_inode_info *pipe, struct splice_desc *sd)
{
	if (sd->need_wakeup)
		wakeup_pipe_writers(pipe);
}

 
ssize_t __splice_from_pipe(struct pipe_inode_info *pipe, struct splice_desc *sd,
			   splice_actor *actor)
{
	int ret;

	splice_from_pipe_begin(sd);
	do {
		cond_resched();
		ret = splice_from_pipe_next(pipe, sd);
		if (ret > 0)
			ret = splice_from_pipe_feed(pipe, sd, actor);
	} while (ret > 0);
	splice_from_pipe_end(pipe, sd);

	return sd->num_spliced ? sd->num_spliced : ret;
}
EXPORT_SYMBOL(__splice_from_pipe);

 
ssize_t splice_from_pipe(struct pipe_inode_info *pipe, struct file *out,
			 loff_t *ppos, size_t len, unsigned int flags,
			 splice_actor *actor)
{
	ssize_t ret;
	struct splice_desc sd = {
		.total_len = len,
		.flags = flags,
		.pos = *ppos,
		.u.file = out,
	};

	pipe_lock(pipe);
	ret = __splice_from_pipe(pipe, &sd, actor);
	pipe_unlock(pipe);

	return ret;
}

 
ssize_t
iter_file_splice_write(struct pipe_inode_info *pipe, struct file *out,
			  loff_t *ppos, size_t len, unsigned int flags)
{
	struct splice_desc sd = {
		.total_len = len,
		.flags = flags,
		.pos = *ppos,
		.u.file = out,
	};
	int nbufs = pipe->max_usage;
	struct bio_vec *array = kcalloc(nbufs, sizeof(struct bio_vec),
					GFP_KERNEL);
	ssize_t ret;

	if (unlikely(!array))
		return -ENOMEM;

	pipe_lock(pipe);

	splice_from_pipe_begin(&sd);
	while (sd.total_len) {
		struct iov_iter from;
		unsigned int head, tail, mask;
		size_t left;
		int n;

		ret = splice_from_pipe_next(pipe, &sd);
		if (ret <= 0)
			break;

		if (unlikely(nbufs < pipe->max_usage)) {
			kfree(array);
			nbufs = pipe->max_usage;
			array = kcalloc(nbufs, sizeof(struct bio_vec),
					GFP_KERNEL);
			if (!array) {
				ret = -ENOMEM;
				break;
			}
		}

		head = pipe->head;
		tail = pipe->tail;
		mask = pipe->ring_size - 1;

		 
		left = sd.total_len;
		for (n = 0; !pipe_empty(head, tail) && left && n < nbufs; tail++) {
			struct pipe_buffer *buf = &pipe->bufs[tail & mask];
			size_t this_len = buf->len;

			 
			if (!this_len)
				continue;
			this_len = min(this_len, left);

			ret = pipe_buf_confirm(pipe, buf);
			if (unlikely(ret)) {
				if (ret == -ENODATA)
					ret = 0;
				goto done;
			}

			bvec_set_page(&array[n], buf->page, this_len,
				      buf->offset);
			left -= this_len;
			n++;
		}

		iov_iter_bvec(&from, ITER_SOURCE, array, n, sd.total_len - left);
		ret = vfs_iter_write(out, &from, &sd.pos, 0);
		if (ret <= 0)
			break;

		sd.num_spliced += ret;
		sd.total_len -= ret;
		*ppos = sd.pos;

		 
		tail = pipe->tail;
		while (ret) {
			struct pipe_buffer *buf = &pipe->bufs[tail & mask];
			if (ret >= buf->len) {
				ret -= buf->len;
				buf->len = 0;
				pipe_buf_release(pipe, buf);
				tail++;
				pipe->tail = tail;
				if (pipe->files)
					sd.need_wakeup = true;
			} else {
				buf->offset += ret;
				buf->len -= ret;
				ret = 0;
			}
		}
	}
done:
	kfree(array);
	splice_from_pipe_end(pipe, &sd);

	pipe_unlock(pipe);

	if (sd.num_spliced)
		ret = sd.num_spliced;

	return ret;
}

EXPORT_SYMBOL(iter_file_splice_write);

#ifdef CONFIG_NET
 
ssize_t splice_to_socket(struct pipe_inode_info *pipe, struct file *out,
			 loff_t *ppos, size_t len, unsigned int flags)
{
	struct socket *sock = sock_from_file(out);
	struct bio_vec bvec[16];
	struct msghdr msg = {};
	ssize_t ret = 0;
	size_t spliced = 0;
	bool need_wakeup = false;

	pipe_lock(pipe);

	while (len > 0) {
		unsigned int head, tail, mask, bc = 0;
		size_t remain = len;

		 
		ret = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		while (pipe_empty(pipe->head, pipe->tail)) {
			ret = 0;
			if (!pipe->writers)
				goto out;

			if (spliced)
				goto out;

			ret = -EAGAIN;
			if (flags & SPLICE_F_NONBLOCK)
				goto out;

			ret = -ERESTARTSYS;
			if (signal_pending(current))
				goto out;

			if (need_wakeup) {
				wakeup_pipe_writers(pipe);
				need_wakeup = false;
			}

			pipe_wait_readable(pipe);
		}

		head = pipe->head;
		tail = pipe->tail;
		mask = pipe->ring_size - 1;

		while (!pipe_empty(head, tail)) {
			struct pipe_buffer *buf = &pipe->bufs[tail & mask];
			size_t seg;

			if (!buf->len) {
				tail++;
				continue;
			}

			seg = min_t(size_t, remain, buf->len);

			ret = pipe_buf_confirm(pipe, buf);
			if (unlikely(ret)) {
				if (ret == -ENODATA)
					ret = 0;
				break;
			}

			bvec_set_page(&bvec[bc++], buf->page, seg, buf->offset);
			remain -= seg;
			if (remain == 0 || bc >= ARRAY_SIZE(bvec))
				break;
			tail++;
		}

		if (!bc)
			break;

		msg.msg_flags = MSG_SPLICE_PAGES;
		if (flags & SPLICE_F_MORE)
			msg.msg_flags |= MSG_MORE;
		if (remain && pipe_occupancy(pipe->head, tail) > 0)
			msg.msg_flags |= MSG_MORE;
		if (out->f_flags & O_NONBLOCK)
			msg.msg_flags |= MSG_DONTWAIT;

		iov_iter_bvec(&msg.msg_iter, ITER_SOURCE, bvec, bc,
			      len - remain);
		ret = sock_sendmsg(sock, &msg);
		if (ret <= 0)
			break;

		spliced += ret;
		len -= ret;
		tail = pipe->tail;
		while (ret > 0) {
			struct pipe_buffer *buf = &pipe->bufs[tail & mask];
			size_t seg = min_t(size_t, ret, buf->len);

			buf->offset += seg;
			buf->len -= seg;
			ret -= seg;

			if (!buf->len) {
				pipe_buf_release(pipe, buf);
				tail++;
			}
		}

		if (tail != pipe->tail) {
			pipe->tail = tail;
			if (pipe->files)
				need_wakeup = true;
		}
	}

out:
	pipe_unlock(pipe);
	if (need_wakeup)
		wakeup_pipe_writers(pipe);
	return spliced ?: ret;
}
#endif

static int warn_unsupported(struct file *file, const char *op)
{
	pr_debug_ratelimited(
		"splice %s not supported for file %pD4 (pid: %d comm: %.20s)\n",
		op, file, current->pid, current->comm);
	return -EINVAL;
}

 
static long do_splice_from(struct pipe_inode_info *pipe, struct file *out,
			   loff_t *ppos, size_t len, unsigned int flags)
{
	if (unlikely(!out->f_op->splice_write))
		return warn_unsupported(out, "write");
	return out->f_op->splice_write(pipe, out, ppos, len, flags);
}

 
static void do_splice_eof(struct splice_desc *sd)
{
	if (sd->splice_eof)
		sd->splice_eof(sd);
}

 
long vfs_splice_read(struct file *in, loff_t *ppos,
		     struct pipe_inode_info *pipe, size_t len,
		     unsigned int flags)
{
	unsigned int p_space;
	int ret;

	if (unlikely(!(in->f_mode & FMODE_READ)))
		return -EBADF;
	if (!len)
		return 0;

	 
	p_space = pipe->max_usage - pipe_occupancy(pipe->head, pipe->tail);
	len = min_t(size_t, len, p_space << PAGE_SHIFT);

	ret = rw_verify_area(READ, in, ppos, len);
	if (unlikely(ret < 0))
		return ret;

	if (unlikely(len > MAX_RW_COUNT))
		len = MAX_RW_COUNT;

	if (unlikely(!in->f_op->splice_read))
		return warn_unsupported(in, "read");
	 
	if ((in->f_flags & O_DIRECT) || IS_DAX(in->f_mapping->host))
		return copy_splice_read(in, ppos, pipe, len, flags);
	return in->f_op->splice_read(in, ppos, pipe, len, flags);
}
EXPORT_SYMBOL_GPL(vfs_splice_read);

 
ssize_t splice_direct_to_actor(struct file *in, struct splice_desc *sd,
			       splice_direct_actor *actor)
{
	struct pipe_inode_info *pipe;
	long ret, bytes;
	size_t len;
	int i, flags, more;

	 
	if (unlikely(!(in->f_mode & FMODE_LSEEK)))
		return -EINVAL;

	 
	pipe = current->splice_pipe;
	if (unlikely(!pipe)) {
		pipe = alloc_pipe_info();
		if (!pipe)
			return -ENOMEM;

		 
		pipe->readers = 1;

		current->splice_pipe = pipe;
	}

	 
	bytes = 0;
	len = sd->total_len;

	 
	flags = sd->flags;
	sd->flags &= ~SPLICE_F_NONBLOCK;

	 
	more = sd->flags & SPLICE_F_MORE;
	sd->flags |= SPLICE_F_MORE;

	WARN_ON_ONCE(!pipe_empty(pipe->head, pipe->tail));

	while (len) {
		size_t read_len;
		loff_t pos = sd->pos, prev_pos = pos;

		ret = vfs_splice_read(in, &pos, pipe, len, flags);
		if (unlikely(ret <= 0))
			goto read_failure;

		read_len = ret;
		sd->total_len = read_len;

		 
		if (read_len >= len && !more)
			sd->flags &= ~SPLICE_F_MORE;

		 
		ret = actor(pipe, sd);
		if (unlikely(ret <= 0)) {
			sd->pos = prev_pos;
			goto out_release;
		}

		bytes += ret;
		len -= ret;
		sd->pos = pos;

		if (ret < read_len) {
			sd->pos = prev_pos + ret;
			goto out_release;
		}
	}

done:
	pipe->tail = pipe->head = 0;
	file_accessed(in);
	return bytes;

read_failure:
	 
	if (ret == 0 && !more && len > 0 && bytes)
		do_splice_eof(sd);
out_release:
	 
	for (i = 0; i < pipe->ring_size; i++) {
		struct pipe_buffer *buf = &pipe->bufs[i];

		if (buf->ops)
			pipe_buf_release(pipe, buf);
	}

	if (!bytes)
		bytes = ret;

	goto done;
}
EXPORT_SYMBOL(splice_direct_to_actor);

static int direct_splice_actor(struct pipe_inode_info *pipe,
			       struct splice_desc *sd)
{
	struct file *file = sd->u.file;

	return do_splice_from(pipe, file, sd->opos, sd->total_len,
			      sd->flags);
}

static void direct_file_splice_eof(struct splice_desc *sd)
{
	struct file *file = sd->u.file;

	if (file->f_op->splice_eof)
		file->f_op->splice_eof(file);
}

 
long do_splice_direct(struct file *in, loff_t *ppos, struct file *out,
		      loff_t *opos, size_t len, unsigned int flags)
{
	struct splice_desc sd = {
		.len		= len,
		.total_len	= len,
		.flags		= flags,
		.pos		= *ppos,
		.u.file		= out,
		.splice_eof	= direct_file_splice_eof,
		.opos		= opos,
	};
	long ret;

	if (unlikely(!(out->f_mode & FMODE_WRITE)))
		return -EBADF;

	if (unlikely(out->f_flags & O_APPEND))
		return -EINVAL;

	ret = rw_verify_area(WRITE, out, opos, len);
	if (unlikely(ret < 0))
		return ret;

	ret = splice_direct_to_actor(in, &sd, direct_splice_actor);
	if (ret > 0)
		*ppos = sd.pos;

	return ret;
}
EXPORT_SYMBOL(do_splice_direct);

static int wait_for_space(struct pipe_inode_info *pipe, unsigned flags)
{
	for (;;) {
		if (unlikely(!pipe->readers)) {
			send_sig(SIGPIPE, current, 0);
			return -EPIPE;
		}
		if (!pipe_full(pipe->head, pipe->tail, pipe->max_usage))
			return 0;
		if (flags & SPLICE_F_NONBLOCK)
			return -EAGAIN;
		if (signal_pending(current))
			return -ERESTARTSYS;
		pipe_wait_writable(pipe);
	}
}

static int splice_pipe_to_pipe(struct pipe_inode_info *ipipe,
			       struct pipe_inode_info *opipe,
			       size_t len, unsigned int flags);

long splice_file_to_pipe(struct file *in,
			 struct pipe_inode_info *opipe,
			 loff_t *offset,
			 size_t len, unsigned int flags)
{
	long ret;

	pipe_lock(opipe);
	ret = wait_for_space(opipe, flags);
	if (!ret)
		ret = vfs_splice_read(in, offset, opipe, len, flags);
	pipe_unlock(opipe);
	if (ret > 0)
		wakeup_pipe_readers(opipe);
	return ret;
}

 
long do_splice(struct file *in, loff_t *off_in, struct file *out,
	       loff_t *off_out, size_t len, unsigned int flags)
{
	struct pipe_inode_info *ipipe;
	struct pipe_inode_info *opipe;
	loff_t offset;
	long ret;

	if (unlikely(!(in->f_mode & FMODE_READ) ||
		     !(out->f_mode & FMODE_WRITE)))
		return -EBADF;

	ipipe = get_pipe_info(in, true);
	opipe = get_pipe_info(out, true);

	if (ipipe && opipe) {
		if (off_in || off_out)
			return -ESPIPE;

		 
		if (ipipe == opipe)
			return -EINVAL;

		if ((in->f_flags | out->f_flags) & O_NONBLOCK)
			flags |= SPLICE_F_NONBLOCK;

		ret = splice_pipe_to_pipe(ipipe, opipe, len, flags);
	} else if (ipipe) {
		if (off_in)
			return -ESPIPE;
		if (off_out) {
			if (!(out->f_mode & FMODE_PWRITE))
				return -EINVAL;
			offset = *off_out;
		} else {
			offset = out->f_pos;
		}

		if (unlikely(out->f_flags & O_APPEND))
			return -EINVAL;

		ret = rw_verify_area(WRITE, out, &offset, len);
		if (unlikely(ret < 0))
			return ret;

		if (in->f_flags & O_NONBLOCK)
			flags |= SPLICE_F_NONBLOCK;

		file_start_write(out);
		ret = do_splice_from(ipipe, out, &offset, len, flags);
		file_end_write(out);

		if (!off_out)
			out->f_pos = offset;
		else
			*off_out = offset;
	} else if (opipe) {
		if (off_out)
			return -ESPIPE;
		if (off_in) {
			if (!(in->f_mode & FMODE_PREAD))
				return -EINVAL;
			offset = *off_in;
		} else {
			offset = in->f_pos;
		}

		if (out->f_flags & O_NONBLOCK)
			flags |= SPLICE_F_NONBLOCK;

		ret = splice_file_to_pipe(in, opipe, &offset, len, flags);

		if (!off_in)
			in->f_pos = offset;
		else
			*off_in = offset;
	} else {
		ret = -EINVAL;
	}

	if (ret > 0) {
		 
		fsnotify_modify(out);
		fsnotify_access(in);
	}

	return ret;
}

static long __do_splice(struct file *in, loff_t __user *off_in,
			struct file *out, loff_t __user *off_out,
			size_t len, unsigned int flags)
{
	struct pipe_inode_info *ipipe;
	struct pipe_inode_info *opipe;
	loff_t offset, *__off_in = NULL, *__off_out = NULL;
	long ret;

	ipipe = get_pipe_info(in, true);
	opipe = get_pipe_info(out, true);

	if (ipipe) {
		if (off_in)
			return -ESPIPE;
		pipe_clear_nowait(in);
	}
	if (opipe) {
		if (off_out)
			return -ESPIPE;
		pipe_clear_nowait(out);
	}

	if (off_out) {
		if (copy_from_user(&offset, off_out, sizeof(loff_t)))
			return -EFAULT;
		__off_out = &offset;
	}
	if (off_in) {
		if (copy_from_user(&offset, off_in, sizeof(loff_t)))
			return -EFAULT;
		__off_in = &offset;
	}

	ret = do_splice(in, __off_in, out, __off_out, len, flags);
	if (ret < 0)
		return ret;

	if (__off_out && copy_to_user(off_out, __off_out, sizeof(loff_t)))
		return -EFAULT;
	if (__off_in && copy_to_user(off_in, __off_in, sizeof(loff_t)))
		return -EFAULT;

	return ret;
}

static int iter_to_pipe(struct iov_iter *from,
			struct pipe_inode_info *pipe,
			unsigned flags)
{
	struct pipe_buffer buf = {
		.ops = &user_page_pipe_buf_ops,
		.flags = flags
	};
	size_t total = 0;
	int ret = 0;

	while (iov_iter_count(from)) {
		struct page *pages[16];
		ssize_t left;
		size_t start;
		int i, n;

		left = iov_iter_get_pages2(from, pages, ~0UL, 16, &start);
		if (left <= 0) {
			ret = left;
			break;
		}

		n = DIV_ROUND_UP(left + start, PAGE_SIZE);
		for (i = 0; i < n; i++) {
			int size = min_t(int, left, PAGE_SIZE - start);

			buf.page = pages[i];
			buf.offset = start;
			buf.len = size;
			ret = add_to_pipe(pipe, &buf);
			if (unlikely(ret < 0)) {
				iov_iter_revert(from, left);
				
				while (++i < n)
					put_page(pages[i]);
				goto out;
			}
			total += ret;
			left -= size;
			start = 0;
		}
	}
out:
	return total ? total : ret;
}

static int pipe_to_user(struct pipe_inode_info *pipe, struct pipe_buffer *buf,
			struct splice_desc *sd)
{
	int n = copy_page_to_iter(buf->page, buf->offset, sd->len, sd->u.data);
	return n == sd->len ? n : -EFAULT;
}

 
static long vmsplice_to_user(struct file *file, struct iov_iter *iter,
			     unsigned int flags)
{
	struct pipe_inode_info *pipe = get_pipe_info(file, true);
	struct splice_desc sd = {
		.total_len = iov_iter_count(iter),
		.flags = flags,
		.u.data = iter
	};
	long ret = 0;

	if (!pipe)
		return -EBADF;

	pipe_clear_nowait(file);

	if (sd.total_len) {
		pipe_lock(pipe);
		ret = __splice_from_pipe(pipe, &sd, pipe_to_user);
		pipe_unlock(pipe);
	}

	if (ret > 0)
		fsnotify_access(file);

	return ret;
}

 
static long vmsplice_to_pipe(struct file *file, struct iov_iter *iter,
			     unsigned int flags)
{
	struct pipe_inode_info *pipe;
	long ret = 0;
	unsigned buf_flag = 0;

	if (flags & SPLICE_F_GIFT)
		buf_flag = PIPE_BUF_FLAG_GIFT;

	pipe = get_pipe_info(file, true);
	if (!pipe)
		return -EBADF;

	pipe_clear_nowait(file);

	pipe_lock(pipe);
	ret = wait_for_space(pipe, flags);
	if (!ret)
		ret = iter_to_pipe(iter, pipe, buf_flag);
	pipe_unlock(pipe);
	if (ret > 0) {
		wakeup_pipe_readers(pipe);
		fsnotify_modify(file);
	}
	return ret;
}

static int vmsplice_type(struct fd f, int *type)
{
	if (!f.file)
		return -EBADF;
	if (f.file->f_mode & FMODE_WRITE) {
		*type = ITER_SOURCE;
	} else if (f.file->f_mode & FMODE_READ) {
		*type = ITER_DEST;
	} else {
		fdput(f);
		return -EBADF;
	}
	return 0;
}

 
SYSCALL_DEFINE4(vmsplice, int, fd, const struct iovec __user *, uiov,
		unsigned long, nr_segs, unsigned int, flags)
{
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov = iovstack;
	struct iov_iter iter;
	ssize_t error;
	struct fd f;
	int type;

	if (unlikely(flags & ~SPLICE_F_ALL))
		return -EINVAL;

	f = fdget(fd);
	error = vmsplice_type(f, &type);
	if (error)
		return error;

	error = import_iovec(type, uiov, nr_segs,
			     ARRAY_SIZE(iovstack), &iov, &iter);
	if (error < 0)
		goto out_fdput;

	if (!iov_iter_count(&iter))
		error = 0;
	else if (type == ITER_SOURCE)
		error = vmsplice_to_pipe(f.file, &iter, flags);
	else
		error = vmsplice_to_user(f.file, &iter, flags);

	kfree(iov);
out_fdput:
	fdput(f);
	return error;
}

SYSCALL_DEFINE6(splice, int, fd_in, loff_t __user *, off_in,
		int, fd_out, loff_t __user *, off_out,
		size_t, len, unsigned int, flags)
{
	struct fd in, out;
	long error;

	if (unlikely(!len))
		return 0;

	if (unlikely(flags & ~SPLICE_F_ALL))
		return -EINVAL;

	error = -EBADF;
	in = fdget(fd_in);
	if (in.file) {
		out = fdget(fd_out);
		if (out.file) {
			error = __do_splice(in.file, off_in, out.file, off_out,
						len, flags);
			fdput(out);
		}
		fdput(in);
	}
	return error;
}

 
static int ipipe_prep(struct pipe_inode_info *pipe, unsigned int flags)
{
	int ret;

	 
	if (!pipe_empty(pipe->head, pipe->tail))
		return 0;

	ret = 0;
	pipe_lock(pipe);

	while (pipe_empty(pipe->head, pipe->tail)) {
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		if (!pipe->writers)
			break;
		if (flags & SPLICE_F_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}
		pipe_wait_readable(pipe);
	}

	pipe_unlock(pipe);
	return ret;
}

 
static int opipe_prep(struct pipe_inode_info *pipe, unsigned int flags)
{
	int ret;

	 
	if (!pipe_full(pipe->head, pipe->tail, pipe->max_usage))
		return 0;

	ret = 0;
	pipe_lock(pipe);

	while (pipe_full(pipe->head, pipe->tail, pipe->max_usage)) {
		if (!pipe->readers) {
			send_sig(SIGPIPE, current, 0);
			ret = -EPIPE;
			break;
		}
		if (flags & SPLICE_F_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		pipe_wait_writable(pipe);
	}

	pipe_unlock(pipe);
	return ret;
}

 
static int splice_pipe_to_pipe(struct pipe_inode_info *ipipe,
			       struct pipe_inode_info *opipe,
			       size_t len, unsigned int flags)
{
	struct pipe_buffer *ibuf, *obuf;
	unsigned int i_head, o_head;
	unsigned int i_tail, o_tail;
	unsigned int i_mask, o_mask;
	int ret = 0;
	bool input_wakeup = false;


retry:
	ret = ipipe_prep(ipipe, flags);
	if (ret)
		return ret;

	ret = opipe_prep(opipe, flags);
	if (ret)
		return ret;

	 
	pipe_double_lock(ipipe, opipe);

	i_tail = ipipe->tail;
	i_mask = ipipe->ring_size - 1;
	o_head = opipe->head;
	o_mask = opipe->ring_size - 1;

	do {
		size_t o_len;

		if (!opipe->readers) {
			send_sig(SIGPIPE, current, 0);
			if (!ret)
				ret = -EPIPE;
			break;
		}

		i_head = ipipe->head;
		o_tail = opipe->tail;

		if (pipe_empty(i_head, i_tail) && !ipipe->writers)
			break;

		 
		if (pipe_empty(i_head, i_tail) ||
		    pipe_full(o_head, o_tail, opipe->max_usage)) {
			 
			if (ret)
				break;

			if (flags & SPLICE_F_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			 
			pipe_unlock(ipipe);
			pipe_unlock(opipe);
			goto retry;
		}

		ibuf = &ipipe->bufs[i_tail & i_mask];
		obuf = &opipe->bufs[o_head & o_mask];

		if (len >= ibuf->len) {
			 
			*obuf = *ibuf;
			ibuf->ops = NULL;
			i_tail++;
			ipipe->tail = i_tail;
			input_wakeup = true;
			o_len = obuf->len;
			o_head++;
			opipe->head = o_head;
		} else {
			 
			if (!pipe_buf_get(ipipe, ibuf)) {
				if (ret == 0)
					ret = -EFAULT;
				break;
			}
			*obuf = *ibuf;

			 
			obuf->flags &= ~PIPE_BUF_FLAG_GIFT;
			obuf->flags &= ~PIPE_BUF_FLAG_CAN_MERGE;

			obuf->len = len;
			ibuf->offset += len;
			ibuf->len -= len;
			o_len = len;
			o_head++;
			opipe->head = o_head;
		}
		ret += o_len;
		len -= o_len;
	} while (len);

	pipe_unlock(ipipe);
	pipe_unlock(opipe);

	 
	if (ret > 0)
		wakeup_pipe_readers(opipe);

	if (input_wakeup)
		wakeup_pipe_writers(ipipe);

	return ret;
}

 
static int link_pipe(struct pipe_inode_info *ipipe,
		     struct pipe_inode_info *opipe,
		     size_t len, unsigned int flags)
{
	struct pipe_buffer *ibuf, *obuf;
	unsigned int i_head, o_head;
	unsigned int i_tail, o_tail;
	unsigned int i_mask, o_mask;
	int ret = 0;

	 
	pipe_double_lock(ipipe, opipe);

	i_tail = ipipe->tail;
	i_mask = ipipe->ring_size - 1;
	o_head = opipe->head;
	o_mask = opipe->ring_size - 1;

	do {
		if (!opipe->readers) {
			send_sig(SIGPIPE, current, 0);
			if (!ret)
				ret = -EPIPE;
			break;
		}

		i_head = ipipe->head;
		o_tail = opipe->tail;

		 
		if (pipe_empty(i_head, i_tail) ||
		    pipe_full(o_head, o_tail, opipe->max_usage))
			break;

		ibuf = &ipipe->bufs[i_tail & i_mask];
		obuf = &opipe->bufs[o_head & o_mask];

		 
		if (!pipe_buf_get(ipipe, ibuf)) {
			if (ret == 0)
				ret = -EFAULT;
			break;
		}

		*obuf = *ibuf;

		 
		obuf->flags &= ~PIPE_BUF_FLAG_GIFT;
		obuf->flags &= ~PIPE_BUF_FLAG_CAN_MERGE;

		if (obuf->len > len)
			obuf->len = len;
		ret += obuf->len;
		len -= obuf->len;

		o_head++;
		opipe->head = o_head;
		i_tail++;
	} while (len);

	pipe_unlock(ipipe);
	pipe_unlock(opipe);

	 
	if (ret > 0)
		wakeup_pipe_readers(opipe);

	return ret;
}

 
long do_tee(struct file *in, struct file *out, size_t len, unsigned int flags)
{
	struct pipe_inode_info *ipipe = get_pipe_info(in, true);
	struct pipe_inode_info *opipe = get_pipe_info(out, true);
	int ret = -EINVAL;

	if (unlikely(!(in->f_mode & FMODE_READ) ||
		     !(out->f_mode & FMODE_WRITE)))
		return -EBADF;

	 
	if (ipipe && opipe && ipipe != opipe) {
		if ((in->f_flags | out->f_flags) & O_NONBLOCK)
			flags |= SPLICE_F_NONBLOCK;

		 
		ret = ipipe_prep(ipipe, flags);
		if (!ret) {
			ret = opipe_prep(opipe, flags);
			if (!ret)
				ret = link_pipe(ipipe, opipe, len, flags);
		}
	}

	if (ret > 0) {
		fsnotify_access(in);
		fsnotify_modify(out);
	}

	return ret;
}

SYSCALL_DEFINE4(tee, int, fdin, int, fdout, size_t, len, unsigned int, flags)
{
	struct fd in, out;
	int error;

	if (unlikely(flags & ~SPLICE_F_ALL))
		return -EINVAL;

	if (unlikely(!len))
		return 0;

	error = -EBADF;
	in = fdget(fdin);
	if (in.file) {
		out = fdget(fdout);
		if (out.file) {
			error = do_tee(in.file, out.file, len, flags);
			fdput(out);
		}
 		fdput(in);
 	}

	return error;
}
