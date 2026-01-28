#ifndef _ASM_S390_AIRQ_H
#define _ASM_S390_AIRQ_H
#include <linux/bit_spinlock.h>
#include <linux/dma-mapping.h>
#include <asm/tpi.h>
struct airq_struct {
	struct hlist_node list;		 
	void (*handler)(struct airq_struct *airq, struct tpi_info *tpi_info);
	u8 *lsi_ptr;			 
	u8 isc;				 
	u8 flags;
};
#define AIRQ_PTR_ALLOCATED	0x01
int register_adapter_interrupt(struct airq_struct *airq);
void unregister_adapter_interrupt(struct airq_struct *airq);
struct airq_iv {
	unsigned long *vector;	 
	dma_addr_t vector_dma;  
	unsigned long *avail;	 
	unsigned long *bitlock;	 
	unsigned long *ptr;	 
	unsigned int *data;	 
	unsigned long bits;	 
	unsigned long end;	 
	unsigned long flags;	 
	spinlock_t lock;	 
};
#define AIRQ_IV_ALLOC		1	 
#define AIRQ_IV_BITLOCK		2	 
#define AIRQ_IV_PTR		4	 
#define AIRQ_IV_DATA		8	 
#define AIRQ_IV_CACHELINE	16	 
#define AIRQ_IV_GUESTVEC	32	 
struct airq_iv *airq_iv_create(unsigned long bits, unsigned long flags,
			       unsigned long *vec);
void airq_iv_release(struct airq_iv *iv);
unsigned long airq_iv_alloc(struct airq_iv *iv, unsigned long num);
void airq_iv_free(struct airq_iv *iv, unsigned long bit, unsigned long num);
unsigned long airq_iv_scan(struct airq_iv *iv, unsigned long start,
			   unsigned long end);
static inline unsigned long airq_iv_alloc_bit(struct airq_iv *iv)
{
	return airq_iv_alloc(iv, 1);
}
static inline void airq_iv_free_bit(struct airq_iv *iv, unsigned long bit)
{
	airq_iv_free(iv, bit, 1);
}
static inline unsigned long airq_iv_end(struct airq_iv *iv)
{
	return iv->end;
}
static inline void airq_iv_lock(struct airq_iv *iv, unsigned long bit)
{
	const unsigned long be_to_le = BITS_PER_LONG - 1;
	bit_spin_lock(bit ^ be_to_le, iv->bitlock);
}
static inline void airq_iv_unlock(struct airq_iv *iv, unsigned long bit)
{
	const unsigned long be_to_le = BITS_PER_LONG - 1;
	bit_spin_unlock(bit ^ be_to_le, iv->bitlock);
}
static inline void airq_iv_set_data(struct airq_iv *iv, unsigned long bit,
				    unsigned int data)
{
	iv->data[bit] = data;
}
static inline unsigned int airq_iv_get_data(struct airq_iv *iv,
					    unsigned long bit)
{
	return iv->data[bit];
}
static inline void airq_iv_set_ptr(struct airq_iv *iv, unsigned long bit,
				   unsigned long ptr)
{
	iv->ptr[bit] = ptr;
}
static inline unsigned long airq_iv_get_ptr(struct airq_iv *iv,
					    unsigned long bit)
{
	return iv->ptr[bit];
}
#endif  
