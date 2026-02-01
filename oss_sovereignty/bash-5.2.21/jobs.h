 

 

#if !defined (_JOBS_H_)
#  define _JOBS_H_

#include "quit.h"
#include "siglist.h"

#include "stdc.h"

#include "posixwait.h"

 
#define JLIST_STANDARD       0
#define JLIST_LONG	     1
#define JLIST_PID_ONLY	     2
#define JLIST_CHANGED_ONLY   3
#define JLIST_NONINTERACTIVE 4

 
#define LONGEST_SIGNAL_DESC 24

 
#define JWAIT_PERROR		(1 << 0)
#define JWAIT_FORCE		(1 << 1)
#define JWAIT_NOWAIT		(1 << 2)  
#define JWAIT_WAITING		(1 << 3)  

 
#define JWAIT_NOTERM		(1 << 8)  

 
#define FORKSLEEP_MAX	16

 

 
#define PS_DONE		0
#define PS_RUNNING	1
#define PS_STOPPED	2
#define PS_RECYCLED	4

 
typedef struct process {
  struct process *next;	 
  pid_t pid;		 
  WAIT status;		 
  int running;		 
  char *command;	 
} PROCESS;

struct pipeline_saver {
  struct process *pipeline;
  struct pipeline_saver *next;
};

 
#define PSTOPPED(p)	(WIFSTOPPED((p)->status))
#define PRUNNING(p)	((p)->running == PS_RUNNING)
#define PALIVE(p)	(PRUNNING(p) || PSTOPPED(p))

#define PEXITED(p)	((p)->running == PS_DONE)
#if defined (RECYCLES_PIDS)
#  define PRECYCLED(p)	((p)->running == PS_RECYCLED)
#else
#  define PRECYCLED(p)	(0)
#endif
#define PDEADPROC(p)	(PEXITED(p) || PRECYCLED(p))

#define get_job_by_jid(ind)	(jobs[(ind)])

 
typedef enum { JNONE = -1, JRUNNING = 1, JSTOPPED = 2, JDEAD = 4, JMIXED = 8 } JOB_STATE;
#define JOBSTATE(job)	(jobs[(job)]->state)
#define J_JOBSTATE(j)	((j)->state)

#define STOPPED(j)	(jobs[(j)]->state == JSTOPPED)
#define RUNNING(j)	(jobs[(j)]->state == JRUNNING)
#define DEADJOB(j)	(jobs[(j)]->state == JDEAD)

#define INVALID_JOB(j)	((j) < 0 || (j) >= js.j_jobslots || get_job_by_jid(j) == 0)

 
#define J_FOREGROUND 0x01  
#define J_NOTIFIED   0x02  
#define J_JOBCONTROL 0x04  
#define J_NOHUP      0x08  
#define J_STATSAVED  0x10  
#define J_ASYNC	     0x20  
#define J_PIPEFAIL   0x40  
#define J_WAITING    0x80  

#define IS_FOREGROUND(j)	((jobs[j]->flags & J_FOREGROUND) != 0)
#define IS_NOTIFIED(j)		((jobs[j]->flags & J_NOTIFIED) != 0)
#define IS_JOBCONTROL(j)	((jobs[j]->flags & J_JOBCONTROL) != 0)
#define IS_ASYNC(j)		((jobs[j]->flags & J_ASYNC) != 0)
#define IS_WAITING(j)		((jobs[j]->flags & J_WAITING) != 0)

typedef struct job {
  char *wd;	    
  PROCESS *pipe;    
  pid_t pgrp;	    
  JOB_STATE state;  
  int flags;	    
#if defined (JOB_CONTROL)
  COMMAND *deferred;	 
  sh_vptrfunc_t *j_cleanup;  
  PTR_T cleanarg;	 
#endif  
} JOB;

struct jobstats {
   
  long c_childmax;
   
  int c_living;		 
  int c_reaped;		 
  int c_injobs;		 
   
  int c_totforked;	 
  int c_totreaped;	 
   
  int j_jobslots;	 
  int j_lastj;		 
  int j_firstj;		 
  int j_njobs;		 
  int j_ndead;		 
   
  int j_current;	 
  int j_previous;	 
   
  JOB *j_lastmade;	 
  JOB *j_lastasync;	 
};

 
typedef pid_t ps_index_t;

struct pidstat {
  ps_index_t bucket_next;
  ps_index_t bucket_prev;

  pid_t pid;
  bits16_t status;		 
};

struct bgpids {
  struct pidstat *storage;	 

  ps_index_t head;
  ps_index_t nalloc;

  int npid;
};

#define NO_PIDSTAT (ps_index_t)-1

 
struct procstat {
  pid_t pid;
  bits16_t status;
};

 
struct procchain {
  PROCESS *head;
  PROCESS *end;
  int nproc;
};

#define NO_JOB  -1	 
#define DUP_JOB -2	 
#define BAD_JOBSPEC -3	 

 
#define NO_PID (pid_t)-1

#define ANY_PID (pid_t)-1

 
#define FORK_SYNC	0		 
#define FORK_ASYNC	1		 
#define FORK_NOJOB	2		 
#define FORK_NOTERM	4		 

 
#if !defined (HAVE_UNISTD_H)
extern pid_t fork (), getpid (), getpgrp ();
#endif  

 
extern struct jobstats js;

extern pid_t original_pgrp, shell_pgrp, pipeline_pgrp;
extern volatile pid_t last_made_pid, last_asynchronous_pid;
extern int asynchronous_notification;

extern int already_making_children;
extern int running_in_background;

extern PROCESS *last_procsub_child;

extern JOB **jobs;

extern void making_children PARAMS((void));
extern void stop_making_children PARAMS((void));
extern void cleanup_the_pipeline PARAMS((void));
extern void discard_last_procsub_child PARAMS((void));
extern void save_pipeline PARAMS((int));
extern PROCESS *restore_pipeline PARAMS((int));
extern void start_pipeline PARAMS((void));
extern int stop_pipeline PARAMS((int, COMMAND *));
extern int discard_pipeline PARAMS((PROCESS *));
extern void append_process PARAMS((char *, pid_t, int, int));

extern void save_proc_status PARAMS((pid_t, int));

extern PROCESS *procsub_add PARAMS((PROCESS *));
extern PROCESS *procsub_search PARAMS((pid_t));
extern PROCESS *procsub_delete PARAMS((pid_t));
extern int procsub_waitpid PARAMS((pid_t));
extern void procsub_waitall PARAMS((void));
extern void procsub_clear PARAMS((void));
extern void procsub_prune PARAMS((void));

extern void delete_job PARAMS((int, int));
extern void nohup_job PARAMS((int));
extern void delete_all_jobs PARAMS((int));
extern void nohup_all_jobs PARAMS((int));

extern int count_all_jobs PARAMS((void));

extern void terminate_current_pipeline PARAMS((void));
extern void terminate_stopped_jobs PARAMS((void));
extern void hangup_all_jobs PARAMS((void));
extern void kill_current_pipeline PARAMS((void));

#if defined (__STDC__) && defined (pid_t)
extern int get_job_by_pid PARAMS((int, int, PROCESS **));
extern void describe_pid PARAMS((int));
#else
extern int get_job_by_pid PARAMS((pid_t, int, PROCESS **));
extern void describe_pid PARAMS((pid_t));
#endif

extern void list_one_job PARAMS((JOB *, int, int, int));
extern void list_all_jobs PARAMS((int));
extern void list_stopped_jobs PARAMS((int));
extern void list_running_jobs PARAMS((int));

extern pid_t make_child PARAMS((char *, int));

extern int get_tty_state PARAMS((void));
extern int set_tty_state PARAMS((void));

extern int job_exit_status PARAMS((int));
extern int job_exit_signal PARAMS((int));

extern int wait_for_single_pid PARAMS((pid_t, int));
extern int wait_for_background_pids PARAMS((struct procstat *));
extern int wait_for PARAMS((pid_t, int));
extern int wait_for_job PARAMS((int, int, struct procstat *));
extern int wait_for_any_job PARAMS((int, struct procstat *));

extern void wait_sigint_cleanup PARAMS((void));

extern void notify_and_cleanup PARAMS((void));
extern void reap_dead_jobs PARAMS((void));
extern int start_job PARAMS((int, int));
extern int kill_pid PARAMS((pid_t, int, int));
extern int initialize_job_control PARAMS((int));
extern void initialize_job_signals PARAMS((void));
extern int give_terminal_to PARAMS((pid_t, int));

extern void run_sigchld_trap PARAMS((int));

extern int freeze_jobs_list PARAMS((void));
extern void unfreeze_jobs_list PARAMS((void));
extern void set_jobs_list_frozen PARAMS((int));
extern int set_job_control PARAMS((int));
extern void without_job_control PARAMS((void));
extern void end_job_control PARAMS((void));
extern void restart_job_control PARAMS((void));
extern void set_sigchld_handler PARAMS((void));
extern void ignore_tty_job_signals PARAMS((void));
extern void default_tty_job_signals PARAMS((void));
extern void get_original_tty_job_signals PARAMS((void));

extern void init_job_stats PARAMS((void));

extern void close_pgrp_pipe PARAMS((void));
extern void save_pgrp_pipe PARAMS((int *, int));
extern void restore_pgrp_pipe PARAMS((int *));

extern void set_maxchild PARAMS((int));

#ifdef DEBUG
extern void debug_print_pgrps (void);
#endif

extern int job_control;		 

#endif  
