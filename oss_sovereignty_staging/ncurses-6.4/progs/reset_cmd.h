 

 

 
#ifndef RESET_CMD_H
#define RESET_CMD_H 1
 

#define USE_LIBTINFO
#define __INTERNAL_CAPS_VISIBLE	 
#include <progs.priv.h>

#undef CTRL
#define CTRL(x)	((x) & 0x1f)

extern bool send_init_strings(int  , TTY *  );
extern void print_tty_chars(TTY *  , TTY *  );
extern void reset_flush(void);
extern void reset_start(FILE *  , bool  , bool   );
extern void reset_tty_settings(int  , TTY *  , int  );
extern void set_control_chars(TTY *  , int  , int  , int  );
extern void set_conversions(TTY *  );

#if HAVE_SIZECHANGE
extern void set_window_size(int  , short *  , short *  );
#endif

extern const char *_nc_progname;

 

#endif  
