

#ifndef _NEXT_POSIX_H
#define _NEXT_POSIX_H

#ifdef HAVE_NEXT
#include <sys/dir.h>


#undef NGROUPS_MAX
#define NGROUPS_MAX NGROUPS


#define dirent direct


pid_t posix_wait(int *);
#define wait(a) posix_wait(a)


pid_t getppid(void);
void vhangup(void);
int innetgr(const char *, const char *, const char *, const char *);


int tcgetattr(int, struct termios *);
int tcsetattr(int, int, const struct termios *);
int tcsetpgrp(int, pid_t);
speed_t cfgetospeed(const struct termios *);
speed_t cfgetispeed(const struct termios *);
int cfsetospeed(struct termios *, int);
int cfsetispeed(struct termios *, int);
#endif 
#endif 
