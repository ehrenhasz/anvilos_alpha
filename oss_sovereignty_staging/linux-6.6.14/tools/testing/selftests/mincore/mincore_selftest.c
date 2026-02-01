
 

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>

#include "../kselftest.h"
#include "../kselftest_harness.h"

 
#define MB (1UL << 20)
#define FILE_SIZE (4 * MB)


 
TEST(basic_interface)
{
	int retval;
	int page_size;
	unsigned char vec[1];
	char *addr;

	page_size = sysconf(_SC_PAGESIZE);

	 
	retval = mincore(0, 0, vec);
	EXPECT_EQ(0, retval);

	 
	errno = 0;
	retval = mincore(NULL, page_size, vec);
	EXPECT_EQ(-1, retval);
	EXPECT_EQ(ENOMEM, errno);

	errno = 0;
	addr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	ASSERT_NE(MAP_FAILED, addr) {
		TH_LOG("mmap error: %s", strerror(errno));
	}

	 
	errno = 0;
	retval = mincore(addr + 1, page_size, vec);
	EXPECT_EQ(-1, retval);
	EXPECT_EQ(EINVAL, errno);

	 
	errno = 0;
	retval = mincore(addr, -1, vec);
	EXPECT_EQ(-1, retval);
	EXPECT_EQ(ENOMEM, errno);

	 
	errno = 0;
	retval = mincore(addr, page_size, NULL);
	EXPECT_EQ(-1, retval);
	EXPECT_EQ(EFAULT, errno);
	munmap(addr, page_size);
}


 
TEST(check_anonymous_locked_pages)
{
	unsigned char vec[1];
	char *addr;
	int retval;
	int page_size;

	page_size = sysconf(_SC_PAGESIZE);

	 
	errno = 0;
	addr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	ASSERT_NE(MAP_FAILED, addr) {
		TH_LOG("mmap error: %s", strerror(errno));
	}
	retval = mincore(addr, page_size, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(0, vec[0]) {
		TH_LOG("Page found in memory before use");
	}

	 
	addr[0] = 1;
	mlock(addr, page_size);
	retval = mincore(addr, page_size, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(1, vec[0]) {
		TH_LOG("Page not found in memory after use");
	}

	 
	munlock(addr, page_size);
	madvise(addr, page_size, MADV_DONTNEED);
	retval = mincore(addr, page_size, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(0, vec[0]) {
		TH_LOG("Page in memory after being zapped");
	}
	munmap(addr, page_size);
}


 
TEST(check_huge_pages)
{
	unsigned char vec[1];
	char *addr;
	int retval;
	int page_size;

	page_size = sysconf(_SC_PAGESIZE);

	errno = 0;
	addr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
		-1, 0);
	if (addr == MAP_FAILED) {
		if (errno == ENOMEM || errno == EINVAL)
			SKIP(return, "No huge pages available or CONFIG_HUGETLB_PAGE disabled.");
		else
			TH_LOG("mmap error: %s", strerror(errno));
	}
	retval = mincore(addr, page_size, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(0, vec[0]) {
		TH_LOG("Page found in memory before use");
	}

	addr[0] = 1;
	mlock(addr, page_size);
	retval = mincore(addr, page_size, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(1, vec[0]) {
		TH_LOG("Page not found in memory after use");
	}

	munlock(addr, page_size);
	munmap(addr, page_size);
}


 
TEST(check_file_mmap)
{
	unsigned char *vec;
	int vec_size;
	char *addr;
	int retval;
	int page_size;
	int fd;
	int i;
	int ra_pages = 0;

	page_size = sysconf(_SC_PAGESIZE);
	vec_size = FILE_SIZE / page_size;
	if (FILE_SIZE % page_size)
		vec_size++;

	vec = calloc(vec_size, sizeof(unsigned char));
	ASSERT_NE(NULL, vec) {
		TH_LOG("Can't allocate array");
	}

	errno = 0;
	fd = open(".", O_TMPFILE | O_RDWR, 0600);
	if (fd < 0) {
		ASSERT_EQ(errno, EOPNOTSUPP) {
			TH_LOG("Can't create temporary file: %s",
			       strerror(errno));
		}
		SKIP(goto out_free, "O_TMPFILE not supported by filesystem.");
	}
	errno = 0;
	retval = fallocate(fd, 0, 0, FILE_SIZE);
	if (retval) {
		ASSERT_EQ(errno, EOPNOTSUPP) {
			TH_LOG("Error allocating space for the temporary file: %s",
			       strerror(errno));
		}
		SKIP(goto out_close, "fallocate not supported by filesystem.");
	}

	 
	errno = 0;
	addr = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, addr) {
		TH_LOG("mmap error: %s", strerror(errno));
	}
	retval = mincore(addr, FILE_SIZE, vec);
	ASSERT_EQ(0, retval);
	for (i = 0; i < vec_size; i++) {
		ASSERT_EQ(0, vec[i]) {
			TH_LOG("Unexpected page in memory");
		}
	}

	 
	addr[FILE_SIZE / 2] = 1;
	retval = mincore(addr, FILE_SIZE, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(1, vec[FILE_SIZE / 2 / page_size]) {
		TH_LOG("Page not found in memory after use");
	}

	i = FILE_SIZE / 2 / page_size + 1;
	while (i < vec_size && vec[i]) {
		ra_pages++;
		i++;
	}
	EXPECT_GT(ra_pages, 0) {
		TH_LOG("No read-ahead pages found in memory");
	}

	EXPECT_LT(i, vec_size) {
		TH_LOG("Read-ahead pages reached the end of the file");
	}
	 
	if (i < vec_size) {
		while (i < vec_size && !vec[i])
			i++;
		EXPECT_EQ(vec_size, i) {
			TH_LOG("Unexpected page in memory beyond readahead window");
		}
	}

	munmap(addr, FILE_SIZE);
out_close:
	close(fd);
out_free:
	free(vec);
}


 
TEST(check_tmpfs_mmap)
{
	unsigned char *vec;
	int vec_size;
	char *addr;
	int retval;
	int page_size;
	int fd;
	int i;
	int ra_pages = 0;

	page_size = sysconf(_SC_PAGESIZE);
	vec_size = FILE_SIZE / page_size;
	if (FILE_SIZE % page_size)
		vec_size++;

	vec = calloc(vec_size, sizeof(unsigned char));
	ASSERT_NE(NULL, vec) {
		TH_LOG("Can't allocate array");
	}

	errno = 0;
	fd = open("/dev/shm", O_TMPFILE | O_RDWR, 0600);
	ASSERT_NE(-1, fd) {
		TH_LOG("Can't create temporary file: %s",
			strerror(errno));
	}
	errno = 0;
	retval = fallocate(fd, 0, 0, FILE_SIZE);
	ASSERT_EQ(0, retval) {
		TH_LOG("Error allocating space for the temporary file: %s",
			strerror(errno));
	}

	 
	errno = 0;
	addr = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, addr) {
		TH_LOG("mmap error: %s", strerror(errno));
	}
	retval = mincore(addr, FILE_SIZE, vec);
	ASSERT_EQ(0, retval);
	for (i = 0; i < vec_size; i++) {
		ASSERT_EQ(0, vec[i]) {
			TH_LOG("Unexpected page in memory");
		}
	}

	 
	addr[FILE_SIZE / 2] = 1;
	retval = mincore(addr, FILE_SIZE, vec);
	ASSERT_EQ(0, retval);
	ASSERT_EQ(1, vec[FILE_SIZE / 2 / page_size]) {
		TH_LOG("Page not found in memory after use");
	}

	i = FILE_SIZE / 2 / page_size + 1;
	while (i < vec_size && vec[i]) {
		ra_pages++;
		i++;
	}
	ASSERT_EQ(ra_pages, 0) {
		TH_LOG("Read-ahead pages found in memory");
	}

	munmap(addr, FILE_SIZE);
	close(fd);
	free(vec);
}

TEST_HARNESS_MAIN
