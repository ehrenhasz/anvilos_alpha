#ifndef _MEM_USER_H
#define _MEM_USER_H
struct iomem_region {
	struct iomem_region *next;
	char *driver;
	int fd;
	int size;
	unsigned long phys;
	unsigned long virt;
};
extern struct iomem_region *iomem_regions;
extern int iomem_size;
#define ROUND_4M(n) ((((unsigned long) (n)) + (1 << 22)) & ~((1 << 22) - 1))
extern unsigned long find_iomem(char *driver, unsigned long *len_out);
extern void mem_total_pages(unsigned long physmem, unsigned long iomem,
		     unsigned long highmem);
extern void setup_physmem(unsigned long start, unsigned long usable,
			  unsigned long len, unsigned long long highmem);
extern void map_memory(unsigned long virt, unsigned long phys,
		       unsigned long len, int r, int w, int x);
#endif
