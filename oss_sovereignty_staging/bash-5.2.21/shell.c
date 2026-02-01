 

 

 
#define INSTALL_DEBUG_MODE

#include "config.h"

#include "bashtypes.h"
#if !defined (_MINIX) && defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif
#include "posixstat.h"
#include "posixtime.h"
#include "bashansi.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include "filecntl.h"
#if defined (HAVE_PWD_H)
#  include <pwd.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashintl.h"

#define NEED_SH_SETLINEBUF_DECL		 

#include "shell.h"
#include "parser.h"
#include "flags.h"
#include "trap.h"
#include "mailcheck.h"
#include "builtins.h"
#include "builtins/common.h"

#if defined (JOB_CONTROL)
#include "jobs.h"
#else
extern int running_in_background;
extern int initialize_job_control PARAMS((int));
extern int get_tty_state PARAMS((void));
#endif  

#include "input.h"
#include "execute_cmd.h"
#include "findcmd.h"

#if defined (USING_BASH_MALLOC) && defined (DEBUG) && !defined (DISABLE_MALLOC_WRAPPERS)
#  include <malloc/shmalloc.h>
#elif defined (MALLOC_DEBUG) && defined (USING_BASH_MALLOC)
#  include <malloc/shmalloc.h>
#endif

#if defined (HISTORY)
#  include "bashhist.h"
#  include <readline/history.h>
#endif

#if defined (READLINE)
#  include <readline/readline.h>
#  include "bashline.h"
#endif

#include <tilde/tilde.h>
#include <glob/strmatch.h>

#if defined (__OPENNT)
#  include <opennt/opennt.h>
#endif

#if !defined (HAVE_GETPW_DECLS)
extern struct passwd *getpwuid ();
#endif  

#if !defined (errno)
extern int errno;
#endif

#if defined (NO_MAIN_ENV_ARG)
extern char **environ;	 
#endif

extern int gnu_error_format;

 
int shell_initialized = 0;
int bash_argv_initialized = 0;

COMMAND *global_command = (COMMAND *)NULL;

 
struct user_info current_user =
{
  (uid_t)-1, (uid_t)-1, (gid_t)-1, (gid_t)-1,
  (char *)NULL, (char *)NULL, (char *)NULL
};

 
char *current_host_name = (char *)NULL;

 
int login_shell = 0;

 
int interactive = 0;

 
int interactive_shell = 0;

 
int hup_on_exit = 0;

 
int check_jobs_at_exit = 0;

 
int autocd = 0;

 
int startup_state = 0;
int reading_shell_script = 0;

 
int debugging_login_shell = 0;

 
char **shell_environment;

 
int executing = 0;

 
int current_command_number = 1;

 
int indirection_level = 0;

 
char *shell_name = (char *)NULL;

 
time_t shell_start_time;
struct timeval shellstart;

 
int running_under_emacs;

 
#ifdef HAVE_DEV_FD
int have_devfd = HAVE_DEV_FD;
#else
int have_devfd = 0;
#endif

 
static char *bashrc_file = DEFAULT_BASHRC;

 
static int act_like_sh;

 
static int su_shell;

 
static int sourced_env;

 
static int running_setuid;

 
static int debugging;			 
static int no_rc;			 
static int no_profile;			 
static int do_version;			 
static int make_login_shell;		 
static int want_initial_help;		 

int debugging_mode = 0;		 
#if defined (READLINE)
int no_line_editing = 0;	 
#else
int no_line_editing = 1;	 
#endif
#if defined (TRANSLATABLE_STRINGS)
int dump_translatable_strings;	 
int dump_po_strings;		 
#endif
int wordexp_only = 0;		 
int protected_mode = 0;		 

int pretty_print_mode = 0;	 

#if defined (STRICT_POSIX)
int posixly_correct = 1;	 
#else
int posixly_correct = 0;	 
#endif

 
#define Int 1
#define Charp 2
static const struct {
  const char *name;
  int type;
  int *int_value;
  char **char_value;
} long_args[] = {
  { "debug", Int, &debugging, (char **)0x0 },
#if defined (DEBUGGER)
  { "debugger", Int, &debugging_mode, (char **)0x0 },
#endif
#if defined (TRANSLATABLE_STRINGS)
  { "dump-po-strings", Int, &dump_po_strings, (char **)0x0 },
  { "dump-strings", Int, &dump_translatable_strings, (char **)0x0 },
#endif
  { "help", Int, &want_initial_help, (char **)0x0 },
  { "init-file", Charp, (int *)0x0, &bashrc_file },
  { "login", Int, &make_login_shell, (char **)0x0 },
  { "noediting", Int, &no_line_editing, (char **)0x0 },
  { "noprofile", Int, &no_profile, (char **)0x0 },
  { "norc", Int, &no_rc, (char **)0x0 },
  { "posix", Int, &posixly_correct, (char **)0x0 },
  { "pretty-print", Int, &pretty_print_mode, (char **)0x0 },
#if defined (WORDEXP_OPTION)
  { "protected", Int, &protected_mode, (char **)0x0 },
#endif
  { "rcfile", Charp, (int *)0x0, &bashrc_file },
#if defined (RESTRICTED_SHELL)
  { "restricted", Int, &restricted, (char **)0x0 },
#endif
  { "verbose", Int, &verbose_flag, (char **)0x0 },
  { "version", Int, &do_version, (char **)0x0 },
#if defined (WORDEXP_OPTION)
  { "wordexp", Int, &wordexp_only, (char **)0x0 },
#endif
  { (char *)0x0, Int, (int *)0x0, (char **)0x0 }
};

 
procenv_t subshell_top_level;
int subshell_argc;
char **subshell_argv;
char **subshell_envp;

char *exec_argv0;

#if defined (BUFFERED_INPUT)
 
int default_buffered_input = -1;
#endif

 
int read_from_stdin;		 
int want_pending_command;	 

 
char *command_execution_string;	 
char *shell_script_filename; 	 

int malloc_trace_at_exit = 0;

static int shell_reinitialized = 0;

static FILE *default_input;

static STRING_INT_ALIST *shopt_alist;
static int shopt_ind = 0, shopt_len = 0;

static int parse_long_options PARAMS((char **, int, int));
static int parse_shell_options PARAMS((char **, int, int));
static int bind_args PARAMS((char **, int, int, int));

static void start_debugger PARAMS((void));

static void add_shopt_to_alist PARAMS((char *, int));
static void run_shopt_alist PARAMS((void));

static void execute_env_file PARAMS((char *));
static void run_startup_files PARAMS((void));
static int open_shell_script PARAMS((char *));
static void set_bash_input PARAMS((void));
static int run_one_command PARAMS((char *));
#if defined (WORDEXP_OPTION)
static int run_wordexp PARAMS((char *));
#endif

static int uidget PARAMS((void));

static void set_option_defaults PARAMS((void));
static void reset_option_defaults PARAMS((void));

static void init_interactive PARAMS((void));
static void init_noninteractive PARAMS((void));
static void init_interactive_script PARAMS((void));

static void set_shell_name PARAMS((char *));
static void shell_initialize PARAMS((void));
static void shell_reinitialize PARAMS((void));

static void show_shell_usage PARAMS((FILE *, int));

#ifdef __CYGWIN__
static void
_cygwin32_check_tmp ()
{
  struct stat sb;

  if (stat ("/tmp", &sb) < 0)
    internal_warning (_("could not find /tmp, please create!"));
  else
    {
      if (S_ISDIR (sb.st_mode) == 0)
	internal_warning (_("/tmp must be a valid directory name"));
    }
}
#endif  

#if defined (NO_MAIN_ENV_ARG)
 
int
main (argc, argv)
     int argc;
     char **argv;
#else  
int
main (argc, argv, env)
     int argc;
     char **argv, **env;
#endif  
{
  register int i;
  int code, old_errexit_flag;
#if defined (RESTRICTED_SHELL)
  int saverst;
#endif
  volatile int locally_skip_execution;
  volatile int arg_index, top_level_arg_index;
#ifdef __OPENNT
  char **env;

  env = environ;
#endif  

  USE_VAR(argc);
  USE_VAR(argv);
  USE_VAR(env);
  USE_VAR(code);
  USE_VAR(old_errexit_flag);
#if defined (RESTRICTED_SHELL)
  USE_VAR(saverst);
#endif

   
  code = setjmp_nosigs (top_level);
  if (code)
    exit (2);

  xtrace_init ();

#if defined (USING_BASH_MALLOC) && defined (DEBUG) && !defined (DISABLE_MALLOC_WRAPPERS)
  malloc_set_register (1);	 
#endif

  check_dev_tty ();

#ifdef __CYGWIN__
  _cygwin32_check_tmp ();
#endif  

   
  while (debugging_login_shell) sleep (3);

  set_default_locale ();

  running_setuid = uidget ();

  if (getenv ("POSIXLY_CORRECT") || getenv ("POSIX_PEDANTIC"))
    posixly_correct = 1;

#if defined (USE_GNU_MALLOC_LIBRARY)
  mcheck (programming_error, (void (*) ())0);
#endif  

  if (setjmp_sigs (subshell_top_level))
    {
      argc = subshell_argc;
      argv = subshell_argv;
      env = subshell_envp;
      sourced_env = 0;
    }

  shell_reinitialized = 0;

   
  arg_index = 1;
  if (arg_index > argc)
    arg_index = argc;
  command_execution_string = shell_script_filename = (char *)NULL;
  want_pending_command = locally_skip_execution = read_from_stdin = 0;
  default_input = stdin;
#if defined (BUFFERED_INPUT)
  default_buffered_input = -1;
#endif

   
  login_shell = make_login_shell = 0;

   
  if (shell_initialized || shell_name)
    {
       
      if (*shell_name == '-')
	shell_name++;

      shell_reinitialize ();
      if (setjmp_nosigs (top_level))
	exit (2);
    }

  shell_environment = env;
  set_shell_name (argv[0]);

  gettimeofday (&shellstart, 0);
  shell_start_time = shellstart.tv_sec;

   

   
  arg_index = parse_long_options (argv, arg_index, argc);
  
  if (want_initial_help)
    {
      show_shell_usage (stdout, 1);
      exit (EXECUTION_SUCCESS);
    }

  if (do_version)
    {
      show_shell_version (1);
      exit (EXECUTION_SUCCESS);
    }

  echo_input_at_read = verbose_flag;	 

   
  this_command_name = shell_name;	 
  arg_index = parse_shell_options (argv, arg_index, argc);

   
  if (make_login_shell)
    {
      login_shell++;
      login_shell = -login_shell;
    }

  set_login_shell ("login_shell", login_shell != 0);

#if defined (TRANSLATABLE_STRINGS)
  if (dump_po_strings)
    dump_translatable_strings = 1;

  if (dump_translatable_strings)
    read_but_dont_execute = 1;
#endif

  if (running_setuid && privileged_mode == 0)
    disable_priv_mode ();

   
  if (want_pending_command)
    {
      command_execution_string = argv[arg_index];
      if (command_execution_string == 0)
	{
	  report_error (_("%s: option requires an argument"), "-c");
	  exit (EX_BADUSAGE);
	}
      arg_index++;
    }
  this_command_name = (char *)NULL;

   

  if (forced_interactive ||		 
      (!command_execution_string &&	 
       wordexp_only == 0 &&		 
       ((arg_index == argc) ||		 
	read_from_stdin) &&		 
       isatty (fileno (stdin)) &&	 
       isatty (fileno (stderr))))	 
    init_interactive ();
  else
    init_noninteractive ();

   
  if (login_shell && interactive_shell)
    {
      for (i = 3; i < 20; i++)
	SET_CLOSE_ON_EXEC (i);
    }

   
  if (posixly_correct)
    {
      bind_variable ("POSIXLY_CORRECT", "y", 0);
      sv_strict_posix ("POSIXLY_CORRECT");
    }

   
  if (shopt_alist)
    run_shopt_alist ();

   
  shell_initialize ();

  set_default_lang ();
  set_default_locale_vars ();

   
  if (interactive_shell)
    {
      char *term, *emacs, *inside_emacs;
      int emacs_term, in_emacs;

      term = get_string_value ("TERM");
      emacs = get_string_value ("EMACS");
      inside_emacs = get_string_value ("INSIDE_EMACS");

      if (inside_emacs)
	{
	  emacs_term = strstr (inside_emacs, ",term:") != 0;
	  in_emacs = 1;
	}
      else if (emacs)
	{
	   
	  emacs_term = strstr (emacs, " (term:") != 0;
	  in_emacs = emacs_term || STREQ (emacs, "t");
	}
      else
	in_emacs = emacs_term = 0;

       
      no_line_editing |= STREQ (term, "emacs");
      no_line_editing |= in_emacs && STREQ (term, "dumb");

       
      running_under_emacs = in_emacs || STREQN (term, "emacs", 5);
      running_under_emacs += emacs_term && STREQN (term, "eterm", 5);

      if (running_under_emacs)
	gnu_error_format = 1;
    }

  top_level_arg_index = arg_index;
  old_errexit_flag = exit_immediately_on_error;

   
  code = setjmp_sigs (top_level);
  if (code)
    {
      if (code == EXITPROG || code == ERREXIT || code == EXITBLTIN)
	exit_shell (last_command_exit_value);
      else
	{
#if defined (JOB_CONTROL)
	   
	  set_job_control (interactive_shell);
#endif
	   
	  exit_immediately_on_error += old_errexit_flag;
	  locally_skip_execution++;
	}
    }

  arg_index = top_level_arg_index;

   

  if (interactive_shell == 0)
    {
      unbind_variable ("PS1");
      unbind_variable ("PS2");
      interactive = 0;
#if 0
       
      expand_aliases = posixly_correct;
#endif
    }
  else
    {
      change_flag ('i', FLAG_ON);
      interactive = 1;
    }

#if defined (RESTRICTED_SHELL)
   
  restricted_shell = shell_is_restricted (shell_name);

   
  saverst = restricted;
  restricted = 0;
#endif

   
  if (wordexp_only)
    ;			 
  else if (command_execution_string)
    arg_index = bind_args (argv, arg_index, argc, 0);	 
  else if (arg_index != argc && read_from_stdin == 0)
    {
      shell_script_filename = argv[arg_index++];
      arg_index = bind_args (argv, arg_index, argc, 1);	 
    }
  else
    arg_index = bind_args (argv, arg_index, argc, 1);	 

   
  if (locally_skip_execution == 0 && running_setuid == 0)
    {
      char *t;

      old_errexit_flag = exit_immediately_on_error;
      exit_immediately_on_error = 0;

       
      if (shell_script_filename)
	{
	  t = dollar_vars[0];
	  dollar_vars[0] = exec_argv0 ? savestring (exec_argv0) : savestring (shell_script_filename);
	}
      run_startup_files ();
      if (shell_script_filename)
	{
	  free (dollar_vars[0]);
	  dollar_vars[0] = t;
	}
      exit_immediately_on_error += old_errexit_flag;
    }

   
  if (act_like_sh)
    {
      bind_variable ("POSIXLY_CORRECT", "y", 0);
      sv_strict_posix ("POSIXLY_CORRECT");
    }

#if defined (RESTRICTED_SHELL)
   
  restricted = saverst || restricted;
  if (shell_reinitialized == 0)
    maybe_make_restricted (shell_name);
#endif  

#if defined (WORDEXP_OPTION)
  if (wordexp_only)
    {
      startup_state = 3;
      last_command_exit_value = run_wordexp (argv[top_level_arg_index]);
      exit_shell (last_command_exit_value);
    }
#endif

  cmd_init ();		 
  uwp_init ();

  if (command_execution_string)
    {
      startup_state = 2;

      if (debugging_mode)
	start_debugger ();

#if defined (ONESHOT)
      executing = 1;
      run_one_command (command_execution_string);
      exit_shell (last_command_exit_value);
#else  
      with_input_from_string (command_execution_string, "-c");
      goto read_and_execute;
#endif  
    }

   
  if (shell_script_filename)
    open_shell_script (shell_script_filename);
  else if (interactive == 0)
    {
       
#if defined (BUFFERED_INPUT)
      default_buffered_input = fileno (stdin);	 
#else
      setbuf (default_input, (char *)NULL);
#endif  
      read_from_stdin = 1;
    }
  else if (top_level_arg_index == argc)		 
     
    read_from_stdin = 1;

  set_bash_input ();

  if (debugging_mode && locally_skip_execution == 0 && running_setuid == 0 && (reading_shell_script || interactive_shell == 0))
    start_debugger ();

   
  if (interactive_shell)
    {
       
      reset_mail_timer ();
      init_mail_dates ();

#if defined (HISTORY)
       
      bash_initialize_history ();
       
      if (shell_initialized == 0 && history_lines_this_session == 0)
	load_history ();
#endif  

       
      get_tty_state ();
    }

#if !defined (ONESHOT)
 read_and_execute:
#endif  

  shell_initialized = 1;

  if (pretty_print_mode && interactive_shell)
    {
      internal_warning (_("pretty-printing mode ignored in interactive shells"));
      pretty_print_mode = 0;
    }
  if (pretty_print_mode)
    exit_shell (pretty_print_loop ());

   
  reader_loop ();
  exit_shell (last_command_exit_value);
}

static int
parse_long_options (argv, arg_start, arg_end)
     char **argv;
     int arg_start, arg_end;
{
  int arg_index, longarg, i;
  char *arg_string;

  arg_index = arg_start;
  while ((arg_index != arg_end) && (arg_string = argv[arg_index]) &&
	 (*arg_string == '-'))
    {
      longarg = 0;

       
      if (arg_string[1] == '-' && arg_string[2])
	{
	  longarg = 1;
	  arg_string++;
	}

      for (i = 0; long_args[i].name; i++)
	{
	  if (STREQ (arg_string + 1, long_args[i].name))
	    {
	      if (long_args[i].type == Int)
		*long_args[i].int_value = 1;
	      else if (argv[++arg_index] == 0)
		{
		  report_error (_("%s: option requires an argument"), long_args[i].name);
		  exit (EX_BADUSAGE);
		}
	      else
		*long_args[i].char_value = argv[arg_index];

	      break;
	    }
	}
      if (long_args[i].name == 0)
	{
	  if (longarg)
	    {
	      report_error (_("%s: invalid option"), argv[arg_index]);
	      show_shell_usage (stderr, 0);
	      exit (EX_BADUSAGE);
	    }
	  break;		 
	}

      arg_index++;
    }

  return (arg_index);
}

static int
parse_shell_options (argv, arg_start, arg_end)
     char **argv;
     int arg_start, arg_end;
{
  int arg_index;
  int arg_character, on_or_off, next_arg, i;
  char *o_option, *arg_string;

  arg_index = arg_start;
  while (arg_index != arg_end && (arg_string = argv[arg_index]) &&
	 (*arg_string == '-' || *arg_string == '+'))
    {
       
      next_arg = arg_index + 1;

       
      if (arg_string[0] == '-' &&
	   (arg_string[1] == '\0' ||
	     (arg_string[1] == '-' && arg_string[2] == '\0')))
	return (next_arg);

      i = 1;
      on_or_off = arg_string[0];
      while (arg_character = arg_string[i++])
	{
	  switch (arg_character)
	    {
	    case 'c':
	      want_pending_command = 1;
	      break;

	    case 'l':
	      make_login_shell = 1;
	      break;

	    case 's':
	      read_from_stdin = 1;
	      break;

	    case 'o':
	      o_option = argv[next_arg];
	      if (o_option == 0)
		{
		  set_option_defaults ();
		  list_minus_o_opts (-1, (on_or_off == '-') ? 0 : 1);
		  reset_option_defaults ();
		  break;
		}
	      if (set_minus_o_option (on_or_off, o_option) != EXECUTION_SUCCESS)
		exit (EX_BADUSAGE);
	      next_arg++;
	      break;

	    case 'O':
	       
	      o_option = argv[next_arg];
	      if (o_option == 0)
		{
		  shopt_listopt (o_option, (on_or_off == '-') ? 0 : 1);
		  break;
		}
	      add_shopt_to_alist (o_option, on_or_off);
	      next_arg++;
	      break;

	    case 'D':
#if defined (TRANSLATABLE_STRINGS)
	      dump_translatable_strings = 1;
#endif
	      break;

	    default:
	      if (change_flag (arg_character, on_or_off) == FLAG_ERROR)
		{
		  report_error (_("%c%c: invalid option"), on_or_off, arg_character);
		  show_shell_usage (stderr, 0);
		  exit (EX_BADUSAGE);
		}
	    }
	}
       
      arg_index = next_arg;
    }

  return (arg_index);
}

 
void
exit_shell (s)
     int s;
{
  fflush (stdout);		 
  fflush (stderr);

   
#if defined (READLINE)
  if (RL_ISSTATE (RL_STATE_TERMPREPPED) && rl_deprep_term_function)
    (*rl_deprep_term_function) ();
#endif
  if (read_tty_modified ())
    read_tty_cleanup ();

   
  if (signal_is_trapped (0))
    s = run_exit_trap ();

#if defined (PROCESS_SUBSTITUTION)
  unlink_all_fifos ();
#endif  

#if defined (HISTORY)
  if (remember_on_history)
    maybe_save_shell_history ();
#endif  

#if defined (COPROCESS_SUPPORT)
  coproc_flush ();
#endif

#if defined (JOB_CONTROL)
   
  if (interactive_shell && login_shell && hup_on_exit)
    hangup_all_jobs ();

   
  if (subshell_environment == 0)
    end_job_control ();
#endif  

   
  sh_exit (s);
}

 
void
sh_exit (s)
     int s;
{
#if defined (MALLOC_DEBUG) && defined (USING_BASH_MALLOC)
  if (malloc_trace_at_exit && (subshell_environment & (SUBSHELL_COMSUB|SUBSHELL_PROCSUB)) == 0)
    trace_malloc_stats (get_name_for_error (), (char *)NULL);
   
#endif

  exit (s);
}

 
void
subshell_exit (s)
     int s;
{
  fflush (stdout);
  fflush (stderr);

   
  last_command_exit_value = s;
  if (signal_is_trapped (0))
    s = run_exit_trap ();

  sh_exit (s);
}

void
set_exit_status (s)
     int s;
{
  set_pipestatus_from_exit (last_command_exit_value = s);
}

 

 

static void
execute_env_file (env_file)
      char *env_file;
{
  char *fn;

  if (env_file && *env_file)
    {
      fn = expand_string_unsplit_to_string (env_file, Q_DOUBLE_QUOTES);
      if (fn && *fn)
	maybe_execute_file (fn, 1);
      FREE (fn);
    }
}

static void
run_startup_files ()
{
#if defined (JOB_CONTROL)
  int old_job_control;
#endif
  int sourced_login, run_by_ssh;

#if 1	 
   
  if (interactive_shell == 0 && no_rc == 0 && login_shell == 0 &&
      act_like_sh == 0 && command_execution_string)
    {
#ifdef SSH_SOURCE_BASHRC
      run_by_ssh = (find_variable ("SSH_CLIENT") != (SHELL_VAR *)0) ||
		   (find_variable ("SSH2_CLIENT") != (SHELL_VAR *)0);
#else
      run_by_ssh = 0;
#endif
#endif

       
#if 1	 
      if ((run_by_ssh || isnetconn (fileno (stdin))) && shell_level < 2)
#else
      if (isnetconn (fileno (stdin) && shell_level < 2)
#endif
	{
#ifdef SYS_BASHRC
#  if defined (__OPENNT)
	  maybe_execute_file (_prefixInstallPath(SYS_BASHRC, NULL, 0), 1);
#  else
	  maybe_execute_file (SYS_BASHRC, 1);
#  endif
#endif
	  maybe_execute_file (bashrc_file, 1);
	  return;
	}
    }

#if defined (JOB_CONTROL)
   
  old_job_control = interactive_shell ? set_job_control (0) : 0;
#endif

  sourced_login = 0;

   
#if defined (NON_INTERACTIVE_LOGIN_SHELLS)
  if (login_shell && posixly_correct == 0)
#else
  if (login_shell < 0 && posixly_correct == 0)
#endif
    {
       
      no_rc++;

       
      if (no_profile == 0)
	{
	  maybe_execute_file (SYS_PROFILE, 1);

	  if (act_like_sh)	 
	    maybe_execute_file ("~/.profile", 1);
	  else if ((maybe_execute_file ("~/.bash_profile", 1) == 0) &&
		   (maybe_execute_file ("~/.bash_login", 1) == 0))	 
	    maybe_execute_file ("~/.profile", 1);
	}

      sourced_login = 1;
    }

   
  if (interactive_shell == 0 && !(su_shell && login_shell))
    {
      if (posixly_correct == 0 && act_like_sh == 0 && privileged_mode == 0 &&
	    sourced_env++ == 0)
	execute_env_file (get_string_value ("BASH_ENV"));
      return;
    }

   
  if (posixly_correct == 0)		   
    {
      if (login_shell && sourced_login++ == 0)
	{
	   
	  no_rc++;

	   
	  if (no_profile == 0)
	    {
	      maybe_execute_file (SYS_PROFILE, 1);

	      if (act_like_sh)	 
		maybe_execute_file ("~/.profile", 1);
	      else if ((maybe_execute_file ("~/.bash_profile", 1) == 0) &&
		       (maybe_execute_file ("~/.bash_login", 1) == 0))	 
		maybe_execute_file ("~/.profile", 1);
	    }
	}

       
      if (act_like_sh == 0 && no_rc == 0)
	{
#ifdef SYS_BASHRC
#  if defined (__OPENNT)
	  maybe_execute_file (_prefixInstallPath(SYS_BASHRC, NULL, 0), 1);
#  else
	  maybe_execute_file (SYS_BASHRC, 1);
#  endif
#endif
	  maybe_execute_file (bashrc_file, 1);
	}
       
      else if (act_like_sh && privileged_mode == 0 && sourced_env++ == 0)
	execute_env_file (get_string_value ("ENV"));
    }
  else		 
    {
       
      if (interactive_shell && privileged_mode == 0 && sourced_env++ == 0)
	execute_env_file (get_string_value ("ENV"));
    }

#if defined (JOB_CONTROL)
  set_job_control (old_job_control);
#endif
}

#if defined (RESTRICTED_SHELL)
 
int
shell_is_restricted (name)
     char *name;
{
  char *temp;

  if (restricted)
    return 1;
  temp = base_pathname (name);
  if (*temp == '-')
    temp++;
  return (STREQ (temp, RESTRICTED_SHELL_NAME));
}

 
int
maybe_make_restricted (name)
     char *name;
{
  char *temp;

  temp = base_pathname (name);
  if (*temp == '-')
    temp++;
  if (restricted || (STREQ (temp, RESTRICTED_SHELL_NAME)))
    {
#if defined (RBASH_STATIC_PATH_VALUE)
      bind_variable ("PATH", RBASH_STATIC_PATH_VALUE, 0);
      stupidly_hack_special_variables ("PATH");		 
#endif
      set_var_read_only ("PATH");
      set_var_read_only ("SHELL");
      set_var_read_only ("ENV");
      set_var_read_only ("BASH_ENV");
      set_var_read_only ("HISTFILE");
      restricted = 1;
    }
  return (restricted);
}
#endif  

 
static int
uidget ()
{
  uid_t u;

  u = getuid ();
  if (current_user.uid != u)
    {
      FREE (current_user.user_name);
      FREE (current_user.shell);
      FREE (current_user.home_dir);
      current_user.user_name = current_user.shell = current_user.home_dir = (char *)NULL;
    }
  current_user.uid = u;
  current_user.gid = getgid ();
  current_user.euid = geteuid ();
  current_user.egid = getegid ();

   
  return (current_user.uid != current_user.euid) ||
	   (current_user.gid != current_user.egid);
}

void
disable_priv_mode ()
{
  int e;

#if HAVE_SETRESUID
  if (setresuid (current_user.uid, current_user.uid, current_user.uid) < 0)
#else
  if (setuid (current_user.uid) < 0)
#endif
    {
      e = errno;
      sys_error (_("cannot set uid to %d: effective uid %d"), current_user.uid, current_user.euid);
#if defined (EXIT_ON_SETUID_FAILURE)
      if (e == EAGAIN)
	exit (e);
#endif
    }
#if HAVE_SETRESGID
  if (setresgid (current_user.gid, current_user.gid, current_user.gid) < 0)
#else
  if (setgid (current_user.gid) < 0)
#endif
    sys_error (_("cannot set gid to %d: effective gid %d"), current_user.gid, current_user.egid);

  current_user.euid = current_user.uid;
  current_user.egid = current_user.gid;
}

#if defined (WORDEXP_OPTION)
static int
run_wordexp (words)
     char *words;
{
  int code, nw, nb;
  WORD_LIST *wl, *tl, *result;

  code = setjmp_nosigs (top_level);

  if (code != NOT_JUMPED)
    {
      switch (code)
	{
	   
	case FORCE_EOF:
	  return last_command_exit_value = 127;
	case ERREXIT:
	case EXITPROG:
	case EXITBLTIN:
	  return last_command_exit_value;
	case DISCARD:
	  return last_command_exit_value = 1;
	default:
	  command_error ("run_wordexp", CMDERR_BADJUMP, code, 0);
	}
    }

   
  if (words && *words)
    {
      with_input_from_string (words, "--wordexp");
      if (parse_command () != 0)
	return (126);
      if (global_command == 0)
	{
	  printf ("0\n0\n");
	  return (0);
	}
      if (global_command->type != cm_simple)
	return (126);
      wl = global_command->value.Simple->words;
      if (protected_mode)
	for (tl = wl; tl; tl = tl->next)
	  tl->word->flags |= W_NOCOMSUB|W_NOPROCSUB;
      result = wl ? expand_words_no_vars (wl) : (WORD_LIST *)0;
    }
  else
    result = (WORD_LIST *)0;

  last_command_exit_value = 0;

  if (result == 0)
    {
      printf ("0\n0\n");
      return (0);
    }

   
  for (nw = nb = 0, wl = result; wl; wl = wl->next)
    {
      nw++;
      nb += strlen (wl->word->word);
    }
  printf ("%u\n%u\n", nw, nb);
   
  for (wl = result; wl; wl = wl->next)
    printf ("%s\n", wl->word->word);

  return (0);
}
#endif

#if defined (ONESHOT)
 
static int
run_one_command (command)
     char *command;
{
  int code;

  code = setjmp_nosigs (top_level);

  if (code != NOT_JUMPED)
    {
#if defined (PROCESS_SUBSTITUTION)
      unlink_fifo_list ();
#endif  
      switch (code)
	{
	   
	case FORCE_EOF:
	  return last_command_exit_value = 127;
	case ERREXIT:
	case EXITPROG:
	case EXITBLTIN:
	  return last_command_exit_value;
	case DISCARD:
	  return last_command_exit_value = 1;
	default:
	  command_error ("run_one_command", CMDERR_BADJUMP, code, 0);
	}
    }
   return (parse_and_execute (savestring (command), "-c", SEVAL_NOHIST|SEVAL_RESETLINE));
}
#endif  

static int
bind_args (argv, arg_start, arg_end, start_index)
     char **argv;
     int arg_start, arg_end, start_index;
{
  register int i;
  WORD_LIST *args, *tl;

  for (i = arg_start, args = tl = (WORD_LIST *)NULL; i < arg_end; i++)
    {
      if (args == 0)
	args = tl = make_word_list (make_word (argv[i]), args);
      else
	{
	  tl->next = make_word_list (make_word (argv[i]), (WORD_LIST *)NULL);
	  tl = tl->next;
	}
    }

  if (args)
    {
      if (start_index == 0)	 
	{
	   
	  shell_name = savestring (args->word->word);
	  FREE (dollar_vars[0]);
	  dollar_vars[0] = savestring (args->word->word);
	  remember_args (args->next, 1);
	  if (debugging_mode)
	    {
	      push_args (args->next);	 
	      bash_argv_initialized = 1;
	    }
	}
      else			 
        {
	  remember_args (args, 1);
	   
	  if (debugging_mode)
	    {
	      push_args (args);		 
	      bash_argv_initialized = 1;
	    }
        }

      dispose_words (args);
    }

  return (i);
}

void
unbind_args ()
{
  remember_args ((WORD_LIST *)NULL, 1);
  pop_args ();				 
}

static void
start_debugger ()
{
#if defined (DEBUGGER) && defined (DEBUGGER_START_FILE)
  int old_errexit;
  int r;

  old_errexit = exit_immediately_on_error;
  exit_immediately_on_error = 0;

  r = force_execute_file (DEBUGGER_START_FILE, 1);
  if (r < 0)
    {
      internal_warning (_("cannot start debugger; debugging mode disabled"));
      debugging_mode = 0;
    }
  error_trace_mode = function_trace_mode = debugging_mode;

  set_shellopts ();
  set_bashopts ();

  exit_immediately_on_error += old_errexit;
#endif
}

static int
open_shell_script (script_name)
     char *script_name;
{
  int fd, e, fd_is_tty;
  char *filename, *path_filename, *t;
  char sample[80];
  int sample_len;
  struct stat sb;
#if defined (ARRAY_VARS)
  SHELL_VAR *funcname_v, *bash_source_v, *bash_lineno_v;
  ARRAY *funcname_a, *bash_source_a, *bash_lineno_a;
#endif

  filename = savestring (script_name);

  fd = open (filename, O_RDONLY);
  if ((fd < 0) && (errno == ENOENT) && (absolute_program (filename) == 0))
    {
      e = errno;
       
      path_filename = find_path_file (script_name);
      if (path_filename)
	{
	  free (filename);
	  filename = path_filename;
	  fd = open (filename, O_RDONLY);
	}
      else
	errno = e;
    }

  if (fd < 0)
    {
      e = errno;
      file_error (filename);
#if defined (JOB_CONTROL)
      end_job_control ();	 
#endif
      sh_exit ((e == ENOENT) ? EX_NOTFOUND : EX_NOINPUT);
    }

  free (dollar_vars[0]);
  dollar_vars[0] = exec_argv0 ? savestring (exec_argv0) : savestring (script_name);
  if (exec_argv0)
    {
      free (exec_argv0);
      exec_argv0 = (char *)NULL;
    }

  if (file_isdir (filename))
    {
#if defined (EISDIR)
      errno = EISDIR;
#else
      errno = EINVAL;
#endif
      file_error (filename);
#if defined (JOB_CONTROL)
      end_job_control ();	 
#endif
      sh_exit (EX_NOINPUT);
    }

#if defined (ARRAY_VARS)
  GET_ARRAY_FROM_VAR ("FUNCNAME", funcname_v, funcname_a);
  GET_ARRAY_FROM_VAR ("BASH_SOURCE", bash_source_v, bash_source_a);
  GET_ARRAY_FROM_VAR ("BASH_LINENO", bash_lineno_v, bash_lineno_a);

  array_push (bash_source_a, filename);
  if (bash_lineno_a)
    {
      t = itos (executing_line_number ());
      array_push (bash_lineno_a, t);
      free (t);
    }
  array_push (funcname_a, "main");
#endif

#ifdef HAVE_DEV_FD
  fd_is_tty = isatty (fd);
#else
  fd_is_tty = 0;
#endif

   
  if (fd_is_tty == 0 && (lseek (fd, 0L, 1) != -1))
    {
       
      sample_len = read (fd, sample, sizeof (sample));
      if (sample_len < 0)
	{
	  e = errno;
	  if ((fstat (fd, &sb) == 0) && S_ISDIR (sb.st_mode))
	    {
#if defined (EISDIR)
	      errno = EISDIR;
	      file_error (filename);
#else	      
	      internal_error (_("%s: Is a directory"), filename);
#endif
	    }
	  else
	    {
	      errno = e;
	      file_error (filename);
	    }
#if defined (JOB_CONTROL)
	  end_job_control ();	 
#endif
	  exit (EX_NOEXEC);
	}
      else if (sample_len > 0 && (check_binary_file (sample, sample_len)))
	{
	  internal_error (_("%s: cannot execute binary file"), filename);
#if defined (JOB_CONTROL)
	  end_job_control ();	 
#endif
	  exit (EX_BINARY_FILE);
	}
       
      lseek (fd, 0L, 0);
    }

   
  fd = move_to_high_fd (fd, 1, -1);

#if defined (BUFFERED_INPUT)
  default_buffered_input = fd;
  SET_CLOSE_ON_EXEC (default_buffered_input);
#else  
  default_input = fdopen (fd, "r");

  if (default_input == 0)
    {
      file_error (filename);
      exit (EX_NOTFOUND);
    }

  SET_CLOSE_ON_EXEC (fd);
  if (fileno (default_input) != fd)
    SET_CLOSE_ON_EXEC (fileno (default_input));
#endif  

   
  if (interactive_shell && fd_is_tty)
    {
      dup2 (fd, 0);
      close (fd);
      fd = 0;
#if defined (BUFFERED_INPUT)
      default_buffered_input = 0;
#else
      fclose (default_input);
      default_input = stdin;
#endif
    }
  else if (forced_interactive && fd_is_tty == 0)
     
    init_interactive_script ();

  free (filename);

  reading_shell_script = 1;
  return (fd);
}

 
static void
set_bash_input ()
{
   
#if defined (BUFFERED_INPUT)
  if (interactive == 0)
    sh_unset_nodelay_mode (default_buffered_input);
  else
#endif  
    sh_unset_nodelay_mode (fileno (stdin));

   
  if (interactive && no_line_editing == 0)
    with_input_from_stdin ();
#if defined (BUFFERED_INPUT)
  else if (interactive == 0)
    with_input_from_buffered_stream (default_buffered_input, dollar_vars[0]);
#endif  
  else
    with_input_from_stream (default_input, dollar_vars[0]);
}

 
void
unset_bash_input (check_zero)
     int check_zero;
{
#if defined (BUFFERED_INPUT)
  if ((check_zero && default_buffered_input >= 0) ||
      (check_zero == 0 && default_buffered_input > 0))
    {
      close_buffered_fd (default_buffered_input);
      default_buffered_input = bash_input.location.buffered_fd = -1;
      bash_input.type = st_none;		 
    }
#else  
  if (default_input)
    {
      fclose (default_input);
      default_input = (FILE *)NULL;
    }
#endif  
}
      

#if !defined (PROGRAM)
#  define PROGRAM "bash"
#endif

static void
set_shell_name (argv0)
     char *argv0;
{
   
  shell_name = argv0 ? base_pathname (argv0) : PROGRAM;

  if (argv0 && *argv0 == '-')
    {
      if (*shell_name == '-')
	shell_name++;
      login_shell = 1;
    }

  if (shell_name[0] == 's' && shell_name[1] == 'h' && shell_name[2] == '\0')
    act_like_sh++;
  if (shell_name[0] == 's' && shell_name[1] == 'u' && shell_name[2] == '\0')
    su_shell++;

  shell_name = argv0 ? argv0 : PROGRAM;
  FREE (dollar_vars[0]);
  dollar_vars[0] = savestring (shell_name);

   
  if (!shell_name || !*shell_name || (shell_name[0] == '-' && !shell_name[1]))
    shell_name = PROGRAM;
}

 
 
static void
set_option_defaults ()
{
#if defined (HISTORY)
  enable_history_list = 0;
#endif
}

static void
reset_option_defaults ()
{
#if defined (HISTORY)
  enable_history_list = -1;
#endif
}

static void
init_interactive ()
{
  expand_aliases = expaliases_flag = 1;
  interactive_shell = startup_state = interactive = 1;
#if defined (HISTORY)
  if (enable_history_list == -1)
    enable_history_list = 1;				 
  remember_on_history = enable_history_list;
#  if defined (BANG_HISTORY)
  histexp_flag = history_expansion;			 
#  endif
#endif
}

static void
init_noninteractive ()
{
#if defined (HISTORY)
  if (enable_history_list == -1)			 
    enable_history_list = 0;
  bash_history_reinit (0);
#endif  
  interactive_shell = startup_state = interactive = 0;
  expand_aliases = expaliases_flag = posixly_correct;	 
  no_line_editing = 1;
#if defined (JOB_CONTROL)
   
  set_job_control (forced_interactive||jobs_m_flag);
#endif  
}

static void
init_interactive_script ()
{
#if defined (HISTORY)
  if (enable_history_list == -1)
    enable_history_list = 1;
#endif
  init_noninteractive ();
  expand_aliases = expaliases_flag = interactive_shell = startup_state = 1;
#if defined (HISTORY)
  remember_on_history = enable_history_list;	 
#endif
}

void
get_current_user_info ()
{
  struct passwd *entry;

   
  if (current_user.user_name == 0)
    {
#if defined (__TANDEM)
      entry = getpwnam (getlogin ());
#else
      entry = getpwuid (current_user.uid);
#endif
      if (entry)
	{
	  current_user.user_name = savestring (entry->pw_name);
	  current_user.shell = (entry->pw_shell && entry->pw_shell[0])
				? savestring (entry->pw_shell)
				: savestring ("/bin/sh");
	  current_user.home_dir = savestring (entry->pw_dir);
	}
      else
	{
	  current_user.user_name = _("I have no name!");
	  current_user.user_name = savestring (current_user.user_name);
	  current_user.shell = savestring ("/bin/sh");
	  current_user.home_dir = savestring ("/");
	}
#if defined (HAVE_GETPWENT)
      endpwent ();
#endif
    }
}

 
static void
shell_initialize ()
{
  char hostname[256];
  int should_be_restricted;

   
  if (shell_initialized == 0)
    {
      sh_setlinebuf (stderr);
      sh_setlinebuf (stdout);
    }

   
  initialize_shell_builtins ();

   
  initialize_traps ();
  initialize_signals (0);

   
  if (current_host_name == 0)
    {
       
      if (gethostname (hostname, 255) < 0)
	current_host_name = "??host??";
      else
	current_host_name = savestring (hostname);
    }

   
  if (interactive_shell)
    get_current_user_info ();

   
  tilde_initialize ();

#if defined (RESTRICTED_SHELL)
  should_be_restricted = shell_is_restricted (shell_name);
#endif

   
#if defined (RESTRICTED_SHELL)
  initialize_shell_variables (shell_environment, privileged_mode||restricted||should_be_restricted||running_setuid);
#else
  initialize_shell_variables (shell_environment, privileged_mode||running_setuid);
#endif

   
  initialize_job_control (jobs_m_flag);

   
  initialize_bash_input ();

  initialize_flags ();

   
#if defined (RESTRICTED_SHELL)
  initialize_shell_options (privileged_mode||restricted||should_be_restricted||running_setuid);
  initialize_bashopts (privileged_mode||restricted||should_be_restricted||running_setuid);
#else
  initialize_shell_options (privileged_mode||running_setuid);
  initialize_bashopts (privileged_mode||running_setuid);
#endif
}

 
static void
shell_reinitialize ()
{
   
  primary_prompt = PPROMPT;
  secondary_prompt = SPROMPT;

   
  current_command_number = 1;

   
  no_rc = no_profile = 1;

   
  login_shell = make_login_shell = interactive = executing = 0;
  debugging = do_version = line_number = last_command_exit_value = 0;
  forced_interactive = interactive_shell = 0;
  subshell_environment = running_in_background = 0;
  expand_aliases = expaliases_flag = 0;
  bash_argv_initialized = 0;

   

#if defined (HISTORY)
  bash_history_reinit (enable_history_list = 0);
#endif  

#if defined (RESTRICTED_SHELL)
  restricted = 0;
#endif  

   
  bashrc_file = DEFAULT_BASHRC;

   
  delete_all_contexts (shell_variables);
  delete_all_variables (shell_functions);

  reinit_special_variables ();

#if defined (READLINE)
  bashline_reinitialize ();
#endif

  shell_reinitialized = 1;
}

static void
show_shell_usage (fp, extra)
     FILE *fp;
     int extra;
{
  int i;
  char *set_opts, *s, *t;

  if (extra)
    fprintf (fp, _("GNU bash, version %s-(%s)\n"), shell_version_string (), MACHTYPE);
  fprintf (fp, _("Usage:\t%s [GNU long option] [option] ...\n\t%s [GNU long option] [option] script-file ...\n"),
	     shell_name, shell_name);
  fputs (_("GNU long options:\n"), fp);
  for (i = 0; long_args[i].name; i++)
    fprintf (fp, "\t--%s\n", long_args[i].name);

  fputs (_("Shell options:\n"), fp);
  fputs (_("\t-ilrsD or -c command or -O shopt_option\t\t(invocation only)\n"), fp);

  for (i = 0, set_opts = 0; shell_builtins[i].name; i++)
    if (STREQ (shell_builtins[i].name, "set"))
      {
	set_opts = savestring (shell_builtins[i].short_doc);
	break;
      }

  if (set_opts)
    {
      s = strchr (set_opts, '[');
      if (s == 0)
	s = set_opts;
      while (*++s == '-')
	;
      t = strchr (s, ']');
      if (t)
	*t = '\0';
      fprintf (fp, _("\t-%s or -o option\n"), s);
      free (set_opts);
    }

  if (extra)
    {
      fprintf (fp, _("Type `%s -c \"help set\"' for more information about shell options.\n"), shell_name);
      fprintf (fp, _("Type `%s -c help' for more information about shell builtin commands.\n"), shell_name);
      fprintf (fp, _("Use the `bashbug' command to report bugs.\n"));
      fprintf (fp, "\n");
      fprintf (fp, _("bash home page: <http://www.gnu.org/software/bash>\n"));
      fprintf (fp, _("General help using GNU software: <http://www.gnu.org/gethelp/>\n"));
    }
}

static void
add_shopt_to_alist (opt, on_or_off)
     char *opt;
     int on_or_off;
{
  if (shopt_ind >= shopt_len)
    {
      shopt_len += 8;
      shopt_alist = (STRING_INT_ALIST *)xrealloc (shopt_alist, shopt_len * sizeof (shopt_alist[0]));
    }
  shopt_alist[shopt_ind].word = opt;
  shopt_alist[shopt_ind].token = on_or_off;
  shopt_ind++;
}

static void
run_shopt_alist ()
{
  register int i;

  for (i = 0; i < shopt_ind; i++)
    if (shopt_setopt (shopt_alist[i].word, (shopt_alist[i].token == '-')) != EXECUTION_SUCCESS)
      exit (EX_BADUSAGE);
  free (shopt_alist);
  shopt_alist = 0;
  shopt_ind = shopt_len = 0;
}
