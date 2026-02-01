 

 

 
#ifndef TTY_SETTINGS_H
#define TTY_SETTINGS_H 1
 

#include <progs.priv.h>

extern int save_tty_settings(TTY *  , bool   );
extern void restore_tty_settings(void);
extern void update_tty_settings(TTY *  , TTY *   );

 

#endif  
