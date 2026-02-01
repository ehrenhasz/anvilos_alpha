 

 

#include "config.h"

#if !defined (__GNUC__) && !defined (HAVE_ALLOCA_H) && defined (_AIX)
  #pragma alloca
#endif  

#include <stdio.h>
#include "chartypes.h"
#include "bashtypes.h"
#if !defined (_MINIX) && defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif
#include "filecntl.h"
#include "posixstat.h"
#include <signal.h>
#if defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "posixtime.h"

#if defined (HAVE_SYS_RESOURCE_H) && !defined (RLIMTYPE)
#  include <sys/resource.h>
#endif

#if defined (HAVE_SYS_TIMES_H) && defined (HAVE_TIMES)
#  include <sys/times.h>
#endif

#include <errno.h>

#if !defined (errno)
extern int errno;
#endif

#define NEED_FPURGE_DECL
#define NEED_SH_SETLINEBUF_DECL

#include "bashansi.h"
#include "bashintl.h"

#include "memalloc.h"
#include "shell.h"
#include <y.tab.h>	 
#include "parser.h"
#include "flags.h"
#include "builtins.h"
#include "hashlib.h"
#include "jobs.h"
#include "execute_cmd.h"
#include "findcmd.h"
#include "redir.h"
#include "trap.h"
#include "pathexp.h"
#include "hashcmd.h"

#if defined (COND_COMMAND)
#  include "test.h"
#endif

#include "builtins/common.h"
#include "builtins/builtext.h"	 

#include "builtins/getopt.h"

#include <glob/strmatch.h>
#include <tilde/tilde.h>

#if defined (BUFFERED_INPUT)
#  include "input.h"
#endif

#if defined (ALIAS)
#  include "alias.h"
#endif

#if defined (HISTORY)
#  include "bashhist.h"
#endif

#if defined (HAVE_MBSTR_H) && defined (HAVE_MBSCHR)
#  include <mbstr.h>		 
#endif

extern int command_string_index;
extern char *the_printed_command;
extern time_t shell_start_time;
#if defined (HAVE_GETTIMEOFDAY)
extern struct timeval shellstart;
#endif
#if 0
extern char *glob_argv_flags;
#endif

extern int close PARAMS((int));

 
static void close_pipes PARAMS((int, int));
static void do_piping PARAMS((int, int));
static void bind_lastarg PARAMS((char *));
static int shell_control_structure PARAMS((enum command_type));
static void cleanup_redirects PARAMS((REDIRECT *));

#if defined (JOB_CONTROL)
static int restore_signal_mask PARAMS((sigset_t *));
#endif

static int builtin_status PARAMS((int));

static int execute_for_command PARAMS((FOR_COM *));
#if defined (SELECT_COMMAND)
static int displen PARAMS((const char *));
static int print_index_and_element PARAMS((int, int, WORD_LIST *));
static void indent PARAMS((int, int));
static void print_select_list PARAMS((WORD_LIST *, int, int, int));
static char *select_query PARAMS((WORD_LIST *, int, char *, int));
static int execute_select_command PARAMS((SELECT_COM *));
#endif
#if defined (DPAREN_ARITHMETIC)
static int execute_arith_command PARAMS((ARITH_COM *));
#endif
#if defined (COND_COMMAND)
static int execute_cond_node PARAMS((COND_COM *));
static int execute_cond_command PARAMS((COND_COM *));
#endif
#if defined (COMMAND_TIMING)
static int mkfmt PARAMS((char *, int, int, time_t, int));
static void print_formatted_time PARAMS((FILE *, char *,
				      time_t, int, time_t, int,
				      time_t, int, int));
static int time_command PARAMS((COMMAND *, int, int, int, struct fd_bitmap *));
#endif
#if defined (ARITH_FOR_COMMAND)
static intmax_t eval_arith_for_expr PARAMS((WORD_LIST *, int *));
static int execute_arith_for_command PARAMS((ARITH_FOR_COM *));
#endif
static int execute_case_command PARAMS((CASE_COM *));
static int execute_while_command PARAMS((WHILE_COM *));
static int execute_until_command PARAMS((WHILE_COM *));
static int execute_while_or_until PARAMS((WHILE_COM *, int));
static int execute_if_command PARAMS((IF_COM *));
static int execute_null_command PARAMS((REDIRECT *, int, int, int));
static void fix_assignment_words PARAMS((WORD_LIST *));
static void fix_arrayref_words PARAMS((WORD_LIST *));
static int execute_simple_command PARAMS((SIMPLE_COM *, int, int, int, struct fd_bitmap *));
static int execute_builtin PARAMS((sh_builtin_func_t *, WORD_LIST *, int, int));
static int execute_function PARAMS((SHELL_VAR *, WORD_LIST *, int, struct fd_bitmap *, int, int));
static int execute_builtin_or_function PARAMS((WORD_LIST *, sh_builtin_func_t *,
					    SHELL_VAR *,
					    REDIRECT *, struct fd_bitmap *, int));
static void execute_subshell_builtin_or_function PARAMS((WORD_LIST *, REDIRECT *,
						      sh_builtin_func_t *,
						      SHELL_VAR *,
						      int, int, int,
						      struct fd_bitmap *,
						      int));
static int execute_disk_command PARAMS((WORD_LIST *, REDIRECT *, char *,
				      int, int, int, struct fd_bitmap *, int));

static char *getinterp PARAMS((char *, int, int *));
static void initialize_subshell PARAMS((void));
static int execute_in_subshell PARAMS((COMMAND *, int, int, int, struct fd_bitmap *));
#if defined (COPROCESS_SUPPORT)
static void coproc_setstatus PARAMS((struct coproc *, int));
static int execute_coproc PARAMS((COMMAND *, int, int, struct fd_bitmap *));
#endif

static int execute_pipeline PARAMS((COMMAND *, int, int, int, struct fd_bitmap *));

static int execute_connection PARAMS((COMMAND *, int, int, int, struct fd_bitmap *));

static int execute_intern_function PARAMS((WORD_DESC *, FUNCTION_DEF *));

 
int stdin_redir;

 
char *this_command_name;

 
char *the_printed_command_except_trap;

 
int return_catch_flag;
int return_catch_value;
procenv_t return_catch;

 
volatile int last_command_exit_value;

 
int last_command_exit_signal;

 
int builtin_ignoring_errexit = 0;

 
REDIRECT *redirection_undo_list = (REDIRECT *)NULL;

 
REDIRECT *exec_redirection_undo_list = (REDIRECT *)NULL;

 
int executing_builtin = 0;

 
int executing_list = 0;

 
int comsub_ignore_return = 0;

 
int subshell_environment;

 
int subshell_level = 0;

 
SHELL_VAR *this_shell_function;

 
int match_ignore_case = 0;

int executing_command_builtin = 0;

struct stat SB;		 

static int special_builtin_failed;

static COMMAND *currently_executing_command;

 
static int function_line_number;

 
static int showing_function_line;

static int connection_count;

 
int line_number_for_err_trap;

 
#define SET_LINE_NUMBER(v) \
do { \
  line_number = v; \
  if (signal_in_progress (ERROR_TRAP) == 0 && running_trap != (ERROR_TRAP + 1)) \
    line_number_for_err_trap = line_number; \
} while (0)

 
#define GET_LINE_NUMBER() \
  (signal_in_progress (ERROR_TRAP) && running_trap == ERROR_TRAP+1) \
    ? line_number_for_err_trap \
    : executing_line_number ()

 
int funcnest = 0;
int funcnest_max = 0;

int evalnest = 0;
int evalnest_max = EVALNEST_MAX;

int sourcenest = 0;
int sourcenest_max = SOURCENEST_MAX;

volatile int from_return_trap = 0;

int lastpipe_opt = 0;

struct fd_bitmap *current_fds_to_close = (struct fd_bitmap *)NULL;

#define FD_BITMAP_DEFAULT_SIZE 32

 
struct fd_bitmap *
new_fd_bitmap (size)
     int size;
{
  struct fd_bitmap *ret;

  ret = (struct fd_bitmap *)xmalloc (sizeof (struct fd_bitmap));

  ret->size = size;

  if (size)
    {
      ret->bitmap = (char *)xmalloc (size);
      memset (ret->bitmap, '\0', size);
    }
  else
    ret->bitmap = (char *)NULL;
  return (ret);
}

void
dispose_fd_bitmap (fdbp)
     struct fd_bitmap *fdbp;
{
  FREE (fdbp->bitmap);
  free (fdbp);
}

void
close_fd_bitmap (fdbp)
     struct fd_bitmap *fdbp;
{
  register int i;

  if (fdbp)
    {
      for (i = 0; i < fdbp->size; i++)
	if (fdbp->bitmap[i])
	  {
	    close (i);
	    fdbp->bitmap[i] = 0;
	  }
    }
}

 
int
executing_line_number ()
{
  if (executing && showing_function_line == 0 &&
      (variable_context == 0 || interactive_shell == 0) &&
      currently_executing_command)
    {
#if defined (COND_COMMAND)
      if (currently_executing_command->type == cm_cond)
	return currently_executing_command->value.Cond->line;
#endif
#if defined (DPAREN_ARITHMETIC)
      if (currently_executing_command->type == cm_arith)
	return currently_executing_command->value.Arith->line;
#endif
#if defined (ARITH_FOR_COMMAND)
      if (currently_executing_command->type == cm_arith_for)
	return currently_executing_command->value.ArithFor->line;
#endif

      return line_number;
    }
  else
    return line_number;
}

 
int
execute_command (command)
     COMMAND *command;
{
  struct fd_bitmap *bitmap;
  int result;

  current_fds_to_close = (struct fd_bitmap *)NULL;
  bitmap = new_fd_bitmap (FD_BITMAP_DEFAULT_SIZE);
  begin_unwind_frame ("execute-command");
  add_unwind_protect (dispose_fd_bitmap, (char *)bitmap);

   
  result = execute_command_internal (command, 0, NO_PIPE, NO_PIPE, bitmap);

  dispose_fd_bitmap (bitmap);
  discard_unwind_frame ("execute-command");

#if defined (PROCESS_SUBSTITUTION)
   
  if (variable_context == 0 && executing_list == 0)
    unlink_fifo_list ();
#endif  

  QUIT;
  return (result);
}

 
static int
shell_control_structure (type)
     enum command_type type;
{
  switch (type)
    {
#if defined (ARITH_FOR_COMMAND)
    case cm_arith_for:
#endif
#if defined (SELECT_COMMAND)
    case cm_select:
#endif
#if defined (DPAREN_ARITHMETIC)
    case cm_arith:
#endif
#if defined (COND_COMMAND)
    case cm_cond:
#endif
    case cm_case:
    case cm_while:
    case cm_until:
    case cm_if:
    case cm_for:
    case cm_group:
    case cm_function_def:
      return (1);

    default:
      return (0);
    }
}

 
static void
cleanup_redirects (list)
     REDIRECT *list;
{
  do_redirections (list, RX_ACTIVE);
  dispose_redirects (list);
}

void
undo_partial_redirects ()
{
  if (redirection_undo_list)
    {
      cleanup_redirects (redirection_undo_list);
      redirection_undo_list = (REDIRECT *)NULL;
    }
}

#if 0
 
static void
cleanup_func_redirects (list)
     REDIRECT *list;
{
  do_redirections (list, RX_ACTIVE);
}
#endif

void
dispose_exec_redirects ()
{
  if (exec_redirection_undo_list)
    {
      dispose_redirects (exec_redirection_undo_list);
      exec_redirection_undo_list = (REDIRECT *)NULL;
    }
}

void
dispose_partial_redirects ()
{
  if (redirection_undo_list)
    {
      dispose_redirects (redirection_undo_list);
      redirection_undo_list = (REDIRECT *)NULL;
    }
}

#if defined (JOB_CONTROL)
 
static int
restore_signal_mask (set)
     sigset_t *set;
{
  return (sigprocmask (SIG_SETMASK, set, (sigset_t *)NULL));
}
#endif  

#ifdef DEBUG
 
void
open_files (void)
{
  register int i;
  int f, fd_table_size;

  fd_table_size = getdtablesize ();

  fprintf (stderr, "pid %ld open files:", (long)getpid ());
  for (i = 3; i < fd_table_size; i++)
    {
      if ((f = fcntl (i, F_GETFD, 0)) != -1)
	fprintf (stderr, " %d (%s)", i, f ? "close" : "open");
    }
  fprintf (stderr, "\n");
}
#endif

void
async_redirect_stdin ()
{
  int fd;

  fd = open ("/dev/null", O_RDONLY);
  if (fd > 0)
    {
      dup2 (fd, 0);
      close (fd);
    }
  else if (fd < 0)
    internal_error (_("cannot redirect standard input from /dev/null: %s"), strerror (errno));
}

#define DESCRIBE_PID(pid) do { if (interactive) describe_pid (pid); } while (0)

 
int
execute_command_internal (command, asynchronous, pipe_in, pipe_out,
			  fds_to_close)
     COMMAND *command;
     int asynchronous;
     int pipe_in, pipe_out;
     struct fd_bitmap *fds_to_close;
{
  int exec_result, user_subshell, invert, ignore_return, was_error_trap, fork_flags;
  REDIRECT *my_undo_list, *exec_undo_list;
  char *tcmd;
  volatile int save_line_number;
#if defined (PROCESS_SUBSTITUTION)
  volatile int ofifo, nfifo, osize, saved_fifo;
  volatile void *ofifo_list;
#endif

  if (breaking || continuing)
    return (last_command_exit_value);
  if (read_but_dont_execute)
    return (last_command_exit_value);
  if (command == 0)
    return (EXECUTION_SUCCESS);

  QUIT;
  run_pending_traps ();

#if 0
  if (running_trap == 0)
#endif
    currently_executing_command = command;

  invert = (command->flags & CMD_INVERT_RETURN) != 0;

   
  if (exit_immediately_on_error && invert)	 
    command->flags |= CMD_IGNORE_RETURN;	 

  exec_result = EXECUTION_SUCCESS;

   
  if (command->type == cm_subshell && (command->flags & CMD_NO_FORK))
    return (execute_in_subshell (command, asynchronous, pipe_in, pipe_out, fds_to_close));

#if defined (COPROCESS_SUPPORT)
  if (command->type == cm_coproc)
    return (last_command_exit_value = execute_coproc (command, pipe_in, pipe_out, fds_to_close));
#endif

  user_subshell = command->type == cm_subshell || ((command->flags & CMD_WANT_SUBSHELL) != 0);

#if defined (TIME_BEFORE_SUBSHELL)
  if ((command->flags & CMD_TIME_PIPELINE) && user_subshell && asynchronous == 0)
    {
      command->flags |= CMD_FORCE_SUBSHELL;
      exec_result = time_command (command, asynchronous, pipe_in, pipe_out, fds_to_close);
      currently_executing_command = (COMMAND *)NULL;
      return (exec_result);
    }
#endif

  if (command->type == cm_subshell ||
      (command->flags & (CMD_WANT_SUBSHELL|CMD_FORCE_SUBSHELL)) ||
      (shell_control_structure (command->type) &&
       (pipe_out != NO_PIPE || pipe_in != NO_PIPE || asynchronous)))
    {
      pid_t paren_pid;
      int s;
      char *p;

       
      save_line_number = line_number;
      if (command->type == cm_subshell)
	SET_LINE_NUMBER (command->value.Subshell->line);	 
	 
      tcmd = make_command_string (command);
      fork_flags = asynchronous ? FORK_ASYNC : 0;
      paren_pid = make_child (p = savestring (tcmd), fork_flags);

      if (user_subshell && signal_is_trapped (ERROR_TRAP) && 
	  signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
	{
	  FREE (the_printed_command_except_trap);
	  the_printed_command_except_trap = savestring (the_printed_command);
	}

      if (paren_pid == 0)
        {
#if defined (JOB_CONTROL)
	  FREE (p);		 
#endif
	   
	   
	  s = user_subshell == 0 && command->type == cm_group && pipe_in == NO_PIPE && pipe_out == NO_PIPE && asynchronous;
	   
	   
	  s += user_subshell == 0 && command->type == cm_group && (pipe_in != NO_PIPE || pipe_out != NO_PIPE) && asynchronous == 0;

	  last_command_exit_value = execute_in_subshell (command, asynchronous, pipe_in, pipe_out, fds_to_close);
	  if (s)
	    subshell_exit (last_command_exit_value);
	  else
	    sh_exit (last_command_exit_value);
	   
        }
      else
	{
	  close_pipes (pipe_in, pipe_out);

#if defined (PROCESS_SUBSTITUTION) && defined (HAVE_DEV_FD)
	  if (variable_context == 0)	 
	    unlink_fifo_list ();
#endif
	   
	  if (pipe_out != NO_PIPE)
	    return (EXECUTION_SUCCESS);

	  stop_pipeline (asynchronous, (COMMAND *)NULL);

	  line_number = save_line_number;

	  if (asynchronous == 0)
	    {
	      was_error_trap = signal_is_trapped (ERROR_TRAP) && signal_is_ignored (ERROR_TRAP) == 0;
	      invert = (command->flags & CMD_INVERT_RETURN) != 0;
	      ignore_return = (command->flags & CMD_IGNORE_RETURN) != 0;

	      exec_result = wait_for (paren_pid, 0);

	       
	      if (invert)
		exec_result = ((exec_result == EXECUTION_SUCCESS)
				? EXECUTION_FAILURE
				: EXECUTION_SUCCESS);

	      last_command_exit_value = exec_result;
	      if (user_subshell && was_error_trap && ignore_return == 0 && invert == 0 && exec_result != EXECUTION_SUCCESS)
		{
		  save_line_number = line_number;
		  line_number = line_number_for_err_trap;
		  run_error_trap ();
		  line_number = save_line_number;
		}

	      if (user_subshell && ignore_return == 0 && invert == 0 && exit_immediately_on_error && exec_result != EXECUTION_SUCCESS)
		{
		  run_pending_traps ();
		  jump_to_top_level (ERREXIT);
		}

	      return (last_command_exit_value);
	    }
	  else
	    {
	      DESCRIBE_PID (paren_pid);

	      run_pending_traps ();

	       
	      last_command_exit_value = 0;
	      return (EXECUTION_SUCCESS);
	    }
	}
    }

#if defined (COMMAND_TIMING)
  if (command->flags & CMD_TIME_PIPELINE)
    {
      if (asynchronous)
	{
	  command->flags |= CMD_FORCE_SUBSHELL;
	  exec_result = execute_command_internal (command, 1, pipe_in, pipe_out, fds_to_close);
	}
      else
	{
	  exec_result = time_command (command, asynchronous, pipe_in, pipe_out, fds_to_close);
#if 0
	  if (running_trap == 0)
#endif
	    currently_executing_command = (COMMAND *)NULL;
	}
      return (exec_result);
    }
#endif  

  if (shell_control_structure (command->type) && command->redirects)
    stdin_redir = stdin_redirects (command->redirects);

#if defined (PROCESS_SUBSTITUTION)
#  if !defined (HAVE_DEV_FD)
  reap_procsubs ();
#  endif

   
  if (variable_context != 0 || executing_list)
    {
      ofifo = num_fifos ();
      ofifo_list = copy_fifo_list ((int *)&osize);
      begin_unwind_frame ("internal_fifos");
      if (ofifo_list)
	add_unwind_protect (xfree, ofifo_list);
      saved_fifo = 1;
    }
  else
    saved_fifo = 0;
#endif

   
  was_error_trap = signal_is_trapped (ERROR_TRAP) && signal_is_ignored (ERROR_TRAP) == 0;
  ignore_return = (command->flags & CMD_IGNORE_RETURN) != 0;

  if (do_redirections (command->redirects, RX_ACTIVE|RX_UNDOABLE) != 0)
    {
      undo_partial_redirects ();
      dispose_exec_redirects ();
#if defined (PROCESS_SUBSTITUTION)
      if (saved_fifo)
	{
	  free ((void *)ofifo_list);
          discard_unwind_frame ("internal_fifos");
	}
#endif

       
      last_command_exit_value = EXECUTION_FAILURE;
      if (ignore_return == 0 && invert == 0 && pipe_in == NO_PIPE && pipe_out == NO_PIPE)
	{
	  if (was_error_trap)
	    {
	      save_line_number = line_number;
	      line_number = line_number_for_err_trap;
	      run_error_trap ();
	      line_number = save_line_number;
	    }
	  if (exit_immediately_on_error)
	    {	  
	      run_pending_traps ();
	      jump_to_top_level (ERREXIT);
	    }
	}
      return (last_command_exit_value);
    }

  my_undo_list = redirection_undo_list;
  redirection_undo_list = (REDIRECT *)NULL;

  exec_undo_list = exec_redirection_undo_list;
  exec_redirection_undo_list = (REDIRECT *)NULL;

  if (my_undo_list || exec_undo_list)
    begin_unwind_frame ("loop_redirections");

  if (my_undo_list)
    add_unwind_protect ((Function *)cleanup_redirects, my_undo_list);

  if (exec_undo_list)
    add_unwind_protect ((Function *)dispose_redirects, exec_undo_list);

  QUIT;

  switch (command->type)
    {
    case cm_simple:
      {
	save_line_number = line_number;
	 
#if defined (RECYCLES_PIDS)
	last_made_pid = NO_PID;
#endif
	was_error_trap = signal_is_trapped (ERROR_TRAP) && signal_is_ignored (ERROR_TRAP) == 0;

	if (ignore_return && command->value.Simple)
	  command->value.Simple->flags |= CMD_IGNORE_RETURN;
	if (command->flags & CMD_STDIN_REDIR)
	  command->value.Simple->flags |= CMD_STDIN_REDIR;

	SET_LINE_NUMBER (command->value.Simple->line);
	exec_result =
	  execute_simple_command (command->value.Simple, pipe_in, pipe_out,
				  asynchronous, fds_to_close);
	line_number = save_line_number;

	 
	dispose_used_env_vars ();

#if (defined (ultrix) && defined (mips)) || defined (C_ALLOCA)
	 
	(void) alloca (0);
#endif  

	 

	 
	if (already_making_children && pipe_out == NO_PIPE)
	  {
	    stop_pipeline (asynchronous, (COMMAND *)NULL);

	    if (asynchronous)
	      {
		DESCRIBE_PID (last_made_pid);
		exec_result = EXECUTION_SUCCESS;
		invert = 0;		 
	      }
	    else
#if !defined (JOB_CONTROL)
	       
	    if (last_made_pid != NO_PID && last_made_pid != last_asynchronous_pid)
#else
	    if (last_made_pid != NO_PID)
#endif
	     
	      exec_result = wait_for (last_made_pid, 0);
	  }
      }

       
        
      if (was_error_trap && ignore_return == 0 && invert == 0 &&
	    pipe_in == NO_PIPE && pipe_out == NO_PIPE &&
	    (command->value.Simple->flags & CMD_COMMAND_BUILTIN) == 0 &&
	    exec_result != EXECUTION_SUCCESS)
	{
	  last_command_exit_value = exec_result;
	  line_number = line_number_for_err_trap;
	  run_error_trap ();
	  line_number = save_line_number;
	}

      if (ignore_return == 0 && invert == 0 &&
	  ((posixly_correct && interactive == 0 && special_builtin_failed) ||
	   (exit_immediately_on_error && pipe_in == NO_PIPE && pipe_out == NO_PIPE && exec_result != EXECUTION_SUCCESS)))
	{
	  last_command_exit_value = exec_result;
	  run_pending_traps ();

	   
	  if (exit_immediately_on_error && signal_is_trapped (0) &&
		unwind_protect_tag_on_stack ("saved-redirects"))
	    run_unwind_frame ("saved-redirects");

	  jump_to_top_level (ERREXIT);
	}

      break;

    case cm_for:
      if (ignore_return)
	command->value.For->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_for_command (command->value.For);
      break;

#if defined (ARITH_FOR_COMMAND)
    case cm_arith_for:
      if (ignore_return)
	command->value.ArithFor->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_arith_for_command (command->value.ArithFor);
      break;
#endif

#if defined (SELECT_COMMAND)
    case cm_select:
      if (ignore_return)
	command->value.Select->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_select_command (command->value.Select);
      break;
#endif

    case cm_case:
      if (ignore_return)
	command->value.Case->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_case_command (command->value.Case);
      break;

    case cm_while:
      if (ignore_return)
	command->value.While->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_while_command (command->value.While);
      break;

    case cm_until:
      if (ignore_return)
	command->value.While->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_until_command (command->value.While);
      break;

    case cm_if:
      if (ignore_return)
	command->value.If->flags |= CMD_IGNORE_RETURN;
      exec_result = execute_if_command (command->value.If);
      break;

    case cm_group:

       

      if (asynchronous)
	{
	  command->flags |= CMD_FORCE_SUBSHELL;
	  exec_result =
	    execute_command_internal (command, 1, pipe_in, pipe_out,
				      fds_to_close);
	}
      else
	{
	  if (ignore_return && command->value.Group->command)
	    command->value.Group->command->flags |= CMD_IGNORE_RETURN;
	  exec_result =
	    execute_command_internal (command->value.Group->command,
				      asynchronous, pipe_in, pipe_out,
				      fds_to_close);
	}
      break;

    case cm_connection:
      exec_result = execute_connection (command, asynchronous,
					pipe_in, pipe_out, fds_to_close);
      if (asynchronous)
	invert = 0;		 

      break;

#if defined (DPAREN_ARITHMETIC)
    case cm_arith:
#endif
#if defined (COND_COMMAND)
    case cm_cond:
#endif
    case cm_function_def:
      was_error_trap = signal_is_trapped (ERROR_TRAP) && signal_is_ignored (ERROR_TRAP) == 0;
#if defined (DPAREN_ARITHMETIC)
      if (ignore_return && command->type == cm_arith)
	command->value.Arith->flags |= CMD_IGNORE_RETURN;
#endif
#if defined (COND_COMMAND)
      if (ignore_return && command->type == cm_cond)
	command->value.Cond->flags |= CMD_IGNORE_RETURN;
#endif

      line_number_for_err_trap = save_line_number = line_number;	 
#if defined (DPAREN_ARITHMETIC)
      if (command->type == cm_arith)
	exec_result = execute_arith_command (command->value.Arith);
      else
#endif
#if defined (COND_COMMAND)
      if (command->type == cm_cond)
	exec_result = execute_cond_command (command->value.Cond);
      else
#endif
      if (command->type == cm_function_def)
	exec_result = execute_intern_function (command->value.Function_def->name,
					       command->value.Function_def);
      line_number = save_line_number;

      if (was_error_trap && ignore_return == 0 && invert == 0 && exec_result != EXECUTION_SUCCESS)
	{
	  last_command_exit_value = exec_result;
	  save_line_number = line_number;
	  line_number = line_number_for_err_trap;
	  run_error_trap ();
	  line_number = save_line_number;
	}

      if (ignore_return == 0 && invert == 0 && exit_immediately_on_error && exec_result != EXECUTION_SUCCESS)
	{
	  last_command_exit_value = exec_result;
	  run_pending_traps ();
	  jump_to_top_level (ERREXIT);
	}

      break;

    default:
      command_error ("execute_command", CMDERR_BADTYPE, command->type, 0);
    }

  if (my_undo_list)
    cleanup_redirects (my_undo_list);

  if (exec_undo_list)
    dispose_redirects (exec_undo_list);

  if (my_undo_list || exec_undo_list)
    discard_unwind_frame ("loop_redirections");

#if defined (PROCESS_SUBSTITUTION)
  if (saved_fifo)
    {
      nfifo = num_fifos ();
      if (nfifo > ofifo)
	close_new_fifos ((void *)ofifo_list, osize);
      free ((void *)ofifo_list);
      discard_unwind_frame ("internal_fifos");
    }
#endif

   
  if (invert)
    exec_result = (exec_result == EXECUTION_SUCCESS)
		    ? EXECUTION_FAILURE
		    : EXECUTION_SUCCESS;

#if defined (DPAREN_ARITHMETIC) || defined (COND_COMMAND)
   
  switch (command->type)
    {
#  if defined (DPAREN_ARITHMETIC)
    case cm_arith:
#  endif
#  if defined (COND_COMMAND)
    case cm_cond:
#  endif
      set_pipestatus_from_exit (exec_result);
      break;
    default:
      break;
    }
#endif

  last_command_exit_value = exec_result;
  run_pending_traps ();
  currently_executing_command = (COMMAND *)NULL;

  return (last_command_exit_value);
}

#if defined (COMMAND_TIMING)

#if defined (HAVE_GETRUSAGE) && defined (HAVE_GETTIMEOFDAY)
extern struct timeval *difftimeval PARAMS((struct timeval *, struct timeval *, struct timeval *));
extern struct timeval *addtimeval PARAMS((struct timeval *, struct timeval *, struct timeval *));
extern int timeval_to_cpu PARAMS((struct timeval *, struct timeval *, struct timeval *));
#endif

#define POSIX_TIMEFORMAT "real %2R\nuser %2U\nsys %2S"
#define BASH_TIMEFORMAT  "\nreal\t%3lR\nuser\t%3lU\nsys\t%3lS"

static const int precs[] = { 0, 100, 10, 1 };

 
static int
mkfmt (buf, prec, lng, sec, sec_fraction)
     char *buf;
     int prec, lng;
     time_t sec;
     int sec_fraction;
{
  time_t min;
  char abuf[INT_STRLEN_BOUND(time_t) + 1];
  int ind, aind;

  ind = 0;
  abuf[sizeof(abuf) - 1] = '\0';

   
  if (lng)
    {
      min = sec / 60;
      sec %= 60;
      aind = sizeof(abuf) - 2;
      do
	abuf[aind--] = (min % 10) + '0';
      while (min /= 10);
      aind++;
      while (abuf[aind])
	buf[ind++] = abuf[aind++];
      buf[ind++] = 'm';
    }

   
  aind = sizeof (abuf) - 2;
  do
    abuf[aind--] = (sec % 10) + '0';
  while (sec /= 10);
  aind++;
  while (abuf[aind])
    buf[ind++] = abuf[aind++];

   
  if (prec != 0)
    {
      buf[ind++] = locale_decpoint ();
      for (aind = 1; aind <= prec; aind++)
	{
	  buf[ind++] = (sec_fraction / precs[aind]) + '0';
	  sec_fraction %= precs[aind];
	}
    }

  if (lng)
    buf[ind++] = 's';
  buf[ind] = '\0';

  return (ind);
}

 
static void
print_formatted_time (fp, format, rs, rsf, us, usf, ss, ssf, cpu)
     FILE *fp;
     char *format;
     time_t rs;
     int rsf;
     time_t us;
     int usf;
     time_t ss;
     int ssf, cpu;
{
  int prec, lng, len;
  char *str, *s, ts[INT_STRLEN_BOUND (time_t) + sizeof ("mSS.FFFF")];
  time_t sum;
  int sum_frac;
  int sindex, ssize;

  len = strlen (format);
  ssize = (len + 64) - (len % 64);
  str = (char *)xmalloc (ssize);
  sindex = 0;

  for (s = format; *s; s++)
    {
      if (*s != '%' || s[1] == '\0')
	{
	  RESIZE_MALLOCED_BUFFER (str, sindex, 1, ssize, 64);
	  str[sindex++] = *s;
	}
      else if (s[1] == '%')
	{
	  s++;
	  RESIZE_MALLOCED_BUFFER (str, sindex, 1, ssize, 64);
	  str[sindex++] = *s;
	}
      else if (s[1] == 'P')
	{
	  s++;
#if 0
	   
	  if (cpu > 10000)
	    cpu = 10000;
#endif
	  sum = cpu / 100;
	  sum_frac = (cpu % 100) * 10;
	  len = mkfmt (ts, 2, 0, sum, sum_frac);
	  RESIZE_MALLOCED_BUFFER (str, sindex, len, ssize, 64);
	  strcpy (str + sindex, ts);
	  sindex += len;
	}
      else
	{
	  prec = 3;	 
	  lng = 0;	 
	  s++;
	  if (DIGIT (*s))		 
	    {
	      prec = *s++ - '0';
	      if (prec > 3) prec = 3;
	    }
	  if (*s == 'l')		 
	    {
	      lng = 1;
	      s++;
	    }
	  if (*s == 'R' || *s == 'E')
	    len = mkfmt (ts, prec, lng, rs, rsf);
	  else if (*s == 'U')
	    len = mkfmt (ts, prec, lng, us, usf);
	  else if (*s == 'S')
	    len = mkfmt (ts, prec, lng, ss, ssf);
	  else
	    {
	      internal_error (_("TIMEFORMAT: `%c': invalid format character"), *s);
	      free (str);
	      return;
	    }
	  RESIZE_MALLOCED_BUFFER (str, sindex, len, ssize, 64);
	  strcpy (str + sindex, ts);
	  sindex += len;
	}
    }

  str[sindex] = '\0';
  fprintf (fp, "%s\n", str);
  fflush (fp);

  free (str);
}

static int
time_command (command, asynchronous, pipe_in, pipe_out, fds_to_close)
     COMMAND *command;
     int asynchronous, pipe_in, pipe_out;
     struct fd_bitmap *fds_to_close;
{
  int rv, posix_time, old_flags, nullcmd, code;
  time_t rs, us, ss;
  int rsf, usf, ssf;
  int cpu;
  char *time_format;
  volatile procenv_t save_top_level;
  volatile int old_subshell;

#if defined (HAVE_GETRUSAGE) && defined (HAVE_GETTIMEOFDAY)
  struct timeval real, user, sys;
  struct timeval before, after;
#  if defined (HAVE_STRUCT_TIMEZONE)
  struct timezone dtz;				 
#  endif
  struct rusage selfb, selfa, kidsb, kidsa;	 
#else
#  if defined (HAVE_TIMES)
  clock_t tbefore, tafter, real, user, sys;
  struct tms before, after;
#  endif
#endif

#if defined (HAVE_GETRUSAGE) && defined (HAVE_GETTIMEOFDAY)
#  if defined (HAVE_STRUCT_TIMEZONE)
  gettimeofday (&before, &dtz);
#  else
  gettimeofday (&before, (void *)NULL);
#  endif  
  getrusage (RUSAGE_SELF, &selfb);
  getrusage (RUSAGE_CHILDREN, &kidsb);
#else
#  if defined (HAVE_TIMES)
  tbefore = times (&before);
#  endif
#endif

  old_subshell = subshell_environment;
  posix_time = command && (command->flags & CMD_TIME_POSIX);

  nullcmd = (command == 0) || (command->type == cm_simple && command->value.Simple->words == 0 && command->value.Simple->redirects == 0);
  if (posixly_correct && nullcmd)
    {
#if defined (HAVE_GETRUSAGE)
      selfb.ru_utime.tv_sec = kidsb.ru_utime.tv_sec = selfb.ru_stime.tv_sec = kidsb.ru_stime.tv_sec = 0;
      selfb.ru_utime.tv_usec = kidsb.ru_utime.tv_usec = selfb.ru_stime.tv_usec = kidsb.ru_stime.tv_usec = 0;
      before = shellstart;
#else
      before.tms_utime = before.tms_stime = before.tms_cutime = before.tms_cstime = 0;
      tbefore = shell_start_time;
#endif
    }

  old_flags = command->flags;
  COPY_PROCENV (top_level, save_top_level);
  command->flags &= ~(CMD_TIME_PIPELINE|CMD_TIME_POSIX);
  code = setjmp_nosigs (top_level);
  if (code == NOT_JUMPED)
    rv = execute_command_internal (command, asynchronous, pipe_in, pipe_out, fds_to_close);
  COPY_PROCENV (save_top_level, top_level);

  command->flags = old_flags;

   
  if (code != NOT_JUMPED && subshell_environment && subshell_environment != old_subshell)
    sh_longjmp (top_level, code);

  rs = us = ss = 0;
  rsf = usf = ssf = cpu = 0;

#if defined (HAVE_GETRUSAGE) && defined (HAVE_GETTIMEOFDAY)
#  if defined (HAVE_STRUCT_TIMEZONE)
  gettimeofday (&after, &dtz);
#  else
  gettimeofday (&after, (void *)NULL);
#  endif  
  getrusage (RUSAGE_SELF, &selfa);
  getrusage (RUSAGE_CHILDREN, &kidsa);

  difftimeval (&real, &before, &after);
  timeval_to_secs (&real, &rs, &rsf);

  addtimeval (&user, difftimeval(&after, &selfb.ru_utime, &selfa.ru_utime),
		     difftimeval(&before, &kidsb.ru_utime, &kidsa.ru_utime));
  timeval_to_secs (&user, &us, &usf);

  addtimeval (&sys, difftimeval(&after, &selfb.ru_stime, &selfa.ru_stime),
		    difftimeval(&before, &kidsb.ru_stime, &kidsa.ru_stime));
  timeval_to_secs (&sys, &ss, &ssf);

  cpu = timeval_to_cpu (&real, &user, &sys);
#else
#  if defined (HAVE_TIMES)
  tafter = times (&after);

  real = tafter - tbefore;
  clock_t_to_secs (real, &rs, &rsf);

  user = (after.tms_utime - before.tms_utime) + (after.tms_cutime - before.tms_cutime);
  clock_t_to_secs (user, &us, &usf);

  sys = (after.tms_stime - before.tms_stime) + (after.tms_cstime - before.tms_cstime);
  clock_t_to_secs (sys, &ss, &ssf);

  cpu = (real == 0) ? 0 : ((user + sys) * 10000) / real;

#  else
  rs = us = ss = 0;
  rsf = usf = ssf = cpu = 0;
#  endif
#endif

  if (posix_time)
    time_format = POSIX_TIMEFORMAT;
  else if ((time_format = get_string_value ("TIMEFORMAT")) == 0)
    {
      if (posixly_correct && nullcmd)
	time_format = "user\t%2lU\nsys\t%2lS";
      else
	time_format = BASH_TIMEFORMAT;
    }

  if (time_format && *time_format)
    print_formatted_time (stderr, time_format, rs, rsf, us, usf, ss, ssf, cpu);

  if (code)
    sh_longjmp (top_level, code);

  return rv;
}
#endif  

 
static int
execute_in_subshell (command, asynchronous, pipe_in, pipe_out, fds_to_close)
     COMMAND *command;
     int asynchronous;
     int pipe_in, pipe_out;
     struct fd_bitmap *fds_to_close;
{
  volatile int user_subshell, user_coproc, invert;
  int return_code, function_value, should_redir_stdin, ois, result;
  volatile COMMAND *tcom;

  USE_VAR(user_subshell);
  USE_VAR(user_coproc);
  USE_VAR(invert);
  USE_VAR(tcom);
  USE_VAR(asynchronous);

  subshell_level++;
  should_redir_stdin = (asynchronous && (command->flags & CMD_STDIN_REDIR) &&
			  pipe_in == NO_PIPE &&
			  stdin_redirects (command->redirects) == 0);

  invert = (command->flags & CMD_INVERT_RETURN) != 0;
  user_subshell = command->type == cm_subshell || ((command->flags & CMD_WANT_SUBSHELL) != 0);
  user_coproc = command->type == cm_coproc;

  command->flags &= ~(CMD_FORCE_SUBSHELL | CMD_WANT_SUBSHELL | CMD_INVERT_RETURN);

   

   
  if (asynchronous)
    {
#if defined (JOB_CONTROL)
       
      original_pgrp = -1;
#endif  
      ois = interactive_shell;
      interactive_shell = 0;
       
      if (ois != interactive_shell)
	expand_aliases = expaliases_flag = 0;
    }

   
  login_shell = interactive = 0;

   
  if (shell_compatibility_level > 44)
    loop_level = 0;

  if (user_subshell)
    {
      subshell_environment = SUBSHELL_PAREN;	 
      if (asynchronous)
	subshell_environment |= SUBSHELL_ASYNC;
    }
  else
    {
      subshell_environment = 0;			 
      if (asynchronous)
	subshell_environment |= SUBSHELL_ASYNC;
      if (pipe_in != NO_PIPE || pipe_out != NO_PIPE)
	subshell_environment |= SUBSHELL_PIPE;
      if (user_coproc)
	subshell_environment |= SUBSHELL_COPROC;
    }

  QUIT;
  CHECK_TERMSIG;

  reset_terminating_signals ();		 
   
   
  clear_pending_traps ();
  reset_signal_handlers ();
  subshell_environment |= SUBSHELL_RESETTRAP;
   
  subshell_environment &= ~SUBSHELL_IGNTRAP;

   
   
  if (running_trap > 0)
    {
      run_trap_cleanup (running_trap - 1);
      running_trap = 0;		 
    }

   
  if (asynchronous)
    {
      setup_async_signals ();
      asynchronous = 0;
    }
  else
    set_sigint_handler ();

#if defined (JOB_CONTROL)
  set_sigchld_handler ();
#endif  

   
  without_job_control ();

  if (fds_to_close)
    close_fd_bitmap (fds_to_close);

  do_piping (pipe_in, pipe_out);

#if defined (COPROCESS_SUPPORT)
  coproc_closeall ();
#endif

#if defined (PROCESS_SUBSTITUTION)
  clear_fifo_list ();		 
#endif

   
  if (user_subshell)
    {
      stdin_redir = stdin_redirects (command->redirects) || pipe_in != NO_PIPE;
#if 0
      restore_default_signal (EXIT_TRAP);	 
#endif
    }
  else if (shell_control_structure (command->type) && pipe_in != NO_PIPE)
    stdin_redir = 1;

   
  if (should_redir_stdin && stdin_redir == 0)
    async_redirect_stdin ();

#if defined (BUFFERED_INPUT)
   
  default_buffered_input = -1;
#endif

   
  if (user_subshell && command->type == cm_subshell &&
      (command->flags & (CMD_TIME_PIPELINE|CMD_INVERT_RETURN)) == 0)
    optimize_subshell_command (command->value.Subshell->command);

   
  if (command->redirects)
    {
      if (do_redirections (command->redirects, RX_ACTIVE) != 0)
	exit (invert ? EXECUTION_SUCCESS : EXECUTION_FAILURE);

      dispose_redirects (command->redirects);
      command->redirects = (REDIRECT *)NULL;
    }

  if (command->type == cm_subshell)
    tcom = command->value.Subshell->command;
  else if (user_coproc)
    tcom = command->value.Coproc->command;
  else
    tcom = command;

  if (command->flags & CMD_TIME_PIPELINE)
    tcom->flags |= CMD_TIME_PIPELINE;
  if (command->flags & CMD_TIME_POSIX)
    tcom->flags |= CMD_TIME_POSIX;
  
   
  if ((command->flags & CMD_IGNORE_RETURN) && tcom != command)
    tcom->flags |= CMD_IGNORE_RETURN;

   
  if ((user_subshell || user_coproc) && (tcom->type == cm_simple || tcom->type == cm_subshell) &&
      ((tcom->flags & CMD_TIME_PIPELINE) == 0) &&
      ((tcom->flags & CMD_INVERT_RETURN) == 0))
    {
      tcom->flags |= CMD_NO_FORK;
      if (tcom->type == cm_simple)
	tcom->value.Simple->flags |= CMD_NO_FORK;
    }

  invert = (tcom->flags & CMD_INVERT_RETURN) != 0;
  tcom->flags &= ~CMD_INVERT_RETURN;

  result = setjmp_nosigs (top_level);

   
  function_value = 0;
  if (return_catch_flag)
    function_value = setjmp_nosigs (return_catch);

   
  if (result == EXITPROG || result == EXITBLTIN)
    invert = 0, return_code = last_command_exit_value;
  else if (result)
    return_code = (last_command_exit_value == EXECUTION_SUCCESS) ? EXECUTION_FAILURE : last_command_exit_value;
  else if (function_value)
    return_code = return_catch_value;
  else
    return_code = execute_command_internal ((COMMAND *)tcom, asynchronous, NO_PIPE, NO_PIPE, fds_to_close);

   
  if (invert)
    return_code = (return_code == EXECUTION_SUCCESS) ? EXECUTION_FAILURE
						     : EXECUTION_SUCCESS;


   
  if (user_subshell && signal_is_trapped (0))
    {
      last_command_exit_value = return_code;
      return_code = run_exit_trap ();
    }

#if 0
  subshell_level--;		 
#endif
  return (return_code);
   
}

#if defined (COPROCESS_SUPPORT)
#define COPROC_MAX	16

typedef struct cpelement
  {
    struct cpelement *next;
    struct coproc *coproc;
  }
cpelement_t;
    
typedef struct cplist
  {
    struct cpelement *head;
    struct cpelement *tail;
    int ncoproc;
    int lock;
  }
cplist_t;

static struct cpelement *cpe_alloc PARAMS((struct coproc *));
static void cpe_dispose PARAMS((struct cpelement *));
static struct cpelement *cpl_add PARAMS((struct coproc *));
static struct cpelement *cpl_delete PARAMS((pid_t));
static void cpl_reap PARAMS((void));
static void cpl_flush PARAMS((void));
static void cpl_closeall PARAMS((void));
static struct cpelement *cpl_search PARAMS((pid_t));
static struct cpelement *cpl_searchbyname PARAMS((const char *));
static void cpl_prune PARAMS((void));

static void coproc_free PARAMS((struct coproc *));

 
Coproc sh_coproc = { 0, NO_PID, -1, -1, 0, 0, 0, 0, 0 };

cplist_t coproc_list = {0, 0, 0};

 

static struct cpelement *
cpe_alloc (cp)
     Coproc *cp;
{
  struct cpelement *cpe;

  cpe = (struct cpelement *)xmalloc (sizeof (struct cpelement));
  cpe->coproc = cp;
  cpe->next = (struct cpelement *)0;
  return cpe;
}

static void
cpe_dispose (cpe)
      struct cpelement *cpe;
{
  free (cpe);
}

static struct cpelement *
cpl_add (cp)
     Coproc *cp;
{
  struct cpelement *cpe;

  cpe = cpe_alloc (cp);

  if (coproc_list.head == 0)
    {
      coproc_list.head = coproc_list.tail = cpe;
      coproc_list.ncoproc = 0;			 
    }
  else
    {
      coproc_list.tail->next = cpe;
      coproc_list.tail = cpe;
    }
  coproc_list.ncoproc++;

  return cpe;
}

static struct cpelement *
cpl_delete (pid)
     pid_t pid;
{
  struct cpelement *prev, *p;

  for (prev = p = coproc_list.head; p; prev = p, p = p->next)
    if (p->coproc->c_pid == pid)
      {
        prev->next = p->next;	 
        break;
      }

  if (p == 0)
    return 0;		 

  INTERNAL_DEBUG (("cpl_delete: deleting %d", pid));

   
  if (p == coproc_list.head)
    coproc_list.head = coproc_list.head->next;
  else if (p == coproc_list.tail)
    coproc_list.tail = prev;

  coproc_list.ncoproc--;
  if (coproc_list.ncoproc == 0)
    coproc_list.head = coproc_list.tail = 0;
  else if (coproc_list.ncoproc == 1)
    coproc_list.tail = coproc_list.head;		 

  return (p);
}

static void
cpl_reap ()
{
  struct cpelement *p, *next, *nh, *nt;

   
  nh = nt = next = (struct cpelement *)0;
  for (p = coproc_list.head; p; p = next)
    {
      next = p->next;
      if (p->coproc->c_flags & COPROC_DEAD)
	{
	  coproc_list.ncoproc--;	 
	  INTERNAL_DEBUG (("cpl_reap: deleting %d", p->coproc->c_pid));
	  coproc_dispose (p->coproc);
	  cpe_dispose (p);
	}
      else if (nh == 0)
	nh = nt = p;
      else
	{
	  nt->next = p;
	  nt = nt->next;
	}
    }

  if (coproc_list.ncoproc == 0)
    coproc_list.head = coproc_list.tail = 0;
  else
    {
      if (nt)
        nt->next = 0;
      coproc_list.head = nh;
      coproc_list.tail = nt;
      if (coproc_list.ncoproc == 1)
	coproc_list.tail = coproc_list.head;		   
    }
}

 
static void
cpl_flush ()
{
  struct cpelement *cpe, *p;

  for (cpe = coproc_list.head; cpe; )
    {
      p = cpe;
      cpe = cpe->next;

      coproc_dispose (p->coproc);
      cpe_dispose (p);
    }

  coproc_list.head = coproc_list.tail = 0;
  coproc_list.ncoproc = 0;
}

static void
cpl_closeall ()
{
  struct cpelement *cpe;

  for (cpe = coproc_list.head; cpe; cpe = cpe->next)
    coproc_close (cpe->coproc);
}

static void
cpl_fdchk (fd)
     int fd;
{
  struct cpelement *cpe;

  for (cpe = coproc_list.head; cpe; cpe = cpe->next)
    coproc_checkfd (cpe->coproc, fd);
}

 
static struct cpelement *
cpl_search (pid)
     pid_t pid;
{
  struct cpelement *cpe;

  for (cpe = coproc_list.head ; cpe; cpe = cpe->next)
    if (cpe->coproc->c_pid == pid)
      return cpe;
  return (struct cpelement *)NULL;
}

 
static struct cpelement *
cpl_searchbyname (name)
     const char *name;
{
  struct cpelement *cp;

  for (cp = coproc_list.head ; cp; cp = cp->next)
    if (STREQ (cp->coproc->c_name, name))
      return cp;
  return (struct cpelement *)NULL;
}

static pid_t
cpl_firstactive ()
{
  struct cpelement *cpe;

  for (cpe = coproc_list.head ; cpe; cpe = cpe->next)
    if ((cpe->coproc->c_flags & COPROC_DEAD) == 0)
      return cpe->coproc->c_pid;
  return (pid_t)NO_PID;
}

#if 0
static void
cpl_prune ()
{
  struct cpelement *cp;

  while (coproc_list.head && coproc_list.ncoproc > COPROC_MAX)
    {
      cp = coproc_list.head;
      coproc_list.head = coproc_list.head->next;
      coproc_dispose (cp->coproc);
      cpe_dispose (cp);
      coproc_list.ncoproc--;
    }
}
#endif

 

struct coproc *
getcoprocbypid (pid)
     pid_t pid;
{
#if MULTIPLE_COPROCS
  struct cpelement *p;

  p = cpl_search (pid);
  return (p ? p->coproc : 0);
#else
  return (pid == sh_coproc.c_pid ? &sh_coproc : 0);
#endif
}

struct coproc *
getcoprocbyname (name)
     const char *name;
{
#if MULTIPLE_COPROCS
  struct cpelement *p;

  p = cpl_searchbyname (name);
  return (p ? p->coproc : 0);
#else
  return ((sh_coproc.c_name && STREQ (sh_coproc.c_name, name)) ? &sh_coproc : 0);
#endif
}

void
coproc_init (cp)
     struct coproc *cp;
{
  cp->c_name = 0;
  cp->c_pid = NO_PID;
  cp->c_rfd = cp->c_wfd = -1;
  cp->c_rsave = cp->c_wsave = -1;
  cp->c_flags = cp->c_status = cp->c_lock = 0;
}

struct coproc *
coproc_alloc (name, pid)
     char *name;
     pid_t pid;
{
  struct coproc *cp;

#if MULTIPLE_COPROCS
  cp = (struct coproc *)xmalloc (sizeof (struct coproc));
#else
  cp = &sh_coproc;
#endif
  coproc_init (cp);
  cp->c_lock = 2;

  cp->c_pid = pid;
  cp->c_name = savestring (name);
#if MULTIPLE_COPROCS
  cpl_add (cp);
#endif
  cp->c_lock = 0;
  return (cp);
}

static void
coproc_free (cp)
     struct coproc *cp;
{
  free (cp);
}

void
coproc_dispose (cp)
     struct coproc *cp;
{
  sigset_t set, oset;

  if (cp == 0)
    return;

  BLOCK_SIGNAL (SIGCHLD, set, oset);
  cp->c_lock = 3;
  coproc_unsetvars (cp);
  FREE (cp->c_name);
  coproc_close (cp);
#if MULTIPLE_COPROCS
  coproc_free (cp);
#else
  coproc_init (cp);
  cp->c_lock = 0;
#endif
  UNBLOCK_SIGNAL (oset);
}

 
void
coproc_flush ()
{
#if MULTIPLE_COPROCS
  cpl_flush ();
#else
  coproc_dispose (&sh_coproc);
#endif
}

void
coproc_close (cp)
     struct coproc *cp;
{
  if (cp->c_rfd >= 0)
    {
      close (cp->c_rfd);
      cp->c_rfd = -1;
    }
  if (cp->c_wfd >= 0)
    {
      close (cp->c_wfd);
      cp->c_wfd = -1;
    }
  cp->c_rsave = cp->c_wsave = -1;
}

void
coproc_closeall ()
{
#if MULTIPLE_COPROCS
  cpl_closeall ();
#else
  coproc_close (&sh_coproc);	 
#endif
}

void
coproc_reap ()
{
#if MULTIPLE_COPROCS
  cpl_reap ();
#else
  struct coproc *cp;

  cp = &sh_coproc;		 
  if (cp && (cp->c_flags & COPROC_DEAD))
    coproc_dispose (cp);
#endif
}

void
coproc_rclose (cp, fd)
     struct coproc *cp;
     int fd;
{
  if (cp->c_rfd >= 0 && cp->c_rfd == fd)
    {
      close (cp->c_rfd);
      cp->c_rfd = -1;
    }
}

void
coproc_wclose (cp, fd)
     struct coproc *cp;
     int fd;
{
  if (cp->c_wfd >= 0 && cp->c_wfd == fd)
    {
      close (cp->c_wfd);
      cp->c_wfd = -1;
    }
}

void
coproc_checkfd (cp, fd)
     struct coproc *cp;
     int fd;
{
  int update;

  update = 0;
  if (cp->c_rfd >= 0 && cp->c_rfd == fd)
    update = cp->c_rfd = -1;
  if (cp->c_wfd >= 0 && cp->c_wfd == fd)
    update = cp->c_wfd = -1;
  if (update)
    coproc_setvars (cp);
}

void
coproc_fdchk (fd)
     int fd;
{
#if MULTIPLE_COPROCS
  cpl_fdchk (fd);
#else
  coproc_checkfd (&sh_coproc, fd);
#endif
}

void
coproc_fdclose (cp, fd)
     struct coproc *cp;
     int fd;
{
  coproc_rclose (cp, fd);
  coproc_wclose (cp, fd);
  coproc_setvars (cp);
}

void
coproc_fdsave (cp)
     struct coproc *cp;
{
  cp->c_rsave = cp->c_rfd;
  cp->c_wsave = cp->c_wfd;
}

void
coproc_fdrestore (cp)
     struct coproc *cp;
{
  cp->c_rfd = cp->c_rsave;
  cp->c_wfd = cp->c_wsave;
}

static void
coproc_setstatus (cp, status)
     struct coproc *cp;
     int status;
{
  cp->c_lock = 4;
  cp->c_status = status;
  cp->c_flags |= COPROC_DEAD;
  cp->c_flags &= ~COPROC_RUNNING;
   
  cp->c_lock = 0;
}

void
coproc_pidchk (pid, status)
     pid_t pid;
     int status;
{
  struct coproc *cp;

#if MULTIPLE_COPROCS
  struct cpelement *cpe;

   
  cpe = cpl_search (pid);
  cp = cpe ? cpe->coproc : 0;
#else
  cp = getcoprocbypid (pid);
#endif
  if (cp)
    coproc_setstatus (cp, status);
}

pid_t
coproc_active ()
{
#if MULTIPLE_COPROCS
  return (cpl_firstactive ());
#else
  return ((sh_coproc.c_flags & COPROC_DEAD) ? NO_PID : sh_coproc.c_pid);
#endif
}
void
coproc_setvars (cp)
     struct coproc *cp;
{
  SHELL_VAR *v;
  char *namevar, *t;
  size_t l;
  WORD_DESC w;
#if defined (ARRAY_VARS)
  arrayind_t ind;
#endif

  if (cp->c_name == 0)
    return;

   
  w.word = cp->c_name;
  w.flags = 0;
  if (check_identifier (&w, 1) == 0)
    return;

  l = strlen (cp->c_name);
  namevar = xmalloc (l + 16);

#if defined (ARRAY_VARS)
  v = find_variable (cp->c_name);

   
  if (v == 0)
    {
      v = find_variable_nameref_for_create (cp->c_name, 1);
      if (v == INVALID_NAMEREF_VALUE)
	{
	  free (namevar);
	  return;
	}
      if (v && nameref_p (v))
	{
	  free (cp->c_name);
	  cp->c_name = savestring (nameref_cell (v));
	  v = make_new_array_variable (cp->c_name);	  
	}
    }

  if (v && (readonly_p (v) || noassign_p (v)))
    {
      if (readonly_p (v))
	err_readonly (cp->c_name);
      free (namevar);
      return;
    }
  if (v == 0)
    v = make_new_array_variable (cp->c_name);
  if (array_p (v) == 0)
    v = convert_var_to_array (v);

  t = itos (cp->c_rfd);
  ind = 0;
  v = bind_array_variable (cp->c_name, ind, t, 0);
  free (t);

  t = itos (cp->c_wfd);
  ind = 1;
  v = bind_array_variable (cp->c_name, ind, t, 0);
  free (t);
#else
  sprintf (namevar, "%s_READ", cp->c_name);
  t = itos (cp->c_rfd);
  bind_variable (namevar, t, 0);
  free (t);
  sprintf (namevar, "%s_WRITE", cp->c_name);
  t = itos (cp->c_wfd);
  bind_variable (namevar, t, 0);
  free (t);
#endif

  sprintf (namevar, "%s_PID", cp->c_name);
  t = itos (cp->c_pid);
  v = bind_variable (namevar, t, 0);
  free (t);

  free (namevar);
}

void
coproc_unsetvars (cp)
     struct coproc *cp;
{
  int l;
  char *namevar;

  if (cp->c_name == 0)
    return;

  l = strlen (cp->c_name);
  namevar = xmalloc (l + 16);

  sprintf (namevar, "%s_PID", cp->c_name);
  unbind_variable_noref (namevar);  

#if defined (ARRAY_VARS)
  check_unbind_variable (cp->c_name);
#else
  sprintf (namevar, "%s_READ", cp->c_name);
  unbind_variable (namevar);
  sprintf (namevar, "%s_WRITE", cp->c_name);
  unbind_variable (namevar);
#endif  

  free (namevar);
}

static int
execute_coproc (command, pipe_in, pipe_out, fds_to_close)
     COMMAND *command;
     int pipe_in, pipe_out;
     struct fd_bitmap *fds_to_close;
{
  int rpipe[2], wpipe[2], estat, invert;
  pid_t coproc_pid;
  Coproc *cp;
  char *tcmd, *p, *name;
  sigset_t set, oset;

   
#if !MULTIPLE_COPROCS
  if (sh_coproc.c_pid != NO_PID && (sh_coproc.c_rfd >= 0 || sh_coproc.c_wfd >= 0))
    internal_warning (_("execute_coproc: coproc [%d:%s] still exists"), sh_coproc.c_pid, sh_coproc.c_name);
  coproc_init (&sh_coproc);
#endif

  invert = (command->flags & CMD_INVERT_RETURN) != 0;

   
  name = expand_string_unsplit_to_string (command->value.Coproc->name, 0);
   
  if (legal_identifier (name) == 0)
    {
      internal_error (_("`%s': not a valid identifier"), name);
      free (name);
      return (invert ? EXECUTION_SUCCESS : EXECUTION_FAILURE);
    }
  else
    {
      free (command->value.Coproc->name);
      command->value.Coproc->name = name;
    }

  command_string_index = 0;
  tcmd = make_command_string (command);

  sh_openpipe ((int *)&rpipe);	 
  sh_openpipe ((int *)&wpipe);  

  BLOCK_SIGNAL (SIGCHLD, set, oset);

  coproc_pid = make_child (p = savestring (tcmd), FORK_ASYNC);

  if (coproc_pid == 0)
    {
      close (rpipe[0]);
      close (wpipe[1]);

#if defined (JOB_CONTROL)
      FREE (p);
#endif

      UNBLOCK_SIGNAL (oset);
      estat = execute_in_subshell (command, 1, wpipe[0], rpipe[1], fds_to_close);

      fflush (stdout);
      fflush (stderr);

      exit (estat);
    }

  close (rpipe[1]);
  close (wpipe[0]);

  cp = coproc_alloc (command->value.Coproc->name, coproc_pid);
  cp->c_rfd = rpipe[0];
  cp->c_wfd = wpipe[1];

  cp->c_flags |= COPROC_RUNNING;

  SET_CLOSE_ON_EXEC (cp->c_rfd);
  SET_CLOSE_ON_EXEC (cp->c_wfd);

  coproc_setvars (cp);

  UNBLOCK_SIGNAL (oset);

#if 0
  itrace ("execute_coproc (%s): [%d] %s", command->value.Coproc->name, coproc_pid, the_printed_command);
#endif

  close_pipes (pipe_in, pipe_out);
#if defined (PROCESS_SUBSTITUTION) && defined (HAVE_DEV_FD)
  unlink_fifo_list ();
#endif
  stop_pipeline (1, (COMMAND *)NULL);
  DESCRIBE_PID (coproc_pid);
  run_pending_traps ();

  return (invert ? EXECUTION_FAILURE : EXECUTION_SUCCESS);
}
#endif

 
static void
restore_stdin (s)
     int s;
{
  if (s == -1)
    close (0);
  else
    {
      dup2 (s, 0);
      close (s);
    }
}

 
static void
lastpipe_cleanup (s)
     int s;
{
  set_jobs_list_frozen (s);
}

static int
execute_pipeline (command, asynchronous, pipe_in, pipe_out, fds_to_close)
     COMMAND *command;
     int asynchronous, pipe_in, pipe_out;
     struct fd_bitmap *fds_to_close;
{
  int prev, fildes[2], new_bitmap_size, dummyfd, ignore_return, exec_result;
  int lstdin, lastpipe_flag, lastpipe_jid, old_frozen, stdin_valid;
  COMMAND *cmd;
  struct fd_bitmap *fd_bitmap;
  pid_t lastpid;

#if defined (JOB_CONTROL)
  sigset_t set, oset;
  BLOCK_CHILD (set, oset);
#endif  

  ignore_return = (command->flags & CMD_IGNORE_RETURN) != 0;

  stdin_valid = sh_validfd (0);

  prev = pipe_in;
  cmd = command;

  while (cmd && cmd->type == cm_connection &&
	 cmd->value.Connection && cmd->value.Connection->connector == '|')
    {
       
      if (pipe (fildes) < 0)
	{
	  sys_error (_("pipe error"));
#if defined (JOB_CONTROL)
	  terminate_current_pipeline ();
	  kill_current_pipeline ();
	  UNBLOCK_CHILD (oset);
#endif  
	  last_command_exit_value = EXECUTION_FAILURE;
	   
	  throw_to_top_level ();
	  return (EXECUTION_FAILURE);	 
	}

       

       
      new_bitmap_size = (fildes[0] < fds_to_close->size)
				? fds_to_close->size
				: fildes[0] + 8;

      fd_bitmap = new_fd_bitmap (new_bitmap_size);

       
      xbcopy ((char *)fds_to_close->bitmap, (char *)fd_bitmap->bitmap, fds_to_close->size);

       
      fd_bitmap->bitmap[fildes[0]] = 1;

       
      begin_unwind_frame ("pipe-file-descriptors");
      add_unwind_protect (dispose_fd_bitmap, fd_bitmap);
      add_unwind_protect (close_fd_bitmap, fd_bitmap);
      if (prev >= 0)
	add_unwind_protect (close, prev);
      dummyfd = fildes[1];
      add_unwind_protect (close, dummyfd);

#if defined (JOB_CONTROL)
      add_unwind_protect (restore_signal_mask, &oset);
#endif  

      if (ignore_return && cmd->value.Connection->first)
	cmd->value.Connection->first->flags |= CMD_IGNORE_RETURN;
      execute_command_internal (cmd->value.Connection->first, asynchronous,
				prev, fildes[1], fd_bitmap);

      if (prev >= 0)
	close (prev);

      prev = fildes[0];
      close (fildes[1]);

      dispose_fd_bitmap (fd_bitmap);
      discard_unwind_frame ("pipe-file-descriptors");

      cmd = cmd->value.Connection->second;
    }

  lastpid = last_made_pid;

   
  if (ignore_return && cmd)
    cmd->flags |= CMD_IGNORE_RETURN;

  lastpipe_flag = 0;

  begin_unwind_frame ("lastpipe-exec");
  lstdin = -2;		 
   
   
  if (lastpipe_opt && job_control == 0 && asynchronous == 0 && pipe_out == NO_PIPE && prev >= 0)
    {
       
      lstdin = (prev > 0 && stdin_valid) ? move_to_high_fd (0, 1, -1) : -1;
      if (lstdin > 0 || lstdin == -1)
	{
	  do_piping (prev, pipe_out);
	  prev = NO_PIPE;
	  add_unwind_protect (restore_stdin, lstdin);
	  lastpipe_flag = 1;
	  old_frozen = freeze_jobs_list ();
	  lastpipe_jid = stop_pipeline (0, (COMMAND *)NULL);	 
	  add_unwind_protect (lastpipe_cleanup, old_frozen);
#if defined (JOB_CONTROL)
	  UNBLOCK_CHILD (oset);		 
#endif
	}
      if (cmd)
	cmd->flags |= CMD_LASTPIPE;
    }	  
  if (prev >= 0)
    add_unwind_protect (close, prev);

  exec_result = execute_command_internal (cmd, asynchronous, prev, pipe_out, fds_to_close);

  if (prev >= 0)
    close (prev);

  if (lstdin > 0 || lstdin == -1)
    restore_stdin (lstdin);

#if defined (JOB_CONTROL)
  UNBLOCK_CHILD (oset);
#endif

  QUIT;

  if (lastpipe_flag)
    {
#if defined (JOB_CONTROL)
      if (INVALID_JOB (lastpipe_jid) == 0)
        {
          append_process (savestring (the_printed_command_except_trap), dollar_dollar_pid, exec_result, lastpipe_jid);
          lstdin = wait_for (lastpid, 0);
        }
      else
	{
	  lstdin = wait_for_single_pid (lastpid, 0);		 
	  if (lstdin > 256)		 
	    lstdin = 127;
	}
#else
      lstdin = wait_for (lastpid, 0);
#endif

#if defined (JOB_CONTROL)
       
      if (INVALID_JOB (lastpipe_jid) == 0)
	exec_result = job_exit_status (lastpipe_jid);
      else if (pipefail_opt)
	exec_result = exec_result | lstdin;	 
       
#endif

      set_jobs_list_frozen (old_frozen);
    }

  discard_unwind_frame ("lastpipe-exec");

  return (exec_result);
}

static int
execute_connection (command, asynchronous, pipe_in, pipe_out, fds_to_close)
     COMMAND *command;
     int asynchronous, pipe_in, pipe_out;
     struct fd_bitmap *fds_to_close;
{
  COMMAND *tc, *second;
  int ignore_return, exec_result, was_error_trap, invert;
  volatile int save_line_number;

  ignore_return = (command->flags & CMD_IGNORE_RETURN) != 0;

  switch (command->value.Connection->connector)
    {
     
    case '&':
      tc = command->value.Connection->first;
      if (tc == 0)
	return (EXECUTION_SUCCESS);

      if (ignore_return)
	tc->flags |= CMD_IGNORE_RETURN;
      tc->flags |= CMD_AMPERSAND;

       
#if defined (JOB_CONTROL)
      if ((subshell_environment || !job_control) && !stdin_redir)
#else
      if (!stdin_redir)
#endif  
	tc->flags |= CMD_STDIN_REDIR;

      exec_result = execute_command_internal (tc, 1, pipe_in, pipe_out, fds_to_close);
      QUIT;

      if (tc->flags & CMD_STDIN_REDIR)
	tc->flags &= ~CMD_STDIN_REDIR;

      second = command->value.Connection->second;
      if (second)
	{
	  if (ignore_return)
	    second->flags |= CMD_IGNORE_RETURN;

	  exec_result = execute_command_internal (second, asynchronous, pipe_in, pipe_out, fds_to_close);
	}

      break;

     
    case ';':
    case '\n':		 
      if (ignore_return)
	{
	  if (command->value.Connection->first)
	    command->value.Connection->first->flags |= CMD_IGNORE_RETURN;
	  if (command->value.Connection->second)
	    command->value.Connection->second->flags |= CMD_IGNORE_RETURN;
	}
      executing_list++;
      QUIT;

#if 1
      execute_command (command->value.Connection->first);
#else
      execute_command_internal (command->value.Connection->first,
				  asynchronous, pipe_in, pipe_out,
				  fds_to_close);
#endif

      QUIT;
      optimize_connection_fork (command);			 
      exec_result = execute_command_internal (command->value.Connection->second,
				      asynchronous, pipe_in, pipe_out,
				      fds_to_close);
      executing_list--;
      break;

    case '|':
      was_error_trap = signal_is_trapped (ERROR_TRAP) && signal_is_ignored (ERROR_TRAP) == 0;
      invert = (command->flags & CMD_INVERT_RETURN) != 0;
      ignore_return = (command->flags & CMD_IGNORE_RETURN) != 0;

      SET_LINE_NUMBER (line_number);	 
      exec_result = execute_pipeline (command, asynchronous, pipe_in, pipe_out, fds_to_close);

      if (asynchronous)
	{
	  exec_result = EXECUTION_SUCCESS;
	  invert = 0;
	}

      if (was_error_trap && ignore_return == 0 && invert == 0 && exec_result != EXECUTION_SUCCESS)
	{
	  last_command_exit_value = exec_result;
	  save_line_number = line_number;
	  line_number = line_number_for_err_trap;
	  run_error_trap ();
	  line_number = save_line_number;
	}

      if (ignore_return == 0 && invert == 0 && exit_immediately_on_error && exec_result != EXECUTION_SUCCESS)
	{
	  last_command_exit_value = exec_result;
	  run_pending_traps ();
	  jump_to_top_level (ERREXIT);
	}

      break;

    case AND_AND:
    case OR_OR:
      if (asynchronous)
	{
	   
	  command->flags |= CMD_FORCE_SUBSHELL;
	  exec_result = execute_command_internal (command, 1, pipe_in, pipe_out, fds_to_close);
	  break;
	}

       

      executing_list++;
      if (command->value.Connection->first)
	command->value.Connection->first->flags |= CMD_IGNORE_RETURN;

#if 1
      exec_result = execute_command (command->value.Connection->first);
#else
      exec_result = execute_command_internal (command->value.Connection->first, 0, NO_PIPE, NO_PIPE, fds_to_close);
#endif
      QUIT;
      if (((command->value.Connection->connector == AND_AND) &&
	   (exec_result == EXECUTION_SUCCESS)) ||
	  ((command->value.Connection->connector == OR_OR) &&
	   (exec_result != EXECUTION_SUCCESS)))
	{
	  optimize_connection_fork (command);

	  second = command->value.Connection->second;
	  if (ignore_return && second)
	    second->flags |= CMD_IGNORE_RETURN;

	  exec_result = execute_command (second);
	}
      executing_list--;
      break;

    default:
      command_error ("execute_connection", CMDERR_BADCONN, command->value.Connection->connector, 0);
      jump_to_top_level (DISCARD);
      exec_result = EXECUTION_FAILURE;
    }

  return exec_result;
}

 
#define REAP() \
  do \
    { \
      if (job_control == 0 || interactive_shell == 0) \
	reap_dead_jobs (); \
    } \
  while (0)

 
static int
execute_for_command (for_command)
     FOR_COM *for_command;
{
  register WORD_LIST *releaser, *list;
  SHELL_VAR *v;
  char *identifier;
  int retval, save_line_number;
#if 0
  SHELL_VAR *old_value = (SHELL_VAR *)NULL;  
#endif

  save_line_number = line_number;
  if (check_identifier (for_command->name, 1) == 0)
    {
      if (posixly_correct && interactive_shell == 0)
	{
	  last_command_exit_value = EX_BADUSAGE;
	  jump_to_top_level (ERREXIT);
	}
      return (EXECUTION_FAILURE);
    }

  loop_level++;
  identifier = for_command->name->word;

  line_number = for_command->line;	 
  list = releaser = expand_words_no_vars (for_command->map_list);

  begin_unwind_frame ("for");
  add_unwind_protect (dispose_words, releaser);

#if 0
  if (lexical_scoping)
    {
      old_value = copy_variable (find_variable (identifier));
      if (old_value)
	add_unwind_protect (dispose_variable, old_value);
    }
#endif

  if (for_command->flags & CMD_IGNORE_RETURN)
    for_command->action->flags |= CMD_IGNORE_RETURN;

  for (retval = EXECUTION_SUCCESS; list; list = list->next)
    {
      QUIT;

      line_number = for_command->line;

       
      command_string_index = 0;
      print_for_command_head (for_command);

      if (echo_command_at_execute)
	xtrace_print_for_command_head (for_command);

       
      if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
	{
	  FREE (the_printed_command_except_trap);
	  the_printed_command_except_trap = savestring (the_printed_command);
	}

      retval = run_debug_trap ();
#if defined (DEBUGGER)
       
      if (debugging_mode && retval != EXECUTION_SUCCESS)
        continue;
#endif

      this_command_name = (char *)NULL;
       
      v = find_variable_last_nameref (identifier, 1);
      if (v && nameref_p (v))
	{
	  if (valid_nameref_value (list->word->word, 1) == 0)
	    {
	      sh_invalidid (list->word->word);
	      v = 0;
	    }
	  else if (readonly_p (v))
	    err_readonly (name_cell (v));
	  else
	    v = bind_variable_value (v, list->word->word, ASS_NAMEREF);
	}
      else
	v = bind_variable (identifier, list->word->word, 0);

      if (v == 0 || readonly_p (v) || noassign_p (v))
	{
	  line_number = save_line_number;
	  if (v && readonly_p (v) && interactive_shell == 0 && posixly_correct)
	    {
	      last_command_exit_value = EXECUTION_FAILURE;
	      jump_to_top_level (FORCE_EOF);
	    }
	  else
	    {
	      dispose_words (releaser);
	      discard_unwind_frame ("for");
	      loop_level--;
	      return (EXECUTION_FAILURE);
	    }
	}

      if (ifsname (identifier))
	setifs (v);
      else
	stupidly_hack_special_variables (identifier);

      retval = execute_command (for_command->action);
      REAP ();
      QUIT;

      if (breaking)
	{
	  breaking--;
	  break;
	}

      if (continuing)
	{
	  continuing--;
	  if (continuing)
	    break;
	}
    }

  loop_level--;
  line_number = save_line_number;

#if 0
  if (lexical_scoping)
    {
      if (!old_value)
        unbind_variable (identifier);
      else
	{
	  SHELL_VAR *new_value;

	  new_value = bind_variable (identifier, value_cell (old_value), 0);
	  new_value->attributes = old_value->attributes;
	  dispose_variable (old_value);
	}
    }
#endif

  dispose_words (releaser);
  discard_unwind_frame ("for");
  return (retval);
}

#if defined (ARITH_FOR_COMMAND)
 
static intmax_t
eval_arith_for_expr (l, okp)
     WORD_LIST *l;
     int *okp;
{
  WORD_LIST *new;
  intmax_t expresult;
  int r, eflag;
  char *expr, *temp;

  expr = l->next ? string_list (l) : l->word->word;
  temp = expand_arith_string (expr, Q_DOUBLE_QUOTES|Q_ARITH);
  if (l->next)
    free (expr);
  new = make_word_list (make_word (temp ? temp : ""), (WORD_LIST *)NULL);
  free (temp);

  if (new)
    {
      if (echo_command_at_execute)
	xtrace_print_arith_cmd (new);

      command_string_index = 0;
      print_arith_command (new);
      if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
	{
	  FREE (the_printed_command_except_trap);
	  the_printed_command_except_trap = savestring (the_printed_command);
	}

      r = run_debug_trap ();
       
      eflag = (shell_compatibility_level > 51) ? 0 : EXP_EXPANDED;
      this_command_name = "((";		 
      
#if defined (DEBUGGER)
      if (debugging_mode == 0 || r == EXECUTION_SUCCESS)
	expresult = evalexp (new->word->word, eflag, okp);
      else
	{
	  expresult = 0;
	  if (okp)
	    *okp = 1;
	}
#else
      expresult = evalexp (new->word->word, eflag, okp);
#endif
      dispose_words (new);
    }
  else
    {
      expresult = 0;
      if (okp)
	*okp = 1;
    }
  return (expresult);
}

static int
execute_arith_for_command (arith_for_command)
     ARITH_FOR_COM *arith_for_command;
{
  intmax_t expresult;
  int expok, body_status, arith_lineno, save_lineno;

  body_status = EXECUTION_SUCCESS;
  loop_level++;
  save_lineno = line_number;

  if (arith_for_command->flags & CMD_IGNORE_RETURN)
    arith_for_command->action->flags |= CMD_IGNORE_RETURN;

  this_command_name = "((";	 

   
  line_number = arith_lineno = arith_for_command->line;
  if (variable_context && interactive_shell && sourcelevel == 0)
    {
       
      line_number -= function_line_number - 1;
      if (line_number <= 0)
	line_number = 1;
    }

   
  expresult = eval_arith_for_expr (arith_for_command->init, &expok);
  if (expok == 0)
    {
      line_number = save_lineno;
      return (EXECUTION_FAILURE);
    }

  while (1)
    {
       
      line_number = arith_lineno;
      expresult = eval_arith_for_expr (arith_for_command->test, &expok);
      line_number = save_lineno;

      if (expok == 0)
	{
	  body_status = EXECUTION_FAILURE;
	  break;
	}
      REAP ();
      if (expresult == 0)
	break;

       
      QUIT;
      body_status = execute_command (arith_for_command->action);
      QUIT;

       
      if (breaking)
	{
	  breaking--;
	  break;
	}

      if (continuing)
	{
	  continuing--;
	  if (continuing)
	    break;
	}

       
      line_number = arith_lineno;
      expresult = eval_arith_for_expr (arith_for_command->step, &expok);
      line_number = save_lineno;

      if (expok == 0)
	{
	  body_status = EXECUTION_FAILURE;
	  break;
	}
    }

  loop_level--;
  line_number = save_lineno;

  return (body_status);
}
#endif

#if defined (SELECT_COMMAND)
static int LINES, COLS, tabsize;

#define RP_SPACE ") "
#define RP_SPACE_LEN 2

 
#define NUMBER_LEN(s) \
((s < 10) ? 1 \
	  : ((s < 100) ? 2 \
		      : ((s < 1000) ? 3 \
				   : ((s < 10000) ? 4 \
						 : ((s < 100000) ? 5 \
								: 6)))))

static int
displen (s)
     const char *s;
{
#if defined (HANDLE_MULTIBYTE)
  wchar_t *wcstr;
  size_t slen;
  int wclen;

  wcstr = 0;
  slen = mbstowcs (wcstr, s, 0);
  if (slen == -1)
    slen = 0;
  wcstr = (wchar_t *)xmalloc (sizeof (wchar_t) * (slen + 1));
  mbstowcs (wcstr, s, slen + 1);
  wclen = wcswidth (wcstr, slen);
  free (wcstr);
  return (wclen < 0 ? STRLEN(s) : wclen);
#else
  return (STRLEN (s));
#endif
}

static int
print_index_and_element (len, ind, list)
      int len, ind;
      WORD_LIST *list;
{
  register WORD_LIST *l;
  register int i;

  if (list == 0)
    return (0);
  for (i = ind, l = list; l && --i; l = l->next)
    ;
  if (l == 0)		 
    return (0);
  fprintf (stderr, "%*d%s%s", len, ind, RP_SPACE, l->word->word);
  return (displen (l->word->word));
}

static void
indent (from, to)
     int from, to;
{
  while (from < to)
    {
      if ((to / tabsize) > (from / tabsize))
	{
	  putc ('\t', stderr);
	  from += tabsize - from % tabsize;
	}
      else
	{
	  putc (' ', stderr);
	  from++;
	}
    }
}

static void
print_select_list (list, list_len, max_elem_len, indices_len)
     WORD_LIST *list;
     int list_len, max_elem_len, indices_len;
{
  int ind, row, elem_len, pos, cols, rows;
  int first_column_indices_len, other_indices_len;

  if (list == 0)
    {
      putc ('\n', stderr);
      return;
    }

  cols = max_elem_len ? COLS / max_elem_len : 1;
  if (cols == 0)
    cols = 1;
  rows = list_len ? list_len / cols + (list_len % cols != 0) : 1;
  cols = list_len ? list_len / rows + (list_len % rows != 0) : 1;

  if (rows == 1)
    {
      rows = cols;
      cols = 1;
    }

  first_column_indices_len = NUMBER_LEN (rows);
  other_indices_len = indices_len;

  for (row = 0; row < rows; row++)
    {
      ind = row;
      pos = 0;
      while (1)
	{
	  indices_len = (pos == 0) ? first_column_indices_len : other_indices_len;
	  elem_len = print_index_and_element (indices_len, ind + 1, list);
	  elem_len += indices_len + RP_SPACE_LEN;
	  ind += rows;
	  if (ind >= list_len)
	    break;
	  indent (pos + elem_len, pos + max_elem_len);
	  pos += max_elem_len;
	}
      putc ('\n', stderr);
    }
}

 
static char *
select_query (list, list_len, prompt, print_menu)
     WORD_LIST *list;
     int list_len;
     char *prompt;
     int print_menu;
{
  int max_elem_len, indices_len, len, r, oe;
  intmax_t reply;
  WORD_LIST *l;
  char *repl_string, *t;

  COLS = default_columns ();

#if 0
  t = get_string_value ("TABSIZE");
  tabsize = (t && *t) ? atoi (t) : 8;
  if (tabsize <= 0)
    tabsize = 8;
#else
  tabsize = 8;
#endif

  max_elem_len = 0;
  for (l = list; l; l = l->next)
    {
      len = displen (l->word->word);
      if (len > max_elem_len)
	max_elem_len = len;
    }
  indices_len = NUMBER_LEN (list_len);
  max_elem_len += indices_len + RP_SPACE_LEN + 2;

  while (1)
    {
      if (print_menu)
	print_select_list (list, list_len, max_elem_len, indices_len);
      fprintf (stderr, "%s", prompt);
      fflush (stderr);
      QUIT;

      oe = executing_builtin;
      executing_builtin = 1;
      r = read_builtin ((WORD_LIST *)NULL);
      executing_builtin = oe;
      if (r != EXECUTION_SUCCESS)
	{
	  putchar ('\n');
	  return ((char *)NULL);
	}
      repl_string = get_string_value ("REPLY");
      if (repl_string == 0)
	return ((char *)NULL);
      if (*repl_string == 0)
	{
	  print_menu = 1;
	  continue;
	}
      if (legal_number (repl_string, &reply) == 0)
	return "";
      if (reply < 1 || reply > list_len)
	return "";

      for (l = list; l && --reply; l = l->next)
	;
      return (l->word->word);		 
    }
}

 
static int
execute_select_command (select_command)
     SELECT_COM *select_command;
{
  WORD_LIST *releaser, *list;
  SHELL_VAR *v;
  char *identifier, *ps3_prompt, *selection;
  int retval, list_len, show_menu, save_line_number;

  if (check_identifier (select_command->name, 1) == 0)
    return (EXECUTION_FAILURE);

  save_line_number = line_number;
  line_number = select_command->line;

  command_string_index = 0;
  print_select_command_head (select_command);

  if (echo_command_at_execute)
    xtrace_print_select_command_head (select_command);

#if 0
  if (signal_in_progress (DEBUG_TRAP) == 0 && (this_command_name == 0 || (STREQ (this_command_name, "trap") == 0)))
#else
  if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
#endif
    {
      FREE (the_printed_command_except_trap);
      the_printed_command_except_trap = savestring (the_printed_command);
    }

  retval = run_debug_trap ();
#if defined (DEBUGGER)
   
  if (debugging_mode && retval != EXECUTION_SUCCESS)
    return (EXECUTION_SUCCESS);
#endif

  this_command_name = (char *)0;

  loop_level++;
  identifier = select_command->name->word;

   
  list = releaser = expand_words_no_vars (select_command->map_list);
  list_len = list_length (list);
  if (list == 0 || list_len == 0)
    {
      if (list)
	dispose_words (list);
      line_number = save_line_number;
      return (EXECUTION_SUCCESS);
    }

  begin_unwind_frame ("select");
  add_unwind_protect (dispose_words, releaser);

  if (select_command->flags & CMD_IGNORE_RETURN)
    select_command->action->flags |= CMD_IGNORE_RETURN;

  retval = EXECUTION_SUCCESS;
  show_menu = 1;

  while (1)
    {
      line_number = select_command->line;
      ps3_prompt = get_string_value ("PS3");
      if (ps3_prompt == 0)
	ps3_prompt = "#? ";

      QUIT;
      selection = select_query (list, list_len, ps3_prompt, show_menu);
      QUIT;
      if (selection == 0)
	{
	   
	  retval = EXECUTION_FAILURE;
	  break;
	}

      v = bind_variable (identifier, selection, 0);
      if (v == 0 || readonly_p (v) || noassign_p (v))
	{
	  if (v && readonly_p (v) && interactive_shell == 0 && posixly_correct)
	    {
	      last_command_exit_value = EXECUTION_FAILURE;
	      jump_to_top_level (FORCE_EOF);
	    }
	  else
	    {
	      dispose_words (releaser);
	      discard_unwind_frame ("select");
	      loop_level--;
	      line_number = save_line_number;
	      return (EXECUTION_FAILURE);
	    }
	}

      stupidly_hack_special_variables (identifier);

      retval = execute_command (select_command->action);

      REAP ();
      QUIT;

      if (breaking)
	{
	  breaking--;
	  break;
	}

      if (continuing)
	{
	  continuing--;
	  if (continuing)
	    break;
	}

#if defined (KSH_COMPATIBLE_SELECT)
      show_menu = 0;
      selection = get_string_value ("REPLY");
      if (selection && *selection == '\0')
        show_menu = 1;
#endif
    }

  loop_level--;
  line_number = save_line_number;

  dispose_words (releaser);
  discard_unwind_frame ("select");
  return (retval);
}
#endif  

 
static int
execute_case_command (case_command)
     CASE_COM *case_command;
{
  register WORD_LIST *list;
  WORD_LIST *wlist, *es;
  PATTERN_LIST *clauses;
  char *word, *pattern;
  int retval, match, ignore_return, save_line_number, qflags;

  save_line_number = line_number;
  line_number = case_command->line;

  command_string_index = 0;
  print_case_command_head (case_command);

  if (echo_command_at_execute)
    xtrace_print_case_command_head (case_command);

#if 0
  if (signal_in_progress (DEBUG_TRAP) == 0 && (this_command_name == 0 || (STREQ (this_command_name, "trap") == 0)))
#else
  if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
#endif
    {
      FREE (the_printed_command_except_trap);
      the_printed_command_except_trap = savestring (the_printed_command);
    }

  retval = run_debug_trap();
#if defined (DEBUGGER)
   
  if (debugging_mode && retval != EXECUTION_SUCCESS)
    {
      line_number = save_line_number;
      return (EXECUTION_SUCCESS);
    }
#endif

   
  wlist = expand_word_leave_quoted (case_command->word, 0);
  if (wlist)
    {
      char *t;
      t = string_list (wlist);
      word = dequote_string (t);
      free (t);
    }
  else
    word = savestring ("");
  dispose_words (wlist);

  retval = EXECUTION_SUCCESS;
  ignore_return = case_command->flags & CMD_IGNORE_RETURN;

  begin_unwind_frame ("case");
  add_unwind_protect (xfree, word);

#define EXIT_CASE()  goto exit_case_command

  for (clauses = case_command->clauses; clauses; clauses = clauses->next)
    {
      QUIT;
      for (list = clauses->patterns; list; list = list->next)
	{
	  es = expand_word_leave_quoted (list->word, 0);

	  if (es && es->word && es->word->word && *(es->word->word))
	    {
	       
	      qflags = QGLOB_CVTNULL;

	       
	      qflags |= QGLOB_CTLESC;
	      pattern = quote_string_for_globbing (es->word->word, qflags);
	    }
	  else
	    {
	      pattern = (char *)xmalloc (1);
	      pattern[0] = '\0';
	    }

	   
	  match = strmatch (pattern, word, FNMATCH_EXTFLAG|FNMATCH_IGNCASE) != FNM_NOMATCH;
	  free (pattern);

	  dispose_words (es);
	  QUIT;

	  if (match)
	    {
	      do
		{
		  if (clauses->action && ignore_return)
		    clauses->action->flags |= CMD_IGNORE_RETURN;
		  retval = execute_command (clauses->action);
		}
	      while ((clauses->flags & CASEPAT_FALLTHROUGH) && (clauses = clauses->next));
	      if (clauses == 0 || (clauses->flags & CASEPAT_TESTNEXT) == 0)
		EXIT_CASE ();
	      else
		break;
	    }

	  QUIT;
	}
    }

exit_case_command:
  free (word);
  discard_unwind_frame ("case");
  line_number = save_line_number;
  return (retval);
}

#define CMD_WHILE 0
#define CMD_UNTIL 1

 
static int
execute_while_command (while_command)
     WHILE_COM *while_command;
{
  return (execute_while_or_until (while_command, CMD_WHILE));
}

 
static int
execute_until_command (while_command)
     WHILE_COM *while_command;
{
  return (execute_while_or_until (while_command, CMD_UNTIL));
}

 
static int
execute_while_or_until (while_command, type)
     WHILE_COM *while_command;
     int type;
{
  int return_value, body_status;

  body_status = EXECUTION_SUCCESS;
  loop_level++;

  while_command->test->flags |= CMD_IGNORE_RETURN;
  if (while_command->flags & CMD_IGNORE_RETURN)
    while_command->action->flags |= CMD_IGNORE_RETURN;

  while (1)
    {
      return_value = execute_command (while_command->test);
      REAP ();

       
      if (type == CMD_WHILE && return_value != EXECUTION_SUCCESS)
	{
	  if (breaking)
	    breaking--;
	  if (continuing)
	    continuing--;
	  break;
	}
      if (type == CMD_UNTIL && return_value == EXECUTION_SUCCESS)
	{
	  if (breaking)
	    breaking--;
	  if (continuing)
	    continuing--;
	  break;
	}

      QUIT;
      body_status = execute_command (while_command->action);
      QUIT;

      if (breaking)
	{
	  breaking--;
	  break;
	}

      if (continuing)
	{
	  continuing--;
	  if (continuing)
	    break;
	}
    }
  loop_level--;

  return (body_status);
}

 
static int
execute_if_command (if_command)
     IF_COM *if_command;
{
  int return_value, save_line_number;

  save_line_number = line_number;
  if_command->test->flags |= CMD_IGNORE_RETURN;
  return_value = execute_command (if_command->test);
  line_number = save_line_number;

  if (return_value == EXECUTION_SUCCESS)
    {
      QUIT;

      if (if_command->true_case && (if_command->flags & CMD_IGNORE_RETURN))
	if_command->true_case->flags |= CMD_IGNORE_RETURN;

      return (execute_command (if_command->true_case));
    }
  else
    {
      QUIT;

      if (if_command->false_case && (if_command->flags & CMD_IGNORE_RETURN))
	if_command->false_case->flags |= CMD_IGNORE_RETURN;

      return (execute_command (if_command->false_case));
    }
}

#if defined (DPAREN_ARITHMETIC)
static int
execute_arith_command (arith_command)
     ARITH_COM *arith_command;
{
  int expok, save_line_number, retval, eflag;
  intmax_t expresult;
  WORD_LIST *new;
  char *exp, *t;

  expresult = 0;

  save_line_number = line_number;
  this_command_name = "((";	 
  SET_LINE_NUMBER (arith_command->line);
   
  if (variable_context && interactive_shell && sourcelevel == 0)
    {
       
      line_number -= function_line_number - 1;
      if (line_number <= 0)
	line_number = 1;
    }      

  command_string_index = 0;
  print_arith_command (arith_command->exp);

  if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
    {
      FREE (the_printed_command_except_trap);
      the_printed_command_except_trap = savestring (the_printed_command);
    }

   
  retval = run_debug_trap ();
#if defined (DEBUGGER)
   
  if (debugging_mode && retval != EXECUTION_SUCCESS)
    {
      line_number = save_line_number;
      return (EXECUTION_SUCCESS);
    }
#endif

  this_command_name = "((";	 
  t = (char *)NULL;
  new = arith_command->exp;
  exp = (new->next) ? (t = string_list (new)) : new->word->word;

  exp = expand_arith_string (exp, Q_DOUBLE_QUOTES|Q_ARITH);
  FREE (t);

   
  if (echo_command_at_execute)
    {
      new = make_word_list (make_word (exp ? exp : ""), (WORD_LIST *)NULL);
      xtrace_print_arith_cmd (new);
      dispose_words (new);
    }

  if (exp)
    {
      eflag = (shell_compatibility_level > 51) ? 0 : EXP_EXPANDED;
      expresult = evalexp (exp, eflag, &expok);
      line_number = save_line_number;
      free (exp);
    }
  else
    {
      expresult = 0;
      expok = 1;
    }

  if (expok == 0)
    return (EXECUTION_FAILURE);

  return (expresult == 0 ? EXECUTION_FAILURE : EXECUTION_SUCCESS);
}
#endif  

#if defined (COND_COMMAND)

static char * const nullstr = "";

 
static int
execute_cond_node (cond)
     COND_COM *cond;
{
  int result, invert, patmatch, rmatch, arith, mode, mflags, ignore;
  char *arg1, *arg2, *op;
#if 0
  char *t1, *t2;
#endif

  invert = (cond->flags & CMD_INVERT_RETURN);
  ignore = (cond->flags & CMD_IGNORE_RETURN);
  if (ignore)
    {
      if (cond->left)
	cond->left->flags |= CMD_IGNORE_RETURN;
      if (cond->right)
	cond->right->flags |= CMD_IGNORE_RETURN;
    }
      
  if (cond->type == COND_EXPR)
    result = execute_cond_node (cond->left);
  else if (cond->type == COND_OR)
    {
      result = execute_cond_node (cond->left);
      if (result != EXECUTION_SUCCESS)
	result = execute_cond_node (cond->right);
    }
  else if (cond->type == COND_AND)
    {
      result = execute_cond_node (cond->left);
      if (result == EXECUTION_SUCCESS)
	result = execute_cond_node (cond->right);
    }
  else if (cond->type == COND_UNARY)
    {
      int oa, varop, varflag;

      if (ignore)
	comsub_ignore_return++;
      varop = STREQ (cond->op->word, "-v");
#if defined (ARRAY_VARS)
      varflag = (varop && valid_array_reference (cond->left->op->word, VA_NOEXPAND)) ? TEST_ARRAYEXP : 0;
#else
      varflag = 0;
#endif
      arg1 = cond_expand_word (cond->left->op, varop ? 3 : 0);
      if (ignore)
	comsub_ignore_return--;
      if (arg1 == 0)
	arg1 = nullstr;
      if (echo_command_at_execute)
	xtrace_print_cond_term (cond->type, invert, cond->op, arg1, (char *)NULL);
#if defined (ARRAY_VARS)
      if (varop)
	oa = set_expand_once (0, 0);	 
#endif
      result = unary_test (cond->op->word, arg1, varflag) ? EXECUTION_SUCCESS : EXECUTION_FAILURE;
#if defined (ARRAY_VARS)
      if (varop)
	assoc_expand_once = oa;
#endif
      if (arg1 != nullstr)
	free (arg1);
    }
  else if (cond->type == COND_BINARY)
    {
      rmatch = 0;
      op = cond->op->word;
      mode = 0;
      patmatch = (((op[1] == '=') && (op[2] == '\0') &&
		   (op[0] == '!' || op[0] == '=')) ||
		  (op[0] == '=' && op[1] == '\0'));
#if defined (COND_REGEXP)
      rmatch = (op[0] == '=' && op[1] == '~' && op[2] == '\0');
#endif
      arith = STREQ (op, "-eq") || STREQ (op, "-ne") || STREQ (op, "-lt") ||
	      STREQ (op, "-le") || STREQ (op, "-gt") || STREQ (op, "-ge");

      if (arith)
	mode = 3;
      else if (rmatch && shell_compatibility_level > 31)
	mode = 2;
      else if (patmatch)
	mode = 1;

      if (ignore)
	comsub_ignore_return++;
      arg1 = cond_expand_word (cond->left->op, arith ? mode : 0);
      if (ignore)
	comsub_ignore_return--;
      if (arg1 == 0)
	arg1 = nullstr;
      if (ignore)
	comsub_ignore_return++;
      arg2 = cond_expand_word (cond->right->op, mode);
      if (ignore)
	comsub_ignore_return--;
      if (arg2 == 0)
	arg2 = nullstr;

      if (echo_command_at_execute)
	xtrace_print_cond_term (cond->type, invert, cond->op, arg1, arg2);

#if defined (COND_REGEXP)
      if (rmatch)
	{
	  mflags = SHMAT_PWARN;
#if defined (ARRAY_VARS)
	  mflags |= SHMAT_SUBEXP;
#endif

#if 0
	  t1 = strescape(arg1);
	  t2 = strescape(arg2);
	  itrace("execute_cond_node: sh_regmatch on `%s' and `%s'", t1, t2);
	  free(t1);
	  free(t2);
#endif

	  result = sh_regmatch (arg1, arg2, mflags);
	}
      else
#endif  
	{
	  extended_glob = 1;
	  result = binary_test (cond->op->word, arg1, arg2, TEST_PATMATCH|TEST_ARITHEXP|TEST_LOCALE)
				  ? EXECUTION_SUCCESS
				  : EXECUTION_FAILURE;
	  extended_glob = extglob_flag;
	}
      if (arg1 != nullstr)
	free (arg1);
      if (arg2 != nullstr)
	free (arg2);
    }
  else
    {
      command_error ("execute_cond_node", CMDERR_BADTYPE, cond->type, 0);
      jump_to_top_level (DISCARD);
      result = EXECUTION_FAILURE;
    }

  if (invert)
    result = (result == EXECUTION_SUCCESS) ? EXECUTION_FAILURE : EXECUTION_SUCCESS;

  return result;
}

static int
execute_cond_command (cond_command)
     COND_COM *cond_command;
{
  int retval, save_line_number;

  save_line_number = line_number;

  SET_LINE_NUMBER (cond_command->line);
   
  if (variable_context && interactive_shell && sourcelevel == 0)
    {
       
      line_number -= function_line_number - 1;
      if (line_number <= 0)
	line_number = 1;
    }
  command_string_index = 0;
  print_cond_command (cond_command);

  if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
    {
      FREE (the_printed_command_except_trap);
      the_printed_command_except_trap = savestring (the_printed_command);
    }

   
  retval = run_debug_trap ();
#if defined (DEBUGGER)
   
  if (debugging_mode && retval != EXECUTION_SUCCESS)
    {
      line_number = save_line_number;
      return (EXECUTION_SUCCESS);
    }
#endif

  this_command_name = "[[";	 

#if 0
  debug_print_cond_command (cond_command);
#endif

  last_command_exit_value = retval = execute_cond_node (cond_command);
  line_number = save_line_number;
  return (retval);
}
#endif  

static void
bind_lastarg (arg)
     char *arg;
{
  SHELL_VAR *var;

  if (arg == 0)
    arg = "";
  var = bind_variable ("_", arg, 0);
  if (var)
    VUNSETATTR (var, att_exported);
}

 
static int
execute_null_command (redirects, pipe_in, pipe_out, async)
     REDIRECT *redirects;
     int pipe_in, pipe_out, async;
{
  int r;
  int forcefork, fork_flags;
  REDIRECT *rd;

  for (forcefork = 0, rd = redirects; rd; rd = rd->next)
    {
      forcefork += rd->rflags & REDIR_VARASSIGN;
       
      forcefork += (rd->redirector.dest == 0 || fd_is_bash_input (rd->redirector.dest)) && (INPUT_REDIRECT (rd->instruction) || TRANSLATE_REDIRECT (rd->instruction) || rd->instruction == r_close_this);
    }

  if (forcefork || pipe_in != NO_PIPE || pipe_out != NO_PIPE || async)
    {
       
      fork_flags = async ? FORK_ASYNC : 0;
      if (make_child ((char *)NULL, fork_flags) == 0)
	{
	   
	  restore_original_signals ();		 

	  do_piping (pipe_in, pipe_out);

#if defined (COPROCESS_SUPPORT)
	  coproc_closeall ();
#endif

	  interactive = 0;			 

	  subshell_environment = 0;
	  if (async)
	    subshell_environment |= SUBSHELL_ASYNC;
	  if (pipe_in != NO_PIPE || pipe_out != NO_PIPE)
	    subshell_environment |= SUBSHELL_PIPE;

	  if (do_redirections (redirects, RX_ACTIVE) == 0)
	    exit (EXECUTION_SUCCESS);
	  else
	    exit (EXECUTION_FAILURE);
	}
      else
	{
	  close_pipes (pipe_in, pipe_out);
#if defined (PROCESS_SUBSTITUTION) && defined (HAVE_DEV_FD)
	  if (pipe_out == NO_PIPE)
	    unlink_fifo_list ();
#endif
	  return (EXECUTION_SUCCESS);
	}
    }
  else
    {
       

      r = do_redirections (redirects, RX_ACTIVE|RX_UNDOABLE);
      cleanup_redirects (redirection_undo_list);
      redirection_undo_list = (REDIRECT *)NULL;

      if (r != 0)
	return (EXECUTION_FAILURE);
      else if (last_command_subst_pid != NO_PID)
	return (last_command_exit_value);
      else
	return (EXECUTION_SUCCESS);
    }
}

 
static void
fix_assignment_words (words)
     WORD_LIST *words;
{
  WORD_LIST *w, *wcmd;
  struct builtin *b;
  int assoc, global, array, integer;

  if (words == 0)
    return;

  b = 0;
  assoc = global = array = integer = 0;

   
  wcmd = words;
  for (wcmd = words; wcmd; wcmd = wcmd->next)
    if ((wcmd->word->flags & W_ASSIGNMENT) == 0)
      break;
   
  while (posixly_correct && wcmd && wcmd->word && wcmd->word->word && STREQ (wcmd->word->word, "command"))
    wcmd = wcmd->next;

  for (w = wcmd; w; w = w->next)
    if (w->word->flags & W_ASSIGNMENT)
      {
      	 
	if (b == 0)
	  {
	    b = builtin_address_internal (wcmd->word->word, 0);
	    if (b == 0 || (b->flags & ASSIGNMENT_BUILTIN) == 0)
	      return;
	    else if (b && (b->flags & ASSIGNMENT_BUILTIN))
	      wcmd->word->flags |= W_ASSNBLTIN;
	  }
	w->word->flags |= (W_NOSPLIT|W_NOGLOB|W_TILDEEXP|W_ASSIGNARG);
#if defined (ARRAY_VARS)
	if (assoc)
	  w->word->flags |= W_ASSIGNASSOC;
	if (array)
	  w->word->flags |= W_ASSIGNARRAY;
#endif
	if (global)
	  w->word->flags |= W_ASSNGLOBAL;

	 
	if (b && ((b->flags & (ASSIGNMENT_BUILTIN|LOCALVAR_BUILTIN)) == ASSIGNMENT_BUILTIN))
	  w->word->flags |= W_ASSNGLOBAL|W_CHKLOCAL;
	else if (b && (b->flags & ASSIGNMENT_BUILTIN) && (b->flags & LOCALVAR_BUILTIN) && variable_context)
	  w->word->flags |= W_FORCELOCAL;
      }
#if defined (ARRAY_VARS)
     
    else if (w->word->word[0] == '-' && (strpbrk (w->word->word+1, "Aag") != 0))
#else
    else if (w->word->word[0] == '-' && strchr (w->word->word+1, 'g'))
#endif
      {
	if (b == 0)
	  {
	    b = builtin_address_internal (wcmd->word->word, 0);
	    if (b == 0 || (b->flags & ASSIGNMENT_BUILTIN) == 0)
	      return;
	    else if (b && (b->flags & ASSIGNMENT_BUILTIN))
	      wcmd->word->flags |= W_ASSNBLTIN;
	  }
	if ((wcmd->word->flags & W_ASSNBLTIN) && strchr (w->word->word+1, 'A'))
	  assoc = 1;
	else if ((wcmd->word->flags & W_ASSNBLTIN) && strchr (w->word->word+1, 'a'))
	  array = 1;
	if ((wcmd->word->flags & W_ASSNBLTIN) && strchr (w->word->word+1, 'g'))
	  global = 1;
      }
}

#if defined (ARRAY_VARS)
 
static void
fix_arrayref_words (words)
     WORD_LIST *words;
{
  WORD_LIST *w, *wcmd;
  struct builtin *b;

  if (words == 0)
    return;

  b = 0;

   
  wcmd = words;
  for (wcmd = words; wcmd; wcmd = wcmd->next)
    if ((wcmd->word->flags & W_ASSIGNMENT) == 0)
      break;

   
  while (wcmd && wcmd->word && wcmd->word->word && STREQ (wcmd->word->word, "command"))
    wcmd = wcmd->next;

  if (wcmd == 0)
    return;

   
  b = builtin_address_internal (wcmd->word->word, 0);
  if (b == 0 || (b->flags & ARRAYREF_BUILTIN) == 0)
    return;

  for (w = wcmd->next; w; w = w->next)
    {
      if (w->word && w->word->word && valid_array_reference (w->word->word, 0))
	w->word->flags |= W_ARRAYREF;
    }
}
#endif

#ifndef ISOPTION
#  define ISOPTION(s, c)  (s[0] == '-' && s[1] == c && s[2] == 0)
#endif

#define RETURN_NOT_COMMAND() \
  do { if (typep) *typep = 0; return words; } while (0)

 
static WORD_LIST *
check_command_builtin (words, typep)
     WORD_LIST *words;
     int *typep;
{
  int type;
  WORD_LIST *w;

  w = words->next;
  type = 1;

  if (w && ISOPTION (w->word->word, 'p'))	 
    {
#if defined (RESTRICTED_SHELL)
      if (restricted)
        RETURN_NOT_COMMAND();
#endif
      w = w->next;
      type = 2;
    }

  if (w && ISOPTION (w->word->word, '-'))	 
    w = w->next;
  else if (w && w->word->word[0] == '-')	 
    RETURN_NOT_COMMAND();

  if (w == 0 || w->word->word == 0)		 
    RETURN_NOT_COMMAND();

  if (typep)
    *typep = type;
  return w;
}

 
static int
is_dirname (pathname)
     char *pathname;
{
  char *temp;
  int ret;

  temp = search_for_command (pathname, 0);
  ret = temp ? file_isdir (temp) : file_isdir (pathname);
  free (temp);
  return ret;
}

 
static int
execute_simple_command (simple_command, pipe_in, pipe_out, async, fds_to_close)
     SIMPLE_COM *simple_command;
     int pipe_in, pipe_out, async;
     struct fd_bitmap *fds_to_close;
{
  WORD_LIST *words, *lastword;
  char *command_line, *lastarg, *temp;
  int first_word_quoted, result, builtin_is_special, already_forked, dofork;
  int fork_flags, cmdflags;
  pid_t old_last_async_pid;
  sh_builtin_func_t *builtin;
  SHELL_VAR *func;
  volatile int old_builtin, old_command_builtin;

  result = EXECUTION_SUCCESS;
  special_builtin_failed = builtin_is_special = 0;
  command_line = (char *)0;

  QUIT;

   
  if (variable_context && interactive_shell && sourcelevel == 0)
    {
       
      line_number -= function_line_number - 1;
      if (line_number <= 0)
	line_number = 1;
    }

   
  command_string_index = 0;
  print_simple_command (simple_command);

#if 0
  if (signal_in_progress (DEBUG_TRAP) == 0 && (this_command_name == 0 || (STREQ (this_command_name, "trap") == 0)))
#else
  if (signal_in_progress (DEBUG_TRAP) == 0 && running_trap == 0)
#endif
    {
      FREE (the_printed_command_except_trap);
      the_printed_command_except_trap = the_printed_command ? savestring (the_printed_command) : (char *)0;
    }

   
  result = run_debug_trap ();
#if defined (DEBUGGER)
   
  if (debugging_mode && result != EXECUTION_SUCCESS)
    return (EXECUTION_SUCCESS);
#endif

  cmdflags = simple_command->flags;

  first_word_quoted =
    simple_command->words ? (simple_command->words->word->flags & W_QUOTED) : 0;

  last_command_subst_pid = NO_PID;
  old_last_async_pid = last_asynchronous_pid;

  already_forked = 0;

   
  dofork = pipe_in != NO_PIPE || pipe_out != NO_PIPE || async;

   
  if (dofork && pipe_in == NO_PIPE && pipe_out == NO_PIPE &&
	simple_command->words && simple_command->words->word &&
	simple_command->words->word->word &&
	(simple_command->words->word->word[0] == '%'))
    dofork = 0;

  if (dofork)
    {
      char *p;

       
      maybe_make_export_env ();

       
      fork_flags = async ? FORK_ASYNC : 0;
      if (make_child (p = savestring (the_printed_command_except_trap), fork_flags) == 0)
	{
	  already_forked = 1;
	  cmdflags |= CMD_NO_FORK;

	   
	  subshell_environment = SUBSHELL_FORK|SUBSHELL_IGNTRAP;	 
	  if (pipe_in != NO_PIPE || pipe_out != NO_PIPE)
	    subshell_environment |= SUBSHELL_PIPE;
	  if (async)
	    subshell_environment |= SUBSHELL_ASYNC;

	   
	  if (fds_to_close)
	    close_fd_bitmap (fds_to_close);

	   
	  stdin_redir |= pipe_in != NO_PIPE;

	  do_piping (pipe_in, pipe_out);
	  pipe_in = pipe_out = NO_PIPE;
#if defined (COPROCESS_SUPPORT)
	  coproc_closeall ();
#endif

	  last_asynchronous_pid = old_last_async_pid;

	  if (async)
	    subshell_level++;		 

#if defined (JOB_CONTROL)
	  FREE (p);			 
#endif
	}
      else
	{
	   
	  if (pipe_out != NO_PIPE)
	    result = last_command_exit_value;
	  close_pipes (pipe_in, pipe_out);
	  command_line = (char *)NULL;       
	  return (result);
	}
    }

  QUIT;		 

   
  if ((cmdflags & CMD_INHIBIT_EXPANSION) == 0)
    {
      current_fds_to_close = fds_to_close;
      fix_assignment_words (simple_command->words);
#if defined (ARRAY_VARS)
      fix_arrayref_words (simple_command->words);
#endif
       
      if (cmdflags & CMD_IGNORE_RETURN)	 
	comsub_ignore_return++;
      words = expand_words (simple_command->words);
      if (cmdflags & CMD_IGNORE_RETURN)
	comsub_ignore_return--;
      current_fds_to_close = (struct fd_bitmap *)NULL;
    }
  else
    words = copy_word_list (simple_command->words);

   
  if (words == 0)
    {
      this_command_name = 0;
      result = execute_null_command (simple_command->redirects,
				     pipe_in, pipe_out,
				     already_forked ? 0 : async);
      if (already_forked)
	sh_exit (result);
      else
	{
	  bind_lastarg ((char *)NULL);
	  set_pipestatus_from_exit (result);
	  return (result);
	}
    }

  lastarg = (char *)NULL;

  begin_unwind_frame ("simple-command");

  if (echo_command_at_execute && (cmdflags & CMD_COMMAND_BUILTIN) == 0)
    xtrace_print_word_list (words, 1);

  builtin = (sh_builtin_func_t *)NULL;
  func = (SHELL_VAR *)NULL;

   
  if ((cmdflags & CMD_NO_FUNCTIONS) == 0)
    {
       
      if (posixly_correct)
	{
	  builtin = find_special_builtin (words->word->word);
	  if (builtin)
	    builtin_is_special = 1;
	}
      if (builtin == 0)
	func = find_function (words->word->word);
    }

   
  if (posixly_correct && tempenv_assign_error)
    {
#if defined (DEBUG)
       
itrace("execute_simple_command: posix mode tempenv assignment error");
#endif
      last_command_exit_value = EXECUTION_FAILURE;
#if defined (STRICT_POSIX)
      jump_to_top_level ((interactive_shell == 0) ? FORCE_EOF : DISCARD);
#else
      if (interactive_shell == 0 && builtin_is_special)
	jump_to_top_level (FORCE_EOF);
      else if (interactive_shell == 0)
	jump_to_top_level (DISCARD);	 
      else
	jump_to_top_level (DISCARD);
#endif
    }
  tempenv_assign_error = 0;	 

   
  old_command_builtin = -1;
  if (builtin == 0 && func == 0)
    {
      WORD_LIST *disposer, *l;
      int cmdtype;

      builtin = find_shell_builtin (words->word->word);
      while (builtin == command_builtin)
	{
	  disposer = words;
	  cmdtype = 0;
	  words = check_command_builtin (words, &cmdtype);
	  if (cmdtype > 0)	 
	    {
	      for (l = disposer; l->next != words; l = l->next)
		;
	      l->next = 0;
	      dispose_words (disposer);
	      cmdflags |= CMD_COMMAND_BUILTIN | CMD_NO_FUNCTIONS;
	      if (cmdtype == 2)
		cmdflags |= CMD_STDPATH;
	      builtin = find_shell_builtin (words->word->word);
	    }
	  else
	    break;
	}
      if (cmdflags & CMD_COMMAND_BUILTIN)
	{
	  old_command_builtin = executing_command_builtin;
	  unwind_protect_int (executing_command_builtin);
	  executing_command_builtin |= 1;
	}        
      builtin = 0;
    }

  add_unwind_protect (dispose_words, words);
  QUIT;

   
  for (lastword = words; lastword->next; lastword = lastword->next)
    ;
  lastarg = lastword->word->word;

#if defined (JOB_CONTROL)
   
  if (words->word->word[0] == '%' && already_forked == 0)
    {
      this_command_name = async ? "bg" : "fg";
      last_shell_builtin = this_shell_builtin;
      this_shell_builtin = builtin_address (this_command_name);
      result = (*this_shell_builtin) (words);
      goto return_result;
    }

   
  if (job_control && already_forked == 0 && async == 0 &&
	!first_word_quoted &&
	!words->next &&
	words->word->word[0] &&
	!simple_command->redirects &&
	pipe_in == NO_PIPE &&
	pipe_out == NO_PIPE &&
	(temp = get_string_value ("auto_resume")))
    {
      int job, jflags, started_status;

      jflags = JM_STOPPED|JM_FIRSTMATCH;
      if (STREQ (temp, "exact"))
	jflags |= JM_EXACT;
      else if (STREQ (temp, "substring"))
	jflags |= JM_SUBSTRING;
      else
	jflags |= JM_PREFIX;
      job = get_job_by_name (words->word->word, jflags);
      if (job != NO_JOB)
	{
	  run_unwind_frame ("simple-command");
	  this_command_name = "fg";
	  last_shell_builtin = this_shell_builtin;
	  this_shell_builtin = builtin_address ("fg");

	  started_status = start_job (job, 1);
	  return ((started_status < 0) ? EXECUTION_FAILURE : started_status);
	}
    }
#endif  

run_builtin:
   
  this_command_name = words->word->word;

  QUIT;

   
  if (func == 0 && builtin == 0)
    builtin = find_shell_builtin (this_command_name);

  last_shell_builtin = this_shell_builtin;
  this_shell_builtin = builtin;

  if (builtin || func)
    {
      if (builtin)
        {
	  old_builtin = executing_builtin;
	  unwind_protect_int (executing_builtin);	 
	  if (old_command_builtin == -1)	 
	    {
	      old_command_builtin = executing_command_builtin;
	      unwind_protect_int (executing_command_builtin);	 
	    }
        }
      if (already_forked)
	{
	   	 
	   
	  reset_signal_handlers ();
	  subshell_environment |= SUBSHELL_RESETTRAP;
	  subshell_environment &= ~SUBSHELL_IGNTRAP;

	  if (async)
	    {
	      if ((cmdflags & CMD_STDIN_REDIR) &&
		    pipe_in == NO_PIPE &&
		    (stdin_redirects (simple_command->redirects) == 0))
		async_redirect_stdin ();
	      setup_async_signals ();
	    }

	  if (async == 0)
	    subshell_level++;
	  execute_subshell_builtin_or_function
	    (words, simple_command->redirects, builtin, func,
	     pipe_in, pipe_out, async, fds_to_close,
	     cmdflags);
	  subshell_level--;
	}
      else
	{
	  result = execute_builtin_or_function
	    (words, builtin, func, simple_command->redirects, fds_to_close,
	     cmdflags);
	  if (builtin)
	    {
	      if (result > EX_SHERRBASE)
		{
		  switch (result)
		    {
		    case EX_REDIRFAIL:
		    case EX_BADASSIGN:
		    case EX_EXPFAIL:
		       
		      if (posixly_correct && builtin_is_special && interactive_shell == 0)
			{
			  last_command_exit_value = EXECUTION_FAILURE;
			  jump_to_top_level (ERREXIT);
			}
		      break;
		    case EX_DISKFALLBACK:
		       
		      executing_builtin = old_builtin;
		      executing_command_builtin = old_command_builtin;
		      builtin = 0;

		       
		      pipe_in = pipe_out = -1;
		      goto execute_from_filesystem;
		    }
		  result = builtin_status (result);
		  if (builtin_is_special)
		    special_builtin_failed = 1;	 
		}
	       
	      if (posixly_correct && builtin_is_special && temporary_env)
		merge_temporary_env ();
	    }
	  else		 
	    {
	      if (result == EX_USAGE)
		result = EX_BADUSAGE;
	      else if (result > EX_SHERRBASE)
		result = builtin_status (result);
	    }

	  set_pipestatus_from_exit (result);

	  goto return_result;
	}
    }

  if (autocd && interactive && words->word && is_dirname (words->word->word))
    {
      words = make_word_list (make_word ("--"), words);
      words = make_word_list (make_word ("cd"), words);
      xtrace_print_word_list (words, 0);
      func = find_function ("cd");
      goto run_builtin;
    }

execute_from_filesystem:
  if (command_line == 0)
    command_line = savestring (the_printed_command_except_trap ? the_printed_command_except_trap : "");

#if defined (PROCESS_SUBSTITUTION)
   
  if (already_forked == 0 && (cmdflags & CMD_NO_FORK) && fifos_pending() > 0)
    cmdflags &= ~CMD_NO_FORK;
#endif
  result = execute_disk_command (words, simple_command->redirects, command_line,
			pipe_in, pipe_out, async, fds_to_close,
			cmdflags);

 return_result:
  bind_lastarg (lastarg);
  FREE (command_line);
  dispose_words (words);
  if (builtin)
    {
      executing_builtin = old_builtin;
      executing_command_builtin = old_command_builtin;
    }
  discard_unwind_frame ("simple-command");
  this_command_name = (char *)NULL;	 
  return (result);
}

 
static int
builtin_status (result)
     int result;
{
  int r;

  switch (result)
    {
    case EX_USAGE:
    case EX_BADSYNTAX:
      r = EX_BADUSAGE;
      break;
    case EX_REDIRFAIL:
    case EX_BADASSIGN:
    case EX_EXPFAIL:
      r = EXECUTION_FAILURE;
      break;
    default:
       
      r = (result > EX_SHERRBASE) ? EXECUTION_FAILURE : EXECUTION_SUCCESS;
      break;
    }
  return (r);
}

static int
execute_builtin (builtin, words, flags, subshell)
     sh_builtin_func_t *builtin;
     WORD_LIST *words;
     int flags, subshell;
{
  int result, eval_unwind, ignexit_flag;
  int isbltinenv, should_keep;
  char *error_trap;

  error_trap = 0;
  should_keep = 0;

   
  if (subshell == 0 && (flags & CMD_IGNORE_RETURN) &&
	(builtin == eval_builtin || (flags & CMD_COMMAND_BUILTIN) || builtin == source_builtin))
    {
      begin_unwind_frame ("eval_builtin");
      unwind_protect_int (exit_immediately_on_error);
      unwind_protect_int (builtin_ignoring_errexit);
      error_trap = TRAP_STRING (ERROR_TRAP);
      if (error_trap)
	{
	  error_trap = savestring (error_trap);
	  add_unwind_protect (xfree, error_trap);
	  add_unwind_protect (set_error_trap, error_trap);
	  restore_default_signal (ERROR_TRAP);
	}
      exit_immediately_on_error = 0;
      ignexit_flag = builtin_ignoring_errexit;
      builtin_ignoring_errexit = 1;
      eval_unwind = 1;
    }
  else
    eval_unwind = 0;

   
   
  isbltinenv = (builtin == source_builtin || builtin == eval_builtin || builtin == unset_builtin || builtin == mapfile_builtin);
  should_keep = isbltinenv && builtin != mapfile_builtin;
#if defined (HISTORY) && defined (READLINE)
  if (builtin == fc_builtin || builtin == read_builtin)
    {
      isbltinenv = 1;
      should_keep = 0;
    }
#endif

  if (isbltinenv)
    {
      if (subshell == 0)
	begin_unwind_frame ("builtin_env");

      if (temporary_env)
	{
	  push_scope (VC_BLTNENV, temporary_env);
	  if (flags & CMD_COMMAND_BUILTIN)
	    should_keep = 0;
	  if (subshell == 0)
	    add_unwind_protect (pop_scope, should_keep ? "1" : 0);
          temporary_env = (HASH_TABLE *)NULL;	  
	}
    }

  if (subshell == 0 && builtin == eval_builtin)
    {
      if (evalnest_max > 0 && evalnest >= evalnest_max)
	{
	  internal_error (_("eval: maximum eval nesting level exceeded (%d)"), evalnest);
	  evalnest = 0;
	  jump_to_top_level (DISCARD);
	}
      unwind_protect_int (evalnest);
       
      evalnest++;	 
    }
  else if (subshell == 0 && builtin == source_builtin)
    {
      if (sourcenest_max > 0 && sourcenest >= sourcenest_max)
	{
	  internal_error (_("%s: maximum source nesting level exceeded (%d)"), this_command_name, sourcenest);
	  sourcenest = 0;
	  jump_to_top_level (DISCARD);
	}
      unwind_protect_int (sourcenest);
       
      sourcenest++;	 
    }

   
  if (posixly_correct && subshell == 0 && builtin == return_builtin && (flags & CMD_COMMAND_BUILTIN) == 0 && temporary_env)
    {
      begin_unwind_frame ("return_temp_env");
      add_unwind_protect (merge_temporary_env, (char *)NULL);
    }

  executing_builtin++;
  executing_command_builtin |= builtin == command_builtin;
  result = ((*builtin) (words->next));

   
  if (posixly_correct && subshell == 0 && builtin == return_builtin && temporary_env)
    discard_unwind_frame ("return_temp_env");

  if (subshell == 0 && isbltinenv)
    run_unwind_frame ("builtin_env");

  if (eval_unwind)
    {
      builtin_ignoring_errexit = ignexit_flag;
      exit_immediately_on_error = builtin_ignoring_errexit ? 0 : errexit_flag;
      if (error_trap)
	{
	  set_error_trap (error_trap);
	  free (error_trap);
	}
      discard_unwind_frame ("eval_builtin");
    }

  return (result);
}

static void
maybe_restore_getopt_state (gs)
     sh_getopt_state_t *gs;
{
   
  if (gs->gs_flags & 1)
    sh_getopt_restore_istate (gs);
  else
    free (gs);
}

#if defined (ARRAY_VARS)
void
restore_funcarray_state (fa)
     struct func_array_state *fa;
{
  SHELL_VAR *nfv;
  ARRAY *funcname_a;

  array_pop (fa->source_a);
  array_pop (fa->lineno_a);

  GET_ARRAY_FROM_VAR ("FUNCNAME", nfv, funcname_a);
  if (nfv == fa->funcname_v)
    array_pop (funcname_a);

  free (fa);
}
#endif

static int
execute_function (var, words, flags, fds_to_close, async, subshell)
     SHELL_VAR *var;
     WORD_LIST *words;
     int flags;
     struct fd_bitmap *fds_to_close;
     int async, subshell;
{
  int return_val, result, lineno;
  COMMAND *tc, *fc, *save_current;
  char *debug_trap, *error_trap, *return_trap;
#if defined (ARRAY_VARS)
  SHELL_VAR *funcname_v, *bash_source_v, *bash_lineno_v;
  ARRAY *funcname_a;
  volatile ARRAY *bash_source_a;
  volatile ARRAY *bash_lineno_a;
  struct func_array_state *fa;
#endif
  FUNCTION_DEF *shell_fn;
  char *sfile, *t;
  sh_getopt_state_t *gs;
  SHELL_VAR *gv;

  USE_VAR(fc);

  if (funcnest_max > 0 && funcnest >= funcnest_max)
    {
      internal_error (_("%s: maximum function nesting level exceeded (%d)"), var->name, funcnest);
      funcnest = 0;	 
      jump_to_top_level (DISCARD);
    }

#if defined (ARRAY_VARS)
  GET_ARRAY_FROM_VAR ("FUNCNAME", funcname_v, funcname_a);
  GET_ARRAY_FROM_VAR ("BASH_SOURCE", bash_source_v, bash_source_a);
  GET_ARRAY_FROM_VAR ("BASH_LINENO", bash_lineno_v, bash_lineno_a);
#endif

  tc = (COMMAND *)copy_command (function_cell (var));
  if (tc && (flags & CMD_IGNORE_RETURN))
    tc->flags |= CMD_IGNORE_RETURN;

   
  if (tc && (flags & CMD_NO_FORK) && (subshell_environment & SUBSHELL_COMSUB))
    optimize_shell_function (tc);

  gs = sh_getopt_save_istate ();
  if (subshell == 0)
    {
      begin_unwind_frame ("function_calling");
       
      push_context (var->name, subshell, temporary_env);
       
      add_unwind_protect (maybe_restore_getopt_state, gs);
      add_unwind_protect (pop_context, (char *)NULL);
      unwind_protect_int (line_number);
      unwind_protect_int (line_number_for_err_trap);
      unwind_protect_int (function_line_number);
      unwind_protect_int (return_catch_flag);
      unwind_protect_jmp_buf (return_catch);
      add_unwind_protect (dispose_command, (char *)tc);
      unwind_protect_pointer (this_shell_function);
      unwind_protect_int (funcnest);
      unwind_protect_int (loop_level);
    }
  else
    push_context (var->name, subshell, temporary_env);	 

  temporary_env = (HASH_TABLE *)NULL;

  this_shell_function = var;
  make_funcname_visible (1);

  debug_trap = TRAP_STRING(DEBUG_TRAP);
  error_trap = TRAP_STRING(ERROR_TRAP);
  return_trap = TRAP_STRING(RETURN_TRAP);
  
   

   
  if (debug_trap && ((trace_p (var) == 0) && function_trace_mode == 0))
    {
      if (subshell == 0)
	{
	  debug_trap = savestring (debug_trap);
	  add_unwind_protect (xfree, debug_trap);
	  add_unwind_protect (maybe_set_debug_trap, debug_trap);
	}
      restore_default_signal (DEBUG_TRAP);
    }

   
  if (error_trap && error_trace_mode == 0)
    {
      if (subshell == 0)
	{
	  error_trap = savestring (error_trap);
	  add_unwind_protect (xfree, error_trap);
	  add_unwind_protect (maybe_set_error_trap, error_trap);
	}
      restore_default_signal (ERROR_TRAP);
    }

   
  if (return_trap && (signal_in_progress (DEBUG_TRAP) || ((trace_p (var) == 0) && function_trace_mode == 0)))
    {
      if (subshell == 0)
	{
	  return_trap = savestring (return_trap);
	  add_unwind_protect (xfree, return_trap);
	  add_unwind_protect (maybe_set_return_trap, return_trap);
	}
      restore_default_signal (RETURN_TRAP);
    }
  
  funcnest++;
#if defined (ARRAY_VARS)
   
  shell_fn = find_function_def (this_shell_function->name);
  sfile = shell_fn ? shell_fn->source_file : "";
  array_push ((ARRAY *)funcname_a, this_shell_function->name);

  array_push ((ARRAY *)bash_source_a, sfile);
  lineno = GET_LINE_NUMBER ();
  t = itos (lineno);
  array_push ((ARRAY *)bash_lineno_a, t);
  free (t);
#endif

#if defined (ARRAY_VARS)
  fa = (struct func_array_state *)xmalloc (sizeof (struct func_array_state));
  fa->source_a = (ARRAY *)bash_source_a;
  fa->source_v = bash_source_v;
  fa->lineno_a = (ARRAY *)bash_lineno_a;
  fa->lineno_v = bash_lineno_v;
  fa->funcname_a = (ARRAY *)funcname_a;
  fa->funcname_v = funcname_v;
  if (subshell == 0)
    add_unwind_protect (restore_funcarray_state, fa);
#endif

   

   
  if (debugging_mode || shell_compatibility_level <= 44)
    init_bash_argv ();

  remember_args (words->next, 1);

   
  if (debugging_mode)
    {
      push_args (words->next);
      if (subshell == 0)
	add_unwind_protect (pop_args, 0);
    }

   
  line_number = function_line_number = tc->line;

#if defined (JOB_CONTROL)
  if (subshell)
    stop_pipeline (async, (COMMAND *)NULL);
#endif

  if (shell_compatibility_level > 43)
    loop_level = 0;

  fc = tc;

  from_return_trap = 0;

  return_catch_flag++;
  return_val = setjmp_nosigs (return_catch);

  if (return_val)
    {
      result = return_catch_value;
       
      save_current = currently_executing_command;
      if (from_return_trap == 0)
	run_return_trap ();
      currently_executing_command = save_current;
    }
  else
    {
       
      showing_function_line = 1;
      save_current = currently_executing_command;
      result = run_debug_trap ();
#if defined (DEBUGGER)
       
      if (debugging_mode == 0 || result == EXECUTION_SUCCESS)
	{
	  showing_function_line = 0;
	  currently_executing_command = save_current;
	  result = execute_command_internal (fc, 0, NO_PIPE, NO_PIPE, fds_to_close);

	   
	  save_current = currently_executing_command;
	  run_return_trap ();
	  currently_executing_command = save_current;
	}
#else
      result = execute_command_internal (fc, 0, NO_PIPE, NO_PIPE, fds_to_close);

      save_current = currently_executing_command;
      run_return_trap ();
      currently_executing_command = save_current;
#endif
      showing_function_line = 0;
    }

   
  gv = find_variable ("OPTIND");
  if (gv && gv->context == variable_context)
    gs->gs_flags |= 1;

  if (subshell == 0)
    run_unwind_frame ("function_calling");
#if defined (ARRAY_VARS)
  else
    {
      restore_funcarray_state (fa);
       
      if (debugging_mode)
	pop_args ();
    }
#endif

  if (variable_context == 0 || this_shell_function == 0)
    {
      make_funcname_visible (0);
#if defined (PROCESS_SUBSTITUTION)
      unlink_fifo_list ();
#endif
    }

  return (result);
}

 
int
execute_shell_function (var, words)
     SHELL_VAR *var;
     WORD_LIST *words;
{
  int ret;
  struct fd_bitmap *bitmap;

  bitmap = new_fd_bitmap (FD_BITMAP_DEFAULT_SIZE);
  begin_unwind_frame ("execute-shell-function");
  add_unwind_protect (dispose_fd_bitmap, (char *)bitmap);
      
  ret = execute_function (var, words, 0, bitmap, 0, 0);

  dispose_fd_bitmap (bitmap);
  discard_unwind_frame ("execute-shell-function");

  return ret;
}

 
static void
execute_subshell_builtin_or_function (words, redirects, builtin, var,
				      pipe_in, pipe_out, async, fds_to_close,
				      flags)
     WORD_LIST *words;
     REDIRECT *redirects;
     sh_builtin_func_t *builtin;
     SHELL_VAR *var;
     int pipe_in, pipe_out, async;
     struct fd_bitmap *fds_to_close;
     int flags;
{
  int result, r, funcvalue;
#if defined (JOB_CONTROL)
  int jobs_hack;

  jobs_hack = (builtin == jobs_builtin) &&
		((subshell_environment & SUBSHELL_ASYNC) == 0 || pipe_out != NO_PIPE);
#endif

   
  login_shell = interactive = 0;
  if (builtin == eval_builtin)
    evalnest = 0;
  else if (builtin == source_builtin)
    sourcenest = 0;

  if (async)
    subshell_environment |= SUBSHELL_ASYNC;
  if (pipe_in != NO_PIPE || pipe_out != NO_PIPE)
    subshell_environment |= SUBSHELL_PIPE;

  maybe_make_export_env ();	 

#if defined (JOB_CONTROL)
   

   
  if (jobs_hack)
    kill_current_pipeline ();
  else
    without_job_control ();

  set_sigchld_handler ();
#else
  without_job_control ();
#endif  

  set_sigint_handler ();

  if (fds_to_close)
    close_fd_bitmap (fds_to_close);

  do_piping (pipe_in, pipe_out);

  if (do_redirections (redirects, RX_ACTIVE) != 0)
    exit (EXECUTION_FAILURE);

  if (builtin)
    {
       
      result = setjmp_nosigs (top_level);

       
      funcvalue = 0;
      if (return_catch_flag && builtin == return_builtin)
        funcvalue = setjmp_nosigs (return_catch);

      if (result == EXITPROG || result == EXITBLTIN)
	subshell_exit (last_command_exit_value);
      else if (result)
	subshell_exit (EXECUTION_FAILURE);
      else if (funcvalue)
	subshell_exit (return_catch_value);
      else
	{
	  r = execute_builtin (builtin, words, flags, 1);
	  fflush (stdout);
	  if (r == EX_USAGE)
	    r = EX_BADUSAGE;
	   
	  else if (r == EX_DISKFALLBACK)
	    {
	      char *command_line;

	      command_line = savestring (the_printed_command_except_trap ? the_printed_command_except_trap : "");
	      r = execute_disk_command (words, (REDIRECT *)0, command_line,
		  -1, -1, async, (struct fd_bitmap *)0, flags|CMD_NO_FORK);
	    }
	  subshell_exit (r);
	}
    }
  else
    {
      r = execute_function (var, words, flags, fds_to_close, async, 1);
      fflush (stdout);
      subshell_exit (r);
    }
}

 
static int
execute_builtin_or_function (words, builtin, var, redirects,
			     fds_to_close, flags)
     WORD_LIST *words;
     sh_builtin_func_t *builtin;
     SHELL_VAR *var;
     REDIRECT *redirects;
     struct fd_bitmap *fds_to_close;
     int flags;
{
  int result;
  REDIRECT *saved_undo_list;
#if defined (PROCESS_SUBSTITUTION)
  int ofifo, nfifo, osize;
  void *ofifo_list;
#endif

#if defined (PROCESS_SUBSTITUTION)
  begin_unwind_frame ("saved_fifos");
   
  ofifo = num_fifos ();
  ofifo_list = copy_fifo_list (&osize);
  if (ofifo_list)
    add_unwind_protect (xfree, ofifo_list);
#endif

  if (do_redirections (redirects, RX_ACTIVE|RX_UNDOABLE) != 0)
    {
      undo_partial_redirects ();
      dispose_exec_redirects ();
#if defined (PROCESS_SUBSTITUTION)
      free (ofifo_list);
#endif
      return (EX_REDIRFAIL);	 
    }

  saved_undo_list = redirection_undo_list;

   
  if (builtin == exec_builtin)
    {
      dispose_redirects (saved_undo_list);
      saved_undo_list = exec_redirection_undo_list;
      exec_redirection_undo_list = (REDIRECT *)NULL;
    }
  else
    dispose_exec_redirects ();

  if (saved_undo_list)
    {
      begin_unwind_frame ("saved-redirects");
      add_unwind_protect (cleanup_redirects, (char *)saved_undo_list);
    }

  redirection_undo_list = (REDIRECT *)NULL;

  if (builtin)
    result = execute_builtin (builtin, words, flags, 0);
  else
    result = execute_function (var, words, flags, fds_to_close, 0, 0);

   
  fflush (stdout);
  fpurge (stdout);
  if (ferror (stdout))
    clearerr (stdout);  

   
  if (builtin == command_builtin && this_shell_builtin == exec_builtin)
    {
      int discard;

      discard = 0;
      if (saved_undo_list)
	{
	  dispose_redirects (saved_undo_list);
	  discard = 1;
	}
      redirection_undo_list = exec_redirection_undo_list;
      saved_undo_list = exec_redirection_undo_list = (REDIRECT *)NULL;      
      if (discard)
	discard_unwind_frame ("saved-redirects");
    }

  if (saved_undo_list)
    {
      redirection_undo_list = saved_undo_list;
      discard_unwind_frame ("saved-redirects");
    }

  undo_partial_redirects ();

#if defined (PROCESS_SUBSTITUTION)
   
  nfifo = num_fifos ();
  if (nfifo > ofifo)
    close_new_fifos (ofifo_list, osize);
  if (ofifo_list)
    free (ofifo_list);
  discard_unwind_frame ("saved_fifos");
#endif

  return (result);
}

void
setup_async_signals ()
{
#if defined (__BEOS__)
  set_signal_handler (SIGHUP, SIG_IGN);	 
#endif

#if defined (JOB_CONTROL)
  if (job_control == 0)
#endif
    {
       
      get_original_signal (SIGINT);
      set_signal_handler (SIGINT, SIG_IGN);

      get_original_signal (SIGQUIT);
      set_signal_handler (SIGQUIT, SIG_IGN);
    }
}

 

 
#ifndef NOTFOUND_HOOK
#  define NOTFOUND_HOOK "command_not_found_handle"
#endif

static int
execute_disk_command (words, redirects, command_line, pipe_in, pipe_out,
		      async, fds_to_close, cmdflags)
     WORD_LIST *words;
     REDIRECT *redirects;
     char *command_line;
     int pipe_in, pipe_out, async;
     struct fd_bitmap *fds_to_close;
     int cmdflags;
{
  char *pathname, *command, **args, *p;
  int nofork, stdpath, result, fork_flags;
  pid_t pid;
  SHELL_VAR *hookf;
  WORD_LIST *wl;

  stdpath = (cmdflags & CMD_STDPATH);	 
  nofork = (cmdflags & CMD_NO_FORK);	 
  pathname = words->word->word;

  p = 0;
  result = EXECUTION_SUCCESS;
#if defined (RESTRICTED_SHELL)
  command = (char *)NULL;
  if (restricted && mbschr (pathname, '/'))
    {
      internal_error (_("%s: restricted: cannot specify `/' in command names"),
		    pathname);
      result = last_command_exit_value = EXECUTION_FAILURE;

       
      if (nofork && pipe_in == NO_PIPE && pipe_out == NO_PIPE)
	exit (last_command_exit_value);
      else
	goto parent_return;
    }
#endif  

   
  command = search_for_command (pathname, CMDSRCH_HASH|(stdpath ? CMDSRCH_STDPATH : 0));
  QUIT;

  if (command)
    {
       
      if (nofork && pipe_in == NO_PIPE && pipe_out == NO_PIPE && (subshell_environment & SUBSHELL_PIPE) == 0)
	adjust_shell_level (-1);

      maybe_make_export_env ();
      put_command_name_into_env (command);
    }

   
   
  if (nofork && pipe_in == NO_PIPE && pipe_out == NO_PIPE)
    pid = 0;
  else
    {
      fork_flags = async ? FORK_ASYNC : 0;
      pid = make_child (p = savestring (command_line), fork_flags);
    }

  if (pid == 0)
    {
      int old_interactive;

      reset_terminating_signals ();	 
       
      restore_original_signals ();
      subshell_environment &= ~SUBSHELL_IGNTRAP;

#if defined (JOB_CONTROL)
      FREE (p);
#endif

       
      if (async)
	{
	  if ((cmdflags & CMD_STDIN_REDIR) &&
		pipe_in == NO_PIPE &&
		(stdin_redirects (redirects) == 0))
	    async_redirect_stdin ();
	  setup_async_signals ();
	}

       
      if (fds_to_close)
	close_fd_bitmap (fds_to_close);

      do_piping (pipe_in, pipe_out);

      old_interactive = interactive;
      if (async)
	interactive = 0;

      subshell_environment |= SUBSHELL_FORK;	 

#if defined (PROCESS_SUBSTITUTION) && !defined (HAVE_DEV_FD)
      clear_fifo_list ();	 
#endif

       
      if (redirects && (do_redirections (redirects, RX_ACTIVE) != 0))
	{
#if defined (PROCESS_SUBSTITUTION)
	   
	  unlink_all_fifos ();
#endif  
	  exit (EXECUTION_FAILURE);
	}

#if defined (PROCESS_SUBSTITUTION) && !defined (HAVE_DEV_FD)
       
      unlink_all_fifos ();
#endif

      if (async)
	interactive = old_interactive;

      if (command == 0)
	{
	  hookf = find_function (NOTFOUND_HOOK);
	  if (hookf == 0)
	    {
	       
	      pathname = printable_filename (pathname, 0);
	      internal_error (_("%s: command not found"), pathname);
	      exit (EX_NOTFOUND);	 
	    }

	   
	  without_job_control ();

#if defined (JOB_CONTROL)
	  set_sigchld_handler ();
#endif

	  wl = make_word_list (make_word (NOTFOUND_HOOK), words);
	  exit (execute_shell_function (hookf, wl));
	}

       
      args = strvec_from_word_list (words, 0, 0, (int *)NULL);
      exit (shell_execve (command, args, export_env));
    }
  else
    {
parent_return:
      QUIT;

       
      close_pipes (pipe_in, pipe_out);
#if defined (PROCESS_SUBSTITUTION) && defined (HAVE_DEV_FD)
#if 0
      if (variable_context == 0)
        unlink_fifo_list ();
#endif
#endif
      FREE (command);
      return (result);
    }
}

 

#if !defined (MSDOS)
#  define STRINGCHAR(ind) \
    (ind < sample_len && !whitespace (sample[ind]) && sample[ind] != '\n')
#  define WHITECHAR(ind) \
    (ind < sample_len && whitespace (sample[ind]))
#else	 
#  define STRINGCHAR(ind) \
    (ind < sample_len && !whitespace (sample[ind]) && sample[ind] != '\n' && sample[ind] != '\r')
#  define WHITECHAR(ind) \
    (ind < sample_len && whitespace (sample[ind]))
#endif	 

static char *
getinterp (sample, sample_len, endp)
     char *sample;
     int sample_len, *endp;
{
  register int i;
  char *execname;
  int start;

   
  for (i = 2; i < sample_len && whitespace (sample[i]); i++)
    ;

  for (start = i; STRINGCHAR(i); i++)
    ;

  execname = substring (sample, start, i);

  if (endp)
    *endp = i;
  return execname;
}

#if !defined (HAVE_HASH_BANG_EXEC)
 

static int
execute_shell_script (sample, sample_len, command, args, env)
     char *sample;
     int sample_len;
     char *command;
     char **args, **env;
{
  char *execname, *firstarg;
  int i, start, size_increment, larry;

   
  execname = getinterp (sample, sample_len, &i);
  size_increment = 1;

   
  for (firstarg = (char *)NULL, start = i; WHITECHAR(i); i++)
    ;

   

  if (STRINGCHAR(i))  
    {
      for (start = i; STRINGCHAR(i); i++)
	;
      firstarg = substring ((char *)sample, start, i);
      size_increment = 2;
    }

  larry = strvec_len (args) + size_increment;
  args = strvec_resize (args, larry + 1);

  for (i = larry - 1; i; i--)
    args[i] = args[i - size_increment];

  args[0] = execname;
  if (firstarg)
    {
      args[1] = firstarg;
      args[2] = command;
    }
  else
    args[1] = command;

  args[larry] = (char *)NULL;

  return (shell_execve (execname, args, env));
}
#undef STRINGCHAR
#undef WHITECHAR

#endif  

static void
initialize_subshell ()
{
#if defined (ALIAS)
   
  delete_all_aliases ();
#endif  

#if defined (HISTORY)
   
  history_lines_this_session = 0;
#endif

   
  without_job_control ();

#if defined (JOB_CONTROL)
  set_sigchld_handler ();
  init_job_stats ();
#endif  

   
  reset_shell_flags ();
  reset_shell_options ();
  reset_shopt_options ();

   
  if (vc_isbltnenv (shell_variables))
    shell_variables = shell_variables->down;

  clear_unwind_protect_list (0);
   
  parse_and_execute_level = 0;		 

   
  variable_context = return_catch_flag = funcnest = evalnest = sourcenest = 0;

  executing_list = 0;		 

   
  if (interactive_shell == 0)
    unset_bash_input (0);
}

#if defined (HAVE_SETOSTYPE) && defined (_POSIX_SOURCE)
#  define SETOSTYPE(x)	__setostype(x)
#else
#  define SETOSTYPE(x)
#endif

#define HASH_BANG_BUFSIZ	128

#define READ_SAMPLE_BUF(file, buf, len) \
  do \
    { \
      fd = open(file, O_RDONLY); \
      if (fd >= 0) \
	{ \
	  len = read (fd, buf, HASH_BANG_BUFSIZ); \
	  close (fd); \
	} \
      else \
	len = -1; \
    } \
  while (0)
      
 
int
shell_execve (command, args, env)
     char *command;
     char **args, **env;
{
  int larray, i, fd;
  char sample[HASH_BANG_BUFSIZ];
  int sample_len;

  SETOSTYPE (0);		 
  execve (command, args, env);
  i = errno;			 
  CHECK_TERMSIG;
  SETOSTYPE (1);

   
  if (i != ENOEXEC)
    {
       
      last_command_exit_value = (i == ENOENT) ?  EX_NOTFOUND : EX_NOEXEC;  
      if (file_isdir (command))
#if defined (EISDIR)
	internal_error (_("%s: %s"), command, strerror (EISDIR));
#else
	internal_error (_("%s: is a directory"), command);
#endif
      else if (executable_file (command) == 0)
	{
	  errno = i;
	  file_error (command);
	}
       
      else if (i == E2BIG || i == ENOMEM)
	{
	  errno = i;
	  file_error (command);
	}
      else if (i == ENOENT)
	{
	  errno = i;
	  internal_error (_("%s: cannot execute: required file not found"), command);
	}
      else
	{
	   
#if defined (HAVE_HASH_BANG_EXEC)
	  READ_SAMPLE_BUF (command, sample, sample_len);
	  if (sample_len > 0)
	    sample[sample_len - 1] = '\0';
	  if (sample_len > 2 && sample[0] == '#' && sample[1] == '!')
	    {
	      char *interp;
	      int ilen;

	      interp = getinterp (sample, sample_len, (int *)NULL);
	      ilen = strlen (interp);
	      errno = i;
	      if (interp[ilen - 1] == '\r')
		{
		  interp = xrealloc (interp, ilen + 2);
		  interp[ilen - 1] = '^';
		  interp[ilen] = 'M';
		  interp[ilen + 1] = '\0';
		}
	      sys_error (_("%s: %s: bad interpreter"), command, interp ? interp : "");
	      FREE (interp);
	      return (EX_NOEXEC);
	    }
#endif
	  errno = i;
	  file_error (command);
	}
      return (last_command_exit_value);
    }

   
  READ_SAMPLE_BUF (command, sample, sample_len);

  if (sample_len == 0)
    return (EXECUTION_SUCCESS);

   
  if (sample_len > 0)
    {
#if !defined (HAVE_HASH_BANG_EXEC)
      if (sample_len > 2 && sample[0] == '#' && sample[1] == '!')
	return (execute_shell_script (sample, sample_len, command, args, env));
      else
#endif
      if (check_binary_file (sample, sample_len))
	{
	  internal_error (_("%s: cannot execute binary file: %s"), command, strerror (i));
	  errno = i;
	  return (EX_BINARY_FILE);
	}
    }

   

  reset_parser ();
  initialize_subshell ();

  set_sigint_handler ();

   
  larray = strvec_len (args) + 1;
  args = strvec_resize (args, larray + 1);

  for (i = larray - 1; i; i--)
    args[i] = args[i - 1];

  args[0] = shell_name;
  args[1] = command;
  args[larray] = (char *)NULL;

  if (args[0][0] == '-')
    args[0]++;

#if defined (RESTRICTED_SHELL)
  if (restricted)
    change_flag ('r', FLAG_OFF);
#endif

  if (subshell_argv)
    {
       
      for (i = 1; i < subshell_argc; i++)
	free (subshell_argv[i]);
      free (subshell_argv);
    }

  dispose_command (currently_executing_command);	 
  currently_executing_command = (COMMAND *)NULL;

  subshell_argc = larray;
  subshell_argv = args;
  subshell_envp = env;

  unbind_args ();	 

#if defined (PROCESS_SUBSTITUTION) && defined (HAVE_DEV_FD)
  clear_fifo_list ();	 
#endif

  sh_longjmp (subshell_top_level, 1);
   
}

static int
execute_intern_function (name, funcdef)
     WORD_DESC *name;
     FUNCTION_DEF *funcdef;
{
  SHELL_VAR *var;
  char *t;

  if (check_identifier (name, posixly_correct) == 0)
    {
      if (posixly_correct && interactive_shell == 0)
	{
	  last_command_exit_value = EX_BADUSAGE;
	  jump_to_top_level (ERREXIT);
	}
      return (EXECUTION_FAILURE);
    }

  if (strchr (name->word, CTLESC))	 
    {
      t = dequote_escapes (name->word);
      free (name->word);
      name->word = t;
    }

   
  if (posixly_correct && find_special_builtin (name->word))
    {
      internal_error (_("`%s': is a special builtin"), name->word);
      last_command_exit_value = EX_BADUSAGE;
      jump_to_top_level (interactive_shell ? DISCARD : ERREXIT);
    }

  var = find_function (name->word);
  if (var && (readonly_p (var) || noassign_p (var)))
    {
      if (readonly_p (var))
	internal_error (_("%s: readonly function"), var->name);
      return (EXECUTION_FAILURE);
    }

#if defined (DEBUGGER)
  bind_function_def (name->word, funcdef, 1);
#endif

  bind_function (name->word, funcdef->command);
  return (EXECUTION_SUCCESS);
}

#if defined (INCLUDE_UNUSED)
#if defined (PROCESS_SUBSTITUTION)
void
close_all_files ()
{
  register int i, fd_table_size;

  fd_table_size = getdtablesize ();
  if (fd_table_size > 256)	 
    fd_table_size = 256;

  for (i = 3; i < fd_table_size; i++)
    close (i);
}
#endif  
#endif

static void
close_pipes (in, out)
     int in, out;
{
  if (in >= 0)
    close (in);
  if (out >= 0)
    close (out);
}

static void
dup_error (oldd, newd)
     int oldd, newd;
{
  sys_error (_("cannot duplicate fd %d to fd %d"), oldd, newd);
}

 
static void
do_piping (pipe_in, pipe_out)
     int pipe_in, pipe_out;
{
  if (pipe_in != NO_PIPE)
    {
      if (dup2 (pipe_in, 0) < 0)
	dup_error (pipe_in, 0);
      if (pipe_in > 0)
	close (pipe_in);
#ifdef __CYGWIN__
       
      freopen (NULL, "r", stdin);
#endif  
    }
  if (pipe_out != NO_PIPE)
    {
      if (pipe_out != REDIRECT_BOTH)
	{
	  if (dup2 (pipe_out, 1) < 0)
	    dup_error (pipe_out, 1);
	  if (pipe_out == 0 || pipe_out > 1)
	    close (pipe_out);
	}
      else
	{
	  if (dup2 (1, 2) < 0)
	    dup_error (1, 2);
	}
#ifdef __CYGWIN__
       
      freopen (NULL, "w", stdout);
      sh_setlinebuf (stdout);
#endif  
    }
}
