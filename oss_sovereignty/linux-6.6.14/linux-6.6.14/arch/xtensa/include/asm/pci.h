#ifndef _XTENSA_PCI_H
#define _XTENSA_PCI_H
#define pcibios_assign_all_busses()	0
#define PCIBIOS_MIN_IO		0x2000
#define PCIBIOS_MIN_MEM		0x10000000
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>
#define HAVE_PCI_MMAP			1
#define ARCH_GENERIC_PCI_MMAP_RESOURCE	1
#define arch_can_pci_mmap_io()		1
#endif	 
