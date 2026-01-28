#ifndef _GPIO_UTILS_H_
#define _GPIO_UTILS_H_
#include <stdbool.h>
#include <string.h>
#include <linux/types.h>
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
static inline int check_prefix(const char *str, const char *prefix)
{
	return strlen(str) > strlen(prefix) &&
		strncmp(str, prefix, strlen(prefix)) == 0;
}
int gpiotools_request_line(const char *device_name,
			   unsigned int *lines,
			   unsigned int num_lines,
			   struct gpio_v2_line_config *config,
			   const char *consumer);
int gpiotools_set_values(const int fd, struct gpio_v2_line_values *values);
int gpiotools_get_values(const int fd, struct gpio_v2_line_values *values);
int gpiotools_release_line(const int fd);
int gpiotools_get(const char *device_name, unsigned int line);
int gpiotools_gets(const char *device_name, unsigned int *lines,
		   unsigned int num_lines, unsigned int *values);
int gpiotools_set(const char *device_name, unsigned int line,
		  unsigned int value);
int gpiotools_sets(const char *device_name, unsigned int *lines,
		   unsigned int num_lines, unsigned int *values);
static inline void gpiotools_set_bit(__u64 *b, int n)
{
	*b |= _BITULL(n);
}
static inline void gpiotools_change_bit(__u64 *b, int n)
{
	*b ^= _BITULL(n);
}
static inline void gpiotools_clear_bit(__u64 *b, int n)
{
	*b &= ~_BITULL(n);
}
static inline int gpiotools_test_bit(__u64 b, int n)
{
	return !!(b & _BITULL(n));
}
static inline void gpiotools_assign_bit(__u64 *b, int n, bool value)
{
	if (value)
		gpiotools_set_bit(b, n);
	else
		gpiotools_clear_bit(b, n);
}
#endif  
