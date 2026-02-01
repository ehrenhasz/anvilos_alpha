
 

#include "uffd-common.h"

#ifdef __NR_userfaultfd

#define BOUNCE_RANDOM		(1<<0)
#define BOUNCE_RACINGFAULTS	(1<<1)
#define BOUNCE_VERIFY		(1<<2)
#define BOUNCE_POLL		(1<<3)
static int bounces;

 
#define ALARM_INTERVAL_SECS 10
static char *zeropage;
pthread_attr_t attr;

#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

const char *examples =
	"# Run anonymous memory test on 100MiB region with 99999 bounces:\n"
	"./uffd-stress anon 100 99999\n\n"
	"# Run share memory test on 1GiB region with 99 bounces:\n"
	"./uffd-stress shmem 1000 99\n\n"
	"# Run hugetlb memory test on 256MiB region with 50 bounces:\n"
	"./uffd-stress hugetlb 256 50\n\n"
	"# Run the same hugetlb test but using private file:\n"
	"./uffd-stress hugetlb-private 256 50\n\n"
	"# 10MiB-~6GiB 999 bounces anonymous test, "
	"continue forever unless an error triggers\n"
	"while ./uffd-stress anon $[RANDOM % 6000 + 10] 999; do true; done\n\n";

static void usage(void)
{
	fprintf(stderr, "\nUsage: ./uffd-stress <test type> <MiB> <bounces>\n\n");
	fprintf(stderr, "Supported <test type>: anon, hugetlb, "
		"hugetlb-private, shmem, shmem-private\n\n");
	fprintf(stderr, "Examples:\n\n");
	fprintf(stderr, "%s", examples);
	exit(1);
}

static void uffd_stats_reset(struct uffd_args *args, unsigned long n_cpus)
{
	int i;

	for (i = 0; i < n_cpus; i++) {
		args[i].cpu = i;
		args[i].apply_wp = test_uffdio_wp;
		args[i].missing_faults = 0;
		args[i].wp_faults = 0;
		args[i].minor_faults = 0;
	}
}

static void *locking_thread(void *arg)
{
	unsigned long cpu = (unsigned long) arg;
	unsigned long page_nr;
	unsigned long long count;

	if (!(bounces & BOUNCE_RANDOM)) {
		page_nr = -bounces;
		if (!(bounces & BOUNCE_RACINGFAULTS))
			page_nr += cpu * nr_pages_per_cpu;
	}

	while (!finished) {
		if (bounces & BOUNCE_RANDOM) {
			if (getrandom(&page_nr, sizeof(page_nr), 0) != sizeof(page_nr))
				err("getrandom failed");
		} else
			page_nr += 1;
		page_nr %= nr_pages;
		pthread_mutex_lock(area_mutex(area_dst, page_nr));
		count = *area_count(area_dst, page_nr);
		if (count != count_verify[page_nr])
			err("page_nr %lu memory corruption %llu %llu",
			    page_nr, count, count_verify[page_nr]);
		count++;
		*area_count(area_dst, page_nr) = count_verify[page_nr] = count;
		pthread_mutex_unlock(area_mutex(area_dst, page_nr));
	}

	return NULL;
}

static int copy_page_retry(int ufd, unsigned long offset)
{
	return __copy_page(ufd, offset, true, test_uffdio_wp);
}

pthread_mutex_t uffd_read_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *uffd_read_thread(void *arg)
{
	struct uffd_args *args = (struct uffd_args *)arg;
	struct uffd_msg msg;

	pthread_mutex_unlock(&uffd_read_mutex);
	 

	for (;;) {
		if (uffd_read_msg(uffd, &msg))
			continue;
		uffd_handle_page_fault(&msg, args);
	}

	return NULL;
}

static void *background_thread(void *arg)
{
	unsigned long cpu = (unsigned long) arg;
	unsigned long page_nr, start_nr, mid_nr, end_nr;

	start_nr = cpu * nr_pages_per_cpu;
	end_nr = (cpu+1) * nr_pages_per_cpu;
	mid_nr = (start_nr + end_nr) / 2;

	 
	for (page_nr = start_nr; page_nr < mid_nr; page_nr++)
		copy_page_retry(uffd, page_nr * page_size);

	 
	if (test_uffdio_wp)
		wp_range(uffd, (unsigned long)area_dst + start_nr * page_size,
			nr_pages_per_cpu * page_size, true);

	 
	for (page_nr = mid_nr; page_nr < end_nr; page_nr++)
		copy_page_retry(uffd, page_nr * page_size);

	return NULL;
}

static int stress(struct uffd_args *args)
{
	unsigned long cpu;
	pthread_t locking_threads[nr_cpus];
	pthread_t uffd_threads[nr_cpus];
	pthread_t background_threads[nr_cpus];

	finished = 0;
	for (cpu = 0; cpu < nr_cpus; cpu++) {
		if (pthread_create(&locking_threads[cpu], &attr,
				   locking_thread, (void *)cpu))
			return 1;
		if (bounces & BOUNCE_POLL) {
			if (pthread_create(&uffd_threads[cpu], &attr, uffd_poll_thread, &args[cpu]))
				err("uffd_poll_thread create");
		} else {
			if (pthread_create(&uffd_threads[cpu], &attr,
					   uffd_read_thread,
					   (void *)&args[cpu]))
				return 1;
			pthread_mutex_lock(&uffd_read_mutex);
		}
		if (pthread_create(&background_threads[cpu], &attr,
				   background_thread, (void *)cpu))
			return 1;
	}
	for (cpu = 0; cpu < nr_cpus; cpu++)
		if (pthread_join(background_threads[cpu], NULL))
			return 1;

	 
	uffd_test_ops->release_pages(area_src);

	finished = 1;
	for (cpu = 0; cpu < nr_cpus; cpu++)
		if (pthread_join(locking_threads[cpu], NULL))
			return 1;

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		char c;
		if (bounces & BOUNCE_POLL) {
			if (write(pipefd[cpu*2+1], &c, 1) != 1)
				err("pipefd write error");
			if (pthread_join(uffd_threads[cpu],
					 (void *)&args[cpu]))
				return 1;
		} else {
			if (pthread_cancel(uffd_threads[cpu]))
				return 1;
			if (pthread_join(uffd_threads[cpu], NULL))
				return 1;
		}
	}

	return 0;
}

static int userfaultfd_stress(void)
{
	void *area;
	unsigned long nr;
	struct uffd_args args[nr_cpus];
	uint64_t mem_size = nr_pages * page_size;

	memset(args, 0, sizeof(struct uffd_args) * nr_cpus);

	if (uffd_test_ctx_init(UFFD_FEATURE_WP_UNPOPULATED, NULL))
		err("context init failed");

	if (posix_memalign(&area, page_size, page_size))
		err("out of memory");
	zeropage = area;
	bzero(zeropage, page_size);

	pthread_mutex_lock(&uffd_read_mutex);

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 16*1024*1024);

	while (bounces--) {
		printf("bounces: %d, mode:", bounces);
		if (bounces & BOUNCE_RANDOM)
			printf(" rnd");
		if (bounces & BOUNCE_RACINGFAULTS)
			printf(" racing");
		if (bounces & BOUNCE_VERIFY)
			printf(" ver");
		if (bounces & BOUNCE_POLL)
			printf(" poll");
		else
			printf(" read");
		printf(", ");
		fflush(stdout);

		if (bounces & BOUNCE_POLL)
			fcntl(uffd, F_SETFL, uffd_flags | O_NONBLOCK);
		else
			fcntl(uffd, F_SETFL, uffd_flags & ~O_NONBLOCK);

		 
		if (uffd_register(uffd, area_dst, mem_size,
				  true, test_uffdio_wp, false))
			err("register failure");

		if (area_dst_alias) {
			if (uffd_register(uffd, area_dst_alias, mem_size,
					  true, test_uffdio_wp, false))
				err("register failure alias");
		}

		 
		uffd_test_ops->release_pages(area_dst);

		uffd_stats_reset(args, nr_cpus);

		 
		if (stress(args))
			return 1;

		 
		if (test_uffdio_wp)
			wp_range(uffd, (unsigned long)area_dst,
				 nr_pages * page_size, false);

		 
		if (uffd_unregister(uffd, area_dst, mem_size))
			err("unregister failure");
		if (area_dst_alias) {
			if (uffd_unregister(uffd, area_dst_alias, mem_size))
				err("unregister failure alias");
		}

		 
		if (bounces & BOUNCE_VERIFY)
			for (nr = 0; nr < nr_pages; nr++)
				if (*area_count(area_dst, nr) != count_verify[nr])
					err("error area_count %llu %llu %lu\n",
					    *area_count(area_src, nr),
					    count_verify[nr], nr);

		 
		swap(area_src, area_dst);

		swap(area_src_alias, area_dst_alias);

		uffd_stats_report(args, nr_cpus);
	}

	return 0;
}

static void set_test_type(const char *type)
{
	if (!strcmp(type, "anon")) {
		test_type = TEST_ANON;
		uffd_test_ops = &anon_uffd_test_ops;
	} else if (!strcmp(type, "hugetlb")) {
		test_type = TEST_HUGETLB;
		uffd_test_ops = &hugetlb_uffd_test_ops;
		map_shared = true;
	} else if (!strcmp(type, "hugetlb-private")) {
		test_type = TEST_HUGETLB;
		uffd_test_ops = &hugetlb_uffd_test_ops;
	} else if (!strcmp(type, "shmem")) {
		map_shared = true;
		test_type = TEST_SHMEM;
		uffd_test_ops = &shmem_uffd_test_ops;
	} else if (!strcmp(type, "shmem-private")) {
		test_type = TEST_SHMEM;
		uffd_test_ops = &shmem_uffd_test_ops;
	}
}

static void parse_test_type_arg(const char *raw_type)
{
	uint64_t features = UFFD_API_FEATURES;

	set_test_type(raw_type);

	if (!test_type)
		err("failed to parse test type argument: '%s'", raw_type);

	if (test_type == TEST_HUGETLB)
		page_size = default_huge_page_size();
	else
		page_size = sysconf(_SC_PAGE_SIZE);

	if (!page_size)
		err("Unable to determine page size");
	if ((unsigned long) area_count(NULL, 0) + sizeof(unsigned long long) * 2
	    > page_size)
		err("Impossible to run this test");

	 

	if (userfaultfd_open(&features))
		err("Userfaultfd open failed");

	test_uffdio_wp = test_uffdio_wp &&
		(features & UFFD_FEATURE_PAGEFAULT_FLAG_WP);

	close(uffd);
	uffd = -1;
}

static void sigalrm(int sig)
{
	if (sig != SIGALRM)
		abort();
	test_uffdio_copy_eexist = true;
	alarm(ALARM_INTERVAL_SECS);
}

int main(int argc, char **argv)
{
	size_t bytes;

	if (argc < 4)
		usage();

	if (signal(SIGALRM, sigalrm) == SIG_ERR)
		err("failed to arm SIGALRM");
	alarm(ALARM_INTERVAL_SECS);

	parse_test_type_arg(argv[1]);
	bytes = atol(argv[2]) * 1024 * 1024;

	nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);

	nr_pages_per_cpu = bytes / page_size / nr_cpus;
	if (!nr_pages_per_cpu) {
		_err("invalid MiB");
		usage();
	}

	bounces = atoi(argv[3]);
	if (bounces <= 0) {
		_err("invalid bounces");
		usage();
	}
	nr_pages = nr_pages_per_cpu * nr_cpus;

	printf("nr_pages: %lu, nr_pages_per_cpu: %lu\n",
	       nr_pages, nr_pages_per_cpu);
	return userfaultfd_stress();
}

#else  

#warning "missing __NR_userfaultfd definition"

int main(void)
{
	printf("skip: Skipping userfaultfd test (missing __NR_userfaultfd)\n");
	return KSFT_SKIP;
}

#endif  
