 

 

 

#include "config.h"

#include "bashtypes.h"
#include "trap.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "posixtime.h"

#if defined (HAVE_SYS_RESOURCE_H) && defined (HAVE_WAIT3) && !defined (_POSIX_VERSION) && !defined (RLIMTYPE)
#  include <sys/resource.h>
#endif  

#if defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif

#include "filecntl.h"
#include <sys/ioctl.h>
#if defined (HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#if defined (BUFFERED_INPUT)
#  include "input.h"
#endif

 
#include "shtty.h"

 
 

 
#if defined (hpux) && !defined (TERMIOS_TTY_DRIVER)
#  include <bsdtty.h>
#endif  

#include "bashansi.h"
#include "bashintl.h"
#include "shell.h"
#include "parser.h"
#include "jobs.h"
#include "execute_cmd.h"
#include "flags.h"

#include "typemax.h"

#include "builtins/builtext.h"
#include "builtins/common.h"

#if defined (READLINE)
# include <readline/readline.h>
#endif

#if !defined (errno)
extern int errno;
#endif  

#if !defined (HAVE_KILLPG)
extern int killpg PARAMS((pid_t, int));
#endif

#if !DEFAULT_CHILD_MAX
#  define DEFAULT_CHILD_MAX 4096
#endif

#if !MAX_CHILD_MAX
#  define MAX_CHILD_MAX 32768
#endif

#if !defined (DEBUG)
#define MAX_JOBS_IN_ARRAY 4096		 
#else
#define MAX_JOBS_IN_ARRAY 128		 
#endif

 
#define PIDSTAT_TABLE_SZ 4096
#define BGPIDS_TABLE_SZ 512

 
#define DEL_WARNSTOPPED		1	 
#define DEL_NOBGPID		2	 

 

#if defined (ultrix) && defined (mips) && defined (_POSIX_VERSION)
#  define WAITPID(pid, statusp, options) \
	wait3 ((union wait *)statusp, options, (struct rusage *)0)
#else
#  if defined (_POSIX_VERSION) || defined (HAVE_WAITPID)
#    define WAITPID(pid, statusp, options) \
	waitpid ((pid_t)pid, statusp, options)
#  else
#    if defined (HAVE_WAIT3)
#      define WAITPID(pid, statusp, options) \
	wait3 (statusp, options, (struct rusage *)0)
#    else
#      define WAITPID(pid, statusp, options) \
	wait3 (statusp, options, (int *)0)
#    endif  
#  endif  
#endif  

 
#if defined (GETPGRP_VOID)
#  define getpgid(p) getpgrp ()
#else
#  define getpgid(p) getpgrp (p)
#endif  

 
#if defined (MUST_REINSTALL_SIGHANDLERS)
#  define REINSTALL_SIGCHLD_HANDLER signal (SIGCHLD, sigchld_handler)
#else
#  define REINSTALL_SIGCHLD_HANDLER
#endif  

 
#if !defined (WCONTINUED) || defined (WCONTINUED_BROKEN)
#  undef WCONTINUED
#  define WCONTINUED 0
#endif
#if !defined (WIFCONTINUED)
#  define WIFCONTINUED(s)	(0)
#endif

 
#define JOB_SLOTS 8

typedef int sh_job_map_func_t PARAMS((JOB *, int, int, int));

 
extern WORD_LIST *subst_assign_varlist;

extern SigHandler **original_signals;

extern void set_original_signal PARAMS((int, SigHandler *));

static struct jobstats zerojs = { -1L, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NO_JOB, NO_JOB, 0, 0 };
struct jobstats js = { -1L, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NO_JOB, NO_JOB, 0, 0 };

ps_index_t pidstat_table[PIDSTAT_TABLE_SZ];
struct bgpids bgpids = { 0, 0, 0, 0 };

struct procchain procsubs = { 0, 0, 0 };

 
JOB **jobs = (JOB **)NULL;

#if 0
 
int job_slots = 0;
#endif

 
int shell_tty = -1;

 
pid_t shell_pgrp = NO_PID;

 
pid_t terminal_pgrp = NO_PID;

 
pid_t original_pgrp = NO_PID;

 
pid_t pipeline_pgrp = (pid_t)0;

#if defined (PGRP_PIPE)
 
int pgrp_pipe[2] = { -1, -1 };
#endif

 
volatile pid_t last_made_pid = NO_PID;

 
volatile pid_t last_asynchronous_pid = NO_PID;

 
PROCESS *the_pipeline = (PROCESS *)NULL;

 
int job_control = 1;

 
int running_in_background = 0;

 
int already_making_children = 0;

 
int check_window_size = CHECKWINSIZE_DEFAULT;

PROCESS *last_procsub_child = (PROCESS *)NULL;

 

void debug_print_pgrps (void);

static sighandler wait_sigint_handler PARAMS((int));
static sighandler sigchld_handler PARAMS((int));
static sighandler sigcont_sighandler PARAMS((int));
static sighandler sigstop_sighandler PARAMS((int));

static int waitchld PARAMS((pid_t, int));

static PROCESS *find_pid_in_pipeline PARAMS((pid_t, PROCESS *, int));
static PROCESS *find_pipeline PARAMS((pid_t, int, int *));
static PROCESS *find_process PARAMS((pid_t, int, int *));

static char *current_working_directory PARAMS((void));
static char *job_working_directory PARAMS((void));
static char *j_strsignal PARAMS((int));
static char *printable_job_status PARAMS((int, PROCESS *, int));

static PROCESS *find_last_proc PARAMS((int, int));
static pid_t find_last_pid PARAMS((int, int));

static int set_new_line_discipline PARAMS((int));
static int map_over_jobs PARAMS((sh_job_map_func_t *, int, int));
static int job_last_stopped PARAMS((int));
static int job_last_running PARAMS((int));
static int most_recent_job_in_state PARAMS((int, JOB_STATE));
static int find_job PARAMS((pid_t, int, PROCESS **));
static int print_job PARAMS((JOB *, int, int, int));
static int process_exit_status PARAMS((WAIT));
static int process_exit_signal PARAMS((WAIT));
static int set_job_status_and_cleanup PARAMS((int));

static WAIT job_signal_status PARAMS((int));
static WAIT raw_job_exit_status PARAMS((int));

static void notify_of_job_status PARAMS((void));
static void reset_job_indices PARAMS((void));
static void cleanup_dead_jobs PARAMS((void));
static int processes_in_job PARAMS((int));
static void realloc_jobs_list PARAMS((void));
static int compact_jobs_list PARAMS((int));
static void add_process PARAMS((char *, pid_t));
static void print_pipeline PARAMS((PROCESS *, int, int, FILE *));
static void pretty_print_job PARAMS((int, int, FILE *));
static void set_current_job PARAMS((int));
static void reset_current PARAMS((void));
static void set_job_running PARAMS((int));
static void setjstatus PARAMS((int));
static int maybe_give_terminal_to PARAMS((pid_t, pid_t, int));
static void mark_all_jobs_as_dead PARAMS((void));
static void mark_dead_jobs_as_notified PARAMS((int));
static void restore_sigint_handler PARAMS((void));
#if defined (PGRP_PIPE)
static void pipe_read PARAMS((int *));
#endif

 

static ps_index_t *pshash_getbucket PARAMS((pid_t));
static void pshash_delindex PARAMS((ps_index_t));

 
static struct pidstat *bgp_add PARAMS((pid_t, int));
static int bgp_delete PARAMS((pid_t));
static void bgp_clear PARAMS((void));
static int bgp_search PARAMS((pid_t));

static struct pipeline_saver *alloc_pipeline_saver PARAMS((void));

static ps_index_t bgp_getindex PARAMS((void));
static void bgp_resize PARAMS((void));	 

#if defined (ARRAY_VARS)
static int *pstatuses;		 
static int statsize;
#endif

 
static int sigchld;
static int queue_sigchld;

#define QUEUE_SIGCHLD(os)	(os) = sigchld, queue_sigchld++

 
#define UNQUEUE_SIGCHLD(os) \
	do { \
	  queue_sigchld--; \
	  if (queue_sigchld == 0 && os != sigchld) \
	    { \
	      queue_sigchld = 1; \
	      waitchld (-1, 0); \
	      queue_sigchld = 0; \
	    } \
	} while (0)

static SigHandler *old_tstp, *old_ttou, *old_ttin;
static SigHandler *old_cont = (SigHandler *)SIG_DFL;

 
static struct pipeline_saver *saved_pipeline;
static int saved_already_making_children;

 
static int jobs_list_frozen;

static char retcode_name_buffer[64];

#if !defined (_POSIX_VERSION)

 
#define setpgid(pid, pgrp)	setpgrp (pid, pgrp)
#define tcsetpgrp(fd, pgrp)	ioctl ((fd), TIOCSPGRP, &(pgrp))

pid_t
tcgetpgrp (fd)
     int fd;
{
  pid_t pgrp;

   
  if (ioctl (fd, TIOCGPGRP, &pgrp) < 0)
    return (-1);
  return (pgrp);
}

#endif  

 
void
init_job_stats ()
{
  js = zerojs;
}

 
static char *
current_working_directory ()
{
  char *dir;
  static char d[PATH_MAX];

  dir = get_string_value ("PWD");

  if (dir == 0 && the_current_working_directory && no_symbolic_links)
    dir = the_current_working_directory;

  if (dir == 0)
    {
      dir = getcwd (d, sizeof(d));
      if (dir)
	dir = d;
    }

  return (dir == 0) ? "<unknown>" : dir;
}

 
static char *
job_working_directory ()
{
  char *dir;

  dir = get_string_value ("PWD");
  if (dir)
    return (savestring (dir));

  dir = get_working_directory ("job-working-directory");
  if (dir)
    return (dir);

  return (savestring ("<unknown>"));
}

void
making_children ()
{
  if (already_making_children)
    return;

  already_making_children = 1;
  start_pipeline ();
}

void
stop_making_children ()
{
  already_making_children = 0;
}

void
cleanup_the_pipeline ()
{
  PROCESS *disposer;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  disposer = the_pipeline;
  the_pipeline = (PROCESS *)NULL;
  UNBLOCK_CHILD (oset);

  if (disposer)
    discard_pipeline (disposer);
}

 
void
discard_last_procsub_child ()
{
  PROCESS *disposer;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  disposer = last_procsub_child;
  last_procsub_child = (PROCESS *)NULL;
  UNBLOCK_CHILD (oset);

  if (disposer)
    discard_pipeline (disposer);
}

static struct pipeline_saver *
alloc_pipeline_saver ()
{
  struct pipeline_saver *ret;

  ret = (struct pipeline_saver *)xmalloc (sizeof (struct pipeline_saver));
  ret->pipeline = 0;
  ret->next = 0;
  return ret;
}

void
save_pipeline (clear)
     int clear;
{
  sigset_t set, oset;
  struct pipeline_saver *saver;

  BLOCK_CHILD (set, oset);
  saver = alloc_pipeline_saver ();
  saver->pipeline = the_pipeline;
  saver->next = saved_pipeline;
  saved_pipeline = saver;
  if (clear)
    the_pipeline = (PROCESS *)NULL;
  saved_already_making_children = already_making_children;
  UNBLOCK_CHILD (oset);
}

PROCESS *
restore_pipeline (discard)
     int discard;
{
  PROCESS *old_pipeline;
  sigset_t set, oset;
  struct pipeline_saver *saver;

  BLOCK_CHILD (set, oset);
  old_pipeline = the_pipeline;
  the_pipeline = saved_pipeline->pipeline;
  saver = saved_pipeline;
  saved_pipeline = saved_pipeline->next;
  free (saver);
  already_making_children = saved_already_making_children;
  UNBLOCK_CHILD (oset);

  if (discard && old_pipeline)
    {
      discard_pipeline (old_pipeline);
      return ((PROCESS *)NULL);
    }
  return old_pipeline;
}

 
void
start_pipeline ()
{
  if (the_pipeline)
    {
      cleanup_the_pipeline ();
       
      if (pipeline_pgrp != shell_pgrp)
	pipeline_pgrp = 0;
#if defined (PGRP_PIPE)
      sh_closepipe (pgrp_pipe);
#endif
    }

#if defined (PGRP_PIPE)
  if (job_control)
    {
      if (pipe (pgrp_pipe) == -1)
	sys_error (_("start_pipeline: pgrp pipe"));
    }
#endif
}

 
int
stop_pipeline (async, deferred)
     int async;
     COMMAND *deferred;
{
  register int i, j;
  JOB *newjob;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);

#if defined (PGRP_PIPE)
   
  sh_closepipe (pgrp_pipe);
#endif

  cleanup_dead_jobs ();

  if (js.j_jobslots == 0)
    {
      js.j_jobslots = JOB_SLOTS;
      jobs = (JOB **)xmalloc (js.j_jobslots * sizeof (JOB *));

       
      for (i = 0; i < js.j_jobslots; i++)
	jobs[i] = (JOB *)NULL;

      js.j_firstj = js.j_lastj = js.j_njobs = 0;
    }

   
   
   
  if (interactive)
    {
      for (i = js.j_jobslots; i; i--)
	if (jobs[i - 1])
	  break;
    }
  else
    {
#if 0
       
      for (i = js.j_lastj+1; i != js.j_lastj; i++)
	{
	  if (i >= js.j_jobslots)
	    i = 0;
	  if (jobs[i] == 0)
	    break;
	}	
      if (i == js.j_lastj)
        i = js.j_jobslots;
#else
       
      for (i = js.j_lastj ? js.j_lastj + 1 : js.j_lastj; i < js.j_jobslots; i++)
	if (jobs[i] == 0)
	  break;
#endif
    }

   

   
  if ((interactive_shell == 0 || subshell_environment) && i == js.j_jobslots && js.j_jobslots >= MAX_JOBS_IN_ARRAY)
    i = compact_jobs_list (0);

   
  if (i == js.j_jobslots)
    {
      js.j_jobslots += JOB_SLOTS;
      jobs = (JOB **)xrealloc (jobs, (js.j_jobslots * sizeof (JOB *)));

      for (j = i; j < js.j_jobslots; j++)
	jobs[j] = (JOB *)NULL;
    }

   
  if (the_pipeline)
    {
      register PROCESS *p;
      int any_running, any_stopped, n;

      newjob = (JOB *)xmalloc (sizeof (JOB));

      for (n = 1, p = the_pipeline; p->next != the_pipeline; n++, p = p->next)
	;
      p->next = (PROCESS *)NULL;
      newjob->pipe = REVERSE_LIST (the_pipeline, PROCESS *);
      for (p = newjob->pipe; p->next; p = p->next)
	;
      p->next = newjob->pipe;

      the_pipeline = (PROCESS *)NULL;
      newjob->pgrp = pipeline_pgrp;

       
      if (pipeline_pgrp != shell_pgrp)
	pipeline_pgrp = 0;

      newjob->flags = 0;
      if (pipefail_opt)
	newjob->flags |= J_PIPEFAIL;

       
      if (job_control)
	newjob->flags |= J_JOBCONTROL;

       
      p = newjob->pipe;
      any_running = any_stopped = 0;
      do
	{
	  any_running |= PRUNNING (p);
	  any_stopped |= PSTOPPED (p);
	  p = p->next;
	}
      while (p != newjob->pipe);

      newjob->state = any_running ? JRUNNING : (any_stopped ? JSTOPPED : JDEAD);
      newjob->wd = job_working_directory ();
      newjob->deferred = deferred;

      newjob->j_cleanup = (sh_vptrfunc_t *)NULL;
      newjob->cleanarg = (PTR_T) NULL;

      jobs[i] = newjob;
      if (newjob->state == JDEAD && (newjob->flags & J_FOREGROUND))
	setjstatus (i);
      if (newjob->state == JDEAD)
	{
	  js.c_reaped += n;	 
	  js.j_ndead++;
	}
      js.c_injobs += n;

      js.j_lastj = i;
      js.j_njobs++;
    }
  else
    newjob = (JOB *)NULL;

  if (newjob)
    js.j_lastmade = newjob;

  if (async)
    {
      if (newjob)
	{
	  newjob->flags &= ~J_FOREGROUND;
	  newjob->flags |= J_ASYNC;
	  js.j_lastasync = newjob;
	}
      reset_current ();
    }
  else
    {
      if (newjob)
	{
	  newjob->flags |= J_FOREGROUND;
	   
	  if (job_control && newjob->pgrp && (subshell_environment&SUBSHELL_ASYNC) == 0 && running_in_background == 0)
	    maybe_give_terminal_to (shell_pgrp, newjob->pgrp, 0);
	}
    }

  stop_making_children ();
  UNBLOCK_CHILD (oset);
  return (newjob ? i : js.j_current);
}

 

 
static void
bgp_resize ()
{
  ps_index_t nsize, nsize_cur, nsize_max;
  ps_index_t psi;

  if (bgpids.nalloc == 0)
    {
       
      for (psi = 0; psi < PIDSTAT_TABLE_SZ; psi++)
        pidstat_table[psi] = NO_PIDSTAT;
      nsize = BGPIDS_TABLE_SZ;	 
      bgpids.head = 0;
    }
  else
    nsize = bgpids.nalloc;

  nsize_max = TYPE_MAXIMUM (ps_index_t);
  nsize_cur = (ps_index_t)js.c_childmax;
  if (nsize_cur < 0)				 
    nsize_cur = MAX_CHILD_MAX;

  while (nsize > 0 && nsize < nsize_cur)	 
    nsize <<= 1;
  if (nsize > nsize_max || nsize <= 0)		 
    nsize = nsize_max;
  if (nsize > MAX_CHILD_MAX)
    nsize = nsize_max = MAX_CHILD_MAX;		 

  if (bgpids.nalloc < nsize_cur && bgpids.nalloc < nsize_max)
    {
      bgpids.storage = (struct pidstat *)xrealloc (bgpids.storage, nsize * sizeof (struct pidstat));

      for (psi = bgpids.nalloc; psi < nsize; psi++)
	bgpids.storage[psi].pid = NO_PID;

      bgpids.nalloc = nsize;

    }
  else if (bgpids.head >= bgpids.nalloc)	 
    bgpids.head = 0;
}

static ps_index_t
bgp_getindex ()
{
  if (bgpids.nalloc < (ps_index_t)js.c_childmax || bgpids.head >= bgpids.nalloc)
    bgp_resize ();

  pshash_delindex (bgpids.head);		 
  return bgpids.head++;
}

static ps_index_t *
pshash_getbucket (pid)
     pid_t pid;
{
  unsigned long hash;		 

  hash = pid * 0x9e370001UL;
  return (&pidstat_table[hash % PIDSTAT_TABLE_SZ]);
}

static struct pidstat *
bgp_add (pid, status)
     pid_t pid;
     int status;
{
  ps_index_t *bucket, psi;
  struct pidstat *ps;

   

  bucket = pshash_getbucket (pid);	 
  psi = bgp_getindex ();		 

   
  if (psi == *bucket)
    {
      internal_debug ("hashed pid %d (pid %d) collides with bgpids.head, skipping", psi, pid);
      bgpids.storage[psi].pid = NO_PID;		 
      psi = bgp_getindex ();			 
    }

  ps = &bgpids.storage[psi];

  ps->pid = pid;
  ps->status = status;
  ps->bucket_next = *bucket;
  ps->bucket_prev = NO_PIDSTAT;

  bgpids.npid++;

#if 0
  if (bgpids.npid > js.c_childmax)
    bgp_prune ();
#endif

  if (ps->bucket_next != NO_PIDSTAT)
    bgpids.storage[ps->bucket_next].bucket_prev = psi;

  *bucket = psi;		 

  return ps;
}

static void
pshash_delindex (psi)
     ps_index_t psi;
{
  struct pidstat *ps;
  ps_index_t *bucket;

  ps = &bgpids.storage[psi];
  if (ps->pid == NO_PID)
    return;

  if (ps->bucket_next != NO_PIDSTAT)
    bgpids.storage[ps->bucket_next].bucket_prev = ps->bucket_prev;
  if (ps->bucket_prev != NO_PIDSTAT)
    bgpids.storage[ps->bucket_prev].bucket_next = ps->bucket_next;
  else
    {
      bucket = pshash_getbucket (ps->pid);
      *bucket = ps->bucket_next;	 
    }

   
  ps->pid = NO_PID;
  ps->bucket_next = ps->bucket_prev = NO_PIDSTAT;
}

static int
bgp_delete (pid)
     pid_t pid;
{
  ps_index_t psi, orig_psi;

  if (bgpids.storage == 0 || bgpids.nalloc == 0 || bgpids.npid == 0)
    return 0;

   
  for (orig_psi = psi = *(pshash_getbucket (pid)); psi != NO_PIDSTAT; psi = bgpids.storage[psi].bucket_next)
    {
      if (bgpids.storage[psi].pid == pid)
	break;
      if (orig_psi == bgpids.storage[psi].bucket_next)	 
	{
	  internal_warning (_("bgp_delete: LOOP: psi (%d) == storage[psi].bucket_next"), psi);
	  return 0;
	}
    }

  if (psi == NO_PIDSTAT)
    return 0;		 

#if 0
  itrace("bgp_delete: deleting %d", pid);
#endif

  pshash_delindex (psi);	 

  bgpids.npid--;
  return 1;
}

 
static void
bgp_clear ()
{
  if (bgpids.storage == 0 || bgpids.nalloc == 0)
    return;

  free (bgpids.storage);

  bgpids.storage = 0;
  bgpids.nalloc = 0;
  bgpids.head = 0;

  bgpids.npid = 0;
}

 
static int
bgp_search (pid)
     pid_t pid;
{
  ps_index_t psi, orig_psi;

  if (bgpids.storage == 0 || bgpids.nalloc == 0 || bgpids.npid == 0)
    return -1;

   
  for (orig_psi = psi = *(pshash_getbucket (pid)); psi != NO_PIDSTAT; psi = bgpids.storage[psi].bucket_next)
    {
      if (bgpids.storage[psi].pid == pid)
	return (bgpids.storage[psi].status);
      if (orig_psi == bgpids.storage[psi].bucket_next)	 
	{
	  internal_warning (_("bgp_search: LOOP: psi (%d) == storage[psi].bucket_next"), psi);
	  return -1;
	}
    }

  return -1;
}

#if 0
static void
bgp_prune ()
{
  return;
}
#endif

 
void
save_proc_status (pid, status)
     pid_t pid;
     int status;
{
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  bgp_add (pid, status);
  UNBLOCK_CHILD (oset);  
}

#if defined (PROCESS_SUBSTITUTION)
 

static void
procsub_free (p)
     PROCESS *p;
{
  FREE (p->command);
  free (p);
}
    
PROCESS *
procsub_add (p)
     PROCESS *p;
{
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  if (procsubs.head == 0)
    {
      procsubs.head = procsubs.end = p;
      procsubs.nproc = 0;
    }
  else
    {
      procsubs.end->next = p;
      procsubs.end = p;
    }
  procsubs.nproc++;
  UNBLOCK_CHILD (oset);

  return p;
}

PROCESS *
procsub_search (pid)
     pid_t pid;
{
  PROCESS *p;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  for (p = procsubs.head; p; p = p->next)
    if (p->pid == pid)
      break;
  UNBLOCK_CHILD (oset);

  return p;
}

PROCESS *
procsub_delete (pid)
     pid_t pid;
{
  PROCESS *p, *prev;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  for (p = prev = procsubs.head; p; prev = p, p = p->next)
    if (p->pid == pid)
      {
	prev->next = p->next;
	break;
      }

  if (p == 0)
    {
      UNBLOCK_CHILD (oset);
      return p;
    }

  if (p == procsubs.head)
    procsubs.head = procsubs.head->next;
  else if (p == procsubs.end)
    procsubs.end = prev;

  procsubs.nproc--;
  if (procsubs.nproc == 0)
    procsubs.head = procsubs.end = 0;
  else if (procsubs.nproc == 1)		 
    procsubs.end = procsubs.head;

   
  bgp_add (p->pid, process_exit_status (p->status));
  UNBLOCK_CHILD (oset);
  return (p);  
}

int
procsub_waitpid (pid)
     pid_t pid;
{
  PROCESS *p;
  int r;

  p = procsub_search (pid);
  if (p == 0)
    return -1;
  if (p->running == PS_DONE)
    return (p->status);
  r = wait_for (p->pid, 0);
  return (r);			 
}

void
procsub_waitall ()
{
  PROCESS *p;
  int r;

  for (p = procsubs.head; p; p = p->next)
    {
      if (p->running == PS_DONE)
	continue;
      r = wait_for (p->pid, 0);
    }
}

void
procsub_clear ()
{
  PROCESS *p, *ps;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);
  
  for (ps = procsubs.head; ps; )
    {
      p = ps;
      ps = ps->next;
      procsub_free (p);
    }
  procsubs.head = procsubs.end = 0;
  procsubs.nproc = 0;        
  UNBLOCK_CHILD (oset);
}

 
void
procsub_prune ()
{
  PROCESS *ohead, *oend, *ps, *p;
  int onproc;

  if (procsubs.nproc == 0)
    return;

  ohead = procsubs.head;
  oend = procsubs.end;
  onproc = procsubs.nproc;

  procsubs.head = procsubs.end = 0;
  procsubs.nproc = 0;

  for (p = ohead; p; )
    {
      ps = p->next;
      p->next = 0;
      if (p->running == PS_DONE)
	{
	  bgp_add (p->pid, process_exit_status (p->status));
	  procsub_free (p);
	}
      else
	procsub_add (p);
      p = ps;
    }
}
#endif

 
static void
reset_job_indices ()
{
  int old;

  if (jobs[js.j_firstj] == 0)
    {
      old = js.j_firstj++;
      if (old >= js.j_jobslots)
	old = js.j_jobslots - 1;
      while (js.j_firstj != old)
	{
	  if (js.j_firstj >= js.j_jobslots)
	    js.j_firstj = 0;
	  if (jobs[js.j_firstj] || js.j_firstj == old)	 
	    break;
	  js.j_firstj++;
	}
      if (js.j_firstj == old)
	js.j_firstj = js.j_lastj = js.j_njobs = 0;
    }
  if (jobs[js.j_lastj] == 0)
    {
      old = js.j_lastj--;
      if (old < 0)
	old = 0;
      while (js.j_lastj != old)
	{
	  if (js.j_lastj < 0)
	    js.j_lastj = js.j_jobslots - 1;
	  if (jobs[js.j_lastj] || js.j_lastj == old)	 
	    break;
	  js.j_lastj--;
	}
      if (js.j_lastj == old)
	js.j_firstj = js.j_lastj = js.j_njobs = 0;
    }
}
      
 
static void
cleanup_dead_jobs ()
{
  register int i;
  int os;
  PROCESS *discard;

  if (js.j_jobslots == 0 || jobs_list_frozen)
    return;

  QUEUE_SIGCHLD(os);

   
  for (i = 0; i < js.j_jobslots; i++)
    {
      if (i < js.j_firstj && jobs[i])
	INTERNAL_DEBUG (("cleanup_dead_jobs: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
      if (i > js.j_lastj && jobs[i])
	INTERNAL_DEBUG(("cleanup_dead_jobs: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

      if (jobs[i] && DEADJOB (i) && IS_NOTIFIED (i))
	delete_job (i, 0);
    }

#if defined (PROCESS_SUBSTITUTION)
  procsub_prune ();
  last_procsub_child = (PROCESS *)NULL;
#endif

#if defined (COPROCESS_SUPPORT)
  coproc_reap ();
#endif

  UNQUEUE_SIGCHLD(os);
}

static int
processes_in_job (job)
     int job;
{
  int nproc;
  register PROCESS *p;

  nproc = 0;
  p = jobs[job]->pipe;
  do
    {
      p = p->next;
      nproc++;
    }
  while (p != jobs[job]->pipe);

  return nproc;
}

static void
delete_old_job (pid)
     pid_t pid;
{
  PROCESS *p;
  int job;

  job = find_job (pid, 0, &p);
  if (job != NO_JOB)
    {
      INTERNAL_DEBUG (("delete_old_job: found pid %d in job %d with state %d", pid, job, jobs[job]->state));
      if (JOBSTATE (job) == JDEAD)
	delete_job (job, DEL_NOBGPID);
      else
	{
	  internal_debug (_("forked pid %d appears in running job %d"), pid, job+1);
	  if (p)
	    p->pid = 0;
	}
    }
}

 
static void
realloc_jobs_list ()
{
  sigset_t set, oset;
  int nsize, i, j, ncur, nprev;
  JOB **nlist;

  ncur = nprev = NO_JOB;
  nsize = ((js.j_njobs + JOB_SLOTS - 1) / JOB_SLOTS);
  nsize *= JOB_SLOTS;
  i = js.j_njobs % JOB_SLOTS;
  if (i == 0 || i > (JOB_SLOTS >> 1))
    nsize += JOB_SLOTS;

  BLOCK_CHILD (set, oset);
  nlist = (js.j_jobslots == nsize) ? jobs : (JOB **) xmalloc (nsize * sizeof (JOB *));

  js.c_reaped = js.j_ndead = 0;
  for (i = j = 0; i < js.j_jobslots; i++)
    if (jobs[i])
      {
	if (i == js.j_current)
	  ncur = j;
	if (i == js.j_previous)
	  nprev = j;
	nlist[j++] = jobs[i];
	if (jobs[i]->state == JDEAD)
	  {
	    js.j_ndead++;
	    js.c_reaped += processes_in_job (i);
	  }
      }

#if 0
  itrace ("realloc_jobs_list: resize jobs list from %d to %d", js.j_jobslots, nsize);
  itrace ("realloc_jobs_list: j_lastj changed from %d to %d", js.j_lastj, (j > 0) ? j - 1 : 0);
  itrace ("realloc_jobs_list: j_njobs changed from %d to %d", js.j_njobs, j);
  itrace ("realloc_jobs_list: js.j_ndead %d js.c_reaped %d", js.j_ndead, js.c_reaped);
#endif

  js.j_firstj = 0;
  js.j_lastj = (j > 0) ? j - 1 : 0;
  js.j_njobs = j;
  js.j_jobslots = nsize;

   
  for ( ; j < nsize; j++)
    nlist[j] = (JOB *)NULL;

  if (jobs != nlist)
    {
      free (jobs);
      jobs = nlist;
    }

  if (ncur != NO_JOB)
    js.j_current = ncur;
  if (nprev != NO_JOB)
    js.j_previous = nprev;

   
  if (js.j_current == NO_JOB || js.j_previous == NO_JOB || js.j_current > js.j_lastj || js.j_previous > js.j_lastj)
    reset_current ();

#if 0
  itrace ("realloc_jobs_list: reset js.j_current (%d) and js.j_previous (%d)", js.j_current, js.j_previous);
#endif

  UNBLOCK_CHILD (oset);
}

 
static int
compact_jobs_list (flags)
     int flags;
{
  if (js.j_jobslots == 0 || jobs_list_frozen)
    return js.j_jobslots;

  reap_dead_jobs ();
  realloc_jobs_list ();

#if 0
  itrace("compact_jobs_list: returning %d", (js.j_lastj || jobs[js.j_lastj]) ? js.j_lastj + 1 : 0);
#endif

  return ((js.j_lastj || jobs[js.j_lastj]) ? js.j_lastj + 1 : 0);
}

 
void
delete_job (job_index, dflags)
     int job_index, dflags;
{
  register JOB *temp;
  PROCESS *proc;
  int ndel;

  if (js.j_jobslots == 0 || jobs_list_frozen)
    return;

  if ((dflags & DEL_WARNSTOPPED) && subshell_environment == 0 && STOPPED (job_index))
    internal_warning (_("deleting stopped job %d with process group %ld"), job_index+1, (long)jobs[job_index]->pgrp);
  temp = jobs[job_index];
  if (temp == 0)
    return;

  if ((dflags & DEL_NOBGPID) == 0 && (temp->flags & (J_ASYNC|J_FOREGROUND)) == J_ASYNC)
    {
      proc = find_last_proc (job_index, 0);
      if (proc)
	bgp_add (proc->pid, process_exit_status (proc->status));
    }

  jobs[job_index] = (JOB *)NULL;
  if (temp == js.j_lastmade)
    js.j_lastmade = 0;
  else if (temp == js.j_lastasync)
    js.j_lastasync = 0;

  free (temp->wd);
  ndel = discard_pipeline (temp->pipe);

  js.c_injobs -= ndel;
  if (temp->state == JDEAD)
    {
       
      js.c_reaped -= ndel;	 
      js.j_ndead--;
      if (js.c_reaped < 0)
	{
	  INTERNAL_DEBUG (("delete_job (%d pgrp %d): js.c_reaped (%d) < 0 ndel = %d js.j_ndead = %d", job_index, temp->pgrp, js.c_reaped, ndel, js.j_ndead));
	  js.c_reaped = 0;
	}
    }

  if (temp->deferred)
    dispose_command (temp->deferred);

  free (temp);

  js.j_njobs--;
  if (js.j_njobs == 0)
    js.j_firstj = js.j_lastj = 0;
  else if (jobs[js.j_firstj] == 0 || jobs[js.j_lastj] == 0)
    reset_job_indices ();

  if (job_index == js.j_current || job_index == js.j_previous)
    reset_current ();
}

 
void
nohup_job (job_index)
     int job_index;
{
  register JOB *temp;

  if (js.j_jobslots == 0)
    return;

  if (temp = jobs[job_index])
    temp->flags |= J_NOHUP;
}

 
int
discard_pipeline (chain)
     register PROCESS *chain;
{
  register PROCESS *this, *next;
  int n;

  this = chain;
  n = 0;
  do
    {
      next = this->next;
      FREE (this->command);
      free (this);
      n++;
      this = next;
    }
  while (this != chain);

  return n;
}

 
static void
add_process (name, pid)
     char *name;
     pid_t pid;
{
  PROCESS *t, *p;

#if defined (RECYCLES_PIDS)
  int j;
  p = find_process (pid, 0, &j);
  if (p)
    {
      if (j == NO_JOB)
	internal_debug ("add_process: process %5ld (%s) in the_pipeline", (long)p->pid, p->command);
      if (PALIVE (p))
	internal_warning (_("add_process: pid %5ld (%s) marked as still alive"), (long)p->pid, p->command);
      p->running = PS_RECYCLED;		 
    }
#endif

  t = (PROCESS *)xmalloc (sizeof (PROCESS));
  t->next = the_pipeline;
  t->pid = pid;
  WSTATUS (t->status) = 0;
  t->running = PS_RUNNING;
  t->command = name;
  the_pipeline = t;

  if (t->next == 0)
    t->next = t;
  else
    {
      p = t->next;
      while (p->next != t->next)
	p = p->next;
      p->next = t;
    }
}

 
void
append_process (name, pid, status, jid)
     char *name;
     pid_t pid;
     int status;
     int jid;
{
  PROCESS *t, *p;

  t = (PROCESS *)xmalloc (sizeof (PROCESS));
  t->next = (PROCESS *)NULL;
  t->pid = pid;
   
  t->status = (status & 0xff) << WEXITSTATUS_OFFSET;
  t->running = PS_DONE;
  t->command = name;

  js.c_reaped++;	 

  for (p = jobs[jid]->pipe; p->next != jobs[jid]->pipe; p = p->next)
    ;
  p->next = t;
  t->next = jobs[jid]->pipe;
}

#if 0
 
int
rotate_the_pipeline ()
{
  PROCESS *p;

  if (the_pipeline->next == the_pipeline)
    return;
  for (p = the_pipeline; p->next != the_pipeline; p = p->next)
    ;
  the_pipeline = p;
}

 
int
reverse_the_pipeline ()
{
  PROCESS *p, *n;

  if (the_pipeline->next == the_pipeline)
    return;

  for (p = the_pipeline; p->next != the_pipeline; p = p->next)
    ;
  p->next = (PROCESS *)NULL;

  n = REVERSE_LIST (the_pipeline, PROCESS *);

  the_pipeline = n;
  for (p = the_pipeline; p->next; p = p->next)
    ;
  p->next = the_pipeline;
}
#endif

 
static int
map_over_jobs (func, arg1, arg2)
     sh_job_map_func_t *func;
     int arg1, arg2;
{
  register int i;
  int result;
  sigset_t set, oset;

  if (js.j_jobslots == 0)
    return 0;

  BLOCK_CHILD (set, oset);

   
  for (i = result = 0; i < js.j_jobslots; i++)
    {
      if (i < js.j_firstj && jobs[i])
	INTERNAL_DEBUG (("map_over_jobs: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
      if (i > js.j_lastj && jobs[i])
	INTERNAL_DEBUG (("map_over_jobs: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

      if (jobs[i])
	{
	  result = (*func)(jobs[i], arg1, arg2, i);
	  if (result)
	    break;
	}
    }

  UNBLOCK_CHILD (oset);

  return (result);
}

 
void
terminate_current_pipeline ()
{
  if (pipeline_pgrp && pipeline_pgrp != shell_pgrp)
    {
      killpg (pipeline_pgrp, SIGTERM);
      killpg (pipeline_pgrp, SIGCONT);
    }
}

 
void
terminate_stopped_jobs ()
{
  register int i;

   
  for (i = 0; i < js.j_jobslots; i++)
    {
      if (jobs[i] && STOPPED (i))
	{
	  killpg (jobs[i]->pgrp, SIGTERM);
	  killpg (jobs[i]->pgrp, SIGCONT);
	}
    }
}

 
void
hangup_all_jobs ()
{
  register int i;

   
  for (i = 0; i < js.j_jobslots; i++)
    {
      if (jobs[i])
	{
	  if  (jobs[i]->flags & J_NOHUP)
	    continue;
	  killpg (jobs[i]->pgrp, SIGHUP);
	  if (STOPPED (i))
	    killpg (jobs[i]->pgrp, SIGCONT);
	}
    }
}

void
kill_current_pipeline ()
{
  stop_making_children ();
  start_pipeline ();
}

static PROCESS *
find_pid_in_pipeline (pid, pipeline, alive_only)
     pid_t pid;
     PROCESS *pipeline;
     int alive_only;
{
  PROCESS *p;

  p = pipeline;
  do
    {
       
      if (p->pid == pid && ((alive_only == 0 && PRECYCLED(p) == 0) || PALIVE(p)))
	return (p);

      p = p->next;
    }
  while (p != pipeline);
  return ((PROCESS *)NULL);
}

 
static PROCESS *
find_pipeline (pid, alive_only, jobp)
     pid_t pid;
     int alive_only;
     int *jobp;		 
{
  int job;
  PROCESS *p;
  struct pipeline_saver *save;

   
  p = (PROCESS *)NULL;
  if (jobp)
    *jobp = NO_JOB;

  if (the_pipeline && (p = find_pid_in_pipeline (pid, the_pipeline, alive_only)))
    return (p);

   
  for (save = saved_pipeline; save; save = save->next)
    if (save->pipeline && (p = find_pid_in_pipeline (pid, save->pipeline, alive_only)))
      return (p);

#if defined (PROCESS_SUBSTITUTION)
  if (procsubs.nproc > 0 && (p = procsub_search (pid)) && ((alive_only == 0 && PRECYCLED(p) == 0) || PALIVE(p)))
    return (p);
#endif

  job = find_job (pid, alive_only, &p);
  if (jobp)
    *jobp = job;
  return (job == NO_JOB) ? (PROCESS *)NULL : jobs[job]->pipe;
}

 
static PROCESS *
find_process (pid, alive_only, jobp)
     pid_t pid;
     int alive_only;
     int *jobp;		 
{
  PROCESS *p;

  p = find_pipeline (pid, alive_only, jobp);
  while (p && p->pid != pid)
    p = p->next;
  return p;
}

 
static int
find_job (pid, alive_only, procp)
     pid_t pid;
     int alive_only;
     PROCESS **procp;
{
  register int i;
  PROCESS *p;

   
  for (i = 0; i < js.j_jobslots; i++)
    {
      if (i < js.j_firstj && jobs[i])
	INTERNAL_DEBUG (("find_job: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
      if (i > js.j_lastj && jobs[i])
	INTERNAL_DEBUG (("find_job: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

      if (jobs[i])
	{
	  p = jobs[i]->pipe;

	  do
	    {
	      if (p->pid == pid && ((alive_only == 0 && PRECYCLED(p) == 0) || PALIVE(p)))
		{
		  if (procp)
		    *procp = p;
		  return (i);
		}

	      p = p->next;
	    }
	  while (p != jobs[i]->pipe);
	}
    }

  return (NO_JOB);
}

 
int
get_job_by_pid (pid, block, procp)
     pid_t pid;
     int block;
     PROCESS **procp;
{
  int job;
  sigset_t set, oset;

  if (block)
    BLOCK_CHILD (set, oset);

  job = find_job (pid, 0, procp);

  if (block)
    UNBLOCK_CHILD (oset);

  return job;
}

 
void
describe_pid (pid)
     pid_t pid;
{
  int job;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);

  job = find_job (pid, 0, NULL);

  if (job != NO_JOB)
    fprintf (stderr, "[%d] %ld\n", job + 1, (long)pid);
  else
    programming_error (_("describe_pid: %ld: no such pid"), (long)pid);

  UNBLOCK_CHILD (oset);
}

static char *
j_strsignal (s)
     int s;
{
  char *x;

  x = strsignal (s);
  if (x == 0)
    {
      x = retcode_name_buffer;
      snprintf (x, sizeof(retcode_name_buffer), _("Signal %d"), s);
    }
  return x;
}

static char *
printable_job_status (j, p, format)
     int j;
     PROCESS *p;
     int format;
{
  static char *temp;
  int es;

  temp = _("Done");

  if (STOPPED (j) && format == 0)
    {
      if (posixly_correct == 0 || p == 0 || (WIFSTOPPED (p->status) == 0))
	temp = _("Stopped");
      else
	{
	  temp = retcode_name_buffer;
	  snprintf (temp, sizeof(retcode_name_buffer), _("Stopped(%s)"), signal_name (WSTOPSIG (p->status)));
	}
    }
  else if (RUNNING (j))
    temp = _("Running");
  else
    {
      if (WIFSTOPPED (p->status))
	temp = j_strsignal (WSTOPSIG (p->status));
      else if (WIFSIGNALED (p->status))
	temp = j_strsignal (WTERMSIG (p->status));
      else if (WIFEXITED (p->status))
	{
	  temp = retcode_name_buffer;
	  es = WEXITSTATUS (p->status);
	  if (es == 0)
	    {
	      strncpy (temp, _("Done"), sizeof (retcode_name_buffer) - 1);
	      temp[sizeof (retcode_name_buffer) - 1] = '\0';
	    }
	  else if (posixly_correct)
	    snprintf (temp, sizeof(retcode_name_buffer), _("Done(%d)"), es);
	  else
	    snprintf (temp, sizeof(retcode_name_buffer), _("Exit %d"), es);
	}
      else
	temp = _("Unknown status");
    }

  return temp;
}

 

 
static void
print_pipeline (p, job_index, format, stream)
     PROCESS *p;
     int job_index, format;
     FILE *stream;
{
  PROCESS *first, *last, *show;
  int es, name_padding;
  char *temp;

  if (p == 0)
    return;

  first = last = p;
  while (last->next != first)
    last = last->next;

  for (;;)
    {
      if (p != first)
	fprintf (stream, format ? "     " : " |");

      if (format != JLIST_STANDARD)
	fprintf (stream, "%5ld", (long)p->pid);

      fprintf (stream, " ");

      if (format > -1 && job_index >= 0)
	{
	  show = format ? p : last;
	  temp = printable_job_status (job_index, show, format);

	  if (p != first)
	    {
	      if (format)
		{
		  if (show->running == first->running &&
		      WSTATUS (show->status) == WSTATUS (first->status))
		    temp = "";
		}
	      else
		temp = (char *)NULL;
	    }

	  if (temp)
	    {
	      fprintf (stream, "%s", temp);

	      es = STRLEN (temp);
	      if (es == 0)
		es = 2;	 
	      name_padding = LONGEST_SIGNAL_DESC - es;

	      fprintf (stream, "%*s", name_padding, "");

	      if ((WIFSTOPPED (show->status) == 0) &&
		  (WIFCONTINUED (show->status) == 0) &&
		  WIFCORED (show->status))
		fprintf (stream, _("(core dumped) "));
	    }
	}

      if (p != first && format)
	fprintf (stream, "| ");

      if (p->command)
	fprintf (stream, "%s", p->command);

      if (p == last && job_index >= 0)
	{
	  temp = current_working_directory ();

	  if (RUNNING (job_index) && (IS_FOREGROUND (job_index) == 0))
	    fprintf (stream, " &");

	  if (strcmp (temp, jobs[job_index]->wd) != 0)
	    fprintf (stream,
	      _("  (wd: %s)"), polite_directory_format (jobs[job_index]->wd));
	}

      if (format || (p == last))
	{
	   
	  if (asynchronous_notification && interactive)
	    putc ('\r', stream);
	  fprintf (stream, "\n");
	}

      if (p == last)
	break;
      p = p->next;
    }
  fflush (stream);
}

 
static void
pretty_print_job (job_index, format, stream)
     int job_index, format;
     FILE *stream;
{
  register PROCESS *p;

   
  if (format == JLIST_PID_ONLY)
    {
      fprintf (stream, "%ld\n", (long)jobs[job_index]->pipe->pid);
      return;
    }

  if (format == JLIST_CHANGED_ONLY)
    {
      if (IS_NOTIFIED (job_index))
	return;
      format = JLIST_STANDARD;
    }

  if (format != JLIST_NONINTERACTIVE)
    fprintf (stream, "[%d]%c ", job_index + 1,
	      (job_index == js.j_current) ? '+':
		(job_index == js.j_previous) ? '-' : ' ');

  if (format == JLIST_NONINTERACTIVE)
    format = JLIST_LONG;

  p = jobs[job_index]->pipe;

  print_pipeline (p, job_index, format, stream);

   
  jobs[job_index]->flags |= J_NOTIFIED;
}

static int
print_job (job, format, state, job_index)
     JOB *job;
     int format, state, job_index;
{
  if (state == -1 || (JOB_STATE)state == job->state)
    pretty_print_job (job_index, format, stdout);
  return (0);
}

void
list_one_job (job, format, ignore, job_index)
     JOB *job;
     int format, ignore, job_index;
{
  pretty_print_job (job_index, format, stdout);
  cleanup_dead_jobs ();
}

void
list_stopped_jobs (format)
     int format;
{
  cleanup_dead_jobs ();
  map_over_jobs (print_job, format, (int)JSTOPPED);
}

void
list_running_jobs (format)
     int format;
{
  cleanup_dead_jobs ();
  map_over_jobs (print_job, format, (int)JRUNNING);
}

 
void
list_all_jobs (format)
     int format;
{
  cleanup_dead_jobs ();
  map_over_jobs (print_job, format, -1);
}

 
pid_t
make_child (command, flags)
     char *command;
     int flags;
{
  int async_p, forksleep;
  sigset_t set, oset, termset, chldset, oset_copy;
  pid_t pid;
  SigHandler *oterm;

  sigemptyset (&oset_copy);
  sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &oset_copy);
  sigaddset (&oset_copy, SIGTERM);

   
  sigemptyset (&set);
  sigaddset (&set, SIGCHLD);
  sigaddset (&set, SIGINT);
  sigaddset (&set, SIGTERM);

  sigemptyset (&oset);
  sigprocmask (SIG_BLOCK, &set, &oset);

   
  if (interactive_shell)
    oterm = set_signal_handler (SIGTERM, SIG_DFL);

  making_children ();

  async_p = (flags & FORK_ASYNC);
  forksleep = 1;

#if defined (BUFFERED_INPUT)
   
  if (default_buffered_input != -1 &&
      (!async_p || default_buffered_input > 0))
    sync_buffered_stream (default_buffered_input);
#endif  

   
  while ((pid = fork ()) < 0 && errno == EAGAIN && forksleep < FORKSLEEP_MAX)
    {
       
       
      sigprocmask (SIG_SETMASK, &oset_copy, (sigset_t *)NULL);
       
      waitchld (-1, 0);

      errno = EAGAIN;		 
      sys_error ("fork: retry");

      if (sleep (forksleep) != 0)
	break;
      forksleep <<= 1;

      if (interrupt_state)
	break;
      sigprocmask (SIG_SETMASK, &set, (sigset_t *)NULL);
    }

  if (pid != 0)
    if (interactive_shell)
      set_signal_handler (SIGTERM, oterm);

  if (pid < 0)
    {
      sys_error ("fork");

       
      terminate_current_pipeline ();

       
      if (the_pipeline)
	kill_current_pipeline ();

      set_exit_status (EX_NOEXEC);
      throw_to_top_level ();	 
    }

  if (pid == 0)
    {
       
      pid_t mypid;

      subshell_environment |= SUBSHELL_IGNTRAP;

       
      mypid = getpid ();
#if defined (BUFFERED_INPUT)
       
      unset_bash_input (0);
#endif  

      CLRINTERRUPT;	 

       
      restore_sigmask ();
  
      if (job_control)
	{
	   

	  if (pipeline_pgrp == 0)	 
	    pipeline_pgrp = mypid;

	   
	  if (pipeline_pgrp == shell_pgrp)
	    ignore_tty_job_signals ();
	  else
	    default_tty_job_signals ();

	   
	   
	  if (setpgid (mypid, pipeline_pgrp) < 0)
	    sys_error (_("child setpgid (%ld to %ld)"), (long)mypid, (long)pipeline_pgrp);

	   
	  if ((flags & FORK_NOTERM) == 0 && async_p == 0 && pipeline_pgrp != shell_pgrp && ((subshell_environment&(SUBSHELL_ASYNC|SUBSHELL_PIPE)) == 0) && running_in_background == 0)
	    give_terminal_to (pipeline_pgrp, 0);

#if defined (PGRP_PIPE)
	  if (pipeline_pgrp == mypid)
	    pipe_read (pgrp_pipe);
#endif
	}
      else			 
	{
	  if (pipeline_pgrp == 0)
	    pipeline_pgrp = shell_pgrp;

	   

	  default_tty_job_signals ();
	}

#if defined (PGRP_PIPE)
       
      sh_closepipe (pgrp_pipe);
#endif  

       

#if defined (RECYCLES_PIDS)
      if (last_asynchronous_pid == mypid)
	 
	last_asynchronous_pid = 1;
#endif
    }
  else
    {
       

      if (job_control)
	{
	  if (pipeline_pgrp == 0)
	    {
	      pipeline_pgrp = pid;
	       
	       
	    }
	   
	  setpgid (pid, pipeline_pgrp);
	}
      else
	{
	  if (pipeline_pgrp == 0)
	    pipeline_pgrp = shell_pgrp;
	}

       
      add_process (command, pid);

      if (async_p)
	last_asynchronous_pid = pid;
#if defined (RECYCLES_PIDS)
      else if (last_asynchronous_pid == pid)
	 
	last_asynchronous_pid = 1;
#endif

       
      delete_old_job (pid);

       
      bgp_delete (pid);		 

      last_made_pid = pid;

       
      js.c_totforked++;
      js.c_living++;

       
      sigprocmask (SIG_SETMASK, &oset, (sigset_t *)NULL);
    }

  return (pid);
}

 
void
ignore_tty_job_signals ()
{
  set_signal_handler (SIGTSTP, SIG_IGN);
  set_signal_handler (SIGTTIN, SIG_IGN);
  set_signal_handler (SIGTTOU, SIG_IGN);
}

 
void
default_tty_job_signals ()
{
  if (signal_is_trapped (SIGTSTP) == 0 && signal_is_hard_ignored (SIGTSTP))
    set_signal_handler (SIGTSTP, SIG_IGN);
  else
    set_signal_handler (SIGTSTP, SIG_DFL);

  if (signal_is_trapped (SIGTTIN) == 0 && signal_is_hard_ignored (SIGTTIN))
    set_signal_handler (SIGTTIN, SIG_IGN);
  else
    set_signal_handler (SIGTTIN, SIG_DFL);

  if (signal_is_trapped (SIGTTOU) == 0 && signal_is_hard_ignored (SIGTTOU))
    set_signal_handler (SIGTTOU, SIG_IGN);
  else
    set_signal_handler (SIGTTOU, SIG_DFL);
}

 
void
get_original_tty_job_signals ()
{
  static int fetched = 0;

  if (fetched == 0)
    {
      if (interactive_shell)
	{
	  set_original_signal (SIGTSTP, SIG_DFL);
	  set_original_signal (SIGTTIN, SIG_DFL);
	  set_original_signal (SIGTTOU, SIG_DFL);
	}
      else
	{
	  get_original_signal (SIGTSTP);
	  get_original_signal (SIGTTIN);
	  get_original_signal (SIGTTOU);
	}
      fetched = 1;
    }
}

 

static TTYSTRUCT shell_tty_info;

#if defined (NEW_TTY_DRIVER)
static struct tchars shell_tchars;
static struct ltchars shell_ltchars;
#endif  

#if defined (NEW_TTY_DRIVER) && defined (DRAIN_OUTPUT)
 

static int ttspeeds[] =
{
  0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
  1800, 2400, 4800, 9600, 19200, 38400
};

static void
draino (fd, ospeed)
     int fd, ospeed;
{
  register int delay = ttspeeds[ospeed];
  int n;

  if (!delay)
    return;

  while ((ioctl (fd, TIOCOUTQ, &n) == 0) && n)
    {
      if (n > (delay / 100))
	{
	  struct timeval tv;

	  n *= 10;		 
	  tv.tv_sec = n / delay;
	  tv.tv_usec = ((n % delay) * 1000000) / delay;
	  select (fd, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
	}
      else
	break;
    }
}
#endif  

 
#define input_tty() (shell_tty != -1) ? shell_tty : fileno (stderr)

 
int
get_tty_state ()
{
  int tty;

  tty = input_tty ();
  if (tty != -1)
    {
#if defined (NEW_TTY_DRIVER)
      ioctl (tty, TIOCGETP, &shell_tty_info);
      ioctl (tty, TIOCGETC, &shell_tchars);
      ioctl (tty, TIOCGLTC, &shell_ltchars);
#endif  

#if defined (TERMIO_TTY_DRIVER)
      ioctl (tty, TCGETA, &shell_tty_info);
#endif  

#if defined (TERMIOS_TTY_DRIVER)
      if (tcgetattr (tty, &shell_tty_info) < 0)
	{
#if 0
	   
	  if (interactive)
	    sys_error ("[%ld: %d (%d)] tcgetattr", (long)getpid (), shell_level, tty);
#endif
	  return -1;
	}
#endif  
      if (check_window_size)
	get_new_window_size (0, (int *)0, (int *)0);
    }
  return 0;
}

 
int
set_tty_state ()
{
  int tty;

  tty = input_tty ();
  if (tty != -1)
    {
#if defined (NEW_TTY_DRIVER)
#  if defined (DRAIN_OUTPUT)
      draino (tty, shell_tty_info.sg_ospeed);
#  endif  
      ioctl (tty, TIOCSETN, &shell_tty_info);
      ioctl (tty, TIOCSETC, &shell_tchars);
      ioctl (tty, TIOCSLTC, &shell_ltchars);
#endif  

#if defined (TERMIO_TTY_DRIVER)
      ioctl (tty, TCSETAW, &shell_tty_info);
#endif  

#if defined (TERMIOS_TTY_DRIVER)
      if (tcsetattr (tty, TCSADRAIN, &shell_tty_info) < 0)
	{
	   
	  if (interactive)
	    sys_error ("[%ld: %d (%d)] tcsetattr", (long)getpid (), shell_level, tty);
	  return -1;
	}
#endif  
    }
  return 0;
}

 
static PROCESS *
find_last_proc (job, block)
     int job;
     int block;
{
  register PROCESS *p;
  sigset_t set, oset;

  if (block)
    BLOCK_CHILD (set, oset);

  p = jobs[job]->pipe;
  while (p && p->next != jobs[job]->pipe)
    p = p->next;

  if (block)
    UNBLOCK_CHILD (oset);

  return (p);
}

static pid_t
find_last_pid (job, block)
     int job;
     int block;
{
  PROCESS *p;

  p = find_last_proc (job, block);
   
  return p->pid;
}     

 

int
wait_for_single_pid (pid, flags)
     pid_t pid;
     int flags;
{
  register PROCESS *child;
  sigset_t set, oset;
  int r, job, alive;

  BLOCK_CHILD (set, oset);
  child = find_pipeline (pid, 0, (int *)NULL);
  UNBLOCK_CHILD (oset);

  if (child == 0)
    {
      r = bgp_search (pid);
      if (r >= 0)
	return r;
    }

  if (child == 0)
    {
      if (flags & JWAIT_PERROR)
	internal_error (_("wait: pid %ld is not a child of this shell"), (long)pid);
      return (257);
    }

  alive = 0;
  do
    {
      r = wait_for (pid, 0);
      if ((flags & JWAIT_FORCE) == 0)
	break;

      BLOCK_CHILD (set, oset);
      alive = PALIVE (child);
      UNBLOCK_CHILD (oset);
    }
  while (alive);

   
  BLOCK_CHILD (set, oset);
  job = find_job (pid, 0, NULL);
  if (job != NO_JOB && jobs[job] && DEADJOB (job))
    jobs[job]->flags |= J_NOTIFIED;
  UNBLOCK_CHILD (oset);

   
  if (posixly_correct)
    {
      cleanup_dead_jobs ();
      bgp_delete (pid);
    }

   
  CHECK_WAIT_INTR;

  return r;
}

 
int
wait_for_background_pids (ps)
     struct procstat *ps;
{
  register int i, r;
  int any_stopped, check_async, njobs;
  sigset_t set, oset;
  pid_t pid;

  for (njobs = any_stopped = 0, check_async = 1;;)
    {
      BLOCK_CHILD (set, oset);

       
       
      for (i = 0; i < js.j_jobslots; i++)
	{
	  if (i < js.j_firstj && jobs[i])
	    INTERNAL_DEBUG (("wait_for_background_pids: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
	  if (i > js.j_lastj && jobs[i])
	    INTERNAL_DEBUG (("wait_for_background_pids: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

	  if (jobs[i] && STOPPED (i))
	    {
	      builtin_warning ("job %d[%d] stopped", i+1, find_last_pid (i, 0));
	      any_stopped = 1;
	    }

	  if (jobs[i] && RUNNING (i) && IS_FOREGROUND (i) == 0)
	    break;
	}
      if (i == js.j_jobslots)
	{
	  UNBLOCK_CHILD (oset);
	  break;
	}

       
      pid = find_last_pid (i, 0);
      UNBLOCK_CHILD (oset);
      QUIT;
      errno = 0;		 
      r = wait_for_single_pid (pid, JWAIT_PERROR);
      if (ps)
	{
	  ps->pid = pid;
	  ps->status = (r < 0 || r > 256) ? 127 : r;
	}
      if (r == -1 && errno == ECHILD)
	{
	   
	  check_async = 0;
	  mark_all_jobs_as_dead ();
	}
      njobs++;
    }

#if defined (PROCESS_SUBSTITUTION)
  procsub_waitall ();
#endif
      
   
  mark_dead_jobs_as_notified (1);
  cleanup_dead_jobs ();
  bgp_clear ();

  return njobs;
}

 
#define INVALID_SIGNAL_HANDLER (SigHandler *)wait_for_background_pids
static SigHandler *old_sigint_handler = INVALID_SIGNAL_HANDLER;

static int wait_sigint_received;
static int child_caught_sigint;

int waiting_for_child;

 
void
wait_sigint_cleanup ()
{
  queue_sigchld = 0;
  waiting_for_child = 0;
  restore_sigint_handler ();
}

static void
restore_sigint_handler ()
{
  if (old_sigint_handler != INVALID_SIGNAL_HANDLER)
    {
      set_signal_handler (SIGINT, old_sigint_handler);
      old_sigint_handler = INVALID_SIGNAL_HANDLER;
      waiting_for_child = 0;
    }
}

 
static sighandler
wait_sigint_handler (sig)
     int sig;
{
  SigHandler *sigint_handler;

  if (this_shell_builtin && this_shell_builtin == wait_builtin)
    {
      set_exit_status (128+SIGINT);
      restore_sigint_handler ();
       
      if (this_shell_builtin && this_shell_builtin == wait_builtin &&
	  signal_is_trapped (SIGINT) &&
	  ((sigint_handler = trap_to_sighandler (SIGINT)) == trap_handler))
	{
	  trap_handler (SIGINT);	 
	  wait_signal_received = SIGINT;
	  if (wait_intr_flag)
	    sh_longjmp (wait_intr_buf, 1);
	  else
	     
	    SIGRETURN (0);
	}
      else  
	kill (getpid (), SIGINT);
    }

   
  if (waiting_for_child)
    wait_sigint_received = 1;
  else
    {
      set_exit_status (128+SIGINT);
      restore_sigint_handler ();
      kill (getpid (), SIGINT);
    }

   
  SIGRETURN (0);
}

static int
process_exit_signal (status)
     WAIT status;
{
  return (WIFSIGNALED (status) ? WTERMSIG (status) : 0);
}

static int
process_exit_status (status)
     WAIT status;
{
  if (WIFSIGNALED (status))
    return (128 + WTERMSIG (status));
  else if (WIFSTOPPED (status) == 0)
    return (WEXITSTATUS (status));
  else
    return (EXECUTION_SUCCESS);
}

static WAIT
job_signal_status (job)
     int job;
{
  register PROCESS *p;
  WAIT s;

  p = jobs[job]->pipe;
  do
    {
      s = p->status;
      if (WIFSIGNALED(s) || WIFSTOPPED(s))
	break;
      p = p->next;
    }
  while (p != jobs[job]->pipe);

  return s;
}
  
 
static WAIT
raw_job_exit_status (job)
     int job;
{
  register PROCESS *p;
  int fail;
  WAIT ret;

  if (jobs[job]->flags & J_PIPEFAIL)
    {
      fail = 0;
      p = jobs[job]->pipe;
      do
	{
	  if (WSTATUS (p->status) != EXECUTION_SUCCESS)
	    fail = WSTATUS(p->status);
	  p = p->next;
	}
      while (p != jobs[job]->pipe);
      WSTATUS (ret) = fail;
      return ret;
    }

  for (p = jobs[job]->pipe; p->next != jobs[job]->pipe; p = p->next)
    ;
  return (p->status);
}

 
int
job_exit_status (job)
     int job;
{
  return (process_exit_status (raw_job_exit_status (job)));
}

int
job_exit_signal (job)
     int job;
{
  return (process_exit_signal (raw_job_exit_status (job)));
}

#define FIND_CHILD(pid, child) \
  do \
    { \
      child = find_pipeline (pid, 0, (int *)NULL); \
      if (child == 0) \
	{ \
	  give_terminal_to (shell_pgrp, 0); \
	  UNBLOCK_CHILD (oset); \
	  internal_error (_("wait_for: No record of process %ld"), (long)pid); \
	  restore_sigint_handler (); \
	  return (termination_state = 127); \
	} \
    } \
  while (0)

 
int
wait_for (pid, flags)
     pid_t pid;
     int flags;
{
  int job, termination_state, r;
  WAIT s;
  register PROCESS *child;
  sigset_t set, oset;

   
  child = 0;
  BLOCK_CHILD (set, oset);

   

   
  wait_sigint_received = child_caught_sigint = 0;
  if (job_control == 0 || (subshell_environment&SUBSHELL_COMSUB))
    {
      SigHandler *temp_sigint_handler;

      temp_sigint_handler = set_signal_handler (SIGINT, wait_sigint_handler);
      if (temp_sigint_handler == wait_sigint_handler)
	internal_debug ("wait_for: recursively setting old_sigint_handler to wait_sigint_handler: running_trap = %d", running_trap);
      else
	old_sigint_handler = temp_sigint_handler;
      waiting_for_child = 0;
      if (old_sigint_handler == SIG_IGN)
	set_signal_handler (SIGINT, old_sigint_handler);
    }

  termination_state = last_command_exit_value;

  if (interactive && job_control == 0)
    QUIT;
   
  CHECK_TERMSIG;

   
  CHECK_WAIT_INTR;

   

  job = NO_JOB;
  do
    {
      if (pid != ANY_PID)
	FIND_CHILD (pid, child);

       
      if (job == NO_JOB && pid != ANY_PID)	 
	job = find_job (pid, 0, NULL);

       

      if (pid == ANY_PID || PRUNNING(child) || (job != NO_JOB && RUNNING (job)))
	{
	  int old_waiting;

	  queue_sigchld = 1;
	  old_waiting = waiting_for_child;
	  waiting_for_child = 1;
	   
	  CHECK_WAIT_INTR;
	  r = waitchld (pid, 1);	 
	  waiting_for_child = old_waiting;
#if 0
itrace("wait_for: blocking wait for %d returns %d child = %p", (int)pid, r, child);
#endif
	  queue_sigchld = 0;
	  if (r == -1 && errno == ECHILD && this_shell_builtin == wait_builtin)
	    {
	      termination_state = -1;
	       
	      restore_sigint_handler ();
	      goto wait_for_return;
	    }

	   
	  if (r == -1 && errno == ECHILD)
	    {
	      if (child)
		{
		  child->running = PS_DONE;
		  WSTATUS (child->status) = 0;	 
		}
	      js.c_living = 0;		 
	      if (job != NO_JOB)
		{
		  jobs[job]->state = JDEAD;
		  js.c_reaped++;
		  js.j_ndead++;
		}
	      if (pid == ANY_PID)
		{
		  termination_state = -1;
		  break;
		}
	    }
	}

       
      if (interactive && job_control == 0)
	QUIT;
       
      CHECK_TERMSIG;

       
      CHECK_WAIT_INTR;

      if (pid == ANY_PID)
	{
	   
	  restore_sigint_handler ();
	  goto wait_for_return;
	}
    }
  while (PRUNNING (child) || (job != NO_JOB && RUNNING (job)));

   
  restore_sigint_handler ();

   
  termination_state = (job != NO_JOB) ? job_exit_status (job)
				      : (child ? process_exit_status (child->status) : EXECUTION_SUCCESS);
  last_command_exit_signal = (job != NO_JOB) ? job_exit_signal (job)
					     : (child ? process_exit_signal (child->status) : 0);

   
  if ((job != NO_JOB && JOBSTATE (job) == JSTOPPED) || (child && WIFSTOPPED (child->status)))
    termination_state = 128 + WSTOPSIG (child->status);

  if (job == NO_JOB || IS_JOBCONTROL (job))
    {
       
#if 0
if (job == NO_JOB)
  itrace("wait_for: job == NO_JOB, giving the terminal to shell_pgrp (%ld)", (long)shell_pgrp);
#endif
       
      if ((flags & JWAIT_NOTERM) == 0 && running_in_background == 0 &&
	  (job == NO_JOB || IS_ASYNC (job) == 0 || IS_FOREGROUND (job)) &&
	  (subshell_environment & (SUBSHELL_ASYNC|SUBSHELL_PIPE)) == 0)
	give_terminal_to (shell_pgrp, 0);
    }

   
  if (job != NO_JOB)
    {
      if (interactive_shell && subshell_environment == 0)
	{
	   
	  s = job_signal_status (job);

	  if (WIFSIGNALED (s) || WIFSTOPPED (s))
	    {
	      set_tty_state ();

	       
	      if (check_window_size && (job == js.j_current || IS_FOREGROUND (job)))
		get_new_window_size (0, (int *)0, (int *)0);
	    }
	  else
#if defined (READLINE)
	     
	    if (RL_ISSTATE (RL_STATE_COMPLETING|RL_STATE_DISPATCHING|RL_STATE_TERMPREPPED) == 0)
#endif
	    get_tty_state ();

	   
	  if (job_control && IS_JOBCONTROL (job) && IS_FOREGROUND (job) &&
		WIFSIGNALED (s) && WTERMSIG (s) == SIGINT)
	    {
	       
	      if (signal_is_trapped (SIGINT) == 0 && (loop_level || (shell_compatibility_level > 32 && executing_list)))
		ADDINTERRUPT;
	       
	      else if (signal_is_trapped (SIGINT) && loop_level)
		ADDINTERRUPT;
	       
	      else if (interactive_shell && signal_is_trapped (SIGINT) == 0 && sourcelevel)
		ADDINTERRUPT;
	      else
		{
		  putchar ('\n');
		  fflush (stdout);
		}
	    }
	}
      else if ((subshell_environment & (SUBSHELL_COMSUB|SUBSHELL_PIPE)) && wait_sigint_received)
	{
	   
	  if (child_caught_sigint == 0 && signal_is_trapped (SIGINT) == 0)
	    {
	      UNBLOCK_CHILD (oset);
	      old_sigint_handler = set_signal_handler (SIGINT, SIG_DFL);
	      if (old_sigint_handler == SIG_IGN)
		restore_sigint_handler ();
	      else
		kill (getpid (), SIGINT);
	    }
	}
      else if (interactive_shell == 0 && subshell_environment == 0 && IS_FOREGROUND (job))
	{
	  s = job_signal_status (job);

	   
	  if (job_control && IS_JOBCONTROL (job) && WIFSIGNALED (s) && WTERMSIG (s) == SIGINT)
	    {
	      ADDINTERRUPT;	 
	    }

	  if (check_window_size)
	    get_new_window_size (0, (int *)0, (int *)0);
	}

       
      if (DEADJOB (job) && IS_FOREGROUND (job)  )
	setjstatus (job);

       
      notify_and_cleanup ();
    }

wait_for_return:

  UNBLOCK_CHILD (oset);

  return (termination_state);
}

 
int
wait_for_job (job, flags, ps)
     int job, flags;
     struct procstat *ps;
{
  pid_t pid;
  int r, state;
  sigset_t set, oset;

  BLOCK_CHILD(set, oset);
  state = JOBSTATE (job);
  if (state == JSTOPPED)
    internal_warning (_("wait_for_job: job %d is stopped"), job+1);

  pid = find_last_pid (job, 0);
  UNBLOCK_CHILD(oset);

  do
    {
      r = wait_for (pid, 0);
      if (r == -1 && errno == ECHILD)
	mark_all_jobs_as_dead ();

      CHECK_WAIT_INTR;

      if ((flags & JWAIT_FORCE) == 0)
	break;

      BLOCK_CHILD (set, oset);
      state = (job != NO_JOB && jobs[job]) ? JOBSTATE (job) : JDEAD;
      UNBLOCK_CHILD (oset);
    }
  while (state != JDEAD);

   
  BLOCK_CHILD (set, oset);
  if (job != NO_JOB && jobs[job] && DEADJOB (job))
    jobs[job]->flags |= J_NOTIFIED;
  UNBLOCK_CHILD (oset);

  if (ps)
    {
      ps->pid = pid;
      ps->status = (r < 0) ? 127 : r;
    }
  return r;
}

 
int
wait_for_any_job (flags, ps)
     int flags;
     struct procstat *ps;
{
  pid_t pid;
  int i, r;
  sigset_t set, oset;

  if (jobs_list_frozen)
    return -1;

   
  BLOCK_CHILD (set, oset);
  for (i = 0; i < js.j_jobslots; i++)
    {
      if ((flags & JWAIT_WAITING) && jobs[i] && IS_WAITING (i) == 0)
	continue;		 
      if (jobs[i] && DEADJOB (i) && IS_NOTIFIED (i) == 0)
	{
return_job:
	  r = job_exit_status (i);
	  pid = find_last_pid (i, 0);
	  if (ps)
	    {
	      ps->pid = pid;
	      ps->status = r;
	    }
	  notify_of_job_status ();		 
	  delete_job (i, 0);
#if defined (COPROCESS_SUPPORT)
	  coproc_reap ();
#endif
	  UNBLOCK_CHILD (oset);
	  return r;
	}
    }
  UNBLOCK_CHILD (oset);

   
  for (;;)
    {
       
      BLOCK_CHILD (set, oset);
      for (i = 0; i < js.j_jobslots; i++)
	if (jobs[i] && RUNNING (i) && IS_FOREGROUND (i) == 0)
	  break;
      if (i == js.j_jobslots)
	{
	  UNBLOCK_CHILD (oset);
	  return -1;
	}

      UNBLOCK_CHILD (oset);

      QUIT;
      CHECK_TERMSIG;
      CHECK_WAIT_INTR;

      errno = 0;
      r = wait_for (ANY_PID, 0);	 
      if (r == -1 && errno == ECHILD)
	mark_all_jobs_as_dead ();
	
       
      BLOCK_CHILD (set, oset);
      for (i = 0; i < js.j_jobslots; i++)
	{
	  if ((flags & JWAIT_WAITING) && jobs[i] && IS_WAITING (i) == 0)
	    continue;		 
	  if (jobs[i] && DEADJOB (i))
	    goto return_job;
	}
      UNBLOCK_CHILD (oset);
    }

  return -1;
}

 
void
notify_and_cleanup ()
{
  if (jobs_list_frozen)
    return;

  if (interactive || interactive_shell == 0 || sourcelevel)
    notify_of_job_status ();

  cleanup_dead_jobs ();
}

 
void
reap_dead_jobs ()
{
  mark_dead_jobs_as_notified (0);
  cleanup_dead_jobs ();
}

 
static int
most_recent_job_in_state (job, state)
     int job;
     JOB_STATE state;
{
  register int i, result;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);

  for (result = NO_JOB, i = job - 1; i >= 0; i--)
    {
      if (jobs[i] && (JOBSTATE (i) == state))
	{
	  result = i;
	  break;
	}
    }

  UNBLOCK_CHILD (oset);

  return (result);
}

 
static int
job_last_stopped (job)
     int job;
{
  return (most_recent_job_in_state (job, JSTOPPED));
}

 
static int
job_last_running (job)
     int job;
{
  return (most_recent_job_in_state (job, JRUNNING));
}

 
static void
set_current_job (job)
     int job;
{
  int candidate;

  if (js.j_current != job)
    {
      js.j_previous = js.j_current;
      js.j_current = job;
    }

   
  if (js.j_previous != js.j_current &&
      js.j_previous != NO_JOB &&
      jobs[js.j_previous] &&
      STOPPED (js.j_previous))
    return;

   
  candidate = NO_JOB;
  if (STOPPED (js.j_current))
    {
      candidate = job_last_stopped (js.j_current);

      if (candidate != NO_JOB)
	{
	  js.j_previous = candidate;
	  return;
	}
    }

   

  candidate = RUNNING (js.j_current) ? job_last_running (js.j_current)
				    : job_last_running (js.j_jobslots);

  if (candidate != NO_JOB)
    {
      js.j_previous = candidate;
      return;
    }

   
  js.j_previous = js.j_current;
}

 

 

static void
reset_current ()
{
  int candidate;

  if (js.j_jobslots && js.j_current != NO_JOB && jobs[js.j_current] && STOPPED (js.j_current))
    candidate = js.j_current;
  else
    {
      candidate = NO_JOB;

       
      if (js.j_previous != NO_JOB && jobs[js.j_previous] && STOPPED (js.j_previous))
	candidate = js.j_previous;

       
      if (candidate == NO_JOB)
	candidate = job_last_stopped (js.j_jobslots);

       
      if (candidate == NO_JOB)
	candidate = job_last_running (js.j_jobslots);
    }

   
  if (candidate != NO_JOB)
    set_current_job (candidate);
  else
    js.j_current = js.j_previous = NO_JOB;
}

 
static void
set_job_running (job)
     int job;
{
  register PROCESS *p;

   
  p = jobs[job]->pipe;

  do
    {
      if (WIFSTOPPED (p->status))
	p->running = PS_RUNNING;	 
      p = p->next;
    }
  while (p != jobs[job]->pipe);

   
  JOBSTATE (job) = JRUNNING;
}

 
int
start_job (job, foreground)
     int job, foreground;
{
  register PROCESS *p;
  int already_running;
  sigset_t set, oset;
  char *wd, *s;
  static TTYSTRUCT save_stty;

  BLOCK_CHILD (set, oset);

  if ((subshell_environment & SUBSHELL_COMSUB) && (pipeline_pgrp == shell_pgrp))
    {
      internal_error (_("%s: no current jobs"), this_command_name);
      UNBLOCK_CHILD (oset);
      return (-1);
    }

  if (DEADJOB (job))
    {
      internal_error (_("%s: job has terminated"), this_command_name);
      UNBLOCK_CHILD (oset);
      return (-1);
    }

  already_running = RUNNING (job);

  if (foreground == 0 && already_running)
    {
      internal_error (_("%s: job %d already in background"), this_command_name, job + 1);
      UNBLOCK_CHILD (oset);
      return (0);		 
    }

  wd = current_working_directory ();

   
  jobs[job]->flags &= ~J_NOTIFIED;

  if (foreground)
    {
      set_current_job (job);
      jobs[job]->flags |= J_FOREGROUND;
    }

   
  p = jobs[job]->pipe;

  if (foreground == 0)
    {
       
      if (posixly_correct == 0)
	s = (job == js.j_current) ? "+ ": ((job == js.j_previous) ? "- " : " ");       
      else
	s = " ";
      printf ("[%d]%s", job + 1, s);
    }

  do
    {
      printf ("%s%s",
	       p->command ? p->command : "",
	       p->next != jobs[job]->pipe? " | " : "");
      p = p->next;
    }
  while (p != jobs[job]->pipe);

  if (foreground == 0)
    printf (" &");

  if (strcmp (wd, jobs[job]->wd) != 0)
    printf ("	(wd: %s)", polite_directory_format (jobs[job]->wd));

  printf ("\n");

   
  if (already_running == 0)
    set_job_running (job);

   
  if (foreground)
    {
      get_tty_state ();
      save_stty = shell_tty_info;
      jobs[job]->flags &= ~J_ASYNC;	 
       
      if (IS_JOBCONTROL (job))
	give_terminal_to (jobs[job]->pgrp, 0);
    }
  else
    jobs[job]->flags &= ~J_FOREGROUND;

   
  if (already_running == 0)
    {
      jobs[job]->flags |= J_NOTIFIED;
      killpg (jobs[job]->pgrp, SIGCONT);
    }

  if (foreground)
    {
      pid_t pid;
      int st;

      pid = find_last_pid (job, 0);
      UNBLOCK_CHILD (oset);
      st = wait_for (pid, 0);
      shell_tty_info = save_stty;
      set_tty_state ();
      return (st);
    }
  else
    {
      reset_current ();
      UNBLOCK_CHILD (oset);
      return (0);
    }
}

 
int
kill_pid (pid, sig, group)
     pid_t pid;
     int sig, group;
{
  register PROCESS *p;
  int job, result, negative;
  sigset_t set, oset;

  if (pid < -1)
    {
      pid = -pid;
      group = negative = 1;
    }
  else
    negative = 0;

  result = EXECUTION_SUCCESS;
  if (group)
    {
      BLOCK_CHILD (set, oset);
      p = find_pipeline (pid, 0, &job);

      if (job != NO_JOB)
	{
	  jobs[job]->flags &= ~J_NOTIFIED;

	   

	   
	  if (negative && jobs[job]->pgrp == shell_pgrp)
	    result = killpg (pid, sig);
	   
	  else if (jobs[job]->pgrp == shell_pgrp)	 
	    {
	      p = jobs[job]->pipe;
	      do
		{
		  if (PALIVE (p) == 0)
		    continue;		 
		  kill (p->pid, sig);
		  if (PEXITED (p) && (sig == SIGTERM || sig == SIGHUP))
		    kill (p->pid, SIGCONT);
		  p = p->next;
		}
	      while  (p != jobs[job]->pipe);
	    }
	  else
	    {
	      result = killpg (jobs[job]->pgrp, sig);
	      if (p && STOPPED (job) && (sig == SIGTERM || sig == SIGHUP))
		killpg (jobs[job]->pgrp, SIGCONT);
	       
	      if (p && STOPPED (job) && (sig == SIGCONT))
		{
		  set_job_running (job);
		  jobs[job]->flags &= ~J_FOREGROUND;
		  jobs[job]->flags |= J_NOTIFIED;
		}
	    }
	}
      else
	result = killpg (pid, sig);

      UNBLOCK_CHILD (oset);
    }
  else
    result = kill (pid, sig);

  return (result);
}

 
static sighandler
sigchld_handler (sig)
     int sig;
{
  int n, oerrno;

  oerrno = errno;
  REINSTALL_SIGCHLD_HANDLER;
  sigchld++;
  n = 0;
  if (queue_sigchld == 0)
    n = waitchld (-1, 0);
  errno = oerrno;
  SIGRETURN (n);
}

 
static int
waitchld (wpid, block)
     pid_t wpid;
     int block;
{
  WAIT status;
  PROCESS *child;
  pid_t pid;
  int ind;

  int call_set_current, last_stopped_job, job, children_exited, waitpid_flags;
  static int wcontinued = WCONTINUED;	 

  call_set_current = children_exited = 0;
  last_stopped_job = NO_JOB;

  do
    {
       
      waitpid_flags = (job_control && subshell_environment == 0)
			? (WUNTRACED|wcontinued)
			: 0;
      if (sigchld || block == 0)
	waitpid_flags |= WNOHANG;

       
      CHECK_TERMSIG;
       
      CHECK_WAIT_INTR;

      if (block == 1 && queue_sigchld == 0 && (waitpid_flags & WNOHANG) == 0)
	{
	  internal_warning (_("waitchld: turning on WNOHANG to avoid indefinite block"));
	  waitpid_flags |= WNOHANG;
	}

      pid = WAITPID (-1, &status, waitpid_flags);

#if 0
if (wpid != -1 && block)
  itrace("waitchld: blocking waitpid returns %d", pid);
#endif
#if 0
if (wpid != -1)
  itrace("waitchld: %s waitpid returns %d", block?"blocking":"non-blocking", pid);
#endif
       
      if (wcontinued && pid < 0 && errno == EINVAL)
	{
	  wcontinued = 0;
	  continue;	 
	}

       
      if (sigchld > 0 && (waitpid_flags & WNOHANG))
	sigchld--;

       
      if (pid < 0 && errno == ECHILD)
	{
	  if (children_exited == 0)
	    return -1;
	  else
	    break;
	}

#if 0
itrace("waitchld: waitpid returns %d block = %d children_exited = %d", pid, block, children_exited);
#endif
       
      CHECK_TERMSIG;
      CHECK_WAIT_INTR;

       
      if (pid < 0 && errno == EINTR && wait_sigint_received)
	child_caught_sigint = 1;

      if (pid <= 0)
	continue;	 

       
      if (wait_sigint_received && (WIFSIGNALED (status) == 0 || WTERMSIG (status) != SIGINT))
	child_caught_sigint = 1;

       
      if (WIFSIGNALED (status) && WTERMSIG (status) == SIGINT)
	child_caught_sigint = 0;

       
      if (WIFCONTINUED(status) == 0)
	{
	  children_exited++;
	  js.c_living--;
	}

       
      child = find_process (pid, 1, &job);	 

#if defined (COPROCESS_SUPPORT)
      coproc_pidchk (pid, WSTATUS(status));
#endif

#if defined (PROCESS_SUBSTITUTION)
       
      if ((ind = find_procsub_child (pid)) >= 0)
	set_procsub_status (ind, pid, WSTATUS (status));
#endif

       
      if (child == 0)
	{
	  if (WIFEXITED (status) || WIFSIGNALED (status))
	    js.c_reaped++;
	  continue;
	}

       
      child->status = status;
      child->running = WIFCONTINUED(status) ? PS_RUNNING : PS_DONE;

      if (PEXITED (child))
	{
	  js.c_totreaped++;
	  if (job != NO_JOB)
	    js.c_reaped++;
	}

      if (job == NO_JOB)
	continue;

      call_set_current += set_job_status_and_cleanup (job);

      if (STOPPED (job))
	last_stopped_job = job;
      else if (DEADJOB (job) && last_stopped_job == job)
	last_stopped_job = NO_JOB;
    }
  while ((sigchld || block == 0) && pid > (pid_t)0);

   
  if (call_set_current)
    {
      if (last_stopped_job != NO_JOB)
	set_current_job (last_stopped_job);
      else
	reset_current ();
    }

   
  if (children_exited &&
      (signal_is_trapped (SIGCHLD) || trap_list[SIGCHLD] == (char *)IMPOSSIBLE_TRAP_HANDLER) &&
      trap_list[SIGCHLD] != (char *)IGNORE_SIG)
    {
      if (posixly_correct && this_shell_builtin && this_shell_builtin == wait_builtin)
	{
	   
	  queue_sigchld_trap (children_exited);
	  wait_signal_received = SIGCHLD;
	   
	  if (sigchld == 0 && wait_intr_flag)
	    sh_longjmp (wait_intr_buf, 1);
	}
       
      else if (sigchld)	 
	queue_sigchld_trap (children_exited);
      else if (signal_in_progress (SIGCHLD))
	queue_sigchld_trap (children_exited);     
      else if (trap_list[SIGCHLD] == (char *)IMPOSSIBLE_TRAP_HANDLER)
	queue_sigchld_trap (children_exited);
      else if (running_trap)
	queue_sigchld_trap (children_exited);
      else if (this_shell_builtin == wait_builtin)
	run_sigchld_trap (children_exited);	 
      else
	queue_sigchld_trap (children_exited);
    }

   
  if (asynchronous_notification && interactive && executing_builtin == 0)
    notify_of_job_status ();

  return (children_exited);
}

 
static int
set_job_status_and_cleanup (job)
     int job;
{
  PROCESS *child;
  int tstatus, job_state, any_stopped, any_tstped, call_set_current;
  SigHandler *temp_handler;

  child = jobs[job]->pipe;
  jobs[job]->flags &= ~J_NOTIFIED;

  call_set_current = 0;

   

   
  job_state = any_stopped = any_tstped = 0;
  do
    {
      job_state |= PRUNNING (child);
#if 0
      if (PEXITED (child) && (WIFSTOPPED (child->status)))
#else
       
      if (PSTOPPED (child))
#endif
	{
	  any_stopped = 1;
	  any_tstped |= job_control && (WSTOPSIG (child->status) == SIGTSTP);
	}
      child = child->next;
    }
  while (child != jobs[job]->pipe);

   
  if (job_state != 0 && JOBSTATE(job) != JSTOPPED)
    return 0;

   

   
  if (any_stopped)
    {
      jobs[job]->state = JSTOPPED;
      jobs[job]->flags &= ~J_FOREGROUND;
      call_set_current++;
       
      if (any_tstped && loop_level)
	breaking = loop_level;
    }
  else if (job_state != 0)	 
    {
      jobs[job]->state = JRUNNING;
      call_set_current++;
    }
  else
    {
      jobs[job]->state = JDEAD;
      js.j_ndead++;

#if 0
      if (IS_FOREGROUND (job))
	setjstatus (job);
#endif

       
      if (jobs[job]->j_cleanup)
	{
	  (*jobs[job]->j_cleanup) (jobs[job]->cleanarg);
	  jobs[job]->j_cleanup = (sh_vptrfunc_t *)NULL;
	}
    }

   

  if (JOBSTATE (job) == JDEAD)
    {
       
      if (wait_sigint_received && interactive_shell == 0 &&
	  child_caught_sigint && IS_FOREGROUND (job) &&
	  signal_is_trapped (SIGINT))
	{
	  int old_frozen;
	  wait_sigint_received = 0;
	  last_command_exit_value = process_exit_status (child->status);

	  old_frozen = jobs_list_frozen;
	  jobs_list_frozen = 1;
	  tstatus = maybe_call_trap_handler (SIGINT);
	  jobs_list_frozen = old_frozen;
	}

       
      else if (wait_sigint_received &&
	      child_caught_sigint == 0 &&
	      IS_FOREGROUND (job) && IS_JOBCONTROL (job) == 0)
	{
	  int old_frozen;

	  wait_sigint_received = 0;

	   
	  if (signal_is_trapped (SIGINT))
	    last_command_exit_value = process_exit_status (child->status);

	   
	  old_frozen = jobs_list_frozen;
	  jobs_list_frozen = 1;
	  tstatus = maybe_call_trap_handler (SIGINT);
	  jobs_list_frozen = old_frozen;
	  if (tstatus == 0 && old_sigint_handler != INVALID_SIGNAL_HANDLER)
	    {
	       

	      temp_handler = old_sigint_handler;

	       
	      if (temp_handler == trap_handler && signal_is_trapped (SIGINT) == 0)
		  temp_handler = trap_to_sighandler (SIGINT);
	      restore_sigint_handler ();
	      if (temp_handler == SIG_DFL)
		termsig_handler (SIGINT);	 
	      else if (temp_handler != SIG_IGN)
		(*temp_handler) (SIGINT);
	    }
	}
    }

  return call_set_current;
}

 
static void
setjstatus (j)
     int j;
{
#if defined (ARRAY_VARS)
  register int i;
  register PROCESS *p;

  for (i = 1, p = jobs[j]->pipe; p->next != jobs[j]->pipe; p = p->next, i++)
    ;
  i++;
  if (statsize < i)
    {
      pstatuses = (int *)xrealloc (pstatuses, i * sizeof (int));
      statsize = i;
    }
  i = 0;
  p = jobs[j]->pipe;
  do
    {
      pstatuses[i++] = process_exit_status (p->status);
      p = p->next;
    }
  while (p != jobs[j]->pipe);

  pstatuses[i] = -1;	 
  set_pipestatus_array (pstatuses, i);
#endif
}

void
run_sigchld_trap (nchild)
     int nchild;
{
  char *trap_command;
  int i;

   
  trap_command = savestring (trap_list[SIGCHLD]);

  begin_unwind_frame ("SIGCHLD trap");
  unwind_protect_int (last_command_exit_value);
  unwind_protect_int (last_command_exit_signal);
  unwind_protect_var (last_made_pid);
  unwind_protect_int (jobs_list_frozen);
  unwind_protect_pointer (the_pipeline);
  unwind_protect_pointer (subst_assign_varlist);
  unwind_protect_pointer (this_shell_builtin);
  unwind_protect_pointer (temporary_env);

   
  add_unwind_protect (xfree, trap_command);
  add_unwind_protect (maybe_set_sigchld_trap, trap_command);

  subst_assign_varlist = (WORD_LIST *)NULL;
  the_pipeline = (PROCESS *)NULL;
  temporary_env = 0;	 

  running_trap = SIGCHLD + 1;

  set_impossible_sigchld_trap ();
  jobs_list_frozen = 1;
  for (i = 0; i < nchild; i++)
    {
      parse_and_execute (savestring (trap_command), "trap", SEVAL_NOHIST|SEVAL_RESETLINE|SEVAL_NOOPTIMIZE);
    }

  run_unwind_frame ("SIGCHLD trap");
  running_trap = 0;
}

 
static void
notify_of_job_status ()
{
  register int job, termsig;
  char *dir;
  sigset_t set, oset;
  WAIT s;

  if (jobs == 0 || js.j_jobslots == 0)
    return;

  if (old_ttou != 0)
    {
      sigemptyset (&set);
      sigaddset (&set, SIGCHLD);
      sigaddset (&set, SIGTTOU);
      sigemptyset (&oset);
      sigprocmask (SIG_BLOCK, &set, &oset);
    }
  else
    queue_sigchld++;

   
  for (job = 0, dir = (char *)NULL; job < js.j_jobslots; job++)
    {
      if (jobs[job] && IS_NOTIFIED (job) == 0)
	{
	  s = raw_job_exit_status (job);
	  termsig = WTERMSIG (s);

	   
	  if (startup_state == 0 && WIFSIGNALED (s) == 0 &&
		((DEADJOB (job) && IS_FOREGROUND (job) == 0) || STOPPED (job)))
	    continue;
	  
	   
	  if ((job_control == 0 && interactive_shell) ||
	      (startup_state == 2 && (subshell_environment & SUBSHELL_COMSUB)) ||
	      (startup_state == 2 && posixly_correct && (subshell_environment & SUBSHELL_COMSUB) == 0))
	    {
	       
	      if (DEADJOB (job) && (interactive_shell || (find_last_pid (job, 0) != last_asynchronous_pid)))
		jobs[job]->flags |= J_NOTIFIED;
	      continue;
	    }

	   
	  switch (JOBSTATE (job))
	    {
	    case JDEAD:
	      if (interactive_shell == 0 && termsig && WIFSIGNALED (s) &&
		  termsig != SIGINT &&
#if defined (DONT_REPORT_SIGTERM)
		  termsig != SIGTERM &&
#endif
#if defined (DONT_REPORT_SIGPIPE)
		  termsig != SIGPIPE &&
#endif
		  signal_is_trapped (termsig) == 0)
		{
		   
		  fprintf (stderr, _("%s: line %d: "), get_name_for_error (), (line_number == 0) ? 1 : line_number);
		  pretty_print_job (job, JLIST_NONINTERACTIVE, stderr);
		}
	      else if (IS_FOREGROUND (job))
		{
#if !defined (DONT_REPORT_SIGPIPE)
		  if (termsig && WIFSIGNALED (s) && termsig != SIGINT)
#else
		  if (termsig && WIFSIGNALED (s) && termsig != SIGINT && termsig != SIGPIPE)
#endif
		    {
		      fprintf (stderr, "%s", j_strsignal (termsig));

		      if (WIFCORED (s))
			fprintf (stderr, _(" (core dumped)"));

		      fprintf (stderr, "\n");
		    }
		}
	      else if (job_control)	 
		{
		  if (dir == 0)
		    dir = current_working_directory ();
		  pretty_print_job (job, JLIST_STANDARD, stderr);
		  if (dir && strcmp (dir, jobs[job]->wd) != 0)
		    fprintf (stderr,
			     _("(wd now: %s)\n"), polite_directory_format (dir));
		}

	      jobs[job]->flags |= J_NOTIFIED;
	      break;

	    case JSTOPPED:
	      fprintf (stderr, "\n");
	      if (dir == 0)
		dir = current_working_directory ();
	      pretty_print_job (job, JLIST_STANDARD, stderr);
	      if (dir && (strcmp (dir, jobs[job]->wd) != 0))
		fprintf (stderr,
			 _("(wd now: %s)\n"), polite_directory_format (dir));
	      jobs[job]->flags |= J_NOTIFIED;
	      break;

	    case JRUNNING:
	    case JMIXED:
	      break;

	    default:
	      programming_error ("notify_of_job_status");
	    }
	}
    }
  if (old_ttou != 0)
    sigprocmask (SIG_SETMASK, &oset, (sigset_t *)NULL);
  else
    queue_sigchld--;
}

 
int
initialize_job_control (force)
     int force;
{
  pid_t t;
  int t_errno, tty_sigs;

  t_errno = -1;
  shell_pgrp = getpgid (0);

  if (shell_pgrp == -1)
    {
      sys_error (_("initialize_job_control: getpgrp failed"));
      exit (1);
    }

   
  if (interactive == 0 && force == 0)
    {
      job_control = 0;
      original_pgrp = NO_PID;
      shell_tty = fileno (stderr);
      terminal_pgrp = tcgetpgrp (shell_tty);	 
    }
  else
    {
      shell_tty = -1;

       
      if (forced_interactive && isatty (fileno (stderr)) == 0)
	shell_tty = open ("/dev/tty", O_RDWR|O_NONBLOCK);

       
      if (shell_tty == -1)
	shell_tty = dup (fileno (stderr));	 

      if (shell_tty != -1)
	shell_tty = move_to_high_fd (shell_tty, 1, -1);

       
      if (shell_pgrp == 0)
	{
	  shell_pgrp = getpid ();
	  setpgid (0, shell_pgrp);
	  if (shell_tty != -1)
	    tcsetpgrp (shell_tty, shell_pgrp);
	}

      tty_sigs = 0;
      while ((terminal_pgrp = tcgetpgrp (shell_tty)) != -1)
	{
	  if (shell_pgrp != terminal_pgrp)
	    {
	      SigHandler *ottin;

	      CHECK_TERMSIG;
	      ottin = set_signal_handler (SIGTTIN, SIG_DFL);
	      kill (0, SIGTTIN);
	      set_signal_handler (SIGTTIN, ottin);
	      if (tty_sigs++ > 16)
		{
		  sys_error (_("initialize_job_control: no job control in background"));
		  job_control = 0;
		  original_pgrp = terminal_pgrp;	 
		  goto just_bail;
		}
	      continue;
	    }
	  break;
	}

      if (terminal_pgrp == -1)
	t_errno = errno;

       
      if (set_new_line_discipline (shell_tty) < 0)
	{
	  sys_error (_("initialize_job_control: line discipline"));
	  job_control = 0;
	}
      else
	{
	  original_pgrp = shell_pgrp;
	  shell_pgrp = getpid ();

	  if ((original_pgrp != shell_pgrp) && (setpgid (0, shell_pgrp) < 0))
	    {
	      sys_error (_("initialize_job_control: setpgid"));
	      shell_pgrp = original_pgrp;
	    }

	  job_control = 1;

	   
	  if (shell_pgrp != original_pgrp && shell_pgrp != terminal_pgrp)
	    {
	      if (give_terminal_to (shell_pgrp, 0) < 0)
		{
		  t_errno = errno;
		  setpgid (0, original_pgrp);
		  shell_pgrp = original_pgrp;
		  errno = t_errno;
		  sys_error (_("cannot set terminal process group (%d)"), shell_pgrp);
		  job_control = 0;
		}
	    }

	  if (job_control && ((t = tcgetpgrp (shell_tty)) == -1 || t != shell_pgrp))
	    {
	      if (t_errno != -1)
		errno = t_errno;
	      sys_error (_("cannot set terminal process group (%d)"), t);
	      job_control = 0;
	    }
	}
      if (job_control == 0)
	internal_error (_("no job control in this shell"));
    }

just_bail:
  running_in_background = terminal_pgrp != shell_pgrp;

  if (shell_tty != fileno (stderr))
    SET_CLOSE_ON_EXEC (shell_tty);

  set_signal_handler (SIGCHLD, sigchld_handler);

  change_flag ('m', job_control ? '-' : '+');

  if (interactive)
    get_tty_state ();

  set_maxchild (0);

  return job_control;
}

#ifdef DEBUG
void
debug_print_pgrps ()
{
  itrace("original_pgrp = %ld shell_pgrp = %ld terminal_pgrp = %ld",
	 (long)original_pgrp, (long)shell_pgrp, (long)terminal_pgrp);
  itrace("tcgetpgrp(%d) -> %ld, getpgid(0) -> %ld",
	 shell_tty, (long)tcgetpgrp (shell_tty), (long)getpgid(0));
  itrace("pipeline_pgrp -> %ld", (long)pipeline_pgrp);
}
#endif

 
static int
set_new_line_discipline (tty)
     int tty;
{
#if defined (NEW_TTY_DRIVER)
  int ldisc;

  if (ioctl (tty, TIOCGETD, &ldisc) < 0)
    return (-1);

  if (ldisc != NTTYDISC)
    {
      ldisc = NTTYDISC;

      if (ioctl (tty, TIOCSETD, &ldisc) < 0)
	return (-1);
    }
  return (0);
#endif  

#if defined (TERMIO_TTY_DRIVER)
#  if defined (TERMIO_LDISC) && (NTTYDISC)
  if (ioctl (tty, TCGETA, &shell_tty_info) < 0)
    return (-1);

  if (shell_tty_info.c_line != NTTYDISC)
    {
      shell_tty_info.c_line = NTTYDISC;
      if (ioctl (tty, TCSETAW, &shell_tty_info) < 0)
	return (-1);
    }
#  endif  
  return (0);
#endif  

#if defined (TERMIOS_TTY_DRIVER)
#  if defined (TERMIOS_LDISC) && defined (NTTYDISC)
  if (tcgetattr (tty, &shell_tty_info) < 0)
    return (-1);

  if (shell_tty_info.c_line != NTTYDISC)
    {
      shell_tty_info.c_line = NTTYDISC;
      if (tcsetattr (tty, TCSADRAIN, &shell_tty_info) < 0)
	return (-1);
    }
#  endif  
  return (0);
#endif  

#if !defined (NEW_TTY_DRIVER) && !defined (TERMIO_TTY_DRIVER) && !defined (TERMIOS_TTY_DRIVER)
  return (-1);
#endif
}

 
void
initialize_job_signals ()
{
  if (interactive)
    {
      set_signal_handler (SIGINT, sigint_sighandler);
      set_signal_handler (SIGTSTP, SIG_IGN);
      set_signal_handler (SIGTTOU, SIG_IGN);
      set_signal_handler (SIGTTIN, SIG_IGN);
    }
  else if (job_control)
    {
      old_tstp = set_signal_handler (SIGTSTP, sigstop_sighandler);
      old_ttin = set_signal_handler (SIGTTIN, sigstop_sighandler);
      old_ttou = set_signal_handler (SIGTTOU, sigstop_sighandler);
    }
   
}

 
static sighandler
sigcont_sighandler (sig)
     int sig;
{
  initialize_job_signals ();
  set_signal_handler (SIGCONT, old_cont);
  kill (getpid (), SIGCONT);

  SIGRETURN (0);
}

 
static sighandler
sigstop_sighandler (sig)
     int sig;
{
  set_signal_handler (SIGTSTP, old_tstp);
  set_signal_handler (SIGTTOU, old_ttou);
  set_signal_handler (SIGTTIN, old_ttin);

  old_cont = set_signal_handler (SIGCONT, sigcont_sighandler);

  give_terminal_to (shell_pgrp, 0);

  kill (getpid (), sig);

  SIGRETURN (0);
}

 
int
give_terminal_to (pgrp, force)
     pid_t pgrp;
     int force;
{
  sigset_t set, oset;
  int r, e;

  r = 0;
  if (job_control || force)
    {
      sigemptyset (&set);
      sigaddset (&set, SIGTTOU);
      sigaddset (&set, SIGTTIN);
      sigaddset (&set, SIGTSTP);
      sigaddset (&set, SIGCHLD);
      sigemptyset (&oset);
      sigprocmask (SIG_BLOCK, &set, &oset);

      if (tcsetpgrp (shell_tty, pgrp) < 0)
	{
	   
#if 0
	  sys_error ("tcsetpgrp(%d) failed: pid %ld to pgrp %ld",
	    shell_tty, (long)getpid(), (long)pgrp);
#endif
	  r = -1;
	  e = errno;
	}
      else
	terminal_pgrp = pgrp;
      sigprocmask (SIG_SETMASK, &oset, (sigset_t *)NULL);
    }

  if (r == -1)
    errno = e;

  return r;
}

 
static int
maybe_give_terminal_to (opgrp, npgrp, flags)
     pid_t opgrp, npgrp;
     int flags;
{
  int tpgrp;

  tpgrp = tcgetpgrp (shell_tty);
  if (tpgrp < 0 && errno == ENOTTY)
    return -1;
  if (tpgrp == npgrp)
    {
      terminal_pgrp = npgrp;
      return 0;
    }
  else if (tpgrp != opgrp)
    {
      internal_debug ("%d: maybe_give_terminal_to: terminal pgrp == %d shell pgrp = %d new pgrp = %d in_background = %d", (int)getpid(), tpgrp, opgrp, npgrp, running_in_background);
      return -1;
    }
  else
    return (give_terminal_to (npgrp, flags));     
}

 
void
delete_all_jobs (running_only)
     int running_only;
{
  register int i;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);

   
  if (js.j_jobslots)
    {
      js.j_current = js.j_previous = NO_JOB;

       
      for (i = 0; i < js.j_jobslots; i++)
	{
	  if (i < js.j_firstj && jobs[i])
	    INTERNAL_DEBUG (("delete_all_jobs: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
	  if (i > js.j_lastj && jobs[i])
	    INTERNAL_DEBUG (("delete_all_jobs: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

	  if (jobs[i] && (running_only == 0 || (running_only && RUNNING(i))))
	     
	    delete_job (i, DEL_WARNSTOPPED|DEL_NOBGPID);
	}
      if (running_only == 0)
	{
	  free ((char *)jobs);
	  js.j_jobslots = 0;
	  js.j_firstj = js.j_lastj = js.j_njobs = 0;
	}
    }

  if (running_only == 0)
    bgp_clear ();

  UNBLOCK_CHILD (oset);
}

 
void
nohup_all_jobs (running_only)
     int running_only;
{
  register int i;
  sigset_t set, oset;

  BLOCK_CHILD (set, oset);

  if (js.j_jobslots)
    {
       
      for (i = 0; i < js.j_jobslots; i++)
	if (jobs[i] && (running_only == 0 || (running_only && RUNNING(i))))
	  nohup_job (i);
    }

  UNBLOCK_CHILD (oset);
}

int
count_all_jobs ()
{
  int i, n;
  sigset_t set, oset;

   
  BLOCK_CHILD (set, oset);
   
  for (i = n = 0; i < js.j_jobslots; i++)
    {
      if (i < js.j_firstj && jobs[i])
	INTERNAL_DEBUG (("count_all_jobs: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
      if (i > js.j_lastj && jobs[i])
	INTERNAL_DEBUG (("count_all_jobs: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

      if (jobs[i] && DEADJOB(i) == 0)
	n++;
    }
  UNBLOCK_CHILD (oset);
  return n;
}

static void
mark_all_jobs_as_dead ()
{
  register int i;
  sigset_t set, oset;

  if (js.j_jobslots == 0)
    return;

  BLOCK_CHILD (set, oset);

   
  for (i = 0; i < js.j_jobslots; i++)
    if (jobs[i])
      {
	jobs[i]->state = JDEAD;
	js.j_ndead++;
      }

  UNBLOCK_CHILD (oset);
}

 
static void
mark_dead_jobs_as_notified (force)
     int force;
{
  register int i, ndead, ndeadproc;
  sigset_t set, oset;

  if (js.j_jobslots == 0)
    return;

  BLOCK_CHILD (set, oset);

   
  if (force)
    {
     
      for (i = 0; i < js.j_jobslots; i++)
	{
	  if (jobs[i] && DEADJOB (i) && (interactive_shell || (find_last_pid (i, 0) != last_asynchronous_pid)))
	    jobs[i]->flags |= J_NOTIFIED;
	}
      UNBLOCK_CHILD (oset);
      return;
    }

   

   
   
  for (i = ndead = ndeadproc = 0; i < js.j_jobslots; i++)
    {
      if (i < js.j_firstj && jobs[i])
	INTERNAL_DEBUG (("mark_dead_jobs_as_notified: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
      if (i > js.j_lastj && jobs[i])
	INTERNAL_DEBUG (("mark_dead_jobs_as_notified: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

      if (jobs[i] && DEADJOB (i))
	{
	  ndead++;
	  ndeadproc += processes_in_job (i);
	}
    }

# if 0
  if (ndeadproc != js.c_reaped)
    itrace("mark_dead_jobs_as_notified: ndeadproc (%d) != js.c_reaped (%d)", ndeadproc, js.c_reaped);
# endif
  if (ndead != js.j_ndead)
    INTERNAL_DEBUG (("mark_dead_jobs_as_notified: ndead (%d) != js.j_ndead (%d)", ndead, js.j_ndead));

  if (js.c_childmax < 0)
    set_maxchild (0);

   
  if (ndeadproc <= js.c_childmax)
    {
      UNBLOCK_CHILD (oset);
      return;
    }

#if 0
itrace("mark_dead_jobs_as_notified: child_max = %d ndead = %d ndeadproc = %d", js.c_childmax, ndead, ndeadproc);
#endif

   
   
  for (i = 0; i < js.j_jobslots; i++)
    {
      if (jobs[i] && DEADJOB (i) && (interactive_shell || (find_last_pid (i, 0) != last_asynchronous_pid)))
	{
	  if (i < js.j_firstj && jobs[i])
	    INTERNAL_DEBUG (("mark_dead_jobs_as_notified: job %d non-null before js.j_firstj (%d)", i, js.j_firstj));
	  if (i > js.j_lastj && jobs[i])
	    INTERNAL_DEBUG (("mark_dead_jobs_as_notified: job %d non-null after js.j_lastj (%d)", i, js.j_lastj));

	   
	  if ((ndeadproc -= processes_in_job (i)) <= js.c_childmax)
	    break;
	  jobs[i]->flags |= J_NOTIFIED;
	}
    }

  UNBLOCK_CHILD (oset);
}

 
int
freeze_jobs_list ()
{
  int o;

  o = jobs_list_frozen;
  jobs_list_frozen = 1;
  return o;
}

void
unfreeze_jobs_list ()
{
  jobs_list_frozen = 0;
}

void
set_jobs_list_frozen (s)
     int s;
{
  jobs_list_frozen = s;
}

 
int
set_job_control (arg)
     int arg;
{
  int old;

  old = job_control;
  job_control = arg;

  if (terminal_pgrp == NO_PID && shell_tty >= 0)
    terminal_pgrp = tcgetpgrp (shell_tty);

   
  if (job_control != old && job_control)
    shell_pgrp = getpgid (0);  

  running_in_background = (terminal_pgrp != shell_pgrp);

#if 0
  if (interactive_shell == 0 && running_in_background == 0 && job_control != old)
    {
      if (job_control)
	initialize_job_signals ();
      else
	default_tty_job_signals ();
    }
#endif

   
  if (job_control != old && job_control)
    pipeline_pgrp = 0;

  return (old);
}

 
void
without_job_control ()
{
  stop_making_children ();
  start_pipeline ();
#if defined (PGRP_PIPE)
  sh_closepipe (pgrp_pipe);
#endif
  delete_all_jobs (0);
  set_job_control (0);
}

 
void
end_job_control ()
{
  if (job_control)
    terminate_stopped_jobs ();

  if (original_pgrp >= 0 && terminal_pgrp != original_pgrp)
    give_terminal_to (original_pgrp, 1);

  if (original_pgrp >= 0 && setpgid (0, original_pgrp) == 0)
    shell_pgrp = original_pgrp;
}

 
void
restart_job_control ()
{
  if (shell_tty != -1)
    close (shell_tty);
  initialize_job_control (0);
}

 
void
set_maxchild (nchild)
     int nchild;
{
  static int lmaxchild = -1;

   
  if (lmaxchild < 0)
    {
      errno = 0;
      lmaxchild = getmaxchild ();
      if (lmaxchild < 0 && errno == 0)
        lmaxchild = MAX_CHILD_MAX;		 
    }
  if (lmaxchild < 0)
    lmaxchild = DEFAULT_CHILD_MAX;

   
  if (nchild < lmaxchild)
    nchild = lmaxchild;
  else if (nchild > MAX_CHILD_MAX)
    nchild = MAX_CHILD_MAX;

  js.c_childmax = nchild;
}

 
void
set_sigchld_handler ()
{
  set_signal_handler (SIGCHLD, sigchld_handler);
}

#if defined (PGRP_PIPE)
 
static void
pipe_read (pp)
     int *pp;
{
  char ch;

  if (pp[1] >= 0)
    {
      close (pp[1]);
      pp[1] = -1;
    }

  if (pp[0] >= 0)
    {
      while (read (pp[0], &ch, 1) == -1 && errno == EINTR)
	;
    }
}

 
void
close_pgrp_pipe ()
{
  sh_closepipe (pgrp_pipe);
}

void
save_pgrp_pipe (p, clear)
     int *p;
     int clear;
{
  p[0] = pgrp_pipe[0];
  p[1] = pgrp_pipe[1];
  if (clear)
    pgrp_pipe[0] = pgrp_pipe[1] = -1;
}

void
restore_pgrp_pipe (p)
     int *p;
{
  pgrp_pipe[0] = p[0];
  pgrp_pipe[1] = p[1];
}

#endif  
