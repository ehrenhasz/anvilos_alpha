#ifndef _EPAPR_HCALLS_H
#define _EPAPR_HCALLS_H
#include <uapi/asm/epapr_hcalls.h>
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/byteorder.h>
#define EV_HCALL_CLOBBERS "r0", "r12", "xer", "ctr", "lr", "cc", "memory"
#define EV_HCALL_CLOBBERS8 EV_HCALL_CLOBBERS
#define EV_HCALL_CLOBBERS7 EV_HCALL_CLOBBERS8, "r10"
#define EV_HCALL_CLOBBERS6 EV_HCALL_CLOBBERS7, "r9"
#define EV_HCALL_CLOBBERS5 EV_HCALL_CLOBBERS6, "r8"
#define EV_HCALL_CLOBBERS4 EV_HCALL_CLOBBERS5, "r7"
#define EV_HCALL_CLOBBERS3 EV_HCALL_CLOBBERS4, "r6"
#define EV_HCALL_CLOBBERS2 EV_HCALL_CLOBBERS3, "r5"
#define EV_HCALL_CLOBBERS1 EV_HCALL_CLOBBERS2, "r4"
extern bool epapr_paravirt_enabled;
extern u32 epapr_hypercall_start[];
#ifdef CONFIG_EPAPR_PARAVIRT
int __init epapr_paravirt_early_init(void);
#else
static inline int epapr_paravirt_early_init(void) { return 0; }
#endif
static inline unsigned int ev_int_set_config(unsigned int interrupt,
	uint32_t config, unsigned int priority, uint32_t destination)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	r11 = EV_HCALL_TOKEN(EV_INT_SET_CONFIG);
	r3  = interrupt;
	r4  = config;
	r5  = priority;
	r6  = destination;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6)
		: : EV_HCALL_CLOBBERS4
	);
	return r3;
}
static inline unsigned int ev_int_get_config(unsigned int interrupt,
	uint32_t *config, unsigned int *priority, uint32_t *destination)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	r11 = EV_HCALL_TOKEN(EV_INT_GET_CONFIG);
	r3 = interrupt;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4), "=r" (r5), "=r" (r6)
		: : EV_HCALL_CLOBBERS4
	);
	*config = r4;
	*priority = r5;
	*destination = r6;
	return r3;
}
static inline unsigned int ev_int_set_mask(unsigned int interrupt,
	unsigned int mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	r11 = EV_HCALL_TOKEN(EV_INT_SET_MASK);
	r3 = interrupt;
	r4 = mask;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "+r" (r4)
		: : EV_HCALL_CLOBBERS2
	);
	return r3;
}
static inline unsigned int ev_int_get_mask(unsigned int interrupt,
	unsigned int *mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	r11 = EV_HCALL_TOKEN(EV_INT_GET_MASK);
	r3 = interrupt;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);
	*mask = r4;
	return r3;
}
static inline unsigned int ev_int_eoi(unsigned int interrupt)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	r11 = EV_HCALL_TOKEN(EV_INT_EOI);
	r3 = interrupt;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);
	return r3;
}
static inline unsigned int ev_byte_channel_send(unsigned int handle,
	unsigned int *count, const char buffer[EV_BYTE_CHANNEL_MAX_BYTES])
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	const uint32_t *p = (const uint32_t *) buffer;
	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_SEND);
	r3 = handle;
	r4 = *count;
	r5 = be32_to_cpu(p[0]);
	r6 = be32_to_cpu(p[1]);
	r7 = be32_to_cpu(p[2]);
	r8 = be32_to_cpu(p[3]);
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3),
		  "+r" (r4), "+r" (r5), "+r" (r6), "+r" (r7), "+r" (r8)
		: : EV_HCALL_CLOBBERS6
	);
	*count = r4;
	return r3;
}
static inline unsigned int ev_byte_channel_receive(unsigned int handle,
	unsigned int *count, char buffer[EV_BYTE_CHANNEL_MAX_BYTES])
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	uint32_t *p = (uint32_t *) buffer;
	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_RECEIVE);
	r3 = handle;
	r4 = *count;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "+r" (r4),
		  "=r" (r5), "=r" (r6), "=r" (r7), "=r" (r8)
		: : EV_HCALL_CLOBBERS6
	);
	*count = r4;
	p[0] = cpu_to_be32(r5);
	p[1] = cpu_to_be32(r6);
	p[2] = cpu_to_be32(r7);
	p[3] = cpu_to_be32(r8);
	return r3;
}
static inline unsigned int ev_byte_channel_poll(unsigned int handle,
	unsigned int *rx_count,	unsigned int *tx_count)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_POLL);
	r3 = handle;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4), "=r" (r5)
		: : EV_HCALL_CLOBBERS3
	);
	*rx_count = r4;
	*tx_count = r5;
	return r3;
}
static inline unsigned int ev_int_iack(unsigned int handle,
	unsigned int *vector)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	r11 = EV_HCALL_TOKEN(EV_INT_IACK);
	r3 = handle;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);
	*vector = r4;
	return r3;
}
static inline unsigned int ev_doorbell_send(unsigned int handle)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	r11 = EV_HCALL_TOKEN(EV_DOORBELL_SEND);
	r3 = handle;
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);
	return r3;
}
static inline unsigned int ev_idle(void)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	r11 = EV_HCALL_TOKEN(EV_IDLE);
	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "=r" (r3)
		: : EV_HCALL_CLOBBERS1
	);
	return r3;
}
#ifdef CONFIG_EPAPR_PARAVIRT
static inline unsigned long epapr_hypercall(unsigned long *in,
			    unsigned long *out,
			    unsigned long nr)
{
	register unsigned long r0 asm("r0");
	register unsigned long r3 asm("r3") = in[0];
	register unsigned long r4 asm("r4") = in[1];
	register unsigned long r5 asm("r5") = in[2];
	register unsigned long r6 asm("r6") = in[3];
	register unsigned long r7 asm("r7") = in[4];
	register unsigned long r8 asm("r8") = in[5];
	register unsigned long r9 asm("r9") = in[6];
	register unsigned long r10 asm("r10") = in[7];
	register unsigned long r11 asm("r11") = nr;
	register unsigned long r12 asm("r12");
	asm volatile("bl	epapr_hypercall_start"
		     : "=r"(r0), "=r"(r3), "=r"(r4), "=r"(r5), "=r"(r6),
		       "=r"(r7), "=r"(r8), "=r"(r9), "=r"(r10), "=r"(r11),
		       "=r"(r12)
		     : "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7), "r"(r8),
		       "r"(r9), "r"(r10), "r"(r11)
		     : "memory", "cc", "xer", "ctr", "lr");
	out[0] = r4;
	out[1] = r5;
	out[2] = r6;
	out[3] = r7;
	out[4] = r8;
	out[5] = r9;
	out[6] = r10;
	out[7] = r11;
	return r3;
}
#else
static unsigned long epapr_hypercall(unsigned long *in,
				   unsigned long *out,
				   unsigned long nr)
{
	return EV_UNIMPLEMENTED;
}
#endif
static inline long epapr_hypercall0_1(unsigned int nr, unsigned long *r2)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	unsigned long r;
	r = epapr_hypercall(in, out, nr);
	*r2 = out[0];
	return r;
}
static inline long epapr_hypercall0(unsigned int nr)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	return epapr_hypercall(in, out, nr);
}
static inline long epapr_hypercall1(unsigned int nr, unsigned long p1)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	in[0] = p1;
	return epapr_hypercall(in, out, nr);
}
static inline long epapr_hypercall2(unsigned int nr, unsigned long p1,
				    unsigned long p2)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	in[0] = p1;
	in[1] = p2;
	return epapr_hypercall(in, out, nr);
}
static inline long epapr_hypercall3(unsigned int nr, unsigned long p1,
				    unsigned long p2, unsigned long p3)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	in[0] = p1;
	in[1] = p2;
	in[2] = p3;
	return epapr_hypercall(in, out, nr);
}
static inline long epapr_hypercall4(unsigned int nr, unsigned long p1,
				    unsigned long p2, unsigned long p3,
				    unsigned long p4)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	in[0] = p1;
	in[1] = p2;
	in[2] = p3;
	in[3] = p4;
	return epapr_hypercall(in, out, nr);
}
#endif  
#endif  
