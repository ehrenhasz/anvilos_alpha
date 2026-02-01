
 

#include <linux/bitmap.h>
#include <linux/cdev.h>
#include <linux/hashtable.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/uio.h>
#include <linux/ioctl.h>
#include <linux/jhash.h>
#include <linux/refcount.h>
#include <linux/trace_events.h>
#include <linux/tracefs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/user_events.h>
#include "trace_dynevent.h"
#include "trace_output.h"
#include "trace.h"

#define USER_EVENTS_PREFIX_LEN (sizeof(USER_EVENTS_PREFIX)-1)

#define FIELD_DEPTH_TYPE 0
#define FIELD_DEPTH_NAME 1
#define FIELD_DEPTH_SIZE 2

 
#define MAX_EVENT_DESC 512
#define EVENT_NAME(user_event) ((user_event)->tracepoint.name)
#define MAX_FIELD_ARRAY_SIZE 1024

 
#define EVENT_STATUS_FTRACE BIT(0)
#define EVENT_STATUS_PERF BIT(1)
#define EVENT_STATUS_OTHER BIT(7)

 
enum user_reg_flag {
	 
	USER_EVENT_REG_PERSIST		= 1U << 0,

	 
	USER_EVENT_REG_MAX		= 1U << 1,
};

 
struct user_event_group {
	char		*system_name;
	struct		hlist_node node;
	struct		mutex reg_mutex;
	DECLARE_HASHTABLE(register_table, 8);
};

 
static struct user_event_group *init_group;

 
static unsigned int max_user_events = 32768;

 
static unsigned int current_user_events;

 
struct user_event {
	struct user_event_group		*group;
	struct tracepoint		tracepoint;
	struct trace_event_call		call;
	struct trace_event_class	class;
	struct dyn_event		devent;
	struct hlist_node		node;
	struct list_head		fields;
	struct list_head		validators;
	struct work_struct		put_work;
	refcount_t			refcnt;
	int				min_size;
	int				reg_flags;
	char				status;
};

 
struct user_event_enabler {
	struct list_head	mm_enablers_link;
	struct user_event	*event;
	unsigned long		addr;

	 
	unsigned long		values;
};

 
#define ENABLE_VAL_BIT_MASK 0x3F

 
#define ENABLE_VAL_FAULTING_BIT 6

 
#define ENABLE_VAL_FREEING_BIT 7

 
#define ENABLE_VAL_32_ON_64_BIT 8

#define ENABLE_VAL_COMPAT_MASK (1 << ENABLE_VAL_32_ON_64_BIT)

 
#define ENABLE_VAL_DUP_MASK (ENABLE_VAL_BIT_MASK | ENABLE_VAL_COMPAT_MASK)

#define ENABLE_BITOPS(e) (&(e)->values)

#define ENABLE_BIT(e) ((int)((e)->values & ENABLE_VAL_BIT_MASK))

 
struct user_event_enabler_fault {
	struct work_struct		work;
	struct user_event_mm		*mm;
	struct user_event_enabler	*enabler;
	int				attempt;
};

static struct kmem_cache *fault_cache;

 
static LIST_HEAD(user_event_mms);
static DEFINE_SPINLOCK(user_event_mms_lock);

 
struct user_event_refs {
	struct rcu_head		rcu;
	int			count;
	struct user_event	*events[];
};

struct user_event_file_info {
	struct user_event_group	*group;
	struct user_event_refs	*refs;
};

#define VALIDATOR_ENSURE_NULL (1 << 0)
#define VALIDATOR_REL (1 << 1)

struct user_event_validator {
	struct list_head	user_event_link;
	int			offset;
	int			flags;
};

static inline void align_addr_bit(unsigned long *addr, int *bit,
				  unsigned long *flags)
{
	if (IS_ALIGNED(*addr, sizeof(long))) {
#ifdef __BIG_ENDIAN
		 
		if (test_bit(ENABLE_VAL_32_ON_64_BIT, flags))
			*bit += 32;
#endif
		return;
	}

	*addr = ALIGN_DOWN(*addr, sizeof(long));

	 
#ifdef __LITTLE_ENDIAN
	*bit += 32;
#endif
}

typedef void (*user_event_func_t) (struct user_event *user, struct iov_iter *i,
				   void *tpdata, bool *faulted);

static int user_event_parse(struct user_event_group *group, char *name,
			    char *args, char *flags,
			    struct user_event **newuser, int reg_flags);

static struct user_event_mm *user_event_mm_get(struct user_event_mm *mm);
static struct user_event_mm *user_event_mm_get_all(struct user_event *user);
static void user_event_mm_put(struct user_event_mm *mm);
static int destroy_user_event(struct user_event *user);

static u32 user_event_key(char *name)
{
	return jhash(name, strlen(name), 0);
}

static struct user_event *user_event_get(struct user_event *user)
{
	refcount_inc(&user->refcnt);

	return user;
}

static void delayed_destroy_user_event(struct work_struct *work)
{
	struct user_event *user = container_of(
		work, struct user_event, put_work);

	mutex_lock(&event_mutex);

	if (!refcount_dec_and_test(&user->refcnt))
		goto out;

	if (destroy_user_event(user)) {
		 
		pr_warn("user_events: Unable to delete event\n");
		refcount_set(&user->refcnt, 1);
	}
out:
	mutex_unlock(&event_mutex);
}

static void user_event_put(struct user_event *user, bool locked)
{
	bool delete;

	if (unlikely(!user))
		return;

	 
	if (!locked) {
		lockdep_assert_not_held(&event_mutex);
		delete = refcount_dec_and_mutex_lock(&user->refcnt, &event_mutex);
	} else {
		lockdep_assert_held(&event_mutex);
		delete = refcount_dec_and_test(&user->refcnt);
	}

	if (!delete)
		return;

	 

	if (user->reg_flags & USER_EVENT_REG_PERSIST) {
		 
		pr_alert("BUG: Auto-delete engaged on persistent event\n");
		goto out;
	}

	 
	INIT_WORK(&user->put_work, delayed_destroy_user_event);

	 
	refcount_set(&user->refcnt, 1);

	if (WARN_ON_ONCE(!schedule_work(&user->put_work))) {
		 
		pr_warn("user_events: Unable to queue delayed destroy\n");
	}
out:
	 
	if (!locked)
		mutex_unlock(&event_mutex);
}

static void user_event_group_destroy(struct user_event_group *group)
{
	kfree(group->system_name);
	kfree(group);
}

static char *user_event_group_system_name(void)
{
	char *system_name;
	int len = sizeof(USER_EVENTS_SYSTEM) + 1;

	system_name = kmalloc(len, GFP_KERNEL);

	if (!system_name)
		return NULL;

	snprintf(system_name, len, "%s", USER_EVENTS_SYSTEM);

	return system_name;
}

static struct user_event_group *current_user_event_group(void)
{
	return init_group;
}

static struct user_event_group *user_event_group_create(void)
{
	struct user_event_group *group;

	group = kzalloc(sizeof(*group), GFP_KERNEL);

	if (!group)
		return NULL;

	group->system_name = user_event_group_system_name();

	if (!group->system_name)
		goto error;

	mutex_init(&group->reg_mutex);
	hash_init(group->register_table);

	return group;
error:
	if (group)
		user_event_group_destroy(group);

	return NULL;
};

static void user_event_enabler_destroy(struct user_event_enabler *enabler,
				       bool locked)
{
	list_del_rcu(&enabler->mm_enablers_link);

	 
	user_event_put(enabler->event, locked);

	kfree(enabler);
}

static int user_event_mm_fault_in(struct user_event_mm *mm, unsigned long uaddr,
				  int attempt)
{
	bool unlocked;
	int ret;

	 
	if (attempt > 10)
		return -EFAULT;

	mmap_read_lock(mm->mm);

	 
	if (refcount_read(&mm->tasks) == 0) {
		ret = -ENOENT;
		goto out;
	}

	ret = fixup_user_fault(mm->mm, uaddr, FAULT_FLAG_WRITE | FAULT_FLAG_REMOTE,
			       &unlocked);
out:
	mmap_read_unlock(mm->mm);

	return ret;
}

static int user_event_enabler_write(struct user_event_mm *mm,
				    struct user_event_enabler *enabler,
				    bool fixup_fault, int *attempt);

static void user_event_enabler_fault_fixup(struct work_struct *work)
{
	struct user_event_enabler_fault *fault = container_of(
		work, struct user_event_enabler_fault, work);
	struct user_event_enabler *enabler = fault->enabler;
	struct user_event_mm *mm = fault->mm;
	unsigned long uaddr = enabler->addr;
	int attempt = fault->attempt;
	int ret;

	ret = user_event_mm_fault_in(mm, uaddr, attempt);

	if (ret && ret != -ENOENT) {
		struct user_event *user = enabler->event;

		pr_warn("user_events: Fault for mm: 0x%pK @ 0x%llx event: %s\n",
			mm->mm, (unsigned long long)uaddr, EVENT_NAME(user));
	}

	 
	mutex_lock(&event_mutex);

	 
	if (test_bit(ENABLE_VAL_FREEING_BIT, ENABLE_BITOPS(enabler))) {
		user_event_enabler_destroy(enabler, true);
		goto out;
	}

	 
	clear_bit(ENABLE_VAL_FAULTING_BIT, ENABLE_BITOPS(enabler));

	if (!ret) {
		mmap_read_lock(mm->mm);
		user_event_enabler_write(mm, enabler, true, &attempt);
		mmap_read_unlock(mm->mm);
	}
out:
	mutex_unlock(&event_mutex);

	 
	user_event_mm_put(mm);
	kmem_cache_free(fault_cache, fault);
}

static bool user_event_enabler_queue_fault(struct user_event_mm *mm,
					   struct user_event_enabler *enabler,
					   int attempt)
{
	struct user_event_enabler_fault *fault;

	fault = kmem_cache_zalloc(fault_cache, GFP_NOWAIT | __GFP_NOWARN);

	if (!fault)
		return false;

	INIT_WORK(&fault->work, user_event_enabler_fault_fixup);
	fault->mm = user_event_mm_get(mm);
	fault->enabler = enabler;
	fault->attempt = attempt;

	 
	set_bit(ENABLE_VAL_FAULTING_BIT, ENABLE_BITOPS(enabler));

	if (!schedule_work(&fault->work)) {
		 
		clear_bit(ENABLE_VAL_FAULTING_BIT, ENABLE_BITOPS(enabler));

		user_event_mm_put(mm);
		kmem_cache_free(fault_cache, fault);

		return false;
	}

	return true;
}

static int user_event_enabler_write(struct user_event_mm *mm,
				    struct user_event_enabler *enabler,
				    bool fixup_fault, int *attempt)
{
	unsigned long uaddr = enabler->addr;
	unsigned long *ptr;
	struct page *page;
	void *kaddr;
	int bit = ENABLE_BIT(enabler);
	int ret;

	lockdep_assert_held(&event_mutex);
	mmap_assert_locked(mm->mm);

	*attempt += 1;

	 
	if (refcount_read(&mm->tasks) == 0)
		return -ENOENT;

	if (unlikely(test_bit(ENABLE_VAL_FAULTING_BIT, ENABLE_BITOPS(enabler)) ||
		     test_bit(ENABLE_VAL_FREEING_BIT, ENABLE_BITOPS(enabler))))
		return -EBUSY;

	align_addr_bit(&uaddr, &bit, ENABLE_BITOPS(enabler));

	ret = pin_user_pages_remote(mm->mm, uaddr, 1, FOLL_WRITE | FOLL_NOFAULT,
				    &page, NULL);

	if (unlikely(ret <= 0)) {
		if (!fixup_fault)
			return -EFAULT;

		if (!user_event_enabler_queue_fault(mm, enabler, *attempt))
			pr_warn("user_events: Unable to queue fault handler\n");

		return -EFAULT;
	}

	kaddr = kmap_local_page(page);
	ptr = kaddr + (uaddr & ~PAGE_MASK);

	 
	if (enabler->event && enabler->event->status)
		set_bit(bit, ptr);
	else
		clear_bit(bit, ptr);

	kunmap_local(kaddr);
	unpin_user_pages_dirty_lock(&page, 1, true);

	return 0;
}

static bool user_event_enabler_exists(struct user_event_mm *mm,
				      unsigned long uaddr, unsigned char bit)
{
	struct user_event_enabler *enabler;

	list_for_each_entry(enabler, &mm->enablers, mm_enablers_link) {
		if (enabler->addr == uaddr && ENABLE_BIT(enabler) == bit)
			return true;
	}

	return false;
}

static void user_event_enabler_update(struct user_event *user)
{
	struct user_event_enabler *enabler;
	struct user_event_mm *next;
	struct user_event_mm *mm;
	int attempt;

	lockdep_assert_held(&event_mutex);

	 
	mm = user_event_mm_get_all(user);

	while (mm) {
		next = mm->next;
		mmap_read_lock(mm->mm);

		list_for_each_entry(enabler, &mm->enablers, mm_enablers_link) {
			if (enabler->event == user) {
				attempt = 0;
				user_event_enabler_write(mm, enabler, true, &attempt);
			}
		}

		mmap_read_unlock(mm->mm);
		user_event_mm_put(mm);
		mm = next;
	}
}

static bool user_event_enabler_dup(struct user_event_enabler *orig,
				   struct user_event_mm *mm)
{
	struct user_event_enabler *enabler;

	 
	if (unlikely(test_bit(ENABLE_VAL_FREEING_BIT, ENABLE_BITOPS(orig))))
		return true;

	enabler = kzalloc(sizeof(*enabler), GFP_NOWAIT | __GFP_ACCOUNT);

	if (!enabler)
		return false;

	enabler->event = user_event_get(orig->event);
	enabler->addr = orig->addr;

	 
	enabler->values = orig->values & ENABLE_VAL_DUP_MASK;

	 
	list_add(&enabler->mm_enablers_link, &mm->enablers);

	return true;
}

static struct user_event_mm *user_event_mm_get(struct user_event_mm *mm)
{
	refcount_inc(&mm->refcnt);

	return mm;
}

static struct user_event_mm *user_event_mm_get_all(struct user_event *user)
{
	struct user_event_mm *found = NULL;
	struct user_event_enabler *enabler;
	struct user_event_mm *mm;

	 
	lockdep_assert_held(&event_mutex);

	 
	rcu_read_lock();

	list_for_each_entry_rcu(mm, &user_event_mms, mms_link) {
		list_for_each_entry_rcu(enabler, &mm->enablers, mm_enablers_link) {
			if (enabler->event == user) {
				mm->next = found;
				found = user_event_mm_get(mm);
				break;
			}
		}
	}

	rcu_read_unlock();

	return found;
}

static struct user_event_mm *user_event_mm_alloc(struct task_struct *t)
{
	struct user_event_mm *user_mm;

	user_mm = kzalloc(sizeof(*user_mm), GFP_KERNEL_ACCOUNT);

	if (!user_mm)
		return NULL;

	user_mm->mm = t->mm;
	INIT_LIST_HEAD(&user_mm->enablers);
	refcount_set(&user_mm->refcnt, 1);
	refcount_set(&user_mm->tasks, 1);

	 
	mmgrab(user_mm->mm);

	return user_mm;
}

static void user_event_mm_attach(struct user_event_mm *user_mm, struct task_struct *t)
{
	unsigned long flags;

	spin_lock_irqsave(&user_event_mms_lock, flags);
	list_add_rcu(&user_mm->mms_link, &user_event_mms);
	spin_unlock_irqrestore(&user_event_mms_lock, flags);

	t->user_event_mm = user_mm;
}

static struct user_event_mm *current_user_event_mm(void)
{
	struct user_event_mm *user_mm = current->user_event_mm;

	if (user_mm)
		goto inc;

	user_mm = user_event_mm_alloc(current);

	if (!user_mm)
		goto error;

	user_event_mm_attach(user_mm, current);
inc:
	refcount_inc(&user_mm->refcnt);
error:
	return user_mm;
}

static void user_event_mm_destroy(struct user_event_mm *mm)
{
	struct user_event_enabler *enabler, *next;

	list_for_each_entry_safe(enabler, next, &mm->enablers, mm_enablers_link)
		user_event_enabler_destroy(enabler, false);

	mmdrop(mm->mm);
	kfree(mm);
}

static void user_event_mm_put(struct user_event_mm *mm)
{
	if (mm && refcount_dec_and_test(&mm->refcnt))
		user_event_mm_destroy(mm);
}

static void delayed_user_event_mm_put(struct work_struct *work)
{
	struct user_event_mm *mm;

	mm = container_of(to_rcu_work(work), struct user_event_mm, put_rwork);
	user_event_mm_put(mm);
}

void user_event_mm_remove(struct task_struct *t)
{
	struct user_event_mm *mm;
	unsigned long flags;

	might_sleep();

	mm = t->user_event_mm;
	t->user_event_mm = NULL;

	 
	if (!refcount_dec_and_test(&mm->tasks))
		return;

	 
	spin_lock_irqsave(&user_event_mms_lock, flags);
	list_del_rcu(&mm->mms_link);
	spin_unlock_irqrestore(&user_event_mms_lock, flags);

	 
	mmap_write_lock(mm->mm);
	mmap_write_unlock(mm->mm);

	 
	INIT_RCU_WORK(&mm->put_rwork, delayed_user_event_mm_put);
	queue_rcu_work(system_wq, &mm->put_rwork);
}

void user_event_mm_dup(struct task_struct *t, struct user_event_mm *old_mm)
{
	struct user_event_mm *mm = user_event_mm_alloc(t);
	struct user_event_enabler *enabler;

	if (!mm)
		return;

	rcu_read_lock();

	list_for_each_entry_rcu(enabler, &old_mm->enablers, mm_enablers_link) {
		if (!user_event_enabler_dup(enabler, mm))
			goto error;
	}

	rcu_read_unlock();

	user_event_mm_attach(mm, t);
	return;
error:
	rcu_read_unlock();
	user_event_mm_destroy(mm);
}

static bool current_user_event_enabler_exists(unsigned long uaddr,
					      unsigned char bit)
{
	struct user_event_mm *user_mm = current_user_event_mm();
	bool exists;

	if (!user_mm)
		return false;

	exists = user_event_enabler_exists(user_mm, uaddr, bit);

	user_event_mm_put(user_mm);

	return exists;
}

static struct user_event_enabler
*user_event_enabler_create(struct user_reg *reg, struct user_event *user,
			   int *write_result)
{
	struct user_event_enabler *enabler;
	struct user_event_mm *user_mm;
	unsigned long uaddr = (unsigned long)reg->enable_addr;
	int attempt = 0;

	user_mm = current_user_event_mm();

	if (!user_mm)
		return NULL;

	enabler = kzalloc(sizeof(*enabler), GFP_KERNEL_ACCOUNT);

	if (!enabler)
		goto out;

	enabler->event = user;
	enabler->addr = uaddr;
	enabler->values = reg->enable_bit;

#if BITS_PER_LONG >= 64
	if (reg->enable_size == 4)
		set_bit(ENABLE_VAL_32_ON_64_BIT, ENABLE_BITOPS(enabler));
#endif

retry:
	 
	mutex_lock(&event_mutex);

	 
	mmap_read_lock(user_mm->mm);
	*write_result = user_event_enabler_write(user_mm, enabler, false,
						 &attempt);
	mmap_read_unlock(user_mm->mm);

	 
	if (!*write_result) {
		user_event_get(user);
		list_add_rcu(&enabler->mm_enablers_link, &user_mm->enablers);
	}

	mutex_unlock(&event_mutex);

	if (*write_result) {
		 
		if (!user_event_mm_fault_in(user_mm, uaddr, attempt))
			goto retry;

		kfree(enabler);
		enabler = NULL;
	}
out:
	user_event_mm_put(user_mm);

	return enabler;
}

static __always_inline __must_check
bool user_event_last_ref(struct user_event *user)
{
	int last = 0;

	if (user->reg_flags & USER_EVENT_REG_PERSIST)
		last = 1;

	return refcount_read(&user->refcnt) == last;
}

static __always_inline __must_check
size_t copy_nofault(void *addr, size_t bytes, struct iov_iter *i)
{
	size_t ret;

	pagefault_disable();

	ret = copy_from_iter_nocache(addr, bytes, i);

	pagefault_enable();

	return ret;
}

static struct list_head *user_event_get_fields(struct trace_event_call *call)
{
	struct user_event *user = (struct user_event *)call->data;

	return &user->fields;
}

 
static int user_event_parse_cmd(struct user_event_group *group,
				char *raw_command, struct user_event **newuser,
				int reg_flags)
{
	char *name = raw_command;
	char *args = strpbrk(name, " ");
	char *flags;

	if (args)
		*args++ = '\0';

	flags = strpbrk(name, ":");

	if (flags)
		*flags++ = '\0';

	return user_event_parse(group, name, args, flags, newuser, reg_flags);
}

static int user_field_array_size(const char *type)
{
	const char *start = strchr(type, '[');
	char val[8];
	char *bracket;
	int size = 0;

	if (start == NULL)
		return -EINVAL;

	if (strscpy(val, start + 1, sizeof(val)) <= 0)
		return -EINVAL;

	bracket = strchr(val, ']');

	if (!bracket)
		return -EINVAL;

	*bracket = '\0';

	if (kstrtouint(val, 0, &size))
		return -EINVAL;

	if (size > MAX_FIELD_ARRAY_SIZE)
		return -EINVAL;

	return size;
}

static int user_field_size(const char *type)
{
	 
	if (strcmp(type, "s64") == 0)
		return sizeof(s64);
	if (strcmp(type, "u64") == 0)
		return sizeof(u64);
	if (strcmp(type, "s32") == 0)
		return sizeof(s32);
	if (strcmp(type, "u32") == 0)
		return sizeof(u32);
	if (strcmp(type, "int") == 0)
		return sizeof(int);
	if (strcmp(type, "unsigned int") == 0)
		return sizeof(unsigned int);
	if (strcmp(type, "s16") == 0)
		return sizeof(s16);
	if (strcmp(type, "u16") == 0)
		return sizeof(u16);
	if (strcmp(type, "short") == 0)
		return sizeof(short);
	if (strcmp(type, "unsigned short") == 0)
		return sizeof(unsigned short);
	if (strcmp(type, "s8") == 0)
		return sizeof(s8);
	if (strcmp(type, "u8") == 0)
		return sizeof(u8);
	if (strcmp(type, "char") == 0)
		return sizeof(char);
	if (strcmp(type, "unsigned char") == 0)
		return sizeof(unsigned char);
	if (str_has_prefix(type, "char["))
		return user_field_array_size(type);
	if (str_has_prefix(type, "unsigned char["))
		return user_field_array_size(type);
	if (str_has_prefix(type, "__data_loc "))
		return sizeof(u32);
	if (str_has_prefix(type, "__rel_loc "))
		return sizeof(u32);

	 
	return -EINVAL;
}

static void user_event_destroy_validators(struct user_event *user)
{
	struct user_event_validator *validator, *next;
	struct list_head *head = &user->validators;

	list_for_each_entry_safe(validator, next, head, user_event_link) {
		list_del(&validator->user_event_link);
		kfree(validator);
	}
}

static void user_event_destroy_fields(struct user_event *user)
{
	struct ftrace_event_field *field, *next;
	struct list_head *head = &user->fields;

	list_for_each_entry_safe(field, next, head, link) {
		list_del(&field->link);
		kfree(field);
	}
}

static int user_event_add_field(struct user_event *user, const char *type,
				const char *name, int offset, int size,
				int is_signed, int filter_type)
{
	struct user_event_validator *validator;
	struct ftrace_event_field *field;
	int validator_flags = 0;

	field = kmalloc(sizeof(*field), GFP_KERNEL_ACCOUNT);

	if (!field)
		return -ENOMEM;

	if (str_has_prefix(type, "__data_loc "))
		goto add_validator;

	if (str_has_prefix(type, "__rel_loc ")) {
		validator_flags |= VALIDATOR_REL;
		goto add_validator;
	}

	goto add_field;

add_validator:
	if (strstr(type, "char") != NULL)
		validator_flags |= VALIDATOR_ENSURE_NULL;

	validator = kmalloc(sizeof(*validator), GFP_KERNEL_ACCOUNT);

	if (!validator) {
		kfree(field);
		return -ENOMEM;
	}

	validator->flags = validator_flags;
	validator->offset = offset;

	 
	list_add_tail(&validator->user_event_link, &user->validators);

add_field:
	field->type = type;
	field->name = name;
	field->offset = offset;
	field->size = size;
	field->is_signed = is_signed;
	field->filter_type = filter_type;

	if (filter_type == FILTER_OTHER)
		field->filter_type = filter_assign_type(type);

	list_add(&field->link, &user->fields);

	 
	user->min_size = (offset + size) - sizeof(struct trace_entry);

	return 0;
}

 
static int user_event_parse_field(char *field, struct user_event *user,
				  u32 *offset)
{
	char *part, *type, *name;
	u32 depth = 0, saved_offset = *offset;
	int len, size = -EINVAL;
	bool is_struct = false;

	field = skip_spaces(field);

	if (*field == '\0')
		return 0;

	 
	len = str_has_prefix(field, "unsigned ");
	if (len)
		goto skip_next;

	len = str_has_prefix(field, "struct ");
	if (len) {
		is_struct = true;
		goto skip_next;
	}

	len = str_has_prefix(field, "__data_loc unsigned ");
	if (len)
		goto skip_next;

	len = str_has_prefix(field, "__data_loc ");
	if (len)
		goto skip_next;

	len = str_has_prefix(field, "__rel_loc unsigned ");
	if (len)
		goto skip_next;

	len = str_has_prefix(field, "__rel_loc ");
	if (len)
		goto skip_next;

	goto parse;
skip_next:
	type = field;
	field = strpbrk(field + len, " ");

	if (field == NULL)
		return -EINVAL;

	*field++ = '\0';
	depth++;
parse:
	name = NULL;

	while ((part = strsep(&field, " ")) != NULL) {
		switch (depth++) {
		case FIELD_DEPTH_TYPE:
			type = part;
			break;
		case FIELD_DEPTH_NAME:
			name = part;
			break;
		case FIELD_DEPTH_SIZE:
			if (!is_struct)
				return -EINVAL;

			if (kstrtou32(part, 10, &size))
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}
	}

	if (depth < FIELD_DEPTH_SIZE || !name)
		return -EINVAL;

	if (depth == FIELD_DEPTH_SIZE)
		size = user_field_size(type);

	if (size == 0)
		return -EINVAL;

	if (size < 0)
		return size;

	*offset = saved_offset + size;

	return user_event_add_field(user, type, name, saved_offset, size,
				    type[0] != 'u', FILTER_OTHER);
}

static int user_event_parse_fields(struct user_event *user, char *args)
{
	char *field;
	u32 offset = sizeof(struct trace_entry);
	int ret = -EINVAL;

	if (args == NULL)
		return 0;

	while ((field = strsep(&args, ";")) != NULL) {
		ret = user_event_parse_field(field, user, &offset);

		if (ret)
			break;
	}

	return ret;
}

static struct trace_event_fields user_event_fields_array[1];

static const char *user_field_format(const char *type)
{
	if (strcmp(type, "s64") == 0)
		return "%lld";
	if (strcmp(type, "u64") == 0)
		return "%llu";
	if (strcmp(type, "s32") == 0)
		return "%d";
	if (strcmp(type, "u32") == 0)
		return "%u";
	if (strcmp(type, "int") == 0)
		return "%d";
	if (strcmp(type, "unsigned int") == 0)
		return "%u";
	if (strcmp(type, "s16") == 0)
		return "%d";
	if (strcmp(type, "u16") == 0)
		return "%u";
	if (strcmp(type, "short") == 0)
		return "%d";
	if (strcmp(type, "unsigned short") == 0)
		return "%u";
	if (strcmp(type, "s8") == 0)
		return "%d";
	if (strcmp(type, "u8") == 0)
		return "%u";
	if (strcmp(type, "char") == 0)
		return "%d";
	if (strcmp(type, "unsigned char") == 0)
		return "%u";
	if (strstr(type, "char[") != NULL)
		return "%s";

	 
	return "%llu";
}

static bool user_field_is_dyn_string(const char *type, const char **str_func)
{
	if (str_has_prefix(type, "__data_loc ")) {
		*str_func = "__get_str";
		goto check;
	}

	if (str_has_prefix(type, "__rel_loc ")) {
		*str_func = "__get_rel_str";
		goto check;
	}

	return false;
check:
	return strstr(type, "char") != NULL;
}

#define LEN_OR_ZERO (len ? len - pos : 0)
static int user_dyn_field_set_string(int argc, const char **argv, int *iout,
				     char *buf, int len, bool *colon)
{
	int pos = 0, i = *iout;

	*colon = false;

	for (; i < argc; ++i) {
		if (i != *iout)
			pos += snprintf(buf + pos, LEN_OR_ZERO, " ");

		pos += snprintf(buf + pos, LEN_OR_ZERO, "%s", argv[i]);

		if (strchr(argv[i], ';')) {
			++i;
			*colon = true;
			break;
		}
	}

	 
	if (len != 0)
		*iout = i;

	return pos + 1;
}

static int user_field_set_string(struct ftrace_event_field *field,
				 char *buf, int len, bool colon)
{
	int pos = 0;

	pos += snprintf(buf + pos, LEN_OR_ZERO, "%s", field->type);
	pos += snprintf(buf + pos, LEN_OR_ZERO, " ");
	pos += snprintf(buf + pos, LEN_OR_ZERO, "%s", field->name);

	if (str_has_prefix(field->type, "struct "))
		pos += snprintf(buf + pos, LEN_OR_ZERO, " %d", field->size);

	if (colon)
		pos += snprintf(buf + pos, LEN_OR_ZERO, ";");

	return pos + 1;
}

static int user_event_set_print_fmt(struct user_event *user, char *buf, int len)
{
	struct ftrace_event_field *field;
	struct list_head *head = &user->fields;
	int pos = 0, depth = 0;
	const char *str_func;

	pos += snprintf(buf + pos, LEN_OR_ZERO, "\"");

	list_for_each_entry_reverse(field, head, link) {
		if (depth != 0)
			pos += snprintf(buf + pos, LEN_OR_ZERO, " ");

		pos += snprintf(buf + pos, LEN_OR_ZERO, "%s=%s",
				field->name, user_field_format(field->type));

		depth++;
	}

	pos += snprintf(buf + pos, LEN_OR_ZERO, "\"");

	list_for_each_entry_reverse(field, head, link) {
		if (user_field_is_dyn_string(field->type, &str_func))
			pos += snprintf(buf + pos, LEN_OR_ZERO,
					", %s(%s)", str_func, field->name);
		else
			pos += snprintf(buf + pos, LEN_OR_ZERO,
					", REC->%s", field->name);
	}

	return pos + 1;
}
#undef LEN_OR_ZERO

static int user_event_create_print_fmt(struct user_event *user)
{
	char *print_fmt;
	int len;

	len = user_event_set_print_fmt(user, NULL, 0);

	print_fmt = kmalloc(len, GFP_KERNEL_ACCOUNT);

	if (!print_fmt)
		return -ENOMEM;

	user_event_set_print_fmt(user, print_fmt, len);

	user->call.print_fmt = print_fmt;

	return 0;
}

static enum print_line_t user_event_print_trace(struct trace_iterator *iter,
						int flags,
						struct trace_event *event)
{
	return print_event_fields(iter, event);
}

static struct trace_event_functions user_event_funcs = {
	.trace = user_event_print_trace,
};

static int user_event_set_call_visible(struct user_event *user, bool visible)
{
	int ret;
	const struct cred *old_cred;
	struct cred *cred;

	cred = prepare_creds();

	if (!cred)
		return -ENOMEM;

	 
	cred->fsuid = GLOBAL_ROOT_UID;

	old_cred = override_creds(cred);

	if (visible)
		ret = trace_add_event_call(&user->call);
	else
		ret = trace_remove_event_call(&user->call);

	revert_creds(old_cred);
	put_cred(cred);

	return ret;
}

static int destroy_user_event(struct user_event *user)
{
	int ret = 0;

	lockdep_assert_held(&event_mutex);

	 
	user_event_destroy_fields(user);

	ret = user_event_set_call_visible(user, false);

	if (ret)
		return ret;

	dyn_event_remove(&user->devent);
	hash_del(&user->node);

	user_event_destroy_validators(user);
	kfree(user->call.print_fmt);
	kfree(EVENT_NAME(user));
	kfree(user);

	if (current_user_events > 0)
		current_user_events--;
	else
		pr_alert("BUG: Bad current_user_events\n");

	return ret;
}

static struct user_event *find_user_event(struct user_event_group *group,
					  char *name, u32 *outkey)
{
	struct user_event *user;
	u32 key = user_event_key(name);

	*outkey = key;

	hash_for_each_possible(group->register_table, user, node, key)
		if (!strcmp(EVENT_NAME(user), name))
			return user_event_get(user);

	return NULL;
}

static int user_event_validate(struct user_event *user, void *data, int len)
{
	struct list_head *head = &user->validators;
	struct user_event_validator *validator;
	void *pos, *end = data + len;
	u32 loc, offset, size;

	list_for_each_entry(validator, head, user_event_link) {
		pos = data + validator->offset;

		 
		loc = *(u32 *)pos;
		offset = loc & 0xffff;
		size = loc >> 16;

		if (likely(validator->flags & VALIDATOR_REL))
			pos += offset + sizeof(loc);
		else
			pos = data + offset;

		pos += size;

		if (unlikely(pos > end))
			return -EFAULT;

		if (likely(validator->flags & VALIDATOR_ENSURE_NULL))
			if (unlikely(*(char *)(pos - 1) != '\0'))
				return -EFAULT;
	}

	return 0;
}

 
static void user_event_ftrace(struct user_event *user, struct iov_iter *i,
			      void *tpdata, bool *faulted)
{
	struct trace_event_file *file;
	struct trace_entry *entry;
	struct trace_event_buffer event_buffer;
	size_t size = sizeof(*entry) + i->count;

	file = (struct trace_event_file *)tpdata;

	if (!file ||
	    !(file->flags & EVENT_FILE_FL_ENABLED) ||
	    trace_trigger_soft_disabled(file))
		return;

	 
	entry = trace_event_buffer_reserve(&event_buffer, file, size);

	if (unlikely(!entry))
		return;

	if (unlikely(i->count != 0 && !copy_nofault(entry + 1, i->count, i)))
		goto discard;

	if (!list_empty(&user->validators) &&
	    unlikely(user_event_validate(user, entry, size)))
		goto discard;

	trace_event_buffer_commit(&event_buffer);

	return;
discard:
	*faulted = true;
	__trace_event_discard_commit(event_buffer.buffer,
				     event_buffer.event);
}

#ifdef CONFIG_PERF_EVENTS
 
static void user_event_perf(struct user_event *user, struct iov_iter *i,
			    void *tpdata, bool *faulted)
{
	struct hlist_head *perf_head;

	perf_head = this_cpu_ptr(user->call.perf_events);

	if (perf_head && !hlist_empty(perf_head)) {
		struct trace_entry *perf_entry;
		struct pt_regs *regs;
		size_t size = sizeof(*perf_entry) + i->count;
		int context;

		perf_entry = perf_trace_buf_alloc(ALIGN(size, 8),
						  &regs, &context);

		if (unlikely(!perf_entry))
			return;

		perf_fetch_caller_regs(regs);

		if (unlikely(i->count != 0 && !copy_nofault(perf_entry + 1, i->count, i)))
			goto discard;

		if (!list_empty(&user->validators) &&
		    unlikely(user_event_validate(user, perf_entry, size)))
			goto discard;

		perf_trace_buf_submit(perf_entry, size, context,
				      user->call.event.type, 1, regs,
				      perf_head, NULL);

		return;
discard:
		*faulted = true;
		perf_swevent_put_recursion_context(context);
	}
}
#endif

 
static void update_enable_bit_for(struct user_event *user)
{
	struct tracepoint *tp = &user->tracepoint;
	char status = 0;

	if (atomic_read(&tp->key.enabled) > 0) {
		struct tracepoint_func *probe_func_ptr;
		user_event_func_t probe_func;

		rcu_read_lock_sched();

		probe_func_ptr = rcu_dereference_sched(tp->funcs);

		if (probe_func_ptr) {
			do {
				probe_func = probe_func_ptr->func;

				if (probe_func == user_event_ftrace)
					status |= EVENT_STATUS_FTRACE;
#ifdef CONFIG_PERF_EVENTS
				else if (probe_func == user_event_perf)
					status |= EVENT_STATUS_PERF;
#endif
				else
					status |= EVENT_STATUS_OTHER;
			} while ((++probe_func_ptr)->func);
		}

		rcu_read_unlock_sched();
	}

	user->status = status;

	user_event_enabler_update(user);
}

 
static int user_event_reg(struct trace_event_call *call,
			  enum trace_reg type,
			  void *data)
{
	struct user_event *user = (struct user_event *)call->data;
	int ret = 0;

	if (!user)
		return -ENOENT;

	switch (type) {
	case TRACE_REG_REGISTER:
		ret = tracepoint_probe_register(call->tp,
						call->class->probe,
						data);
		if (!ret)
			goto inc;
		break;

	case TRACE_REG_UNREGISTER:
		tracepoint_probe_unregister(call->tp,
					    call->class->probe,
					    data);
		goto dec;

#ifdef CONFIG_PERF_EVENTS
	case TRACE_REG_PERF_REGISTER:
		ret = tracepoint_probe_register(call->tp,
						call->class->perf_probe,
						data);
		if (!ret)
			goto inc;
		break;

	case TRACE_REG_PERF_UNREGISTER:
		tracepoint_probe_unregister(call->tp,
					    call->class->perf_probe,
					    data);
		goto dec;

	case TRACE_REG_PERF_OPEN:
	case TRACE_REG_PERF_CLOSE:
	case TRACE_REG_PERF_ADD:
	case TRACE_REG_PERF_DEL:
		break;
#endif
	}

	return ret;
inc:
	user_event_get(user);
	update_enable_bit_for(user);
	return 0;
dec:
	update_enable_bit_for(user);
	user_event_put(user, true);
	return 0;
}

static int user_event_create(const char *raw_command)
{
	struct user_event_group *group;
	struct user_event *user;
	char *name;
	int ret;

	if (!str_has_prefix(raw_command, USER_EVENTS_PREFIX))
		return -ECANCELED;

	raw_command += USER_EVENTS_PREFIX_LEN;
	raw_command = skip_spaces(raw_command);

	name = kstrdup(raw_command, GFP_KERNEL_ACCOUNT);

	if (!name)
		return -ENOMEM;

	group = current_user_event_group();

	if (!group) {
		kfree(name);
		return -ENOENT;
	}

	mutex_lock(&group->reg_mutex);

	 
	ret = user_event_parse_cmd(group, name, &user, USER_EVENT_REG_PERSIST);

	if (!ret)
		user_event_put(user, false);

	mutex_unlock(&group->reg_mutex);

	if (ret)
		kfree(name);

	return ret;
}

static int user_event_show(struct seq_file *m, struct dyn_event *ev)
{
	struct user_event *user = container_of(ev, struct user_event, devent);
	struct ftrace_event_field *field;
	struct list_head *head;
	int depth = 0;

	seq_printf(m, "%s%s", USER_EVENTS_PREFIX, EVENT_NAME(user));

	head = trace_get_fields(&user->call);

	list_for_each_entry_reverse(field, head, link) {
		if (depth == 0)
			seq_puts(m, " ");
		else
			seq_puts(m, "; ");

		seq_printf(m, "%s %s", field->type, field->name);

		if (str_has_prefix(field->type, "struct "))
			seq_printf(m, " %d", field->size);

		depth++;
	}

	seq_puts(m, "\n");

	return 0;
}

static bool user_event_is_busy(struct dyn_event *ev)
{
	struct user_event *user = container_of(ev, struct user_event, devent);

	return !user_event_last_ref(user);
}

static int user_event_free(struct dyn_event *ev)
{
	struct user_event *user = container_of(ev, struct user_event, devent);

	if (!user_event_last_ref(user))
		return -EBUSY;

	return destroy_user_event(user);
}

static bool user_field_match(struct ftrace_event_field *field, int argc,
			     const char **argv, int *iout)
{
	char *field_name = NULL, *dyn_field_name = NULL;
	bool colon = false, match = false;
	int dyn_len, len;

	if (*iout >= argc)
		return false;

	dyn_len = user_dyn_field_set_string(argc, argv, iout, dyn_field_name,
					    0, &colon);

	len = user_field_set_string(field, field_name, 0, colon);

	if (dyn_len != len)
		return false;

	dyn_field_name = kmalloc(dyn_len, GFP_KERNEL);
	field_name = kmalloc(len, GFP_KERNEL);

	if (!dyn_field_name || !field_name)
		goto out;

	user_dyn_field_set_string(argc, argv, iout, dyn_field_name,
				  dyn_len, &colon);

	user_field_set_string(field, field_name, len, colon);

	match = strcmp(dyn_field_name, field_name) == 0;
out:
	kfree(dyn_field_name);
	kfree(field_name);

	return match;
}

static bool user_fields_match(struct user_event *user, int argc,
			      const char **argv)
{
	struct ftrace_event_field *field;
	struct list_head *head = &user->fields;
	int i = 0;

	list_for_each_entry_reverse(field, head, link) {
		if (!user_field_match(field, argc, argv, &i))
			return false;
	}

	if (i != argc)
		return false;

	return true;
}

static bool user_event_match(const char *system, const char *event,
			     int argc, const char **argv, struct dyn_event *ev)
{
	struct user_event *user = container_of(ev, struct user_event, devent);
	bool match;

	match = strcmp(EVENT_NAME(user), event) == 0 &&
		(!system || strcmp(system, USER_EVENTS_SYSTEM) == 0);

	if (match && argc > 0)
		match = user_fields_match(user, argc, argv);
	else if (match && argc == 0)
		match = list_empty(&user->fields);

	return match;
}

static struct dyn_event_operations user_event_dops = {
	.create = user_event_create,
	.show = user_event_show,
	.is_busy = user_event_is_busy,
	.free = user_event_free,
	.match = user_event_match,
};

static int user_event_trace_register(struct user_event *user)
{
	int ret;

	ret = register_trace_event(&user->call.event);

	if (!ret)
		return -ENODEV;

	ret = user_event_set_call_visible(user, true);

	if (ret)
		unregister_trace_event(&user->call.event);

	return ret;
}

 
static int user_event_parse(struct user_event_group *group, char *name,
			    char *args, char *flags,
			    struct user_event **newuser, int reg_flags)
{
	int ret;
	u32 key;
	struct user_event *user;
	int argc = 0;
	char **argv;

	 
	if (reg_flags != 0 || flags != NULL)
		return -EINVAL;

	 
	mutex_lock(&event_mutex);
	user = find_user_event(group, name, &key);
	mutex_unlock(&event_mutex);

	if (user) {
		if (args) {
			argv = argv_split(GFP_KERNEL, args, &argc);
			if (!argv) {
				ret = -ENOMEM;
				goto error;
			}

			ret = user_fields_match(user, argc, (const char **)argv);
			argv_free(argv);

		} else
			ret = list_empty(&user->fields);

		if (ret) {
			*newuser = user;
			 
			kfree(name);
		} else {
			ret = -EADDRINUSE;
			goto error;
		}

		return 0;
error:
		user_event_put(user, false);
		return ret;
	}

	user = kzalloc(sizeof(*user), GFP_KERNEL_ACCOUNT);

	if (!user)
		return -ENOMEM;

	INIT_LIST_HEAD(&user->class.fields);
	INIT_LIST_HEAD(&user->fields);
	INIT_LIST_HEAD(&user->validators);

	user->group = group;
	user->tracepoint.name = name;

	ret = user_event_parse_fields(user, args);

	if (ret)
		goto put_user;

	ret = user_event_create_print_fmt(user);

	if (ret)
		goto put_user;

	user->call.data = user;
	user->call.class = &user->class;
	user->call.name = name;
	user->call.flags = TRACE_EVENT_FL_TRACEPOINT;
	user->call.tp = &user->tracepoint;
	user->call.event.funcs = &user_event_funcs;
	user->class.system = group->system_name;

	user->class.fields_array = user_event_fields_array;
	user->class.get_fields = user_event_get_fields;
	user->class.reg = user_event_reg;
	user->class.probe = user_event_ftrace;
#ifdef CONFIG_PERF_EVENTS
	user->class.perf_probe = user_event_perf;
#endif

	mutex_lock(&event_mutex);

	if (current_user_events >= max_user_events) {
		ret = -EMFILE;
		goto put_user_lock;
	}

	ret = user_event_trace_register(user);

	if (ret)
		goto put_user_lock;

	user->reg_flags = reg_flags;

	if (user->reg_flags & USER_EVENT_REG_PERSIST) {
		 
		refcount_set(&user->refcnt, 2);
	} else {
		 
		refcount_set(&user->refcnt, 1);
	}

	dyn_event_init(&user->devent, &user_event_dops);
	dyn_event_add(&user->devent, &user->call);
	hash_add(group->register_table, &user->node, key);
	current_user_events++;

	mutex_unlock(&event_mutex);

	*newuser = user;
	return 0;
put_user_lock:
	mutex_unlock(&event_mutex);
put_user:
	user_event_destroy_fields(user);
	user_event_destroy_validators(user);
	kfree(user->call.print_fmt);
	kfree(user);
	return ret;
}

 
static int delete_user_event(struct user_event_group *group, char *name)
{
	u32 key;
	struct user_event *user = find_user_event(group, name, &key);

	if (!user)
		return -ENOENT;

	user_event_put(user, true);

	if (!user_event_last_ref(user))
		return -EBUSY;

	return destroy_user_event(user);
}

 
static ssize_t user_events_write_core(struct file *file, struct iov_iter *i)
{
	struct user_event_file_info *info = file->private_data;
	struct user_event_refs *refs;
	struct user_event *user = NULL;
	struct tracepoint *tp;
	ssize_t ret = i->count;
	int idx;

	if (unlikely(copy_from_iter(&idx, sizeof(idx), i) != sizeof(idx)))
		return -EFAULT;

	if (idx < 0)
		return -EINVAL;

	rcu_read_lock_sched();

	refs = rcu_dereference_sched(info->refs);

	 
	if (likely(refs && idx < refs->count))
		user = refs->events[idx];

	rcu_read_unlock_sched();

	if (unlikely(user == NULL))
		return -ENOENT;

	if (unlikely(i->count < user->min_size))
		return -EINVAL;

	tp = &user->tracepoint;

	 
	if (likely(atomic_read(&tp->key.enabled) > 0)) {
		struct tracepoint_func *probe_func_ptr;
		user_event_func_t probe_func;
		struct iov_iter copy;
		void *tpdata;
		bool faulted;

		if (unlikely(fault_in_iov_iter_readable(i, i->count)))
			return -EFAULT;

		faulted = false;

		rcu_read_lock_sched();

		probe_func_ptr = rcu_dereference_sched(tp->funcs);

		if (probe_func_ptr) {
			do {
				copy = *i;
				probe_func = probe_func_ptr->func;
				tpdata = probe_func_ptr->data;
				probe_func(user, &copy, tpdata, &faulted);
			} while ((++probe_func_ptr)->func);
		}

		rcu_read_unlock_sched();

		if (unlikely(faulted))
			return -EFAULT;
	} else
		return -EBADF;

	return ret;
}

static int user_events_open(struct inode *node, struct file *file)
{
	struct user_event_group *group;
	struct user_event_file_info *info;

	group = current_user_event_group();

	if (!group)
		return -ENOENT;

	info = kzalloc(sizeof(*info), GFP_KERNEL_ACCOUNT);

	if (!info)
		return -ENOMEM;

	info->group = group;

	file->private_data = info;

	return 0;
}

static ssize_t user_events_write(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct iovec iov;
	struct iov_iter i;

	if (unlikely(*ppos != 0))
		return -EFAULT;

	if (unlikely(import_single_range(ITER_SOURCE, (char __user *)ubuf,
					 count, &iov, &i)))
		return -EFAULT;

	return user_events_write_core(file, &i);
}

static ssize_t user_events_write_iter(struct kiocb *kp, struct iov_iter *i)
{
	return user_events_write_core(kp->ki_filp, i);
}

static int user_events_ref_add(struct user_event_file_info *info,
			       struct user_event *user)
{
	struct user_event_group *group = info->group;
	struct user_event_refs *refs, *new_refs;
	int i, size, count = 0;

	refs = rcu_dereference_protected(info->refs,
					 lockdep_is_held(&group->reg_mutex));

	if (refs) {
		count = refs->count;

		for (i = 0; i < count; ++i)
			if (refs->events[i] == user)
				return i;
	}

	size = struct_size(refs, events, count + 1);

	new_refs = kzalloc(size, GFP_KERNEL_ACCOUNT);

	if (!new_refs)
		return -ENOMEM;

	new_refs->count = count + 1;

	for (i = 0; i < count; ++i)
		new_refs->events[i] = refs->events[i];

	new_refs->events[i] = user_event_get(user);

	rcu_assign_pointer(info->refs, new_refs);

	if (refs)
		kfree_rcu(refs, rcu);

	return i;
}

static long user_reg_get(struct user_reg __user *ureg, struct user_reg *kreg)
{
	u32 size;
	long ret;

	ret = get_user(size, &ureg->size);

	if (ret)
		return ret;

	if (size > PAGE_SIZE)
		return -E2BIG;

	if (size < offsetofend(struct user_reg, write_index))
		return -EINVAL;

	ret = copy_struct_from_user(kreg, sizeof(*kreg), ureg, size);

	if (ret)
		return ret;

	 
	if (kreg->flags & ~(USER_EVENT_REG_MAX-1))
		return -EINVAL;

	 
	switch (kreg->enable_size) {
	case 4:
		 
		break;
#if BITS_PER_LONG >= 64
	case 8:
		 
		break;
#endif
	default:
		return -EINVAL;
	}

	 
	if (kreg->enable_addr % kreg->enable_size)
		return -EINVAL;

	 
	if (kreg->enable_bit > (kreg->enable_size * BITS_PER_BYTE) - 1)
		return -EINVAL;

	 
	if (!access_ok((const void __user *)(uintptr_t)kreg->enable_addr,
		       kreg->enable_size))
		return -EFAULT;

	kreg->size = size;

	return 0;
}

 
static long user_events_ioctl_reg(struct user_event_file_info *info,
				  unsigned long uarg)
{
	struct user_reg __user *ureg = (struct user_reg __user *)uarg;
	struct user_reg reg;
	struct user_event *user;
	struct user_event_enabler *enabler;
	char *name;
	long ret;
	int write_result;

	ret = user_reg_get(ureg, &reg);

	if (ret)
		return ret;

	 
	if (current_user_event_enabler_exists((unsigned long)reg.enable_addr,
					      reg.enable_bit))
		return -EADDRINUSE;

	name = strndup_user((const char __user *)(uintptr_t)reg.name_args,
			    MAX_EVENT_DESC);

	if (IS_ERR(name)) {
		ret = PTR_ERR(name);
		return ret;
	}

	ret = user_event_parse_cmd(info->group, name, &user, reg.flags);

	if (ret) {
		kfree(name);
		return ret;
	}

	ret = user_events_ref_add(info, user);

	 
	user_event_put(user, false);

	 
	if (ret < 0)
		return ret;

	 
	enabler = user_event_enabler_create(&reg, user, &write_result);

	if (!enabler)
		return -ENOMEM;

	 
	if (write_result)
		return write_result;

	put_user((u32)ret, &ureg->write_index);

	return 0;
}

 
static long user_events_ioctl_del(struct user_event_file_info *info,
				  unsigned long uarg)
{
	void __user *ubuf = (void __user *)uarg;
	char *name;
	long ret;

	name = strndup_user(ubuf, MAX_EVENT_DESC);

	if (IS_ERR(name))
		return PTR_ERR(name);

	 
	mutex_lock(&event_mutex);
	ret = delete_user_event(info->group, name);
	mutex_unlock(&event_mutex);

	kfree(name);

	return ret;
}

static long user_unreg_get(struct user_unreg __user *ureg,
			   struct user_unreg *kreg)
{
	u32 size;
	long ret;

	ret = get_user(size, &ureg->size);

	if (ret)
		return ret;

	if (size > PAGE_SIZE)
		return -E2BIG;

	if (size < offsetofend(struct user_unreg, disable_addr))
		return -EINVAL;

	ret = copy_struct_from_user(kreg, sizeof(*kreg), ureg, size);

	 
	if (kreg->__reserved || kreg->__reserved2)
		return -EINVAL;

	return ret;
}

static int user_event_mm_clear_bit(struct user_event_mm *user_mm,
				   unsigned long uaddr, unsigned char bit,
				   unsigned long flags)
{
	struct user_event_enabler enabler;
	int result;
	int attempt = 0;

	memset(&enabler, 0, sizeof(enabler));
	enabler.addr = uaddr;
	enabler.values = bit | flags;
retry:
	 
	mutex_lock(&event_mutex);

	 
	mmap_read_lock(user_mm->mm);
	result = user_event_enabler_write(user_mm, &enabler, false, &attempt);
	mmap_read_unlock(user_mm->mm);

	mutex_unlock(&event_mutex);

	if (result) {
		 
		if (!user_event_mm_fault_in(user_mm, uaddr, attempt))
			goto retry;
	}

	return result;
}

 
static long user_events_ioctl_unreg(unsigned long uarg)
{
	struct user_unreg __user *ureg = (struct user_unreg __user *)uarg;
	struct user_event_mm *mm = current->user_event_mm;
	struct user_event_enabler *enabler, *next;
	struct user_unreg reg;
	unsigned long flags;
	long ret;

	ret = user_unreg_get(ureg, &reg);

	if (ret)
		return ret;

	if (!mm)
		return -ENOENT;

	flags = 0;
	ret = -ENOENT;

	 
	mutex_lock(&event_mutex);

	list_for_each_entry_safe(enabler, next, &mm->enablers, mm_enablers_link) {
		if (enabler->addr == reg.disable_addr &&
		    ENABLE_BIT(enabler) == reg.disable_bit) {
			set_bit(ENABLE_VAL_FREEING_BIT, ENABLE_BITOPS(enabler));

			 
			flags |= enabler->values & ENABLE_VAL_COMPAT_MASK;

			if (!test_bit(ENABLE_VAL_FAULTING_BIT, ENABLE_BITOPS(enabler)))
				user_event_enabler_destroy(enabler, true);

			 
			ret = 0;
		}
	}

	mutex_unlock(&event_mutex);

	 
	if (!ret)
		ret = user_event_mm_clear_bit(mm, reg.disable_addr,
					      reg.disable_bit, flags);

	return ret;
}

 
static long user_events_ioctl(struct file *file, unsigned int cmd,
			      unsigned long uarg)
{
	struct user_event_file_info *info = file->private_data;
	struct user_event_group *group = info->group;
	long ret = -ENOTTY;

	switch (cmd) {
	case DIAG_IOCSREG:
		mutex_lock(&group->reg_mutex);
		ret = user_events_ioctl_reg(info, uarg);
		mutex_unlock(&group->reg_mutex);
		break;

	case DIAG_IOCSDEL:
		mutex_lock(&group->reg_mutex);
		ret = user_events_ioctl_del(info, uarg);
		mutex_unlock(&group->reg_mutex);
		break;

	case DIAG_IOCSUNREG:
		mutex_lock(&group->reg_mutex);
		ret = user_events_ioctl_unreg(uarg);
		mutex_unlock(&group->reg_mutex);
		break;
	}

	return ret;
}

 
static int user_events_release(struct inode *node, struct file *file)
{
	struct user_event_file_info *info = file->private_data;
	struct user_event_group *group;
	struct user_event_refs *refs;
	int i;

	if (!info)
		return -EINVAL;

	group = info->group;

	 
	mutex_lock(&group->reg_mutex);

	refs = info->refs;

	if (!refs)
		goto out;

	 
	for (i = 0; i < refs->count; ++i)
		user_event_put(refs->events[i], false);

out:
	file->private_data = NULL;

	mutex_unlock(&group->reg_mutex);

	kfree(refs);
	kfree(info);

	return 0;
}

static const struct file_operations user_data_fops = {
	.open		= user_events_open,
	.write		= user_events_write,
	.write_iter	= user_events_write_iter,
	.unlocked_ioctl	= user_events_ioctl,
	.release	= user_events_release,
};

static void *user_seq_start(struct seq_file *m, loff_t *pos)
{
	if (*pos)
		return NULL;

	return (void *)1;
}

static void *user_seq_next(struct seq_file *m, void *p, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void user_seq_stop(struct seq_file *m, void *p)
{
}

static int user_seq_show(struct seq_file *m, void *p)
{
	struct user_event_group *group = m->private;
	struct user_event *user;
	char status;
	int i, active = 0, busy = 0;

	if (!group)
		return -EINVAL;

	mutex_lock(&group->reg_mutex);

	hash_for_each(group->register_table, i, user, node) {
		status = user->status;

		seq_printf(m, "%s", EVENT_NAME(user));

		if (status != 0)
			seq_puts(m, " #");

		if (status != 0) {
			seq_puts(m, " Used by");
			if (status & EVENT_STATUS_FTRACE)
				seq_puts(m, " ftrace");
			if (status & EVENT_STATUS_PERF)
				seq_puts(m, " perf");
			if (status & EVENT_STATUS_OTHER)
				seq_puts(m, " other");
			busy++;
		}

		seq_puts(m, "\n");
		active++;
	}

	mutex_unlock(&group->reg_mutex);

	seq_puts(m, "\n");
	seq_printf(m, "Active: %d\n", active);
	seq_printf(m, "Busy: %d\n", busy);

	return 0;
}

static const struct seq_operations user_seq_ops = {
	.start	= user_seq_start,
	.next	= user_seq_next,
	.stop	= user_seq_stop,
	.show	= user_seq_show,
};

static int user_status_open(struct inode *node, struct file *file)
{
	struct user_event_group *group;
	int ret;

	group = current_user_event_group();

	if (!group)
		return -ENOENT;

	ret = seq_open(file, &user_seq_ops);

	if (!ret) {
		 
		struct seq_file *m = file->private_data;

		m->private = group;
	}

	return ret;
}

static const struct file_operations user_status_fops = {
	.open		= user_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

 
static int create_user_tracefs(void)
{
	struct dentry *edata, *emmap;

	edata = tracefs_create_file("user_events_data", TRACE_MODE_WRITE,
				    NULL, NULL, &user_data_fops);

	if (!edata) {
		pr_warn("Could not create tracefs 'user_events_data' entry\n");
		goto err;
	}

	emmap = tracefs_create_file("user_events_status", TRACE_MODE_READ,
				    NULL, NULL, &user_status_fops);

	if (!emmap) {
		tracefs_remove(edata);
		pr_warn("Could not create tracefs 'user_events_mmap' entry\n");
		goto err;
	}

	return 0;
err:
	return -ENODEV;
}

static int set_max_user_events_sysctl(struct ctl_table *table, int write,
				      void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	mutex_lock(&event_mutex);

	ret = proc_douintvec(table, write, buffer, lenp, ppos);

	mutex_unlock(&event_mutex);

	return ret;
}

static struct ctl_table user_event_sysctls[] = {
	{
		.procname	= "user_events_max",
		.data		= &max_user_events,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= set_max_user_events_sysctl,
	},
	{}
};

static int __init trace_events_user_init(void)
{
	int ret;

	fault_cache = KMEM_CACHE(user_event_enabler_fault, 0);

	if (!fault_cache)
		return -ENOMEM;

	init_group = user_event_group_create();

	if (!init_group) {
		kmem_cache_destroy(fault_cache);
		return -ENOMEM;
	}

	ret = create_user_tracefs();

	if (ret) {
		pr_warn("user_events could not register with tracefs\n");
		user_event_group_destroy(init_group);
		kmem_cache_destroy(fault_cache);
		init_group = NULL;
		return ret;
	}

	if (dyn_event_register(&user_event_dops))
		pr_warn("user_events could not register with dyn_events\n");

	register_sysctl_init("kernel", user_event_sysctls);

	return 0;
}

fs_initcall(trace_events_user_init);
