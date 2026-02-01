 

#ifndef __READUTMP_H__
#define __READUTMP_H__

 
#if !_GL_CONFIG_H_INCLUDED
# error "Please include config.h first."
#endif

#include "idx.h"

#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

 
#if (HAVE_UTMPX_H && HAVE_UTMP_H && HAVE_STRUCT_UTMP_UT_EXIT \
     && ! HAVE_STRUCT_UTMPX_UT_EXIT)
# undef HAVE_UTMPX_H
#endif

 
#if HAVE_UTMP_H
# include <utmp.h>
#endif

 
#if HAVE_UTMPX_H
# if defined _THREAD_SAFE && defined UTMP_DATA_INIT
     
#  define utmp_data gl_aix_4_3_workaround_utmp_data
# endif
# include <utmpx.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


 
struct gl_utmp
{
   
  char *ut_user;                 
  char *ut_id;                   
  char *ut_line;                 
  char *ut_host;                 
  struct timespec ut_ts;         
  pid_t ut_pid;                  
  pid_t ut_session;              
  short ut_type;                 
  struct { int e_termination; int e_exit; } ut_exit;
};

 
#define UT_USER(UT) ((UT)->ut_user)
#define UT_TIME_MEMBER(UT) ((UT)->ut_ts.tv_sec)
#define UT_PID(UT) ((UT)->ut_pid)
#define UT_TYPE_EQ(UT, V) ((UT)->ut_type == (V))
#define UT_TYPE_NOT_DEFINED 0
#define UT_EXIT_E_TERMINATION(UT) ((UT)->ut_exit.e_termination)
#define UT_EXIT_E_EXIT(UT) ((UT)->ut_exit.e_exit)

 
typedef struct gl_utmp STRUCT_UTMP;

 
enum { UT_USER_SIZE = -1 };

 
enum { UT_ID_SIZE = -1 };

 
enum { UT_LINE_SIZE = -1 };

 
enum { UT_HOST_SIZE = -1 };


 

#if HAVE_UTMPX_H

 

# if __GLIBC__ && _TIME_BITS == 64
 
#define _GL_UT_USER_SIZE  sizeof (((struct utmpx *) 0)->ut_user)
#define _GL_UT_ID_SIZE    sizeof (((struct utmpx *) 0)->ut_id)
#define _GL_UT_LINE_SIZE  sizeof (((struct utmpx *) 0)->ut_line)
#define _GL_UT_HOST_SIZE  sizeof (((struct utmpx *) 0)->ut_host)
struct utmpx32
{
  short int ut_type;                
  pid_t ut_pid;                     
  char ut_line[_GL_UT_LINE_SIZE];   
  char ut_id[_GL_UT_ID_SIZE];       
  char ut_user[_GL_UT_USER_SIZE];   
  char ut_host[_GL_UT_HOST_SIZE];   
  struct __exit_status ut_exit;     
   
  int ut_session;                   
  struct
  {
     
    unsigned int tv_sec;
    int tv_usec;                    
  } ut_tv;                          
  int ut_addr_v6[4];                
  char ut_reserved[20];             
};
#  define UTMP_STRUCT_NAME utmpx32
# else
#  define UTMP_STRUCT_NAME utmpx
# endif
# define SET_UTMP_ENT setutxent
# define GET_UTMP_ENT getutxent
# define END_UTMP_ENT endutxent
# ifdef HAVE_UTMPXNAME  
#  define UTMP_NAME_FUNCTION utmpxname
# elif defined UTXDB_ACTIVE  
#  define UTMP_NAME_FUNCTION(x) setutxdb (UTXDB_ACTIVE, x)
# endif

#elif HAVE_UTMP_H

 

# define UTMP_STRUCT_NAME utmp
# define SET_UTMP_ENT setutent
# define GET_UTMP_ENT getutent
# define END_UTMP_ENT endutent
# ifdef HAVE_UTMPNAME  
#  define UTMP_NAME_FUNCTION utmpname
# endif

#endif

 
#define HAVE_STRUCT_XTMP_UT_ID \
  (READUTMP_USE_SYSTEMD \
   || (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_ID : HAVE_STRUCT_UTMP_UT_ID))

 
#define HAVE_STRUCT_XTMP_UT_PID \
  (READUTMP_USE_SYSTEMD \
   || (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_PID : HAVE_STRUCT_UTMP_UT_PID))

 
#define HAVE_STRUCT_XTMP_UT_HOST \
  (READUTMP_USE_SYSTEMD \
   || (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_HOST : HAVE_STRUCT_UTMP_UT_HOST))

 
#if !defined UTMP_FILE && defined _PATH_UTMP
# define UTMP_FILE _PATH_UTMP
#endif
#ifdef UTMPX_FILE  
# undef UTMP_FILE
# define UTMP_FILE UTMPX_FILE
#endif
#ifndef UTMP_FILE
# define UTMP_FILE "/etc/utmp"
#endif

 
#if !defined WTMP_FILE && defined _PATH_WTMP
# define WTMP_FILE _PATH_WTMP
#endif
#ifdef WTMPX_FILE  
# undef WTMP_FILE
# define WTMP_FILE WTMPX_FILE
#endif
#ifndef WTMP_FILE
# define WTMP_FILE "/etc/wtmp"
#endif

 
#if defined __ANDROID__ && !defined BOOT_TIME
# define BOOT_TIME 2
#endif

 
#if !(HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_TYPE : HAVE_STRUCT_UTMP_UT_TYPE)
# define BOOT_TIME 2
# define USER_PROCESS 0
#endif

 
#ifdef BOOT_TIME
# define UT_TYPE_BOOT_TIME(UT) ((UT)->ut_type == BOOT_TIME)
#else
# define UT_TYPE_BOOT_TIME(UT) 0
#endif
#ifdef USER_PROCESS
# define UT_TYPE_USER_PROCESS(UT) ((UT)->ut_type == USER_PROCESS)
#else
# define UT_TYPE_USER_PROCESS(UT) 0
#endif

 
#define IS_USER_PROCESS(UT)                                    \
  ((UT)->ut_user[0] && UT_TYPE_USER_PROCESS (UT))

 
#if READUTMP_USE_SYSTEMD || HAVE_UTMPX_H || HAVE_UTMP_H || defined __CYGWIN__ || defined _WIN32
# define READ_UTMP_SUPPORTED 1
#endif

 
enum
  {
    READ_UTMP_CHECK_PIDS   = 1,
    READ_UTMP_USER_PROCESS = 2,
    READ_UTMP_BOOT_TIME    = 4,
    READ_UTMP_NO_BOOT_TIME = 8
  };

 
char *extract_trimmed_name (const STRUCT_UTMP *ut)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

 
int read_utmp (char const *file, idx_t *n_entries, STRUCT_UTMP **utmp_buf,
               int options);


#ifdef __cplusplus
}
#endif

#endif  
