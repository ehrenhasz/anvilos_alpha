
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>

 

#define SZ_1GB	(1024 * 1024 * 1024UL)
#define SZ_1TB	(1024 * 1024 * 1024 * 1024UL)

#define MAP_CHUNK_SIZE	SZ_1GB

 

#define NR_CHUNKS_128TB   ((128 * SZ_1TB) / MAP_CHUNK_SIZE)  
#define NR_CHUNKS_256TB   (NR_CHUNKS_128TB * 2UL)
#define NR_CHUNKS_384TB   (NR_CHUNKS_128TB * 3UL)
#define NR_CHUNKS_3840TB  (NR_CHUNKS_128TB * 30UL)

#define ADDR_MARK_128TB  (1UL << 47)  
#define ADDR_MARK_256TB  (1UL << 48)  

#ifdef __aarch64__
#define HIGH_ADDR_MARK  ADDR_MARK_256TB
#define HIGH_ADDR_SHIFT 49
#define NR_CHUNKS_LOW   NR_CHUNKS_256TB
#define NR_CHUNKS_HIGH  NR_CHUNKS_3840TB
#else
#define HIGH_ADDR_MARK  ADDR_MARK_128TB
#define HIGH_ADDR_SHIFT 48
#define NR_CHUNKS_LOW   NR_CHUNKS_128TB
#define NR_CHUNKS_HIGH  NR_CHUNKS_384TB
#endif

static char *hind_addr(void)
{
	int bits = HIGH_ADDR_SHIFT + rand() % (63 - HIGH_ADDR_SHIFT);

	return (char *) (1UL << bits);
}

static int validate_addr(char *ptr, int high_addr)
{
	unsigned long addr = (unsigned long) ptr;

	if (high_addr) {
		if (addr < HIGH_ADDR_MARK) {
			printf("Bad address %lx\n", addr);
			return 1;
		}
		return 0;
	}

	if (addr > HIGH_ADDR_MARK) {
		printf("Bad address %lx\n", addr);
		return 1;
	}
	return 0;
}

static int validate_lower_address_hint(void)
{
	char *ptr;

	ptr = mmap((void *) (1UL << 45), MAP_CHUNK_SIZE, PROT_READ |
			PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (ptr == MAP_FAILED)
		return 0;

	return 1;
}

int main(int argc, char *argv[])
{
	char *ptr[NR_CHUNKS_LOW];
	char **hptr;
	char *hint;
	unsigned long i, lchunks, hchunks;

	for (i = 0; i < NR_CHUNKS_LOW; i++) {
		ptr[i] = mmap(NULL, MAP_CHUNK_SIZE, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if (ptr[i] == MAP_FAILED) {
			if (validate_lower_address_hint())
				return 1;
			break;
		}

		if (validate_addr(ptr[i], 0))
			return 1;
	}
	lchunks = i;
	hptr = (char **) calloc(NR_CHUNKS_HIGH, sizeof(char *));
	if (hptr == NULL)
		return 1;

	for (i = 0; i < NR_CHUNKS_HIGH; i++) {
		hint = hind_addr();
		hptr[i] = mmap(hint, MAP_CHUNK_SIZE, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if (hptr[i] == MAP_FAILED)
			break;

		if (validate_addr(hptr[i], 1))
			return 1;
	}
	hchunks = i;

	for (i = 0; i < lchunks; i++)
		munmap(ptr[i], MAP_CHUNK_SIZE);

	for (i = 0; i < hchunks; i++)
		munmap(hptr[i], MAP_CHUNK_SIZE);

	free(hptr);
	return 0;
}
