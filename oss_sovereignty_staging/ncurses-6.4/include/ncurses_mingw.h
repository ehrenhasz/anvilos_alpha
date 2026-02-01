 

 

 

 

#ifdef _WIN32
#ifndef _NC_MINGWH
#define _NC_MINGWH

#define USE_CONSOLE_DRIVER 1

#undef  TERMIOS
#define TERMIOS 1

typedef unsigned char cc_t;
typedef unsigned int  tcflag_t;
typedef unsigned int  speed_t;
typedef unsigned short otcflag_t;
typedef unsigned char ospeed_t;

#define NCCS 18
struct termios
{
  tcflag_t	c_iflag;
  tcflag_t	c_oflag;
  tcflag_t	c_cflag;
  tcflag_t	c_lflag;
  char		c_line;
  cc_t		c_cc[NCCS];
  speed_t	c_ispeed;
  speed_t	c_ospeed;
};

extern NCURSES_EXPORT(int)  _nc_mingw_tcsetattr(
    int fd,
    int optional_actions,
    const struct termios* arg);
extern NCURSES_EXPORT(int)  _nc_mingw_tcgetattr(
    int fd,
    struct termios* arg);
extern NCURSES_EXPORT(int)  _nc_mingw_tcflush(
    int fd,
    int queue);
extern NCURSES_EXPORT(void) _nc_set_term_driver(void* term);

#endif  
#endif  
