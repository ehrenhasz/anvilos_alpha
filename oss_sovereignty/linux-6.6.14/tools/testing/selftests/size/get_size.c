
 

#include <sys/sysinfo.h>
#include <unistd.h>
#include <sys/syscall.h>

#define STDOUT_FILENO 1

static int print(const char *s)
{
	size_t len = 0;

	while (s[len] != '\0')
		len++;

	return syscall(SYS_write, STDOUT_FILENO, s, len);
}

static inline char *num_to_str(unsigned long num, char *buf, int len)
{
	unsigned int digit;

	 
	buf += len - 1;
	*buf = 0;
	do {
		digit = num % 10;
		*(--buf) = digit + '0';
		num /= 10;
	} while (num > 0);

	return buf;
}

static int print_num(unsigned long num)
{
	char num_buf[30];

	return print(num_to_str(num, num_buf, sizeof(num_buf)));
}

static int print_k_value(const char *s, unsigned long num, unsigned long units)
{
	unsigned long long temp;
	int ccode;

	print(s);

	temp = num;
	temp = (temp * units)/1024;
	num = temp;
	ccode = print_num(num);
	print("\n");
	return ccode;
}

 
void _start(void)
{
	int ccode;
	struct sysinfo info;
	unsigned long used;
	static const char *test_name = " get runtime memory use\n";

	print("TAP version 13\n");
	print("# Testing system size.\n");

	ccode = syscall(SYS_sysinfo, &info);
	if (ccode < 0) {
		print("not ok 1");
		print(test_name);
		print(" ---\n reason: \"could not get sysinfo\"\n ...\n");
		syscall(SYS_exit, ccode);
	}
	print("ok 1");
	print(test_name);

	 
	used = info.totalram - info.freeram - info.bufferram;
	print("# System runtime memory report (units in Kilobytes):\n");
	print(" ---\n");
	print_k_value(" Total:  ", info.totalram, info.mem_unit);
	print_k_value(" Free:   ", info.freeram, info.mem_unit);
	print_k_value(" Buffer: ", info.bufferram, info.mem_unit);
	print_k_value(" In use: ", used, info.mem_unit);
	print(" ...\n");
	print("1..1\n");

	syscall(SYS_exit, 0);
}
