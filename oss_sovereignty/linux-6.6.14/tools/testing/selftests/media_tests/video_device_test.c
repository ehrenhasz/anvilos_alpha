

 

 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/videodev2.h>

#define PRIORITY_MAX 4

int priority_test(int fd)
{
	 

	enum v4l2_priority old_priority, new_priority, priority_to_compare;
	int ret;
	int result = 0;

	ret = ioctl(fd, VIDIOC_G_PRIORITY, &old_priority);
	if (ret < 0) {
		printf("Failed to get priority: %s\n", strerror(errno));
		return -1;
	}
	new_priority = (old_priority + 1) % PRIORITY_MAX;
	ret = ioctl(fd, VIDIOC_S_PRIORITY, &new_priority);
	if (ret < 0) {
		printf("Failed to set priority: %s\n", strerror(errno));
		return -1;
	}
	ret = ioctl(fd, VIDIOC_G_PRIORITY, &priority_to_compare);
	if (ret < 0) {
		printf("Failed to get new priority: %s\n", strerror(errno));
		result = -1;
		goto cleanup;
	}
	if (priority_to_compare != new_priority) {
		printf("Priority wasn't set - test failed\n");
		result = -1;
	}

cleanup:
	ret = ioctl(fd, VIDIOC_S_PRIORITY, &old_priority);
	if (ret < 0) {
		printf("Failed to restore priority: %s\n", strerror(errno));
		return -1;
	}
	return result;
}

int loop_test(int fd)
{
	int count;
	struct v4l2_tuner vtuner;
	struct v4l2_capability vcap;
	int ret;

	 
	srand((unsigned int) time(NULL));
	count = rand();

	printf("\nNote:\n"
	       "While test is running, remove the device or unbind\n"
	       "driver and ensure there are no use after free errors\n"
	       "and other Oops in the dmesg. When possible, enable KaSan\n"
	       "kernel config option for use-after-free error detection.\n\n");

	while (count > 0) {
		ret = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
		if (ret < 0)
			printf("VIDIOC_QUERYCAP errno %s\n", strerror(errno));
		else
			printf("Video device driver %s\n", vcap.driver);

		ret = ioctl(fd, VIDIOC_G_TUNER, &vtuner);
		if (ret < 0)
			printf("VIDIOC_G_TUNER, errno %s\n", strerror(errno));
		else
			printf("type %d rangelow %d rangehigh %d\n",
				vtuner.type, vtuner.rangelow, vtuner.rangehigh);
		sleep(10);
		count--;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	char video_dev[256];
	int fd;
	int test_result;

	if (argc < 2) {
		printf("Usage: %s [-d </dev/videoX>]\n", argv[0]);
		exit(-1);
	}

	 
	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
		case 'd':
			strncpy(video_dev, optarg, sizeof(video_dev) - 1);
			video_dev[sizeof(video_dev)-1] = '\0';
			break;
		default:
			printf("Usage: %s [-d </dev/videoX>]\n", argv[0]);
			exit(-1);
		}
	}

	 
	fd = open(video_dev, O_RDWR);
	if (fd == -1) {
		printf("Video Device open errno %s\n", strerror(errno));
		exit(-1);
	}

	test_result = priority_test(fd);
	if (!test_result)
		printf("Priority test - PASSED\n");
	else
		printf("Priority test - FAILED\n");

	loop_test(fd);
}
