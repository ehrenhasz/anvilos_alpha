 

#define SIZEOF(a) (sizeof(a)/sizeof(a[0]))

#if defined __linux__ || defined __ANDROID__

 
_GL_ATTRIBUTE_MAYBE_UNUSED
static int
get_linux_uptime (struct timespec *p_uptime)
{
   
# if !defined __GLIBC__ || 2 < __GLIBC__ + (17 <= __GLIBC_MINOR__)
  if (clock_gettime (CLOCK_BOOTTIME, p_uptime) >= 0)
    return 0;
# endif

   
# if !defined __ANDROID__
  FILE *fp = fopen ("/proc/uptime", "re");
  if (fp != NULL)
    {
      char buf[32 + 1];
      size_t n = fread (buf, 1, sizeof (buf) - 1, fp);
      fclose (fp);
      if (n > 0)
        {
          buf[n] = '\0';
           
          time_t s = 0;
          char *p;
          for (p = buf; '0' <= *p && *p <= '9'; p++)
            s = 10 * s + (*p - '0');
          if (buf < p)
            {
              long ns = 0;
              if (*p++ == '.')
                for (int i = 0; i < 9; i++)
                  ns = 10 * ns + ('0' <= *p && *p <= '9' ? *p++ - '0' : 0);
              p_uptime->tv_sec = s;
              p_uptime->tv_nsec = ns;
              return 0;
            }
        }
    }
# endif

# if HAVE_DECL_SYSINFO  
   
  struct sysinfo info;
  if (sysinfo (&info) >= 0)
    {
      p_uptime->tv_sec = info.uptime;
      p_uptime->tv_nsec = 0;
      return 0;
    }
# endif

  return -1;
}

#endif

#if defined __linux__ && !defined __ANDROID__

static int
get_linux_boot_time_fallback (struct timespec *p_boot_time)
{
   

  const char * const boot_touched_files[] =
    {
      "/var/lib/systemd/random-seed",  
      "/var/lib/urandom/random-seed",  
      "/var/lib/random-seed",          
       
      "/var/run/utmp"                  
    };
  for (idx_t i = 0; i < SIZEOF (boot_touched_files); i++)
    {
      const char *filename = boot_touched_files[i];
      struct stat statbuf;
      if (stat (filename, &statbuf) >= 0)
        {
          *p_boot_time = get_stat_mtime (&statbuf);
          return 0;
        }
    }
  return -1;
}

 
static int
get_linux_boot_time_final_fallback (struct timespec *p_boot_time)
{
  struct timespec uptime;
  if (get_linux_uptime (&uptime) >= 0)
    {
      struct timespec result;
# if !defined __GLIBC__ || 2 < __GLIBC__ + (16 <= __GLIBC_MINOR__)
       
      if (! timespec_get (&result, TIME_UTC))
        return -1;
#  else
       
      struct timeval tv;
      int r = gettimeofday (&tv, NULL);
      if (r < 0)
        return r;
      result.tv_sec = tv.tv_sec;
      result.tv_nsec = tv.tv_usec * 1000;
#  endif

      if (result.tv_nsec < uptime.tv_nsec)
        {
          result.tv_nsec += 1000000000;
          result.tv_sec -= 1;
        }
      result.tv_sec -= uptime.tv_sec;
      result.tv_nsec -= uptime.tv_nsec;
      *p_boot_time = result;
      return 0;
    }
  return -1;
}

#endif

#if defined __ANDROID__

static int
get_android_boot_time (struct timespec *p_boot_time)
{
   
  struct timespec uptime;
  if (get_linux_uptime (&uptime) >= 0)
    {
      struct timespec result;
      if (clock_gettime (CLOCK_REALTIME, &result) >= 0)
        {
          if (result.tv_nsec < uptime.tv_nsec)
            {
              result.tv_nsec += 1000000000;
              result.tv_sec -= 1;
            }
          result.tv_sec -= uptime.tv_sec;
          result.tv_nsec -= uptime.tv_nsec;
          *p_boot_time = result;
          return 0;
        }
    }
  return -1;
}

#endif

#if defined __OpenBSD__

static int
get_openbsd_boot_time (struct timespec *p_boot_time)
{
   
  const char * const boot_touched_files[] =
    {
      "/var/db/host.random",
      "/var/run/utmp"
    };
  for (idx_t i = 0; i < SIZEOF (boot_touched_files); i++)
    {
      const char *filename = boot_touched_files[i];
      struct stat statbuf;
      if (stat (filename, &statbuf) >= 0)
        {
          *p_boot_time = get_stat_mtime (&statbuf);
          return 0;
        }
    }
  return -1;
}

#endif

#if HAVE_SYS_SYSCTL_H && HAVE_SYSCTL \
    && defined CTL_KERN && defined KERN_BOOTTIME \
    && !defined __minix
 
 

 
static int
get_bsd_boot_time_final_fallback (struct timespec *p_boot_time)
{
  static int request[2] = { CTL_KERN, KERN_BOOTTIME };
  struct timeval result;
  size_t result_len = sizeof result;

  if (sysctl (request, 2, &result, &result_len, NULL, 0) >= 0)
    {
      p_boot_time->tv_sec = result.tv_sec;
      p_boot_time->tv_nsec = result.tv_usec * 1000;
      return 0;
    }
  return -1;
}

#endif

#if defined __HAIKU__

static int
get_haiku_boot_time (struct timespec *p_boot_time)
{
   
  const char * const boot_touched_file = "/dev/input";
  struct stat statbuf;
  if (stat (boot_touched_file, &statbuf) >= 0)
    {
      *p_boot_time = get_stat_mtime (&statbuf);
      return 0;
    }
  return -1;
}

#endif

#if HAVE_OS_H  

 
static int
get_haiku_boot_time_final_fallback (struct timespec *p_boot_time)
{
  system_info si;

  get_system_info (&si);
  p_boot_time->tv_sec = si.boot_time / 1000000;
  p_boot_time->tv_nsec = (si.boot_time % 1000000) * 1000;
  return 0;
}

#endif

#if defined __CYGWIN__ || defined _WIN32

static int
get_windows_boot_time (struct timespec *p_boot_time)
{
   
  const char * const boot_touched_file =
    #if defined __CYGWIN__ && !defined _WIN32
    "/cygdrive/c/pagefile.sys"
    #else
    "C:\\pagefile.sys"
    #endif
    ;
  struct stat statbuf;
  if (stat (boot_touched_file, &statbuf) >= 0)
    {
      *p_boot_time = get_stat_mtime (&statbuf);
      return 0;
    }
  return -1;
}

#endif
