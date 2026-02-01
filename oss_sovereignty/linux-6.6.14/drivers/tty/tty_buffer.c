
 

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include "tty.h"

#define MIN_TTYB_SIZE	256
#define TTYB_ALIGN_MASK	0xff

 
#define TTYB_DEFAULT_MEM_LIMIT	(640 * 1024UL)

 

#define TTY_BUFFER_PAGE	(((PAGE_SIZE - sizeof(struct tty_buffer)) / 2) & ~TTYB_ALIGN_MASK)

 
void tty_buffer_lock_exclusive(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;

	atomic_inc(&buf->priority);
	mutex_lock(&buf->lock);
}
EXPORT_SYMBOL_GPL(tty_buffer_lock_exclusive);

 
void tty_buffer_unlock_exclusive(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;
	int restart;

	restart = buf->head->commit != buf->head->read;

	atomic_dec(&buf->priority);
	mutex_unlock(&buf->lock);
	if (restart)
		queue_work(system_unbound_wq, &buf->work);
}
EXPORT_SYMBOL_GPL(tty_buffer_unlock_exclusive);

 
unsigned int tty_buffer_space_avail(struct tty_port *port)
{
	int space = port->buf.mem_limit - atomic_read(&port->buf.mem_used);

	return max(space, 0);
}
EXPORT_SYMBOL_GPL(tty_buffer_space_avail);

static void tty_buffer_reset(struct tty_buffer *p, size_t size)
{
	p->used = 0;
	p->size = size;
	p->next = NULL;
	p->commit = 0;
	p->lookahead = 0;
	p->read = 0;
	p->flags = true;
}

 
void tty_buffer_free_all(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;
	struct tty_buffer *p, *next;
	struct llist_node *llist;
	unsigned int freed = 0;
	int still_used;

	while ((p = buf->head) != NULL) {
		buf->head = p->next;
		freed += p->size;
		if (p->size > 0)
			kfree(p);
	}
	llist = llist_del_all(&buf->free);
	llist_for_each_entry_safe(p, next, llist, free)
		kfree(p);

	tty_buffer_reset(&buf->sentinel, 0);
	buf->head = &buf->sentinel;
	buf->tail = &buf->sentinel;

	still_used = atomic_xchg(&buf->mem_used, 0);
	WARN(still_used != freed, "we still have not freed %d bytes!",
			still_used - freed);
}

 
static struct tty_buffer *tty_buffer_alloc(struct tty_port *port, size_t size)
{
	struct llist_node *free;
	struct tty_buffer *p;

	 
	size = __ALIGN_MASK(size, TTYB_ALIGN_MASK);

	if (size <= MIN_TTYB_SIZE) {
		free = llist_del_first(&port->buf.free);
		if (free) {
			p = llist_entry(free, struct tty_buffer, free);
			goto found;
		}
	}

	 
	if (atomic_read(&port->buf.mem_used) > port->buf.mem_limit)
		return NULL;
	p = kmalloc(struct_size(p, data, 2 * size), GFP_ATOMIC | __GFP_NOWARN);
	if (p == NULL)
		return NULL;

found:
	tty_buffer_reset(p, size);
	atomic_add(size, &port->buf.mem_used);
	return p;
}

 
static void tty_buffer_free(struct tty_port *port, struct tty_buffer *b)
{
	struct tty_bufhead *buf = &port->buf;

	 
	WARN_ON(atomic_sub_return(b->size, &buf->mem_used) < 0);

	if (b->size > MIN_TTYB_SIZE)
		kfree(b);
	else if (b->size > 0)
		llist_add(&b->free, &buf->free);
}

 
void tty_buffer_flush(struct tty_struct *tty, struct tty_ldisc *ld)
{
	struct tty_port *port = tty->port;
	struct tty_bufhead *buf = &port->buf;
	struct tty_buffer *next;

	atomic_inc(&buf->priority);

	mutex_lock(&buf->lock);
	 
	while ((next = smp_load_acquire(&buf->head->next)) != NULL) {
		tty_buffer_free(port, buf->head);
		buf->head = next;
	}
	buf->head->read = buf->head->commit;
	buf->head->lookahead = buf->head->read;

	if (ld && ld->ops->flush_buffer)
		ld->ops->flush_buffer(tty);

	atomic_dec(&buf->priority);
	mutex_unlock(&buf->lock);
}

 
static int __tty_buffer_request_room(struct tty_port *port, size_t size,
				     bool flags)
{
	struct tty_bufhead *buf = &port->buf;
	struct tty_buffer *n, *b = buf->tail;
	size_t left = (b->flags ? 1 : 2) * b->size - b->used;
	bool change = !b->flags && flags;

	if (!change && left >= size)
		return size;

	 
	n = tty_buffer_alloc(port, size);
	if (n == NULL)
		return change ? 0 : left;

	n->flags = flags;
	buf->tail = n;
	 
	smp_store_release(&b->commit, b->used);
	 
	smp_store_release(&b->next, n);

	return size;
}

int tty_buffer_request_room(struct tty_port *port, size_t size)
{
	return __tty_buffer_request_room(port, size, true);
}
EXPORT_SYMBOL_GPL(tty_buffer_request_room);

size_t __tty_insert_flip_string_flags(struct tty_port *port, const u8 *chars,
				      const u8 *flags, bool mutable_flags,
				      size_t size)
{
	bool need_flags = mutable_flags || flags[0] != TTY_NORMAL;
	size_t copied = 0;

	do {
		size_t goal = min_t(size_t, size - copied, TTY_BUFFER_PAGE);
		size_t space = __tty_buffer_request_room(port, goal, need_flags);
		struct tty_buffer *tb = port->buf.tail;

		if (unlikely(space == 0))
			break;

		memcpy(char_buf_ptr(tb, tb->used), chars, space);

		if (mutable_flags) {
			memcpy(flag_buf_ptr(tb, tb->used), flags, space);
			flags += space;
		} else if (tb->flags) {
			memset(flag_buf_ptr(tb, tb->used), flags[0], space);
		} else {
			 
			WARN_ON_ONCE(need_flags);
		}

		tb->used += space;
		copied += space;
		chars += space;

		 
	} while (unlikely(size > copied));

	return copied;
}
EXPORT_SYMBOL(__tty_insert_flip_string_flags);

 
size_t tty_prepare_flip_string(struct tty_port *port, u8 **chars, size_t size)
{
	size_t space = __tty_buffer_request_room(port, size, false);

	if (likely(space)) {
		struct tty_buffer *tb = port->buf.tail;

		*chars = char_buf_ptr(tb, tb->used);
		if (tb->flags)
			memset(flag_buf_ptr(tb, tb->used), TTY_NORMAL, space);
		tb->used += space;
	}

	return space;
}
EXPORT_SYMBOL_GPL(tty_prepare_flip_string);

 
size_t tty_ldisc_receive_buf(struct tty_ldisc *ld, const u8 *p, const u8 *f,
			     size_t count)
{
	if (ld->ops->receive_buf2)
		count = ld->ops->receive_buf2(ld->tty, p, f, count);
	else {
		count = min_t(size_t, count, ld->tty->receive_room);
		if (count && ld->ops->receive_buf)
			ld->ops->receive_buf(ld->tty, p, f, count);
	}
	return count;
}
EXPORT_SYMBOL_GPL(tty_ldisc_receive_buf);

static void lookahead_bufs(struct tty_port *port, struct tty_buffer *head)
{
	head->lookahead = max(head->lookahead, head->read);

	while (head) {
		struct tty_buffer *next;
		unsigned int count;

		 
		next = smp_load_acquire(&head->next);
		 
		count = smp_load_acquire(&head->commit) - head->lookahead;
		if (!count) {
			head = next;
			continue;
		}

		if (port->client_ops->lookahead_buf) {
			u8 *p, *f = NULL;

			p = char_buf_ptr(head, head->lookahead);
			if (head->flags)
				f = flag_buf_ptr(head, head->lookahead);

			port->client_ops->lookahead_buf(port, p, f, count);
		}

		head->lookahead += count;
	}
}

static size_t
receive_buf(struct tty_port *port, struct tty_buffer *head, size_t count)
{
	u8 *p = char_buf_ptr(head, head->read);
	const u8 *f = NULL;
	size_t n;

	if (head->flags)
		f = flag_buf_ptr(head, head->read);

	n = port->client_ops->receive_buf(port, p, f, count);
	if (n > 0)
		memset(p, 0, n);
	return n;
}

 
static void flush_to_ldisc(struct work_struct *work)
{
	struct tty_port *port = container_of(work, struct tty_port, buf.work);
	struct tty_bufhead *buf = &port->buf;

	mutex_lock(&buf->lock);

	while (1) {
		struct tty_buffer *head = buf->head;
		struct tty_buffer *next;
		size_t count, rcvd;

		 
		if (atomic_read(&buf->priority))
			break;

		 
		next = smp_load_acquire(&head->next);
		 
		count = smp_load_acquire(&head->commit) - head->read;
		if (!count) {
			if (next == NULL)
				break;
			buf->head = next;
			tty_buffer_free(port, head);
			continue;
		}

		rcvd = receive_buf(port, head, count);
		head->read += rcvd;
		if (rcvd < count)
			lookahead_bufs(port, head);
		if (!rcvd)
			break;

		if (need_resched())
			cond_resched();
	}

	mutex_unlock(&buf->lock);

}

static inline void tty_flip_buffer_commit(struct tty_buffer *tail)
{
	 
	smp_store_release(&tail->commit, tail->used);
}

 
void tty_flip_buffer_push(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;

	tty_flip_buffer_commit(buf->tail);
	queue_work(system_unbound_wq, &buf->work);
}
EXPORT_SYMBOL(tty_flip_buffer_push);

 
int tty_insert_flip_string_and_push_buffer(struct tty_port *port,
					   const u8 *chars, size_t size)
{
	struct tty_bufhead *buf = &port->buf;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	size = tty_insert_flip_string(port, chars, size);
	if (size)
		tty_flip_buffer_commit(buf->tail);
	spin_unlock_irqrestore(&port->lock, flags);

	queue_work(system_unbound_wq, &buf->work);

	return size;
}

 
void tty_buffer_init(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;

	mutex_init(&buf->lock);
	tty_buffer_reset(&buf->sentinel, 0);
	buf->head = &buf->sentinel;
	buf->tail = &buf->sentinel;
	init_llist_head(&buf->free);
	atomic_set(&buf->mem_used, 0);
	atomic_set(&buf->priority, 0);
	INIT_WORK(&buf->work, flush_to_ldisc);
	buf->mem_limit = TTYB_DEFAULT_MEM_LIMIT;
}

 
int tty_buffer_set_limit(struct tty_port *port, int limit)
{
	if (limit < MIN_TTYB_SIZE)
		return -EINVAL;
	port->buf.mem_limit = limit;
	return 0;
}
EXPORT_SYMBOL_GPL(tty_buffer_set_limit);

 
void tty_buffer_set_lock_subclass(struct tty_port *port)
{
	lockdep_set_subclass(&port->buf.lock, TTY_LOCK_SLAVE);
}

bool tty_buffer_restart_work(struct tty_port *port)
{
	return queue_work(system_unbound_wq, &port->buf.work);
}

bool tty_buffer_cancel_work(struct tty_port *port)
{
	return cancel_work_sync(&port->buf.work);
}

void tty_buffer_flush_work(struct tty_port *port)
{
	flush_work(&port->buf.work);
}
