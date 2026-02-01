

 

 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/media.h>

#include "../kselftest.h"

int main(int argc, char **argv)
{
	int opt;
	char media_device[256];
	int count;
	struct media_device_info mdi;
	int ret;
	int fd;

	if (argc < 2) {
		printf("Usage: %s [-d </dev/mediaX>]\n", argv[0]);
		exit(-1);
	}

	 
	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
		case 'd':
			strncpy(media_device, optarg, sizeof(media_device) - 1);
			media_device[sizeof(media_device)-1] = '\0';
			break;
		default:
			printf("Usage: %s [-d </dev/mediaX>]\n", argv[0]);
			exit(-1);
		}
	}

	if (getuid() != 0)
		ksft_exit_skip("Please run the test as root - Exiting.\n");

	 
	srand((unsigned int) time(NULL));
	count = rand();

	 
	fd = open(media_device, O_RDWR);
	if (fd == -1) {
		printf("Media Device open errno %s\n", strerror(errno));
		exit(-1);
	}

	printf("\nNote:\n"
	       "While test is running, remove the device and\n"
	       "ensure there are no use after free errors and\n"
	       "other Oops in the dmesg. Enable KaSan kernel\n"
	       "config option for use-after-free error detection.\n\n");

	printf("Running test for %d iterations\n", count);

	while (count > 0) {
		ret = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
		if (ret < 0)
			printf("Media Device Info errno %s\n", strerror(errno));
		else
			printf("Media device model %s driver %s - count %d\n",
				mdi.model, mdi.driver, count);
		sleep(10);
		count--;
	}
}
