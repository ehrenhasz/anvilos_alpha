 

 

#include <config.h>

#include "readutmp.h"

#include <errno.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#if defined __linux__ || defined __ANDROID__
# include <sys/sysinfo.h>
# include <time.h>
#endif
#if READUTMP_USE_SYSTEMD
# include <dirent.h>
# include <systemd/sd-login.h>
#endif

#if HAVE_SYS_SYSCTL_H && !(defined __GLIBC__ && defined __linux__) && !defined __minix
# if HAVE_SYS_PARAM_H
#  include <sys/param.h>
# endif
# include <sys/sysctl.h>
#endif

#if HAVE_OS_H
# include <OS.h>
#endif

#include "stat-time.h"
#include "xalloc.h"

 
#include "unlocked-io.h"

 
#include "boot-time-aux.h"

 
#undef UT_USER
#undef UT_TIME_MEMBER
#undef UT_PID
#undef UT_TYPE_EQ
#undef UT_TYPE_NOT_DEFINED
#undef UT_EXIT_E_TERMINATION
#undef UT_EXIT_E_EXIT

 
#if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_NAME \
     : HAVE_UTMP_H && HAVE_STRUCT_UTMP_UT_NAME)
# define UT_USER(UT) ((UT)->ut_name)
#else
# define UT_USER(UT) ((UT)->ut_user)
#endif

 
#if HAVE_UTMPX_H || (HAVE_UTMP_H && HAVE_STRUCT_UTMP_UT_TV)
# define UT_TIME_MEMBER(UT) ((UT)->ut_tv.tv_sec)
#else
# define UT_TIME_MEMBER(UT) ((UT)->ut_time)
#endif

 
#if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_PID : HAVE_STRUCT_UTMP_UT_PID)
# define UT_PID(UT) ((UT)->ut_pid)
#else
# define UT_PID(UT) 0
#endif

 
#if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_TYPE : HAVE_STRUCT_UTMP_UT_TYPE)
# define UT_TYPE_EQ(UT, V) ((UT)->ut_type == (V))
# define UT_TYPE_NOT_DEFINED 0
#else
# define UT_TYPE_EQ(UT, V) 0
# define UT_TYPE_NOT_DEFINED 1
#endif

#if HAVE_UTMPX_H
# if HAVE_STRUCT_UTMPX_UT_EXIT_E_TERMINATION
#  define UT_EXIT_E_TERMINATION(UT) ((UT)->ut_exit.e_termination)
# elif HAVE_STRUCT_UTMPX_UT_EXIT_UT_TERMINATION  
#  define UT_EXIT_E_TERMINATION(UT) ((UT)->ut_exit.ut_termination)
# else
#  define UT_EXIT_E_TERMINATION(UT) 0
# endif
#elif HAVE_UTMP_H
# if HAVE_STRUCT_UTMP_UT_EXIT_E_TERMINATION
#  define UT_EXIT_E_TERMINATION(UT) ((UT)->ut_exit.e_termination)
# else
#  define UT_EXIT_E_TERMINATION(UT) 0
# endif
#endif

#if HAVE_UTMPX_H
# if HAVE_STRUCT_UTMPX_UT_EXIT_E_EXIT
#  define UT_EXIT_E_EXIT(UT) ((UT)->ut_exit.e_exit)
# elif HAVE_STRUCT_UTMPX_UT_EXIT_UT_EXIT  
#  define UT_EXIT_E_EXIT(UT) ((UT)->ut_exit.ut_exit)
# else
#  define UT_EXIT_E_EXIT(UT) 0
# endif
#elif HAVE_UTMP_H
# if HAVE_STRUCT_UTMP_UT_EXIT_E_EXIT
#  define UT_EXIT_E_EXIT(UT) ((UT)->ut_exit.e_exit)
# else
#  define UT_EXIT_E_EXIT(UT) 0
# endif
#endif

 
#define UT_USER_SIZE  sizeof UT_USER ((struct UTMP_STRUCT_NAME *) 0)
 
#define UT_ID_SIZE    sizeof (((struct UTMP_STRUCT_NAME *) 0)->ut_id)
 
#define UT_LINE_SIZE  sizeof (((struct UTMP_STRUCT_NAME *) 0)->ut_line)
 
#define UT_HOST_SIZE  sizeof (((struct UTMP_STRUCT_NAME *) 0)->ut_host)

#if 8 <= __GNUC__
# pragma GCC diagnostic ignored "-Wsizeof-pointer-memaccess"
#endif

 

char *
extract_trimmed_name (const STRUCT_UTMP *ut)
{
  char const *name = ut->ut_user;
  idx_t len = strlen (name);
  char const *p;
  for (p = name + len; name < p && p[-1] == ' '; p--)
    continue;
  return ximemdup0 (name, p - name);
}

#if READ_UTMP_SUPPORTED

 

static bool
desirable_utmp_entry (STRUCT_UTMP const *ut, int options)
{
# if defined __OpenBSD__ && !HAVE_UTMPX_H
   
  if (ut->ut_ts.tv_sec == 0 && ut->ut_user[0] == '\0'
      && ut->ut_line[0] == '\0' && ut->ut_host[0] == '\0')
    return false;
# endif

  bool boot_time = UT_TYPE_BOOT_TIME (ut);
  if ((options & READ_UTMP_BOOT_TIME) && !boot_time)
    return false;
  if ((options & READ_UTMP_NO_BOOT_TIME) && boot_time)
    return false;

  bool user_proc = IS_USER_PROCESS (ut);
  if ((options & READ_UTMP_USER_PROCESS) && !user_proc)
    return false;
# if !(defined __CYGWIN__ || defined _WIN32)
  if ((options & READ_UTMP_CHECK_PIDS)
      && user_proc
      && 0 < UT_PID (ut)
      && (kill (UT_PID (ut), 0) < 0 && errno == ESRCH))
    return false;
# endif

  return true;
}

 

struct utmp_alloc
{
   
  struct gl_utmp *utmp;

   
  idx_t filled;

   
  idx_t string_bytes;

   
  idx_t alloc_bytes;
};

 

static struct utmp_alloc
add_utmp (struct utmp_alloc a, int options,
          char const *user, idx_t user_len,
          char const *id, idx_t id_len,
          char const *line, idx_t line_len,
          char const *host, idx_t host_len,
          pid_t pid, short type, struct timespec ts, long session,
          int termination, int exit)
{
  int entry_bytes = sizeof (struct gl_utmp);
  idx_t avail = a.alloc_bytes - (entry_bytes * a.filled + a.string_bytes);
  idx_t needed_string_bytes =
    (user_len + 1) + (id_len + 1) + (line_len + 1) + (host_len + 1);
  idx_t needed = entry_bytes + needed_string_bytes;
  if (avail < needed)
    {
      idx_t old_string_offset = a.alloc_bytes - a.string_bytes;
      void *new = xpalloc (a.utmp, &a.alloc_bytes, needed - avail, -1, 1);
      idx_t new_string_offset = a.alloc_bytes - a.string_bytes;
      a.utmp = new;
      char *q = new;
      memmove (q + new_string_offset, q + old_string_offset, a.string_bytes);
    }
  struct gl_utmp *ut = &a.utmp[a.filled];
  char *stringlim = (char *) a.utmp + a.alloc_bytes;
  char *p = stringlim - a.string_bytes;
  *--p = '\0';  
  ut->ut_user = p = memcpy (p - user_len, user, user_len);
  *--p = '\0';  
  ut->ut_id   = p = memcpy (p -   id_len,   id,   id_len);
  *--p = '\0';  
  ut->ut_line = p = memcpy (p - line_len, line, line_len);
  *--p = '\0';  
  ut->ut_host =     memcpy (p - host_len, host, host_len);
  ut->ut_ts = ts;
  ut->ut_pid = pid;
  ut->ut_session = session;
  ut->ut_type = type;
  ut->ut_exit.e_termination = termination;
  ut->ut_exit.e_exit = exit;
  if (desirable_utmp_entry (ut, options))
    {
       
      ut->ut_user = (char *) (intptr_t) (ut->ut_user - stringlim);
      ut->ut_id   = (char *) (intptr_t) (ut->ut_id   - stringlim);
      ut->ut_line = (char *) (intptr_t) (ut->ut_line - stringlim);
      ut->ut_host = (char *) (intptr_t) (ut->ut_host - stringlim);
      a.filled++;
      a.string_bytes += needed_string_bytes;
    }
  return a;
}

 
static struct utmp_alloc
finish_utmp (struct utmp_alloc a)
{
  char *stringlim = (char *) a.utmp + a.alloc_bytes;

  for (idx_t i = 0; i < a.filled; i++)
    {
      a.utmp[i].ut_user = (intptr_t) a.utmp[i].ut_user + stringlim;
      a.utmp[i].ut_id   = (intptr_t) a.utmp[i].ut_id   + stringlim;
      a.utmp[i].ut_line = (intptr_t) a.utmp[i].ut_line + stringlim;
      a.utmp[i].ut_host = (intptr_t) a.utmp[i].ut_host + stringlim;
    }

  return a;
}

 
_GL_ATTRIBUTE_MAYBE_UNUSED
static bool
have_boot_time (struct utmp_alloc a)
{
  for (idx_t i = 0; i < a.filled; i++)
    {
      struct gl_utmp *ut = &a.utmp[i];
      if (UT_TYPE_BOOT_TIME (ut))
        return true;
    }
  return false;
}

#if !HAVE_UTMPX_H && HAVE_UTMP_H && defined UTMP_NAME_FUNCTION
# if !HAVE_DECL_ENDUTENT  
void endutent (void);
# endif
#endif

static int
read_utmp_from_file (char const *file, idx_t *n_entries, STRUCT_UTMP **utmp_buf,
                     int options)
{
  if ((options & READ_UTMP_BOOT_TIME) != 0
      && (options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) != 0)
    {
       
      *n_entries = 0;
      *utmp_buf = NULL;
      return 0;
    }

  struct utmp_alloc a = {0};

# if READUTMP_USE_SYSTEMD || HAVE_UTMPX_H || HAVE_UTMP_H

#  if defined UTMP_NAME_FUNCTION  

   
  UTMP_NAME_FUNCTION ((char *) file);

  SET_UTMP_ENT ();

#   if (defined __linux__ && !defined __ANDROID__) || defined __minix
  bool file_is_utmp = (strcmp (file, UTMP_FILE) == 0);
   
  struct timespec runlevel_ts = {0};
#   endif

  void const *entry;

  while ((entry = GET_UTMP_ENT ()) != NULL)
    {
      struct UTMP_STRUCT_NAME const *ut = (struct UTMP_STRUCT_NAME const *) entry;

      struct timespec ts =
        #if (HAVE_UTMPX_H ? 1 : HAVE_STRUCT_UTMP_UT_TV)
        { .tv_sec = ut->ut_tv.tv_sec, .tv_nsec = ut->ut_tv.tv_usec * 1000 };
        #else
        { .tv_sec = ut->ut_time, .tv_nsec = 0 };
        #endif

      a = add_utmp (a, options,
                    UT_USER (ut), strnlen (UT_USER (ut), UT_USER_SIZE),
                    #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_ID : HAVE_STRUCT_UTMP_UT_ID)
                    ut->ut_id, strnlen (ut->ut_id, UT_ID_SIZE),
                    #else
                    "", 0,
                    #endif
                    ut->ut_line, strnlen (ut->ut_line, UT_LINE_SIZE),
                    #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_HOST : HAVE_STRUCT_UTMP_UT_HOST)
                    ut->ut_host, strnlen (ut->ut_host, UT_HOST_SIZE),
                    #else
                    "", 0,
                    #endif
                    #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_PID : HAVE_STRUCT_UTMP_UT_PID)
                    ut->ut_pid,
                    #else
                    0,
                    #endif
                    #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_TYPE : HAVE_STRUCT_UTMP_UT_TYPE)
                    ut->ut_type,
                    #else
                    0,
                    #endif
                    ts,
                    #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_SESSION : HAVE_STRUCT_UTMP_UT_SESSION)
                    ut->ut_session,
                    #else
                    0,
                    #endif
                    UT_EXIT_E_TERMINATION (ut), UT_EXIT_E_EXIT (ut)
                   );
#   if defined __linux__ && !defined __ANDROID__
      if (file_is_utmp
          && memcmp (UT_USER (ut), "runlevel", strlen ("runlevel") + 1) == 0
          && memcmp (ut->ut_line, "~", strlen ("~") + 1) == 0)
        runlevel_ts = ts;
#   endif
#   if defined __minix
      if (file_is_utmp
          && UT_USER (ut)[0] == '\0'
          && memcmp (ut->ut_line, "run-level ", strlen ("run-level ")) == 0)
        runlevel_ts = ts;
#   endif
    }

  END_UTMP_ENT ();

#   if defined __linux__ && !defined __ANDROID__
   
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && file_is_utmp)
    {
      for (idx_t i = 0; i < a.filled; i++)
        {
          struct gl_utmp *ut = &a.utmp[i];
          if (UT_TYPE_BOOT_TIME (ut))
            {
               
              if (ut->ut_ts.tv_sec <= 60 && runlevel_ts.tv_sec != 0)
                ut->ut_ts = runlevel_ts;
              break;
            }
        }
      if (!have_boot_time (a))
        {
           
          struct timespec boot_time;
          if (get_linux_boot_time_fallback (&boot_time) >= 0)
            a = add_utmp (a, options,
                          "reboot", strlen ("reboot"),
                          "", 0,
                          "~", strlen ("~"),
                          "", 0,
                          0, BOOT_TIME, boot_time, 0, 0, 0);
        }
    }
#   endif

#   if defined __ANDROID__
   
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_android_boot_time (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "", 0,
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }
#   endif

#   if defined __minix
   
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && file_is_utmp)
    {
      for (idx_t i = 0; i < a.filled; i++)
        {
          struct gl_utmp *ut = &a.utmp[i];
          if (UT_TYPE_BOOT_TIME (ut))
            {
              if (ut->ut_ts.tv_sec <= 60 && runlevel_ts.tv_sec != 0)
                ut->ut_ts = runlevel_ts;
              break;
            }
        }
    }
#   endif

#  else  

  FILE *f = fopen (file, "re");

  if (f != NULL)
    {
      for (;;)
        {
          struct UTMP_STRUCT_NAME ut;

          if (fread (&ut, sizeof ut, 1, f) == 0)
            break;
          a = add_utmp (a, options,
                        UT_USER (&ut), strnlen (UT_USER (&ut), UT_USER_SIZE),
                        #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_ID : HAVE_STRUCT_UTMP_UT_ID)
                        ut.ut_id, strnlen (ut.ut_id, UT_ID_SIZE),
                        #else
                        "", 0,
                        #endif
                        ut.ut_line, strnlen (ut.ut_line, UT_LINE_SIZE),
                        #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_HOST : HAVE_STRUCT_UTMP_UT_HOST)
                        ut.ut_host, strnlen (ut.ut_host, UT_HOST_SIZE),
                        #else
                        "", 0,
                        #endif
                        #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_PID : HAVE_STRUCT_UTMP_UT_PID)
                        ut.ut_pid,
                        #else
                        0,
                        #endif
                        #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_TYPE : HAVE_STRUCT_UTMP_UT_TYPE)
                        ut.ut_type,
                        #else
                        0,
                        #endif
                        #if (HAVE_UTMPX_H ? 1 : HAVE_STRUCT_UTMP_UT_TV)
                        (struct timespec) { .tv_sec = ut.ut_tv.tv_sec, .tv_nsec = ut.ut_tv.tv_usec * 1000 },
                        #else
                        (struct timespec) { .tv_sec = ut.ut_time, .tv_nsec = 0 },
                        #endif
                        #if (HAVE_UTMPX_H ? HAVE_STRUCT_UTMPX_UT_SESSION : HAVE_STRUCT_UTMP_UT_SESSION)
                        ut.ut_session,
                        #else
                        0,
                        #endif
                        UT_EXIT_E_TERMINATION (&ut), UT_EXIT_E_EXIT (&ut)
                       );
        }

      int saved_errno = ferror (f) ? errno : 0;
      if (fclose (f) != 0)
        saved_errno = errno;
      if (saved_errno != 0)
        {
          free (a.utmp);
          errno = saved_errno;
          return -1;
        }
    }
  else
    {
      if (strcmp (file, UTMP_FILE) != 0)
        {
          int saved_errno = errno;
          free (a.utmp);
          errno = saved_errno;
          return -1;
        }
    }

#   if defined __OpenBSD__
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_openbsd_boot_time (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "", 0,
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }
#   endif

#  endif

#  if defined __linux__ && !defined __ANDROID__
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_linux_boot_time_final_fallback (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "~", strlen ("~"),
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }

#  endif

#  if HAVE_SYS_SYSCTL_H && HAVE_SYSCTL \
      && defined CTL_KERN && defined KERN_BOOTTIME \
      && !defined __minix
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_bsd_boot_time_final_fallback (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "", 0,
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }
#  endif

#  if defined __HAIKU__
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_haiku_boot_time (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "", 0,
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }
#  endif

#  if HAVE_OS_H  
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_haiku_boot_time_final_fallback (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "", 0,
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }
#  endif

# endif

# if defined __CYGWIN__ || defined _WIN32
  if ((options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)) == 0
      && strcmp (file, UTMP_FILE) == 0
      && !have_boot_time (a))
    {
      struct timespec boot_time;
      if (get_windows_boot_time (&boot_time) >= 0)
        a = add_utmp (a, options,
                      "reboot", strlen ("reboot"),
                      "", 0,
                      "", 0,
                      "", 0,
                      0, BOOT_TIME, boot_time, 0, 0, 0);
    }
# endif

  a = finish_utmp (a);

  *n_entries = a.filled;
  *utmp_buf = a.utmp;

  return 0;
}

# if READUTMP_USE_SYSTEMD
 

static struct timespec
get_boot_time_uncached (void)
{
   
  {
    idx_t n_entries = 0;
    STRUCT_UTMP *utmp = NULL;
    read_utmp_from_file (UTMP_FILE, &n_entries, &utmp, READ_UTMP_BOOT_TIME);
    if (n_entries > 0)
      {
        struct timespec result = utmp[0].ut_ts;
        free (utmp);
        return result;
      }
    free (utmp);
  }

   
  return (struct timespec) {0};
}

static struct timespec
get_boot_time (void)
{
  static bool volatile cached;
  static struct timespec volatile boot_time;

  if (!cached)
    {
      boot_time = get_boot_time_uncached ();
      cached = true;
    }
  return boot_time;
}

 
static char *
guess_pty_name (uid_t uid, const struct timespec at)
{
   
  DIR *dirp = opendir ("/dev/pts");
  if (dirp != NULL)
    {
       
      char name_buf[9 + 10 + 1];
      memcpy (name_buf, "/dev/pts/", 9);

      char best_name[9 + 10 + 1];
      struct timespec best_time = { .tv_sec = 0, .tv_nsec = 0 };

      for (;;)
        {
          struct dirent *dp = readdir (dirp);
          if (dp == NULL)
            break;
          if (dp->d_name[0] != '.' && strlen (dp->d_name) <= 10)
            {
               
              strcpy (name_buf + 9, dp->d_name);

               
              struct stat st;
              if (stat (name_buf, &st) >= 0
                  && st.st_uid == uid
                  && (st.st_ctim.tv_sec > at.tv_sec
                      || (st.st_ctim.tv_sec == at.tv_sec
                          && st.st_ctim.tv_nsec >= at.tv_nsec)))
                {
                   
                   
                  if ((best_time.tv_sec == 0 && best_time.tv_nsec == 0)
                      || (st.st_ctim.tv_sec < best_time.tv_sec
                          || (st.st_ctim.tv_sec == best_time.tv_sec
                              && st.st_ctim.tv_nsec < best_time.tv_nsec)))
                    {
                      strcpy (best_name, name_buf);
                      best_time = st.st_ctim;
                    }
                }
            }
        }

      closedir (dirp);

       
      if (!(best_time.tv_sec == 0 && best_time.tv_nsec == 0)
          && (best_time.tv_sec < at.tv_sec + 5
              || (best_time.tv_sec == at.tv_sec + 5
                  && best_time.tv_nsec <= at.tv_nsec)))
        return xstrdup (best_name + 5);
    }

  return NULL;
}

static int
read_utmp_from_systemd (idx_t *n_entries, STRUCT_UTMP **utmp_buf, int options)
{
   
  struct utmp_alloc a = {0};

   
  if (!(options & (READ_UTMP_USER_PROCESS | READ_UTMP_NO_BOOT_TIME)))
    a = add_utmp (a, options,
                  "reboot", strlen ("reboot"),
                  "", 0,
                  "~", strlen ("~"),
                  "", 0,
                  0, BOOT_TIME, get_boot_time (), 0, 0, 0);

   
  if (!(options & READ_UTMP_BOOT_TIME))
    {
      char **sessions;
      int num_sessions = sd_get_sessions (&sessions);
      if (num_sessions >= 0)
        {
          char **session_ptr;
          for (session_ptr = sessions; *session_ptr != NULL; session_ptr++)
            {
              char *session = *session_ptr;

              uint64_t start_usec;
              if (sd_session_get_start_time (session, &start_usec) < 0)
                start_usec = 0;
              struct timespec start_ts;
              start_ts.tv_sec = start_usec / 1000000;
              start_ts.tv_nsec = start_usec % 1000000 * 1000;

              char *seat;
              if (sd_session_get_seat (session, &seat) < 0)
                seat = NULL;

              char missing[] = "";

              char *type = NULL;
              char *tty;
              if (sd_session_get_tty (session, &tty) < 0)
                {
                  tty = NULL;
                   
                  if (sd_session_get_type (session, &type) < 0)
                    type = missing;
                  if (strcmp (type, "tty") == 0)
                    {
                      char *service;
                      if (sd_session_get_service (session, &service) < 0)
                        service = NULL;

                      uid_t uid;
                      char *pty = (sd_session_get_uid (session, &uid) < 0 ? NULL
                                   : guess_pty_name (uid, start_ts));

                      if (service != NULL && pty != NULL)
                        {
                          tty = xmalloc (strlen (service) + 1 + strlen (pty) + 1);
                          stpcpy (stpcpy (stpcpy (tty, service), " "), pty);
                          free (pty);
                          free (service);
                        }
                      else if (service != NULL)
                        tty = service;
                      else if (pty != NULL)
                        tty = pty;
                    }
                }

               
              if (seat != NULL || tty != NULL)
                {
                  char *user;
                  if (sd_session_get_username (session, &user) < 0)
                    user = missing;

                  pid_t leader_pid;
                  if (sd_session_get_leader (session, &leader_pid) < 0)
                    leader_pid = 0;

                  char *host;
                  char *remote_host;
                  if (sd_session_get_remote_host (session, &remote_host) < 0)
                    {
                      host = missing;
                       
                      if (!type && sd_session_get_type (session, &type) < 0)
                        type = missing;
                      if (strcmp (type, "x11") == 0)
                        {
                          char *display;
                          if (sd_session_get_display (session, &display) < 0)
                            display = NULL;
                          host = display;
                        }
                    }
                  else
                    {
                      char *remote_user;
                      if (sd_session_get_remote_user (session, &remote_user) < 0)
                        host = remote_host;
                      else
                        {
                          host = xmalloc (strlen (remote_user) + 1
                                          + strlen (remote_host) + 1);
                          stpcpy (stpcpy (stpcpy (host, remote_user), "@"),
                                  remote_host);
                          free (remote_user);
                          free (remote_host);
                        }
                    }

                  if (seat != NULL)
                    a = add_utmp (a, options,
                                  user, strlen (user),
                                  session, strlen (session),
                                  seat, strlen (seat),
                                  host, strlen (host),
                                  leader_pid  ,
                                  USER_PROCESS, start_ts, leader_pid, 0, 0);
                  if (tty != NULL)
                    a = add_utmp (a, options,
                                  user, strlen (user),
                                  session, strlen (session),
                                  tty, strlen (tty),
                                  host, strlen (host),
                                  leader_pid  ,
                                  USER_PROCESS, start_ts, leader_pid, 0, 0);

                  if (host != missing)
                    free (host);
                  if (user != missing)
                    free (user);
                }

              if (type != missing)
                free (type);
              free (tty);
              free (seat);
              free (session);
            }
          free (sessions);
        }
    }

  a = finish_utmp (a);

  *n_entries = a.filled;
  *utmp_buf = a.utmp;

  return 0;
}

# endif

int
read_utmp (char const *file, idx_t *n_entries, STRUCT_UTMP **utmp_buf,
           int options)
{
# if READUTMP_USE_SYSTEMD
  if (strcmp (file, UTMP_FILE) == 0)
     
    return read_utmp_from_systemd (n_entries, utmp_buf, options);
# endif

  return read_utmp_from_file (file, n_entries, utmp_buf, options);
}

#else  

int
read_utmp (char const *file, idx_t *n_entries, STRUCT_UTMP **utmp_buf,
           int options)
{
  errno = ENOSYS;
  return -1;
}

#endif
