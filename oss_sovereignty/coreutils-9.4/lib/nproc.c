 

#include <config.h>
#include "nproc.h"

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#if HAVE_PTHREAD_GETAFFINITY_NP && 0
# include <pthread.h>
# include <sched.h>
#endif
#if HAVE_SCHED_GETAFFINITY_LIKE_GLIBC || HAVE_SCHED_GETAFFINITY_NP
# include <sched.h>
#endif

#include <sys/types.h>

#if HAVE_SYS_PSTAT_H
# include <sys/pstat.h>
#endif

#if HAVE_SYS_SYSMP_H
# include <sys/sysmp.h>
#endif

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#if HAVE_SYS_SYSCTL_H && !(defined __GLIBC__ && defined __linux__)
# include <sys/sysctl.h>
#endif

#if defined _WIN32 && ! defined __CYGWIN__
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#include "c-ctype.h"

#include "minmax.h"

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

 
static unsigned long
num_processors_via_affinity_mask (void)
{
   
#if HAVE_PTHREAD_GETAFFINITY_NP && defined __GLIBC__ && 0
  {
    cpu_set_t set;

    if (pthread_getaffinity_np (pthread_self (), sizeof (set), &set) == 0)
      {
        unsigned long count;

# ifdef CPU_COUNT
         
        count = CPU_COUNT (&set);
# else
        size_t i;

        count = 0;
        for (i = 0; i < CPU_SETSIZE; i++)
          if (CPU_ISSET (i, &set))
            count++;
# endif
        if (count > 0)
          return count;
      }
  }
#elif HAVE_PTHREAD_GETAFFINITY_NP && defined __NetBSD__ && 0
  {
    cpuset_t *set;

    set = cpuset_create ();
    if (set != NULL)
      {
        unsigned long count = 0;

        if (pthread_getaffinity_np (pthread_self (), cpuset_size (set), set)
            == 0)
          {
            cpuid_t i;

            for (i = 0;; i++)
              {
                int ret = cpuset_isset (i, set);
                if (ret < 0)
                  break;
                if (ret > 0)
                  count++;
              }
          }
        cpuset_destroy (set);
        if (count > 0)
          return count;
      }
  }
#elif HAVE_SCHED_GETAFFINITY_LIKE_GLIBC  
  {
    cpu_set_t set;

    if (sched_getaffinity (0, sizeof (set), &set) == 0)
      {
        unsigned long count;

# ifdef CPU_COUNT
         
        count = CPU_COUNT (&set);
# else
        size_t i;

        count = 0;
        for (i = 0; i < CPU_SETSIZE; i++)
          if (CPU_ISSET (i, &set))
            count++;
# endif
        if (count > 0)
          return count;
      }
  }
#elif HAVE_SCHED_GETAFFINITY_NP  
  {
    cpuset_t *set;

    set = cpuset_create ();
    if (set != NULL)
      {
        unsigned long count = 0;

        if (sched_getaffinity_np (getpid (), cpuset_size (set), set) == 0)
          {
            cpuid_t i;

            for (i = 0;; i++)
              {
                int ret = cpuset_isset (i, set);
                if (ret < 0)
                  break;
                if (ret > 0)
                  count++;
              }
          }
        cpuset_destroy (set);
        if (count > 0)
          return count;
      }
  }
#endif

#if defined _WIN32 && ! defined __CYGWIN__
  {  
    DWORD_PTR process_mask;
    DWORD_PTR system_mask;

    if (GetProcessAffinityMask (GetCurrentProcess (),
                                &process_mask, &system_mask))
      {
        DWORD_PTR mask = process_mask;
        unsigned long count = 0;

        for (; mask != 0; mask = mask >> 1)
          if (mask & 1)
            count++;
        if (count > 0)
          return count;
      }
  }
#endif

  return 0;
}


 
static unsigned long int
num_processors_ignoring_omp (enum nproc_query query)
{
   

  if (query == NPROC_CURRENT)
    {
       
      {
        unsigned long nprocs = num_processors_via_affinity_mask ();

        if (nprocs > 0)
          return nprocs;
      }

#if defined _SC_NPROCESSORS_ONLN
      {  
        long int nprocs = sysconf (_SC_NPROCESSORS_ONLN);
        if (nprocs > 0)
          return nprocs;
      }
#endif
    }
  else  
    {
#if defined _SC_NPROCESSORS_CONF
      {  
        long int nprocs = sysconf (_SC_NPROCESSORS_CONF);

# if __GLIBC__ >= 2 && defined __linux__
         
        if (nprocs == 1 || nprocs == 2)
          {
            unsigned long nprocs_current = num_processors_via_affinity_mask ();

            if (  nprocs_current > nprocs)
              nprocs = nprocs_current;
          }
# endif

        if (nprocs > 0)
          return nprocs;
      }
#endif
    }

#if HAVE_PSTAT_GETDYNAMIC
  {  
    struct pst_dynamic psd;
    if (pstat_getdynamic (&psd, sizeof psd, 1, 0) >= 0)
      {
         
        if (query == NPROC_CURRENT)
          {
            if (psd.psd_proc_cnt > 0)
              return psd.psd_proc_cnt;
          }
        else
          {
            if (psd.psd_max_proc_cnt > 0)
              return psd.psd_max_proc_cnt;
          }
      }
  }
#endif

#if HAVE_SYSMP && defined MP_NAPROCS && defined MP_NPROCS
  {  
     
    int nprocs =
      sysmp (query == NPROC_CURRENT && getuid () != 0
             ? MP_NAPROCS
             : MP_NPROCS);
    if (nprocs > 0)
      return nprocs;
  }
#endif

   

#if HAVE_SYSCTL && !(defined __GLIBC__ && defined __linux__) && defined HW_NCPU
  {  
    int nprocs;
    size_t len = sizeof (nprocs);
    static int mib[][2] = {
# ifdef HW_NCPUONLINE
      { CTL_HW, HW_NCPUONLINE },
# endif
      { CTL_HW, HW_NCPU }
    };
    for (int i = 0; i < ARRAY_SIZE (mib); i++)
      {
        if (sysctl (mib[i], ARRAY_SIZE (mib[i]), &nprocs, &len, NULL, 0) == 0
            && len == sizeof (nprocs)
            && 0 < nprocs)
          return nprocs;
      }
  }
#endif

#if defined _WIN32 && ! defined __CYGWIN__
  {  
    SYSTEM_INFO system_info;
    GetSystemInfo (&system_info);
    if (0 < system_info.dwNumberOfProcessors)
      return system_info.dwNumberOfProcessors;
  }
#endif

  return 1;
}

 
static unsigned long int
parse_omp_threads (char const* threads)
{
  unsigned long int ret = 0;

  if (threads == NULL)
    return ret;

   
  while (*threads != '\0' && c_isspace (*threads))
    threads++;

   
  if (c_isdigit (*threads))
    {
      char *endptr = NULL;
      unsigned long int value = strtoul (threads, &endptr, 10);

      if (endptr != NULL)
        {
          while (*endptr != '\0' && c_isspace (*endptr))
            endptr++;
          if (*endptr == '\0')
            return value;
           
          else if (*endptr == ',')
            return value;
        }
    }

  return ret;
}

unsigned long int
num_processors (enum nproc_query query)
{
  unsigned long int omp_env_limit = ULONG_MAX;

  if (query == NPROC_CURRENT_OVERRIDABLE)
    {
      unsigned long int omp_env_threads;
       
      omp_env_threads = parse_omp_threads (getenv ("OMP_NUM_THREADS"));
      omp_env_limit = parse_omp_threads (getenv ("OMP_THREAD_LIMIT"));
      if (! omp_env_limit)
        omp_env_limit = ULONG_MAX;

      if (omp_env_threads)
        return MIN (omp_env_threads, omp_env_limit);

      query = NPROC_CURRENT;
    }
   
  {
    unsigned long nprocs = num_processors_ignoring_omp (query);
    return MIN (nprocs, omp_env_limit);
  }
}
