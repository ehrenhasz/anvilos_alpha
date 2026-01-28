#ifndef __FIRMWARE_GOOGLE_MEMCONSOLE_H
#define __FIRMWARE_GOOGLE_MEMCONSOLE_H
#include <linux/types.h>
void memconsole_setup(ssize_t (*read_func)(char *, loff_t, size_t));
int memconsole_sysfs_init(void);
void memconsole_exit(void);
#endif  
