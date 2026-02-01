 

 

 
#define CONTINUE_AFTER_KILL_ERROR

 
#define BREAK_COMPLAINS

 
#define CD_COMPLAINS

 
#define BUFFERED_INPUT

 
#define ONESHOT

 
#define V9_ECHO

 
#define DONT_REPORT_SIGPIPE

 
#define DONT_REPORT_SIGTERM

 
 

 
#ifndef DEFAULT_PATH_VALUE
#define DEFAULT_PATH_VALUE \
  "/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin:."
#endif

 
 

 
#ifndef STANDARD_UTILS_PATH
#define STANDARD_UTILS_PATH \
  "/bin:/usr/bin:/sbin:/usr/sbin:/etc:/usr/etc"
#endif

 
#ifndef DEFAULT_LOADABLE_BUILTINS_PATH
#define DEFAULT_LOADABLE_BUILTINS_PATH \
  "/usr/local/lib/bash:/usr/lib/bash:/opt/local/lib/bash:/usr/pkg/lib/bash:/opt/pkg/lib/bash:."
#endif

 
#define PPROMPT "\\s-\\v\\$ "
#define SPROMPT "> "

 
#define KSH_COMPATIBLE_SELECT

 
#define DEFAULT_BASHRC "~/.bashrc"

 
 

 
 

 
 

 
 

 
 
#define CASEMOD_TOGGLECASE
#define CASEMOD_CAPCASE

 
 

 
 
#if defined (SYSLOG_HISTORY)
#  define SYSLOG_FACILITY LOG_USER
#  define SYSLOG_LEVEL LOG_INFO
#  define OPENLOG_OPTS LOG_PID
#endif

 
#if defined (SYSLOG_HISTORY)
 
#endif

 
 

 
#ifndef MULTIPLE_COPROCS
#  define MULTIPLE_COPROCS 0
#endif

 
#define CHECKWINSIZE_DEFAULT	1

 
#define OPTIMIZE_SEQUENTIAL_ARRAY_ASSIGNMENT	1

 
 

 
 

 
#define CHECKHASH_DEFAULT 0

 
#define EVALNEST_MAX 0

 
#define SOURCENEST_MAX 0

 
#define USE_MKTEMP
#define USE_MKSTEMP
#define USE_MKDTEMP

 
#define OLDPWD_CHECK_DIRECTORY 1

 
 

 
#define HISTEXPAND_DEFAULT	1

 
#define ASSOC_KVPAIR_ASSIGNMENT 1
