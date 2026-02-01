 
 
 

 

#include <sys/zfs_context.h>
#include <sys/zfs_rlock.h>


 
static int
zfs_rangelock_compare(const void *arg1, const void *arg2)
{
	const zfs_locked_range_t *rl1 = (const zfs_locked_range_t *)arg1;
	const zfs_locked_range_t *rl2 = (const zfs_locked_range_t *)arg2;

	return (TREE_CMP(rl1->lr_offset, rl2->lr_offset));
}

 
void
zfs_rangelock_init(zfs_rangelock_t *rl, zfs_rangelock_cb_t *cb, void *arg)
{
	mutex_init(&rl->rl_lock, NULL, MUTEX_DEFAULT, NULL);
	avl_create(&rl->rl_tree, zfs_rangelock_compare,
	    sizeof (zfs_locked_range_t), offsetof(zfs_locked_range_t, lr_node));
	rl->rl_cb = cb;
	rl->rl_arg = arg;
}

void
zfs_rangelock_fini(zfs_rangelock_t *rl)
{
	mutex_destroy(&rl->rl_lock);
	avl_destroy(&rl->rl_tree);
}

 
static boolean_t
zfs_rangelock_enter_writer(zfs_rangelock_t *rl, zfs_locked_range_t *new,
    boolean_t nonblock)
{
	avl_tree_t *tree = &rl->rl_tree;
	zfs_locked_range_t *lr;
	avl_index_t where;
	uint64_t orig_off = new->lr_offset;
	uint64_t orig_len = new->lr_length;
	zfs_rangelock_type_t orig_type = new->lr_type;

	for (;;) {
		 
		if (rl->rl_cb != NULL) {
			rl->rl_cb(new, rl->rl_arg);
		}

		 
		ASSERT3U(new->lr_type, ==, RL_WRITER);

		 
		if (avl_numnodes(tree) == 0) {
			avl_add(tree, new);
			return (B_TRUE);
		}

		 
		lr = avl_find(tree, new, &where);
		if (lr != NULL)
			goto wait;  

		lr = avl_nearest(tree, where, AVL_AFTER);
		if (lr != NULL &&
		    lr->lr_offset < new->lr_offset + new->lr_length)
			goto wait;

		lr = avl_nearest(tree, where, AVL_BEFORE);
		if (lr != NULL &&
		    lr->lr_offset + lr->lr_length > new->lr_offset)
			goto wait;

		avl_insert(tree, new, where);
		return (B_TRUE);
wait:
		if (nonblock)
			return (B_FALSE);
		if (!lr->lr_write_wanted) {
			cv_init(&lr->lr_write_cv, NULL, CV_DEFAULT, NULL);
			lr->lr_write_wanted = B_TRUE;
		}
		cv_wait(&lr->lr_write_cv, &rl->rl_lock);

		 
		new->lr_offset = orig_off;
		new->lr_length = orig_len;
		new->lr_type = orig_type;
	}
}

 
static zfs_locked_range_t *
zfs_rangelock_proxify(avl_tree_t *tree, zfs_locked_range_t *lr)
{
	zfs_locked_range_t *proxy;

	if (lr->lr_proxy)
		return (lr);  

	ASSERT3U(lr->lr_count, ==, 1);
	ASSERT(lr->lr_write_wanted == B_FALSE);
	ASSERT(lr->lr_read_wanted == B_FALSE);
	avl_remove(tree, lr);
	lr->lr_count = 0;

	 
	proxy = kmem_alloc(sizeof (zfs_locked_range_t), KM_SLEEP);
	proxy->lr_offset = lr->lr_offset;
	proxy->lr_length = lr->lr_length;
	proxy->lr_count = 1;
	proxy->lr_type = RL_READER;
	proxy->lr_proxy = B_TRUE;
	proxy->lr_write_wanted = B_FALSE;
	proxy->lr_read_wanted = B_FALSE;
	avl_add(tree, proxy);

	return (proxy);
}

 
static zfs_locked_range_t *
zfs_rangelock_split(avl_tree_t *tree, zfs_locked_range_t *lr, uint64_t off)
{
	zfs_locked_range_t *rear;

	ASSERT3U(lr->lr_length, >, 1);
	ASSERT3U(off, >, lr->lr_offset);
	ASSERT3U(off, <, lr->lr_offset + lr->lr_length);
	ASSERT(lr->lr_write_wanted == B_FALSE);
	ASSERT(lr->lr_read_wanted == B_FALSE);

	 
	rear = kmem_alloc(sizeof (zfs_locked_range_t), KM_SLEEP);
	rear->lr_offset = off;
	rear->lr_length = lr->lr_offset + lr->lr_length - off;
	rear->lr_count = lr->lr_count;
	rear->lr_type = RL_READER;
	rear->lr_proxy = B_TRUE;
	rear->lr_write_wanted = B_FALSE;
	rear->lr_read_wanted = B_FALSE;

	zfs_locked_range_t *front = zfs_rangelock_proxify(tree, lr);
	front->lr_length = off - lr->lr_offset;

	avl_insert_here(tree, rear, front, AVL_AFTER);
	return (front);
}

 
static void
zfs_rangelock_new_proxy(avl_tree_t *tree, uint64_t off, uint64_t len)
{
	zfs_locked_range_t *lr;

	ASSERT(len != 0);
	lr = kmem_alloc(sizeof (zfs_locked_range_t), KM_SLEEP);
	lr->lr_offset = off;
	lr->lr_length = len;
	lr->lr_count = 1;
	lr->lr_type = RL_READER;
	lr->lr_proxy = B_TRUE;
	lr->lr_write_wanted = B_FALSE;
	lr->lr_read_wanted = B_FALSE;
	avl_add(tree, lr);
}

static void
zfs_rangelock_add_reader(avl_tree_t *tree, zfs_locked_range_t *new,
    zfs_locked_range_t *prev, avl_index_t where)
{
	zfs_locked_range_t *next;
	uint64_t off = new->lr_offset;
	uint64_t len = new->lr_length;

	 
	if (prev != NULL) {
		if (prev->lr_offset + prev->lr_length <= off) {
			prev = NULL;
		} else if (prev->lr_offset != off) {
			 
			prev = zfs_rangelock_split(tree, prev, off);
			prev = AVL_NEXT(tree, prev);  
		}
	}
	ASSERT((prev == NULL) || (prev->lr_offset == off));

	if (prev != NULL)
		next = prev;
	else
		next = avl_nearest(tree, where, AVL_AFTER);

	if (next == NULL || off + len <= next->lr_offset) {
		 
		avl_insert(tree, new, where);
		return;
	}

	if (off < next->lr_offset) {
		 
		zfs_rangelock_new_proxy(tree, off, next->lr_offset - off);
	}

	new->lr_count = 0;  
	 
	for (prev = NULL; next; prev = next, next = AVL_NEXT(tree, next)) {
		if (off + len <= next->lr_offset)
			break;
		if (prev != NULL && prev->lr_offset + prev->lr_length <
		    next->lr_offset) {
			 
			ASSERT3U(next->lr_offset, >,
			    prev->lr_offset + prev->lr_length);
			zfs_rangelock_new_proxy(tree,
			    prev->lr_offset + prev->lr_length,
			    next->lr_offset -
			    (prev->lr_offset + prev->lr_length));
		}
		if (off + len == next->lr_offset + next->lr_length) {
			 
			next = zfs_rangelock_proxify(tree, next);
			next->lr_count++;
			return;
		}
		if (off + len < next->lr_offset + next->lr_length) {
			 
			next = zfs_rangelock_split(tree, next, off + len);
			next->lr_count++;
			return;
		}
		ASSERT3U(off + len, >, next->lr_offset + next->lr_length);
		next = zfs_rangelock_proxify(tree, next);
		next->lr_count++;
	}

	 
	zfs_rangelock_new_proxy(tree, prev->lr_offset + prev->lr_length,
	    (off + len) - (prev->lr_offset + prev->lr_length));
}

 
static boolean_t
zfs_rangelock_enter_reader(zfs_rangelock_t *rl, zfs_locked_range_t *new,
    boolean_t nonblock)
{
	avl_tree_t *tree = &rl->rl_tree;
	zfs_locked_range_t *prev, *next;
	avl_index_t where;
	uint64_t off = new->lr_offset;
	uint64_t len = new->lr_length;

	 
retry:
	prev = avl_find(tree, new, &where);
	if (prev == NULL)
		prev = avl_nearest(tree, where, AVL_BEFORE);

	 
	if (prev && (off < prev->lr_offset + prev->lr_length)) {
		if ((prev->lr_type == RL_WRITER) || (prev->lr_write_wanted)) {
			if (nonblock)
				return (B_FALSE);
			if (!prev->lr_read_wanted) {
				cv_init(&prev->lr_read_cv,
				    NULL, CV_DEFAULT, NULL);
				prev->lr_read_wanted = B_TRUE;
			}
			cv_wait(&prev->lr_read_cv, &rl->rl_lock);
			goto retry;
		}
		if (off + len < prev->lr_offset + prev->lr_length)
			goto got_lock;
	}

	 
	if (prev != NULL)
		next = AVL_NEXT(tree, prev);
	else
		next = avl_nearest(tree, where, AVL_AFTER);
	for (; next != NULL; next = AVL_NEXT(tree, next)) {
		if (off + len <= next->lr_offset)
			goto got_lock;
		if ((next->lr_type == RL_WRITER) || (next->lr_write_wanted)) {
			if (nonblock)
				return (B_FALSE);
			if (!next->lr_read_wanted) {
				cv_init(&next->lr_read_cv,
				    NULL, CV_DEFAULT, NULL);
				next->lr_read_wanted = B_TRUE;
			}
			cv_wait(&next->lr_read_cv, &rl->rl_lock);
			goto retry;
		}
		if (off + len <= next->lr_offset + next->lr_length)
			goto got_lock;
	}

got_lock:
	 
	zfs_rangelock_add_reader(tree, new, prev, where);
	return (B_TRUE);
}

 
static zfs_locked_range_t *
zfs_rangelock_enter_impl(zfs_rangelock_t *rl, uint64_t off, uint64_t len,
    zfs_rangelock_type_t type, boolean_t nonblock)
{
	zfs_locked_range_t *new;

	ASSERT(type == RL_READER || type == RL_WRITER || type == RL_APPEND);

	new = kmem_alloc(sizeof (zfs_locked_range_t), KM_SLEEP);
	new->lr_rangelock = rl;
	new->lr_offset = off;
	if (len + off < off)	 
		len = UINT64_MAX - off;
	new->lr_length = len;
	new->lr_count = 1;  
	new->lr_type = type;
	new->lr_proxy = B_FALSE;
	new->lr_write_wanted = B_FALSE;
	new->lr_read_wanted = B_FALSE;

	mutex_enter(&rl->rl_lock);
	if (type == RL_READER) {
		 
		if (avl_numnodes(&rl->rl_tree) == 0) {
			avl_add(&rl->rl_tree, new);
		} else if (!zfs_rangelock_enter_reader(rl, new, nonblock)) {
			kmem_free(new, sizeof (*new));
			new = NULL;
		}
	} else if (!zfs_rangelock_enter_writer(rl, new, nonblock)) {
		kmem_free(new, sizeof (*new));
		new = NULL;
	}
	mutex_exit(&rl->rl_lock);
	return (new);
}

zfs_locked_range_t *
zfs_rangelock_enter(zfs_rangelock_t *rl, uint64_t off, uint64_t len,
    zfs_rangelock_type_t type)
{
	return (zfs_rangelock_enter_impl(rl, off, len, type, B_FALSE));
}

zfs_locked_range_t *
zfs_rangelock_tryenter(zfs_rangelock_t *rl, uint64_t off, uint64_t len,
    zfs_rangelock_type_t type)
{
	return (zfs_rangelock_enter_impl(rl, off, len, type, B_TRUE));
}

 
static void
zfs_rangelock_free(zfs_locked_range_t *lr)
{
	if (lr->lr_write_wanted)
		cv_destroy(&lr->lr_write_cv);

	if (lr->lr_read_wanted)
		cv_destroy(&lr->lr_read_cv);

	kmem_free(lr, sizeof (zfs_locked_range_t));
}

 
static void
zfs_rangelock_exit_reader(zfs_rangelock_t *rl, zfs_locked_range_t *remove,
    list_t *free_list)
{
	avl_tree_t *tree = &rl->rl_tree;
	uint64_t len;

	 
	if (remove->lr_count == 1) {
		avl_remove(tree, remove);
		if (remove->lr_write_wanted)
			cv_broadcast(&remove->lr_write_cv);
		if (remove->lr_read_wanted)
			cv_broadcast(&remove->lr_read_cv);
		list_insert_tail(free_list, remove);
	} else {
		ASSERT0(remove->lr_count);
		ASSERT0(remove->lr_write_wanted);
		ASSERT0(remove->lr_read_wanted);
		 
		zfs_locked_range_t *lr = avl_find(tree, remove, NULL);
		ASSERT3P(lr, !=, NULL);
		ASSERT3U(lr->lr_count, !=, 0);
		ASSERT3U(lr->lr_type, ==, RL_READER);
		zfs_locked_range_t *next = NULL;
		for (len = remove->lr_length; len != 0; lr = next) {
			len -= lr->lr_length;
			if (len != 0) {
				next = AVL_NEXT(tree, lr);
				ASSERT3P(next, !=, NULL);
				ASSERT3U(lr->lr_offset + lr->lr_length, ==,
				    next->lr_offset);
				ASSERT3U(next->lr_count, !=, 0);
				ASSERT3U(next->lr_type, ==, RL_READER);
			}
			lr->lr_count--;
			if (lr->lr_count == 0) {
				avl_remove(tree, lr);
				if (lr->lr_write_wanted)
					cv_broadcast(&lr->lr_write_cv);
				if (lr->lr_read_wanted)
					cv_broadcast(&lr->lr_read_cv);
				list_insert_tail(free_list, lr);
			}
		}
		kmem_free(remove, sizeof (zfs_locked_range_t));
	}
}

 
void
zfs_rangelock_exit(zfs_locked_range_t *lr)
{
	zfs_rangelock_t *rl = lr->lr_rangelock;
	list_t free_list;
	zfs_locked_range_t *free_lr;

	ASSERT(lr->lr_type == RL_WRITER || lr->lr_type == RL_READER);
	ASSERT(lr->lr_count == 1 || lr->lr_count == 0);
	ASSERT(!lr->lr_proxy);

	 
	list_create(&free_list, sizeof (zfs_locked_range_t),
	    offsetof(zfs_locked_range_t, lr_node));

	mutex_enter(&rl->rl_lock);
	if (lr->lr_type == RL_WRITER) {
		 
		avl_remove(&rl->rl_tree, lr);
		if (lr->lr_write_wanted)
			cv_broadcast(&lr->lr_write_cv);
		if (lr->lr_read_wanted)
			cv_broadcast(&lr->lr_read_cv);
		list_insert_tail(&free_list, lr);
	} else {
		 
		zfs_rangelock_exit_reader(rl, lr, &free_list);
	}
	mutex_exit(&rl->rl_lock);

	while ((free_lr = list_remove_head(&free_list)) != NULL)
		zfs_rangelock_free(free_lr);

	list_destroy(&free_list);
}

 
void
zfs_rangelock_reduce(zfs_locked_range_t *lr, uint64_t off, uint64_t len)
{
	zfs_rangelock_t *rl = lr->lr_rangelock;

	 
	ASSERT3U(avl_numnodes(&rl->rl_tree), ==, 1);
	ASSERT3U(lr->lr_offset, ==, 0);
	ASSERT3U(lr->lr_type, ==, RL_WRITER);
	ASSERT(!lr->lr_proxy);
	ASSERT3U(lr->lr_length, ==, UINT64_MAX);
	ASSERT3U(lr->lr_count, ==, 1);

	mutex_enter(&rl->rl_lock);
	lr->lr_offset = off;
	lr->lr_length = len;
	mutex_exit(&rl->rl_lock);
	if (lr->lr_write_wanted)
		cv_broadcast(&lr->lr_write_cv);
	if (lr->lr_read_wanted)
		cv_broadcast(&lr->lr_read_cv);
}

#if defined(_KERNEL)
EXPORT_SYMBOL(zfs_rangelock_init);
EXPORT_SYMBOL(zfs_rangelock_fini);
EXPORT_SYMBOL(zfs_rangelock_enter);
EXPORT_SYMBOL(zfs_rangelock_tryenter);
EXPORT_SYMBOL(zfs_rangelock_exit);
EXPORT_SYMBOL(zfs_rangelock_reduce);
#endif
