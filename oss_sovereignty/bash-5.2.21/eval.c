 

 

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "bashansi.h"
#include <stdio.h>

#include <signal.h>

#include "bashintl.h"

#include "shell.h"
#include "parser.h"
#include "flags.h"
#include "trap.h"

#include "builtins/common.h"

#include "input.h"
#include "execute_cmd.h"

#if defined (HISTORY)
#  include "bashhist.h"
#endif

static void send_pwd_to_eterm PARAMS((void));
static sighandler alrm_catcher PARAMS((int));

 
int
reader_loop ()
{
  int our_indirection_level;
  COMMAND * volatile current_command;

  USE_VAR(current_command);

  current_command = (COMMAND *)NULL;

  our_indirection_level = ++indirection_level;

  if (just_one_command)
    reset_readahead_token ();

  while (EOF_Reached == 0)
    {
      int code;

      code = setjmp_nosigs (top_level);

#if defined (PROCESS_SUBSTITUTION)
      unlink_fifo_list ();
#endif  

       
      if (interactive_shell && signal_is_ignored (SIGINT) == 0 && signal_is_trapped (SIGINT) == 0)
	set_signal_handler (SIGINT, sigint_sighandler);

      if (code != NOT_JUMPED)
	{
	  indirection_level = our_indirection_level;

	  switch (code)
	    {
	       
	    case ERREXIT:
	      if (exit_immediately_on_error)
		reset_local_contexts ();	 
	    case FORCE_EOF:
	    case EXITPROG:
	    case EXITBLTIN:
	      current_command = (COMMAND *)NULL;
	      EOF_Reached = EOF;
	      goto exec_done;

	    case DISCARD:
	       
	      if (last_command_exit_value == 0)
		set_exit_status (EXECUTION_FAILURE);
	      if (subshell_environment)
		{
		  current_command = (COMMAND *)NULL;
		  EOF_Reached = EOF;
		  goto exec_done;
		}
	       
	      if (current_command)
		{
		  dispose_command (current_command);
		  current_command = (COMMAND *)NULL;
		}

	      restore_sigmask ();
	      break;

	    default:
	      command_error ("reader_loop", CMDERR_BADJUMP, code, 0);
	    }
	}

      executing = 0;
      if (temporary_env)
	dispose_used_env_vars ();

#if (defined (ultrix) && defined (mips)) || defined (C_ALLOCA)
       
      (void) alloca (0);
#endif

      if (read_command () == 0)
	{
	  if (interactive_shell == 0 && read_but_dont_execute)
	    {
	      set_exit_status (last_command_exit_value);
	      dispose_command (global_command);
	      global_command = (COMMAND *)NULL;
	    }
	  else if (current_command = global_command)
	    {
	      global_command = (COMMAND *)NULL;

	       
	      if (interactive && ps0_prompt)
		{
		  char *ps0_string;

		  ps0_string = decode_prompt_string (ps0_prompt);
		  if (ps0_string && *ps0_string)
		    {
		      fprintf (stderr, "%s", ps0_string);
		      fflush (stderr);
		    }
		  free (ps0_string);
		}

	      current_command_number++;

	      executing = 1;
	      stdin_redir = 0;

	      execute_command (current_command);

	    exec_done:
	      QUIT;

	      if (current_command)
		{
		  dispose_command (current_command);
		  current_command = (COMMAND *)NULL;
		}
	    }
	}
      else
	{
	   
	  if (interactive == 0)
	    EOF_Reached = EOF;
	}
      if (just_one_command)
	EOF_Reached = EOF;
    }
  indirection_level--;
  return (last_command_exit_value);
}

 
int
pretty_print_loop ()
{
  COMMAND *current_command;
  char *command_to_print;
  int code;
  int global_posix_mode, last_was_newline;

  global_posix_mode = posixly_correct;
  last_was_newline = 0;
  while (EOF_Reached == 0)
    {
      code = setjmp_nosigs (top_level);
      if (code)
        return (EXECUTION_FAILURE);
      if (read_command() == 0)
	{
	  current_command = global_command;
	  global_command = 0;
	  posixly_correct = 1;			 
	  if (current_command && (command_to_print = make_command_string (current_command)))
	    {
	      printf ("%s\n", command_to_print);	 
	      last_was_newline = 0;
	    }
	  else if (last_was_newline == 0)
	    {
	       printf ("\n");
	       last_was_newline = 1;
	    }
	  posixly_correct = global_posix_mode;
	  dispose_command (current_command);
	}
      else
	return (EXECUTION_FAILURE);
    }
    
  return (EXECUTION_SUCCESS);
}

static sighandler
alrm_catcher(i)
     int i;
{
  char *msg;

  msg = _("\007timed out waiting for input: auto-logout\n");
  write (1, msg, strlen (msg));

  bash_logout ();	 
  jump_to_top_level (EXITPROG);
  SIGRETURN (0);
}

 
static void
send_pwd_to_eterm ()
{
  char *pwd, *f;

  f = 0;
  pwd = get_string_value ("PWD");
  if (pwd == 0)
    f = pwd = get_working_directory ("eterm");
  fprintf (stderr, "\032/%s\n", pwd);
  free (f);
}

#if defined (ARRAY_VARS)
 
int
execute_array_command (a, v)
     ARRAY *a;
     void *v;
{
  char *tag;
  char **argv;
  int argc, i;

  tag = (char *)v;
  argc = 0;
  argv = array_to_argv (a, &argc);
  for (i = 0; i < argc; i++)
    {
      if (argv[i] && argv[i][0])
	execute_variable_command (argv[i], tag);
    }
  strvec_dispose (argv);
  return 0;
}
#endif
  
static void
execute_prompt_command ()
{
  char *command_to_execute;
  SHELL_VAR *pcv;
#if defined (ARRAY_VARS)
  ARRAY *pcmds;
#endif

  pcv = find_variable ("PROMPT_COMMAND");
  if (pcv  == 0 || var_isset (pcv) == 0 || invisible_p (pcv))
    return;
#if defined (ARRAY_VARS)
  if (array_p (pcv))
    {
      if ((pcmds = array_cell (pcv)) && array_num_elements (pcmds) > 0)
	execute_array_command (pcmds, "PROMPT_COMMAND");
      return;
    }
  else if (assoc_p (pcv))
    return;	 
#endif

  command_to_execute = value_cell (pcv);
  if (command_to_execute && *command_to_execute)
    execute_variable_command (command_to_execute, "PROMPT_COMMAND");
}

 
int
parse_command ()
{
  int r;

  need_here_doc = 0;
  run_pending_traps ();

   
   
  if (interactive && bash_input.type != st_string && parser_expanding_alias() == 0)
    {
#if defined (READLINE)
      if (no_line_editing || (bash_input.type == st_stdin && parser_will_prompt ()))
#endif
        execute_prompt_command ();

      if (running_under_emacs == 2)
	send_pwd_to_eterm ();	 
    }

  current_command_line_count = 0;
  r = yyparse ();

  if (need_here_doc)
    gather_here_documents ();

  return (r);
}

 
int
read_command ()
{
  SHELL_VAR *tmout_var;
  int tmout_len, result;
  SigHandler *old_alrm;

  set_current_prompt_level (1);
  global_command = (COMMAND *)NULL;

   
  tmout_var = (SHELL_VAR *)NULL;
  tmout_len = 0;
  old_alrm = (SigHandler *)NULL;

  if (interactive)
    {
      tmout_var = find_variable ("TMOUT");

      if (tmout_var && var_isset (tmout_var))
	{
	  tmout_len = atoi (value_cell (tmout_var));
	  if (tmout_len > 0)
	    {
	      old_alrm = set_signal_handler (SIGALRM, alrm_catcher);
	      alarm (tmout_len);
	    }
	}
    }

  QUIT;

  current_command_line_count = 0;
  result = parse_command ();

  if (interactive && tmout_var && (tmout_len > 0))
    {
      alarm(0);
      set_signal_handler (SIGALRM, old_alrm);
    }

  return (result);
}
