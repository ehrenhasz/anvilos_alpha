#ifndef __ASM_OPENRISC_IO_H
#define __ASM_OPENRISC_IO_H
#include <linux/types.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#define IO_SPACE_LIMIT		0
#define HAVE_ARCH_PIO_SIZE	1
#define PIO_RESERVED		0X0UL
#define PIO_OFFSET		0
#define PIO_MASK		0
#define _PAGE_IOREMAP (pgprot_val(PAGE_KERNEL) | _PAGE_CI)
#include <asm-generic/io.h>
#endif
