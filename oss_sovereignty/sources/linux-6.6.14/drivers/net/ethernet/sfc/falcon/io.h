


#ifndef EF4_IO_H
#define EF4_IO_H

#include <linux/io.h>
#include <linux/spinlock.h>



#if BITS_PER_LONG == 64
#define EF4_USE_QWORD_IO 1
#endif

#ifdef EF4_USE_QWORD_IO
static inline void _ef4_writeq(struct ef4_nic *efx, __le64 value,
				  unsigned int reg)
{
	__raw_writeq((__force u64)value, efx->membase + reg);
}
static inline __le64 _ef4_readq(struct ef4_nic *efx, unsigned int reg)
{
	return (__force __le64)__raw_readq(efx->membase + reg);
}
#endif

static inline void _ef4_writed(struct ef4_nic *efx, __le32 value,
				  unsigned int reg)
{
	__raw_writel((__force u32)value, efx->membase + reg);
}
static inline __le32 _ef4_readd(struct ef4_nic *efx, unsigned int reg)
{
	return (__force __le32)__raw_readl(efx->membase + reg);
}


static inline void ef4_writeo(struct ef4_nic *efx, const ef4_oword_t *value,
			      unsigned int reg)
{
	unsigned long flags __attribute__ ((unused));

	netif_vdbg(efx, hw, efx->net_dev,
		   "writing register %x with " EF4_OWORD_FMT "\n", reg,
		   EF4_OWORD_VAL(*value));

	spin_lock_irqsave(&efx->biu_lock, flags);
#ifdef EF4_USE_QWORD_IO
	_ef4_writeq(efx, value->u64[0], reg + 0);
	_ef4_writeq(efx, value->u64[1], reg + 8);
#else
	_ef4_writed(efx, value->u32[0], reg + 0);
	_ef4_writed(efx, value->u32[1], reg + 4);
	_ef4_writed(efx, value->u32[2], reg + 8);
	_ef4_writed(efx, value->u32[3], reg + 12);
#endif
	spin_unlock_irqrestore(&efx->biu_lock, flags);
}


static inline void ef4_sram_writeq(struct ef4_nic *efx, void __iomem *membase,
				   const ef4_qword_t *value, unsigned int index)
{
	unsigned int addr = index * sizeof(*value);
	unsigned long flags __attribute__ ((unused));

	netif_vdbg(efx, hw, efx->net_dev,
		   "writing SRAM address %x with " EF4_QWORD_FMT "\n",
		   addr, EF4_QWORD_VAL(*value));

	spin_lock_irqsave(&efx->biu_lock, flags);
#ifdef EF4_USE_QWORD_IO
	__raw_writeq((__force u64)value->u64[0], membase + addr);
#else
	__raw_writel((__force u32)value->u32[0], membase + addr);
	__raw_writel((__force u32)value->u32[1], membase + addr + 4);
#endif
	spin_unlock_irqrestore(&efx->biu_lock, flags);
}


static inline void ef4_writed(struct ef4_nic *efx, const ef4_dword_t *value,
			      unsigned int reg)
{
	netif_vdbg(efx, hw, efx->net_dev,
		   "writing register %x with "EF4_DWORD_FMT"\n",
		   reg, EF4_DWORD_VAL(*value));

	
	_ef4_writed(efx, value->u32[0], reg);
}


static inline void ef4_reado(struct ef4_nic *efx, ef4_oword_t *value,
			     unsigned int reg)
{
	unsigned long flags __attribute__ ((unused));

	spin_lock_irqsave(&efx->biu_lock, flags);
	value->u32[0] = _ef4_readd(efx, reg + 0);
	value->u32[1] = _ef4_readd(efx, reg + 4);
	value->u32[2] = _ef4_readd(efx, reg + 8);
	value->u32[3] = _ef4_readd(efx, reg + 12);
	spin_unlock_irqrestore(&efx->biu_lock, flags);

	netif_vdbg(efx, hw, efx->net_dev,
		   "read from register %x, got " EF4_OWORD_FMT "\n", reg,
		   EF4_OWORD_VAL(*value));
}


static inline void ef4_sram_readq(struct ef4_nic *efx, void __iomem *membase,
				  ef4_qword_t *value, unsigned int index)
{
	unsigned int addr = index * sizeof(*value);
	unsigned long flags __attribute__ ((unused));

	spin_lock_irqsave(&efx->biu_lock, flags);
#ifdef EF4_USE_QWORD_IO
	value->u64[0] = (__force __le64)__raw_readq(membase + addr);
#else
	value->u32[0] = (__force __le32)__raw_readl(membase + addr);
	value->u32[1] = (__force __le32)__raw_readl(membase + addr + 4);
#endif
	spin_unlock_irqrestore(&efx->biu_lock, flags);

	netif_vdbg(efx, hw, efx->net_dev,
		   "read from SRAM address %x, got "EF4_QWORD_FMT"\n",
		   addr, EF4_QWORD_VAL(*value));
}


static inline void ef4_readd(struct ef4_nic *efx, ef4_dword_t *value,
				unsigned int reg)
{
	value->u32[0] = _ef4_readd(efx, reg);
	netif_vdbg(efx, hw, efx->net_dev,
		   "read from register %x, got "EF4_DWORD_FMT"\n",
		   reg, EF4_DWORD_VAL(*value));
}


static inline void
ef4_writeo_table(struct ef4_nic *efx, const ef4_oword_t *value,
		 unsigned int reg, unsigned int index)
{
	ef4_writeo(efx, value, reg + index * sizeof(ef4_oword_t));
}


static inline void ef4_reado_table(struct ef4_nic *efx, ef4_oword_t *value,
				     unsigned int reg, unsigned int index)
{
	ef4_reado(efx, value, reg + index * sizeof(ef4_oword_t));
}


#define EF4_VI_PAGE_SIZE 0x2000


#define EF4_PAGED_REG(page, reg) \
	((page) * EF4_VI_PAGE_SIZE + (reg))


static inline void _ef4_writeo_page(struct ef4_nic *efx, ef4_oword_t *value,
				    unsigned int reg, unsigned int page)
{
	reg = EF4_PAGED_REG(page, reg);

	netif_vdbg(efx, hw, efx->net_dev,
		   "writing register %x with " EF4_OWORD_FMT "\n", reg,
		   EF4_OWORD_VAL(*value));

#ifdef EF4_USE_QWORD_IO
	_ef4_writeq(efx, value->u64[0], reg + 0);
	_ef4_writeq(efx, value->u64[1], reg + 8);
#else
	_ef4_writed(efx, value->u32[0], reg + 0);
	_ef4_writed(efx, value->u32[1], reg + 4);
	_ef4_writed(efx, value->u32[2], reg + 8);
	_ef4_writed(efx, value->u32[3], reg + 12);
#endif
}
#define ef4_writeo_page(efx, value, reg, page)				\
	_ef4_writeo_page(efx, value,					\
			 reg +						\
			 BUILD_BUG_ON_ZERO((reg) != 0x830 && (reg) != 0xa10), \
			 page)


static inline void
_ef4_writed_page(struct ef4_nic *efx, const ef4_dword_t *value,
		 unsigned int reg, unsigned int page)
{
	ef4_writed(efx, value, EF4_PAGED_REG(page, reg));
}
#define ef4_writed_page(efx, value, reg, page)				\
	_ef4_writed_page(efx, value,					\
			 reg +						\
			 BUILD_BUG_ON_ZERO((reg) != 0x400 &&		\
					   (reg) != 0x420 &&		\
					   (reg) != 0x830 &&		\
					   (reg) != 0x83c &&		\
					   (reg) != 0xa18 &&		\
					   (reg) != 0xa1c),		\
			 page)


static inline void _ef4_writed_page_locked(struct ef4_nic *efx,
					   const ef4_dword_t *value,
					   unsigned int reg,
					   unsigned int page)
{
	unsigned long flags __attribute__ ((unused));

	if (page == 0) {
		spin_lock_irqsave(&efx->biu_lock, flags);
		ef4_writed(efx, value, EF4_PAGED_REG(page, reg));
		spin_unlock_irqrestore(&efx->biu_lock, flags);
	} else {
		ef4_writed(efx, value, EF4_PAGED_REG(page, reg));
	}
}
#define ef4_writed_page_locked(efx, value, reg, page)			\
	_ef4_writed_page_locked(efx, value,				\
				reg + BUILD_BUG_ON_ZERO((reg) != 0x420), \
				page)

#endif 
