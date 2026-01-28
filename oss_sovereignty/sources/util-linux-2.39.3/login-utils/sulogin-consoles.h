
#ifndef UTIL_LINUX_SULOGIN_CONSOLES_H
#define UTIL_LINUX_SULOGIN_CONSOLES_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>

#include "list.h"
#include "ttyutils.h"

struct console {
	struct list_head entry;
	char *tty;
	FILE *file;
	uint32_t flags;
	int fd, id;
#define	CON_SERIAL	0x0001
#define	CON_NOTTY	0x0002
#define	CON_EIO		0x0004
	pid_t pid;
	struct chardata cp;
	struct termios tio;
};

extern int detect_consoles(const char *device, int fallback,
			   struct list_head *consoles);

extern void emergency_do_umounts(void);
extern void emergency_do_mounts(void);

#endif 
