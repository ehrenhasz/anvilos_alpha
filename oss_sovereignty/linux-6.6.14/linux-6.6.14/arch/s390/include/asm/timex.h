#ifndef _ASM_S390_TIMEX_H
#define _ASM_S390_TIMEX_H
#include <linux/preempt.h>
#include <linux/time64.h>
#include <asm/lowcore.h>
#define TOD_UNIX_EPOCH 0x7d91048bca000000ULL
extern u64 clock_comparator_max;
union tod_clock {
	__uint128_t val;
	struct {
		__uint128_t ei	:  8;  
		__uint128_t tod : 64;  
		__uint128_t	: 40;
		__uint128_t pf	: 16;  
	};
	struct {
		__uint128_t eitod : 72;  
		__uint128_t	  : 56;
	};
	struct {
		__uint128_t us	: 60;  
		__uint128_t sus	: 12;  
		__uint128_t	: 56;
	};
} __packed;
static inline int set_tod_clock(__u64 time)
{
	int cc;
	asm volatile(
		"   sck   %1\n"
		"   ipm   %0\n"
		"   srl   %0,28\n"
		: "=d" (cc) : "Q" (time) : "cc");
	return cc;
}
static inline int store_tod_clock_ext_cc(union tod_clock *clk)
{
	int cc;
	asm volatile(
		"   stcke  %1\n"
		"   ipm   %0\n"
		"   srl   %0,28\n"
		: "=d" (cc), "=Q" (*clk) : : "cc");
	return cc;
}
static __always_inline void store_tod_clock_ext(union tod_clock *tod)
{
	asm volatile("stcke %0" : "=Q" (*tod) : : "cc");
}
static inline void set_clock_comparator(__u64 time)
{
	asm volatile("sckc %0" : : "Q" (time));
}
static inline void set_tod_programmable_field(u16 val)
{
	asm volatile(
		"	lgr	0,%[val]\n"
		"	sckpf\n"
		:
		: [val] "d" ((unsigned long)val)
		: "0");
}
void clock_comparator_work(void);
void __init time_early_init(void);
extern unsigned char ptff_function_mask[16];
#define PTFF_QAF	0x00	 
#define PTFF_QTO	0x01	 
#define PTFF_QSI	0x02	 
#define PTFF_QUI	0x04	 
#define PTFF_ATO	0x40	 
#define PTFF_STO	0x41	 
#define PTFF_SFS	0x42	 
#define PTFF_SGS	0x43	 
struct ptff_qto {
	unsigned long physical_clock;
	unsigned long tod_offset;
	unsigned long logical_tod_offset;
	unsigned long tod_epoch_difference;
} __packed;
static inline int ptff_query(unsigned int nr)
{
	unsigned char *ptr;
	ptr = ptff_function_mask + (nr >> 3);
	return (*ptr & (0x80 >> (nr & 7))) != 0;
}
struct ptff_qui {
	unsigned int tm : 2;
	unsigned int ts : 2;
	unsigned int : 28;
	unsigned int pad_0x04;
	unsigned long leap_event;
	short old_leap;
	short new_leap;
	unsigned int pad_0x14;
	unsigned long prt[5];
	unsigned long cst[3];
	unsigned int skew;
	unsigned int pad_0x5c[41];
} __packed;
#define ptff(ptff_block, len, func)					\
({									\
	struct addrtype { char _[len]; };				\
	unsigned int reg0 = func;					\
	unsigned long reg1 = (unsigned long)(ptff_block);		\
	int rc;								\
									\
	asm volatile(							\
		"	lgr	0,%[reg0]\n"				\
		"	lgr	1,%[reg1]\n"				\
		"	ptff\n"						\
		"	ipm	%[rc]\n"				\
		"	srl	%[rc],28\n"				\
		: [rc] "=&d" (rc), "+m" (*(struct addrtype *)reg1)	\
		: [reg0] "d" (reg0), [reg1] "d" (reg1)			\
		: "cc", "0", "1");					\
	rc;								\
})
static inline unsigned long local_tick_disable(void)
{
	unsigned long old;
	old = S390_lowcore.clock_comparator;
	S390_lowcore.clock_comparator = clock_comparator_max;
	set_clock_comparator(S390_lowcore.clock_comparator);
	return old;
}
static inline void local_tick_enable(unsigned long comp)
{
	S390_lowcore.clock_comparator = comp;
	set_clock_comparator(S390_lowcore.clock_comparator);
}
#define CLOCK_TICK_RATE		1193180  
typedef unsigned long cycles_t;
static __always_inline unsigned long get_tod_clock(void)
{
	union tod_clock clk;
	store_tod_clock_ext(&clk);
	return clk.tod;
}
static inline unsigned long get_tod_clock_fast(void)
{
	unsigned long clk;
	asm volatile("stckf %0" : "=Q" (clk) : : "cc");
	return clk;
}
static inline cycles_t get_cycles(void)
{
	return (cycles_t) get_tod_clock() >> 2;
}
#define get_cycles get_cycles
int get_phys_clock(unsigned long *clock);
void init_cpu_timer(void);
extern union tod_clock tod_clock_base;
static __always_inline unsigned long __get_tod_clock_monotonic(void)
{
	return get_tod_clock() - tod_clock_base.tod;
}
static inline unsigned long get_tod_clock_monotonic(void)
{
	unsigned long tod;
	preempt_disable_notrace();
	tod = __get_tod_clock_monotonic();
	preempt_enable_notrace();
	return tod;
}
static __always_inline unsigned long tod_to_ns(unsigned long todval)
{
	return ((todval >> 9) * 125) + (((todval & 0x1ff) * 125) >> 9);
}
static inline int tod_after(unsigned long a, unsigned long b)
{
	if (MACHINE_HAS_SCC)
		return (long) a > (long) b;
	return a > b;
}
static inline int tod_after_eq(unsigned long a, unsigned long b)
{
	if (MACHINE_HAS_SCC)
		return (long) a >= (long) b;
	return a >= b;
}
#endif
