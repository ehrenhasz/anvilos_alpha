#ifndef _ASM_IA64_IO_H
#define _ASM_IA64_IO_H
#include <asm/unaligned.h>
#include <asm/early_ioremap.h>
#define __IA64_UNCACHED_OFFSET	RGN_BASE(RGN_UNCACHED)
#define IO_SPACE_LIMIT		0xffffffffffffffffUL
#define MAX_IO_SPACES_BITS		8
#define MAX_IO_SPACES			(1UL << MAX_IO_SPACES_BITS)
#define IO_SPACE_BITS			24
#define IO_SPACE_SIZE			(1UL << IO_SPACE_BITS)
#define IO_SPACE_NR(port)		((port) >> IO_SPACE_BITS)
#define IO_SPACE_BASE(space)		((space) << IO_SPACE_BITS)
#define IO_SPACE_PORT(port)		((port) & (IO_SPACE_SIZE - 1))
#define IO_SPACE_SPARSE_ENCODING(p)	((((p) >> 2) << 12) | ((p) & 0xfff))
struct io_space {
	unsigned long mmio_base;	 
	int sparse;
};
extern struct io_space io_space[];
extern unsigned int num_io_spaces;
# ifdef __KERNEL__
#define PIO_OFFSET		(1UL << (MAX_IO_SPACES_BITS + IO_SPACE_BITS))
#define PIO_MASK		(PIO_OFFSET - 1)
#define PIO_RESERVED		__IA64_UNCACHED_OFFSET
#define HAVE_ARCH_PIO_SIZE
#include <asm/intrinsics.h>
#include <asm/page.h>
#include <asm-generic/iomap.h>
static inline unsigned long
virt_to_phys (volatile void *address)
{
	return (unsigned long) address - PAGE_OFFSET;
}
#define virt_to_phys virt_to_phys
static inline void*
phys_to_virt (unsigned long address)
{
	return (void *) (address + PAGE_OFFSET);
}
#define phys_to_virt phys_to_virt
#define ARCH_HAS_VALID_PHYS_ADDR_RANGE
extern u64 kern_mem_attribute (unsigned long phys_addr, unsigned long size);
extern int valid_phys_addr_range (phys_addr_t addr, size_t count);  
extern int valid_mmap_phys_addr_range (unsigned long pfn, size_t count);
# endif  
#define __ia64_mf_a()	ia64_mfa()
static inline void*
__ia64_mk_io_addr (unsigned long port)
{
	struct io_space *space;
	unsigned long offset;
	space = &io_space[IO_SPACE_NR(port)];
	port = IO_SPACE_PORT(port);
	if (space->sparse)
		offset = IO_SPACE_SPARSE_ENCODING(port);
	else
		offset = port;
	return (void *) (space->mmio_base | offset);
}
#define inb inb
static inline unsigned int inb(unsigned long port)
{
	volatile unsigned char *addr = __ia64_mk_io_addr(port);
	unsigned char ret;
	ret = *addr;
	__ia64_mf_a();
	return ret;
}
#define inw inw
static inline unsigned int inw(unsigned long port)
{
	volatile unsigned short *addr = __ia64_mk_io_addr(port);
	unsigned short ret;
	ret = *addr;
	__ia64_mf_a();
	return ret;
}
#define inl inl
static inline unsigned int inl(unsigned long port)
{
	volatile unsigned int *addr = __ia64_mk_io_addr(port);
	unsigned int ret;
	ret = *addr;
	__ia64_mf_a();
	return ret;
}
#define outb outb
static inline void outb(unsigned char val, unsigned long port)
{
	volatile unsigned char *addr = __ia64_mk_io_addr(port);
	*addr = val;
	__ia64_mf_a();
}
#define outw outw
static inline void outw(unsigned short val, unsigned long port)
{
	volatile unsigned short *addr = __ia64_mk_io_addr(port);
	*addr = val;
	__ia64_mf_a();
}
#define outl outl
static inline void outl(unsigned int val, unsigned long port)
{
	volatile unsigned int *addr = __ia64_mk_io_addr(port);
	*addr = val;
	__ia64_mf_a();
}
#define insb insb
static inline void insb(unsigned long port, void *dst, unsigned long count)
{
	unsigned char *dp = dst;
	while (count--)
		*dp++ = inb(port);
}
#define insw insw
static inline void insw(unsigned long port, void *dst, unsigned long count)
{
	unsigned short *dp = dst;
	while (count--)
		put_unaligned(inw(port), dp++);
}
#define insl insl
static inline void insl(unsigned long port, void *dst, unsigned long count)
{
	unsigned int *dp = dst;
	while (count--)
		put_unaligned(inl(port), dp++);
}
#define outsb outsb
static inline void outsb(unsigned long port, const void *src,
		unsigned long count)
{
	const unsigned char *sp = src;
	while (count--)
		outb(*sp++, port);
}
#define outsw outsw
static inline void outsw(unsigned long port, const void *src,
		unsigned long count)
{
	const unsigned short *sp = src;
	while (count--)
		outw(get_unaligned(sp++), port);
}
#define outsl outsl
static inline void outsl(unsigned long port, const void *src,
		unsigned long count)
{
	const unsigned int *sp = src;
	while (count--)
		outl(get_unaligned(sp++), port);
}
# ifdef __KERNEL__
#define _PAGE_IOREMAP pgprot_val(PAGE_KERNEL)
extern void __iomem * ioremap_uc(unsigned long offset, unsigned long size);
#define ioremap_prot ioremap_prot
#define ioremap_cache ioremap
#define ioremap_uc ioremap_uc
#define iounmap iounmap
extern void memcpy_fromio(void *dst, const volatile void __iomem *src, long n);
extern void memcpy_toio(volatile void __iomem *dst, const void *src, long n);
extern void memset_io(volatile void __iomem *s, int c, long n);
#define memcpy_fromio memcpy_fromio
#define memcpy_toio memcpy_toio
#define memset_io memset_io
#define xlate_dev_mem_ptr xlate_dev_mem_ptr
#include <asm-generic/io.h>
#undef PCI_IOBASE
# endif  
#endif  
