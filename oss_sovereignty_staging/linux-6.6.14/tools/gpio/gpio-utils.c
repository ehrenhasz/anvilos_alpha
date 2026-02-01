
 

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include "gpio-utils.h"

#define CONSUMER "gpio-utils"

 

 
int gpiotools_request_line(const char *device_name, unsigned int *lines,
			   unsigned int num_lines,
			   struct gpio_v2_line_config *config,
			   const char *consumer)
{
	struct gpio_v2_line_request req;
	char *chrdev_name;
	int fd;
	int i;
	int ret;

	ret = asprintf(&chrdev_name, "/dev/%s", device_name);
	if (ret < 0)
		return -ENOMEM;

	fd = open(chrdev_name, 0);
	if (fd == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to open %s, %s\n",
			chrdev_name, strerror(errno));
		goto exit_free_name;
	}

	memset(&req, 0, sizeof(req));
	for (i = 0; i < num_lines; i++)
		req.offsets[i] = lines[i];

	req.config = *config;
	strcpy(req.consumer, consumer);
	req.num_lines = num_lines;

	ret = ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIO_GET_LINE_IOCTL", ret, strerror(errno));
	}

	if (close(fd) == -1)
		perror("Failed to close GPIO character device file");
exit_free_name:
	free(chrdev_name);
	return ret < 0 ? ret : req.fd;
}

 
int gpiotools_set_values(const int fd, struct gpio_v2_line_values *values)
{
	int ret;

	ret = ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, values);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIOHANDLE_SET_LINE_VALUES_IOCTL", ret,
			strerror(errno));
	}

	return ret;
}

 
int gpiotools_get_values(const int fd, struct gpio_v2_line_values *values)
{
	int ret;

	ret = ioctl(fd, GPIO_V2_LINE_GET_VALUES_IOCTL, values);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIOHANDLE_GET_LINE_VALUES_IOCTL", ret,
			strerror(errno));
	}

	return ret;
}

 
int gpiotools_release_line(const int fd)
{
	int ret;

	ret = close(fd);
	if (ret == -1) {
		perror("Failed to close GPIO LINE device file");
		ret = -errno;
	}

	return ret;
}

 
int gpiotools_get(const char *device_name, unsigned int line)
{
	int ret;
	unsigned int value;
	unsigned int lines[] = {line};

	ret = gpiotools_gets(device_name, lines, 1, &value);
	if (ret)
		return ret;
	return value;
}


 
int gpiotools_gets(const char *device_name, unsigned int *lines,
		   unsigned int num_lines, unsigned int *values)
{
	int fd, i;
	int ret;
	int ret_close;
	struct gpio_v2_line_config config;
	struct gpio_v2_line_values lv;

	memset(&config, 0, sizeof(config));
	config.flags = GPIO_V2_LINE_FLAG_INPUT;
	ret = gpiotools_request_line(device_name, lines, num_lines,
				     &config, CONSUMER);
	if (ret < 0)
		return ret;

	fd = ret;
	for (i = 0; i < num_lines; i++)
		gpiotools_set_bit(&lv.mask, i);
	ret = gpiotools_get_values(fd, &lv);
	if (!ret)
		for (i = 0; i < num_lines; i++)
			values[i] = gpiotools_test_bit(lv.bits, i);
	ret_close = gpiotools_release_line(fd);
	return ret < 0 ? ret : ret_close;
}

 
int gpiotools_set(const char *device_name, unsigned int line,
		  unsigned int value)
{
	unsigned int lines[] = {line};

	return gpiotools_sets(device_name, lines, 1, &value);
}

 
int gpiotools_sets(const char *device_name, unsigned int *lines,
		   unsigned int num_lines, unsigned int *values)
{
	int ret, i;
	struct gpio_v2_line_config config;

	memset(&config, 0, sizeof(config));
	config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	config.num_attrs = 1;
	config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
	for (i = 0; i < num_lines; i++) {
		gpiotools_set_bit(&config.attrs[0].mask, i);
		gpiotools_assign_bit(&config.attrs[0].attr.values,
				     i, values[i]);
	}
	ret = gpiotools_request_line(device_name, lines, num_lines,
				     &config, CONSUMER);
	if (ret < 0)
		return ret;

	return gpiotools_release_line(ret);
}
