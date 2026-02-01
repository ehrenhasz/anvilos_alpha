
 

#include "../kselftest_harness.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>


 
#include <lib/test_hmm_uapi.h>
#include <mm/gup_test.h>

struct hmm_buffer {
	void		*ptr;
	void		*mirror;
	unsigned long	size;
	int		fd;
	uint64_t	cpages;
	uint64_t	faults;
};

enum {
	HMM_PRIVATE_DEVICE_ONE,
	HMM_PRIVATE_DEVICE_TWO,
	HMM_COHERENCE_DEVICE_ONE,
	HMM_COHERENCE_DEVICE_TWO,
};

#define TWOMEG		(1 << 21)
#define HMM_BUFFER_SIZE (1024 << 12)
#define HMM_PATH_MAX    64
#define NTIMES		10

#define ALIGN(x, a) (((x) + (a - 1)) & (~((a) - 1)))
 

#ifndef FOLL_WRITE
#define FOLL_WRITE	0x01	 
#endif

#ifndef FOLL_LONGTERM
#define FOLL_LONGTERM   0x100  
#endif
FIXTURE(hmm)
{
	int		fd;
	unsigned int	page_size;
	unsigned int	page_shift;
};

FIXTURE_VARIANT(hmm)
{
	int     device_number;
};

FIXTURE_VARIANT_ADD(hmm, hmm_device_private)
{
	.device_number = HMM_PRIVATE_DEVICE_ONE,
};

FIXTURE_VARIANT_ADD(hmm, hmm_device_coherent)
{
	.device_number = HMM_COHERENCE_DEVICE_ONE,
};

FIXTURE(hmm2)
{
	int		fd0;
	int		fd1;
	unsigned int	page_size;
	unsigned int	page_shift;
};

FIXTURE_VARIANT(hmm2)
{
	int     device_number0;
	int     device_number1;
};

FIXTURE_VARIANT_ADD(hmm2, hmm2_device_private)
{
	.device_number0 = HMM_PRIVATE_DEVICE_ONE,
	.device_number1 = HMM_PRIVATE_DEVICE_TWO,
};

FIXTURE_VARIANT_ADD(hmm2, hmm2_device_coherent)
{
	.device_number0 = HMM_COHERENCE_DEVICE_ONE,
	.device_number1 = HMM_COHERENCE_DEVICE_TWO,
};

static int hmm_open(int unit)
{
	char pathname[HMM_PATH_MAX];
	int fd;

	snprintf(pathname, sizeof(pathname), "/dev/hmm_dmirror%d", unit);
	fd = open(pathname, O_RDWR, 0);
	if (fd < 0)
		fprintf(stderr, "could not open hmm dmirror driver (%s)\n",
			pathname);
	return fd;
}

static bool hmm_is_coherent_type(int dev_num)
{
	return (dev_num >= HMM_COHERENCE_DEVICE_ONE);
}

FIXTURE_SETUP(hmm)
{
	self->page_size = sysconf(_SC_PAGE_SIZE);
	self->page_shift = ffs(self->page_size) - 1;

	self->fd = hmm_open(variant->device_number);
	if (self->fd < 0 && hmm_is_coherent_type(variant->device_number))
		SKIP(exit(0), "DEVICE_COHERENT not available");
	ASSERT_GE(self->fd, 0);
}

FIXTURE_SETUP(hmm2)
{
	self->page_size = sysconf(_SC_PAGE_SIZE);
	self->page_shift = ffs(self->page_size) - 1;

	self->fd0 = hmm_open(variant->device_number0);
	if (self->fd0 < 0 && hmm_is_coherent_type(variant->device_number0))
		SKIP(exit(0), "DEVICE_COHERENT not available");
	ASSERT_GE(self->fd0, 0);
	self->fd1 = hmm_open(variant->device_number1);
	ASSERT_GE(self->fd1, 0);
}

FIXTURE_TEARDOWN(hmm)
{
	int ret = close(self->fd);

	ASSERT_EQ(ret, 0);
	self->fd = -1;
}

FIXTURE_TEARDOWN(hmm2)
{
	int ret = close(self->fd0);

	ASSERT_EQ(ret, 0);
	self->fd0 = -1;

	ret = close(self->fd1);
	ASSERT_EQ(ret, 0);
	self->fd1 = -1;
}

static int hmm_dmirror_cmd(int fd,
			   unsigned long request,
			   struct hmm_buffer *buffer,
			   unsigned long npages)
{
	struct hmm_dmirror_cmd cmd;
	int ret;

	 
	cmd.addr = (__u64)buffer->ptr;
	cmd.ptr = (__u64)buffer->mirror;
	cmd.npages = npages;

	for (;;) {
		ret = ioctl(fd, request, &cmd);
		if (ret == 0)
			break;
		if (errno == EINTR)
			continue;
		return -errno;
	}
	buffer->cpages = cmd.cpages;
	buffer->faults = cmd.faults;

	return 0;
}

static void hmm_buffer_free(struct hmm_buffer *buffer)
{
	if (buffer == NULL)
		return;

	if (buffer->ptr)
		munmap(buffer->ptr, buffer->size);
	free(buffer->mirror);
	free(buffer);
}

 
static int hmm_create_file(unsigned long size)
{
	char path[HMM_PATH_MAX];
	int fd;

	strcpy(path, "/tmp");
	fd = open(path, O_TMPFILE | O_EXCL | O_RDWR, 0600);
	if (fd >= 0) {
		int r;

		do {
			r = ftruncate(fd, size);
		} while (r == -1 && errno == EINTR);
		if (!r)
			return fd;
		close(fd);
	}
	return -1;
}

 
static unsigned int hmm_random(void)
{
	static int fd = -1;
	unsigned int r;

	if (fd < 0) {
		fd = open("/dev/urandom", O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "%s:%d failed to open /dev/urandom\n",
					__FILE__, __LINE__);
			return ~0U;
		}
	}
	read(fd, &r, sizeof(r));
	return r;
}

static void hmm_nanosleep(unsigned int n)
{
	struct timespec t;

	t.tv_sec = 0;
	t.tv_nsec = n;
	nanosleep(&t, NULL);
}

static int hmm_migrate_sys_to_dev(int fd,
				   struct hmm_buffer *buffer,
				   unsigned long npages)
{
	return hmm_dmirror_cmd(fd, HMM_DMIRROR_MIGRATE_TO_DEV, buffer, npages);
}

static int hmm_migrate_dev_to_sys(int fd,
				   struct hmm_buffer *buffer,
				   unsigned long npages)
{
	return hmm_dmirror_cmd(fd, HMM_DMIRROR_MIGRATE_TO_SYS, buffer, npages);
}

 
TEST_F(hmm, open_close)
{
}

 
TEST_F(hmm, anon_read)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;
	int val;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	i = 2 * self->page_size / sizeof(*ptr);
	for (ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = mprotect(buffer->ptr, size, PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	val = *(int *)(buffer->ptr + self->page_size);
	ASSERT_EQ(val, 0);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_READ, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	ptr = buffer->mirror;
	for (i = 0; i < 2 * self->page_size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], 0);
	for (; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, anon_read_prot)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = -i;

	 
	ret = mprotect(buffer->ptr, size, PROT_NONE);
	ASSERT_EQ(ret, 0);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_READ, buffer, npages);
	ASSERT_EQ(ret, -EFAULT);

	 
	ret = mprotect(buffer->ptr, size, PROT_READ);
	ASSERT_EQ(ret, 0);
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], -i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, anon_write)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, anon_write_prot)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_READ, buffer, 1);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, 1);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, -EPERM);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], 0);

	 
	ret = mprotect(buffer->ptr, size, PROT_WRITE | PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, anon_write_child)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	pid_t pid;
	int child_fd;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = -i;

	pid = fork();
	if (pid == -1)
		ASSERT_EQ(pid, 0);
	if (pid != 0) {
		waitpid(pid, &ret, 0);
		ASSERT_EQ(WIFEXITED(ret), 1);

		 
		for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
			ASSERT_EQ(ptr[i], i);
		return;
	}

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], -i);

	 
	child_fd = hmm_open(0);
	ASSERT_GE(child_fd, 0);

	 
	ret = hmm_dmirror_cmd(child_fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], -i);

	close(child_fd);
	exit(0);
}

 
TEST_F(hmm, anon_write_child_shared)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	pid_t pid;
	int child_fd;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = -i;

	pid = fork();
	if (pid == -1)
		ASSERT_EQ(pid, 0);
	if (pid != 0) {
		waitpid(pid, &ret, 0);
		ASSERT_EQ(WIFEXITED(ret), 1);

		 
		for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
			ASSERT_EQ(ptr[i], -i);
		return;
	}

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], -i);

	 
	child_fd = hmm_open(0);
	ASSERT_GE(child_fd, 0);

	 
	ret = hmm_dmirror_cmd(child_fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], -i);

	close(child_fd);
	exit(0);
}

 
TEST_F(hmm, anon_write_huge)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	void *old_ptr;
	void *map;
	int *ptr;
	int ret;

	size = 2 * TWOMEG;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	size = TWOMEG;
	npages = size >> self->page_shift;
	map = (void *)ALIGN((uintptr_t)buffer->ptr, size);
	ret = madvise(map, size, MADV_HUGEPAGE);
	ASSERT_EQ(ret, 0);
	old_ptr = buffer->ptr;
	buffer->ptr = map;

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	buffer->ptr = old_ptr;
	hmm_buffer_free(buffer);
}

 
static long file_read_ulong(char *file, const char *tag)
{
	int fd;
	char buf[2048];
	int len;
	char *p, *q;
	long val;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		 
		return -1;
	}

	len = read(fd, buf, sizeof(buf));
	close(fd);
	if (len < 0) {
		 
		return -1;
	}
	if (len == sizeof(buf)) {
		 
		return -1;
	}
	buf[len] = '\0';

	 
	if (tag) {
		p = strstr(buf, tag);
		if (!p)
			return -1;  
		p += strlen(tag);
	} else
		p = buf;

	val = strtol(p, &q, 0);
	if (*q != ' ') {
		 
		return -1;
	}

	return val;
}

 
TEST_F(hmm, anon_write_hugetlbfs)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long default_hsize;
	unsigned long i;
	int *ptr;
	int ret;

	default_hsize = file_read_ulong("/proc/meminfo", "Hugepagesize:");
	if (default_hsize < 0 || default_hsize*1024 < default_hsize)
		SKIP(return, "Huge page size could not be determined");
	default_hsize = default_hsize*1024;  

	size = ALIGN(TWOMEG, default_hsize);
	npages = size >> self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->ptr = mmap(NULL, size,
				   PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
				   -1, 0);
	if (buffer->ptr == MAP_FAILED) {
		free(buffer);
		SKIP(return, "Huge page could not be allocated");
	}

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	munmap(buffer->ptr, buffer->size);
	buffer->ptr = NULL;
	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, file_read)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;
	int fd;
	ssize_t len;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	fd = hmm_create_file(size);
	ASSERT_GE(fd, 0);

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = fd;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;
	len = pwrite(fd, buffer->mirror, size, 0);
	ASSERT_EQ(len, size);
	memset(buffer->mirror, 0, size);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ,
			   MAP_SHARED,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_READ, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, file_write)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;
	int fd;
	ssize_t len;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	fd = hmm_create_file(size);
	ASSERT_GE(fd, 0);

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = fd;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	len = pread(fd, buffer->mirror, size, 0);
	ASSERT_EQ(len, size);
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, migrate)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, migrate_fault)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	for (i = 0, ptr = buffer->ptr; i < size / (2 * sizeof(*ptr)); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

TEST_F(hmm, migrate_release)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_RELEASE, buffer, npages);
	ASSERT_EQ(ret, 0);

	 
	for (i = 0, ptr = buffer->ptr; i < size / (2 * sizeof(*ptr)); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, migrate_shared)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, -ENOENT);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm2, migrate_mixed)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	int *ptr;
	unsigned char *p;
	int ret;
	int val;

	npages = 6;
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	 
	buffer->ptr = mmap(NULL, size,
			   PROT_NONE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);
	p = buffer->ptr;

	 
	ret = hmm_migrate_sys_to_dev(self->fd1, buffer, npages);
	ASSERT_EQ(ret, -EINVAL);

	 
	ret = munmap(buffer->ptr + self->page_size, self->page_size);
	ASSERT_EQ(ret, 0);

	 
	ret = hmm_migrate_sys_to_dev(self->fd1, buffer, 3);
	ASSERT_EQ(ret, -EINVAL);

	 
	ret = mprotect(buffer->ptr + 2 * self->page_size, self->page_size,
				PROT_READ);
	ASSERT_EQ(ret, 0);
	ptr = (int *)(buffer->ptr + 2 * self->page_size);
	val = *ptr + 3;
	ASSERT_EQ(val, 3);

	 
	ret = mprotect(buffer->ptr + 3 * self->page_size, self->page_size,
				PROT_READ | PROT_WRITE);
	ASSERT_EQ(ret, 0);
	ptr = (int *)(buffer->ptr + 3 * self->page_size);
	*ptr = val;
	ret = mprotect(buffer->ptr + 3 * self->page_size, self->page_size,
				PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	ret = mprotect(buffer->ptr + 4 * self->page_size, 2 * self->page_size,
				PROT_READ | PROT_WRITE);
	ASSERT_EQ(ret, 0);
	ptr = (int *)(buffer->ptr + 4 * self->page_size);
	*ptr = val;
	ptr = (int *)(buffer->ptr + 5 * self->page_size);
	*ptr = val;

	 
	buffer->ptr = p + 2 * self->page_size;
	ret = hmm_migrate_sys_to_dev(self->fd1, buffer, 4);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, 4);

	 
	buffer->ptr = p + 5 * self->page_size;
	ret = hmm_migrate_sys_to_dev(self->fd0, buffer, 1);
	ASSERT_EQ(ret, -ENOENT);
	buffer->ptr = p;

	buffer->ptr = p;
	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, migrate_multiple)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	unsigned long c;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	for (c = 0; c < NTIMES; c++) {
		buffer = malloc(sizeof(*buffer));
		ASSERT_NE(buffer, NULL);

		buffer->fd = -1;
		buffer->size = size;
		buffer->mirror = malloc(size);
		ASSERT_NE(buffer->mirror, NULL);

		buffer->ptr = mmap(NULL, size,
				   PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS,
				   buffer->fd, 0);
		ASSERT_NE(buffer->ptr, MAP_FAILED);

		 
		for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
			ptr[i] = i;

		 
		ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
		ASSERT_EQ(ret, 0);
		ASSERT_EQ(buffer->cpages, npages);

		 
		for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
			ASSERT_EQ(ptr[i], i);

		 
		if (hmm_is_coherent_type(variant->device_number)) {
			ret = hmm_migrate_dev_to_sys(self->fd, buffer, npages);
			ASSERT_EQ(ret, 0);
			ASSERT_EQ(buffer->cpages, npages);
		}

		for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
			ASSERT_EQ(ptr[i], i);

		hmm_buffer_free(buffer);
	}
}

 
TEST_F(hmm, anon_read_multiple)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	unsigned long c;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	for (c = 0; c < NTIMES; c++) {
		buffer = malloc(sizeof(*buffer));
		ASSERT_NE(buffer, NULL);

		buffer->fd = -1;
		buffer->size = size;
		buffer->mirror = malloc(size);
		ASSERT_NE(buffer->mirror, NULL);

		buffer->ptr = mmap(NULL, size,
				   PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS,
				   buffer->fd, 0);
		ASSERT_NE(buffer->ptr, MAP_FAILED);

		 
		for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
			ptr[i] = i + c;

		 
		ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_READ, buffer,
				      npages);
		ASSERT_EQ(ret, 0);
		ASSERT_EQ(buffer->cpages, npages);
		ASSERT_EQ(buffer->faults, 1);

		 
		for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
			ASSERT_EQ(ptr[i], i + c);

		hmm_buffer_free(buffer);
	}
}

void *unmap_buffer(void *p)
{
	struct hmm_buffer *buffer = p;

	 
	hmm_nanosleep(hmm_random() % 32000);
	munmap(buffer->ptr + buffer->size / 2, buffer->size / 2);
	buffer->ptr = NULL;

	return NULL;
}

 
TEST_F(hmm, anon_teardown)
{
	unsigned long npages;
	unsigned long size;
	unsigned long c;
	void *ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	for (c = 0; c < NTIMES; ++c) {
		pthread_t thread;
		struct hmm_buffer *buffer;
		unsigned long i;
		int *ptr;
		int rc;

		buffer = malloc(sizeof(*buffer));
		ASSERT_NE(buffer, NULL);

		buffer->fd = -1;
		buffer->size = size;
		buffer->mirror = malloc(size);
		ASSERT_NE(buffer->mirror, NULL);

		buffer->ptr = mmap(NULL, size,
				   PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS,
				   buffer->fd, 0);
		ASSERT_NE(buffer->ptr, MAP_FAILED);

		 
		for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
			ptr[i] = i + c;

		rc = pthread_create(&thread, NULL, unmap_buffer, buffer);
		ASSERT_EQ(rc, 0);

		 
		rc = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_READ, buffer,
				     npages);
		if (rc == 0) {
			ASSERT_EQ(buffer->cpages, npages);
			ASSERT_EQ(buffer->faults, 1);

			 
			for (i = 0, ptr = buffer->mirror;
			     i < size / sizeof(*ptr);
			     ++i)
				ASSERT_EQ(ptr[i], i + c);
		}

		pthread_join(thread, &ret);
		hmm_buffer_free(buffer);
	}
}

 
TEST_F(hmm, mixedmap)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned char *m;
	int ret;

	npages = 1;
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(npages);
	ASSERT_NE(buffer->mirror, NULL);


	 
	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE,
			   self->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_SNAPSHOT, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	m = buffer->mirror;
	ASSERT_EQ(m[0], HMM_DMIRROR_PROT_READ);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm2, snapshot)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	int *ptr;
	unsigned char *p;
	unsigned char *m;
	int ret;
	int val;

	npages = 7;
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(npages);
	ASSERT_NE(buffer->mirror, NULL);

	 
	buffer->ptr = mmap(NULL, size,
			   PROT_NONE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);
	p = buffer->ptr;

	 
	ret = munmap(buffer->ptr + self->page_size, self->page_size);
	ASSERT_EQ(ret, 0);

	 
	ret = mprotect(buffer->ptr + 2 * self->page_size, self->page_size,
				PROT_READ);
	ASSERT_EQ(ret, 0);
	ptr = (int *)(buffer->ptr + 2 * self->page_size);
	val = *ptr + 3;
	ASSERT_EQ(val, 3);

	 
	ret = mprotect(buffer->ptr + 3 * self->page_size, self->page_size,
				PROT_READ | PROT_WRITE);
	ASSERT_EQ(ret, 0);
	ptr = (int *)(buffer->ptr + 3 * self->page_size);
	*ptr = val;
	ret = mprotect(buffer->ptr + 3 * self->page_size, self->page_size,
				PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	ret = mprotect(buffer->ptr + 4 * self->page_size, 3 * self->page_size,
				PROT_READ | PROT_WRITE);
	ASSERT_EQ(ret, 0);
	ptr = (int *)(buffer->ptr + 4 * self->page_size);
	*ptr = val;

	 
	buffer->ptr = p + 5 * self->page_size;
	ret = hmm_migrate_sys_to_dev(self->fd0, buffer, 1);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, 1);

	 
	buffer->ptr = p + 6 * self->page_size;
	ret = hmm_migrate_sys_to_dev(self->fd1, buffer, 1);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, 1);

	 
	buffer->ptr = p;
	ret = hmm_dmirror_cmd(self->fd0, HMM_DMIRROR_SNAPSHOT, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	m = buffer->mirror;
	ASSERT_EQ(m[0], HMM_DMIRROR_PROT_ERROR);
	ASSERT_EQ(m[1], HMM_DMIRROR_PROT_ERROR);
	ASSERT_EQ(m[2], HMM_DMIRROR_PROT_ZERO | HMM_DMIRROR_PROT_READ);
	ASSERT_EQ(m[3], HMM_DMIRROR_PROT_READ);
	ASSERT_EQ(m[4], HMM_DMIRROR_PROT_WRITE);
	if (!hmm_is_coherent_type(variant->device_number0)) {
		ASSERT_EQ(m[5], HMM_DMIRROR_PROT_DEV_PRIVATE_LOCAL |
				HMM_DMIRROR_PROT_WRITE);
		ASSERT_EQ(m[6], HMM_DMIRROR_PROT_NONE);
	} else {
		ASSERT_EQ(m[5], HMM_DMIRROR_PROT_DEV_COHERENT_LOCAL |
				HMM_DMIRROR_PROT_WRITE);
		ASSERT_EQ(m[6], HMM_DMIRROR_PROT_DEV_COHERENT_REMOTE |
				HMM_DMIRROR_PROT_WRITE);
	}

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, compound)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long default_hsize;
	int *ptr;
	unsigned char *m;
	int ret;
	unsigned long i;

	 

	default_hsize = file_read_ulong("/proc/meminfo", "Hugepagesize:");
	if (default_hsize < 0 || default_hsize*1024 < default_hsize)
		SKIP(return, "Huge page size could not be determined");
	default_hsize = default_hsize*1024;  

	size = ALIGN(TWOMEG, default_hsize);
	npages = size >> self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->ptr = mmap(NULL, size,
				   PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
				   -1, 0);
	if (buffer->ptr == MAP_FAILED) {
		free(buffer);
		return;
	}

	buffer->size = size;
	buffer->mirror = malloc(npages);
	ASSERT_NE(buffer->mirror, NULL);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_SNAPSHOT, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	m = buffer->mirror;
	for (i = 0; i < npages; ++i)
		ASSERT_EQ(m[i], HMM_DMIRROR_PROT_WRITE |
				HMM_DMIRROR_PROT_PMD);

	 
	ret = mprotect(buffer->ptr, size, PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_SNAPSHOT, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	m = buffer->mirror;
	for (i = 0; i < npages; ++i)
		ASSERT_EQ(m[i], HMM_DMIRROR_PROT_READ |
				HMM_DMIRROR_PROT_PMD);

	munmap(buffer->ptr, buffer->size);
	buffer->ptr = NULL;
	hmm_buffer_free(buffer);
}

 
TEST_F(hmm2, double_map)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = 6;
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(npages);
	ASSERT_NE(buffer->mirror, NULL);

	 
	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = mprotect(buffer->ptr, size, PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	ret = hmm_dmirror_cmd(self->fd0, HMM_DMIRROR_READ, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	ret = hmm_dmirror_cmd(self->fd1, HMM_DMIRROR_READ, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	ret = hmm_migrate_sys_to_dev(self->fd1, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	ret = hmm_dmirror_cmd(self->fd0, HMM_DMIRROR_READ, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	ASSERT_EQ(buffer->faults, 1);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, exclusive)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_EXCLUSIVE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i]++, i);

	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i+1);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_CHECK_EXCLUSIVE, buffer, npages);
	ASSERT_EQ(ret, 0);

	hmm_buffer_free(buffer);
}

TEST_F(hmm, exclusive_mprotect)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_EXCLUSIVE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	ret = mprotect(buffer->ptr, size, PROT_READ);
	ASSERT_EQ(ret, 0);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_WRITE, buffer, npages);
	ASSERT_EQ(ret, -EPERM);

	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, exclusive_cow)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;

	npages = ALIGN(HMM_BUFFER_SIZE, self->page_size) >> self->page_shift;
	ASSERT_NE(npages, 0);
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_EXCLUSIVE, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	fork();

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i]++, i);

	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i+1);

	hmm_buffer_free(buffer);
}

static int gup_test_exec(int gup_fd, unsigned long addr, int cmd,
			 int npages, int size, int flags)
{
	struct gup_test gup = {
		.nr_pages_per_call	= npages,
		.addr			= addr,
		.gup_flags		= FOLL_WRITE | flags,
		.size			= size,
	};

	if (ioctl(gup_fd, cmd, &gup)) {
		perror("ioctl on error\n");
		return errno;
	}

	return 0;
}

 
TEST_F(hmm, hmm_gup_test)
{
	struct hmm_buffer *buffer;
	int gup_fd;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;
	unsigned char *m;

	gup_fd = open("/sys/kernel/debug/gup_test", O_RDWR);
	if (gup_fd == -1)
		SKIP(return, "Skipping test, could not find gup_test driver");

	npages = 4;
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	 
	for (i = 0, ptr = buffer->mirror; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	ASSERT_EQ(gup_test_exec(gup_fd,
				(unsigned long)buffer->ptr,
				GUP_BASIC_TEST, 1, self->page_size, 0), 0);
	ASSERT_EQ(gup_test_exec(gup_fd,
				(unsigned long)buffer->ptr + 1 * self->page_size,
				GUP_FAST_BENCHMARK, 1, self->page_size, 0), 0);
	ASSERT_EQ(gup_test_exec(gup_fd,
				(unsigned long)buffer->ptr + 2 * self->page_size,
				PIN_FAST_BENCHMARK, 1, self->page_size, FOLL_LONGTERM), 0);
	ASSERT_EQ(gup_test_exec(gup_fd,
				(unsigned long)buffer->ptr + 3 * self->page_size,
				PIN_LONGTERM_BENCHMARK, 1, self->page_size, 0), 0);

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_SNAPSHOT, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	m = buffer->mirror;
	if (hmm_is_coherent_type(variant->device_number)) {
		ASSERT_EQ(HMM_DMIRROR_PROT_DEV_COHERENT_LOCAL | HMM_DMIRROR_PROT_WRITE, m[0]);
		ASSERT_EQ(HMM_DMIRROR_PROT_DEV_COHERENT_LOCAL | HMM_DMIRROR_PROT_WRITE, m[1]);
	} else {
		ASSERT_EQ(HMM_DMIRROR_PROT_WRITE, m[0]);
		ASSERT_EQ(HMM_DMIRROR_PROT_WRITE, m[1]);
	}
	ASSERT_EQ(HMM_DMIRROR_PROT_WRITE, m[2]);
	ASSERT_EQ(HMM_DMIRROR_PROT_WRITE, m[3]);
	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ASSERT_EQ(ptr[i], i);

	close(gup_fd);
	hmm_buffer_free(buffer);
}

 
TEST_F(hmm, hmm_cow_in_device)
{
	struct hmm_buffer *buffer;
	unsigned long npages;
	unsigned long size;
	unsigned long i;
	int *ptr;
	int ret;
	unsigned char *m;
	pid_t pid;
	int status;

	npages = 4;
	size = npages << self->page_shift;

	buffer = malloc(sizeof(*buffer));
	ASSERT_NE(buffer, NULL);

	buffer->fd = -1;
	buffer->size = size;
	buffer->mirror = malloc(size);
	ASSERT_NE(buffer->mirror, NULL);

	buffer->ptr = mmap(NULL, size,
			   PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   buffer->fd, 0);
	ASSERT_NE(buffer->ptr, MAP_FAILED);

	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 

	ret = hmm_migrate_sys_to_dev(self->fd, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);

	pid = fork();
	if (pid == -1)
		ASSERT_EQ(pid, 0);
	if (!pid) {
		 
		while (1) {
		}
		perror("Should not reach this\n");
		exit(0);
	}
	 
	for (i = 0, ptr = buffer->ptr; i < size / sizeof(*ptr); ++i)
		ptr[i] = i;

	 
	EXPECT_EQ(0, kill(pid, SIGTERM));
	EXPECT_EQ(pid, waitpid(pid, &status, 0));
	EXPECT_NE(0, WIFSIGNALED(status));
	EXPECT_EQ(SIGTERM, WTERMSIG(status));

	 
	ret = hmm_dmirror_cmd(self->fd, HMM_DMIRROR_SNAPSHOT, buffer, npages);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(buffer->cpages, npages);
	m = buffer->mirror;
	for (i = 0; i < npages; i++)
		ASSERT_EQ(HMM_DMIRROR_PROT_WRITE, m[i]);

	hmm_buffer_free(buffer);
}
TEST_HARNESS_MAIN
