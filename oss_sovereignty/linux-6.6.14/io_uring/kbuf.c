
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/poll.h>
#include <linux/io_uring.h>

#include <uapi/linux/io_uring.h>

#include "io_uring.h"
#include "opdef.h"
#include "kbuf.h"

#define IO_BUFFER_LIST_BUF_PER_PAGE (PAGE_SIZE / sizeof(struct io_uring_buf))

#define BGID_ARRAY	64

 
#define MAX_BIDS_PER_BGID (1 << 16)

struct io_provide_buf {
	struct file			*file;
	__u64				addr;
	__u32				len;
	__u32				bgid;
	__u32				nbufs;
	__u16				bid;
};

static struct io_buffer_list *__io_buffer_get_list(struct io_ring_ctx *ctx,
						   struct io_buffer_list *bl,
						   unsigned int bgid)
{
	if (bl && bgid < BGID_ARRAY)
		return &bl[bgid];

	return xa_load(&ctx->io_bl_xa, bgid);
}

struct io_buf_free {
	struct hlist_node		list;
	void				*mem;
	size_t				size;
	int				inuse;
};

static inline struct io_buffer_list *io_buffer_get_list(struct io_ring_ctx *ctx,
							unsigned int bgid)
{
	lockdep_assert_held(&ctx->uring_lock);

	return __io_buffer_get_list(ctx, ctx->io_bl, bgid);
}

static int io_buffer_add_list(struct io_ring_ctx *ctx,
			      struct io_buffer_list *bl, unsigned int bgid)
{
	 
	bl->bgid = bgid;
	smp_store_release(&bl->is_ready, 1);

	if (bgid < BGID_ARRAY)
		return 0;

	return xa_err(xa_store(&ctx->io_bl_xa, bgid, bl, GFP_KERNEL));
}

void io_kbuf_recycle_legacy(struct io_kiocb *req, unsigned issue_flags)
{
	struct io_ring_ctx *ctx = req->ctx;
	struct io_buffer_list *bl;
	struct io_buffer *buf;

	 
	if (req->flags & REQ_F_PARTIAL_IO)
		return;

	io_ring_submit_lock(ctx, issue_flags);

	buf = req->kbuf;
	bl = io_buffer_get_list(ctx, buf->bgid);
	list_add(&buf->list, &bl->buf_list);
	req->flags &= ~REQ_F_BUFFER_SELECTED;
	req->buf_index = buf->bgid;

	io_ring_submit_unlock(ctx, issue_flags);
	return;
}

unsigned int __io_put_kbuf(struct io_kiocb *req, unsigned issue_flags)
{
	unsigned int cflags;

	 
	if (req->flags & REQ_F_BUFFER_RING) {
		 
		cflags = __io_put_kbuf_list(req, NULL);
	} else if (issue_flags & IO_URING_F_UNLOCKED) {
		struct io_ring_ctx *ctx = req->ctx;

		spin_lock(&ctx->completion_lock);
		cflags = __io_put_kbuf_list(req, &ctx->io_buffers_comp);
		spin_unlock(&ctx->completion_lock);
	} else {
		lockdep_assert_held(&req->ctx->uring_lock);

		cflags = __io_put_kbuf_list(req, &req->ctx->io_buffers_cache);
	}
	return cflags;
}

static void __user *io_provided_buffer_select(struct io_kiocb *req, size_t *len,
					      struct io_buffer_list *bl)
{
	if (!list_empty(&bl->buf_list)) {
		struct io_buffer *kbuf;

		kbuf = list_first_entry(&bl->buf_list, struct io_buffer, list);
		list_del(&kbuf->list);
		if (*len == 0 || *len > kbuf->len)
			*len = kbuf->len;
		req->flags |= REQ_F_BUFFER_SELECTED;
		req->kbuf = kbuf;
		req->buf_index = kbuf->bid;
		return u64_to_user_ptr(kbuf->addr);
	}
	return NULL;
}

static void __user *io_ring_buffer_select(struct io_kiocb *req, size_t *len,
					  struct io_buffer_list *bl,
					  unsigned int issue_flags)
{
	struct io_uring_buf_ring *br = bl->buf_ring;
	struct io_uring_buf *buf;
	__u16 head = bl->head;

	if (unlikely(smp_load_acquire(&br->tail) == head))
		return NULL;

	head &= bl->mask;
	 
	if (bl->is_mmap || head < IO_BUFFER_LIST_BUF_PER_PAGE) {
		buf = &br->bufs[head];
	} else {
		int off = head & (IO_BUFFER_LIST_BUF_PER_PAGE - 1);
		int index = head / IO_BUFFER_LIST_BUF_PER_PAGE;
		buf = page_address(bl->buf_pages[index]);
		buf += off;
	}
	if (*len == 0 || *len > buf->len)
		*len = buf->len;
	req->flags |= REQ_F_BUFFER_RING;
	req->buf_list = bl;
	req->buf_index = buf->bid;

	if (issue_flags & IO_URING_F_UNLOCKED || !file_can_poll(req->file)) {
		 
		req->buf_list = NULL;
		bl->head++;
	}
	return u64_to_user_ptr(buf->addr);
}

void __user *io_buffer_select(struct io_kiocb *req, size_t *len,
			      unsigned int issue_flags)
{
	struct io_ring_ctx *ctx = req->ctx;
	struct io_buffer_list *bl;
	void __user *ret = NULL;

	io_ring_submit_lock(req->ctx, issue_flags);

	bl = io_buffer_get_list(ctx, req->buf_index);
	if (likely(bl)) {
		if (bl->is_mapped)
			ret = io_ring_buffer_select(req, len, bl, issue_flags);
		else
			ret = io_provided_buffer_select(req, len, bl);
	}
	io_ring_submit_unlock(req->ctx, issue_flags);
	return ret;
}

static __cold int io_init_bl_list(struct io_ring_ctx *ctx)
{
	struct io_buffer_list *bl;
	int i;

	bl = kcalloc(BGID_ARRAY, sizeof(struct io_buffer_list), GFP_KERNEL);
	if (!bl)
		return -ENOMEM;

	for (i = 0; i < BGID_ARRAY; i++) {
		INIT_LIST_HEAD(&bl[i].buf_list);
		bl[i].bgid = i;
	}

	smp_store_release(&ctx->io_bl, bl);
	return 0;
}

 
static void io_kbuf_mark_free(struct io_ring_ctx *ctx, struct io_buffer_list *bl)
{
	struct io_buf_free *ibf;

	hlist_for_each_entry(ibf, &ctx->io_buf_list, list) {
		if (bl->buf_ring == ibf->mem) {
			ibf->inuse = 0;
			return;
		}
	}

	 
	WARN_ON_ONCE(1);
}

static int __io_remove_buffers(struct io_ring_ctx *ctx,
			       struct io_buffer_list *bl, unsigned nbufs)
{
	unsigned i = 0;

	 
	if (!nbufs)
		return 0;

	if (bl->is_mapped) {
		i = bl->buf_ring->tail - bl->head;
		if (bl->is_mmap) {
			 
			io_kbuf_mark_free(ctx, bl);
			bl->buf_ring = NULL;
			bl->is_mmap = 0;
		} else if (bl->buf_nr_pages) {
			int j;

			for (j = 0; j < bl->buf_nr_pages; j++)
				unpin_user_page(bl->buf_pages[j]);
			kvfree(bl->buf_pages);
			bl->buf_pages = NULL;
			bl->buf_nr_pages = 0;
		}
		 
		INIT_LIST_HEAD(&bl->buf_list);
		bl->is_mapped = 0;
		return i;
	}

	 
	lockdep_assert_held(&ctx->uring_lock);

	while (!list_empty(&bl->buf_list)) {
		struct io_buffer *nxt;

		nxt = list_first_entry(&bl->buf_list, struct io_buffer, list);
		list_move(&nxt->list, &ctx->io_buffers_cache);
		if (++i == nbufs)
			return i;
		cond_resched();
	}

	return i;
}

void io_destroy_buffers(struct io_ring_ctx *ctx)
{
	struct io_buffer_list *bl;
	unsigned long index;
	int i;

	for (i = 0; i < BGID_ARRAY; i++) {
		if (!ctx->io_bl)
			break;
		__io_remove_buffers(ctx, &ctx->io_bl[i], -1U);
	}

	xa_for_each(&ctx->io_bl_xa, index, bl) {
		xa_erase(&ctx->io_bl_xa, bl->bgid);
		__io_remove_buffers(ctx, bl, -1U);
		kfree_rcu(bl, rcu);
	}

	while (!list_empty(&ctx->io_buffers_pages)) {
		struct page *page;

		page = list_first_entry(&ctx->io_buffers_pages, struct page, lru);
		list_del_init(&page->lru);
		__free_page(page);
	}
}

int io_remove_buffers_prep(struct io_kiocb *req, const struct io_uring_sqe *sqe)
{
	struct io_provide_buf *p = io_kiocb_to_cmd(req, struct io_provide_buf);
	u64 tmp;

	if (sqe->rw_flags || sqe->addr || sqe->len || sqe->off ||
	    sqe->splice_fd_in)
		return -EINVAL;

	tmp = READ_ONCE(sqe->fd);
	if (!tmp || tmp > MAX_BIDS_PER_BGID)
		return -EINVAL;

	memset(p, 0, sizeof(*p));
	p->nbufs = tmp;
	p->bgid = READ_ONCE(sqe->buf_group);
	return 0;
}

int io_remove_buffers(struct io_kiocb *req, unsigned int issue_flags)
{
	struct io_provide_buf *p = io_kiocb_to_cmd(req, struct io_provide_buf);
	struct io_ring_ctx *ctx = req->ctx;
	struct io_buffer_list *bl;
	int ret = 0;

	io_ring_submit_lock(ctx, issue_flags);

	ret = -ENOENT;
	bl = io_buffer_get_list(ctx, p->bgid);
	if (bl) {
		ret = -EINVAL;
		 
		if (!bl->is_mapped)
			ret = __io_remove_buffers(ctx, bl, p->nbufs);
	}
	io_ring_submit_unlock(ctx, issue_flags);
	if (ret < 0)
		req_set_fail(req);
	io_req_set_res(req, ret, 0);
	return IOU_OK;
}

int io_provide_buffers_prep(struct io_kiocb *req, const struct io_uring_sqe *sqe)
{
	unsigned long size, tmp_check;
	struct io_provide_buf *p = io_kiocb_to_cmd(req, struct io_provide_buf);
	u64 tmp;

	if (sqe->rw_flags || sqe->splice_fd_in)
		return -EINVAL;

	tmp = READ_ONCE(sqe->fd);
	if (!tmp || tmp > MAX_BIDS_PER_BGID)
		return -E2BIG;
	p->nbufs = tmp;
	p->addr = READ_ONCE(sqe->addr);
	p->len = READ_ONCE(sqe->len);

	if (check_mul_overflow((unsigned long)p->len, (unsigned long)p->nbufs,
				&size))
		return -EOVERFLOW;
	if (check_add_overflow((unsigned long)p->addr, size, &tmp_check))
		return -EOVERFLOW;

	size = (unsigned long)p->len * p->nbufs;
	if (!access_ok(u64_to_user_ptr(p->addr), size))
		return -EFAULT;

	p->bgid = READ_ONCE(sqe->buf_group);
	tmp = READ_ONCE(sqe->off);
	if (tmp > USHRT_MAX)
		return -E2BIG;
	if (tmp + p->nbufs > MAX_BIDS_PER_BGID)
		return -EINVAL;
	p->bid = tmp;
	return 0;
}

static int io_refill_buffer_cache(struct io_ring_ctx *ctx)
{
	struct io_buffer *buf;
	struct page *page;
	int bufs_in_page;

	 
	if (!list_empty_careful(&ctx->io_buffers_comp)) {
		spin_lock(&ctx->completion_lock);
		if (!list_empty(&ctx->io_buffers_comp)) {
			list_splice_init(&ctx->io_buffers_comp,
						&ctx->io_buffers_cache);
			spin_unlock(&ctx->completion_lock);
			return 0;
		}
		spin_unlock(&ctx->completion_lock);
	}

	 
	page = alloc_page(GFP_KERNEL_ACCOUNT);
	if (!page)
		return -ENOMEM;

	list_add(&page->lru, &ctx->io_buffers_pages);

	buf = page_address(page);
	bufs_in_page = PAGE_SIZE / sizeof(*buf);
	while (bufs_in_page) {
		list_add_tail(&buf->list, &ctx->io_buffers_cache);
		buf++;
		bufs_in_page--;
	}

	return 0;
}

static int io_add_buffers(struct io_ring_ctx *ctx, struct io_provide_buf *pbuf,
			  struct io_buffer_list *bl)
{
	struct io_buffer *buf;
	u64 addr = pbuf->addr;
	int i, bid = pbuf->bid;

	for (i = 0; i < pbuf->nbufs; i++) {
		if (list_empty(&ctx->io_buffers_cache) &&
		    io_refill_buffer_cache(ctx))
			break;
		buf = list_first_entry(&ctx->io_buffers_cache, struct io_buffer,
					list);
		list_move_tail(&buf->list, &bl->buf_list);
		buf->addr = addr;
		buf->len = min_t(__u32, pbuf->len, MAX_RW_COUNT);
		buf->bid = bid;
		buf->bgid = pbuf->bgid;
		addr += pbuf->len;
		bid++;
		cond_resched();
	}

	return i ? 0 : -ENOMEM;
}

int io_provide_buffers(struct io_kiocb *req, unsigned int issue_flags)
{
	struct io_provide_buf *p = io_kiocb_to_cmd(req, struct io_provide_buf);
	struct io_ring_ctx *ctx = req->ctx;
	struct io_buffer_list *bl;
	int ret = 0;

	io_ring_submit_lock(ctx, issue_flags);

	if (unlikely(p->bgid < BGID_ARRAY && !ctx->io_bl)) {
		ret = io_init_bl_list(ctx);
		if (ret)
			goto err;
	}

	bl = io_buffer_get_list(ctx, p->bgid);
	if (unlikely(!bl)) {
		bl = kzalloc(sizeof(*bl), GFP_KERNEL_ACCOUNT);
		if (!bl) {
			ret = -ENOMEM;
			goto err;
		}
		INIT_LIST_HEAD(&bl->buf_list);
		ret = io_buffer_add_list(ctx, bl, p->bgid);
		if (ret) {
			 
			if (p->bgid >= BGID_ARRAY)
				kfree_rcu(bl, rcu);
			else
				WARN_ON_ONCE(1);
			goto err;
		}
	}
	 
	if (bl->is_mapped) {
		ret = -EINVAL;
		goto err;
	}

	ret = io_add_buffers(ctx, p, bl);
err:
	io_ring_submit_unlock(ctx, issue_flags);

	if (ret < 0)
		req_set_fail(req);
	io_req_set_res(req, ret, 0);
	return IOU_OK;
}

static int io_pin_pbuf_ring(struct io_uring_buf_reg *reg,
			    struct io_buffer_list *bl)
{
	struct io_uring_buf_ring *br;
	struct page **pages;
	int i, nr_pages;

	pages = io_pin_pages(reg->ring_addr,
			     flex_array_size(br, bufs, reg->ring_entries),
			     &nr_pages);
	if (IS_ERR(pages))
		return PTR_ERR(pages);

	 
	for (i = 0; i < nr_pages; i++)
		if (PageHighMem(pages[i]))
			goto error_unpin;

	br = page_address(pages[0]);
#ifdef SHM_COLOUR
	 
	if ((reg->ring_addr | (unsigned long) br) & (SHM_COLOUR - 1))
		goto error_unpin;
#endif
	bl->buf_pages = pages;
	bl->buf_nr_pages = nr_pages;
	bl->buf_ring = br;
	bl->is_mapped = 1;
	bl->is_mmap = 0;
	return 0;
error_unpin:
	for (i = 0; i < nr_pages; i++)
		unpin_user_page(pages[i]);
	kvfree(pages);
	return -EINVAL;
}

 
static struct io_buf_free *io_lookup_buf_free_entry(struct io_ring_ctx *ctx,
						    size_t ring_size)
{
	struct io_buf_free *ibf, *best = NULL;
	size_t best_dist;

	hlist_for_each_entry(ibf, &ctx->io_buf_list, list) {
		size_t dist;

		if (ibf->inuse || ibf->size < ring_size)
			continue;
		dist = ibf->size - ring_size;
		if (!best || dist < best_dist) {
			best = ibf;
			if (!dist)
				break;
			best_dist = dist;
		}
	}

	return best;
}

static int io_alloc_pbuf_ring(struct io_ring_ctx *ctx,
			      struct io_uring_buf_reg *reg,
			      struct io_buffer_list *bl)
{
	struct io_buf_free *ibf;
	size_t ring_size;
	void *ptr;

	ring_size = reg->ring_entries * sizeof(struct io_uring_buf_ring);

	 
	ibf = io_lookup_buf_free_entry(ctx, ring_size);
	if (!ibf) {
		ptr = io_mem_alloc(ring_size);
		if (IS_ERR(ptr))
			return PTR_ERR(ptr);

		 
		ibf = kmalloc(sizeof(*ibf), GFP_KERNEL_ACCOUNT);
		if (!ibf) {
			io_mem_free(ptr);
			return -ENOMEM;
		}
		ibf->mem = ptr;
		ibf->size = ring_size;
		hlist_add_head(&ibf->list, &ctx->io_buf_list);
	}
	ibf->inuse = 1;
	bl->buf_ring = ibf->mem;
	bl->is_mapped = 1;
	bl->is_mmap = 1;
	return 0;
}

int io_register_pbuf_ring(struct io_ring_ctx *ctx, void __user *arg)
{
	struct io_uring_buf_reg reg;
	struct io_buffer_list *bl, *free_bl = NULL;
	int ret;

	lockdep_assert_held(&ctx->uring_lock);

	if (copy_from_user(&reg, arg, sizeof(reg)))
		return -EFAULT;

	if (reg.resv[0] || reg.resv[1] || reg.resv[2])
		return -EINVAL;
	if (reg.flags & ~IOU_PBUF_RING_MMAP)
		return -EINVAL;
	if (!(reg.flags & IOU_PBUF_RING_MMAP)) {
		if (!reg.ring_addr)
			return -EFAULT;
		if (reg.ring_addr & ~PAGE_MASK)
			return -EINVAL;
	} else {
		if (reg.ring_addr)
			return -EINVAL;
	}

	if (!is_power_of_2(reg.ring_entries))
		return -EINVAL;

	 
	if (reg.ring_entries >= 65536)
		return -EINVAL;

	if (unlikely(reg.bgid < BGID_ARRAY && !ctx->io_bl)) {
		int ret = io_init_bl_list(ctx);
		if (ret)
			return ret;
	}

	bl = io_buffer_get_list(ctx, reg.bgid);
	if (bl) {
		 
		if (bl->is_mapped || !list_empty(&bl->buf_list))
			return -EEXIST;
	} else {
		free_bl = bl = kzalloc(sizeof(*bl), GFP_KERNEL);
		if (!bl)
			return -ENOMEM;
	}

	if (!(reg.flags & IOU_PBUF_RING_MMAP))
		ret = io_pin_pbuf_ring(&reg, bl);
	else
		ret = io_alloc_pbuf_ring(ctx, &reg, bl);

	if (!ret) {
		bl->nr_entries = reg.ring_entries;
		bl->mask = reg.ring_entries - 1;

		io_buffer_add_list(ctx, bl, reg.bgid);
		return 0;
	}

	kfree_rcu(free_bl, rcu);
	return ret;
}

int io_unregister_pbuf_ring(struct io_ring_ctx *ctx, void __user *arg)
{
	struct io_uring_buf_reg reg;
	struct io_buffer_list *bl;

	lockdep_assert_held(&ctx->uring_lock);

	if (copy_from_user(&reg, arg, sizeof(reg)))
		return -EFAULT;
	if (reg.resv[0] || reg.resv[1] || reg.resv[2])
		return -EINVAL;
	if (reg.flags)
		return -EINVAL;

	bl = io_buffer_get_list(ctx, reg.bgid);
	if (!bl)
		return -ENOENT;
	if (!bl->is_mapped)
		return -EINVAL;

	__io_remove_buffers(ctx, bl, -1U);
	if (bl->bgid >= BGID_ARRAY) {
		xa_erase(&ctx->io_bl_xa, bl->bgid);
		kfree_rcu(bl, rcu);
	}
	return 0;
}

void *io_pbuf_get_address(struct io_ring_ctx *ctx, unsigned long bgid)
{
	struct io_buffer_list *bl;

	bl = __io_buffer_get_list(ctx, smp_load_acquire(&ctx->io_bl), bgid);

	if (!bl || !bl->is_mmap)
		return NULL;
	 
	if (!smp_load_acquire(&bl->is_ready))
		return NULL;

	return bl->buf_ring;
}

 
void io_kbuf_mmap_list_free(struct io_ring_ctx *ctx)
{
	struct io_buf_free *ibf;
	struct hlist_node *tmp;

	hlist_for_each_entry_safe(ibf, tmp, &ctx->io_buf_list, list) {
		hlist_del(&ibf->list);
		io_mem_free(ibf->mem);
		kfree(ibf);
	}
}
