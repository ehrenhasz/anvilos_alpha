 

 

 

 

#if (defined(_WIN32) || defined(_WIN64))
#ifndef _NC_WIN32_CURSES_H
#define _NC_WIN32_CURSES_H 1

struct winconmode
{
  unsigned long dwFlagIn;
  unsigned long dwFlagOut;
};

extern NCURSES_EXPORT(void*) _nc_console_fd2handle(int fd);
extern NCURSES_EXPORT(int)   _nc_console_setmode(void* handle, const struct winconmode* arg);
extern NCURSES_EXPORT(int)   _nc_console_getmode(void* handle, struct winconmode* arg);
extern NCURSES_EXPORT(int)   _nc_console_flush(void* handle);

 
#define SIGHUP  1
#define SIGKILL 9

#undef  getlogin
#define getlogin() getenv("USERNAME")

#undef  ttyname
#define ttyname(fd) NULL

#undef sleep
#define sleep(n) Sleep((n) * 1000)

#undef gettimeofday
#define gettimeofday(tv,tz) _nc_gettimeofday(tv,tz)

#endif  
#endif  
