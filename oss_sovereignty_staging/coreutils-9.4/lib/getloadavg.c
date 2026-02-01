 

#include <config.h>

 
#include <stdlib.h>

#include <errno.h>
#include <stdio.h>

# include <sys/types.h>

# if HAVE_SYS_PARAM_H
#  include <sys/param.h>
# endif

# include "intprops.h"

# if defined _WIN32 && ! defined __CYGWIN__ && ! defined WINDOWS32
#  define WINDOWS32
# endif

# ifdef NeXT
 
#  undef BSD

 
#  undef FSCALE
# endif

 
# ifdef __GNU__
#  undef BSD
#  undef FSCALE
# endif  

 


 

# if defined (HPUX) && !defined (hpux)
#  define hpux
# endif

# if defined (__hpux) && !defined (hpux)
#  define hpux
# endif

# if defined (__sun) && !defined (sun)
#  define sun
# endif

# if defined (hp300) && !defined (hpux)
#  define MORE_BSD
# endif

# if defined (__SVR4) && !defined (SVR4)
#  define SVR4
# endif

# if (defined (sun) && defined (SVR4)) || defined (SOLARIS2)
#  define SUNOS_5
# endif

# if defined (__osf__) && (defined (__alpha) || defined (__alpha__))
#  define OSF_ALPHA
#  include <sys/mbuf.h>
#  include <sys/socket.h>
#  include <net/route.h>
#  include <sys/table.h>
 
#  undef sys
# endif

# if defined (__osf__) && (defined (mips) || defined (__mips__))
#  define OSF_MIPS
#  include <sys/table.h>
# endif


 
# ifndef LOAD_AVE_TYPE

#  ifdef MORE_BSD
#   define LOAD_AVE_TYPE long
#  endif

#  ifdef sun
#   define LOAD_AVE_TYPE long
#  endif

#  ifdef sgi
#   define LOAD_AVE_TYPE long
#  endif

#  ifdef SVR4
#   define LOAD_AVE_TYPE long
#  endif

#  ifdef OSF_ALPHA
#   define LOAD_AVE_TYPE long
#  endif

#  if defined _AIX && ! defined HAVE_LIBPERFSTAT
#   define LOAD_AVE_TYPE long
#  endif

# endif  

# ifdef OSF_ALPHA
 
#  undef FSCALE
#  define FSCALE 1024.0
# endif


# ifndef FSCALE

 

#  ifdef MORE_BSD
#   define FSCALE 2048.0
#  endif

#  if defined (MIPS) || defined (SVR4)
#   define FSCALE 256
#  endif

#  if defined (sgi)
 
#   undef FSCALE
#   define FSCALE 1000.0
#  endif

#  if defined _AIX && !defined HAVE_LIBPERFSTAT
#   define FSCALE 65536.0
#  endif

# endif  

# if !defined (LDAV_CVT) && defined (FSCALE)
#  define LDAV_CVT(n) (((double) (n)) / FSCALE)
# endif

# ifndef NLIST_STRUCT
#  if HAVE_NLIST_H
#   define NLIST_STRUCT
#  endif
# endif

# if defined (sgi) || (defined (mips) && !defined (BSD))
#  define FIXUP_KERNEL_SYMBOL_ADDR(nl) ((nl)[0].n_value &= ~(1 << 31))
# endif


# if !defined (KERNEL_FILE) && defined (hpux)
#  define KERNEL_FILE "/hp-ux"
# endif

# if !defined (KERNEL_FILE) && (defined (MIPS) || defined (SVR4) || defined (ISC) || defined (sgi))
#  define KERNEL_FILE "/unix"
# endif


# if !defined (LDAV_SYMBOL) && (defined (hpux) || defined (SVR4) || defined (ISC) || defined (sgi) || (defined (_AIX) && !defined(HAVE_LIBPERFSTAT)))
#  define LDAV_SYMBOL "avenrun"
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

 
# if !defined (LOAD_AVE_TYPE) && (defined (BSD) || defined (LDAV_CVT) || defined (KERNEL_FILE) || defined (LDAV_SYMBOL))
#  define LOAD_AVE_TYPE double
# endif

# ifdef LOAD_AVE_TYPE

#  ifndef __VMS
#   if !(defined __linux__ || defined __ANDROID__)
#    ifndef NLIST_STRUCT
#     include <a.out.h>
#    else  
#     include <nlist.h>
#    endif  

#    ifdef SUNOS_5
#     include <kvm.h>
#     include <kstat.h>
#    endif

#    if defined (hpux) && defined (HAVE_PSTAT_GETDYNAMIC)
#     include <sys/pstat.h>
#    endif

#    ifndef KERNEL_FILE
#     define KERNEL_FILE "/vmunix"
#    endif  

#    ifndef LDAV_SYMBOL
#     define LDAV_SYMBOL "_avenrun"
#    endif  
#   endif  

#  else  

#   ifndef eunice
#    include <iodef.h>
#    include <descrip.h>
#   else  
#    include <vms/iodef.h>
#   endif  
#  endif  

#  ifndef LDAV_CVT
#   define LDAV_CVT(n) ((double) (n))
#  endif  

# endif  

# if defined HAVE_LIBPERFSTAT
#  include <sys/protosw.h>
#  include <libperfstat.h>
#  include <sys/proc.h>
#  ifndef SBITS
#   define SBITS 16
#  endif
# endif

# if defined (__GNU__) && !defined (NeXT)
 
 
#  define NeXT
#  define host_self mach_host_self
# endif

# ifdef NeXT
#  ifdef HAVE_MACH_MACH_H
#   include <mach/mach.h>
#  else
#   include <mach.h>
#  endif
# endif  

# ifdef sgi
#  include <sys/sysmp.h>
# endif  

# ifdef UMAX
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/wait.h>
#  include <sys/syscall.h>

#  ifdef UMAX_43
#   include <machine/cpu.h>
#   include <inq_stats/statistics.h>
#   include <inq_stats/sysstats.h>
#   include <inq_stats/cpustats.h>
#   include <inq_stats/procstats.h>
#  else  
#   include <sys/sysdefs.h>
#   include <sys/statistics.h>
#   include <sys/sysstats.h>
#   include <sys/cpudefs.h>
#   include <sys/cpustats.h>
#   include <sys/procstats.h>
#  endif  
# endif  

# ifdef DGUX
#  include <sys/dg_sys_info.h>
# endif

# if (defined __linux__ || defined __ANDROID__ \
      || defined __CYGWIN__ || defined SUNOS_5 \
      || (defined LOAD_AVE_TYPE && ! defined __VMS))
#  include <fcntl.h>
# endif

 

# ifdef NeXT
static processor_set_t default_set;
static bool getloadavg_initialized;
# endif  

# ifdef UMAX
static unsigned int cpus = 0;
static unsigned int samples;
# endif  

# ifdef DGUX
static struct dg_sys_info_load_info load_info;   
# endif  

# if !defined (HAVE_LIBKSTAT) && defined (LOAD_AVE_TYPE)
 
static int channel;
 
static bool getloadavg_initialized;
 
static long offset;

#  if ! defined __VMS && ! defined sgi && ! (defined __linux__ || defined __ANDROID__)
static struct nlist name_list[2];
#  endif

#  ifdef SUNOS_5
static kvm_t *kd;
#  endif  

# endif  

 

int
getloadavg (double loadavg[], int nelem)
{
  int elem = 0;                  

# ifdef NO_GET_LOAD_AVG
#  define LDAV_DONE
  errno = ENOSYS;
  elem = -1;
# endif

# if !defined (LDAV_DONE) && defined (HAVE_LIBKSTAT)        
 
#  define LDAV_DONE
  kstat_ctl_t *kc;
  kstat_t *ksp;
  kstat_named_t *kn;
  int saved_errno;

  kc = kstat_open ();
  if (kc == NULL)
    return -1;
  ksp = kstat_lookup (kc, "unix", 0, "system_misc");
  if (ksp == NULL)
    return -1;
  if (kstat_read (kc, ksp, 0) == -1)
    return -1;


  kn = kstat_data_lookup (ksp, "avenrun_1min");
  if (kn == NULL)
    {
       
      nelem = 0;
      elem = -1;
    }

  if (nelem >= 1)
    loadavg[elem++] = (double) kn->value.ul / FSCALE;

  if (nelem >= 2)
    {
      kn = kstat_data_lookup (ksp, "avenrun_5min");
      if (kn != NULL)
        {
          loadavg[elem++] = (double) kn->value.ul / FSCALE;

          if (nelem >= 3)
            {
              kn = kstat_data_lookup (ksp, "avenrun_15min");
              if (kn != NULL)
                loadavg[elem++] = (double) kn->value.ul / FSCALE;
            }
        }
    }

  saved_errno = errno;
  kstat_close (kc);
  errno = saved_errno;
# endif  

# if !defined (LDAV_DONE) && defined (hpux) && defined (HAVE_PSTAT_GETDYNAMIC)
                                                            
 
#  define LDAV_DONE
#  undef LOAD_AVE_TYPE

  struct pst_dynamic dyn_info;
  if (pstat_getdynamic (&dyn_info, sizeof (dyn_info), 0, 0) < 0)
    return -1;
  if (nelem > 0)
    loadavg[elem++] = dyn_info.psd_avg_1_min;
  if (nelem > 1)
    loadavg[elem++] = dyn_info.psd_avg_5_min;
  if (nelem > 2)
    loadavg[elem++] = dyn_info.psd_avg_15_min;

# endif  

# if ! defined LDAV_DONE && defined HAVE_LIBPERFSTAT        
#  define LDAV_DONE
#  undef LOAD_AVE_TYPE
 
  {
    perfstat_cpu_total_t cpu_stats;
    int result = perfstat_cpu_total (NULL, &cpu_stats, sizeof cpu_stats, 1);
    if (result == -1)
      return result;
    loadavg[0] = cpu_stats.loadavg[0] / (double)(1 << SBITS);
    loadavg[1] = cpu_stats.loadavg[1] / (double)(1 << SBITS);
    loadavg[2] = cpu_stats.loadavg[2] / (double)(1 << SBITS);
    elem = 3;
  }
# endif

# if !defined (LDAV_DONE) && (defined __linux__ || defined __ANDROID__ || defined __CYGWIN__)
                                       
#  define LDAV_DONE
#  undef LOAD_AVE_TYPE

#  ifndef LINUX_LDAV_FILE
#   define LINUX_LDAV_FILE "/proc/loadavg"
#  endif

  char ldavgbuf[3 * (INT_STRLEN_BOUND (int) + sizeof ".00 ")];
  char const *ptr = ldavgbuf;
  int fd, count, saved_errno;

  fd = open (LINUX_LDAV_FILE, O_RDONLY | O_CLOEXEC);
  if (fd == -1)
    return -1;
  count = read (fd, ldavgbuf, sizeof ldavgbuf - 1);
  saved_errno = errno;
  (void) close (fd);
  errno = saved_errno;
  if (count <= 0)
    return -1;
  ldavgbuf[count] = '\0';

  for (elem = 0; elem < nelem; elem++)
    {
      double numerator = 0;
      double denominator = 1;

      while (*ptr == ' ')
        ptr++;

       
      if (! ('0' <= *ptr && *ptr <= '9'))
        {
          if (elem == 0)
            {
              errno = ENOTSUP;
              return -1;
            }
          break;
        }

      while ('0' <= *ptr && *ptr <= '9')
        numerator = 10 * numerator + (*ptr++ - '0');

      if (*ptr == '.')
        for (ptr++; '0' <= *ptr && *ptr <= '9'; ptr++)
          numerator = 10 * numerator + (*ptr - '0'), denominator *= 10;

      loadavg[elem] = numerator / denominator;
    }

  return elem;

# endif  

# if !defined (LDAV_DONE) && defined (__NetBSD__)           
#  define LDAV_DONE
#  undef LOAD_AVE_TYPE

#  ifndef NETBSD_LDAV_FILE
#   define NETBSD_LDAV_FILE "/kern/loadavg"
#  endif

  unsigned long int load_ave[3], scale;
  int count;
  char readbuf[4 * INT_BUFSIZE_BOUND (unsigned long int) + 1];
  int fd = open (NETBSD_LDAV_FILE, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return fd;
  int nread = read (fd, readbuf, sizeof readbuf - 1);
  int err = errno;
  close (fd);
  if (nread < 0)
    {
      errno = err;
      return -1;
    }
  readbuf[nread] = '\0';
  count = sscanf (readbuf, "%lu %lu %lu %lu\n",
                  &load_ave[0], &load_ave[1], &load_ave[2],
                  &scale);
  if (count != 4)
    {
      errno = ENOTSUP;
      return -1;
    }

  for (elem = 0; elem < nelem; elem++)
    loadavg[elem] = (double) load_ave[elem] / (double) scale;

  return elem;

# endif  

# if !defined (LDAV_DONE) && defined (NeXT)                 
#  define LDAV_DONE
   

  host_t host;
  struct processor_set_basic_info info;
  unsigned int info_count;

   

  if (!getloadavg_initialized)
    {
      if (processor_set_default (host_self (), &default_set) == KERN_SUCCESS)
        getloadavg_initialized = true;
    }

  if (getloadavg_initialized)
    {
      info_count = PROCESSOR_SET_BASIC_INFO_COUNT;
      if (processor_set_info (default_set, PROCESSOR_SET_BASIC_INFO, &host,
                              (processor_set_info_t) &info, &info_count)
          != KERN_SUCCESS)
        getloadavg_initialized = false;
      else
        {
          if (nelem > 0)
            loadavg[elem++] = (double) info.load_average / LOAD_SCALE;
        }
    }

  if (!getloadavg_initialized)
    {
      errno = ENOTSUP;
      return -1;
    }
# endif  

# if !defined (LDAV_DONE) && defined (UMAX)
#  define LDAV_DONE
 

  struct proc_summary proc_sum_data;
  struct stat_descr proc_info;
  double load;
  register unsigned int i, j;

  if (cpus == 0)
    {
      register unsigned int c, i;
      struct cpu_config conf;
      struct stat_descr desc;

      desc.sd_next = 0;
      desc.sd_subsys = SUBSYS_CPU;
      desc.sd_type = CPUTYPE_CONFIG;
      desc.sd_addr = (char *) &conf;
      desc.sd_size = sizeof conf;

      if (inq_stats (1, &desc))
        return -1;

      c = 0;
      for (i = 0; i < conf.config_maxclass; ++i)
        {
          struct class_stats stats;
          memset (&stats, 0, sizeof stats);

          desc.sd_type = CPUTYPE_CLASS;
          desc.sd_objid = i;
          desc.sd_addr = (char *) &stats;
          desc.sd_size = sizeof stats;

          if (inq_stats (1, &desc))
            return -1;

          c += stats.class_numcpus;
        }
      cpus = c;
      samples = cpus < 2 ? 3 : (2 * cpus / 3);
    }

  proc_info.sd_next = 0;
  proc_info.sd_subsys = SUBSYS_PROC;
  proc_info.sd_type = PROCTYPE_SUMMARY;
  proc_info.sd_addr = (char *) &proc_sum_data;
  proc_info.sd_size = sizeof (struct proc_summary);
  proc_info.sd_sizeused = 0;

  if (inq_stats (1, &proc_info) != 0)
    return -1;

  load = proc_sum_data.ps_nrunnable;
  j = 0;
  for (i = samples - 1; i > 0; --i)
    {
      load += proc_sum_data.ps_nrun[j];
      if (j++ == PS_NRUNSIZE)
        j = 0;
    }

  if (nelem > 0)
    loadavg[elem++] = load / samples / cpus;
# endif  

# if !defined (LDAV_DONE) && defined (DGUX)
#  define LDAV_DONE
   
  dg_sys_info ((long int *) &load_info,
               DG_SYS_INFO_LOAD_INFO_TYPE,
               DG_SYS_INFO_LOAD_VERSION_0);

  if (nelem > 0)
    loadavg[elem++] = load_info.one_minute;
  if (nelem > 1)
    loadavg[elem++] = load_info.five_minute;
  if (nelem > 2)
    loadavg[elem++] = load_info.fifteen_minute;
# endif  

# if !defined (LDAV_DONE) && defined (apollo)
#  define LDAV_DONE
 

  extern void proc1_$get_loadav ();
  unsigned long load_ave[3];

  proc1_$get_loadav (load_ave);

  if (nelem > 0)
    loadavg[elem++] = load_ave[0] / 65536.0;
  if (nelem > 1)
    loadavg[elem++] = load_ave[1] / 65536.0;
  if (nelem > 2)
    loadavg[elem++] = load_ave[2] / 65536.0;
# endif  

# if !defined (LDAV_DONE) && defined (OSF_MIPS)
#  define LDAV_DONE

  struct tbl_loadavg load_ave;
  table (TBL_LOADAVG, 0, &load_ave, 1, sizeof (load_ave));
  loadavg[elem++]
    = (load_ave.tl_lscale == 0
       ? load_ave.tl_avenrun.d[0]
       : (load_ave.tl_avenrun.l[0] / (double) load_ave.tl_lscale));
# endif  

# if !defined (LDAV_DONE) && (defined (__MSDOS__) || defined (WINDOWS32))
                                                            
#  define LDAV_DONE

   
  for ( ; elem < nelem; elem++)
    {
      loadavg[elem] = 0.0;
    }
# endif   

# if !defined (LDAV_DONE) && defined (OSF_ALPHA)            
#  define LDAV_DONE

  struct tbl_loadavg load_ave;
  table (TBL_LOADAVG, 0, &load_ave, 1, sizeof (load_ave));
  for (elem = 0; elem < nelem; elem++)
    loadavg[elem]
      = (load_ave.tl_lscale == 0
         ? load_ave.tl_avenrun.d[elem]
         : (load_ave.tl_avenrun.l[elem] / (double) load_ave.tl_lscale));
# endif  

# if ! defined LDAV_DONE && defined __VMS                   
   

  LOAD_AVE_TYPE load_ave[3];
  static bool getloadavg_initialized;
#  ifdef eunice
  struct
  {
    int dsc$w_length;
    char *dsc$a_pointer;
  } descriptor;
#  endif

   
  if (!getloadavg_initialized)
    {
       
#  ifdef eunice
      descriptor.dsc$w_length = 18;
      descriptor.dsc$a_pointer = "$$VMS_LOAD_AVERAGE";
#  else
      $DESCRIPTOR (descriptor, "LAV0:");
#  endif
      if (sys$assign (&descriptor, &channel, 0, 0) & 1)
        getloadavg_initialized = true;
    }

   
  if (getloadavg_initialized
      && !(sys$qiow (0, channel, IO$_READVBLK, 0, 0, 0,
                     load_ave, 12, 0, 0, 0, 0) & 1))
    {
      sys$dassgn (channel);
      getloadavg_initialized = false;
    }

  if (!getloadavg_initialized)
    {
      errno = ENOTSUP;
      return -1;
    }
# endif  

# if ! defined LDAV_DONE && defined LOAD_AVE_TYPE && ! defined __VMS
                                                   

   

#  define LDAV_PRIVILEGED                

  LOAD_AVE_TYPE load_ave[3];

   
  if (offset == 0)
    {
#  ifndef sgi
#   if ! defined NLIST_STRUCT || ! defined N_NAME_POINTER
      strcpy (name_list[0].n_name, LDAV_SYMBOL);
      strcpy (name_list[1].n_name, "");
#   else  
#    ifdef HAVE_STRUCT_NLIST_N_UN_N_NAME
      name_list[0].n_un.n_name = LDAV_SYMBOL;
      name_list[1].n_un.n_name = 0;
#    else  
      name_list[0].n_name = LDAV_SYMBOL;
      name_list[1].n_name = 0;
#    endif  
#   endif  

#   ifndef SUNOS_5
      if (
#    if !defined (_AIX)
          nlist (KERNEL_FILE, name_list)
#    else   
          knlist (name_list, 1, sizeof (name_list[0]))
#    endif
          >= 0)
           
          {
#    ifdef FIXUP_KERNEL_SYMBOL_ADDR
            FIXUP_KERNEL_SYMBOL_ADDR (name_list);
#    endif
            offset = name_list[0].n_value;
          }
#   endif  
#  else   
      ptrdiff_t ldav_off = sysmp (MP_KERNADDR, MPKA_AVENRUN);
      if (ldav_off != -1)
        offset = (long int) ldav_off & 0x7fffffff;
#  endif  
    }

   
  if (!getloadavg_initialized)
    {
#  ifndef SUNOS_5
      int fd = open ("/dev/kmem", O_RDONLY | O_CLOEXEC);
      if (0 <= fd)
        {
          channel = fd;
          getloadavg_initialized = true;
        }
#  else  
       
      kd = kvm_open (0, 0, 0, O_RDONLY, 0);
      if (kd != NULL)
        {
           
          kvm_nlist (kd, name_list);
          offset = name_list[0].n_value;
          getloadavg_initialized = true;
        }
#  endif  
    }

   
  if (offset && getloadavg_initialized)
    {
       
#  ifndef SUNOS_5
      if (lseek (channel, offset, 0) == -1L
          || read (channel, (char *) load_ave, sizeof (load_ave))
          != sizeof (load_ave))
        {
          close (channel);
          getloadavg_initialized = false;
        }
#  else   
      if (kvm_read (kd, offset, (char *) load_ave, sizeof (load_ave))
          != sizeof (load_ave))
        {
          kvm_close (kd);
          getloadavg_initialized = false;
        }
#  endif  
    }

  if (offset == 0 || !getloadavg_initialized)
    {
      errno = ENOTSUP;
      return -1;
    }
# endif  

# if !defined (LDAV_DONE) && defined (LOAD_AVE_TYPE)  
  if (nelem > 0)
    loadavg[elem++] = LDAV_CVT (load_ave[0]);
  if (nelem > 1)
    loadavg[elem++] = LDAV_CVT (load_ave[1]);
  if (nelem > 2)
    loadavg[elem++] = LDAV_CVT (load_ave[2]);

#  define LDAV_DONE
# endif  

# if !defined LDAV_DONE
  errno = ENOSYS;
  elem = -1;
# endif
  return elem;
}
