
 

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "vm_util.h"

#define MIN_FREE_PAGES	20
#define NR_HUGE_PAGES	10	 

#define validate_free_pages(exp_free)					\
	do {								\
		int fhp = get_free_hugepages();				\
		if (fhp != (exp_free)) {				\
			printf("Unexpected number of free huge "	\
				"pages line %d\n", __LINE__);		\
			exit(1);					\
		}							\
	} while (0)

unsigned long huge_page_size;
unsigned long base_page_size;

unsigned long get_free_hugepages(void)
{
	unsigned long fhp = 0;
	char *line = NULL;
	size_t linelen = 0;
	FILE *f = fopen("/proc/meminfo", "r");

	if (!f)
		return fhp;
	while (getline(&line, &linelen, f) > 0) {
		if (sscanf(line, "HugePages_Free:      %lu", &fhp) == 1)
			break;
	}

	free(line);
	fclose(f);
	return fhp;
}

void write_fault_pages(void *addr, unsigned long nr_pages)
{
	unsigned long i;

	for (i = 0; i < nr_pages; i++)
		*((unsigned long *)(addr + (i * huge_page_size))) = i;
}

void read_fault_pages(void *addr, unsigned long nr_pages)
{
	volatile unsigned long dummy = 0;
	unsigned long i;

	for (i = 0; i < nr_pages; i++) {
		dummy += *((unsigned long *)(addr + (i * huge_page_size)));

		 
		asm volatile("" : "+r" (dummy));
	}
}

int main(int argc, char **argv)
{
	unsigned long free_hugepages;
	void *addr, *addr2;
	int fd;
	int ret;

	huge_page_size = default_huge_page_size();
	if (!huge_page_size) {
		printf("Unable to determine huge page size, exiting!\n");
		exit(1);
	}
	base_page_size = sysconf(_SC_PAGE_SIZE);
	if (!huge_page_size) {
		printf("Unable to determine base page size, exiting!\n");
		exit(1);
	}

	free_hugepages = get_free_hugepages();
	if (free_hugepages < MIN_FREE_PAGES) {
		printf("Not enough free huge pages to test, exiting!\n");
		exit(1);
	}

	fd = memfd_create(argv[0], MFD_HUGETLB);
	if (fd < 0) {
		perror("memfd_create() failed");
		exit(1);
	}

	 
	addr = mmap(NULL, (NR_HUGE_PAGES + 2) * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
			-1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	if (munmap(addr, huge_page_size) ||
			munmap(addr + (NR_HUGE_PAGES + 1) * huge_page_size,
				huge_page_size)) {
		perror("munmap");
		exit(1);
	}
	addr = addr + huge_page_size;

	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	ret = madvise(addr - base_page_size, NR_HUGE_PAGES * huge_page_size,
		MADV_DONTNEED);
	if (!ret) {
		printf("Unexpected success of madvise call with invalid addr line %d\n",
				__LINE__);
			exit(1);
	}

	 
	ret = madvise(addr, (NR_HUGE_PAGES * huge_page_size) + base_page_size,
		MADV_DONTNEED);
	if (!ret) {
		printf("Unexpected success of madvise call with invalid length line %d\n",
				__LINE__);
			exit(1);
	}

	(void)munmap(addr, NR_HUGE_PAGES * huge_page_size);

	 
	addr = mmap(NULL, NR_HUGE_PAGES * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
			-1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	ret = madvise(addr + base_page_size,
			NR_HUGE_PAGES * huge_page_size - base_page_size,
			MADV_DONTNEED);
	if (!ret) {
		printf("Unexpected success of madvise call with unaligned start address %d\n",
				__LINE__);
			exit(1);
	}

	 
	if (madvise(addr,
			((NR_HUGE_PAGES - 1) * huge_page_size) + base_page_size,
			MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}

	 
	validate_free_pages(free_hugepages - 1);

	(void)munmap(addr, NR_HUGE_PAGES * huge_page_size);
	validate_free_pages(free_hugepages);

	 
	addr = mmap(NULL, NR_HUGE_PAGES * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
			-1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}

	 
	validate_free_pages(free_hugepages);

	(void)munmap(addr, NR_HUGE_PAGES * huge_page_size);

	 
	if (fallocate(fd, 0, 0, NR_HUGE_PAGES * huge_page_size)) {
		perror("fallocate");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	addr = mmap(NULL, NR_HUGE_PAGES * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	 
	read_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - (2 * NR_HUGE_PAGES));

	 
	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - (2 * NR_HUGE_PAGES));

	 
	if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
					0, NR_HUGE_PAGES * huge_page_size)) {
		perror("fallocate");
		exit(1);
	}
	validate_free_pages(free_hugepages);

	(void)munmap(addr, NR_HUGE_PAGES * huge_page_size);

	 
	if (fallocate(fd, 0, 0, NR_HUGE_PAGES * huge_page_size)) {
		perror("fallocate");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	addr = mmap(NULL, NR_HUGE_PAGES * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	 
	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_REMOVE)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages);
	(void)munmap(addr, NR_HUGE_PAGES * huge_page_size);

	 
	if (fallocate(fd, 0, 0, NR_HUGE_PAGES * huge_page_size)) {
		perror("fallocate");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	addr = mmap(NULL, NR_HUGE_PAGES * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	 
	write_fault_pages(addr, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	addr2 = mmap(NULL, NR_HUGE_PAGES * huge_page_size,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	if (addr2 == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	 
	read_fault_pages(addr2, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	write_fault_pages(addr2, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - (2 * NR_HUGE_PAGES));

	 
	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages - (2 * NR_HUGE_PAGES));

	 
	if (madvise(addr2, NR_HUGE_PAGES * huge_page_size, MADV_DONTNEED)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages - NR_HUGE_PAGES);

	 
	write_fault_pages(addr2, NR_HUGE_PAGES);
	validate_free_pages(free_hugepages - (2 * NR_HUGE_PAGES));

	 
	if (madvise(addr, NR_HUGE_PAGES * huge_page_size, MADV_REMOVE)) {
		perror("madvise");
		exit(1);
	}
	validate_free_pages(free_hugepages);

	(void)munmap(addr, NR_HUGE_PAGES * huge_page_size);
	(void)munmap(addr2, NR_HUGE_PAGES * huge_page_size);

	close(fd);
	return 0;
}
