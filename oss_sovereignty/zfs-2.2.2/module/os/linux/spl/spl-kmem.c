 

#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/vmem.h>

 
 
unsigned int spl_kmem_alloc_warn = MIN(16 * PAGE_SIZE, 64 * 1024);
module_param(spl_kmem_alloc_warn, uint, 0644);
MODULE_PARM_DESC(spl_kmem_alloc_warn,
	"Warning threshold in bytes for a kmem_alloc()");
EXPORT_SYMBOL(spl_kmem_alloc_warn);

 
unsigned int spl_kmem_alloc_max = (KMALLOC_MAX_SIZE >> 2);
module_param(spl_kmem_alloc_max, uint, 0644);
MODULE_PARM_DESC(spl_kmem_alloc_max,
	"Maximum size in bytes for a kmem_alloc()");
EXPORT_SYMBOL(spl_kmem_alloc_max);
 

int
kmem_debugging(void)
{
	return (0);
}
EXPORT_SYMBOL(kmem_debugging);

char *
kmem_vasprintf(const char *fmt, va_list ap)
{
	va_list aq;
	char *ptr;

	do {
		va_copy(aq, ap);
		ptr = kvasprintf(kmem_flags_convert(KM_SLEEP), fmt, aq);
		va_end(aq);
	} while (ptr == NULL);

	return (ptr);
}
EXPORT_SYMBOL(kmem_vasprintf);

char *
kmem_asprintf(const char *fmt, ...)
{
	va_list ap;
	char *ptr;

	do {
		va_start(ap, fmt);
		ptr = kvasprintf(kmem_flags_convert(KM_SLEEP), fmt, ap);
		va_end(ap);
	} while (ptr == NULL);

	return (ptr);
}
EXPORT_SYMBOL(kmem_asprintf);

static char *
__strdup(const char *str, int flags)
{
	char *ptr;
	int n;

	n = strlen(str);
	ptr = kmalloc(n + 1, kmem_flags_convert(flags));
	if (ptr)
		memcpy(ptr, str, n + 1);

	return (ptr);
}

char *
kmem_strdup(const char *str)
{
	return (__strdup(str, KM_SLEEP));
}
EXPORT_SYMBOL(kmem_strdup);

void
kmem_strfree(char *str)
{
	kfree(str);
}
EXPORT_SYMBOL(kmem_strfree);

void *
spl_kvmalloc(size_t size, gfp_t lflags)
{
#ifdef HAVE_KVMALLOC
	 
	if ((lflags & GFP_KERNEL) == GFP_KERNEL)
		return (kvmalloc(size, lflags));
#endif

	gfp_t kmalloc_lflags = lflags;

	if (size > PAGE_SIZE) {
		 
		kmalloc_lflags |= __GFP_NOWARN;

		 
		if (!(kmalloc_lflags & __GFP_RETRY_MAYFAIL) ||
		    (size <= PAGE_SIZE << PAGE_ALLOC_COSTLY_ORDER)) {
			kmalloc_lflags |= __GFP_NORETRY;
		}
	}

	 
	void *ptr = kmalloc_node(size, kmalloc_lflags, NUMA_NO_NODE);
	if (ptr || size <= PAGE_SIZE ||
	    (lflags & __GFP_RECLAIM) != __GFP_RECLAIM) {
		return (ptr);
	}

	return (spl_vmalloc(size, lflags | __GFP_HIGHMEM));
}

 
inline void *
spl_kmem_alloc_impl(size_t size, int flags, int node)
{
	gfp_t lflags = kmem_flags_convert(flags);
	void *ptr;

	 
	if ((spl_kmem_alloc_warn > 0) && (size > spl_kmem_alloc_warn) &&
	    !(flags & KM_VMEM)) {
		printk(KERN_WARNING
		    "Large kmem_alloc(%lu, 0x%x), please file an issue at:\n"
		    "https://github.com/openzfs/zfs/issues/new\n",
		    (unsigned long)size, flags);
		dump_stack();
	}

	 
	do {
		 
		if (size > spl_kmem_alloc_max) {
			if (flags & KM_VMEM) {
				ptr = spl_vmalloc(size, lflags | __GFP_HIGHMEM);
			} else {
				return (NULL);
			}
		} else {
			 
#ifdef CONFIG_HIGHMEM
			if (flags & KM_VMEM) {
#else
			if ((flags & KM_VMEM) || !(flags & KM_NOSLEEP)) {
#endif
				ptr = spl_kvmalloc(size, lflags);
			} else {
				ptr = kmalloc_node(size, lflags, node);
			}
		}

		if (likely(ptr) || (flags & KM_NOSLEEP))
			return (ptr);

		 
		if ((lflags & GFP_KERNEL) == GFP_KERNEL)
			lflags |= __GFP_RETRY_MAYFAIL;

		 
		cond_resched();
	} while (1);

	return (NULL);
}

inline void
spl_kmem_free_impl(const void *buf, size_t size)
{
	if (is_vmalloc_addr(buf))
		vfree(buf);
	else
		kfree(buf);
}

 
#ifdef DEBUG_KMEM

 
#ifdef HAVE_ATOMIC64_T
atomic64_t kmem_alloc_used = ATOMIC64_INIT(0);
unsigned long long kmem_alloc_max = 0;
#else   
atomic_t kmem_alloc_used = ATOMIC_INIT(0);
unsigned long long kmem_alloc_max = 0;
#endif  

EXPORT_SYMBOL(kmem_alloc_used);
EXPORT_SYMBOL(kmem_alloc_max);

inline void *
spl_kmem_alloc_debug(size_t size, int flags, int node)
{
	void *ptr;

	ptr = spl_kmem_alloc_impl(size, flags, node);
	if (ptr) {
		kmem_alloc_used_add(size);
		if (unlikely(kmem_alloc_used_read() > kmem_alloc_max))
			kmem_alloc_max = kmem_alloc_used_read();
	}

	return (ptr);
}

inline void
spl_kmem_free_debug(const void *ptr, size_t size)
{
	kmem_alloc_used_sub(size);
	spl_kmem_free_impl(ptr, size);
}

 
#ifdef DEBUG_KMEM_TRACKING

#include <linux/hash.h>
#include <linux/ctype.h>

#define	KMEM_HASH_BITS		10
#define	KMEM_TABLE_SIZE		(1 << KMEM_HASH_BITS)

typedef struct kmem_debug {
	struct hlist_node kd_hlist;	 
	struct list_head kd_list;	 
	void *kd_addr;			 
	size_t kd_size;			 
	const char *kd_func;		 
	int kd_line;			 
} kmem_debug_t;

static spinlock_t kmem_lock;
static struct hlist_head kmem_table[KMEM_TABLE_SIZE];
static struct list_head kmem_list;

static kmem_debug_t *
kmem_del_init(spinlock_t *lock, struct hlist_head *table,
    int bits, const void *addr)
{
	struct hlist_head *head;
	struct hlist_node *node = NULL;
	struct kmem_debug *p;
	unsigned long flags;

	spin_lock_irqsave(lock, flags);

	head = &table[hash_ptr((void *)addr, bits)];
	hlist_for_each(node, head) {
		p = list_entry(node, struct kmem_debug, kd_hlist);
		if (p->kd_addr == addr) {
			hlist_del_init(&p->kd_hlist);
			list_del_init(&p->kd_list);
			spin_unlock_irqrestore(lock, flags);
			return (p);
		}
	}

	spin_unlock_irqrestore(lock, flags);

	return (NULL);
}

inline void *
spl_kmem_alloc_track(size_t size, int flags,
    const char *func, int line, int node)
{
	void *ptr = NULL;
	kmem_debug_t *dptr;
	unsigned long irq_flags;

	dptr = kmalloc(sizeof (kmem_debug_t), kmem_flags_convert(flags));
	if (dptr == NULL)
		return (NULL);

	dptr->kd_func = __strdup(func, flags);
	if (dptr->kd_func == NULL) {
		kfree(dptr);
		return (NULL);
	}

	ptr = spl_kmem_alloc_debug(size, flags, node);
	if (ptr == NULL) {
		kfree(dptr->kd_func);
		kfree(dptr);
		return (NULL);
	}

	INIT_HLIST_NODE(&dptr->kd_hlist);
	INIT_LIST_HEAD(&dptr->kd_list);

	dptr->kd_addr = ptr;
	dptr->kd_size = size;
	dptr->kd_line = line;

	spin_lock_irqsave(&kmem_lock, irq_flags);
	hlist_add_head(&dptr->kd_hlist,
	    &kmem_table[hash_ptr(ptr, KMEM_HASH_BITS)]);
	list_add_tail(&dptr->kd_list, &kmem_list);
	spin_unlock_irqrestore(&kmem_lock, irq_flags);

	return (ptr);
}

inline void
spl_kmem_free_track(const void *ptr, size_t size)
{
	kmem_debug_t *dptr;

	 
	if (ptr == NULL)
		return;

	 
	dptr = kmem_del_init(&kmem_lock, kmem_table, KMEM_HASH_BITS, ptr);
	ASSERT3P(dptr, !=, NULL);
	ASSERT3S(dptr->kd_size, ==, size);

	kfree(dptr->kd_func);
	kfree(dptr);

	spl_kmem_free_debug(ptr, size);
}
#endif  
#endif  

 
void *
spl_kmem_alloc(size_t size, int flags, const char *func, int line)
{
	ASSERT0(flags & ~KM_PUBLIC_MASK);

#if !defined(DEBUG_KMEM)
	return (spl_kmem_alloc_impl(size, flags, NUMA_NO_NODE));
#elif !defined(DEBUG_KMEM_TRACKING)
	return (spl_kmem_alloc_debug(size, flags, NUMA_NO_NODE));
#else
	return (spl_kmem_alloc_track(size, flags, func, line, NUMA_NO_NODE));
#endif
}
EXPORT_SYMBOL(spl_kmem_alloc);

void *
spl_kmem_zalloc(size_t size, int flags, const char *func, int line)
{
	ASSERT0(flags & ~KM_PUBLIC_MASK);

	flags |= KM_ZERO;

#if !defined(DEBUG_KMEM)
	return (spl_kmem_alloc_impl(size, flags, NUMA_NO_NODE));
#elif !defined(DEBUG_KMEM_TRACKING)
	return (spl_kmem_alloc_debug(size, flags, NUMA_NO_NODE));
#else
	return (spl_kmem_alloc_track(size, flags, func, line, NUMA_NO_NODE));
#endif
}
EXPORT_SYMBOL(spl_kmem_zalloc);

void
spl_kmem_free(const void *buf, size_t size)
{
#if !defined(DEBUG_KMEM)
	return (spl_kmem_free_impl(buf, size));
#elif !defined(DEBUG_KMEM_TRACKING)
	return (spl_kmem_free_debug(buf, size));
#else
	return (spl_kmem_free_track(buf, size));
#endif
}
EXPORT_SYMBOL(spl_kmem_free);

#if defined(DEBUG_KMEM) && defined(DEBUG_KMEM_TRACKING)
static char *
spl_sprintf_addr(kmem_debug_t *kd, char *str, int len, int min)
{
	int size = ((len - 1) < kd->kd_size) ? (len - 1) : kd->kd_size;
	int i, flag = 1;

	ASSERT(str != NULL && len >= 17);
	memset(str, 0, len);

	 
	for (i = 0; i < size; i++) {
		str[i] = ((char *)(kd->kd_addr))[i];
		if (isprint(str[i])) {
			continue;
		} else {
			 
			if (i > min)
				break;

			flag = 0;
			break;
		}
	}

	if (!flag) {
		sprintf(str, "%02x%02x%02x%02x%02x%02x%02x%02x",
		    *((uint8_t *)kd->kd_addr),
		    *((uint8_t *)kd->kd_addr + 2),
		    *((uint8_t *)kd->kd_addr + 4),
		    *((uint8_t *)kd->kd_addr + 6),
		    *((uint8_t *)kd->kd_addr + 8),
		    *((uint8_t *)kd->kd_addr + 10),
		    *((uint8_t *)kd->kd_addr + 12),
		    *((uint8_t *)kd->kd_addr + 14));
	}

	return (str);
}

static int
spl_kmem_init_tracking(struct list_head *list, spinlock_t *lock, int size)
{
	int i;

	spin_lock_init(lock);
	INIT_LIST_HEAD(list);

	for (i = 0; i < size; i++)
		INIT_HLIST_HEAD(&kmem_table[i]);

	return (0);
}

static void
spl_kmem_fini_tracking(struct list_head *list, spinlock_t *lock)
{
	unsigned long flags;
	kmem_debug_t *kd = NULL;
	char str[17];

	spin_lock_irqsave(lock, flags);
	if (!list_empty(list))
		printk(KERN_WARNING "%-16s %-5s %-16s %s:%s\n", "address",
		    "size", "data", "func", "line");

	list_for_each_entry(kd, list, kd_list) {
		printk(KERN_WARNING "%p %-5d %-16s %s:%d\n", kd->kd_addr,
		    (int)kd->kd_size, spl_sprintf_addr(kd, str, 17, 8),
		    kd->kd_func, kd->kd_line);
	}

	spin_unlock_irqrestore(lock, flags);
}
#endif  

int
spl_kmem_init(void)
{

#ifdef DEBUG_KMEM
	kmem_alloc_used_set(0);



#ifdef DEBUG_KMEM_TRACKING
	spl_kmem_init_tracking(&kmem_list, &kmem_lock, KMEM_TABLE_SIZE);
#endif  
#endif  

	return (0);
}

void
spl_kmem_fini(void)
{
#ifdef DEBUG_KMEM
	 
	if (kmem_alloc_used_read() != 0)
		printk(KERN_WARNING "kmem leaked %ld/%llu bytes\n",
		    (unsigned long)kmem_alloc_used_read(), kmem_alloc_max);

#ifdef DEBUG_KMEM_TRACKING
	spl_kmem_fini_tracking(&kmem_list, &kmem_lock);
#endif  
#endif  
}
