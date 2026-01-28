
#ifndef BOOT_COMPRESSED_ERROR_H
#define BOOT_COMPRESSED_ERROR_H

#include <linux/compiler.h>

void warn(const char *m);
void error(char *m) __noreturn;
void panic(const char *fmt, ...) __noreturn __cold;

#endif 
