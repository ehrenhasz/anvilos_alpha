 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include <signal.h>

#include <errno.h>

#include "filecntl.h"
#include "../bashansi.h"

#include "../shell.h"
#include "../jobs.h"
#include "../builtins.h"
#include "../flags.h"
#include "../parser.h"
#include "../input.h"
#include "../execute_cmd.h"
#include "../redir.h"
#include "../trap.h"
#include "../bashintl.h"

#include <y.tab.h>

#if defined (HISTORY)
#  include "../bashhist.h"
#endif

#include "common.h"
#include "builtext.h"

#if !defined (errno)
extern int errno;
#endif

#define IS_BUILTIN(s)	(builtin_address_internal(s, 0) != (struct builtin *)NULL)

int parse_and_execute_level = 0;

static int cat_file PARAMS((REDIRECT *));

#define PE_TAG "parse_and_execute top"
#define PS_TAG "parse_string top"

#if defined (HISTORY)
static void
set_history_remembering ()
{
  remember_on_history = enable_history_list;
}
#endif

static void
restore_lastcom (x)
     char *x;
{
  FREE (the_printed_command_except_trap);
  the_printed_command_except_trap = x;
}

int
should_optimize_fork (command, subshell)
     COMMAND *command;
     int subshell;
{
  return (running_trap == 0 &&
      command->type == cm_simple &&
      signal_is_trapped (EXIT_TRAP) == 0 &&
      signal_is_trapped (ERROR_TRAP) == 0 &&
      any_signals_trapped () < 0 &&
      (subshell || (command->redirects == 0 && command->value.Simple->redirects == 0)) &&
      ((command->flags & CMD_TIME_PIPELINE) == 0) &&
      ((command->flags & CMD_INVERT_RETURN) == 0));
}

 
int
should_suppress_fork (command)
     COMMAND *command;
{
  int subshell;

  subshell = subshell_environment & SUBSHELL_PROCSUB;	 
  return (startup_state == 2 && parse_and_execute_level == 1 &&
	  *bash_input.location.string == '\0' &&
	  parser_expanding_alias () == 0 &&
	  should_optimize_fork (command, subshell));
}

int
can_optimize_connection (command)
     COMMAND *command;
{
  return (*bash_input.location.string == '\0' &&
	  parser_expanding_alias () == 0 &&
	  (command->value.Connection->connector == AND_AND || command->value.Connection->connector == OR_OR || command->value.Connection->connector == ';') &&
	  command->value.Connection->second->type == cm_simple);
}

void
optimize_connection_fork (command)
     COMMAND *command;
{
  if (command->type == cm_connection &&
      (command->value.Connection->connector == AND_AND || command->value.Connection->connector == OR_OR || command->value.Connection->connector == ';') &&
      (command->value.Connection->second->flags & CMD_TRY_OPTIMIZING) &&
      (should_suppress_fork (command->value.Connection->second) ||
      ((subshell_environment & SUBSHELL_PAREN) && should_optimize_fork (command->value.Connection->second, 0))))
    {
      command->value.Connection->second->flags |= CMD_NO_FORK;
      command->value.Connection->second->value.Simple->flags |= CMD_NO_FORK;
    }
}

void
optimize_subshell_command (command)
     COMMAND *command;
{
  if (should_optimize_fork (command, 0))
    {
      command->flags |= CMD_NO_FORK;
      command->value.Simple->flags |= CMD_NO_FORK;
    }
  else if (command->type == cm_connection &&
	   (command->value.Connection->connector == AND_AND || command->value.Connection->connector == OR_OR || command->value.Connection->connector == ';') &&
	   command->value.Connection->second->type == cm_simple &&
	   parser_expanding_alias () == 0)
    {	   
      command->value.Connection->second->flags |= CMD_TRY_OPTIMIZING;
      command->value.Connection->second->value.Simple->flags |= CMD_TRY_OPTIMIZING;
    }
}

void
optimize_shell_function (command)
     COMMAND *command;
{
  COMMAND *fc;

  fc = (command->type == cm_group) ? command->value.Group->command : command;

  if (fc->type == cm_simple && should_suppress_fork (fc))
    {
      fc->flags |= CMD_NO_FORK;
      fc->value.Simple->flags |= CMD_NO_FORK;
    }
  else if (fc->type == cm_connection && can_optimize_connection (fc) && should_suppress_fork (fc->value.Connection->second))
    {
      fc->value.Connection->second->flags |= CMD_NO_FORK;
      fc->value.Connection->second->value.Simple->flags |= CMD_NO_FORK;
    }  
}

int
can_optimize_cat_file (command)
     COMMAND *command;
{
  return (command->type == cm_simple && !command->redirects &&
	    (command->flags & CMD_TIME_PIPELINE) == 0 &&
	    command->value.Simple->words == 0 &&
	    command->value.Simple->redirects &&
	    command->value.Simple->redirects->next == 0 &&
	    command->value.Simple->redirects->instruction == r_input_direction &&
	    command->value.Simple->redirects->redirector.dest == 0);
}

 
void
parse_and_execute_cleanup (old_running_trap)
     int old_running_trap;
{
  if (running_trap > 0)
    {
       
      if (running_trap != old_running_trap)
	run_trap_cleanup (running_trap - 1);
      unfreeze_jobs_list ();
    }

  if (have_unwind_protects ())
     run_unwind_frame (PE_TAG);
  else
    parse_and_execute_level = 0;			 
}

static void
parse_prologue (string, flags, tag)
     char *string;
     int flags;
     char *tag;
{
  char *orig_string, *lastcom;
  int x;

  orig_string = string;
   
  begin_unwind_frame (tag);
  unwind_protect_int (parse_and_execute_level);
  unwind_protect_jmp_buf (top_level);
  unwind_protect_int (indirection_level);
  unwind_protect_int (line_number);
  unwind_protect_int (line_number_for_err_trap);
  unwind_protect_int (loop_level);
  unwind_protect_int (executing_list);
  unwind_protect_int (comsub_ignore_return);
  if (flags & (SEVAL_NONINT|SEVAL_INTERACT))
    unwind_protect_int (interactive);

#if defined (HISTORY)
  if (parse_and_execute_level == 0)
    add_unwind_protect (set_history_remembering, (char *)NULL);
  else
    unwind_protect_int (remember_on_history);	 
#  if defined (BANG_HISTORY)
  unwind_protect_int (history_expansion_inhibited);
#  endif  
#endif  

  if (interactive_shell)
    {
      x = get_current_prompt_level ();
      add_unwind_protect (set_current_prompt_level, x);
    }

  if (the_printed_command_except_trap)
    {
      lastcom = savestring (the_printed_command_except_trap);
      add_unwind_protect (restore_lastcom, lastcom);
    }

  add_unwind_protect (pop_stream, (char *)NULL);
  if (parser_expanding_alias ())
    add_unwind_protect (parser_restore_alias, (char *)NULL);

  if (orig_string && ((flags & SEVAL_NOFREE) == 0))
    add_unwind_protect (xfree, orig_string);
  end_unwind_frame ();

  if (flags & (SEVAL_NONINT|SEVAL_INTERACT))
    interactive = (flags & SEVAL_NONINT) ? 0 : 1;

#if defined (HISTORY)
  if (flags & SEVAL_NOHIST)
    bash_history_disable ();
#  if defined (BANG_HISTORY)
  if (flags & SEVAL_NOHISTEXP)
    history_expansion_inhibited = 1;
#  endif  
#endif  
}

 

int
parse_and_execute (string, from_file, flags)
     char *string;
     const char *from_file;
     int flags;
{
  int code, lreset;
  volatile int should_jump_to_top_level, last_result;
  COMMAND *volatile command;
  volatile sigset_t pe_sigmask;

  parse_prologue (string, flags, PE_TAG);

  parse_and_execute_level++;

  lreset = flags & SEVAL_RESETLINE;

#if defined (HAVE_POSIX_SIGNALS)
   
  sigemptyset ((sigset_t *)&pe_sigmask);
  sigprocmask (SIG_BLOCK, (sigset_t *)NULL, (sigset_t *)&pe_sigmask);
#endif

   
  push_stream (lreset);
  if (parser_expanding_alias ())
     
    parser_save_alias ();
  
  if (lreset == 0)
    line_number--;
    
  indirection_level++;

  code = should_jump_to_top_level = 0;
  last_result = EXECUTION_SUCCESS;

   
  if (current_token == yacc_EOF)
    current_token = '\n';		 

  with_input_from_string (string, from_file);
  clear_shell_input_line ();
  while (*(bash_input.location.string) || parser_expanding_alias ())
    {
      command = (COMMAND *)NULL;

      if (interrupt_state)
	{
	  last_result = EXECUTION_FAILURE;
	  break;
	}

       
      code = setjmp_nosigs (top_level);

      if (code)
	{
	  should_jump_to_top_level = 0;
	  switch (code)
	    {
	    case ERREXIT:
	       
	      if (exit_immediately_on_error && variable_context)
	        {
	          discard_unwind_frame ("pe_dispose");
	          reset_local_contexts ();  
	        }
	      should_jump_to_top_level = 1;
	      goto out;
	    case FORCE_EOF:	      
	    case EXITPROG:
	      if (command)
		run_unwind_frame ("pe_dispose");
	       
	      should_jump_to_top_level = 1;
	      goto out;

	    case EXITBLTIN:
	      if (command)
		{
		  if (variable_context && signal_is_trapped (0))
		    {
		       
		      dispose_command (command);
		      discard_unwind_frame ("pe_dispose");
		    }
		  else
		    run_unwind_frame ("pe_dispose");
		}
	      should_jump_to_top_level = 1;
	      goto out;

	    case DISCARD:
	      if (command)
		run_unwind_frame ("pe_dispose");
	      last_result = last_command_exit_value = EXECUTION_FAILURE;  
	      set_pipestatus_from_exit (last_command_exit_value);
	      if (subshell_environment)
		{
		  should_jump_to_top_level = 1;
		  goto out;
		}
	      else
		{
#if 0
		  dispose_command (command);	 
#endif
#if defined (HAVE_POSIX_SIGNALS)
		  sigprocmask (SIG_SETMASK, (sigset_t *)&pe_sigmask, (sigset_t *)NULL);
#endif
		  continue;
		}

	    default:
	      command_error ("parse_and_execute", CMDERR_BADJUMP, code, 0);
	      break;
	    }
	}

      if (parse_command () == 0)
	{
	  int local_expalias, local_alflag;

	  if ((flags & SEVAL_PARSEONLY) || (interactive_shell == 0 && read_but_dont_execute))
	    {
	      last_result = EXECUTION_SUCCESS;
	      dispose_command (global_command);
	      global_command = (COMMAND *)NULL;
	    }
	  else if (command = global_command)
	    {
	      struct fd_bitmap *bitmap;

	      if (flags & SEVAL_FUNCDEF)
		{
		  char *x;

		   
		  if (command->type != cm_function_def ||
		      ((x = parser_remaining_input ()) && *x) ||
		      (STREQ (from_file, command->value.Function_def->name->word) == 0))
		    {
		      internal_warning (_("%s: ignoring function definition attempt"), from_file);
		      should_jump_to_top_level = 0;
		      last_result = last_command_exit_value = EX_BADUSAGE;
		      set_pipestatus_from_exit (last_command_exit_value);
		      reset_parser ();
		      break;
		    }
		}

	      bitmap = new_fd_bitmap (FD_BITMAP_SIZE);
	      begin_unwind_frame ("pe_dispose");
	      add_unwind_protect (dispose_fd_bitmap, bitmap);
	      add_unwind_protect (dispose_command, command);	 

	      global_command = (COMMAND *)NULL;

	      if ((subshell_environment & SUBSHELL_COMSUB) && comsub_ignore_return)
		command->flags |= CMD_IGNORE_RETURN;

#if defined (ONESHOT)
	       
	      if (should_suppress_fork (command))
		{
		  command->flags |= CMD_NO_FORK;
		  command->value.Simple->flags |= CMD_NO_FORK;
		}

	       
	      else if (command->type == cm_connection &&
		       (flags & SEVAL_NOOPTIMIZE) == 0 &&
		       can_optimize_connection (command))
		{
		  command->value.Connection->second->flags |= CMD_TRY_OPTIMIZING;
		  command->value.Connection->second->value.Simple->flags |= CMD_TRY_OPTIMIZING;
		}
#endif  

	       
	      local_expalias = expand_aliases;
	      local_alflag = expaliases_flag;
	      if (subshell_environment & SUBSHELL_COMSUB)
		expand_aliases = expaliases_flag;

	       
	      if (startup_state == 2 &&
		  (subshell_environment & SUBSHELL_COMSUB) &&
		  *bash_input.location.string == '\0' &&
		  can_optimize_cat_file (command))
		{
		  int r;
		  r = cat_file (command->value.Simple->redirects);
		  last_result = (r < 0) ? EXECUTION_FAILURE : EXECUTION_SUCCESS;
		}
	      else
		last_result = execute_command_internal
				(command, 0, NO_PIPE, NO_PIPE, bitmap);
	      dispose_command (command);
	      dispose_fd_bitmap (bitmap);
	      discard_unwind_frame ("pe_dispose");

	       
	      if ((subshell_environment & SUBSHELL_COMSUB) && local_alflag == expaliases_flag)
		expand_aliases = local_expalias;

	      if (flags & SEVAL_ONECMD)
		{
		  reset_parser ();
		  break;
		}
	    }
	}
      else
	{
	  last_result = EX_BADUSAGE;	 

	  if (interactive_shell == 0 && this_shell_builtin &&
	      (this_shell_builtin == source_builtin || this_shell_builtin == eval_builtin) &&
	      last_command_exit_value == EX_BADSYNTAX && posixly_correct && executing_command_builtin == 0)
	    {
	      should_jump_to_top_level = 1;
	      code = ERREXIT;
	      last_command_exit_value = EX_BADUSAGE;
	    }

	   
	  break;
	}
    }

 out:

  run_unwind_frame (PE_TAG);

  if (interrupt_state && parse_and_execute_level == 0)
    {
       
      interactive = interactive_shell;
      throw_to_top_level ();
    }

  CHECK_TERMSIG;

  if (should_jump_to_top_level)
    jump_to_top_level (code);

  return (last_result);
}

 
int
parse_string (string, from_file, flags, cmdp, endp)
     char *string;
     const char *from_file;
     int flags;
     COMMAND **cmdp;
     char **endp;
{
  int code, nc;
  volatile int should_jump_to_top_level;
  COMMAND *volatile command, *oglobal;
  char *ostring;
  volatile sigset_t ps_sigmask;

  parse_prologue (string, flags, PS_TAG);

#if defined (HAVE_POSIX_SIGNALS)
   
  sigemptyset ((sigset_t *)&ps_sigmask);
  sigprocmask (SIG_BLOCK, (sigset_t *)NULL, (sigset_t *)&ps_sigmask);
#endif

   
  push_stream (0);
  if (parser_expanding_alias ())
     
    parser_save_alias ();

  code = should_jump_to_top_level = 0;
  oglobal = global_command;

  with_input_from_string (string, from_file);
  ostring = bash_input.location.string;
  while (*(bash_input.location.string))		 
    {
      command = (COMMAND *)NULL;

#if 0
      if (interrupt_state)
	break;
#endif

       
      code = setjmp_nosigs (top_level);

      if (code)
	{
	  INTERNAL_DEBUG(("parse_string: longjmp executed: code = %d", code));

	  should_jump_to_top_level = 0;
	  switch (code)
	    {
	    case FORCE_EOF:
	    case ERREXIT:
	    case EXITPROG:
	    case EXITBLTIN:
	    case DISCARD:		 
	      if (command)
		dispose_command (command);
	       
	      should_jump_to_top_level = 1;
	      goto out;

	    default:
#if defined (HAVE_POSIX_SIGNALS)
	      sigprocmask (SIG_SETMASK, (sigset_t *)&ps_sigmask, (sigset_t *)NULL);
#endif
	      command_error ("parse_string", CMDERR_BADJUMP, code, 0);
	      break;
	    }
	}
	  
      if (parse_command () == 0)
	{
	  if (cmdp)
	    *cmdp = global_command;
	  else
	    dispose_command (global_command);
	  global_command = (COMMAND *)NULL;
	}
      else
	{
	  if ((flags & SEVAL_NOLONGJMP) == 0)
	    {
	      should_jump_to_top_level = 1;
	      code = DISCARD;
	    }
	  else
	    reset_parser ();	 
	  break;
	}

      if (current_token == yacc_EOF || current_token == shell_eof_token)
	{
	  if (current_token == shell_eof_token)
	    rewind_input_string ();
	  break;
	}
    }

out:

  global_command = oglobal;
  nc = bash_input.location.string - ostring;
  if (endp)
    *endp = bash_input.location.string;

  run_unwind_frame (PS_TAG);

   
  if (should_jump_to_top_level)
    {
      if (parse_and_execute_level == 0)
	top_level_cleanup ();
      if (code == DISCARD)
	return -DISCARD;
      jump_to_top_level (code);
    }

  return (nc);
}

int
open_redir_file (r, fnp)
     REDIRECT *r;
     char **fnp;
{
  char *fn;
  int fd, rval;

  if (r->instruction != r_input_direction)
    return -1;

   
  if (posixly_correct && !interactive_shell)
    disallow_filename_globbing++;
  fn = redirection_expand (r->redirectee.filename);
  if (posixly_correct && !interactive_shell)
    disallow_filename_globbing--;

  if (fn == 0)
    {
      redirection_error (r, AMBIGUOUS_REDIRECT, fn);
      return -1;
    }

  fd = open(fn, O_RDONLY);
  if (fd < 0)
    {
      file_error (fn);
      free (fn);
      if (fnp)
	*fnp = 0;
      return -1;
    }

  if (fnp)
    *fnp = fn;
  return fd;
}

 
static int
cat_file (r)
     REDIRECT *r;
{
  char *fn;
  int fd, rval;

  fd = open_redir_file (r, &fn);
  if (fd < 0)
    return -1;

  rval = zcatfd (fd, 1, fn);

  free (fn);
  close (fd);

  return (rval);
}

int
evalstring (string, from_file, flags)
     char *string;
     const char *from_file;
     int flags;
{
  volatile int r, rflag, rcatch;
  volatile int was_trap;

   
  was_trap = running_trap;

  rcatch = 0;
  rflag = return_catch_flag;
   
  if (rflag)
    {
      begin_unwind_frame ("evalstring");

      unwind_protect_int (return_catch_flag);
      unwind_protect_jmp_buf (return_catch);

      return_catch_flag++;	 
      rcatch = setjmp_nosigs (return_catch);
    }

  if (rcatch)
    {
       
      parse_and_execute_cleanup (was_trap);
      r = return_catch_value;
    }
  else
     
    r = parse_and_execute (string, from_file, flags);

  if (rflag)
    {
      run_unwind_frame ("evalstring");
      if (rcatch && return_catch_flag)
	{
	  return_catch_value = r;
	  sh_longjmp (return_catch, 1);
	}
    }
    
  return (r);
}
