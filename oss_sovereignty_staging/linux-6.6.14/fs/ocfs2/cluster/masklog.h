 
 

#ifndef O2CLUSTER_MASKLOG_H
#define O2CLUSTER_MASKLOG_H

 

 
#include <linux/sched.h>

 
 
#define ML_TCP		0x0000000000000001ULL  
#define ML_MSG		0x0000000000000002ULL  
#define ML_SOCKET	0x0000000000000004ULL  
#define ML_HEARTBEAT	0x0000000000000008ULL  
#define ML_HB_BIO	0x0000000000000010ULL  
#define ML_DLMFS	0x0000000000000020ULL  
#define ML_DLM		0x0000000000000040ULL  
#define ML_DLM_DOMAIN	0x0000000000000080ULL  
#define ML_DLM_THREAD	0x0000000000000100ULL  
#define ML_DLM_MASTER	0x0000000000000200ULL  
#define ML_DLM_RECOVERY	0x0000000000000400ULL  
#define ML_DLM_GLUE	0x0000000000000800ULL  
#define ML_VOTE		0x0000000000001000ULL  
#define ML_CONN		0x0000000000002000ULL  
#define ML_QUORUM	0x0000000000004000ULL  
#define ML_BASTS	0x0000000000008000ULL  
#define ML_CLUSTER	0x0000000000010000ULL  

 
#define ML_ERROR	0x1000000000000000ULL  
#define ML_NOTICE	0x2000000000000000ULL  
#define ML_KTHREAD	0x4000000000000000ULL  

#define MLOG_INITIAL_AND_MASK (ML_ERROR|ML_NOTICE)
#ifndef MLOG_MASK_PREFIX
#define MLOG_MASK_PREFIX 0
#endif

 
#if defined(CONFIG_OCFS2_DEBUG_MASKLOG)
#define ML_ALLOWED_BITS ~0
#else
#define ML_ALLOWED_BITS (ML_ERROR|ML_NOTICE)
#endif

#define MLOG_MAX_BITS 64

struct mlog_bits {
	unsigned long words[MLOG_MAX_BITS / BITS_PER_LONG];
};

extern struct mlog_bits mlog_and_bits, mlog_not_bits;

#if BITS_PER_LONG == 32

#define __mlog_test_u64(mask, bits)			\
	( (u32)(mask & 0xffffffff) & bits.words[0] || 	\
	  ((u64)(mask) >> 32) & bits.words[1] )
#define __mlog_set_u64(mask, bits) do {			\
	bits.words[0] |= (u32)(mask & 0xffffffff);	\
       	bits.words[1] |= (u64)(mask) >> 32;		\
} while (0)
#define __mlog_clear_u64(mask, bits) do {		\
	bits.words[0] &= ~((u32)(mask & 0xffffffff));	\
       	bits.words[1] &= ~((u64)(mask) >> 32);		\
} while (0)
#define MLOG_BITS_RHS(mask) {				\
	{						\
		[0] = (u32)(mask & 0xffffffff),		\
		[1] = (u64)(mask) >> 32,		\
	}						\
}

#else  

#define __mlog_test_u64(mask, bits)	((mask) & bits.words[0])
#define __mlog_set_u64(mask, bits) do {		\
	bits.words[0] |= (mask);		\
} while (0)
#define __mlog_clear_u64(mask, bits) do {	\
	bits.words[0] &= ~(mask);		\
} while (0)
#define MLOG_BITS_RHS(mask) { { (mask) } }

#endif

__printf(4, 5)
void __mlog_printk(const u64 *m, const char *func, int line,
		   const char *fmt, ...);

 
#define mlog(mask, fmt, ...)						\
do {									\
	u64 _m = MLOG_MASK_PREFIX | (mask);				\
	if (_m & ML_ALLOWED_BITS)					\
		__mlog_printk(&_m, __func__, __LINE__, fmt,		\
			      ##__VA_ARGS__);				\
} while (0)

#define mlog_ratelimited(mask, fmt, ...)				\
do {									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      DEFAULT_RATELIMIT_BURST);		\
	if (__ratelimit(&_rs))						\
		mlog(mask, fmt, ##__VA_ARGS__);				\
} while (0)

#define mlog_errno(st) ({						\
	int _st = (st);							\
	if (_st != -ERESTARTSYS && _st != -EINTR &&			\
	    _st != AOP_TRUNCATED_PAGE && _st != -ENOSPC &&		\
	    _st != -EDQUOT)						\
		mlog(ML_ERROR, "status = %lld\n", (long long)_st);	\
	_st;								\
})

#define mlog_bug_on_msg(cond, fmt, args...) do {			\
	if (cond) {							\
		mlog(ML_ERROR, "bug expression: " #cond "\n");		\
		mlog(ML_ERROR, fmt, ##args);				\
		BUG();							\
	}								\
} while (0)

#include <linux/kobject.h>
#include <linux/sysfs.h>
int mlog_sys_init(struct kset *o2cb_subsys);
void mlog_sys_shutdown(void);

#endif  
