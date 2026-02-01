
 

#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <err.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

static void *threadproc(void *ctx)
{
	 
	while (true)
		;

	return NULL;
}

#ifdef __x86_64__
extern unsigned long call32_from_64(void *stack, void (*function)(void));

asm (".pushsection .text\n\t"
     ".code32\n\t"
     "test_ss:\n\t"
     "pushl $0\n\t"
     "popl %eax\n\t"
     "ret\n\t"
     ".code64");
extern void test_ss(void);
#endif

int main()
{
	 
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0)
		printf("[WARN]\tsched_setaffinity failed\n");

	pthread_t thread;
	if (pthread_create(&thread, 0, threadproc, 0) != 0)
		err(1, "pthread_create");

#ifdef __x86_64__
	unsigned char *stack32 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
				      MAP_32BIT | MAP_ANONYMOUS | MAP_PRIVATE,
				      -1, 0);
	if (stack32 == MAP_FAILED)
		err(1, "mmap");
#endif

	printf("[RUN]\tSyscalls followed by SS validation\n");

	for (int i = 0; i < 1000; i++) {
		 
		usleep(2);

#ifdef __x86_64__
		 
		call32_from_64(stack32 + 4088, test_ss);
#endif
	}

	printf("[OK]\tWe survived\n");

#ifdef __x86_64__
	munmap(stack32, 4096);
#endif

	return 0;
}
