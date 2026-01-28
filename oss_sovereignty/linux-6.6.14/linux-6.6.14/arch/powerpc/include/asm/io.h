#ifndef _ASM_POWERPC_IO_H
#define _ASM_POWERPC_IO_H
#ifdef __KERNEL__
extern int check_legacy_ioport(unsigned long base_port);
#define I8042_DATA_REG	0x60
#define FDC_BASE	0x3f0
#if defined(CONFIG_PPC64) && defined(CONFIG_PCI)
extern struct pci_dev *isa_bridge_pcidev;
#define arch_has_dev_port()	(isa_bridge_pcidev != NULL || isa_io_special)
#endif
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/byteorder.h>
#include <asm/synch.h>
#include <asm/delay.h>
#include <asm/mmiowb.h>
#include <asm/mmu.h>
#define SIO_CONFIG_RA	0x398
#define SIO_CONFIG_RD	0x399
#ifndef CONFIG_PCI
#define _IO_BASE	0
#define _ISA_MEM_BASE	0
#define PCI_DRAM_OFFSET 0
#elif defined(CONFIG_PPC32)
#define _IO_BASE	isa_io_base
#define _ISA_MEM_BASE	isa_mem_base
#define PCI_DRAM_OFFSET	pci_dram_offset
#else
#define _IO_BASE	pci_io_base
#define _ISA_MEM_BASE	isa_mem_base
#define PCI_DRAM_OFFSET	0
#endif
extern unsigned long isa_io_base;
extern unsigned long pci_io_base;
extern unsigned long pci_dram_offset;
extern resource_size_t isa_mem_base;
extern bool isa_io_special;
#ifdef CONFIG_PPC32
#if defined(CONFIG_PPC_INDIRECT_PIO) || defined(CONFIG_PPC_INDIRECT_MMIO)
#error CONFIG_PPC_INDIRECT_{PIO,MMIO} are not yet supported on 32 bits
#endif
#endif
#ifdef CONFIG_PPC_KERNEL_PREFIXED
#define DEF_MMIO_IN_X(name, size, insn)				\
static inline u##size name(const volatile u##size __iomem *addr)	\
{									\
	u##size ret;							\
	__asm__ __volatile__("sync;"#insn" %0,0,%1;twi 0,%0,0;isync"	\
		: "=r" (ret) : "r" (addr) : "memory");			\
	return ret;							\
}
#define DEF_MMIO_OUT_X(name, size, insn)				\
static inline void name(volatile u##size __iomem *addr, u##size val)	\
{									\
	__asm__ __volatile__("sync;"#insn" %1,0,%0"			\
		: : "r" (addr), "r" (val) : "memory");			\
	mmiowb_set_pending();						\
}
#define DEF_MMIO_IN_D(name, size, insn)				\
static inline u##size name(const volatile u##size __iomem *addr)	\
{									\
	u##size ret;							\
	__asm__ __volatile__("sync;"#insn" %0,0(%1);twi 0,%0,0;isync"\
		: "=r" (ret) : "b" (addr) : "memory");	\
	return ret;							\
}
#define DEF_MMIO_OUT_D(name, size, insn)				\
static inline void name(volatile u##size __iomem *addr, u##size val)	\
{									\
	__asm__ __volatile__("sync;"#insn" %1,0(%0)"			\
		: : "b" (addr), "r" (val) : "memory");	\
	mmiowb_set_pending();						\
}
#else
#define DEF_MMIO_IN_X(name, size, insn)				\
static inline u##size name(const volatile u##size __iomem *addr)	\
{									\
	u##size ret;							\
	__asm__ __volatile__("sync;"#insn" %0,%y1;twi 0,%0,0;isync"	\
		: "=r" (ret) : "Z" (*addr) : "memory");			\
	return ret;							\
}
#define DEF_MMIO_OUT_X(name, size, insn)				\
static inline void name(volatile u##size __iomem *addr, u##size val)	\
{									\
	__asm__ __volatile__("sync;"#insn" %1,%y0"			\
		: "=Z" (*addr) : "r" (val) : "memory");			\
	mmiowb_set_pending();						\
}
#define DEF_MMIO_IN_D(name, size, insn)				\
static inline u##size name(const volatile u##size __iomem *addr)	\
{									\
	u##size ret;							\
	__asm__ __volatile__("sync;"#insn"%U1%X1 %0,%1;twi 0,%0,0;isync"\
		: "=r" (ret) : "m<>" (*addr) : "memory");	\
	return ret;							\
}
#define DEF_MMIO_OUT_D(name, size, insn)				\
static inline void name(volatile u##size __iomem *addr, u##size val)	\
{									\
	__asm__ __volatile__("sync;"#insn"%U0%X0 %1,%0"			\
		: "=m<>" (*addr) : "r" (val) : "memory");	\
	mmiowb_set_pending();						\
}
#endif
DEF_MMIO_IN_D(in_8,     8, lbz);
DEF_MMIO_OUT_D(out_8,   8, stb);
#ifdef __BIG_ENDIAN__
DEF_MMIO_IN_D(in_be16, 16, lhz);
DEF_MMIO_IN_D(in_be32, 32, lwz);
DEF_MMIO_IN_X(in_le16, 16, lhbrx);
DEF_MMIO_IN_X(in_le32, 32, lwbrx);
DEF_MMIO_OUT_D(out_be16, 16, sth);
DEF_MMIO_OUT_D(out_be32, 32, stw);
DEF_MMIO_OUT_X(out_le16, 16, sthbrx);
DEF_MMIO_OUT_X(out_le32, 32, stwbrx);
#else
DEF_MMIO_IN_X(in_be16, 16, lhbrx);
DEF_MMIO_IN_X(in_be32, 32, lwbrx);
DEF_MMIO_IN_D(in_le16, 16, lhz);
DEF_MMIO_IN_D(in_le32, 32, lwz);
DEF_MMIO_OUT_X(out_be16, 16, sthbrx);
DEF_MMIO_OUT_X(out_be32, 32, stwbrx);
DEF_MMIO_OUT_D(out_le16, 16, sth);
DEF_MMIO_OUT_D(out_le32, 32, stw);
#endif  
#ifdef __powerpc64__
#ifdef __BIG_ENDIAN__
DEF_MMIO_OUT_D(out_be64, 64, std);
DEF_MMIO_IN_D(in_be64, 64, ld);
static inline u64 in_le64(const volatile u64 __iomem *addr)
{
	return swab64(in_be64(addr));
}
static inline void out_le64(volatile u64 __iomem *addr, u64 val)
{
	out_be64(addr, swab64(val));
}
#else
DEF_MMIO_OUT_D(out_le64, 64, std);
DEF_MMIO_IN_D(in_le64, 64, ld);
static inline u64 in_be64(const volatile u64 __iomem *addr)
{
	return swab64(in_le64(addr));
}
static inline void out_be64(volatile u64 __iomem *addr, u64 val)
{
	out_le64(addr, swab64(val));
}
#endif
#endif  
extern void _insb(const volatile u8 __iomem *addr, void *buf, long count);
extern void _outsb(volatile u8 __iomem *addr,const void *buf,long count);
extern void _insw_ns(const volatile u16 __iomem *addr, void *buf, long count);
extern void _outsw_ns(volatile u16 __iomem *addr, const void *buf, long count);
extern void _insl_ns(const volatile u32 __iomem *addr, void *buf, long count);
extern void _outsl_ns(volatile u32 __iomem *addr, const void *buf, long count);
#define _insw	_insw_ns
#define _insl	_insl_ns
#define _outsw	_outsw_ns
#define _outsl	_outsl_ns
extern void _memset_io(volatile void __iomem *addr, int c, unsigned long n);
extern void _memcpy_fromio(void *dest, const volatile void __iomem *src,
			   unsigned long n);
extern void _memcpy_toio(volatile void __iomem *dest, const void *src,
			 unsigned long n);
#ifdef CONFIG_EEH
#include <asm/eeh.h>
#endif
#define PCI_IO_ADDR	volatile void __iomem *
#ifdef CONFIG_PPC_INDIRECT_MMIO
#define PCI_IO_IND_TOKEN_SHIFT	52
#define PCI_IO_IND_TOKEN_MASK	(0xfful << PCI_IO_IND_TOKEN_SHIFT)
#define PCI_FIX_ADDR(addr)						\
	((PCI_IO_ADDR)(((unsigned long)(addr)) & ~PCI_IO_IND_TOKEN_MASK))
#define PCI_GET_ADDR_TOKEN(addr)					\
	(((unsigned long)(addr) & PCI_IO_IND_TOKEN_MASK) >> 		\
		PCI_IO_IND_TOKEN_SHIFT)
#define PCI_SET_ADDR_TOKEN(addr, token) 				\
do {									\
	unsigned long __a = (unsigned long)(addr);			\
	__a &= ~PCI_IO_IND_TOKEN_MASK;					\
	__a |= ((unsigned long)(token)) << PCI_IO_IND_TOKEN_SHIFT;	\
	(addr) = (void __iomem *)__a;					\
} while(0)
#else
#define PCI_FIX_ADDR(addr) (addr)
#endif
static inline unsigned char __raw_readb(const volatile void __iomem *addr)
{
	return *(volatile unsigned char __force *)PCI_FIX_ADDR(addr);
}
#define __raw_readb __raw_readb
static inline unsigned short __raw_readw(const volatile void __iomem *addr)
{
	return *(volatile unsigned short __force *)PCI_FIX_ADDR(addr);
}
#define __raw_readw __raw_readw
static inline unsigned int __raw_readl(const volatile void __iomem *addr)
{
	return *(volatile unsigned int __force *)PCI_FIX_ADDR(addr);
}
#define __raw_readl __raw_readl
static inline void __raw_writeb(unsigned char v, volatile void __iomem *addr)
{
	*(volatile unsigned char __force *)PCI_FIX_ADDR(addr) = v;
}
#define __raw_writeb __raw_writeb
static inline void __raw_writew(unsigned short v, volatile void __iomem *addr)
{
	*(volatile unsigned short __force *)PCI_FIX_ADDR(addr) = v;
}
#define __raw_writew __raw_writew
static inline void __raw_writel(unsigned int v, volatile void __iomem *addr)
{
	*(volatile unsigned int __force *)PCI_FIX_ADDR(addr) = v;
}
#define __raw_writel __raw_writel
#ifdef __powerpc64__
static inline unsigned long __raw_readq(const volatile void __iomem *addr)
{
	return *(volatile unsigned long __force *)PCI_FIX_ADDR(addr);
}
#define __raw_readq __raw_readq
static inline void __raw_writeq(unsigned long v, volatile void __iomem *addr)
{
	*(volatile unsigned long __force *)PCI_FIX_ADDR(addr) = v;
}
#define __raw_writeq __raw_writeq
static inline void __raw_writeq_be(unsigned long v, volatile void __iomem *addr)
{
	__raw_writeq((__force unsigned long)cpu_to_be64(v), addr);
}
#define __raw_writeq_be __raw_writeq_be
static inline void __raw_rm_writeb(u8 val, volatile void __iomem *paddr)
{
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      stbcix %0,0,%1;  \
			      .machine pop;"
		: : "r" (val), "r" (paddr) : "memory");
}
static inline void __raw_rm_writew(u16 val, volatile void __iomem *paddr)
{
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      sthcix %0,0,%1;  \
			      .machine pop;"
		: : "r" (val), "r" (paddr) : "memory");
}
static inline void __raw_rm_writel(u32 val, volatile void __iomem *paddr)
{
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      stwcix %0,0,%1;  \
			      .machine pop;"
		: : "r" (val), "r" (paddr) : "memory");
}
static inline void __raw_rm_writeq(u64 val, volatile void __iomem *paddr)
{
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      stdcix %0,0,%1;  \
			      .machine pop;"
		: : "r" (val), "r" (paddr) : "memory");
}
static inline void __raw_rm_writeq_be(u64 val, volatile void __iomem *paddr)
{
	__raw_rm_writeq((__force u64)cpu_to_be64(val), paddr);
}
static inline u8 __raw_rm_readb(volatile void __iomem *paddr)
{
	u8 ret;
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      lbzcix %0,0, %1; \
			      .machine pop;"
			     : "=r" (ret) : "r" (paddr) : "memory");
	return ret;
}
static inline u16 __raw_rm_readw(volatile void __iomem *paddr)
{
	u16 ret;
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      lhzcix %0,0, %1; \
			      .machine pop;"
			     : "=r" (ret) : "r" (paddr) : "memory");
	return ret;
}
static inline u32 __raw_rm_readl(volatile void __iomem *paddr)
{
	u32 ret;
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      lwzcix %0,0, %1; \
			      .machine pop;"
			     : "=r" (ret) : "r" (paddr) : "memory");
	return ret;
}
static inline u64 __raw_rm_readq(volatile void __iomem *paddr)
{
	u64 ret;
	__asm__ __volatile__(".machine push;   \
			      .machine power6; \
			      ldcix %0,0, %1;  \
			      .machine pop;"
			     : "=r" (ret) : "r" (paddr) : "memory");
	return ret;
}
#endif  
#ifdef CONFIG_PPC32
#define __do_in_asm(name, op)				\
static inline unsigned int name(unsigned int port)	\
{							\
	unsigned int x;					\
	__asm__ __volatile__(				\
		"sync\n"				\
		"0:"	op "	%0,0,%1\n"		\
		"1:	twi	0,%0,0\n"		\
		"2:	isync\n"			\
		"3:	nop\n"				\
		"4:\n"					\
		".section .fixup,\"ax\"\n"		\
		"5:	li	%0,-1\n"		\
		"	b	4b\n"			\
		".previous\n"				\
		EX_TABLE(0b, 5b)			\
		EX_TABLE(1b, 5b)			\
		EX_TABLE(2b, 5b)			\
		EX_TABLE(3b, 5b)			\
		: "=&r" (x)				\
		: "r" (port + _IO_BASE)			\
		: "memory");  				\
	return x;					\
}
#define __do_out_asm(name, op)				\
static inline void name(unsigned int val, unsigned int port) \
{							\
	__asm__ __volatile__(				\
		"sync\n"				\
		"0:" op " %0,0,%1\n"			\
		"1:	sync\n"				\
		"2:\n"					\
		EX_TABLE(0b, 2b)			\
		EX_TABLE(1b, 2b)			\
		: : "r" (val), "r" (port + _IO_BASE)	\
		: "memory");   	   	   		\
}
__do_in_asm(_rec_inb, "lbzx")
__do_in_asm(_rec_inw, "lhbrx")
__do_in_asm(_rec_inl, "lwbrx")
__do_out_asm(_rec_outb, "stbx")
__do_out_asm(_rec_outw, "sthbrx")
__do_out_asm(_rec_outl, "stwbrx")
#endif  
#define __do_writeb(val, addr)	out_8(PCI_FIX_ADDR(addr), val)
#define __do_writew(val, addr)	out_le16(PCI_FIX_ADDR(addr), val)
#define __do_writel(val, addr)	out_le32(PCI_FIX_ADDR(addr), val)
#define __do_writeq(val, addr)	out_le64(PCI_FIX_ADDR(addr), val)
#define __do_writew_be(val, addr) out_be16(PCI_FIX_ADDR(addr), val)
#define __do_writel_be(val, addr) out_be32(PCI_FIX_ADDR(addr), val)
#define __do_writeq_be(val, addr) out_be64(PCI_FIX_ADDR(addr), val)
#ifdef CONFIG_EEH
#define __do_readb(addr)	eeh_readb(PCI_FIX_ADDR(addr))
#define __do_readw(addr)	eeh_readw(PCI_FIX_ADDR(addr))
#define __do_readl(addr)	eeh_readl(PCI_FIX_ADDR(addr))
#define __do_readq(addr)	eeh_readq(PCI_FIX_ADDR(addr))
#define __do_readw_be(addr)	eeh_readw_be(PCI_FIX_ADDR(addr))
#define __do_readl_be(addr)	eeh_readl_be(PCI_FIX_ADDR(addr))
#define __do_readq_be(addr)	eeh_readq_be(PCI_FIX_ADDR(addr))
#else  
#define __do_readb(addr)	in_8(PCI_FIX_ADDR(addr))
#define __do_readw(addr)	in_le16(PCI_FIX_ADDR(addr))
#define __do_readl(addr)	in_le32(PCI_FIX_ADDR(addr))
#define __do_readq(addr)	in_le64(PCI_FIX_ADDR(addr))
#define __do_readw_be(addr)	in_be16(PCI_FIX_ADDR(addr))
#define __do_readl_be(addr)	in_be32(PCI_FIX_ADDR(addr))
#define __do_readq_be(addr)	in_be64(PCI_FIX_ADDR(addr))
#endif  
#ifdef CONFIG_PPC32
#define __do_outb(val, port)	_rec_outb(val, port)
#define __do_outw(val, port)	_rec_outw(val, port)
#define __do_outl(val, port)	_rec_outl(val, port)
#define __do_inb(port)		_rec_inb(port)
#define __do_inw(port)		_rec_inw(port)
#define __do_inl(port)		_rec_inl(port)
#else  
#define __do_outb(val, port)	writeb(val,(PCI_IO_ADDR)_IO_BASE+port);
#define __do_outw(val, port)	writew(val,(PCI_IO_ADDR)_IO_BASE+port);
#define __do_outl(val, port)	writel(val,(PCI_IO_ADDR)_IO_BASE+port);
#define __do_inb(port)		readb((PCI_IO_ADDR)_IO_BASE + port);
#define __do_inw(port)		readw((PCI_IO_ADDR)_IO_BASE + port);
#define __do_inl(port)		readl((PCI_IO_ADDR)_IO_BASE + port);
#endif  
#ifdef CONFIG_EEH
#define __do_readsb(a, b, n)	eeh_readsb(PCI_FIX_ADDR(a), (b), (n))
#define __do_readsw(a, b, n)	eeh_readsw(PCI_FIX_ADDR(a), (b), (n))
#define __do_readsl(a, b, n)	eeh_readsl(PCI_FIX_ADDR(a), (b), (n))
#else  
#define __do_readsb(a, b, n)	_insb(PCI_FIX_ADDR(a), (b), (n))
#define __do_readsw(a, b, n)	_insw(PCI_FIX_ADDR(a), (b), (n))
#define __do_readsl(a, b, n)	_insl(PCI_FIX_ADDR(a), (b), (n))
#endif  
#define __do_writesb(a, b, n)	_outsb(PCI_FIX_ADDR(a),(b),(n))
#define __do_writesw(a, b, n)	_outsw(PCI_FIX_ADDR(a),(b),(n))
#define __do_writesl(a, b, n)	_outsl(PCI_FIX_ADDR(a),(b),(n))
#define __do_insb(p, b, n)	readsb((PCI_IO_ADDR)_IO_BASE+(p), (b), (n))
#define __do_insw(p, b, n)	readsw((PCI_IO_ADDR)_IO_BASE+(p), (b), (n))
#define __do_insl(p, b, n)	readsl((PCI_IO_ADDR)_IO_BASE+(p), (b), (n))
#define __do_outsb(p, b, n)	writesb((PCI_IO_ADDR)_IO_BASE+(p),(b),(n))
#define __do_outsw(p, b, n)	writesw((PCI_IO_ADDR)_IO_BASE+(p),(b),(n))
#define __do_outsl(p, b, n)	writesl((PCI_IO_ADDR)_IO_BASE+(p),(b),(n))
#define __do_memset_io(addr, c, n)	\
				_memset_io(PCI_FIX_ADDR(addr), c, n)
#define __do_memcpy_toio(dst, src, n)	\
				_memcpy_toio(PCI_FIX_ADDR(dst), src, n)
#ifdef CONFIG_EEH
#define __do_memcpy_fromio(dst, src, n)	\
				eeh_memcpy_fromio(dst, PCI_FIX_ADDR(src), n)
#else  
#define __do_memcpy_fromio(dst, src, n)	\
				_memcpy_fromio(dst,PCI_FIX_ADDR(src),n)
#endif  
#ifdef CONFIG_PPC_INDIRECT_PIO
#define DEF_PCI_HOOK_pio(x)	x
#else
#define DEF_PCI_HOOK_pio(x)	NULL
#endif
#ifdef CONFIG_PPC_INDIRECT_MMIO
#define DEF_PCI_HOOK_mem(x)	x
#else
#define DEF_PCI_HOOK_mem(x)	NULL
#endif
extern struct ppc_pci_io {
#define DEF_PCI_AC_RET(name, ret, at, al, space, aa)	ret (*name) at;
#define DEF_PCI_AC_NORET(name, at, al, space, aa)	void (*name) at;
#include <asm/io-defs.h>
#undef DEF_PCI_AC_RET
#undef DEF_PCI_AC_NORET
} ppc_pci_io;
#define DEF_PCI_AC_RET(name, ret, at, al, space, aa)		\
static inline ret name at					\
{								\
	if (DEF_PCI_HOOK_##space(ppc_pci_io.name) != NULL)	\
		return ppc_pci_io.name al;			\
	return __do_##name al;					\
}
#define DEF_PCI_AC_NORET(name, at, al, space, aa)		\
static inline void name at					\
{								\
	if (DEF_PCI_HOOK_##space(ppc_pci_io.name) != NULL)		\
		ppc_pci_io.name al;				\
	else							\
		__do_##name al;					\
}
#include <asm/io-defs.h>
#undef DEF_PCI_AC_RET
#undef DEF_PCI_AC_NORET
#define readb readb
#define readw readw
#define readl readl
#define writeb writeb
#define writew writew
#define writel writel
#define readsb readsb
#define readsw readsw
#define readsl readsl
#define writesb writesb
#define writesw writesw
#define writesl writesl
#define inb inb
#define inw inw
#define inl inl
#define outb outb
#define outw outw
#define outl outl
#define insb insb
#define insw insw
#define insl insl
#define outsb outsb
#define outsw outsw
#define outsl outsl
#ifdef __powerpc64__
#define readq	readq
#define writeq	writeq
#endif
#define memset_io memset_io
#define memcpy_fromio memcpy_fromio
#define memcpy_toio memcpy_toio
#define xlate_dev_mem_ptr(p)	__va(p)
#define readb_relaxed(addr)	readb(addr)
#define readw_relaxed(addr)	readw(addr)
#define readl_relaxed(addr)	readl(addr)
#define readq_relaxed(addr)	readq(addr)
#define writeb_relaxed(v, addr)	writeb(v, addr)
#define writew_relaxed(v, addr)	writew(v, addr)
#define writel_relaxed(v, addr)	writel(v, addr)
#define writeq_relaxed(v, addr)	writeq(v, addr)
#ifndef CONFIG_GENERIC_IOMAP
static inline unsigned int ioread16be(const void __iomem *addr)
{
	return readw_be(addr);
}
#define ioread16be ioread16be
static inline unsigned int ioread32be(const void __iomem *addr)
{
	return readl_be(addr);
}
#define ioread32be ioread32be
#ifdef __powerpc64__
static inline u64 ioread64_lo_hi(const void __iomem *addr)
{
	return readq(addr);
}
#define ioread64_lo_hi ioread64_lo_hi
static inline u64 ioread64_hi_lo(const void __iomem *addr)
{
	return readq(addr);
}
#define ioread64_hi_lo ioread64_hi_lo
static inline u64 ioread64be(const void __iomem *addr)
{
	return readq_be(addr);
}
#define ioread64be ioread64be
static inline u64 ioread64be_lo_hi(const void __iomem *addr)
{
	return readq_be(addr);
}
#define ioread64be_lo_hi ioread64be_lo_hi
static inline u64 ioread64be_hi_lo(const void __iomem *addr)
{
	return readq_be(addr);
}
#define ioread64be_hi_lo ioread64be_hi_lo
#endif  
static inline void iowrite16be(u16 val, void __iomem *addr)
{
	writew_be(val, addr);
}
#define iowrite16be iowrite16be
static inline void iowrite32be(u32 val, void __iomem *addr)
{
	writel_be(val, addr);
}
#define iowrite32be iowrite32be
#ifdef __powerpc64__
static inline void iowrite64_lo_hi(u64 val, void __iomem *addr)
{
	writeq(val, addr);
}
#define iowrite64_lo_hi iowrite64_lo_hi
static inline void iowrite64_hi_lo(u64 val, void __iomem *addr)
{
	writeq(val, addr);
}
#define iowrite64_hi_lo iowrite64_hi_lo
static inline void iowrite64be(u64 val, void __iomem *addr)
{
	writeq_be(val, addr);
}
#define iowrite64be iowrite64be
static inline void iowrite64be_lo_hi(u64 val, void __iomem *addr)
{
	writeq_be(val, addr);
}
#define iowrite64be_lo_hi iowrite64be_lo_hi
static inline void iowrite64be_hi_lo(u64 val, void __iomem *addr)
{
	writeq_be(val, addr);
}
#define iowrite64be_hi_lo iowrite64be_hi_lo
#endif  
struct pci_dev;
void pci_iounmap(struct pci_dev *dev, void __iomem *addr);
#define pci_iounmap pci_iounmap
void __iomem *ioport_map(unsigned long port, unsigned int len);
#define ioport_map ioport_map
#endif
static inline void iosync(void)
{
        __asm__ __volatile__ ("sync" : : : "memory");
}
#define iobarrier_rw() eieio()
#define iobarrier_r()  eieio()
#define iobarrier_w()  eieio()
#define inb_p(port)             inb(port)
#define outb_p(val, port)       (udelay(1), outb((val), (port)))
#define inw_p(port)             inw(port)
#define outw_p(val, port)       (udelay(1), outw((val), (port)))
#define inl_p(port)             inl(port)
#define outl_p(val, port)       (udelay(1), outl((val), (port)))
#define IO_SPACE_LIMIT ~(0UL)
extern void __iomem *ioremap(phys_addr_t address, unsigned long size);
#define ioremap ioremap
#define ioremap_prot ioremap_prot
extern void __iomem *ioremap_wc(phys_addr_t address, unsigned long size);
#define ioremap_wc ioremap_wc
#ifdef CONFIG_PPC32
void __iomem *ioremap_wt(phys_addr_t address, unsigned long size);
#define ioremap_wt ioremap_wt
#endif
void __iomem *ioremap_coherent(phys_addr_t address, unsigned long size);
#define ioremap_uc(addr, size)		ioremap((addr), (size))
#define ioremap_cache(addr, size) \
	ioremap_prot((addr), (size), pgprot_val(PAGE_KERNEL))
#define iounmap iounmap
void __iomem *ioremap_phb(phys_addr_t paddr, unsigned long size);
int early_ioremap_range(unsigned long ea, phys_addr_t pa,
			unsigned long size, pgprot_t prot);
extern void __iomem *__ioremap_caller(phys_addr_t, unsigned long size,
				      pgprot_t prot, void *caller);
#define HAVE_ARCH_PIO_SIZE		1
#define PIO_OFFSET			0x00000000UL
#define PIO_MASK			(FULL_IO_SIZE - 1)
#define PIO_RESERVED			(FULL_IO_SIZE)
#define mmio_read16be(addr)		readw_be(addr)
#define mmio_read32be(addr)		readl_be(addr)
#define mmio_read64be(addr)		readq_be(addr)
#define mmio_write16be(val, addr)	writew_be(val, addr)
#define mmio_write32be(val, addr)	writel_be(val, addr)
#define mmio_write64be(val, addr)	writeq_be(val, addr)
#define mmio_insb(addr, dst, count)	readsb(addr, dst, count)
#define mmio_insw(addr, dst, count)	readsw(addr, dst, count)
#define mmio_insl(addr, dst, count)	readsl(addr, dst, count)
#define mmio_outsb(addr, src, count)	writesb(addr, src, count)
#define mmio_outsw(addr, src, count)	writesw(addr, src, count)
#define mmio_outsl(addr, src, count)	writesl(addr, src, count)
static inline unsigned long virt_to_phys(volatile void * address)
{
	WARN_ON(IS_ENABLED(CONFIG_DEBUG_VIRTUAL) && !virt_addr_valid(address));
	return __pa((unsigned long)address);
}
#define virt_to_phys virt_to_phys
static inline void * phys_to_virt(unsigned long address)
{
	return (void *)__va(address);
}
#define phys_to_virt phys_to_virt
static inline phys_addr_t page_to_phys(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);
	WARN_ON(IS_ENABLED(CONFIG_DEBUG_VIRTUAL) && !pfn_valid(pfn));
	return PFN_PHYS(pfn);
}
#ifdef CONFIG_PPC32
static inline unsigned long virt_to_bus(volatile void * address)
{
        if (address == NULL)
		return 0;
        return __pa(address) + PCI_DRAM_OFFSET;
}
#define virt_to_bus virt_to_bus
static inline void * bus_to_virt(unsigned long address)
{
        if (address == 0)
		return NULL;
        return __va(address - PCI_DRAM_OFFSET);
}
#define bus_to_virt bus_to_virt
#endif  
#define setbits32(_addr, _v) out_be32((_addr), in_be32(_addr) |  (_v))
#define clrbits32(_addr, _v) out_be32((_addr), in_be32(_addr) & ~(_v))
#define setbits16(_addr, _v) out_be16((_addr), in_be16(_addr) |  (_v))
#define clrbits16(_addr, _v) out_be16((_addr), in_be16(_addr) & ~(_v))
#define setbits8(_addr, _v) out_8((_addr), in_8(_addr) |  (_v))
#define clrbits8(_addr, _v) out_8((_addr), in_8(_addr) & ~(_v))
#define clrsetbits(type, addr, clear, set) \
	out_##type((addr), (in_##type(addr) & ~(clear)) | (set))
#ifdef __powerpc64__
#define clrsetbits_be64(addr, clear, set) clrsetbits(be64, addr, clear, set)
#define clrsetbits_le64(addr, clear, set) clrsetbits(le64, addr, clear, set)
#endif
#define clrsetbits_be32(addr, clear, set) clrsetbits(be32, addr, clear, set)
#define clrsetbits_le32(addr, clear, set) clrsetbits(le32, addr, clear, set)
#define clrsetbits_be16(addr, clear, set) clrsetbits(be16, addr, clear, set)
#define clrsetbits_le16(addr, clear, set) clrsetbits(le16, addr, clear, set)
#define clrsetbits_8(addr, clear, set) clrsetbits(8, addr, clear, set)
#include <asm-generic/io.h>
#endif  
#endif  
