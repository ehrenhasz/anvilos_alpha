

#include <stdint.h>
#include "resctrl.h"

struct read_format {
	__u64 nr;			 
	struct {
		__u64 value;		 
	} values[2];
};

static struct perf_event_attr pea_llc_miss;
static struct read_format rf_cqm;
static int fd_lm;
char llc_occup_path[1024];

static void initialize_perf_event_attr(void)
{
	pea_llc_miss.type = PERF_TYPE_HARDWARE;
	pea_llc_miss.size = sizeof(struct perf_event_attr);
	pea_llc_miss.read_format = PERF_FORMAT_GROUP;
	pea_llc_miss.exclude_kernel = 1;
	pea_llc_miss.exclude_hv = 1;
	pea_llc_miss.exclude_idle = 1;
	pea_llc_miss.exclude_callchain_kernel = 1;
	pea_llc_miss.inherit = 1;
	pea_llc_miss.exclude_guest = 1;
	pea_llc_miss.disabled = 1;
}

static void ioctl_perf_event_ioc_reset_enable(void)
{
	ioctl(fd_lm, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_lm, PERF_EVENT_IOC_ENABLE, 0);
}

static int perf_event_open_llc_miss(pid_t pid, int cpu_no)
{
	fd_lm = perf_event_open(&pea_llc_miss, pid, cpu_no, -1,
				PERF_FLAG_FD_CLOEXEC);
	if (fd_lm == -1) {
		perror("Error opening leader");
		ctrlc_handler(0, NULL, NULL);
		return -1;
	}

	return 0;
}

static void initialize_llc_perf(void)
{
	memset(&pea_llc_miss, 0, sizeof(struct perf_event_attr));
	memset(&rf_cqm, 0, sizeof(struct read_format));

	 
	initialize_perf_event_attr();

	pea_llc_miss.config = PERF_COUNT_HW_CACHE_MISSES;

	rf_cqm.nr = 1;
}

static int reset_enable_llc_perf(pid_t pid, int cpu_no)
{
	int ret = 0;

	ret = perf_event_open_llc_miss(pid, cpu_no);
	if (ret < 0)
		return ret;

	 
	ioctl_perf_event_ioc_reset_enable();

	return 0;
}

 
static int get_llc_perf(unsigned long *llc_perf_miss)
{
	__u64 total_misses;
	int ret;

	 

	ioctl(fd_lm, PERF_EVENT_IOC_DISABLE, 0);

	ret = read(fd_lm, &rf_cqm, sizeof(struct read_format));
	if (ret == -1) {
		perror("Could not get llc misses through perf");
		return -1;
	}

	total_misses = rf_cqm.values[0].value;
	*llc_perf_miss = total_misses;

	return 0;
}

 
static int get_llc_occu_resctrl(unsigned long *llc_occupancy)
{
	FILE *fp;

	fp = fopen(llc_occup_path, "r");
	if (!fp) {
		perror("Failed to open results file");

		return errno;
	}
	if (fscanf(fp, "%lu", llc_occupancy) <= 0) {
		perror("Could not get llc occupancy");
		fclose(fp);

		return -1;
	}
	fclose(fp);

	return 0;
}

 
static int print_results_cache(char *filename, int bm_pid,
			       unsigned long llc_value)
{
	FILE *fp;

	if (strcmp(filename, "stdio") == 0 || strcmp(filename, "stderr") == 0) {
		printf("Pid: %d \t LLC_value: %lu\n", bm_pid,
		       llc_value);
	} else {
		fp = fopen(filename, "a");
		if (!fp) {
			perror("Cannot open results file");

			return errno;
		}
		fprintf(fp, "Pid: %d \t llc_value: %lu\n", bm_pid, llc_value);
		fclose(fp);
	}

	return 0;
}

int measure_cache_vals(struct resctrl_val_param *param, int bm_pid)
{
	unsigned long llc_perf_miss = 0, llc_occu_resc = 0, llc_value = 0;
	int ret;

	 
	if (!strncmp(param->resctrl_val, CAT_STR, sizeof(CAT_STR))) {
		ret = get_llc_perf(&llc_perf_miss);
		if (ret < 0)
			return ret;
		llc_value = llc_perf_miss;
	}

	 
	if (!strncmp(param->resctrl_val, CMT_STR, sizeof(CMT_STR))) {
		ret = get_llc_occu_resctrl(&llc_occu_resc);
		if (ret < 0)
			return ret;
		llc_value = llc_occu_resc;
	}
	ret = print_results_cache(param->filename, bm_pid, llc_value);
	if (ret)
		return ret;

	return 0;
}

 
int cat_val(struct resctrl_val_param *param, size_t span)
{
	int memflush = 1, operation = 0, ret = 0;
	char *resctrl_val = param->resctrl_val;
	pid_t bm_pid;

	if (strcmp(param->filename, "") == 0)
		sprintf(param->filename, "stdio");

	bm_pid = getpid();

	 
	ret = taskset_benchmark(bm_pid, param->cpu_no);
	if (ret)
		return ret;

	 
	ret = write_bm_pid_to_resctrl(bm_pid, param->ctrlgrp, param->mongrp,
				      resctrl_val);
	if (ret)
		return ret;

	initialize_llc_perf();

	 
	while (1) {
		ret = param->setup(param);
		if (ret == END_OF_TESTS) {
			ret = 0;
			break;
		}
		if (ret < 0)
			break;
		ret = reset_enable_llc_perf(bm_pid, param->cpu_no);
		if (ret)
			break;

		if (run_fill_buf(span, memflush, operation, true)) {
			fprintf(stderr, "Error-running fill buffer\n");
			ret = -1;
			goto pe_close;
		}

		sleep(1);
		ret = measure_cache_vals(param, bm_pid);
		if (ret)
			goto pe_close;
	}

	return ret;

pe_close:
	close(fd_lm);
	return ret;
}

 
int show_cache_info(unsigned long sum_llc_val, int no_of_bits,
		    size_t cache_span, unsigned long max_diff,
		    unsigned long max_diff_percent, unsigned long num_of_runs,
		    bool platform, bool cmt)
{
	unsigned long avg_llc_val = 0;
	float diff_percent;
	long avg_diff = 0;
	int ret;

	avg_llc_val = sum_llc_val / num_of_runs;
	avg_diff = (long)abs(cache_span - avg_llc_val);
	diff_percent = ((float)cache_span - avg_llc_val) / cache_span * 100;

	ret = platform && abs((int)diff_percent) > max_diff_percent &&
	      (cmt ? (abs(avg_diff) > max_diff) : true);

	ksft_print_msg("%s Check cache miss rate within %d%%\n",
		       ret ? "Fail:" : "Pass:", max_diff_percent);

	ksft_print_msg("Percent diff=%d\n", abs((int)diff_percent));
	ksft_print_msg("Number of bits: %d\n", no_of_bits);
	ksft_print_msg("Average LLC val: %lu\n", avg_llc_val);
	ksft_print_msg("Cache span (%s): %zu\n", cmt ? "bytes" : "lines",
		       cache_span);

	return ret;
}
