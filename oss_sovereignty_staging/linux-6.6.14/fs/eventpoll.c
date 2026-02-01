
 

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/rbtree.h>
#include <linux/wait.h>
#include <linux/eventpoll.h>
#include <linux/mount.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/anon_inodes.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/mman.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/compat.h>
#include <linux/rculist.h>
#include <net/busy_poll.h>

 

 
#define EP_PRIVATE_BITS (EPOLLWAKEUP | EPOLLONESHOT | EPOLLET | EPOLLEXCLUSIVE)

#define EPOLLINOUT_BITS (EPOLLIN | EPOLLOUT)

#define EPOLLEXCLUSIVE_OK_BITS (EPOLLINOUT_BITS | EPOLLERR | EPOLLHUP | \
				EPOLLWAKEUP | EPOLLET | EPOLLEXCLUSIVE)

 
#define EP_MAX_NESTS 4

#define EP_MAX_EVENTS (INT_MAX / sizeof(struct epoll_event))

#define EP_UNACTIVE_PTR ((void *) -1L)

#define EP_ITEM_COST (sizeof(struct epitem) + sizeof(struct eppoll_entry))

struct epoll_filefd {
	struct file *file;
	int fd;
} __packed;

 
struct eppoll_entry {
	 
	struct eppoll_entry *next;

	 
	struct epitem *base;

	 
	wait_queue_entry_t wait;

	 
	wait_queue_head_t *whead;
};

 
struct epitem {
	union {
		 
		struct rb_node rbn;
		 
		struct rcu_head rcu;
	};

	 
	struct list_head rdllink;

	 
	struct epitem *next;

	 
	struct epoll_filefd ffd;

	 
	bool dying;

	 
	struct eppoll_entry *pwqlist;

	 
	struct eventpoll *ep;

	 
	struct hlist_node fllink;

	 
	struct wakeup_source __rcu *ws;

	 
	struct epoll_event event;
};

 
struct eventpoll {
	 
	struct mutex mtx;

	 
	wait_queue_head_t wq;

	 
	wait_queue_head_t poll_wait;

	 
	struct list_head rdllist;

	 
	rwlock_t lock;

	 
	struct rb_root_cached rbr;

	 
	struct epitem *ovflist;

	 
	struct wakeup_source *ws;

	 
	struct user_struct *user;

	struct file *file;

	 
	u64 gen;
	struct hlist_head refs;

	 
	refcount_t refcount;

#ifdef CONFIG_NET_RX_BUSY_POLL
	 
	unsigned int napi_id;
#endif

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	 
	u8 nests;
#endif
};

 
struct ep_pqueue {
	poll_table pt;
	struct epitem *epi;
};

 
 
static long max_user_watches __read_mostly;

 
static DEFINE_MUTEX(epnested_mutex);

static u64 loop_check_gen = 0;

 
static struct eventpoll *inserting_into;

 
static struct kmem_cache *epi_cache __read_mostly;

 
static struct kmem_cache *pwq_cache __read_mostly;

 
struct epitems_head {
	struct hlist_head epitems;
	struct epitems_head *next;
};
static struct epitems_head *tfile_check_list = EP_UNACTIVE_PTR;

static struct kmem_cache *ephead_cache __read_mostly;

static inline void free_ephead(struct epitems_head *head)
{
	if (head)
		kmem_cache_free(ephead_cache, head);
}

static void list_file(struct file *file)
{
	struct epitems_head *head;

	head = container_of(file->f_ep, struct epitems_head, epitems);
	if (!head->next) {
		head->next = tfile_check_list;
		tfile_check_list = head;
	}
}

static void unlist_file(struct epitems_head *head)
{
	struct epitems_head *to_free = head;
	struct hlist_node *p = rcu_dereference(hlist_first_rcu(&head->epitems));
	if (p) {
		struct epitem *epi= container_of(p, struct epitem, fllink);
		spin_lock(&epi->ffd.file->f_lock);
		if (!hlist_empty(&head->epitems))
			to_free = NULL;
		head->next = NULL;
		spin_unlock(&epi->ffd.file->f_lock);
	}
	free_ephead(to_free);
}

#ifdef CONFIG_SYSCTL

#include <linux/sysctl.h>

static long long_zero;
static long long_max = LONG_MAX;

static struct ctl_table epoll_table[] = {
	{
		.procname	= "max_user_watches",
		.data		= &max_user_watches,
		.maxlen		= sizeof(max_user_watches),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &long_zero,
		.extra2		= &long_max,
	},
	{ }
};

static void __init epoll_sysctls_init(void)
{
	register_sysctl("fs/epoll", epoll_table);
}
#else
#define epoll_sysctls_init() do { } while (0)
#endif  

static const struct file_operations eventpoll_fops;

static inline int is_file_epoll(struct file *f)
{
	return f->f_op == &eventpoll_fops;
}

 
static inline void ep_set_ffd(struct epoll_filefd *ffd,
			      struct file *file, int fd)
{
	ffd->file = file;
	ffd->fd = fd;
}

 
static inline int ep_cmp_ffd(struct epoll_filefd *p1,
			     struct epoll_filefd *p2)
{
	return (p1->file > p2->file ? +1:
	        (p1->file < p2->file ? -1 : p1->fd - p2->fd));
}

 
static inline int ep_is_linked(struct epitem *epi)
{
	return !list_empty(&epi->rdllink);
}

static inline struct eppoll_entry *ep_pwq_from_wait(wait_queue_entry_t *p)
{
	return container_of(p, struct eppoll_entry, wait);
}

 
static inline struct epitem *ep_item_from_wait(wait_queue_entry_t *p)
{
	return container_of(p, struct eppoll_entry, wait)->base;
}

 
static inline int ep_events_available(struct eventpoll *ep)
{
	return !list_empty_careful(&ep->rdllist) ||
		READ_ONCE(ep->ovflist) != EP_UNACTIVE_PTR;
}

#ifdef CONFIG_NET_RX_BUSY_POLL
static bool ep_busy_loop_end(void *p, unsigned long start_time)
{
	struct eventpoll *ep = p;

	return ep_events_available(ep) || busy_loop_timeout(start_time);
}

 
static bool ep_busy_loop(struct eventpoll *ep, int nonblock)
{
	unsigned int napi_id = READ_ONCE(ep->napi_id);

	if ((napi_id >= MIN_NAPI_ID) && net_busy_loop_on()) {
		napi_busy_loop(napi_id, nonblock ? NULL : ep_busy_loop_end, ep, false,
			       BUSY_POLL_BUDGET);
		if (ep_events_available(ep))
			return true;
		 
		ep->napi_id = 0;
		return false;
	}
	return false;
}

 
static inline void ep_set_busy_poll_napi_id(struct epitem *epi)
{
	struct eventpoll *ep;
	unsigned int napi_id;
	struct socket *sock;
	struct sock *sk;

	if (!net_busy_loop_on())
		return;

	sock = sock_from_file(epi->ffd.file);
	if (!sock)
		return;

	sk = sock->sk;
	if (!sk)
		return;

	napi_id = READ_ONCE(sk->sk_napi_id);
	ep = epi->ep;

	 
	if (napi_id < MIN_NAPI_ID || napi_id == ep->napi_id)
		return;

	 
	ep->napi_id = napi_id;
}

#else

static inline bool ep_busy_loop(struct eventpoll *ep, int nonblock)
{
	return false;
}

static inline void ep_set_busy_poll_napi_id(struct epitem *epi)
{
}

#endif  

 
#ifdef CONFIG_DEBUG_LOCK_ALLOC

static void ep_poll_safewake(struct eventpoll *ep, struct epitem *epi,
			     unsigned pollflags)
{
	struct eventpoll *ep_src;
	unsigned long flags;
	u8 nests = 0;

	 
	if (epi) {
		if ((is_file_epoll(epi->ffd.file))) {
			ep_src = epi->ffd.file->private_data;
			nests = ep_src->nests;
		} else {
			nests = 1;
		}
	}
	spin_lock_irqsave_nested(&ep->poll_wait.lock, flags, nests);
	ep->nests = nests + 1;
	wake_up_locked_poll(&ep->poll_wait, EPOLLIN | pollflags);
	ep->nests = 0;
	spin_unlock_irqrestore(&ep->poll_wait.lock, flags);
}

#else

static void ep_poll_safewake(struct eventpoll *ep, struct epitem *epi,
			     __poll_t pollflags)
{
	wake_up_poll(&ep->poll_wait, EPOLLIN | pollflags);
}

#endif

static void ep_remove_wait_queue(struct eppoll_entry *pwq)
{
	wait_queue_head_t *whead;

	rcu_read_lock();
	 
	whead = smp_load_acquire(&pwq->whead);
	if (whead)
		remove_wait_queue(whead, &pwq->wait);
	rcu_read_unlock();
}

 
static void ep_unregister_pollwait(struct eventpoll *ep, struct epitem *epi)
{
	struct eppoll_entry **p = &epi->pwqlist;
	struct eppoll_entry *pwq;

	while ((pwq = *p) != NULL) {
		*p = pwq->next;
		ep_remove_wait_queue(pwq);
		kmem_cache_free(pwq_cache, pwq);
	}
}

 
static inline struct wakeup_source *ep_wakeup_source(struct epitem *epi)
{
	return rcu_dereference_check(epi->ws, lockdep_is_held(&epi->ep->mtx));
}

 
static inline void ep_pm_stay_awake(struct epitem *epi)
{
	struct wakeup_source *ws = ep_wakeup_source(epi);

	if (ws)
		__pm_stay_awake(ws);
}

static inline bool ep_has_wakeup_source(struct epitem *epi)
{
	return rcu_access_pointer(epi->ws) ? true : false;
}

 
static inline void ep_pm_stay_awake_rcu(struct epitem *epi)
{
	struct wakeup_source *ws;

	rcu_read_lock();
	ws = rcu_dereference(epi->ws);
	if (ws)
		__pm_stay_awake(ws);
	rcu_read_unlock();
}


 
static void ep_start_scan(struct eventpoll *ep, struct list_head *txlist)
{
	 
	lockdep_assert_irqs_enabled();
	write_lock_irq(&ep->lock);
	list_splice_init(&ep->rdllist, txlist);
	WRITE_ONCE(ep->ovflist, NULL);
	write_unlock_irq(&ep->lock);
}

static void ep_done_scan(struct eventpoll *ep,
			 struct list_head *txlist)
{
	struct epitem *epi, *nepi;

	write_lock_irq(&ep->lock);
	 
	for (nepi = READ_ONCE(ep->ovflist); (epi = nepi) != NULL;
	     nepi = epi->next, epi->next = EP_UNACTIVE_PTR) {
		 
		if (!ep_is_linked(epi)) {
			 
			list_add(&epi->rdllink, &ep->rdllist);
			ep_pm_stay_awake(epi);
		}
	}
	 
	WRITE_ONCE(ep->ovflist, EP_UNACTIVE_PTR);

	 
	list_splice(txlist, &ep->rdllist);
	__pm_relax(ep->ws);

	if (!list_empty(&ep->rdllist)) {
		if (waitqueue_active(&ep->wq))
			wake_up(&ep->wq);
	}

	write_unlock_irq(&ep->lock);
}

static void epi_rcu_free(struct rcu_head *head)
{
	struct epitem *epi = container_of(head, struct epitem, rcu);
	kmem_cache_free(epi_cache, epi);
}

static void ep_get(struct eventpoll *ep)
{
	refcount_inc(&ep->refcount);
}

 
static bool ep_refcount_dec_and_test(struct eventpoll *ep)
{
	if (!refcount_dec_and_test(&ep->refcount))
		return false;

	WARN_ON_ONCE(!RB_EMPTY_ROOT(&ep->rbr.rb_root));
	return true;
}

static void ep_free(struct eventpoll *ep)
{
	mutex_destroy(&ep->mtx);
	free_uid(ep->user);
	wakeup_source_unregister(ep->ws);
	kfree(ep);
}

 
static bool __ep_remove(struct eventpoll *ep, struct epitem *epi, bool force)
{
	struct file *file = epi->ffd.file;
	struct epitems_head *to_free;
	struct hlist_head *head;

	lockdep_assert_irqs_enabled();

	 
	ep_unregister_pollwait(ep, epi);

	 
	spin_lock(&file->f_lock);
	if (epi->dying && !force) {
		spin_unlock(&file->f_lock);
		return false;
	}

	to_free = NULL;
	head = file->f_ep;
	if (head->first == &epi->fllink && !epi->fllink.next) {
		file->f_ep = NULL;
		if (!is_file_epoll(file)) {
			struct epitems_head *v;
			v = container_of(head, struct epitems_head, epitems);
			if (!smp_load_acquire(&v->next))
				to_free = v;
		}
	}
	hlist_del_rcu(&epi->fllink);
	spin_unlock(&file->f_lock);
	free_ephead(to_free);

	rb_erase_cached(&epi->rbn, &ep->rbr);

	write_lock_irq(&ep->lock);
	if (ep_is_linked(epi))
		list_del_init(&epi->rdllink);
	write_unlock_irq(&ep->lock);

	wakeup_source_unregister(ep_wakeup_source(epi));
	 
	call_rcu(&epi->rcu, epi_rcu_free);

	percpu_counter_dec(&ep->user->epoll_watches);
	return ep_refcount_dec_and_test(ep);
}

 
static void ep_remove_safe(struct eventpoll *ep, struct epitem *epi)
{
	WARN_ON_ONCE(__ep_remove(ep, epi, false));
}

static void ep_clear_and_put(struct eventpoll *ep)
{
	struct rb_node *rbp, *next;
	struct epitem *epi;
	bool dispose;

	 
	if (waitqueue_active(&ep->poll_wait))
		ep_poll_safewake(ep, NULL, 0);

	mutex_lock(&ep->mtx);

	 
	for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = rb_next(rbp)) {
		epi = rb_entry(rbp, struct epitem, rbn);

		ep_unregister_pollwait(ep, epi);
		cond_resched();
	}

	 
	for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = next) {
		next = rb_next(rbp);
		epi = rb_entry(rbp, struct epitem, rbn);
		ep_remove_safe(ep, epi);
		cond_resched();
	}

	dispose = ep_refcount_dec_and_test(ep);
	mutex_unlock(&ep->mtx);

	if (dispose)
		ep_free(ep);
}

static int ep_eventpoll_release(struct inode *inode, struct file *file)
{
	struct eventpoll *ep = file->private_data;

	if (ep)
		ep_clear_and_put(ep);

	return 0;
}

static __poll_t ep_item_poll(const struct epitem *epi, poll_table *pt, int depth);

static __poll_t __ep_eventpoll_poll(struct file *file, poll_table *wait, int depth)
{
	struct eventpoll *ep = file->private_data;
	LIST_HEAD(txlist);
	struct epitem *epi, *tmp;
	poll_table pt;
	__poll_t res = 0;

	init_poll_funcptr(&pt, NULL);

	 
	poll_wait(file, &ep->poll_wait, wait);

	 
	mutex_lock_nested(&ep->mtx, depth);
	ep_start_scan(ep, &txlist);
	list_for_each_entry_safe(epi, tmp, &txlist, rdllink) {
		if (ep_item_poll(epi, &pt, depth + 1)) {
			res = EPOLLIN | EPOLLRDNORM;
			break;
		} else {
			 
			__pm_relax(ep_wakeup_source(epi));
			list_del_init(&epi->rdllink);
		}
	}
	ep_done_scan(ep, &txlist);
	mutex_unlock(&ep->mtx);
	return res;
}

 
static __poll_t ep_item_poll(const struct epitem *epi, poll_table *pt,
				 int depth)
{
	struct file *file = epi->ffd.file;
	__poll_t res;

	pt->_key = epi->event.events;
	if (!is_file_epoll(file))
		res = vfs_poll(file, pt);
	else
		res = __ep_eventpoll_poll(file, pt, depth);
	return res & epi->event.events;
}

static __poll_t ep_eventpoll_poll(struct file *file, poll_table *wait)
{
	return __ep_eventpoll_poll(file, wait, 0);
}

#ifdef CONFIG_PROC_FS
static void ep_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct eventpoll *ep = f->private_data;
	struct rb_node *rbp;

	mutex_lock(&ep->mtx);
	for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = rb_next(rbp)) {
		struct epitem *epi = rb_entry(rbp, struct epitem, rbn);
		struct inode *inode = file_inode(epi->ffd.file);

		seq_printf(m, "tfd: %8d events: %8x data: %16llx "
			   " pos:%lli ino:%lx sdev:%x\n",
			   epi->ffd.fd, epi->event.events,
			   (long long)epi->event.data,
			   (long long)epi->ffd.file->f_pos,
			   inode->i_ino, inode->i_sb->s_dev);
		if (seq_has_overflowed(m))
			break;
	}
	mutex_unlock(&ep->mtx);
}
#endif

 
static const struct file_operations eventpoll_fops = {
#ifdef CONFIG_PROC_FS
	.show_fdinfo	= ep_show_fdinfo,
#endif
	.release	= ep_eventpoll_release,
	.poll		= ep_eventpoll_poll,
	.llseek		= noop_llseek,
};

 
void eventpoll_release_file(struct file *file)
{
	struct eventpoll *ep;
	struct epitem *epi;
	bool dispose;

	 
again:
	spin_lock(&file->f_lock);
	if (file->f_ep && file->f_ep->first) {
		epi = hlist_entry(file->f_ep->first, struct epitem, fllink);
		epi->dying = true;
		spin_unlock(&file->f_lock);

		 
		ep = epi->ep;
		mutex_lock(&ep->mtx);
		dispose = __ep_remove(ep, epi, true);
		mutex_unlock(&ep->mtx);

		if (dispose)
			ep_free(ep);
		goto again;
	}
	spin_unlock(&file->f_lock);
}

static int ep_alloc(struct eventpoll **pep)
{
	struct eventpoll *ep;

	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (unlikely(!ep))
		return -ENOMEM;

	mutex_init(&ep->mtx);
	rwlock_init(&ep->lock);
	init_waitqueue_head(&ep->wq);
	init_waitqueue_head(&ep->poll_wait);
	INIT_LIST_HEAD(&ep->rdllist);
	ep->rbr = RB_ROOT_CACHED;
	ep->ovflist = EP_UNACTIVE_PTR;
	ep->user = get_current_user();
	refcount_set(&ep->refcount, 1);

	*pep = ep;

	return 0;
}

 
static struct epitem *ep_find(struct eventpoll *ep, struct file *file, int fd)
{
	int kcmp;
	struct rb_node *rbp;
	struct epitem *epi, *epir = NULL;
	struct epoll_filefd ffd;

	ep_set_ffd(&ffd, file, fd);
	for (rbp = ep->rbr.rb_root.rb_node; rbp; ) {
		epi = rb_entry(rbp, struct epitem, rbn);
		kcmp = ep_cmp_ffd(&ffd, &epi->ffd);
		if (kcmp > 0)
			rbp = rbp->rb_right;
		else if (kcmp < 0)
			rbp = rbp->rb_left;
		else {
			epir = epi;
			break;
		}
	}

	return epir;
}

#ifdef CONFIG_KCMP
static struct epitem *ep_find_tfd(struct eventpoll *ep, int tfd, unsigned long toff)
{
	struct rb_node *rbp;
	struct epitem *epi;

	for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = rb_next(rbp)) {
		epi = rb_entry(rbp, struct epitem, rbn);
		if (epi->ffd.fd == tfd) {
			if (toff == 0)
				return epi;
			else
				toff--;
		}
		cond_resched();
	}

	return NULL;
}

struct file *get_epoll_tfile_raw_ptr(struct file *file, int tfd,
				     unsigned long toff)
{
	struct file *file_raw;
	struct eventpoll *ep;
	struct epitem *epi;

	if (!is_file_epoll(file))
		return ERR_PTR(-EINVAL);

	ep = file->private_data;

	mutex_lock(&ep->mtx);
	epi = ep_find_tfd(ep, tfd, toff);
	if (epi)
		file_raw = epi->ffd.file;
	else
		file_raw = ERR_PTR(-ENOENT);
	mutex_unlock(&ep->mtx);

	return file_raw;
}
#endif  

 
static inline bool list_add_tail_lockless(struct list_head *new,
					  struct list_head *head)
{
	struct list_head *prev;

	 
	if (!try_cmpxchg(&new->next, &new, head))
		return false;

	 

	prev = xchg(&head->prev, new);

	 

	prev->next = new;
	new->prev = prev;

	return true;
}

 
static inline bool chain_epi_lockless(struct epitem *epi)
{
	struct eventpoll *ep = epi->ep;

	 
	if (epi->next != EP_UNACTIVE_PTR)
		return false;

	 
	if (cmpxchg(&epi->next, EP_UNACTIVE_PTR, NULL) != EP_UNACTIVE_PTR)
		return false;

	 
	epi->next = xchg(&ep->ovflist, epi);

	return true;
}

 
static int ep_poll_callback(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
{
	int pwake = 0;
	struct epitem *epi = ep_item_from_wait(wait);
	struct eventpoll *ep = epi->ep;
	__poll_t pollflags = key_to_poll(key);
	unsigned long flags;
	int ewake = 0;

	read_lock_irqsave(&ep->lock, flags);

	ep_set_busy_poll_napi_id(epi);

	 
	if (!(epi->event.events & ~EP_PRIVATE_BITS))
		goto out_unlock;

	 
	if (pollflags && !(pollflags & epi->event.events))
		goto out_unlock;

	 
	if (READ_ONCE(ep->ovflist) != EP_UNACTIVE_PTR) {
		if (chain_epi_lockless(epi))
			ep_pm_stay_awake_rcu(epi);
	} else if (!ep_is_linked(epi)) {
		 
		if (list_add_tail_lockless(&epi->rdllink, &ep->rdllist))
			ep_pm_stay_awake_rcu(epi);
	}

	 
	if (waitqueue_active(&ep->wq)) {
		if ((epi->event.events & EPOLLEXCLUSIVE) &&
					!(pollflags & POLLFREE)) {
			switch (pollflags & EPOLLINOUT_BITS) {
			case EPOLLIN:
				if (epi->event.events & EPOLLIN)
					ewake = 1;
				break;
			case EPOLLOUT:
				if (epi->event.events & EPOLLOUT)
					ewake = 1;
				break;
			case 0:
				ewake = 1;
				break;
			}
		}
		wake_up(&ep->wq);
	}
	if (waitqueue_active(&ep->poll_wait))
		pwake++;

out_unlock:
	read_unlock_irqrestore(&ep->lock, flags);

	 
	if (pwake)
		ep_poll_safewake(ep, epi, pollflags & EPOLL_URING_WAKE);

	if (!(epi->event.events & EPOLLEXCLUSIVE))
		ewake = 1;

	if (pollflags & POLLFREE) {
		 
		list_del_init(&wait->entry);
		 
		smp_store_release(&ep_pwq_from_wait(wait)->whead, NULL);
	}

	return ewake;
}

 
static void ep_ptable_queue_proc(struct file *file, wait_queue_head_t *whead,
				 poll_table *pt)
{
	struct ep_pqueue *epq = container_of(pt, struct ep_pqueue, pt);
	struct epitem *epi = epq->epi;
	struct eppoll_entry *pwq;

	if (unlikely(!epi))	
		return;

	pwq = kmem_cache_alloc(pwq_cache, GFP_KERNEL);
	if (unlikely(!pwq)) {
		epq->epi = NULL;
		return;
	}

	init_waitqueue_func_entry(&pwq->wait, ep_poll_callback);
	pwq->whead = whead;
	pwq->base = epi;
	if (epi->event.events & EPOLLEXCLUSIVE)
		add_wait_queue_exclusive(whead, &pwq->wait);
	else
		add_wait_queue(whead, &pwq->wait);
	pwq->next = epi->pwqlist;
	epi->pwqlist = pwq;
}

static void ep_rbtree_insert(struct eventpoll *ep, struct epitem *epi)
{
	int kcmp;
	struct rb_node **p = &ep->rbr.rb_root.rb_node, *parent = NULL;
	struct epitem *epic;
	bool leftmost = true;

	while (*p) {
		parent = *p;
		epic = rb_entry(parent, struct epitem, rbn);
		kcmp = ep_cmp_ffd(&epi->ffd, &epic->ffd);
		if (kcmp > 0) {
			p = &parent->rb_right;
			leftmost = false;
		} else
			p = &parent->rb_left;
	}
	rb_link_node(&epi->rbn, parent, p);
	rb_insert_color_cached(&epi->rbn, &ep->rbr, leftmost);
}



#define PATH_ARR_SIZE 5
 
static const int path_limits[PATH_ARR_SIZE] = { 1000, 500, 100, 50, 10 };
static int path_count[PATH_ARR_SIZE];

static int path_count_inc(int nests)
{
	 
	if (nests == 0)
		return 0;

	if (++path_count[nests] > path_limits[nests])
		return -1;
	return 0;
}

static void path_count_init(void)
{
	int i;

	for (i = 0; i < PATH_ARR_SIZE; i++)
		path_count[i] = 0;
}

static int reverse_path_check_proc(struct hlist_head *refs, int depth)
{
	int error = 0;
	struct epitem *epi;

	if (depth > EP_MAX_NESTS)  
		return -1;

	 
	hlist_for_each_entry_rcu(epi, refs, fllink) {
		struct hlist_head *refs = &epi->ep->refs;
		if (hlist_empty(refs))
			error = path_count_inc(depth);
		else
			error = reverse_path_check_proc(refs, depth + 1);
		if (error != 0)
			break;
	}
	return error;
}

 
static int reverse_path_check(void)
{
	struct epitems_head *p;

	for (p = tfile_check_list; p != EP_UNACTIVE_PTR; p = p->next) {
		int error;
		path_count_init();
		rcu_read_lock();
		error = reverse_path_check_proc(&p->epitems, 0);
		rcu_read_unlock();
		if (error)
			return error;
	}
	return 0;
}

static int ep_create_wakeup_source(struct epitem *epi)
{
	struct name_snapshot n;
	struct wakeup_source *ws;

	if (!epi->ep->ws) {
		epi->ep->ws = wakeup_source_register(NULL, "eventpoll");
		if (!epi->ep->ws)
			return -ENOMEM;
	}

	take_dentry_name_snapshot(&n, epi->ffd.file->f_path.dentry);
	ws = wakeup_source_register(NULL, n.name.name);
	release_dentry_name_snapshot(&n);

	if (!ws)
		return -ENOMEM;
	rcu_assign_pointer(epi->ws, ws);

	return 0;
}

 
static noinline void ep_destroy_wakeup_source(struct epitem *epi)
{
	struct wakeup_source *ws = ep_wakeup_source(epi);

	RCU_INIT_POINTER(epi->ws, NULL);

	 
	synchronize_rcu();
	wakeup_source_unregister(ws);
}

static int attach_epitem(struct file *file, struct epitem *epi)
{
	struct epitems_head *to_free = NULL;
	struct hlist_head *head = NULL;
	struct eventpoll *ep = NULL;

	if (is_file_epoll(file))
		ep = file->private_data;

	if (ep) {
		head = &ep->refs;
	} else if (!READ_ONCE(file->f_ep)) {
allocate:
		to_free = kmem_cache_zalloc(ephead_cache, GFP_KERNEL);
		if (!to_free)
			return -ENOMEM;
		head = &to_free->epitems;
	}
	spin_lock(&file->f_lock);
	if (!file->f_ep) {
		if (unlikely(!head)) {
			spin_unlock(&file->f_lock);
			goto allocate;
		}
		file->f_ep = head;
		to_free = NULL;
	}
	hlist_add_head_rcu(&epi->fllink, file->f_ep);
	spin_unlock(&file->f_lock);
	free_ephead(to_free);
	return 0;
}

 
static int ep_insert(struct eventpoll *ep, const struct epoll_event *event,
		     struct file *tfile, int fd, int full_check)
{
	int error, pwake = 0;
	__poll_t revents;
	struct epitem *epi;
	struct ep_pqueue epq;
	struct eventpoll *tep = NULL;

	if (is_file_epoll(tfile))
		tep = tfile->private_data;

	lockdep_assert_irqs_enabled();

	if (unlikely(percpu_counter_compare(&ep->user->epoll_watches,
					    max_user_watches) >= 0))
		return -ENOSPC;
	percpu_counter_inc(&ep->user->epoll_watches);

	if (!(epi = kmem_cache_zalloc(epi_cache, GFP_KERNEL))) {
		percpu_counter_dec(&ep->user->epoll_watches);
		return -ENOMEM;
	}

	 
	INIT_LIST_HEAD(&epi->rdllink);
	epi->ep = ep;
	ep_set_ffd(&epi->ffd, tfile, fd);
	epi->event = *event;
	epi->next = EP_UNACTIVE_PTR;

	if (tep)
		mutex_lock_nested(&tep->mtx, 1);
	 
	if (unlikely(attach_epitem(tfile, epi) < 0)) {
		if (tep)
			mutex_unlock(&tep->mtx);
		kmem_cache_free(epi_cache, epi);
		percpu_counter_dec(&ep->user->epoll_watches);
		return -ENOMEM;
	}

	if (full_check && !tep)
		list_file(tfile);

	 
	ep_rbtree_insert(ep, epi);
	if (tep)
		mutex_unlock(&tep->mtx);

	 
	ep_get(ep);

	 
	if (unlikely(full_check && reverse_path_check())) {
		ep_remove_safe(ep, epi);
		return -EINVAL;
	}

	if (epi->event.events & EPOLLWAKEUP) {
		error = ep_create_wakeup_source(epi);
		if (error) {
			ep_remove_safe(ep, epi);
			return error;
		}
	}

	 
	epq.epi = epi;
	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc);

	 
	revents = ep_item_poll(epi, &epq.pt, 1);

	 
	if (unlikely(!epq.epi)) {
		ep_remove_safe(ep, epi);
		return -ENOMEM;
	}

	 
	write_lock_irq(&ep->lock);

	 
	ep_set_busy_poll_napi_id(epi);

	 
	if (revents && !ep_is_linked(epi)) {
		list_add_tail(&epi->rdllink, &ep->rdllist);
		ep_pm_stay_awake(epi);

		 
		if (waitqueue_active(&ep->wq))
			wake_up(&ep->wq);
		if (waitqueue_active(&ep->poll_wait))
			pwake++;
	}

	write_unlock_irq(&ep->lock);

	 
	if (pwake)
		ep_poll_safewake(ep, NULL, 0);

	return 0;
}

 
static int ep_modify(struct eventpoll *ep, struct epitem *epi,
		     const struct epoll_event *event)
{
	int pwake = 0;
	poll_table pt;

	lockdep_assert_irqs_enabled();

	init_poll_funcptr(&pt, NULL);

	 
	epi->event.events = event->events;  
	epi->event.data = event->data;  
	if (epi->event.events & EPOLLWAKEUP) {
		if (!ep_has_wakeup_source(epi))
			ep_create_wakeup_source(epi);
	} else if (ep_has_wakeup_source(epi)) {
		ep_destroy_wakeup_source(epi);
	}

	 
	smp_mb();

	 
	if (ep_item_poll(epi, &pt, 1)) {
		write_lock_irq(&ep->lock);
		if (!ep_is_linked(epi)) {
			list_add_tail(&epi->rdllink, &ep->rdllist);
			ep_pm_stay_awake(epi);

			 
			if (waitqueue_active(&ep->wq))
				wake_up(&ep->wq);
			if (waitqueue_active(&ep->poll_wait))
				pwake++;
		}
		write_unlock_irq(&ep->lock);
	}

	 
	if (pwake)
		ep_poll_safewake(ep, NULL, 0);

	return 0;
}

static int ep_send_events(struct eventpoll *ep,
			  struct epoll_event __user *events, int maxevents)
{
	struct epitem *epi, *tmp;
	LIST_HEAD(txlist);
	poll_table pt;
	int res = 0;

	 
	if (fatal_signal_pending(current))
		return -EINTR;

	init_poll_funcptr(&pt, NULL);

	mutex_lock(&ep->mtx);
	ep_start_scan(ep, &txlist);

	 
	list_for_each_entry_safe(epi, tmp, &txlist, rdllink) {
		struct wakeup_source *ws;
		__poll_t revents;

		if (res >= maxevents)
			break;

		 
		ws = ep_wakeup_source(epi);
		if (ws) {
			if (ws->active)
				__pm_stay_awake(ep->ws);
			__pm_relax(ws);
		}

		list_del_init(&epi->rdllink);

		 
		revents = ep_item_poll(epi, &pt, 1);
		if (!revents)
			continue;

		events = epoll_put_uevent(revents, epi->event.data, events);
		if (!events) {
			list_add(&epi->rdllink, &txlist);
			ep_pm_stay_awake(epi);
			if (!res)
				res = -EFAULT;
			break;
		}
		res++;
		if (epi->event.events & EPOLLONESHOT)
			epi->event.events &= EP_PRIVATE_BITS;
		else if (!(epi->event.events & EPOLLET)) {
			 
			list_add_tail(&epi->rdllink, &ep->rdllist);
			ep_pm_stay_awake(epi);
		}
	}
	ep_done_scan(ep, &txlist);
	mutex_unlock(&ep->mtx);

	return res;
}

static struct timespec64 *ep_timeout_to_timespec(struct timespec64 *to, long ms)
{
	struct timespec64 now;

	if (ms < 0)
		return NULL;

	if (!ms) {
		to->tv_sec = 0;
		to->tv_nsec = 0;
		return to;
	}

	to->tv_sec = ms / MSEC_PER_SEC;
	to->tv_nsec = NSEC_PER_MSEC * (ms % MSEC_PER_SEC);

	ktime_get_ts64(&now);
	*to = timespec64_add_safe(now, *to);
	return to;
}

 
static int ep_autoremove_wake_function(struct wait_queue_entry *wq_entry,
				       unsigned int mode, int sync, void *key)
{
	int ret = default_wake_function(wq_entry, mode, sync, key);

	 
	list_del_init_careful(&wq_entry->entry);
	return ret;
}

 
static int ep_poll(struct eventpoll *ep, struct epoll_event __user *events,
		   int maxevents, struct timespec64 *timeout)
{
	int res, eavail, timed_out = 0;
	u64 slack = 0;
	wait_queue_entry_t wait;
	ktime_t expires, *to = NULL;

	lockdep_assert_irqs_enabled();

	if (timeout && (timeout->tv_sec | timeout->tv_nsec)) {
		slack = select_estimate_accuracy(timeout);
		to = &expires;
		*to = timespec64_to_ktime(*timeout);
	} else if (timeout) {
		 
		timed_out = 1;
	}

	 
	eavail = ep_events_available(ep);

	while (1) {
		if (eavail) {
			 
			res = ep_send_events(ep, events, maxevents);
			if (res)
				return res;
		}

		if (timed_out)
			return 0;

		eavail = ep_busy_loop(ep, timed_out);
		if (eavail)
			continue;

		if (signal_pending(current))
			return -EINTR;

		 
		init_wait(&wait);
		wait.func = ep_autoremove_wake_function;

		write_lock_irq(&ep->lock);
		 
		__set_current_state(TASK_INTERRUPTIBLE);

		 
		eavail = ep_events_available(ep);
		if (!eavail)
			__add_wait_queue_exclusive(&ep->wq, &wait);

		write_unlock_irq(&ep->lock);

		if (!eavail)
			timed_out = !schedule_hrtimeout_range(to, slack,
							      HRTIMER_MODE_ABS);
		__set_current_state(TASK_RUNNING);

		 
		eavail = 1;

		if (!list_empty_careful(&wait.entry)) {
			write_lock_irq(&ep->lock);
			 
			if (timed_out)
				eavail = list_empty(&wait.entry);
			__remove_wait_queue(&ep->wq, &wait);
			write_unlock_irq(&ep->lock);
		}
	}
}

 
static int ep_loop_check_proc(struct eventpoll *ep, int depth)
{
	int error = 0;
	struct rb_node *rbp;
	struct epitem *epi;

	mutex_lock_nested(&ep->mtx, depth + 1);
	ep->gen = loop_check_gen;
	for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = rb_next(rbp)) {
		epi = rb_entry(rbp, struct epitem, rbn);
		if (unlikely(is_file_epoll(epi->ffd.file))) {
			struct eventpoll *ep_tovisit;
			ep_tovisit = epi->ffd.file->private_data;
			if (ep_tovisit->gen == loop_check_gen)
				continue;
			if (ep_tovisit == inserting_into || depth > EP_MAX_NESTS)
				error = -1;
			else
				error = ep_loop_check_proc(ep_tovisit, depth + 1);
			if (error != 0)
				break;
		} else {
			 
			list_file(epi->ffd.file);
		}
	}
	mutex_unlock(&ep->mtx);

	return error;
}

 
static int ep_loop_check(struct eventpoll *ep, struct eventpoll *to)
{
	inserting_into = ep;
	return ep_loop_check_proc(to, 0);
}

static void clear_tfile_check_list(void)
{
	rcu_read_lock();
	while (tfile_check_list != EP_UNACTIVE_PTR) {
		struct epitems_head *head = tfile_check_list;
		tfile_check_list = head->next;
		unlist_file(head);
	}
	rcu_read_unlock();
}

 
static int do_epoll_create(int flags)
{
	int error, fd;
	struct eventpoll *ep = NULL;
	struct file *file;

	 
	BUILD_BUG_ON(EPOLL_CLOEXEC != O_CLOEXEC);

	if (flags & ~EPOLL_CLOEXEC)
		return -EINVAL;
	 
	error = ep_alloc(&ep);
	if (error < 0)
		return error;
	 
	fd = get_unused_fd_flags(O_RDWR | (flags & O_CLOEXEC));
	if (fd < 0) {
		error = fd;
		goto out_free_ep;
	}
	file = anon_inode_getfile("[eventpoll]", &eventpoll_fops, ep,
				 O_RDWR | (flags & O_CLOEXEC));
	if (IS_ERR(file)) {
		error = PTR_ERR(file);
		goto out_free_fd;
	}
	ep->file = file;
	fd_install(fd, file);
	return fd;

out_free_fd:
	put_unused_fd(fd);
out_free_ep:
	ep_clear_and_put(ep);
	return error;
}

SYSCALL_DEFINE1(epoll_create1, int, flags)
{
	return do_epoll_create(flags);
}

SYSCALL_DEFINE1(epoll_create, int, size)
{
	if (size <= 0)
		return -EINVAL;

	return do_epoll_create(0);
}

#ifdef CONFIG_PM_SLEEP
static inline void ep_take_care_of_epollwakeup(struct epoll_event *epev)
{
	if ((epev->events & EPOLLWAKEUP) && !capable(CAP_BLOCK_SUSPEND))
		epev->events &= ~EPOLLWAKEUP;
}
#else
static inline void ep_take_care_of_epollwakeup(struct epoll_event *epev)
{
	epev->events &= ~EPOLLWAKEUP;
}
#endif

static inline int epoll_mutex_lock(struct mutex *mutex, int depth,
				   bool nonblock)
{
	if (!nonblock) {
		mutex_lock_nested(mutex, depth);
		return 0;
	}
	if (mutex_trylock(mutex))
		return 0;
	return -EAGAIN;
}

int do_epoll_ctl(int epfd, int op, int fd, struct epoll_event *epds,
		 bool nonblock)
{
	int error;
	int full_check = 0;
	struct fd f, tf;
	struct eventpoll *ep;
	struct epitem *epi;
	struct eventpoll *tep = NULL;

	error = -EBADF;
	f = fdget(epfd);
	if (!f.file)
		goto error_return;

	 
	tf = fdget(fd);
	if (!tf.file)
		goto error_fput;

	 
	error = -EPERM;
	if (!file_can_poll(tf.file))
		goto error_tgt_fput;

	 
	if (ep_op_has_event(op))
		ep_take_care_of_epollwakeup(epds);

	 
	error = -EINVAL;
	if (f.file == tf.file || !is_file_epoll(f.file))
		goto error_tgt_fput;

	 
	if (ep_op_has_event(op) && (epds->events & EPOLLEXCLUSIVE)) {
		if (op == EPOLL_CTL_MOD)
			goto error_tgt_fput;
		if (op == EPOLL_CTL_ADD && (is_file_epoll(tf.file) ||
				(epds->events & ~EPOLLEXCLUSIVE_OK_BITS)))
			goto error_tgt_fput;
	}

	 
	ep = f.file->private_data;

	 
	error = epoll_mutex_lock(&ep->mtx, 0, nonblock);
	if (error)
		goto error_tgt_fput;
	if (op == EPOLL_CTL_ADD) {
		if (READ_ONCE(f.file->f_ep) || ep->gen == loop_check_gen ||
		    is_file_epoll(tf.file)) {
			mutex_unlock(&ep->mtx);
			error = epoll_mutex_lock(&epnested_mutex, 0, nonblock);
			if (error)
				goto error_tgt_fput;
			loop_check_gen++;
			full_check = 1;
			if (is_file_epoll(tf.file)) {
				tep = tf.file->private_data;
				error = -ELOOP;
				if (ep_loop_check(ep, tep) != 0)
					goto error_tgt_fput;
			}
			error = epoll_mutex_lock(&ep->mtx, 0, nonblock);
			if (error)
				goto error_tgt_fput;
		}
	}

	 
	epi = ep_find(ep, tf.file, fd);

	error = -EINVAL;
	switch (op) {
	case EPOLL_CTL_ADD:
		if (!epi) {
			epds->events |= EPOLLERR | EPOLLHUP;
			error = ep_insert(ep, epds, tf.file, fd, full_check);
		} else
			error = -EEXIST;
		break;
	case EPOLL_CTL_DEL:
		if (epi) {
			 
			ep_remove_safe(ep, epi);
			error = 0;
		} else {
			error = -ENOENT;
		}
		break;
	case EPOLL_CTL_MOD:
		if (epi) {
			if (!(epi->event.events & EPOLLEXCLUSIVE)) {
				epds->events |= EPOLLERR | EPOLLHUP;
				error = ep_modify(ep, epi, epds);
			}
		} else
			error = -ENOENT;
		break;
	}
	mutex_unlock(&ep->mtx);

error_tgt_fput:
	if (full_check) {
		clear_tfile_check_list();
		loop_check_gen++;
		mutex_unlock(&epnested_mutex);
	}

	fdput(tf);
error_fput:
	fdput(f);
error_return:

	return error;
}

 
SYSCALL_DEFINE4(epoll_ctl, int, epfd, int, op, int, fd,
		struct epoll_event __user *, event)
{
	struct epoll_event epds;

	if (ep_op_has_event(op) &&
	    copy_from_user(&epds, event, sizeof(struct epoll_event)))
		return -EFAULT;

	return do_epoll_ctl(epfd, op, fd, &epds, false);
}

 
static int do_epoll_wait(int epfd, struct epoll_event __user *events,
			 int maxevents, struct timespec64 *to)
{
	int error;
	struct fd f;
	struct eventpoll *ep;

	 
	if (maxevents <= 0 || maxevents > EP_MAX_EVENTS)
		return -EINVAL;

	 
	if (!access_ok(events, maxevents * sizeof(struct epoll_event)))
		return -EFAULT;

	 
	f = fdget(epfd);
	if (!f.file)
		return -EBADF;

	 
	error = -EINVAL;
	if (!is_file_epoll(f.file))
		goto error_fput;

	 
	ep = f.file->private_data;

	 
	error = ep_poll(ep, events, maxevents, to);

error_fput:
	fdput(f);
	return error;
}

SYSCALL_DEFINE4(epoll_wait, int, epfd, struct epoll_event __user *, events,
		int, maxevents, int, timeout)
{
	struct timespec64 to;

	return do_epoll_wait(epfd, events, maxevents,
			     ep_timeout_to_timespec(&to, timeout));
}

 
static int do_epoll_pwait(int epfd, struct epoll_event __user *events,
			  int maxevents, struct timespec64 *to,
			  const sigset_t __user *sigmask, size_t sigsetsize)
{
	int error;

	 
	error = set_user_sigmask(sigmask, sigsetsize);
	if (error)
		return error;

	error = do_epoll_wait(epfd, events, maxevents, to);

	restore_saved_sigmask_unless(error == -EINTR);

	return error;
}

SYSCALL_DEFINE6(epoll_pwait, int, epfd, struct epoll_event __user *, events,
		int, maxevents, int, timeout, const sigset_t __user *, sigmask,
		size_t, sigsetsize)
{
	struct timespec64 to;

	return do_epoll_pwait(epfd, events, maxevents,
			      ep_timeout_to_timespec(&to, timeout),
			      sigmask, sigsetsize);
}

SYSCALL_DEFINE6(epoll_pwait2, int, epfd, struct epoll_event __user *, events,
		int, maxevents, const struct __kernel_timespec __user *, timeout,
		const sigset_t __user *, sigmask, size_t, sigsetsize)
{
	struct timespec64 ts, *to = NULL;

	if (timeout) {
		if (get_timespec64(&ts, timeout))
			return -EFAULT;
		to = &ts;
		if (poll_select_set_timeout(to, ts.tv_sec, ts.tv_nsec))
			return -EINVAL;
	}

	return do_epoll_pwait(epfd, events, maxevents, to,
			      sigmask, sigsetsize);
}

#ifdef CONFIG_COMPAT
static int do_compat_epoll_pwait(int epfd, struct epoll_event __user *events,
				 int maxevents, struct timespec64 *timeout,
				 const compat_sigset_t __user *sigmask,
				 compat_size_t sigsetsize)
{
	long err;

	 
	err = set_compat_user_sigmask(sigmask, sigsetsize);
	if (err)
		return err;

	err = do_epoll_wait(epfd, events, maxevents, timeout);

	restore_saved_sigmask_unless(err == -EINTR);

	return err;
}

COMPAT_SYSCALL_DEFINE6(epoll_pwait, int, epfd,
		       struct epoll_event __user *, events,
		       int, maxevents, int, timeout,
		       const compat_sigset_t __user *, sigmask,
		       compat_size_t, sigsetsize)
{
	struct timespec64 to;

	return do_compat_epoll_pwait(epfd, events, maxevents,
				     ep_timeout_to_timespec(&to, timeout),
				     sigmask, sigsetsize);
}

COMPAT_SYSCALL_DEFINE6(epoll_pwait2, int, epfd,
		       struct epoll_event __user *, events,
		       int, maxevents,
		       const struct __kernel_timespec __user *, timeout,
		       const compat_sigset_t __user *, sigmask,
		       compat_size_t, sigsetsize)
{
	struct timespec64 ts, *to = NULL;

	if (timeout) {
		if (get_timespec64(&ts, timeout))
			return -EFAULT;
		to = &ts;
		if (poll_select_set_timeout(to, ts.tv_sec, ts.tv_nsec))
			return -EINVAL;
	}

	return do_compat_epoll_pwait(epfd, events, maxevents, to,
				     sigmask, sigsetsize);
}

#endif

static int __init eventpoll_init(void)
{
	struct sysinfo si;

	si_meminfo(&si);
	 
	max_user_watches = (((si.totalram - si.totalhigh) / 25) << PAGE_SHIFT) /
		EP_ITEM_COST;
	BUG_ON(max_user_watches < 0);

	 
	BUILD_BUG_ON(sizeof(void *) <= 8 && sizeof(struct epitem) > 128);

	 
	epi_cache = kmem_cache_create("eventpoll_epi", sizeof(struct epitem),
			0, SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT, NULL);

	 
	pwq_cache = kmem_cache_create("eventpoll_pwq",
		sizeof(struct eppoll_entry), 0, SLAB_PANIC|SLAB_ACCOUNT, NULL);
	epoll_sysctls_init();

	ephead_cache = kmem_cache_create("ep_head",
		sizeof(struct epitems_head), 0, SLAB_PANIC|SLAB_ACCOUNT, NULL);

	return 0;
}
fs_initcall(eventpoll_init);
