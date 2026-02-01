

 

 

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/futex.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>

static inline void dcbf(volatile unsigned int *addr)
{
	__asm__ __volatile__ ("dcbf %y0; sync" : : "Z"(*(unsigned char *)addr) : "memory");
}

static void err_msg(char *msg)
{

	time_t now;
	time(&now);
	printf("=================================\n");
	printf("    Error: %s\n", msg);
	printf("    %s", ctime(&now));
	printf("=================================\n");
	exit(1);
}

static char *map1;
static char *map2;
static pid_t rim_process_pid;

 

 
static volatile unsigned int corruption_found;

 
#define MAX_THREADS 		64
#define THREAD_ID_BITS		8
#define THREAD_ID_MASK		((1 << THREAD_ID_BITS) - 1)
static unsigned int rim_thread_ids[MAX_THREADS];
static pthread_t rim_threads[MAX_THREADS];


 
#define RIM_CHUNK_SIZE  	1024
#define BITS_PER_BYTE 		8
#define WORD_SIZE     		(sizeof(unsigned int))
#define WORD_BITS		(WORD_SIZE * BITS_PER_BYTE)
#define WORDS_PER_CHUNK		(RIM_CHUNK_SIZE/WORD_SIZE)

static inline char *compute_chunk_start_addr(unsigned int thread_id)
{
	char *chunk_start;

	chunk_start = (char *)((unsigned long)map1 +
			       (thread_id * RIM_CHUNK_SIZE));

	return chunk_start;
}

 
#define WORD_OFFSET_BITS	(__builtin_ctz(WORDS_PER_CHUNK))
#define WORD_OFFSET_MASK	((1 << WORD_OFFSET_BITS) - 1)

static inline unsigned int compute_word_offset(char *start, unsigned int *addr)
{
	unsigned int delta_bytes, ret;
	delta_bytes = (unsigned long)addr - (unsigned long)start;

	ret = delta_bytes/WORD_SIZE;

	return ret;
}

 
#define SWEEP_ID_BITS		(WORD_BITS - (THREAD_ID_BITS + WORD_OFFSET_BITS))
#define SWEEP_ID_MASK		((1 << SWEEP_ID_BITS) - 1)

 
#define SWEEP_ID_SHIFT	0
#define WORD_OFFSET_SHIFT	(SWEEP_ID_BITS)
#define THREAD_ID_SHIFT		(WORD_OFFSET_BITS + SWEEP_ID_BITS)

 
static inline unsigned int compute_store_pattern(unsigned int tid,
						 unsigned int *addr,
						 unsigned int sweep_id)
{
	unsigned int ret = 0;
	char *start = compute_chunk_start_addr(tid);
	unsigned int word_offset = compute_word_offset(start, addr);

	ret += (tid & THREAD_ID_MASK) << THREAD_ID_SHIFT;
	ret += (word_offset & WORD_OFFSET_MASK) << WORD_OFFSET_SHIFT;
	ret += (sweep_id & SWEEP_ID_MASK) << SWEEP_ID_SHIFT;
	return ret;
}

 
static inline unsigned int extract_tid(unsigned int pattern)
{
	unsigned int ret;

	ret = (pattern >> THREAD_ID_SHIFT) & THREAD_ID_MASK;
	return ret;
}

 
static inline unsigned int extract_word_offset(unsigned int pattern)
{
	unsigned int ret;

	ret = (pattern >> WORD_OFFSET_SHIFT) & WORD_OFFSET_MASK;

	return ret;
}

 
static inline unsigned int extract_sweep_id(unsigned int pattern)

{
	unsigned int ret;

	ret = (pattern >> SWEEP_ID_SHIFT) & SWEEP_ID_MASK;

	return ret;
}

 
#define LOGDIR_NAME_SIZE 100
static char logdir[LOGDIR_NAME_SIZE];

static FILE *fp[MAX_THREADS];
static const char logfilename[] ="Thread-%02d-Chunk";

static inline void start_verification_log(unsigned int tid,
					  unsigned int *addr,
					  unsigned int cur_sweep_id,
					  unsigned int prev_sweep_id)
{
	FILE *f;
	char logfile[30];
	char path[LOGDIR_NAME_SIZE + 30];
	char separator[2] = "/";
	char *chunk_start = compute_chunk_start_addr(tid);
	unsigned int size = RIM_CHUNK_SIZE;

	sprintf(logfile, logfilename, tid);
	strcpy(path, logdir);
	strcat(path, separator);
	strcat(path, logfile);
	f = fopen(path, "w");

	if (!f) {
		err_msg("Unable to create logfile\n");
	}

	fp[tid] = f;

	fprintf(f, "----------------------------------------------------------\n");
	fprintf(f, "PID                = %d\n", rim_process_pid);
	fprintf(f, "Thread id          = %02d\n", tid);
	fprintf(f, "Chunk Start Addr   = 0x%016lx\n", (unsigned long)chunk_start);
	fprintf(f, "Chunk Size         = %d\n", size);
	fprintf(f, "Next Store Addr    = 0x%016lx\n", (unsigned long)addr);
	fprintf(f, "Current sweep-id   = 0x%08x\n", cur_sweep_id);
	fprintf(f, "Previous sweep-id  = 0x%08x\n", prev_sweep_id);
	fprintf(f, "----------------------------------------------------------\n");
}

static inline void log_anamoly(unsigned int tid, unsigned int *addr,
			       unsigned int expected, unsigned int observed)
{
	FILE *f = fp[tid];

	fprintf(f, "Thread %02d: Addr 0x%lx: Expected 0x%x, Observed 0x%x\n",
	        tid, (unsigned long)addr, expected, observed);
	fprintf(f, "Thread %02d: Expected Thread id   = %02d\n", tid, extract_tid(expected));
	fprintf(f, "Thread %02d: Observed Thread id   = %02d\n", tid, extract_tid(observed));
	fprintf(f, "Thread %02d: Expected Word offset = %03d\n", tid, extract_word_offset(expected));
	fprintf(f, "Thread %02d: Observed Word offset = %03d\n", tid, extract_word_offset(observed));
	fprintf(f, "Thread %02d: Expected sweep-id    = 0x%x\n", tid, extract_sweep_id(expected));
	fprintf(f, "Thread %02d: Observed sweep-id    = 0x%x\n", tid, extract_sweep_id(observed));
	fprintf(f, "----------------------------------------------------------\n");
}

static inline void end_verification_log(unsigned int tid, unsigned nr_anamolies)
{
	FILE *f = fp[tid];
	char logfile[30];
	char path[LOGDIR_NAME_SIZE + 30];
	char separator[] = "/";

	fclose(f);

	if (nr_anamolies == 0) {
		remove(path);
		return;
	}

	sprintf(logfile, logfilename, tid);
	strcpy(path, logdir);
	strcat(path, separator);
	strcat(path, logfile);

	printf("Thread %02d chunk has %d corrupted words. For details check %s\n",
		tid, nr_anamolies, path);
}

 
static void verify_chunk(unsigned int tid, unsigned int *next_store_addr,
		  unsigned int cur_sweep_id,
		  unsigned int prev_sweep_id)
{
	unsigned int *iter_ptr;
	unsigned int size = RIM_CHUNK_SIZE;
	unsigned int expected;
	unsigned int observed;
	char *chunk_start = compute_chunk_start_addr(tid);

	int nr_anamolies = 0;

	start_verification_log(tid, next_store_addr,
			       cur_sweep_id, prev_sweep_id);

	for (iter_ptr = (unsigned int *)chunk_start;
	     (unsigned long)iter_ptr < (unsigned long)chunk_start + size;
	     iter_ptr++) {
		unsigned int expected_sweep_id;

		if (iter_ptr < next_store_addr) {
			expected_sweep_id = cur_sweep_id;
		} else {
			expected_sweep_id = prev_sweep_id;
		}

		expected = compute_store_pattern(tid, iter_ptr, expected_sweep_id);

		dcbf((volatile unsigned int*)iter_ptr); 
		observed = *iter_ptr;

	        if (observed != expected) {
			nr_anamolies++;
			log_anamoly(tid, iter_ptr, expected, observed);
		}
	}

	end_verification_log(tid, nr_anamolies);
}

static void set_pthread_cpu(pthread_t th, int cpu)
{
	cpu_set_t run_cpu_mask;
	struct sched_param param;

	CPU_ZERO(&run_cpu_mask);
	CPU_SET(cpu, &run_cpu_mask);
	pthread_setaffinity_np(th, sizeof(cpu_set_t), &run_cpu_mask);

	param.sched_priority = 1;
	if (0 && sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		 
		fprintf(stderr, "could not set SCHED_FIFO, run as root?\n");
	}
}

static void set_mycpu(int cpu)
{
	cpu_set_t run_cpu_mask;
	struct sched_param param;

	CPU_ZERO(&run_cpu_mask);
	CPU_SET(cpu, &run_cpu_mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &run_cpu_mask);

	param.sched_priority = 1;
	if (0 && sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		fprintf(stderr, "could not set SCHED_FIFO, run as root?\n");
	}
}

static volatile int segv_wait;

static void segv_handler(int signo, siginfo_t *info, void *extra)
{
	while (segv_wait) {
		sched_yield();
	}

}

static void set_segv_handler(void)
{
	struct sigaction sa;

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = segv_handler;

	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}

int timeout = 0;
 
static void *rim_fn(void *arg)
{
	unsigned int tid = *((unsigned int *)arg);

	int size = RIM_CHUNK_SIZE;
	char *chunk_start = compute_chunk_start_addr(tid);

	unsigned int prev_sweep_id;
	unsigned int cur_sweep_id = 0;

	 
	unsigned int pattern = cur_sweep_id;
	unsigned int *pattern_ptr = &pattern;
	unsigned int *w_ptr, read_data;

	set_segv_handler();

	 
	for (w_ptr = (unsigned int *)chunk_start;
	     (unsigned long)w_ptr < (unsigned long)(chunk_start) + size;
	     w_ptr++) {

		*pattern_ptr = compute_store_pattern(tid, w_ptr, cur_sweep_id);
		*w_ptr = *pattern_ptr;
	}

	while (!corruption_found && !timeout) {
		prev_sweep_id = cur_sweep_id;
		cur_sweep_id = cur_sweep_id + 1;

		for (w_ptr = (unsigned int *)chunk_start;
		     (unsigned long)w_ptr < (unsigned long)(chunk_start) + size;
		     w_ptr++)  {
			unsigned int old_pattern;

			 
			old_pattern = compute_store_pattern(tid, w_ptr, prev_sweep_id);

			 
			dcbf((volatile unsigned int*)w_ptr);  

			 
			read_data = *w_ptr;  

			 
			if (read_data != old_pattern) {
				 
				corruption_found = 1;
			}

			 
			if (corruption_found || timeout) {
				 
				 
				verify_chunk(tid, w_ptr, cur_sweep_id, prev_sweep_id);

				return 0;
			}

			 
			*pattern_ptr = compute_store_pattern(tid, w_ptr, cur_sweep_id);

			 
			*w_ptr = *pattern_ptr;
		}
	}

	return NULL;
}


static unsigned long start_cpu = 0;
static unsigned long nrthreads = 4;

static pthread_t mem_snapshot_thread;

static void *mem_snapshot_fn(void *arg)
{
	int page_size = getpagesize();
	size_t size = page_size;
	void *tmp = malloc(size);

	while (!corruption_found && !timeout) {
		 
		segv_wait = 1;

		mprotect(map1, size, PROT_READ);

		 
		memcpy(tmp, map1, size);

		 
		memcpy(map2, tmp, size);
		 
		asm volatile("sync" ::: "memory");
		mprotect(map1, size, PROT_READ|PROT_WRITE);
		asm volatile("sync" ::: "memory");
		segv_wait = 0;

		usleep(1);  
	}

	return 0;
}

void alrm_sighandler(int sig)
{
	timeout = 1;
}

int main(int argc, char *argv[])
{
	int c;
	int page_size = getpagesize();
	time_t now;
	int i, dir_error;
	pthread_attr_t attr;
	key_t shm_key = (key_t) getpid();
	int shmid, run_time = 20 * 60;
	struct sigaction sa_alrm;

	snprintf(logdir, LOGDIR_NAME_SIZE,
		 "/tmp/logdir-%u", (unsigned int)getpid());
	while ((c = getopt(argc, argv, "r:hn:l:t:")) != -1) {
		switch(c) {
		case 'r':
			start_cpu = strtoul(optarg, NULL, 10);
			break;
		case 'h':
			printf("%s [-r <start_cpu>] [-n <nrthreads>] [-l <logdir>] [-t <timeout>]\n", argv[0]);
			exit(0);
			break;
		case 'n':
			nrthreads = strtoul(optarg, NULL, 10);
			break;
		case 'l':
			strncpy(logdir, optarg, LOGDIR_NAME_SIZE - 1);
			break;
		case 't':
			run_time = strtoul(optarg, NULL, 10);
			break;
		default:
			printf("invalid option\n");
			exit(0);
			break;
		}
	}

	if (nrthreads > MAX_THREADS)
		nrthreads = MAX_THREADS;

	shmid = shmget(shm_key, page_size, IPC_CREAT|0666);
	if (shmid < 0) {
		err_msg("Failed shmget\n");
	}

	map1 = shmat(shmid, NULL, 0);
	if (map1 == (void *) -1) {
		err_msg("Failed shmat");
	}

	map2 = shmat(shmid, NULL, 0);
	if (map2 == (void *) -1) {
		err_msg("Failed shmat");
	}

	dir_error = mkdir(logdir, 0755);

	if (dir_error) {
		err_msg("Failed mkdir");
	}

	printf("start_cpu list:%lu\n", start_cpu);
	printf("number of worker threads:%lu + 1 snapshot thread\n", nrthreads);
	printf("Allocated address:0x%016lx + secondary map:0x%016lx\n", (unsigned long)map1, (unsigned long)map2);
	printf("logdir at : %s\n", logdir);
	printf("Timeout: %d seconds\n", run_time);

	time(&now);
	printf("=================================\n");
	printf("     Starting Test\n");
	printf("     %s", ctime(&now));
	printf("=================================\n");

	for (i = 0; i < nrthreads; i++) {
		if (1 && !fork()) {
			prctl(PR_SET_PDEATHSIG, SIGKILL);
			set_mycpu(start_cpu + i);
			for (;;)
				sched_yield();
			exit(0);
		}
	}


	sa_alrm.sa_handler = &alrm_sighandler;
	sigemptyset(&sa_alrm.sa_mask);
	sa_alrm.sa_flags = 0;

	if (sigaction(SIGALRM, &sa_alrm, 0) == -1) {
		err_msg("Failed signal handler registration\n");
	}

	alarm(run_time);

	pthread_attr_init(&attr);
	for (i = 0; i < nrthreads; i++) {
		rim_thread_ids[i] = i;
		pthread_create(&rim_threads[i], &attr, rim_fn, &rim_thread_ids[i]);
		set_pthread_cpu(rim_threads[i], start_cpu + i);
	}

	pthread_create(&mem_snapshot_thread, &attr, mem_snapshot_fn, map1);
	set_pthread_cpu(mem_snapshot_thread, start_cpu + i);


	pthread_join(mem_snapshot_thread, NULL);
	for (i = 0; i < nrthreads; i++) {
		pthread_join(rim_threads[i], NULL);
	}

	if (!timeout) {
		time(&now);
		printf("=================================\n");
		printf("      Data Corruption Detected\n");
		printf("      %s", ctime(&now));
		printf("      See logfiles in %s\n", logdir);
		printf("=================================\n");
		return 1;
	}
	return 0;
}
