
#ifndef	_LINUX_TYPES_H_
#define	_LINUX_TYPES_H_

#include <linux/compiler.h>


#ifndef __bitwise__
#ifdef __CHECKER__
#define	__bitwise__ __attribute__((bitwise))
#else
#define	__bitwise__
#endif
#endif

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;
typedef uint64_t __le64;
typedef uint64_t __be64;

typedef unsigned gfp_t;
typedef off_t loff_t;
typedef vm_paddr_t resource_size_t;
typedef uint16_t __bitwise__ __sum16;
typedef unsigned long pgoff_t;
typedef unsigned __poll_t;

typedef uint64_t u64;
typedef u64 phys_addr_t;

typedef size_t __kernel_size_t;

#define	DECLARE_BITMAP(n, bits)						\
	unsigned long n[howmany(bits, sizeof (long) * 8)]

typedef unsigned long irq_hw_number_t;

struct rcu_head {
	void *raw[2];
} __aligned(sizeof (void *));

typedef void (*rcu_callback_t)(struct rcu_head *head);
typedef void (*call_rcu_func_t)(struct rcu_head *head, rcu_callback_t func);
typedef int linux_task_fn_t(void *data);

#endif	
