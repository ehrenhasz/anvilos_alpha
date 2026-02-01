 

 

#if !defined (_QUIT_H_)
#define _QUIT_H_

#include "sig.h"	 

 
extern volatile sig_atomic_t interrupt_state;
extern volatile sig_atomic_t terminating_signal;

 
#define QUIT \
  do { \
    if (terminating_signal) termsig_handler (terminating_signal); \
    if (interrupt_state) throw_to_top_level (); \
  } while (0)

#define SETINTERRUPT interrupt_state = 1
#define CLRINTERRUPT interrupt_state = 0

#define ADDINTERRUPT interrupt_state++
#define DELINTERRUPT interrupt_state--

#define ISINTERRUPT interrupt_state != 0

 

#define CHECK_TERMSIG \
  do { \
    if (terminating_signal) termsig_handler (terminating_signal); \
  } while (0)

#define LASTSIG() \
  (terminating_signal ? terminating_signal : (interrupt_state ? SIGINT : 0))

#define CHECK_WAIT_INTR \
  do { \
    if (wait_intr_flag && wait_signal_received && this_shell_builtin && (this_shell_builtin == wait_builtin)) \
      sh_longjmp (wait_intr_buf, 1); \
  } while (0)

#define RESET_SIGTERM \
  do { \
    sigterm_received = 0; \
  } while (0)

#define CHECK_SIGTERM \
  do { \
    if (sigterm_received) termsig_handler (SIGTERM); \
  } while (0)
#endif  
