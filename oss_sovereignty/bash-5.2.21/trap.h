 

 

#if !defined (_TRAP_H_)
#define _TRAP_H_

#include "stdc.h"

#if !defined (SIG_DFL)
#include "bashtypes.h"
#include <signal.h>
#endif  

#if !defined (NSIG)
#define NSIG 64
#endif  

#define NO_SIG -1
#define DEFAULT_SIG	SIG_DFL
#define IGNORE_SIG	SIG_IGN

 
#define DEBUG_TRAP	NSIG
#define ERROR_TRAP	NSIG+1
#define RETURN_TRAP	NSIG+2
#define EXIT_TRAP 	0

 
#define BASH_NSIG	NSIG+3

 
#define DSIG_SIGPREFIX	0x01		 
#define DSIG_NOCASE	0x02		 

 
#define IMPOSSIBLE_TRAP_HANDLER (SigHandler *)initialize_traps

#define signal_object_p(x,f) (decode_signal (x,f) != NO_SIG)

#define TRAP_STRING(s) \
  (signal_is_trapped (s) && signal_is_ignored (s) == 0) ? trap_list[s] \
							: (char *)NULL

extern char *trap_list[];

extern int trapped_signal_received;
extern int wait_signal_received;
extern int running_trap;
extern int trap_saved_exit_value;
extern int suppress_debug_trap_verbose;

 
extern void initialize_traps PARAMS((void));

extern void run_pending_traps PARAMS((void));

extern void queue_sigchld_trap PARAMS((int));
extern void maybe_set_sigchld_trap PARAMS((char *));
extern void set_impossible_sigchld_trap PARAMS((void));
extern void set_sigchld_trap PARAMS((char *));

extern void set_debug_trap PARAMS((char *));
extern void set_error_trap PARAMS((char *));
extern void set_return_trap PARAMS((char *));

extern void maybe_set_debug_trap PARAMS((char *));
extern void maybe_set_error_trap PARAMS((char *));
extern void maybe_set_return_trap PARAMS((char *));

extern void set_sigint_trap PARAMS((char *));
extern void set_signal PARAMS((int, char *));

extern void restore_default_signal PARAMS((int));
extern void ignore_signal PARAMS((int));
extern int run_exit_trap PARAMS((void));
extern void run_trap_cleanup PARAMS((int));
extern int run_debug_trap PARAMS((void));
extern void run_error_trap PARAMS((void));
extern void run_return_trap PARAMS((void));

extern void free_trap_strings PARAMS((void));
extern void reset_signal_handlers PARAMS((void));
extern void restore_original_signals PARAMS((void));
extern void restore_traps PARAMS((void));

extern void get_original_signal PARAMS((int));
extern void get_all_original_signals PARAMS((void));

extern char *signal_name PARAMS((int));

extern int decode_signal PARAMS((char *, int));
extern void run_interrupt_trap PARAMS((int));
extern int maybe_call_trap_handler PARAMS((int));
extern int signal_is_special PARAMS((int));
extern int signal_is_trapped PARAMS((int));
extern int signal_is_pending PARAMS((int));
extern int signal_is_ignored PARAMS((int));
extern int signal_is_hard_ignored PARAMS((int));
extern void set_signal_hard_ignored PARAMS((int));
extern void set_signal_ignored PARAMS((int));
extern int signal_in_progress PARAMS((int));

extern void set_trap_state PARAMS((int));

extern int next_pending_trap PARAMS((int));
extern int first_pending_trap PARAMS((void));
extern void clear_pending_traps PARAMS((void));
extern int any_signals_trapped PARAMS((void));
extern void check_signals PARAMS((void));
extern void check_signals_and_traps PARAMS((void));

#endif  
