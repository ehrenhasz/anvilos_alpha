
 
#include <linux/trace_recursion.h>
#include <linux/trace_events.h>
#include <linux/ring_buffer.h>
#include <linux/trace_clock.h>
#include <linux/sched/clock.h>
#include <linux/trace_seq.h>
#include <linux/spinlock.h>
#include <linux/irq_work.h>
#include <linux/security.h>
#include <linux/uaccess.h>
#include <linux/hardirq.h>
#include <linux/kthread.h>	 
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/hash.h>
#include <linux/list.h>
#include <linux/cpu.h>
#include <linux/oom.h>

#include <asm/local.h>

 
#define TS_MSB		(0xf8ULL << 56)
#define ABS_TS_MASK	(~TS_MSB)

static void update_pages_handler(struct work_struct *work);

 
int ring_buffer_print_entry_header(struct trace_seq *s)
{
	trace_seq_puts(s, "# compressed entry header\n");
	trace_seq_puts(s, "\ttype_len    :    5 bits\n");
	trace_seq_puts(s, "\ttime_delta  :   27 bits\n");
	trace_seq_puts(s, "\tarray       :   32 bits\n");
	trace_seq_putc(s, '\n');
	trace_seq_printf(s, "\tpadding     : type == %d\n",
			 RINGBUF_TYPE_PADDING);
	trace_seq_printf(s, "\ttime_extend : type == %d\n",
			 RINGBUF_TYPE_TIME_EXTEND);
	trace_seq_printf(s, "\ttime_stamp : type == %d\n",
			 RINGBUF_TYPE_TIME_STAMP);
	trace_seq_printf(s, "\tdata max type_len  == %d\n",
			 RINGBUF_TYPE_DATA_TYPE_LEN_MAX);

	return !trace_seq_has_overflowed(s);
}

 

 
#define RB_BUFFER_OFF		(1 << 20)

#define BUF_PAGE_HDR_SIZE offsetof(struct buffer_data_page, data)

#define RB_EVNT_HDR_SIZE (offsetof(struct ring_buffer_event, array))
#define RB_ALIGNMENT		4U
#define RB_MAX_SMALL_DATA	(RB_ALIGNMENT * RINGBUF_TYPE_DATA_TYPE_LEN_MAX)
#define RB_EVNT_MIN_SIZE	8U	 

#ifndef CONFIG_HAVE_64BIT_ALIGNED_ACCESS
# define RB_FORCE_8BYTE_ALIGNMENT	0
# define RB_ARCH_ALIGNMENT		RB_ALIGNMENT
#else
# define RB_FORCE_8BYTE_ALIGNMENT	1
# define RB_ARCH_ALIGNMENT		8U
#endif

#define RB_ALIGN_DATA		__aligned(RB_ARCH_ALIGNMENT)

 
#define RINGBUF_TYPE_DATA 0 ... RINGBUF_TYPE_DATA_TYPE_LEN_MAX

enum {
	RB_LEN_TIME_EXTEND = 8,
	RB_LEN_TIME_STAMP =  8,
};

#define skip_time_extend(event) \
	((struct ring_buffer_event *)((char *)event + RB_LEN_TIME_EXTEND))

#define extended_time(event) \
	(event->type_len >= RINGBUF_TYPE_TIME_EXTEND)

static inline bool rb_null_event(struct ring_buffer_event *event)
{
	return event->type_len == RINGBUF_TYPE_PADDING && !event->time_delta;
}

static void rb_event_set_padding(struct ring_buffer_event *event)
{
	 
	event->type_len = RINGBUF_TYPE_PADDING;
	event->time_delta = 0;
}

static unsigned
rb_event_data_length(struct ring_buffer_event *event)
{
	unsigned length;

	if (event->type_len)
		length = event->type_len * RB_ALIGNMENT;
	else
		length = event->array[0];
	return length + RB_EVNT_HDR_SIZE;
}

 
static inline unsigned
rb_event_length(struct ring_buffer_event *event)
{
	switch (event->type_len) {
	case RINGBUF_TYPE_PADDING:
		if (rb_null_event(event))
			 
			return -1;
		return  event->array[0] + RB_EVNT_HDR_SIZE;

	case RINGBUF_TYPE_TIME_EXTEND:
		return RB_LEN_TIME_EXTEND;

	case RINGBUF_TYPE_TIME_STAMP:
		return RB_LEN_TIME_STAMP;

	case RINGBUF_TYPE_DATA:
		return rb_event_data_length(event);
	default:
		WARN_ON_ONCE(1);
	}
	 
	return 0;
}

 
static inline unsigned
rb_event_ts_length(struct ring_buffer_event *event)
{
	unsigned len = 0;

	if (extended_time(event)) {
		 
		len = RB_LEN_TIME_EXTEND;
		event = skip_time_extend(event);
	}
	return len + rb_event_length(event);
}

 
unsigned ring_buffer_event_length(struct ring_buffer_event *event)
{
	unsigned length;

	if (extended_time(event))
		event = skip_time_extend(event);

	length = rb_event_length(event);
	if (event->type_len > RINGBUF_TYPE_DATA_TYPE_LEN_MAX)
		return length;
	length -= RB_EVNT_HDR_SIZE;
	if (length > RB_MAX_SMALL_DATA + sizeof(event->array[0]))
                length -= sizeof(event->array[0]);
	return length;
}
EXPORT_SYMBOL_GPL(ring_buffer_event_length);

 
static __always_inline void *
rb_event_data(struct ring_buffer_event *event)
{
	if (extended_time(event))
		event = skip_time_extend(event);
	WARN_ON_ONCE(event->type_len > RINGBUF_TYPE_DATA_TYPE_LEN_MAX);
	 
	if (event->type_len)
		return (void *)&event->array[0];
	 
	return (void *)&event->array[1];
}

 
void *ring_buffer_event_data(struct ring_buffer_event *event)
{
	return rb_event_data(event);
}
EXPORT_SYMBOL_GPL(ring_buffer_event_data);

#define for_each_buffer_cpu(buffer, cpu)		\
	for_each_cpu(cpu, buffer->cpumask)

#define for_each_online_buffer_cpu(buffer, cpu)		\
	for_each_cpu_and(cpu, buffer->cpumask, cpu_online_mask)

#define TS_SHIFT	27
#define TS_MASK		((1ULL << TS_SHIFT) - 1)
#define TS_DELTA_TEST	(~TS_MASK)

static u64 rb_event_time_stamp(struct ring_buffer_event *event)
{
	u64 ts;

	ts = event->array[0];
	ts <<= TS_SHIFT;
	ts += event->time_delta;

	return ts;
}

 
#define RB_MISSED_EVENTS	(1 << 31)
 
#define RB_MISSED_STORED	(1 << 30)

struct buffer_data_page {
	u64		 time_stamp;	 
	local_t		 commit;	 
	unsigned char	 data[] RB_ALIGN_DATA;	 
};

 
struct buffer_page {
	struct list_head list;		 
	local_t		 write;		 
	unsigned	 read;		 
	local_t		 entries;	 
	unsigned long	 real_end;	 
	struct buffer_data_page *page;	 
};

 
#define RB_WRITE_MASK		0xfffff
#define RB_WRITE_INTCNT		(1 << 20)

static void rb_init_page(struct buffer_data_page *bpage)
{
	local_set(&bpage->commit, 0);
}

static __always_inline unsigned int rb_page_commit(struct buffer_page *bpage)
{
	return local_read(&bpage->page->commit);
}

static void free_buffer_page(struct buffer_page *bpage)
{
	free_page((unsigned long)bpage->page);
	kfree(bpage);
}

 
static inline bool test_time_stamp(u64 delta)
{
	return !!(delta & TS_DELTA_TEST);
}

#define BUF_PAGE_SIZE (PAGE_SIZE - BUF_PAGE_HDR_SIZE)

 
#define BUF_MAX_DATA_SIZE (BUF_PAGE_SIZE - (sizeof(u32) * 2))

int ring_buffer_print_page_header(struct trace_seq *s)
{
	struct buffer_data_page field;

	trace_seq_printf(s, "\tfield: u64 timestamp;\t"
			 "offset:0;\tsize:%u;\tsigned:%u;\n",
			 (unsigned int)sizeof(field.time_stamp),
			 (unsigned int)is_signed_type(u64));

	trace_seq_printf(s, "\tfield: local_t commit;\t"
			 "offset:%u;\tsize:%u;\tsigned:%u;\n",
			 (unsigned int)offsetof(typeof(field), commit),
			 (unsigned int)sizeof(field.commit),
			 (unsigned int)is_signed_type(long));

	trace_seq_printf(s, "\tfield: int overwrite;\t"
			 "offset:%u;\tsize:%u;\tsigned:%u;\n",
			 (unsigned int)offsetof(typeof(field), commit),
			 1,
			 (unsigned int)is_signed_type(long));

	trace_seq_printf(s, "\tfield: char data;\t"
			 "offset:%u;\tsize:%u;\tsigned:%u;\n",
			 (unsigned int)offsetof(typeof(field), data),
			 (unsigned int)BUF_PAGE_SIZE,
			 (unsigned int)is_signed_type(char));

	return !trace_seq_has_overflowed(s);
}

struct rb_irq_work {
	struct irq_work			work;
	wait_queue_head_t		waiters;
	wait_queue_head_t		full_waiters;
	long				wait_index;
	bool				waiters_pending;
	bool				full_waiters_pending;
	bool				wakeup_full;
};

 
struct rb_event_info {
	u64			ts;
	u64			delta;
	u64			before;
	u64			after;
	unsigned long		length;
	struct buffer_page	*tail_page;
	int			add_timestamp;
};

 
enum {
	RB_ADD_STAMP_NONE		= 0,
	RB_ADD_STAMP_EXTEND		= BIT(1),
	RB_ADD_STAMP_ABSOLUTE		= BIT(2),
	RB_ADD_STAMP_FORCE		= BIT(3)
};
 
enum {
	RB_CTX_TRANSITION,
	RB_CTX_NMI,
	RB_CTX_IRQ,
	RB_CTX_SOFTIRQ,
	RB_CTX_NORMAL,
	RB_CTX_MAX
};

#if BITS_PER_LONG == 32
#define RB_TIME_32
#endif

 


#ifdef RB_TIME_32

struct rb_time_struct {
	local_t		cnt;
	local_t		top;
	local_t		bottom;
	local_t		msb;
};
#else
#include <asm/local64.h>
struct rb_time_struct {
	local64_t	time;
};
#endif
typedef struct rb_time_struct rb_time_t;

#define MAX_NEST	5

 
struct ring_buffer_per_cpu {
	int				cpu;
	atomic_t			record_disabled;
	atomic_t			resize_disabled;
	struct trace_buffer	*buffer;
	raw_spinlock_t			reader_lock;	 
	arch_spinlock_t			lock;
	struct lock_class_key		lock_key;
	struct buffer_data_page		*free_page;
	unsigned long			nr_pages;
	unsigned int			current_context;
	struct list_head		*pages;
	struct buffer_page		*head_page;	 
	struct buffer_page		*tail_page;	 
	struct buffer_page		*commit_page;	 
	struct buffer_page		*reader_page;
	unsigned long			lost_events;
	unsigned long			last_overrun;
	unsigned long			nest;
	local_t				entries_bytes;
	local_t				entries;
	local_t				overrun;
	local_t				commit_overrun;
	local_t				dropped_events;
	local_t				committing;
	local_t				commits;
	local_t				pages_touched;
	local_t				pages_lost;
	local_t				pages_read;
	long				last_pages_touch;
	size_t				shortest_full;
	unsigned long			read;
	unsigned long			read_bytes;
	rb_time_t			write_stamp;
	rb_time_t			before_stamp;
	u64				event_stamp[MAX_NEST];
	u64				read_stamp;
	 
	unsigned long			pages_removed;
	 
	long				nr_pages_to_update;
	struct list_head		new_pages;  
	struct work_struct		update_pages_work;
	struct completion		update_done;

	struct rb_irq_work		irq_work;
};

struct trace_buffer {
	unsigned			flags;
	int				cpus;
	atomic_t			record_disabled;
	atomic_t			resizing;
	cpumask_var_t			cpumask;

	struct lock_class_key		*reader_lock_key;

	struct mutex			mutex;

	struct ring_buffer_per_cpu	**buffers;

	struct hlist_node		node;
	u64				(*clock)(void);

	struct rb_irq_work		irq_work;
	bool				time_stamp_abs;
};

struct ring_buffer_iter {
	struct ring_buffer_per_cpu	*cpu_buffer;
	unsigned long			head;
	unsigned long			next_event;
	struct buffer_page		*head_page;
	struct buffer_page		*cache_reader_page;
	unsigned long			cache_read;
	unsigned long			cache_pages_removed;
	u64				read_stamp;
	u64				page_stamp;
	struct ring_buffer_event	*event;
	int				missed_events;
};

#ifdef RB_TIME_32

 
#define RB_TIME_SHIFT	30
#define RB_TIME_VAL_MASK ((1 << RB_TIME_SHIFT) - 1)
#define RB_TIME_MSB_SHIFT	 60

static inline int rb_time_cnt(unsigned long val)
{
	return (val >> RB_TIME_SHIFT) & 3;
}

static inline u64 rb_time_val(unsigned long top, unsigned long bottom)
{
	u64 val;

	val = top & RB_TIME_VAL_MASK;
	val <<= RB_TIME_SHIFT;
	val |= bottom & RB_TIME_VAL_MASK;

	return val;
}

static inline bool __rb_time_read(rb_time_t *t, u64 *ret, unsigned long *cnt)
{
	unsigned long top, bottom, msb;
	unsigned long c;

	 
	do {
		c = local_read(&t->cnt);
		top = local_read(&t->top);
		bottom = local_read(&t->bottom);
		msb = local_read(&t->msb);
	} while (c != local_read(&t->cnt));

	*cnt = rb_time_cnt(top);

	 
	if (*cnt != rb_time_cnt(msb) || *cnt != rb_time_cnt(bottom))
		return false;

	 
	*ret = rb_time_val(top, bottom) | ((u64)msb << RB_TIME_MSB_SHIFT);
	return true;
}

static bool rb_time_read(rb_time_t *t, u64 *ret)
{
	unsigned long cnt;

	return __rb_time_read(t, ret, &cnt);
}

static inline unsigned long rb_time_val_cnt(unsigned long val, unsigned long cnt)
{
	return (val & RB_TIME_VAL_MASK) | ((cnt & 3) << RB_TIME_SHIFT);
}

static inline void rb_time_split(u64 val, unsigned long *top, unsigned long *bottom,
				 unsigned long *msb)
{
	*top = (unsigned long)((val >> RB_TIME_SHIFT) & RB_TIME_VAL_MASK);
	*bottom = (unsigned long)(val & RB_TIME_VAL_MASK);
	*msb = (unsigned long)(val >> RB_TIME_MSB_SHIFT);
}

static inline void rb_time_val_set(local_t *t, unsigned long val, unsigned long cnt)
{
	val = rb_time_val_cnt(val, cnt);
	local_set(t, val);
}

static void rb_time_set(rb_time_t *t, u64 val)
{
	unsigned long cnt, top, bottom, msb;

	rb_time_split(val, &top, &bottom, &msb);

	 
	do {
		cnt = local_inc_return(&t->cnt);
		rb_time_val_set(&t->top, top, cnt);
		rb_time_val_set(&t->bottom, bottom, cnt);
		rb_time_val_set(&t->msb, val >> RB_TIME_MSB_SHIFT, cnt);
	} while (cnt != local_read(&t->cnt));
}

static inline bool
rb_time_read_cmpxchg(local_t *l, unsigned long expect, unsigned long set)
{
	return local_try_cmpxchg(l, &expect, set);
}

#else  

 

static inline bool rb_time_read(rb_time_t *t, u64 *ret)
{
	*ret = local64_read(&t->time);
	return true;
}
static void rb_time_set(rb_time_t *t, u64 val)
{
	local64_set(&t->time, val);
}
#endif

 

#ifdef RB_VERIFY_EVENT
static struct list_head *rb_list_head(struct list_head *list);
static void verify_event(struct ring_buffer_per_cpu *cpu_buffer,
			 void *event)
{
	struct buffer_page *page = cpu_buffer->commit_page;
	struct buffer_page *tail_page = READ_ONCE(cpu_buffer->tail_page);
	struct list_head *next;
	long commit, write;
	unsigned long addr = (unsigned long)event;
	bool done = false;
	int stop = 0;

	 
	do {
		if (page == tail_page || WARN_ON_ONCE(stop++ > 100))
			done = true;
		commit = local_read(&page->page->commit);
		write = local_read(&page->write);
		if (addr >= (unsigned long)&page->page->data[commit] &&
		    addr < (unsigned long)&page->page->data[write])
			return;

		next = rb_list_head(page->list.next);
		page = list_entry(next, struct buffer_page, list);
	} while (!done);
	WARN_ON_ONCE(1);
}
#else
static inline void verify_event(struct ring_buffer_per_cpu *cpu_buffer,
			 void *event)
{
}
#endif

 
static inline u64 rb_fix_abs_ts(u64 abs, u64 save_ts)
{
	if (save_ts & TS_MSB) {
		abs |= save_ts & TS_MSB;
		 
		if (unlikely(abs < save_ts))
			abs += 1ULL << 59;
	}
	return abs;
}

static inline u64 rb_time_stamp(struct trace_buffer *buffer);

 
u64 ring_buffer_event_time_stamp(struct trace_buffer *buffer,
				 struct ring_buffer_event *event)
{
	struct ring_buffer_per_cpu *cpu_buffer = buffer->buffers[smp_processor_id()];
	unsigned int nest;
	u64 ts;

	 
	if (event->type_len == RINGBUF_TYPE_TIME_STAMP) {
		ts = rb_event_time_stamp(event);
		return rb_fix_abs_ts(ts, cpu_buffer->tail_page->page->time_stamp);
	}

	nest = local_read(&cpu_buffer->committing);
	verify_event(cpu_buffer, event);
	if (WARN_ON_ONCE(!nest))
		goto fail;

	 
	if (likely(--nest < MAX_NEST))
		return cpu_buffer->event_stamp[nest];

	 
	WARN_ONCE(1, "nest (%d) greater than max", nest);

 fail:
	 
	if (!rb_time_read(&cpu_buffer->write_stamp, &ts))
		 
		ts = rb_time_stamp(cpu_buffer->buffer);

	return ts;
}

 
size_t ring_buffer_nr_pages(struct trace_buffer *buffer, int cpu)
{
	return buffer->buffers[cpu]->nr_pages;
}

 
size_t ring_buffer_nr_dirty_pages(struct trace_buffer *buffer, int cpu)
{
	size_t read;
	size_t lost;
	size_t cnt;

	read = local_read(&buffer->buffers[cpu]->pages_read);
	lost = local_read(&buffer->buffers[cpu]->pages_lost);
	cnt = local_read(&buffer->buffers[cpu]->pages_touched);

	if (WARN_ON_ONCE(cnt < lost))
		return 0;

	cnt -= lost;

	 
	if (cnt < read) {
		WARN_ON_ONCE(read > cnt + 1);
		return 0;
	}

	return cnt - read;
}

static __always_inline bool full_hit(struct trace_buffer *buffer, int cpu, int full)
{
	struct ring_buffer_per_cpu *cpu_buffer = buffer->buffers[cpu];
	size_t nr_pages;
	size_t dirty;

	nr_pages = cpu_buffer->nr_pages;
	if (!nr_pages || !full)
		return true;

	 
	dirty = ring_buffer_nr_dirty_pages(buffer, cpu) + 1;

	return (dirty * 100) >= (full * nr_pages);
}

 
static void rb_wake_up_waiters(struct irq_work *work)
{
	struct rb_irq_work *rbwork = container_of(work, struct rb_irq_work, work);

	wake_up_all(&rbwork->waiters);
	if (rbwork->full_waiters_pending || rbwork->wakeup_full) {
		rbwork->wakeup_full = false;
		rbwork->full_waiters_pending = false;
		wake_up_all(&rbwork->full_waiters);
	}
}

 
void ring_buffer_wake_waiters(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct rb_irq_work *rbwork;

	if (!buffer)
		return;

	if (cpu == RING_BUFFER_ALL_CPUS) {

		 
		for_each_buffer_cpu(buffer, cpu)
			ring_buffer_wake_waiters(buffer, cpu);

		rbwork = &buffer->irq_work;
	} else {
		if (WARN_ON_ONCE(!buffer->buffers))
			return;
		if (WARN_ON_ONCE(cpu >= nr_cpu_ids))
			return;

		cpu_buffer = buffer->buffers[cpu];
		 
		if (!cpu_buffer)
			return;
		rbwork = &cpu_buffer->irq_work;
	}

	rbwork->wait_index++;
	 
	smp_wmb();

	 
	irq_work_queue(&rbwork->work);
}

 
int ring_buffer_wait(struct trace_buffer *buffer, int cpu, int full)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	DEFINE_WAIT(wait);
	struct rb_irq_work *work;
	long wait_index;
	int ret = 0;

	 
	if (cpu == RING_BUFFER_ALL_CPUS) {
		work = &buffer->irq_work;
		 
		full = 0;
	} else {
		if (!cpumask_test_cpu(cpu, buffer->cpumask))
			return -ENODEV;
		cpu_buffer = buffer->buffers[cpu];
		work = &cpu_buffer->irq_work;
	}

	wait_index = READ_ONCE(work->wait_index);

	while (true) {
		if (full)
			prepare_to_wait(&work->full_waiters, &wait, TASK_INTERRUPTIBLE);
		else
			prepare_to_wait(&work->waiters, &wait, TASK_INTERRUPTIBLE);

		 
		if (full)
			work->full_waiters_pending = true;
		else
			work->waiters_pending = true;

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		if (cpu == RING_BUFFER_ALL_CPUS && !ring_buffer_empty(buffer))
			break;

		if (cpu != RING_BUFFER_ALL_CPUS &&
		    !ring_buffer_empty_cpu(buffer, cpu)) {
			unsigned long flags;
			bool pagebusy;
			bool done;

			if (!full)
				break;

			raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
			pagebusy = cpu_buffer->reader_page == cpu_buffer->commit_page;
			done = !pagebusy && full_hit(buffer, cpu, full);

			if (!cpu_buffer->shortest_full ||
			    cpu_buffer->shortest_full > full)
				cpu_buffer->shortest_full = full;
			raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);
			if (done)
				break;
		}

		schedule();

		 
		smp_rmb();
		if (wait_index != work->wait_index)
			break;
	}

	if (full)
		finish_wait(&work->full_waiters, &wait);
	else
		finish_wait(&work->waiters, &wait);

	return ret;
}

 
__poll_t ring_buffer_poll_wait(struct trace_buffer *buffer, int cpu,
			  struct file *filp, poll_table *poll_table, int full)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct rb_irq_work *work;

	if (cpu == RING_BUFFER_ALL_CPUS) {
		work = &buffer->irq_work;
		full = 0;
	} else {
		if (!cpumask_test_cpu(cpu, buffer->cpumask))
			return -EINVAL;

		cpu_buffer = buffer->buffers[cpu];
		work = &cpu_buffer->irq_work;
	}

	if (full) {
		poll_wait(filp, &work->full_waiters, poll_table);
		work->full_waiters_pending = true;
		if (!cpu_buffer->shortest_full ||
		    cpu_buffer->shortest_full > full)
			cpu_buffer->shortest_full = full;
	} else {
		poll_wait(filp, &work->waiters, poll_table);
		work->waiters_pending = true;
	}

	 
	smp_mb();

	if (full)
		return full_hit(buffer, cpu, full) ? EPOLLIN | EPOLLRDNORM : 0;

	if ((cpu == RING_BUFFER_ALL_CPUS && !ring_buffer_empty(buffer)) ||
	    (cpu != RING_BUFFER_ALL_CPUS && !ring_buffer_empty_cpu(buffer, cpu)))
		return EPOLLIN | EPOLLRDNORM;
	return 0;
}

 
#define RB_WARN_ON(b, cond)						\
	({								\
		int _____ret = unlikely(cond);				\
		if (_____ret) {						\
			if (__same_type(*(b), struct ring_buffer_per_cpu)) { \
				struct ring_buffer_per_cpu *__b =	\
					(void *)b;			\
				atomic_inc(&__b->buffer->record_disabled); \
			} else						\
				atomic_inc(&b->record_disabled);	\
			WARN_ON(1);					\
		}							\
		_____ret;						\
	})

 
#define DEBUG_SHIFT 0

static inline u64 rb_time_stamp(struct trace_buffer *buffer)
{
	u64 ts;

	 
	if (IS_ENABLED(CONFIG_RETPOLINE) && likely(buffer->clock == trace_clock_local))
		ts = trace_clock_local();
	else
		ts = buffer->clock();

	 
	return ts << DEBUG_SHIFT;
}

u64 ring_buffer_time_stamp(struct trace_buffer *buffer)
{
	u64 time;

	preempt_disable_notrace();
	time = rb_time_stamp(buffer);
	preempt_enable_notrace();

	return time;
}
EXPORT_SYMBOL_GPL(ring_buffer_time_stamp);

void ring_buffer_normalize_time_stamp(struct trace_buffer *buffer,
				      int cpu, u64 *ts)
{
	 
	*ts >>= DEBUG_SHIFT;
}
EXPORT_SYMBOL_GPL(ring_buffer_normalize_time_stamp);

 

#define RB_PAGE_NORMAL		0UL
#define RB_PAGE_HEAD		1UL
#define RB_PAGE_UPDATE		2UL


#define RB_FLAG_MASK		3UL

 
#define RB_PAGE_MOVED		4UL

 
static struct list_head *rb_list_head(struct list_head *list)
{
	unsigned long val = (unsigned long)list;

	return (struct list_head *)(val & ~RB_FLAG_MASK);
}

 
static inline int
rb_is_head_page(struct buffer_page *page, struct list_head *list)
{
	unsigned long val;

	val = (unsigned long)list->next;

	if ((val & ~RB_FLAG_MASK) != (unsigned long)&page->list)
		return RB_PAGE_MOVED;

	return val & RB_FLAG_MASK;
}

 
static bool rb_is_reader_page(struct buffer_page *page)
{
	struct list_head *list = page->list.prev;

	return rb_list_head(list->next) != &page->list;
}

 
static void rb_set_list_to_head(struct list_head *list)
{
	unsigned long *ptr;

	ptr = (unsigned long *)&list->next;
	*ptr |= RB_PAGE_HEAD;
	*ptr &= ~RB_PAGE_UPDATE;
}

 
static void rb_head_page_activate(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct buffer_page *head;

	head = cpu_buffer->head_page;
	if (!head)
		return;

	 
	rb_set_list_to_head(head->list.prev);
}

static void rb_list_head_clear(struct list_head *list)
{
	unsigned long *ptr = (unsigned long *)&list->next;

	*ptr &= ~RB_FLAG_MASK;
}

 
static void
rb_head_page_deactivate(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct list_head *hd;

	 
	rb_list_head_clear(cpu_buffer->pages);

	list_for_each(hd, cpu_buffer->pages)
		rb_list_head_clear(hd);
}

static int rb_head_page_set(struct ring_buffer_per_cpu *cpu_buffer,
			    struct buffer_page *head,
			    struct buffer_page *prev,
			    int old_flag, int new_flag)
{
	struct list_head *list;
	unsigned long val = (unsigned long)&head->list;
	unsigned long ret;

	list = &prev->list;

	val &= ~RB_FLAG_MASK;

	ret = cmpxchg((unsigned long *)&list->next,
		      val | old_flag, val | new_flag);

	 
	if ((ret & ~RB_FLAG_MASK) != val)
		return RB_PAGE_MOVED;

	return ret & RB_FLAG_MASK;
}

static int rb_head_page_set_update(struct ring_buffer_per_cpu *cpu_buffer,
				   struct buffer_page *head,
				   struct buffer_page *prev,
				   int old_flag)
{
	return rb_head_page_set(cpu_buffer, head, prev,
				old_flag, RB_PAGE_UPDATE);
}

static int rb_head_page_set_head(struct ring_buffer_per_cpu *cpu_buffer,
				 struct buffer_page *head,
				 struct buffer_page *prev,
				 int old_flag)
{
	return rb_head_page_set(cpu_buffer, head, prev,
				old_flag, RB_PAGE_HEAD);
}

static int rb_head_page_set_normal(struct ring_buffer_per_cpu *cpu_buffer,
				   struct buffer_page *head,
				   struct buffer_page *prev,
				   int old_flag)
{
	return rb_head_page_set(cpu_buffer, head, prev,
				old_flag, RB_PAGE_NORMAL);
}

static inline void rb_inc_page(struct buffer_page **bpage)
{
	struct list_head *p = rb_list_head((*bpage)->list.next);

	*bpage = list_entry(p, struct buffer_page, list);
}

static struct buffer_page *
rb_set_head_page(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct buffer_page *head;
	struct buffer_page *page;
	struct list_head *list;
	int i;

	if (RB_WARN_ON(cpu_buffer, !cpu_buffer->head_page))
		return NULL;

	 
	list = cpu_buffer->pages;
	if (RB_WARN_ON(cpu_buffer, rb_list_head(list->prev->next) != list))
		return NULL;

	page = head = cpu_buffer->head_page;
	 
	for (i = 0; i < 3; i++) {
		do {
			if (rb_is_head_page(page, page->list.prev)) {
				cpu_buffer->head_page = page;
				return page;
			}
			rb_inc_page(&page);
		} while (page != head);
	}

	RB_WARN_ON(cpu_buffer, 1);

	return NULL;
}

static bool rb_head_page_replace(struct buffer_page *old,
				struct buffer_page *new)
{
	unsigned long *ptr = (unsigned long *)&old->list.prev->next;
	unsigned long val;

	val = *ptr & ~RB_FLAG_MASK;
	val |= RB_PAGE_HEAD;

	return try_cmpxchg(ptr, &val, (unsigned long)&new->list);
}

 
static void rb_tail_page_update(struct ring_buffer_per_cpu *cpu_buffer,
			       struct buffer_page *tail_page,
			       struct buffer_page *next_page)
{
	unsigned long old_entries;
	unsigned long old_write;

	 
	old_write = local_add_return(RB_WRITE_INTCNT, &next_page->write);
	old_entries = local_add_return(RB_WRITE_INTCNT, &next_page->entries);

	local_inc(&cpu_buffer->pages_touched);
	 
	barrier();

	 
	if (tail_page == READ_ONCE(cpu_buffer->tail_page)) {
		 
		unsigned long val = old_write & ~RB_WRITE_MASK;
		unsigned long eval = old_entries & ~RB_WRITE_MASK;

		 
		(void)local_cmpxchg(&next_page->write, old_write, val);
		(void)local_cmpxchg(&next_page->entries, old_entries, eval);

		 
		local_set(&next_page->page->commit, 0);

		 
		(void)cmpxchg(&cpu_buffer->tail_page, tail_page, next_page);
	}
}

static void rb_check_bpage(struct ring_buffer_per_cpu *cpu_buffer,
			  struct buffer_page *bpage)
{
	unsigned long val = (unsigned long)bpage;

	RB_WARN_ON(cpu_buffer, val & RB_FLAG_MASK);
}

 
static void rb_check_pages(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct list_head *head = rb_list_head(cpu_buffer->pages);
	struct list_head *tmp;

	if (RB_WARN_ON(cpu_buffer,
			rb_list_head(rb_list_head(head->next)->prev) != head))
		return;

	if (RB_WARN_ON(cpu_buffer,
			rb_list_head(rb_list_head(head->prev)->next) != head))
		return;

	for (tmp = rb_list_head(head->next); tmp != head; tmp = rb_list_head(tmp->next)) {
		if (RB_WARN_ON(cpu_buffer,
				rb_list_head(rb_list_head(tmp->next)->prev) != tmp))
			return;

		if (RB_WARN_ON(cpu_buffer,
				rb_list_head(rb_list_head(tmp->prev)->next) != tmp))
			return;
	}
}

static int __rb_allocate_pages(struct ring_buffer_per_cpu *cpu_buffer,
		long nr_pages, struct list_head *pages)
{
	struct buffer_page *bpage, *tmp;
	bool user_thread = current->mm != NULL;
	gfp_t mflags;
	long i;

	 
	i = si_mem_available();
	if (i < nr_pages)
		return -ENOMEM;

	 
	mflags = GFP_KERNEL | __GFP_RETRY_MAYFAIL;

	 
	if (user_thread)
		set_current_oom_origin();
	for (i = 0; i < nr_pages; i++) {
		struct page *page;

		bpage = kzalloc_node(ALIGN(sizeof(*bpage), cache_line_size()),
				    mflags, cpu_to_node(cpu_buffer->cpu));
		if (!bpage)
			goto free_pages;

		rb_check_bpage(cpu_buffer, bpage);

		list_add(&bpage->list, pages);

		page = alloc_pages_node(cpu_to_node(cpu_buffer->cpu), mflags, 0);
		if (!page)
			goto free_pages;
		bpage->page = page_address(page);
		rb_init_page(bpage->page);

		if (user_thread && fatal_signal_pending(current))
			goto free_pages;
	}
	if (user_thread)
		clear_current_oom_origin();

	return 0;

free_pages:
	list_for_each_entry_safe(bpage, tmp, pages, list) {
		list_del_init(&bpage->list);
		free_buffer_page(bpage);
	}
	if (user_thread)
		clear_current_oom_origin();

	return -ENOMEM;
}

static int rb_allocate_pages(struct ring_buffer_per_cpu *cpu_buffer,
			     unsigned long nr_pages)
{
	LIST_HEAD(pages);

	WARN_ON(!nr_pages);

	if (__rb_allocate_pages(cpu_buffer, nr_pages, &pages))
		return -ENOMEM;

	 
	cpu_buffer->pages = pages.next;
	list_del(&pages);

	cpu_buffer->nr_pages = nr_pages;

	rb_check_pages(cpu_buffer);

	return 0;
}

static struct ring_buffer_per_cpu *
rb_allocate_cpu_buffer(struct trace_buffer *buffer, long nr_pages, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct buffer_page *bpage;
	struct page *page;
	int ret;

	cpu_buffer = kzalloc_node(ALIGN(sizeof(*cpu_buffer), cache_line_size()),
				  GFP_KERNEL, cpu_to_node(cpu));
	if (!cpu_buffer)
		return NULL;

	cpu_buffer->cpu = cpu;
	cpu_buffer->buffer = buffer;
	raw_spin_lock_init(&cpu_buffer->reader_lock);
	lockdep_set_class(&cpu_buffer->reader_lock, buffer->reader_lock_key);
	cpu_buffer->lock = (arch_spinlock_t)__ARCH_SPIN_LOCK_UNLOCKED;
	INIT_WORK(&cpu_buffer->update_pages_work, update_pages_handler);
	init_completion(&cpu_buffer->update_done);
	init_irq_work(&cpu_buffer->irq_work.work, rb_wake_up_waiters);
	init_waitqueue_head(&cpu_buffer->irq_work.waiters);
	init_waitqueue_head(&cpu_buffer->irq_work.full_waiters);

	bpage = kzalloc_node(ALIGN(sizeof(*bpage), cache_line_size()),
			    GFP_KERNEL, cpu_to_node(cpu));
	if (!bpage)
		goto fail_free_buffer;

	rb_check_bpage(cpu_buffer, bpage);

	cpu_buffer->reader_page = bpage;
	page = alloc_pages_node(cpu_to_node(cpu), GFP_KERNEL, 0);
	if (!page)
		goto fail_free_reader;
	bpage->page = page_address(page);
	rb_init_page(bpage->page);

	INIT_LIST_HEAD(&cpu_buffer->reader_page->list);
	INIT_LIST_HEAD(&cpu_buffer->new_pages);

	ret = rb_allocate_pages(cpu_buffer, nr_pages);
	if (ret < 0)
		goto fail_free_reader;

	cpu_buffer->head_page
		= list_entry(cpu_buffer->pages, struct buffer_page, list);
	cpu_buffer->tail_page = cpu_buffer->commit_page = cpu_buffer->head_page;

	rb_head_page_activate(cpu_buffer);

	return cpu_buffer;

 fail_free_reader:
	free_buffer_page(cpu_buffer->reader_page);

 fail_free_buffer:
	kfree(cpu_buffer);
	return NULL;
}

static void rb_free_cpu_buffer(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct list_head *head = cpu_buffer->pages;
	struct buffer_page *bpage, *tmp;

	irq_work_sync(&cpu_buffer->irq_work.work);

	free_buffer_page(cpu_buffer->reader_page);

	if (head) {
		rb_head_page_deactivate(cpu_buffer);

		list_for_each_entry_safe(bpage, tmp, head, list) {
			list_del_init(&bpage->list);
			free_buffer_page(bpage);
		}
		bpage = list_entry(head, struct buffer_page, list);
		free_buffer_page(bpage);
	}

	free_page((unsigned long)cpu_buffer->free_page);

	kfree(cpu_buffer);
}

 
struct trace_buffer *__ring_buffer_alloc(unsigned long size, unsigned flags,
					struct lock_class_key *key)
{
	struct trace_buffer *buffer;
	long nr_pages;
	int bsize;
	int cpu;
	int ret;

	 
	buffer = kzalloc(ALIGN(sizeof(*buffer), cache_line_size()),
			 GFP_KERNEL);
	if (!buffer)
		return NULL;

	if (!zalloc_cpumask_var(&buffer->cpumask, GFP_KERNEL))
		goto fail_free_buffer;

	nr_pages = DIV_ROUND_UP(size, BUF_PAGE_SIZE);
	buffer->flags = flags;
	buffer->clock = trace_clock_local;
	buffer->reader_lock_key = key;

	init_irq_work(&buffer->irq_work.work, rb_wake_up_waiters);
	init_waitqueue_head(&buffer->irq_work.waiters);

	 
	if (nr_pages < 2)
		nr_pages = 2;

	buffer->cpus = nr_cpu_ids;

	bsize = sizeof(void *) * nr_cpu_ids;
	buffer->buffers = kzalloc(ALIGN(bsize, cache_line_size()),
				  GFP_KERNEL);
	if (!buffer->buffers)
		goto fail_free_cpumask;

	cpu = raw_smp_processor_id();
	cpumask_set_cpu(cpu, buffer->cpumask);
	buffer->buffers[cpu] = rb_allocate_cpu_buffer(buffer, nr_pages, cpu);
	if (!buffer->buffers[cpu])
		goto fail_free_buffers;

	ret = cpuhp_state_add_instance(CPUHP_TRACE_RB_PREPARE, &buffer->node);
	if (ret < 0)
		goto fail_free_buffers;

	mutex_init(&buffer->mutex);

	return buffer;

 fail_free_buffers:
	for_each_buffer_cpu(buffer, cpu) {
		if (buffer->buffers[cpu])
			rb_free_cpu_buffer(buffer->buffers[cpu]);
	}
	kfree(buffer->buffers);

 fail_free_cpumask:
	free_cpumask_var(buffer->cpumask);

 fail_free_buffer:
	kfree(buffer);
	return NULL;
}
EXPORT_SYMBOL_GPL(__ring_buffer_alloc);

 
void
ring_buffer_free(struct trace_buffer *buffer)
{
	int cpu;

	cpuhp_state_remove_instance(CPUHP_TRACE_RB_PREPARE, &buffer->node);

	irq_work_sync(&buffer->irq_work.work);

	for_each_buffer_cpu(buffer, cpu)
		rb_free_cpu_buffer(buffer->buffers[cpu]);

	kfree(buffer->buffers);
	free_cpumask_var(buffer->cpumask);

	kfree(buffer);
}
EXPORT_SYMBOL_GPL(ring_buffer_free);

void ring_buffer_set_clock(struct trace_buffer *buffer,
			   u64 (*clock)(void))
{
	buffer->clock = clock;
}

void ring_buffer_set_time_stamp_abs(struct trace_buffer *buffer, bool abs)
{
	buffer->time_stamp_abs = abs;
}

bool ring_buffer_time_stamp_abs(struct trace_buffer *buffer)
{
	return buffer->time_stamp_abs;
}

static void rb_reset_cpu(struct ring_buffer_per_cpu *cpu_buffer);

static inline unsigned long rb_page_entries(struct buffer_page *bpage)
{
	return local_read(&bpage->entries) & RB_WRITE_MASK;
}

static inline unsigned long rb_page_write(struct buffer_page *bpage)
{
	return local_read(&bpage->write) & RB_WRITE_MASK;
}

static bool
rb_remove_pages(struct ring_buffer_per_cpu *cpu_buffer, unsigned long nr_pages)
{
	struct list_head *tail_page, *to_remove, *next_page;
	struct buffer_page *to_remove_page, *tmp_iter_page;
	struct buffer_page *last_page, *first_page;
	unsigned long nr_removed;
	unsigned long head_bit;
	int page_entries;

	head_bit = 0;

	raw_spin_lock_irq(&cpu_buffer->reader_lock);
	atomic_inc(&cpu_buffer->record_disabled);
	 
	tail_page = &cpu_buffer->tail_page->list;

	 
	if (cpu_buffer->tail_page == cpu_buffer->reader_page)
		tail_page = rb_list_head(tail_page->next);
	to_remove = tail_page;

	 
	first_page = list_entry(rb_list_head(to_remove->next),
				struct buffer_page, list);

	for (nr_removed = 0; nr_removed < nr_pages; nr_removed++) {
		to_remove = rb_list_head(to_remove)->next;
		head_bit |= (unsigned long)to_remove & RB_PAGE_HEAD;
	}
	 
	cpu_buffer->pages_removed += nr_removed;

	next_page = rb_list_head(to_remove)->next;

	 
	tail_page->next = (struct list_head *)((unsigned long)next_page |
						head_bit);
	next_page = rb_list_head(next_page);
	next_page->prev = tail_page;

	 
	cpu_buffer->pages = next_page;

	 
	if (head_bit)
		cpu_buffer->head_page = list_entry(next_page,
						struct buffer_page, list);

	 
	atomic_dec(&cpu_buffer->record_disabled);
	raw_spin_unlock_irq(&cpu_buffer->reader_lock);

	RB_WARN_ON(cpu_buffer, list_empty(cpu_buffer->pages));

	 
	last_page = list_entry(rb_list_head(to_remove), struct buffer_page,
				list);
	tmp_iter_page = first_page;

	do {
		cond_resched();

		to_remove_page = tmp_iter_page;
		rb_inc_page(&tmp_iter_page);

		 
		page_entries = rb_page_entries(to_remove_page);
		if (page_entries) {
			 
			local_add(page_entries, &cpu_buffer->overrun);
			local_sub(rb_page_commit(to_remove_page), &cpu_buffer->entries_bytes);
			local_inc(&cpu_buffer->pages_lost);
		}

		 
		free_buffer_page(to_remove_page);
		nr_removed--;

	} while (to_remove_page != last_page);

	RB_WARN_ON(cpu_buffer, nr_removed);

	return nr_removed == 0;
}

static bool
rb_insert_pages(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct list_head *pages = &cpu_buffer->new_pages;
	unsigned long flags;
	bool success;
	int retries;

	 
	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
	 
	retries = 10;
	success = false;
	while (retries--) {
		struct list_head *head_page, *prev_page, *r;
		struct list_head *last_page, *first_page;
		struct list_head *head_page_with_bit;
		struct buffer_page *hpage = rb_set_head_page(cpu_buffer);

		if (!hpage)
			break;
		head_page = &hpage->list;
		prev_page = head_page->prev;

		first_page = pages->next;
		last_page  = pages->prev;

		head_page_with_bit = (struct list_head *)
				     ((unsigned long)head_page | RB_PAGE_HEAD);

		last_page->next = head_page_with_bit;
		first_page->prev = prev_page;

		r = cmpxchg(&prev_page->next, head_page_with_bit, first_page);

		if (r == head_page_with_bit) {
			 
			head_page->prev = last_page;
			success = true;
			break;
		}
	}

	if (success)
		INIT_LIST_HEAD(pages);
	 
	RB_WARN_ON(cpu_buffer, !success);
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);

	 
	if (!success) {
		struct buffer_page *bpage, *tmp;
		list_for_each_entry_safe(bpage, tmp, &cpu_buffer->new_pages,
					 list) {
			list_del_init(&bpage->list);
			free_buffer_page(bpage);
		}
	}
	return success;
}

static void rb_update_pages(struct ring_buffer_per_cpu *cpu_buffer)
{
	bool success;

	if (cpu_buffer->nr_pages_to_update > 0)
		success = rb_insert_pages(cpu_buffer);
	else
		success = rb_remove_pages(cpu_buffer,
					-cpu_buffer->nr_pages_to_update);

	if (success)
		cpu_buffer->nr_pages += cpu_buffer->nr_pages_to_update;
}

static void update_pages_handler(struct work_struct *work)
{
	struct ring_buffer_per_cpu *cpu_buffer = container_of(work,
			struct ring_buffer_per_cpu, update_pages_work);
	rb_update_pages(cpu_buffer);
	complete(&cpu_buffer->update_done);
}

 
int ring_buffer_resize(struct trace_buffer *buffer, unsigned long size,
			int cpu_id)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long nr_pages;
	int cpu, err;

	 
	if (!buffer)
		return 0;

	 
	if (cpu_id != RING_BUFFER_ALL_CPUS &&
	    !cpumask_test_cpu(cpu_id, buffer->cpumask))
		return 0;

	nr_pages = DIV_ROUND_UP(size, BUF_PAGE_SIZE);

	 
	if (nr_pages < 2)
		nr_pages = 2;

	 
	mutex_lock(&buffer->mutex);
	atomic_inc(&buffer->resizing);

	if (cpu_id == RING_BUFFER_ALL_CPUS) {
		 
		for_each_buffer_cpu(buffer, cpu) {
			cpu_buffer = buffer->buffers[cpu];
			if (atomic_read(&cpu_buffer->resize_disabled)) {
				err = -EBUSY;
				goto out_err_unlock;
			}
		}

		 
		for_each_buffer_cpu(buffer, cpu) {
			cpu_buffer = buffer->buffers[cpu];

			cpu_buffer->nr_pages_to_update = nr_pages -
							cpu_buffer->nr_pages;
			 
			if (cpu_buffer->nr_pages_to_update <= 0)
				continue;
			 
			INIT_LIST_HEAD(&cpu_buffer->new_pages);
			if (__rb_allocate_pages(cpu_buffer, cpu_buffer->nr_pages_to_update,
						&cpu_buffer->new_pages)) {
				 
				err = -ENOMEM;
				goto out_err;
			}

			cond_resched();
		}

		cpus_read_lock();
		 
		for_each_buffer_cpu(buffer, cpu) {
			cpu_buffer = buffer->buffers[cpu];
			if (!cpu_buffer->nr_pages_to_update)
				continue;

			 
			if (!cpu_online(cpu)) {
				rb_update_pages(cpu_buffer);
				cpu_buffer->nr_pages_to_update = 0;
			} else {
				 
				migrate_disable();
				if (cpu != smp_processor_id()) {
					migrate_enable();
					schedule_work_on(cpu,
							 &cpu_buffer->update_pages_work);
				} else {
					update_pages_handler(&cpu_buffer->update_pages_work);
					migrate_enable();
				}
			}
		}

		 
		for_each_buffer_cpu(buffer, cpu) {
			cpu_buffer = buffer->buffers[cpu];
			if (!cpu_buffer->nr_pages_to_update)
				continue;

			if (cpu_online(cpu))
				wait_for_completion(&cpu_buffer->update_done);
			cpu_buffer->nr_pages_to_update = 0;
		}

		cpus_read_unlock();
	} else {
		cpu_buffer = buffer->buffers[cpu_id];

		if (nr_pages == cpu_buffer->nr_pages)
			goto out;

		 
		if (atomic_read(&cpu_buffer->resize_disabled)) {
			err = -EBUSY;
			goto out_err_unlock;
		}

		cpu_buffer->nr_pages_to_update = nr_pages -
						cpu_buffer->nr_pages;

		INIT_LIST_HEAD(&cpu_buffer->new_pages);
		if (cpu_buffer->nr_pages_to_update > 0 &&
			__rb_allocate_pages(cpu_buffer, cpu_buffer->nr_pages_to_update,
					    &cpu_buffer->new_pages)) {
			err = -ENOMEM;
			goto out_err;
		}

		cpus_read_lock();

		 
		if (!cpu_online(cpu_id))
			rb_update_pages(cpu_buffer);
		else {
			 
			migrate_disable();
			if (cpu_id == smp_processor_id()) {
				rb_update_pages(cpu_buffer);
				migrate_enable();
			} else {
				migrate_enable();
				schedule_work_on(cpu_id,
						 &cpu_buffer->update_pages_work);
				wait_for_completion(&cpu_buffer->update_done);
			}
		}

		cpu_buffer->nr_pages_to_update = 0;
		cpus_read_unlock();
	}

 out:
	 
	if (atomic_read(&buffer->record_disabled)) {
		atomic_inc(&buffer->record_disabled);
		 
		synchronize_rcu();
		for_each_buffer_cpu(buffer, cpu) {
			cpu_buffer = buffer->buffers[cpu];
			rb_check_pages(cpu_buffer);
		}
		atomic_dec(&buffer->record_disabled);
	}

	atomic_dec(&buffer->resizing);
	mutex_unlock(&buffer->mutex);
	return 0;

 out_err:
	for_each_buffer_cpu(buffer, cpu) {
		struct buffer_page *bpage, *tmp;

		cpu_buffer = buffer->buffers[cpu];
		cpu_buffer->nr_pages_to_update = 0;

		if (list_empty(&cpu_buffer->new_pages))
			continue;

		list_for_each_entry_safe(bpage, tmp, &cpu_buffer->new_pages,
					list) {
			list_del_init(&bpage->list);
			free_buffer_page(bpage);
		}
	}
 out_err_unlock:
	atomic_dec(&buffer->resizing);
	mutex_unlock(&buffer->mutex);
	return err;
}
EXPORT_SYMBOL_GPL(ring_buffer_resize);

void ring_buffer_change_overwrite(struct trace_buffer *buffer, int val)
{
	mutex_lock(&buffer->mutex);
	if (val)
		buffer->flags |= RB_FL_OVERWRITE;
	else
		buffer->flags &= ~RB_FL_OVERWRITE;
	mutex_unlock(&buffer->mutex);
}
EXPORT_SYMBOL_GPL(ring_buffer_change_overwrite);

static __always_inline void *__rb_page_index(struct buffer_page *bpage, unsigned index)
{
	return bpage->page->data + index;
}

static __always_inline struct ring_buffer_event *
rb_reader_event(struct ring_buffer_per_cpu *cpu_buffer)
{
	return __rb_page_index(cpu_buffer->reader_page,
			       cpu_buffer->reader_page->read);
}

static struct ring_buffer_event *
rb_iter_head_event(struct ring_buffer_iter *iter)
{
	struct ring_buffer_event *event;
	struct buffer_page *iter_head_page = iter->head_page;
	unsigned long commit;
	unsigned length;

	if (iter->head != iter->next_event)
		return iter->event;

	 
	commit = rb_page_commit(iter_head_page);
	smp_rmb();

	 
	if (iter->head > commit - 8)
		goto reset;

	event = __rb_page_index(iter_head_page, iter->head);
	length = rb_event_length(event);

	 
	barrier();

	if ((iter->head + length) > commit || length > BUF_PAGE_SIZE)
		 
		goto reset;

	memcpy(iter->event, event, length);
	 
	smp_rmb();

	 
	if (iter->page_stamp != iter_head_page->page->time_stamp ||
	    commit > rb_page_commit(iter_head_page))
		goto reset;

	iter->next_event = iter->head + length;
	return iter->event;
 reset:
	 
	iter->page_stamp = iter->read_stamp = iter->head_page->page->time_stamp;
	iter->head = 0;
	iter->next_event = 0;
	iter->missed_events = 1;
	return NULL;
}

 
static __always_inline unsigned rb_page_size(struct buffer_page *bpage)
{
	return rb_page_commit(bpage);
}

static __always_inline unsigned
rb_commit_index(struct ring_buffer_per_cpu *cpu_buffer)
{
	return rb_page_commit(cpu_buffer->commit_page);
}

static __always_inline unsigned
rb_event_index(struct ring_buffer_event *event)
{
	unsigned long addr = (unsigned long)event;

	return (addr & ~PAGE_MASK) - BUF_PAGE_HDR_SIZE;
}

static void rb_inc_iter(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer = iter->cpu_buffer;

	 
	if (iter->head_page == cpu_buffer->reader_page)
		iter->head_page = rb_set_head_page(cpu_buffer);
	else
		rb_inc_page(&iter->head_page);

	iter->page_stamp = iter->read_stamp = iter->head_page->page->time_stamp;
	iter->head = 0;
	iter->next_event = 0;
}

 
static int
rb_handle_head_page(struct ring_buffer_per_cpu *cpu_buffer,
		    struct buffer_page *tail_page,
		    struct buffer_page *next_page)
{
	struct buffer_page *new_head;
	int entries;
	int type;
	int ret;

	entries = rb_page_entries(next_page);

	 
	type = rb_head_page_set_update(cpu_buffer, next_page, tail_page,
				       RB_PAGE_HEAD);

	 

	switch (type) {
	case RB_PAGE_HEAD:
		 
		local_add(entries, &cpu_buffer->overrun);
		local_sub(rb_page_commit(next_page), &cpu_buffer->entries_bytes);
		local_inc(&cpu_buffer->pages_lost);

		 

		 
		break;

	case RB_PAGE_UPDATE:
		 
		break;
	case RB_PAGE_NORMAL:
		 
		return 1;
	case RB_PAGE_MOVED:
		 
		return 1;
	default:
		RB_WARN_ON(cpu_buffer, 1);  
		return -1;
	}

	 
	new_head = next_page;
	rb_inc_page(&new_head);

	ret = rb_head_page_set_head(cpu_buffer, new_head, next_page,
				    RB_PAGE_NORMAL);

	 
	switch (ret) {
	case RB_PAGE_HEAD:
	case RB_PAGE_NORMAL:
		 
		break;
	default:
		RB_WARN_ON(cpu_buffer, 1);
		return -1;
	}

	 
	if (ret == RB_PAGE_NORMAL) {
		struct buffer_page *buffer_tail_page;

		buffer_tail_page = READ_ONCE(cpu_buffer->tail_page);
		 
		if (buffer_tail_page != tail_page &&
		    buffer_tail_page != next_page)
			rb_head_page_set_normal(cpu_buffer, new_head,
						next_page,
						RB_PAGE_HEAD);
	}

	 
	if (type == RB_PAGE_HEAD) {
		ret = rb_head_page_set_normal(cpu_buffer, next_page,
					      tail_page,
					      RB_PAGE_UPDATE);
		if (RB_WARN_ON(cpu_buffer,
			       ret != RB_PAGE_UPDATE))
			return -1;
	}

	return 0;
}

static inline void
rb_reset_tail(struct ring_buffer_per_cpu *cpu_buffer,
	      unsigned long tail, struct rb_event_info *info)
{
	struct buffer_page *tail_page = info->tail_page;
	struct ring_buffer_event *event;
	unsigned long length = info->length;

	 
	if (tail >= BUF_PAGE_SIZE) {
		 
		if (tail == BUF_PAGE_SIZE)
			tail_page->real_end = 0;

		local_sub(length, &tail_page->write);
		return;
	}

	event = __rb_page_index(tail_page, tail);

	 
	tail_page->real_end = tail;

	 
	if (tail > (BUF_PAGE_SIZE - RB_EVNT_MIN_SIZE)) {
		 

		 
		rb_event_set_padding(event);

		 
		smp_wmb();

		 
		local_sub(length, &tail_page->write);
		return;
	}

	 
	event->array[0] = (BUF_PAGE_SIZE - tail) - RB_EVNT_HDR_SIZE;
	event->type_len = RINGBUF_TYPE_PADDING;
	 
	event->time_delta = 1;

	 
	local_add(BUF_PAGE_SIZE - tail, &cpu_buffer->entries_bytes);

	 
	smp_wmb();

	 
	length = (tail + length) - BUF_PAGE_SIZE;
	local_sub(length, &tail_page->write);
}

static inline void rb_end_commit(struct ring_buffer_per_cpu *cpu_buffer);

 
static noinline struct ring_buffer_event *
rb_move_tail(struct ring_buffer_per_cpu *cpu_buffer,
	     unsigned long tail, struct rb_event_info *info)
{
	struct buffer_page *tail_page = info->tail_page;
	struct buffer_page *commit_page = cpu_buffer->commit_page;
	struct trace_buffer *buffer = cpu_buffer->buffer;
	struct buffer_page *next_page;
	int ret;

	next_page = tail_page;

	rb_inc_page(&next_page);

	 
	if (unlikely(next_page == commit_page)) {
		local_inc(&cpu_buffer->commit_overrun);
		goto out_reset;
	}

	 
	if (rb_is_head_page(next_page, &tail_page->list)) {

		 
		if (!rb_is_reader_page(cpu_buffer->commit_page)) {
			 
			if (!(buffer->flags & RB_FL_OVERWRITE)) {
				local_inc(&cpu_buffer->dropped_events);
				goto out_reset;
			}

			ret = rb_handle_head_page(cpu_buffer,
						  tail_page,
						  next_page);
			if (ret < 0)
				goto out_reset;
			if (ret)
				goto out_again;
		} else {
			 
			if (unlikely((cpu_buffer->commit_page !=
				      cpu_buffer->tail_page) &&
				     (cpu_buffer->commit_page ==
				      cpu_buffer->reader_page))) {
				local_inc(&cpu_buffer->commit_overrun);
				goto out_reset;
			}
		}
	}

	rb_tail_page_update(cpu_buffer, tail_page, next_page);

 out_again:

	rb_reset_tail(cpu_buffer, tail, info);

	 
	rb_end_commit(cpu_buffer);
	 
	local_inc(&cpu_buffer->committing);

	 
	return ERR_PTR(-EAGAIN);

 out_reset:
	 
	rb_reset_tail(cpu_buffer, tail, info);

	return NULL;
}

 
static struct ring_buffer_event *
rb_add_time_stamp(struct ring_buffer_event *event, u64 delta, bool abs)
{
	if (abs)
		event->type_len = RINGBUF_TYPE_TIME_STAMP;
	else
		event->type_len = RINGBUF_TYPE_TIME_EXTEND;

	 
	if (abs || rb_event_index(event)) {
		event->time_delta = delta & TS_MASK;
		event->array[0] = delta >> TS_SHIFT;
	} else {
		 
		event->time_delta = 0;
		event->array[0] = 0;
	}

	return skip_time_extend(event);
}

#ifndef CONFIG_HAVE_UNSTABLE_SCHED_CLOCK
static inline bool sched_clock_stable(void)
{
	return true;
}
#endif

static void
rb_check_timestamp(struct ring_buffer_per_cpu *cpu_buffer,
		   struct rb_event_info *info)
{
	u64 write_stamp;

	WARN_ONCE(1, "Delta way too big! %llu ts=%llu before=%llu after=%llu write stamp=%llu\n%s",
		  (unsigned long long)info->delta,
		  (unsigned long long)info->ts,
		  (unsigned long long)info->before,
		  (unsigned long long)info->after,
		  (unsigned long long)(rb_time_read(&cpu_buffer->write_stamp, &write_stamp) ? write_stamp : 0),
		  sched_clock_stable() ? "" :
		  "If you just came from a suspend/resume,\n"
		  "please switch to the trace global clock:\n"
		  "  echo global > /sys/kernel/tracing/trace_clock\n"
		  "or add trace_clock=global to the kernel command line\n");
}

static void rb_add_timestamp(struct ring_buffer_per_cpu *cpu_buffer,
				      struct ring_buffer_event **event,
				      struct rb_event_info *info,
				      u64 *delta,
				      unsigned int *length)
{
	bool abs = info->add_timestamp &
		(RB_ADD_STAMP_FORCE | RB_ADD_STAMP_ABSOLUTE);

	if (unlikely(info->delta > (1ULL << 59))) {
		 
		if (abs && (info->ts & TS_MSB)) {
			info->delta &= ABS_TS_MASK;

		 
		} else if (info->before == info->after && info->before > info->ts) {
			 
			static int once;

			 
			if (!once) {
				once++;
				pr_warn("Ring buffer clock went backwards: %llu -> %llu\n",
					info->before, info->ts);
			}
		} else
			rb_check_timestamp(cpu_buffer, info);
		if (!abs)
			info->delta = 0;
	}
	*event = rb_add_time_stamp(*event, info->delta, abs);
	*length -= RB_LEN_TIME_EXTEND;
	*delta = 0;
}

 
static void
rb_update_event(struct ring_buffer_per_cpu *cpu_buffer,
		struct ring_buffer_event *event,
		struct rb_event_info *info)
{
	unsigned length = info->length;
	u64 delta = info->delta;
	unsigned int nest = local_read(&cpu_buffer->committing) - 1;

	if (!WARN_ON_ONCE(nest >= MAX_NEST))
		cpu_buffer->event_stamp[nest] = info->ts;

	 
	if (unlikely(info->add_timestamp))
		rb_add_timestamp(cpu_buffer, &event, info, &delta, &length);

	event->time_delta = delta;
	length -= RB_EVNT_HDR_SIZE;
	if (length > RB_MAX_SMALL_DATA || RB_FORCE_8BYTE_ALIGNMENT) {
		event->type_len = 0;
		event->array[0] = length;
	} else
		event->type_len = DIV_ROUND_UP(length, RB_ALIGNMENT);
}

static unsigned rb_calculate_event_length(unsigned length)
{
	struct ring_buffer_event event;  

	 
	if (!length)
		length++;

	if (length > RB_MAX_SMALL_DATA || RB_FORCE_8BYTE_ALIGNMENT)
		length += sizeof(event.array[0]);

	length += RB_EVNT_HDR_SIZE;
	length = ALIGN(length, RB_ARCH_ALIGNMENT);

	 
	if (length == RB_LEN_TIME_EXTEND + RB_ALIGNMENT)
		length += RB_ALIGNMENT;

	return length;
}

static inline bool
rb_try_to_discard(struct ring_buffer_per_cpu *cpu_buffer,
		  struct ring_buffer_event *event)
{
	unsigned long new_index, old_index;
	struct buffer_page *bpage;
	unsigned long addr;

	new_index = rb_event_index(event);
	old_index = new_index + rb_event_ts_length(event);
	addr = (unsigned long)event;
	addr &= PAGE_MASK;

	bpage = READ_ONCE(cpu_buffer->tail_page);

	 
	if (bpage->page == (void *)addr && rb_page_write(bpage) == old_index) {
		unsigned long write_mask =
			local_read(&bpage->write) & ~RB_WRITE_MASK;
		unsigned long event_length = rb_event_length(event);

		 
		rb_time_set(&cpu_buffer->before_stamp, 0);

		 

		 
		old_index += write_mask;
		new_index += write_mask;

		 
		if (local_try_cmpxchg(&bpage->write, &old_index, new_index)) {
			 
			local_sub(event_length, &cpu_buffer->entries_bytes);
			return true;
		}
	}

	 
	return false;
}

static void rb_start_commit(struct ring_buffer_per_cpu *cpu_buffer)
{
	local_inc(&cpu_buffer->committing);
	local_inc(&cpu_buffer->commits);
}

static __always_inline void
rb_set_commit_to_write(struct ring_buffer_per_cpu *cpu_buffer)
{
	unsigned long max_count;

	 
 again:
	max_count = cpu_buffer->nr_pages * 100;

	while (cpu_buffer->commit_page != READ_ONCE(cpu_buffer->tail_page)) {
		if (RB_WARN_ON(cpu_buffer, !(--max_count)))
			return;
		if (RB_WARN_ON(cpu_buffer,
			       rb_is_reader_page(cpu_buffer->tail_page)))
			return;
		 
		local_set(&cpu_buffer->commit_page->page->commit,
			  rb_page_write(cpu_buffer->commit_page));
		rb_inc_page(&cpu_buffer->commit_page);
		 
		barrier();
	}
	while (rb_commit_index(cpu_buffer) !=
	       rb_page_write(cpu_buffer->commit_page)) {

		 
		smp_wmb();
		local_set(&cpu_buffer->commit_page->page->commit,
			  rb_page_write(cpu_buffer->commit_page));
		RB_WARN_ON(cpu_buffer,
			   local_read(&cpu_buffer->commit_page->page->commit) &
			   ~RB_WRITE_MASK);
		barrier();
	}

	 
	barrier();

	 
	if (unlikely(cpu_buffer->commit_page != READ_ONCE(cpu_buffer->tail_page)))
		goto again;
}

static __always_inline void rb_end_commit(struct ring_buffer_per_cpu *cpu_buffer)
{
	unsigned long commits;

	if (RB_WARN_ON(cpu_buffer,
		       !local_read(&cpu_buffer->committing)))
		return;

 again:
	commits = local_read(&cpu_buffer->commits);
	 
	barrier();
	if (local_read(&cpu_buffer->committing) == 1)
		rb_set_commit_to_write(cpu_buffer);

	local_dec(&cpu_buffer->committing);

	 
	barrier();

	 
	if (unlikely(local_read(&cpu_buffer->commits) != commits) &&
	    !local_read(&cpu_buffer->committing)) {
		local_inc(&cpu_buffer->committing);
		goto again;
	}
}

static inline void rb_event_discard(struct ring_buffer_event *event)
{
	if (extended_time(event))
		event = skip_time_extend(event);

	 
	event->array[0] = rb_event_data_length(event) - RB_EVNT_HDR_SIZE;
	event->type_len = RINGBUF_TYPE_PADDING;
	 
	if (!event->time_delta)
		event->time_delta = 1;
}

static void rb_commit(struct ring_buffer_per_cpu *cpu_buffer)
{
	local_inc(&cpu_buffer->entries);
	rb_end_commit(cpu_buffer);
}

static __always_inline void
rb_wakeups(struct trace_buffer *buffer, struct ring_buffer_per_cpu *cpu_buffer)
{
	if (buffer->irq_work.waiters_pending) {
		buffer->irq_work.waiters_pending = false;
		 
		irq_work_queue(&buffer->irq_work.work);
	}

	if (cpu_buffer->irq_work.waiters_pending) {
		cpu_buffer->irq_work.waiters_pending = false;
		 
		irq_work_queue(&cpu_buffer->irq_work.work);
	}

	if (cpu_buffer->last_pages_touch == local_read(&cpu_buffer->pages_touched))
		return;

	if (cpu_buffer->reader_page == cpu_buffer->commit_page)
		return;

	if (!cpu_buffer->irq_work.full_waiters_pending)
		return;

	cpu_buffer->last_pages_touch = local_read(&cpu_buffer->pages_touched);

	if (!full_hit(buffer, cpu_buffer->cpu, cpu_buffer->shortest_full))
		return;

	cpu_buffer->irq_work.wakeup_full = true;
	cpu_buffer->irq_work.full_waiters_pending = false;
	 
	irq_work_queue(&cpu_buffer->irq_work.work);
}

#ifdef CONFIG_RING_BUFFER_RECORD_RECURSION
# define do_ring_buffer_record_recursion()	\
	do_ftrace_record_recursion(_THIS_IP_, _RET_IP_)
#else
# define do_ring_buffer_record_recursion() do { } while (0)
#endif

 

static __always_inline bool
trace_recursive_lock(struct ring_buffer_per_cpu *cpu_buffer)
{
	unsigned int val = cpu_buffer->current_context;
	int bit = interrupt_context_level();

	bit = RB_CTX_NORMAL - bit;

	if (unlikely(val & (1 << (bit + cpu_buffer->nest)))) {
		 
		bit = RB_CTX_TRANSITION;
		if (val & (1 << (bit + cpu_buffer->nest))) {
			do_ring_buffer_record_recursion();
			return true;
		}
	}

	val |= (1 << (bit + cpu_buffer->nest));
	cpu_buffer->current_context = val;

	return false;
}

static __always_inline void
trace_recursive_unlock(struct ring_buffer_per_cpu *cpu_buffer)
{
	cpu_buffer->current_context &=
		cpu_buffer->current_context - (1 << cpu_buffer->nest);
}

 
#define NESTED_BITS 5

 
void ring_buffer_nest_start(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	int cpu;

	 
	preempt_disable_notrace();
	cpu = raw_smp_processor_id();
	cpu_buffer = buffer->buffers[cpu];
	 
	cpu_buffer->nest += NESTED_BITS;
}

 
void ring_buffer_nest_end(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	int cpu;

	 
	cpu = raw_smp_processor_id();
	cpu_buffer = buffer->buffers[cpu];
	 
	cpu_buffer->nest -= NESTED_BITS;
	preempt_enable_notrace();
}

 
int ring_buffer_unlock_commit(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	int cpu = raw_smp_processor_id();

	cpu_buffer = buffer->buffers[cpu];

	rb_commit(cpu_buffer);

	rb_wakeups(buffer, cpu_buffer);

	trace_recursive_unlock(cpu_buffer);

	preempt_enable_notrace();

	return 0;
}
EXPORT_SYMBOL_GPL(ring_buffer_unlock_commit);

 
#define CHECK_FULL_PAGE		1L

#ifdef CONFIG_RING_BUFFER_VALIDATE_TIME_DELTAS
static void dump_buffer_page(struct buffer_data_page *bpage,
			     struct rb_event_info *info,
			     unsigned long tail)
{
	struct ring_buffer_event *event;
	u64 ts, delta;
	int e;

	ts = bpage->time_stamp;
	pr_warn("  [%lld] PAGE TIME STAMP\n", ts);

	for (e = 0; e < tail; e += rb_event_length(event)) {

		event = (struct ring_buffer_event *)(bpage->data + e);

		switch (event->type_len) {

		case RINGBUF_TYPE_TIME_EXTEND:
			delta = rb_event_time_stamp(event);
			ts += delta;
			pr_warn("  [%lld] delta:%lld TIME EXTEND\n", ts, delta);
			break;

		case RINGBUF_TYPE_TIME_STAMP:
			delta = rb_event_time_stamp(event);
			ts = rb_fix_abs_ts(delta, ts);
			pr_warn("  [%lld] absolute:%lld TIME STAMP\n", ts, delta);
			break;

		case RINGBUF_TYPE_PADDING:
			ts += event->time_delta;
			pr_warn("  [%lld] delta:%d PADDING\n", ts, event->time_delta);
			break;

		case RINGBUF_TYPE_DATA:
			ts += event->time_delta;
			pr_warn("  [%lld] delta:%d\n", ts, event->time_delta);
			break;

		default:
			break;
		}
	}
}

static DEFINE_PER_CPU(atomic_t, checking);
static atomic_t ts_dump;

 
static void check_buffer(struct ring_buffer_per_cpu *cpu_buffer,
			 struct rb_event_info *info,
			 unsigned long tail)
{
	struct ring_buffer_event *event;
	struct buffer_data_page *bpage;
	u64 ts, delta;
	bool full = false;
	int e;

	bpage = info->tail_page->page;

	if (tail == CHECK_FULL_PAGE) {
		full = true;
		tail = local_read(&bpage->commit);
	} else if (info->add_timestamp &
		   (RB_ADD_STAMP_FORCE | RB_ADD_STAMP_ABSOLUTE)) {
		 
		return;
	}

	 
	if (tail <= 8 || tail > local_read(&bpage->commit))
		return;

	 
	if (atomic_inc_return(this_cpu_ptr(&checking)) != 1)
		goto out;

	ts = bpage->time_stamp;

	for (e = 0; e < tail; e += rb_event_length(event)) {

		event = (struct ring_buffer_event *)(bpage->data + e);

		switch (event->type_len) {

		case RINGBUF_TYPE_TIME_EXTEND:
			delta = rb_event_time_stamp(event);
			ts += delta;
			break;

		case RINGBUF_TYPE_TIME_STAMP:
			delta = rb_event_time_stamp(event);
			ts = rb_fix_abs_ts(delta, ts);
			break;

		case RINGBUF_TYPE_PADDING:
			if (event->time_delta == 1)
				break;
			fallthrough;
		case RINGBUF_TYPE_DATA:
			ts += event->time_delta;
			break;

		default:
			RB_WARN_ON(cpu_buffer, 1);
		}
	}
	if ((full && ts > info->ts) ||
	    (!full && ts + info->delta != info->ts)) {
		 
		if (atomic_inc_return(&ts_dump) != 1) {
			atomic_dec(&ts_dump);
			goto out;
		}
		atomic_inc(&cpu_buffer->record_disabled);
		 
		WARN_ON_ONCE(system_state != SYSTEM_BOOTING);
		pr_warn("[CPU: %d]TIME DOES NOT MATCH expected:%lld actual:%lld delta:%lld before:%lld after:%lld%s\n",
			cpu_buffer->cpu,
			ts + info->delta, info->ts, info->delta,
			info->before, info->after,
			full ? " (full)" : "");
		dump_buffer_page(bpage, info, tail);
		atomic_dec(&ts_dump);
		 
		return;
	}
out:
	atomic_dec(this_cpu_ptr(&checking));
}
#else
static inline void check_buffer(struct ring_buffer_per_cpu *cpu_buffer,
			 struct rb_event_info *info,
			 unsigned long tail)
{
}
#endif  

static struct ring_buffer_event *
__rb_reserve_next(struct ring_buffer_per_cpu *cpu_buffer,
		  struct rb_event_info *info)
{
	struct ring_buffer_event *event;
	struct buffer_page *tail_page;
	unsigned long tail, write, w;
	bool a_ok;
	bool b_ok;

	 
	tail_page = info->tail_page = READ_ONCE(cpu_buffer->tail_page);

  	w = local_read(&tail_page->write) & RB_WRITE_MASK;
	barrier();
	b_ok = rb_time_read(&cpu_buffer->before_stamp, &info->before);
	a_ok = rb_time_read(&cpu_buffer->write_stamp, &info->after);
	barrier();
	info->ts = rb_time_stamp(cpu_buffer->buffer);

	if ((info->add_timestamp & RB_ADD_STAMP_ABSOLUTE)) {
		info->delta = info->ts;
	} else {
		 
		if (!w) {
			 
			info->delta = 0;
		} else if (unlikely(!a_ok || !b_ok || info->before != info->after)) {
			info->add_timestamp |= RB_ADD_STAMP_FORCE | RB_ADD_STAMP_EXTEND;
			info->length += RB_LEN_TIME_EXTEND;
		} else {
			info->delta = info->ts - info->after;
			if (unlikely(test_time_stamp(info->delta))) {
				info->add_timestamp |= RB_ADD_STAMP_EXTEND;
				info->length += RB_LEN_TIME_EXTEND;
			}
		}
	}

  	rb_time_set(&cpu_buffer->before_stamp, info->ts);

  	write = local_add_return(info->length, &tail_page->write);

	 
	write &= RB_WRITE_MASK;

	tail = write - info->length;

	 
	if (unlikely(write > BUF_PAGE_SIZE)) {
		check_buffer(cpu_buffer, info, CHECK_FULL_PAGE);
		return rb_move_tail(cpu_buffer, tail, info);
	}

	if (likely(tail == w)) {
		 
  		rb_time_set(&cpu_buffer->write_stamp, info->ts);
		 
		if (likely(!(info->add_timestamp &
			     (RB_ADD_STAMP_FORCE | RB_ADD_STAMP_ABSOLUTE))))
			 
			info->delta = info->ts - info->after;
		else
			 
			info->delta = info->ts;
		check_buffer(cpu_buffer, info, tail);
	} else {
		u64 ts;
		 

		 
		a_ok = rb_time_read(&cpu_buffer->before_stamp, &info->before);
		RB_WARN_ON(cpu_buffer, !a_ok);

		 
		ts = rb_time_stamp(cpu_buffer->buffer);
		rb_time_set(&cpu_buffer->before_stamp, ts);

		barrier();
  		a_ok = rb_time_read(&cpu_buffer->write_stamp, &info->after);
		 
		RB_WARN_ON(cpu_buffer, !a_ok);
		barrier();
  		if (write == (local_read(&tail_page->write) & RB_WRITE_MASK) &&
		    info->after == info->before && info->after < ts) {
			 
			info->delta = ts - info->after;
		} else {
			 
			info->delta = 0;
		}
		info->ts = ts;
		info->add_timestamp &= ~RB_ADD_STAMP_FORCE;
	}

	 
	if (unlikely(!tail && !(info->add_timestamp &
				(RB_ADD_STAMP_FORCE | RB_ADD_STAMP_ABSOLUTE))))
		info->delta = 0;

	 

	event = __rb_page_index(tail_page, tail);
	rb_update_event(cpu_buffer, event, info);

	local_inc(&tail_page->entries);

	 
	if (unlikely(!tail))
		tail_page->page->time_stamp = info->ts;

	 
	local_add(info->length, &cpu_buffer->entries_bytes);

	return event;
}

static __always_inline struct ring_buffer_event *
rb_reserve_next_event(struct trace_buffer *buffer,
		      struct ring_buffer_per_cpu *cpu_buffer,
		      unsigned long length)
{
	struct ring_buffer_event *event;
	struct rb_event_info info;
	int nr_loops = 0;
	int add_ts_default;

	 
	if (!IS_ENABLED(CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG) &&
	    (unlikely(in_nmi()))) {
		return NULL;
	}

	rb_start_commit(cpu_buffer);
	 

#ifdef CONFIG_RING_BUFFER_ALLOW_SWAP
	 
	barrier();
	if (unlikely(READ_ONCE(cpu_buffer->buffer) != buffer)) {
		local_dec(&cpu_buffer->committing);
		local_dec(&cpu_buffer->commits);
		return NULL;
	}
#endif

	info.length = rb_calculate_event_length(length);

	if (ring_buffer_time_stamp_abs(cpu_buffer->buffer)) {
		add_ts_default = RB_ADD_STAMP_ABSOLUTE;
		info.length += RB_LEN_TIME_EXTEND;
		if (info.length > BUF_MAX_DATA_SIZE)
			goto out_fail;
	} else {
		add_ts_default = RB_ADD_STAMP_NONE;
	}

 again:
	info.add_timestamp = add_ts_default;
	info.delta = 0;

	 
	if (RB_WARN_ON(cpu_buffer, ++nr_loops > 1000))
		goto out_fail;

	event = __rb_reserve_next(cpu_buffer, &info);

	if (unlikely(PTR_ERR(event) == -EAGAIN)) {
		if (info.add_timestamp & (RB_ADD_STAMP_FORCE | RB_ADD_STAMP_EXTEND))
			info.length -= RB_LEN_TIME_EXTEND;
		goto again;
	}

	if (likely(event))
		return event;
 out_fail:
	rb_end_commit(cpu_buffer);
	return NULL;
}

 
struct ring_buffer_event *
ring_buffer_lock_reserve(struct trace_buffer *buffer, unsigned long length)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct ring_buffer_event *event;
	int cpu;

	 
	preempt_disable_notrace();

	if (unlikely(atomic_read(&buffer->record_disabled)))
		goto out;

	cpu = raw_smp_processor_id();

	if (unlikely(!cpumask_test_cpu(cpu, buffer->cpumask)))
		goto out;

	cpu_buffer = buffer->buffers[cpu];

	if (unlikely(atomic_read(&cpu_buffer->record_disabled)))
		goto out;

	if (unlikely(length > BUF_MAX_DATA_SIZE))
		goto out;

	if (unlikely(trace_recursive_lock(cpu_buffer)))
		goto out;

	event = rb_reserve_next_event(buffer, cpu_buffer, length);
	if (!event)
		goto out_unlock;

	return event;

 out_unlock:
	trace_recursive_unlock(cpu_buffer);
 out:
	preempt_enable_notrace();
	return NULL;
}
EXPORT_SYMBOL_GPL(ring_buffer_lock_reserve);

 
static inline void
rb_decrement_entry(struct ring_buffer_per_cpu *cpu_buffer,
		   struct ring_buffer_event *event)
{
	unsigned long addr = (unsigned long)event;
	struct buffer_page *bpage = cpu_buffer->commit_page;
	struct buffer_page *start;

	addr &= PAGE_MASK;

	 
	if (likely(bpage->page == (void *)addr)) {
		local_dec(&bpage->entries);
		return;
	}

	 
	rb_inc_page(&bpage);
	start = bpage;
	do {
		if (bpage->page == (void *)addr) {
			local_dec(&bpage->entries);
			return;
		}
		rb_inc_page(&bpage);
	} while (bpage != start);

	 
	RB_WARN_ON(cpu_buffer, 1);
}

 
void ring_buffer_discard_commit(struct trace_buffer *buffer,
				struct ring_buffer_event *event)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	int cpu;

	 
	rb_event_discard(event);

	cpu = smp_processor_id();
	cpu_buffer = buffer->buffers[cpu];

	 
	RB_WARN_ON(buffer, !local_read(&cpu_buffer->committing));

	rb_decrement_entry(cpu_buffer, event);
	if (rb_try_to_discard(cpu_buffer, event))
		goto out;

 out:
	rb_end_commit(cpu_buffer);

	trace_recursive_unlock(cpu_buffer);

	preempt_enable_notrace();

}
EXPORT_SYMBOL_GPL(ring_buffer_discard_commit);

 
int ring_buffer_write(struct trace_buffer *buffer,
		      unsigned long length,
		      void *data)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct ring_buffer_event *event;
	void *body;
	int ret = -EBUSY;
	int cpu;

	preempt_disable_notrace();

	if (atomic_read(&buffer->record_disabled))
		goto out;

	cpu = raw_smp_processor_id();

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		goto out;

	cpu_buffer = buffer->buffers[cpu];

	if (atomic_read(&cpu_buffer->record_disabled))
		goto out;

	if (length > BUF_MAX_DATA_SIZE)
		goto out;

	if (unlikely(trace_recursive_lock(cpu_buffer)))
		goto out;

	event = rb_reserve_next_event(buffer, cpu_buffer, length);
	if (!event)
		goto out_unlock;

	body = rb_event_data(event);

	memcpy(body, data, length);

	rb_commit(cpu_buffer);

	rb_wakeups(buffer, cpu_buffer);

	ret = 0;

 out_unlock:
	trace_recursive_unlock(cpu_buffer);

 out:
	preempt_enable_notrace();

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_write);

static bool rb_per_cpu_empty(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct buffer_page *reader = cpu_buffer->reader_page;
	struct buffer_page *head = rb_set_head_page(cpu_buffer);
	struct buffer_page *commit = cpu_buffer->commit_page;

	 
	if (unlikely(!head))
		return true;

	 
	if (reader->read != rb_page_commit(reader))
		return false;

	 
	if (commit == reader)
		return true;

	 
	if (commit != head)
		return false;

	 
	return rb_page_commit(commit) == 0;
}

 
void ring_buffer_record_disable(struct trace_buffer *buffer)
{
	atomic_inc(&buffer->record_disabled);
}
EXPORT_SYMBOL_GPL(ring_buffer_record_disable);

 
void ring_buffer_record_enable(struct trace_buffer *buffer)
{
	atomic_dec(&buffer->record_disabled);
}
EXPORT_SYMBOL_GPL(ring_buffer_record_enable);

 
void ring_buffer_record_off(struct trace_buffer *buffer)
{
	unsigned int rd;
	unsigned int new_rd;

	rd = atomic_read(&buffer->record_disabled);
	do {
		new_rd = rd | RB_BUFFER_OFF;
	} while (!atomic_try_cmpxchg(&buffer->record_disabled, &rd, new_rd));
}
EXPORT_SYMBOL_GPL(ring_buffer_record_off);

 
void ring_buffer_record_on(struct trace_buffer *buffer)
{
	unsigned int rd;
	unsigned int new_rd;

	rd = atomic_read(&buffer->record_disabled);
	do {
		new_rd = rd & ~RB_BUFFER_OFF;
	} while (!atomic_try_cmpxchg(&buffer->record_disabled, &rd, new_rd));
}
EXPORT_SYMBOL_GPL(ring_buffer_record_on);

 
bool ring_buffer_record_is_on(struct trace_buffer *buffer)
{
	return !atomic_read(&buffer->record_disabled);
}

 
bool ring_buffer_record_is_set_on(struct trace_buffer *buffer)
{
	return !(atomic_read(&buffer->record_disabled) & RB_BUFFER_OFF);
}

 
void ring_buffer_record_disable_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return;

	cpu_buffer = buffer->buffers[cpu];
	atomic_inc(&cpu_buffer->record_disabled);
}
EXPORT_SYMBOL_GPL(ring_buffer_record_disable_cpu);

 
void ring_buffer_record_enable_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return;

	cpu_buffer = buffer->buffers[cpu];
	atomic_dec(&cpu_buffer->record_disabled);
}
EXPORT_SYMBOL_GPL(ring_buffer_record_enable_cpu);

 
static inline unsigned long
rb_num_of_entries(struct ring_buffer_per_cpu *cpu_buffer)
{
	return local_read(&cpu_buffer->entries) -
		(local_read(&cpu_buffer->overrun) + cpu_buffer->read);
}

 
u64 ring_buffer_oldest_event_ts(struct trace_buffer *buffer, int cpu)
{
	unsigned long flags;
	struct ring_buffer_per_cpu *cpu_buffer;
	struct buffer_page *bpage;
	u64 ret = 0;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];
	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
	 
	if (cpu_buffer->tail_page == cpu_buffer->reader_page)
		bpage = cpu_buffer->reader_page;
	else
		bpage = rb_set_head_page(cpu_buffer);
	if (bpage)
		ret = bpage->page->time_stamp;
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_oldest_event_ts);

 
unsigned long ring_buffer_bytes_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long ret;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];
	ret = local_read(&cpu_buffer->entries_bytes) - cpu_buffer->read_bytes;

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_bytes_cpu);

 
unsigned long ring_buffer_entries_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];

	return rb_num_of_entries(cpu_buffer);
}
EXPORT_SYMBOL_GPL(ring_buffer_entries_cpu);

 
unsigned long ring_buffer_overrun_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long ret;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];
	ret = local_read(&cpu_buffer->overrun);

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_overrun_cpu);

 
unsigned long
ring_buffer_commit_overrun_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long ret;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];
	ret = local_read(&cpu_buffer->commit_overrun);

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_commit_overrun_cpu);

 
unsigned long
ring_buffer_dropped_events_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long ret;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];
	ret = local_read(&cpu_buffer->dropped_events);

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_dropped_events_cpu);

 
unsigned long
ring_buffer_read_events_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	cpu_buffer = buffer->buffers[cpu];
	return cpu_buffer->read;
}
EXPORT_SYMBOL_GPL(ring_buffer_read_events_cpu);

 
unsigned long ring_buffer_entries(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long entries = 0;
	int cpu;

	 
	for_each_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];
		entries += rb_num_of_entries(cpu_buffer);
	}

	return entries;
}
EXPORT_SYMBOL_GPL(ring_buffer_entries);

 
unsigned long ring_buffer_overruns(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long overruns = 0;
	int cpu;

	 
	for_each_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];
		overruns += local_read(&cpu_buffer->overrun);
	}

	return overruns;
}
EXPORT_SYMBOL_GPL(ring_buffer_overruns);

static void rb_iter_reset(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer = iter->cpu_buffer;

	 
	iter->head_page = cpu_buffer->reader_page;
	iter->head = cpu_buffer->reader_page->read;
	iter->next_event = iter->head;

	iter->cache_reader_page = iter->head_page;
	iter->cache_read = cpu_buffer->read;
	iter->cache_pages_removed = cpu_buffer->pages_removed;

	if (iter->head) {
		iter->read_stamp = cpu_buffer->read_stamp;
		iter->page_stamp = cpu_buffer->reader_page->page->time_stamp;
	} else {
		iter->read_stamp = iter->head_page->page->time_stamp;
		iter->page_stamp = iter->read_stamp;
	}
}

 
void ring_buffer_iter_reset(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long flags;

	if (!iter)
		return;

	cpu_buffer = iter->cpu_buffer;

	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
	rb_iter_reset(iter);
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);
}
EXPORT_SYMBOL_GPL(ring_buffer_iter_reset);

 
int ring_buffer_iter_empty(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct buffer_page *reader;
	struct buffer_page *head_page;
	struct buffer_page *commit_page;
	struct buffer_page *curr_commit_page;
	unsigned commit;
	u64 curr_commit_ts;
	u64 commit_ts;

	cpu_buffer = iter->cpu_buffer;
	reader = cpu_buffer->reader_page;
	head_page = cpu_buffer->head_page;
	commit_page = cpu_buffer->commit_page;
	commit_ts = commit_page->page->time_stamp;

	 
	smp_rmb();
	commit = rb_page_commit(commit_page);
	 
	smp_rmb();

	 
	curr_commit_page = READ_ONCE(cpu_buffer->commit_page);
	curr_commit_ts = READ_ONCE(curr_commit_page->page->time_stamp);

	 
	if (curr_commit_page != commit_page ||
	    curr_commit_ts != commit_ts)
		return 0;

	 
	return ((iter->head_page == commit_page && iter->head >= commit) ||
		(iter->head_page == reader && commit_page == head_page &&
		 head_page->read == commit &&
		 iter->head == rb_page_commit(cpu_buffer->reader_page)));
}
EXPORT_SYMBOL_GPL(ring_buffer_iter_empty);

static void
rb_update_read_stamp(struct ring_buffer_per_cpu *cpu_buffer,
		     struct ring_buffer_event *event)
{
	u64 delta;

	switch (event->type_len) {
	case RINGBUF_TYPE_PADDING:
		return;

	case RINGBUF_TYPE_TIME_EXTEND:
		delta = rb_event_time_stamp(event);
		cpu_buffer->read_stamp += delta;
		return;

	case RINGBUF_TYPE_TIME_STAMP:
		delta = rb_event_time_stamp(event);
		delta = rb_fix_abs_ts(delta, cpu_buffer->read_stamp);
		cpu_buffer->read_stamp = delta;
		return;

	case RINGBUF_TYPE_DATA:
		cpu_buffer->read_stamp += event->time_delta;
		return;

	default:
		RB_WARN_ON(cpu_buffer, 1);
	}
}

static void
rb_update_iter_read_stamp(struct ring_buffer_iter *iter,
			  struct ring_buffer_event *event)
{
	u64 delta;

	switch (event->type_len) {
	case RINGBUF_TYPE_PADDING:
		return;

	case RINGBUF_TYPE_TIME_EXTEND:
		delta = rb_event_time_stamp(event);
		iter->read_stamp += delta;
		return;

	case RINGBUF_TYPE_TIME_STAMP:
		delta = rb_event_time_stamp(event);
		delta = rb_fix_abs_ts(delta, iter->read_stamp);
		iter->read_stamp = delta;
		return;

	case RINGBUF_TYPE_DATA:
		iter->read_stamp += event->time_delta;
		return;

	default:
		RB_WARN_ON(iter->cpu_buffer, 1);
	}
}

static struct buffer_page *
rb_get_reader_page(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct buffer_page *reader = NULL;
	unsigned long overwrite;
	unsigned long flags;
	int nr_loops = 0;
	bool ret;

	local_irq_save(flags);
	arch_spin_lock(&cpu_buffer->lock);

 again:
	 
	if (RB_WARN_ON(cpu_buffer, ++nr_loops > 3)) {
		reader = NULL;
		goto out;
	}

	reader = cpu_buffer->reader_page;

	 
	if (cpu_buffer->reader_page->read < rb_page_size(reader))
		goto out;

	 
	if (RB_WARN_ON(cpu_buffer,
		       cpu_buffer->reader_page->read > rb_page_size(reader)))
		goto out;

	 
	reader = NULL;
	if (cpu_buffer->commit_page == cpu_buffer->reader_page)
		goto out;

	 
	if (rb_num_of_entries(cpu_buffer) == 0)
		goto out;

	 
	local_set(&cpu_buffer->reader_page->write, 0);
	local_set(&cpu_buffer->reader_page->entries, 0);
	local_set(&cpu_buffer->reader_page->page->commit, 0);
	cpu_buffer->reader_page->real_end = 0;

 spin:
	 
	reader = rb_set_head_page(cpu_buffer);
	if (!reader)
		goto out;
	cpu_buffer->reader_page->list.next = rb_list_head(reader->list.next);
	cpu_buffer->reader_page->list.prev = reader->list.prev;

	 
	cpu_buffer->pages = reader->list.prev;

	 
	rb_set_list_to_head(&cpu_buffer->reader_page->list);

	 
	smp_mb();
	overwrite = local_read(&(cpu_buffer->overrun));

	 

	ret = rb_head_page_replace(reader, cpu_buffer->reader_page);

	 
	if (!ret)
		goto spin;

	 
	rb_list_head(reader->list.next)->prev = &cpu_buffer->reader_page->list;
	rb_inc_page(&cpu_buffer->head_page);

	local_inc(&cpu_buffer->pages_read);

	 
	cpu_buffer->reader_page = reader;
	cpu_buffer->reader_page->read = 0;

	if (overwrite != cpu_buffer->last_overrun) {
		cpu_buffer->lost_events = overwrite - cpu_buffer->last_overrun;
		cpu_buffer->last_overrun = overwrite;
	}

	goto again;

 out:
	 
	if (reader && reader->read == 0)
		cpu_buffer->read_stamp = reader->page->time_stamp;

	arch_spin_unlock(&cpu_buffer->lock);
	local_irq_restore(flags);

	 
#define USECS_WAIT	1000000
        for (nr_loops = 0; nr_loops < USECS_WAIT; nr_loops++) {
		 
		if (likely(!reader || rb_page_write(reader) <= BUF_PAGE_SIZE))
			break;

		udelay(1);

		 
		smp_rmb();
	}

	 
	if (RB_WARN_ON(cpu_buffer, nr_loops == USECS_WAIT))
		reader = NULL;

	 
	smp_rmb();


	return reader;
}

static void rb_advance_reader(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct ring_buffer_event *event;
	struct buffer_page *reader;
	unsigned length;

	reader = rb_get_reader_page(cpu_buffer);

	 
	if (RB_WARN_ON(cpu_buffer, !reader))
		return;

	event = rb_reader_event(cpu_buffer);

	if (event->type_len <= RINGBUF_TYPE_DATA_TYPE_LEN_MAX)
		cpu_buffer->read++;

	rb_update_read_stamp(cpu_buffer, event);

	length = rb_event_length(event);
	cpu_buffer->reader_page->read += length;
	cpu_buffer->read_bytes += length;
}

static void rb_advance_iter(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer;

	cpu_buffer = iter->cpu_buffer;

	 
	if (iter->head == iter->next_event) {
		 
		if (rb_iter_head_event(iter) == NULL)
			return;
	}

	iter->head = iter->next_event;

	 
	if (iter->next_event >= rb_page_size(iter->head_page)) {
		 
		if (iter->head_page == cpu_buffer->commit_page)
			return;
		rb_inc_iter(iter);
		return;
	}

	rb_update_iter_read_stamp(iter, iter->event);
}

static int rb_lost_events(struct ring_buffer_per_cpu *cpu_buffer)
{
	return cpu_buffer->lost_events;
}

static struct ring_buffer_event *
rb_buffer_peek(struct ring_buffer_per_cpu *cpu_buffer, u64 *ts,
	       unsigned long *lost_events)
{
	struct ring_buffer_event *event;
	struct buffer_page *reader;
	int nr_loops = 0;

	if (ts)
		*ts = 0;
 again:
	 
	if (RB_WARN_ON(cpu_buffer, ++nr_loops > 2))
		return NULL;

	reader = rb_get_reader_page(cpu_buffer);
	if (!reader)
		return NULL;

	event = rb_reader_event(cpu_buffer);

	switch (event->type_len) {
	case RINGBUF_TYPE_PADDING:
		if (rb_null_event(event))
			RB_WARN_ON(cpu_buffer, 1);
		 
		return event;

	case RINGBUF_TYPE_TIME_EXTEND:
		 
		rb_advance_reader(cpu_buffer);
		goto again;

	case RINGBUF_TYPE_TIME_STAMP:
		if (ts) {
			*ts = rb_event_time_stamp(event);
			*ts = rb_fix_abs_ts(*ts, reader->page->time_stamp);
			ring_buffer_normalize_time_stamp(cpu_buffer->buffer,
							 cpu_buffer->cpu, ts);
		}
		 
		rb_advance_reader(cpu_buffer);
		goto again;

	case RINGBUF_TYPE_DATA:
		if (ts && !(*ts)) {
			*ts = cpu_buffer->read_stamp + event->time_delta;
			ring_buffer_normalize_time_stamp(cpu_buffer->buffer,
							 cpu_buffer->cpu, ts);
		}
		if (lost_events)
			*lost_events = rb_lost_events(cpu_buffer);
		return event;

	default:
		RB_WARN_ON(cpu_buffer, 1);
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(ring_buffer_peek);

static struct ring_buffer_event *
rb_iter_peek(struct ring_buffer_iter *iter, u64 *ts)
{
	struct trace_buffer *buffer;
	struct ring_buffer_per_cpu *cpu_buffer;
	struct ring_buffer_event *event;
	int nr_loops = 0;

	if (ts)
		*ts = 0;

	cpu_buffer = iter->cpu_buffer;
	buffer = cpu_buffer->buffer;

	 
	if (unlikely(iter->cache_read != cpu_buffer->read ||
		     iter->cache_reader_page != cpu_buffer->reader_page ||
		     iter->cache_pages_removed != cpu_buffer->pages_removed))
		rb_iter_reset(iter);

 again:
	if (ring_buffer_iter_empty(iter))
		return NULL;

	 
	if (++nr_loops > 3)
		return NULL;

	if (rb_per_cpu_empty(cpu_buffer))
		return NULL;

	if (iter->head >= rb_page_size(iter->head_page)) {
		rb_inc_iter(iter);
		goto again;
	}

	event = rb_iter_head_event(iter);
	if (!event)
		goto again;

	switch (event->type_len) {
	case RINGBUF_TYPE_PADDING:
		if (rb_null_event(event)) {
			rb_inc_iter(iter);
			goto again;
		}
		rb_advance_iter(iter);
		return event;

	case RINGBUF_TYPE_TIME_EXTEND:
		 
		rb_advance_iter(iter);
		goto again;

	case RINGBUF_TYPE_TIME_STAMP:
		if (ts) {
			*ts = rb_event_time_stamp(event);
			*ts = rb_fix_abs_ts(*ts, iter->head_page->page->time_stamp);
			ring_buffer_normalize_time_stamp(cpu_buffer->buffer,
							 cpu_buffer->cpu, ts);
		}
		 
		rb_advance_iter(iter);
		goto again;

	case RINGBUF_TYPE_DATA:
		if (ts && !(*ts)) {
			*ts = iter->read_stamp + event->time_delta;
			ring_buffer_normalize_time_stamp(buffer,
							 cpu_buffer->cpu, ts);
		}
		return event;

	default:
		RB_WARN_ON(cpu_buffer, 1);
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(ring_buffer_iter_peek);

static inline bool rb_reader_lock(struct ring_buffer_per_cpu *cpu_buffer)
{
	if (likely(!in_nmi())) {
		raw_spin_lock(&cpu_buffer->reader_lock);
		return true;
	}

	 
	if (raw_spin_trylock(&cpu_buffer->reader_lock))
		return true;

	 
	atomic_inc(&cpu_buffer->record_disabled);
	return false;
}

static inline void
rb_reader_unlock(struct ring_buffer_per_cpu *cpu_buffer, bool locked)
{
	if (likely(locked))
		raw_spin_unlock(&cpu_buffer->reader_lock);
}

 
struct ring_buffer_event *
ring_buffer_peek(struct trace_buffer *buffer, int cpu, u64 *ts,
		 unsigned long *lost_events)
{
	struct ring_buffer_per_cpu *cpu_buffer = buffer->buffers[cpu];
	struct ring_buffer_event *event;
	unsigned long flags;
	bool dolock;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return NULL;

 again:
	local_irq_save(flags);
	dolock = rb_reader_lock(cpu_buffer);
	event = rb_buffer_peek(cpu_buffer, ts, lost_events);
	if (event && event->type_len == RINGBUF_TYPE_PADDING)
		rb_advance_reader(cpu_buffer);
	rb_reader_unlock(cpu_buffer, dolock);
	local_irq_restore(flags);

	if (event && event->type_len == RINGBUF_TYPE_PADDING)
		goto again;

	return event;
}

 
bool ring_buffer_iter_dropped(struct ring_buffer_iter *iter)
{
	bool ret = iter->missed_events != 0;

	iter->missed_events = 0;
	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_iter_dropped);

 
struct ring_buffer_event *
ring_buffer_iter_peek(struct ring_buffer_iter *iter, u64 *ts)
{
	struct ring_buffer_per_cpu *cpu_buffer = iter->cpu_buffer;
	struct ring_buffer_event *event;
	unsigned long flags;

 again:
	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
	event = rb_iter_peek(iter, ts);
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);

	if (event && event->type_len == RINGBUF_TYPE_PADDING)
		goto again;

	return event;
}

 
struct ring_buffer_event *
ring_buffer_consume(struct trace_buffer *buffer, int cpu, u64 *ts,
		    unsigned long *lost_events)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct ring_buffer_event *event = NULL;
	unsigned long flags;
	bool dolock;

 again:
	 
	preempt_disable();

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		goto out;

	cpu_buffer = buffer->buffers[cpu];
	local_irq_save(flags);
	dolock = rb_reader_lock(cpu_buffer);

	event = rb_buffer_peek(cpu_buffer, ts, lost_events);
	if (event) {
		cpu_buffer->lost_events = 0;
		rb_advance_reader(cpu_buffer);
	}

	rb_reader_unlock(cpu_buffer, dolock);
	local_irq_restore(flags);

 out:
	preempt_enable();

	if (event && event->type_len == RINGBUF_TYPE_PADDING)
		goto again;

	return event;
}
EXPORT_SYMBOL_GPL(ring_buffer_consume);

 
struct ring_buffer_iter *
ring_buffer_read_prepare(struct trace_buffer *buffer, int cpu, gfp_t flags)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct ring_buffer_iter *iter;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return NULL;

	iter = kzalloc(sizeof(*iter), flags);
	if (!iter)
		return NULL;

	 
	iter->event = kmalloc(BUF_PAGE_SIZE, flags);
	if (!iter->event) {
		kfree(iter);
		return NULL;
	}

	cpu_buffer = buffer->buffers[cpu];

	iter->cpu_buffer = cpu_buffer;

	atomic_inc(&cpu_buffer->resize_disabled);

	return iter;
}
EXPORT_SYMBOL_GPL(ring_buffer_read_prepare);

 
void
ring_buffer_read_prepare_sync(void)
{
	synchronize_rcu();
}
EXPORT_SYMBOL_GPL(ring_buffer_read_prepare_sync);

 
void
ring_buffer_read_start(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long flags;

	if (!iter)
		return;

	cpu_buffer = iter->cpu_buffer;

	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
	arch_spin_lock(&cpu_buffer->lock);
	rb_iter_reset(iter);
	arch_spin_unlock(&cpu_buffer->lock);
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);
}
EXPORT_SYMBOL_GPL(ring_buffer_read_start);

 
void
ring_buffer_read_finish(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer = iter->cpu_buffer;
	unsigned long flags;

	 
	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);
	rb_check_pages(cpu_buffer);
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);

	atomic_dec(&cpu_buffer->resize_disabled);
	kfree(iter->event);
	kfree(iter);
}
EXPORT_SYMBOL_GPL(ring_buffer_read_finish);

 
void ring_buffer_iter_advance(struct ring_buffer_iter *iter)
{
	struct ring_buffer_per_cpu *cpu_buffer = iter->cpu_buffer;
	unsigned long flags;

	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);

	rb_advance_iter(iter);

	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);
}
EXPORT_SYMBOL_GPL(ring_buffer_iter_advance);

 
unsigned long ring_buffer_size(struct trace_buffer *buffer, int cpu)
{
	 
	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	return BUF_PAGE_SIZE * buffer->buffers[cpu]->nr_pages;
}
EXPORT_SYMBOL_GPL(ring_buffer_size);

static void rb_clear_buffer_page(struct buffer_page *page)
{
	local_set(&page->write, 0);
	local_set(&page->entries, 0);
	rb_init_page(page->page);
	page->read = 0;
}

static void
rb_reset_cpu(struct ring_buffer_per_cpu *cpu_buffer)
{
	struct buffer_page *page;

	rb_head_page_deactivate(cpu_buffer);

	cpu_buffer->head_page
		= list_entry(cpu_buffer->pages, struct buffer_page, list);
	rb_clear_buffer_page(cpu_buffer->head_page);
	list_for_each_entry(page, cpu_buffer->pages, list) {
		rb_clear_buffer_page(page);
	}

	cpu_buffer->tail_page = cpu_buffer->head_page;
	cpu_buffer->commit_page = cpu_buffer->head_page;

	INIT_LIST_HEAD(&cpu_buffer->reader_page->list);
	INIT_LIST_HEAD(&cpu_buffer->new_pages);
	rb_clear_buffer_page(cpu_buffer->reader_page);

	local_set(&cpu_buffer->entries_bytes, 0);
	local_set(&cpu_buffer->overrun, 0);
	local_set(&cpu_buffer->commit_overrun, 0);
	local_set(&cpu_buffer->dropped_events, 0);
	local_set(&cpu_buffer->entries, 0);
	local_set(&cpu_buffer->committing, 0);
	local_set(&cpu_buffer->commits, 0);
	local_set(&cpu_buffer->pages_touched, 0);
	local_set(&cpu_buffer->pages_lost, 0);
	local_set(&cpu_buffer->pages_read, 0);
	cpu_buffer->last_pages_touch = 0;
	cpu_buffer->shortest_full = 0;
	cpu_buffer->read = 0;
	cpu_buffer->read_bytes = 0;

	rb_time_set(&cpu_buffer->write_stamp, 0);
	rb_time_set(&cpu_buffer->before_stamp, 0);

	memset(cpu_buffer->event_stamp, 0, sizeof(cpu_buffer->event_stamp));

	cpu_buffer->lost_events = 0;
	cpu_buffer->last_overrun = 0;

	rb_head_page_activate(cpu_buffer);
	cpu_buffer->pages_removed = 0;
}

 
static void reset_disabled_cpu_buffer(struct ring_buffer_per_cpu *cpu_buffer)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);

	if (RB_WARN_ON(cpu_buffer, local_read(&cpu_buffer->committing)))
		goto out;

	arch_spin_lock(&cpu_buffer->lock);

	rb_reset_cpu(cpu_buffer);

	arch_spin_unlock(&cpu_buffer->lock);

 out:
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);
}

 
void ring_buffer_reset_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer = buffer->buffers[cpu];

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return;

	 
	mutex_lock(&buffer->mutex);

	atomic_inc(&cpu_buffer->resize_disabled);
	atomic_inc(&cpu_buffer->record_disabled);

	 
	synchronize_rcu();

	reset_disabled_cpu_buffer(cpu_buffer);

	atomic_dec(&cpu_buffer->record_disabled);
	atomic_dec(&cpu_buffer->resize_disabled);

	mutex_unlock(&buffer->mutex);
}
EXPORT_SYMBOL_GPL(ring_buffer_reset_cpu);

 
#define RESET_BIT	(1 << 30)

 
void ring_buffer_reset_online_cpus(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	int cpu;

	 
	mutex_lock(&buffer->mutex);

	for_each_online_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];

		atomic_add(RESET_BIT, &cpu_buffer->resize_disabled);
		atomic_inc(&cpu_buffer->record_disabled);
	}

	 
	synchronize_rcu();

	for_each_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];

		 
		if (!(atomic_read(&cpu_buffer->resize_disabled) & RESET_BIT))
			continue;

		reset_disabled_cpu_buffer(cpu_buffer);

		atomic_dec(&cpu_buffer->record_disabled);
		atomic_sub(RESET_BIT, &cpu_buffer->resize_disabled);
	}

	mutex_unlock(&buffer->mutex);
}

 
void ring_buffer_reset(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	int cpu;

	 
	mutex_lock(&buffer->mutex);

	for_each_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];

		atomic_inc(&cpu_buffer->resize_disabled);
		atomic_inc(&cpu_buffer->record_disabled);
	}

	 
	synchronize_rcu();

	for_each_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];

		reset_disabled_cpu_buffer(cpu_buffer);

		atomic_dec(&cpu_buffer->record_disabled);
		atomic_dec(&cpu_buffer->resize_disabled);
	}

	mutex_unlock(&buffer->mutex);
}
EXPORT_SYMBOL_GPL(ring_buffer_reset);

 
bool ring_buffer_empty(struct trace_buffer *buffer)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long flags;
	bool dolock;
	bool ret;
	int cpu;

	 
	for_each_buffer_cpu(buffer, cpu) {
		cpu_buffer = buffer->buffers[cpu];
		local_irq_save(flags);
		dolock = rb_reader_lock(cpu_buffer);
		ret = rb_per_cpu_empty(cpu_buffer);
		rb_reader_unlock(cpu_buffer, dolock);
		local_irq_restore(flags);

		if (!ret)
			return false;
	}

	return true;
}
EXPORT_SYMBOL_GPL(ring_buffer_empty);

 
bool ring_buffer_empty_cpu(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	unsigned long flags;
	bool dolock;
	bool ret;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return true;

	cpu_buffer = buffer->buffers[cpu];
	local_irq_save(flags);
	dolock = rb_reader_lock(cpu_buffer);
	ret = rb_per_cpu_empty(cpu_buffer);
	rb_reader_unlock(cpu_buffer, dolock);
	local_irq_restore(flags);

	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_empty_cpu);

#ifdef CONFIG_RING_BUFFER_ALLOW_SWAP
 
int ring_buffer_swap_cpu(struct trace_buffer *buffer_a,
			 struct trace_buffer *buffer_b, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer_a;
	struct ring_buffer_per_cpu *cpu_buffer_b;
	int ret = -EINVAL;

	if (!cpumask_test_cpu(cpu, buffer_a->cpumask) ||
	    !cpumask_test_cpu(cpu, buffer_b->cpumask))
		goto out;

	cpu_buffer_a = buffer_a->buffers[cpu];
	cpu_buffer_b = buffer_b->buffers[cpu];

	 
	if (cpu_buffer_a->nr_pages != cpu_buffer_b->nr_pages)
		goto out;

	ret = -EAGAIN;

	if (atomic_read(&buffer_a->record_disabled))
		goto out;

	if (atomic_read(&buffer_b->record_disabled))
		goto out;

	if (atomic_read(&cpu_buffer_a->record_disabled))
		goto out;

	if (atomic_read(&cpu_buffer_b->record_disabled))
		goto out;

	 
	atomic_inc(&cpu_buffer_a->record_disabled);
	atomic_inc(&cpu_buffer_b->record_disabled);

	ret = -EBUSY;
	if (local_read(&cpu_buffer_a->committing))
		goto out_dec;
	if (local_read(&cpu_buffer_b->committing))
		goto out_dec;

	 
	if (atomic_read(&buffer_a->resizing))
		goto out_dec;
	if (atomic_read(&buffer_b->resizing))
		goto out_dec;

	buffer_a->buffers[cpu] = cpu_buffer_b;
	buffer_b->buffers[cpu] = cpu_buffer_a;

	cpu_buffer_b->buffer = buffer_a;
	cpu_buffer_a->buffer = buffer_b;

	ret = 0;

out_dec:
	atomic_dec(&cpu_buffer_a->record_disabled);
	atomic_dec(&cpu_buffer_b->record_disabled);
out:
	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_swap_cpu);
#endif  

 
void *ring_buffer_alloc_read_page(struct trace_buffer *buffer, int cpu)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct buffer_data_page *bpage = NULL;
	unsigned long flags;
	struct page *page;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		return ERR_PTR(-ENODEV);

	cpu_buffer = buffer->buffers[cpu];
	local_irq_save(flags);
	arch_spin_lock(&cpu_buffer->lock);

	if (cpu_buffer->free_page) {
		bpage = cpu_buffer->free_page;
		cpu_buffer->free_page = NULL;
	}

	arch_spin_unlock(&cpu_buffer->lock);
	local_irq_restore(flags);

	if (bpage)
		goto out;

	page = alloc_pages_node(cpu_to_node(cpu),
				GFP_KERNEL | __GFP_NORETRY, 0);
	if (!page)
		return ERR_PTR(-ENOMEM);

	bpage = page_address(page);

 out:
	rb_init_page(bpage);

	return bpage;
}
EXPORT_SYMBOL_GPL(ring_buffer_alloc_read_page);

 
void ring_buffer_free_read_page(struct trace_buffer *buffer, int cpu, void *data)
{
	struct ring_buffer_per_cpu *cpu_buffer;
	struct buffer_data_page *bpage = data;
	struct page *page = virt_to_page(bpage);
	unsigned long flags;

	if (!buffer || !buffer->buffers || !buffer->buffers[cpu])
		return;

	cpu_buffer = buffer->buffers[cpu];

	 
	if (page_ref_count(page) > 1)
		goto out;

	local_irq_save(flags);
	arch_spin_lock(&cpu_buffer->lock);

	if (!cpu_buffer->free_page) {
		cpu_buffer->free_page = bpage;
		bpage = NULL;
	}

	arch_spin_unlock(&cpu_buffer->lock);
	local_irq_restore(flags);

 out:
	free_page((unsigned long)bpage);
}
EXPORT_SYMBOL_GPL(ring_buffer_free_read_page);

 
int ring_buffer_read_page(struct trace_buffer *buffer,
			  void **data_page, size_t len, int cpu, int full)
{
	struct ring_buffer_per_cpu *cpu_buffer = buffer->buffers[cpu];
	struct ring_buffer_event *event;
	struct buffer_data_page *bpage;
	struct buffer_page *reader;
	unsigned long missed_events;
	unsigned long flags;
	unsigned int commit;
	unsigned int read;
	u64 save_timestamp;
	int ret = -1;

	if (!cpumask_test_cpu(cpu, buffer->cpumask))
		goto out;

	 
	if (len <= BUF_PAGE_HDR_SIZE)
		goto out;

	len -= BUF_PAGE_HDR_SIZE;

	if (!data_page)
		goto out;

	bpage = *data_page;
	if (!bpage)
		goto out;

	raw_spin_lock_irqsave(&cpu_buffer->reader_lock, flags);

	reader = rb_get_reader_page(cpu_buffer);
	if (!reader)
		goto out_unlock;

	event = rb_reader_event(cpu_buffer);

	read = reader->read;
	commit = rb_page_commit(reader);

	 
	missed_events = cpu_buffer->lost_events;

	 
	if (read || (len < (commit - read)) ||
	    cpu_buffer->reader_page == cpu_buffer->commit_page) {
		struct buffer_data_page *rpage = cpu_buffer->reader_page->page;
		unsigned int rpos = read;
		unsigned int pos = 0;
		unsigned int size;

		 
		if (full &&
		    (!read || (len < (commit - read)) ||
		     cpu_buffer->reader_page == cpu_buffer->commit_page))
			goto out_unlock;

		if (len > (commit - read))
			len = (commit - read);

		 
		size = rb_event_ts_length(event);

		if (len < size)
			goto out_unlock;

		 
		save_timestamp = cpu_buffer->read_stamp;

		 
		do {
			 
			size = rb_event_length(event);
			memcpy(bpage->data + pos, rpage->data + rpos, size);

			len -= size;

			rb_advance_reader(cpu_buffer);
			rpos = reader->read;
			pos += size;

			if (rpos >= commit)
				break;

			event = rb_reader_event(cpu_buffer);
			 
			size = rb_event_ts_length(event);
		} while (len >= size);

		 
		local_set(&bpage->commit, pos);
		bpage->time_stamp = save_timestamp;

		 
		read = 0;
	} else {
		 
		cpu_buffer->read += rb_page_entries(reader);
		cpu_buffer->read_bytes += rb_page_commit(reader);

		 
		rb_init_page(bpage);
		bpage = reader->page;
		reader->page = *data_page;
		local_set(&reader->write, 0);
		local_set(&reader->entries, 0);
		reader->read = 0;
		*data_page = bpage;

		 
		if (reader->real_end)
			local_set(&bpage->commit, reader->real_end);
	}
	ret = read;

	cpu_buffer->lost_events = 0;

	commit = local_read(&bpage->commit);
	 
	if (missed_events) {
		 
		if (BUF_PAGE_SIZE - commit >= sizeof(missed_events)) {
			memcpy(&bpage->data[commit], &missed_events,
			       sizeof(missed_events));
			local_add(RB_MISSED_STORED, &bpage->commit);
			commit += sizeof(missed_events);
		}
		local_add(RB_MISSED_EVENTS, &bpage->commit);
	}

	 
	if (commit < BUF_PAGE_SIZE)
		memset(&bpage->data[commit], 0, BUF_PAGE_SIZE - commit);

 out_unlock:
	raw_spin_unlock_irqrestore(&cpu_buffer->reader_lock, flags);

 out:
	return ret;
}
EXPORT_SYMBOL_GPL(ring_buffer_read_page);

 
int trace_rb_cpu_prepare(unsigned int cpu, struct hlist_node *node)
{
	struct trace_buffer *buffer;
	long nr_pages_same;
	int cpu_i;
	unsigned long nr_pages;

	buffer = container_of(node, struct trace_buffer, node);
	if (cpumask_test_cpu(cpu, buffer->cpumask))
		return 0;

	nr_pages = 0;
	nr_pages_same = 1;
	 
	for_each_buffer_cpu(buffer, cpu_i) {
		 
		if (nr_pages == 0)
			nr_pages = buffer->buffers[cpu_i]->nr_pages;
		if (nr_pages != buffer->buffers[cpu_i]->nr_pages) {
			nr_pages_same = 0;
			break;
		}
	}
	 
	if (!nr_pages_same)
		nr_pages = 2;
	buffer->buffers[cpu] =
		rb_allocate_cpu_buffer(buffer, nr_pages, cpu);
	if (!buffer->buffers[cpu]) {
		WARN(1, "failed to allocate ring buffer on CPU %u\n",
		     cpu);
		return -ENOMEM;
	}
	smp_wmb();
	cpumask_set_cpu(cpu, buffer->cpumask);
	return 0;
}

#ifdef CONFIG_RING_BUFFER_STARTUP_TEST
 
static struct task_struct *rb_threads[NR_CPUS] __initdata;

struct rb_test_data {
	struct trace_buffer *buffer;
	unsigned long		events;
	unsigned long		bytes_written;
	unsigned long		bytes_alloc;
	unsigned long		bytes_dropped;
	unsigned long		events_nested;
	unsigned long		bytes_written_nested;
	unsigned long		bytes_alloc_nested;
	unsigned long		bytes_dropped_nested;
	int			min_size_nested;
	int			max_size_nested;
	int			max_size;
	int			min_size;
	int			cpu;
	int			cnt;
};

static struct rb_test_data rb_data[NR_CPUS] __initdata;

 
#define RB_TEST_BUFFER_SIZE	1048576

static char rb_string[] __initdata =
	"abcdefghijklmnopqrstuvwxyz1234567890!@#$%^&*()?+\\"
	"?+|:';\",.<>/?abcdefghijklmnopqrstuvwxyz1234567890"
	"!@#$%^&*()?+\\?+|:';\",.<>/?abcdefghijklmnopqrstuv";

static bool rb_test_started __initdata;

struct rb_item {
	int size;
	char str[];
};

static __init int rb_write_something(struct rb_test_data *data, bool nested)
{
	struct ring_buffer_event *event;
	struct rb_item *item;
	bool started;
	int event_len;
	int size;
	int len;
	int cnt;

	 
	cnt = data->cnt + (nested ? 27 : 0);

	 
	size = (cnt * 68 / 25) % (sizeof(rb_string) - 1);

	len = size + sizeof(struct rb_item);

	started = rb_test_started;
	 
	smp_rmb();

	event = ring_buffer_lock_reserve(data->buffer, len);
	if (!event) {
		 
		if (started) {
			if (nested)
				data->bytes_dropped += len;
			else
				data->bytes_dropped_nested += len;
		}
		return len;
	}

	event_len = ring_buffer_event_length(event);

	if (RB_WARN_ON(data->buffer, event_len < len))
		goto out;

	item = ring_buffer_event_data(event);
	item->size = size;
	memcpy(item->str, rb_string, size);

	if (nested) {
		data->bytes_alloc_nested += event_len;
		data->bytes_written_nested += len;
		data->events_nested++;
		if (!data->min_size_nested || len < data->min_size_nested)
			data->min_size_nested = len;
		if (len > data->max_size_nested)
			data->max_size_nested = len;
	} else {
		data->bytes_alloc += event_len;
		data->bytes_written += len;
		data->events++;
		if (!data->min_size || len < data->min_size)
			data->max_size = len;
		if (len > data->max_size)
			data->max_size = len;
	}

 out:
	ring_buffer_unlock_commit(data->buffer);

	return 0;
}

static __init int rb_test(void *arg)
{
	struct rb_test_data *data = arg;

	while (!kthread_should_stop()) {
		rb_write_something(data, false);
		data->cnt++;

		set_current_state(TASK_INTERRUPTIBLE);
		 
		usleep_range(((data->cnt % 3) + 1) * 100, 1000);
	}

	return 0;
}

static __init void rb_ipi(void *ignore)
{
	struct rb_test_data *data;
	int cpu = smp_processor_id();

	data = &rb_data[cpu];
	rb_write_something(data, true);
}

static __init int rb_hammer_test(void *arg)
{
	while (!kthread_should_stop()) {

		 
		smp_call_function(rb_ipi, NULL, 1);
		 
		schedule();
	}

	return 0;
}

static __init int test_ringbuffer(void)
{
	struct task_struct *rb_hammer;
	struct trace_buffer *buffer;
	int cpu;
	int ret = 0;

	if (security_locked_down(LOCKDOWN_TRACEFS)) {
		pr_warn("Lockdown is enabled, skipping ring buffer tests\n");
		return 0;
	}

	pr_info("Running ring buffer tests...\n");

	buffer = ring_buffer_alloc(RB_TEST_BUFFER_SIZE, RB_FL_OVERWRITE);
	if (WARN_ON(!buffer))
		return 0;

	 
	ring_buffer_record_off(buffer);

	for_each_online_cpu(cpu) {
		rb_data[cpu].buffer = buffer;
		rb_data[cpu].cpu = cpu;
		rb_data[cpu].cnt = cpu;
		rb_threads[cpu] = kthread_run_on_cpu(rb_test, &rb_data[cpu],
						     cpu, "rbtester/%u");
		if (WARN_ON(IS_ERR(rb_threads[cpu]))) {
			pr_cont("FAILED\n");
			ret = PTR_ERR(rb_threads[cpu]);
			goto out_free;
		}
	}

	 
	rb_hammer = kthread_run(rb_hammer_test, NULL, "rbhammer");
	if (WARN_ON(IS_ERR(rb_hammer))) {
		pr_cont("FAILED\n");
		ret = PTR_ERR(rb_hammer);
		goto out_free;
	}

	ring_buffer_record_on(buffer);
	 
	smp_wmb();
	rb_test_started = true;

	set_current_state(TASK_INTERRUPTIBLE);
	 ;
	schedule_timeout(10 * HZ);

	kthread_stop(rb_hammer);

 out_free:
	for_each_online_cpu(cpu) {
		if (!rb_threads[cpu])
			break;
		kthread_stop(rb_threads[cpu]);
	}
	if (ret) {
		ring_buffer_free(buffer);
		return ret;
	}

	 
	pr_info("finished\n");
	for_each_online_cpu(cpu) {
		struct ring_buffer_event *event;
		struct rb_test_data *data = &rb_data[cpu];
		struct rb_item *item;
		unsigned long total_events;
		unsigned long total_dropped;
		unsigned long total_written;
		unsigned long total_alloc;
		unsigned long total_read = 0;
		unsigned long total_size = 0;
		unsigned long total_len = 0;
		unsigned long total_lost = 0;
		unsigned long lost;
		int big_event_size;
		int small_event_size;

		ret = -1;

		total_events = data->events + data->events_nested;
		total_written = data->bytes_written + data->bytes_written_nested;
		total_alloc = data->bytes_alloc + data->bytes_alloc_nested;
		total_dropped = data->bytes_dropped + data->bytes_dropped_nested;

		big_event_size = data->max_size + data->max_size_nested;
		small_event_size = data->min_size + data->min_size_nested;

		pr_info("CPU %d:\n", cpu);
		pr_info("              events:    %ld\n", total_events);
		pr_info("       dropped bytes:    %ld\n", total_dropped);
		pr_info("       alloced bytes:    %ld\n", total_alloc);
		pr_info("       written bytes:    %ld\n", total_written);
		pr_info("       biggest event:    %d\n", big_event_size);
		pr_info("      smallest event:    %d\n", small_event_size);

		if (RB_WARN_ON(buffer, total_dropped))
			break;

		ret = 0;

		while ((event = ring_buffer_consume(buffer, cpu, NULL, &lost))) {
			total_lost += lost;
			item = ring_buffer_event_data(event);
			total_len += ring_buffer_event_length(event);
			total_size += item->size + sizeof(struct rb_item);
			if (memcmp(&item->str[0], rb_string, item->size) != 0) {
				pr_info("FAILED!\n");
				pr_info("buffer had: %.*s\n", item->size, item->str);
				pr_info("expected:   %.*s\n", item->size, rb_string);
				RB_WARN_ON(buffer, 1);
				ret = -1;
				break;
			}
			total_read++;
		}
		if (ret)
			break;

		ret = -1;

		pr_info("         read events:   %ld\n", total_read);
		pr_info("         lost events:   %ld\n", total_lost);
		pr_info("        total events:   %ld\n", total_lost + total_read);
		pr_info("  recorded len bytes:   %ld\n", total_len);
		pr_info(" recorded size bytes:   %ld\n", total_size);
		if (total_lost) {
			pr_info(" With dropped events, record len and size may not match\n"
				" alloced and written from above\n");
		} else {
			if (RB_WARN_ON(buffer, total_len != total_alloc ||
				       total_size != total_written))
				break;
		}
		if (RB_WARN_ON(buffer, total_lost + total_read != total_events))
			break;

		ret = 0;
	}
	if (!ret)
		pr_info("Ring buffer PASSED!\n");

	ring_buffer_free(buffer);
	return 0;
}

late_initcall(test_ringbuffer);
#endif  
