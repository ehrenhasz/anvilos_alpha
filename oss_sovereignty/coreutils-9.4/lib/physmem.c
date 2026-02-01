 

#include <config.h>

#include "physmem.h"

#include <unistd.h>

#if HAVE_SYS_PSTAT_H
# include <sys/pstat.h>
#endif

#if HAVE_SYS_SYSMP_H
# include <sys/sysmp.h>
#endif

#if HAVE_SYS_SYSINFO_H
# include <sys/sysinfo.h>
#endif

#if HAVE_MACHINE_HAL_SYSINFO_H
# include <machine/hal_sysinfo.h>
#endif

#if HAVE_SYS_TABLE_H
# include <sys/table.h>
#endif

#include <sys/types.h>

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#if HAVE_SYS_SYSCTL_H && !(defined __GLIBC__ && defined __linux__)
# include <sys/sysctl.h>
#endif

#if HAVE_SYS_SYSTEMCFG_H
# include <sys/systemcfg.h>
#endif

#ifdef _WIN32

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

 
# undef GetModuleHandle
# define GetModuleHandle GetModuleHandleA

 
# define GetProcAddress \
   (void *) GetProcAddress

 
typedef struct
{
  DWORD dwLength;
  DWORD dwMemoryLoad;
  DWORDLONG ullTotalPhys;
  DWORDLONG ullAvailPhys;
  DWORDLONG ullTotalPageFile;
  DWORDLONG ullAvailPageFile;
  DWORDLONG ullTotalVirtual;
  DWORDLONG ullAvailVirtual;
  DWORDLONG ullAvailExtendedVirtual;
} lMEMORYSTATUSEX;
typedef BOOL (WINAPI *PFN_MS_EX) (lMEMORYSTATUSEX*);

#endif

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

 
double
physmem_total (void)
{
#if defined _SC_PHYS_PAGES && defined _SC_PAGESIZE
  {  
    double pages = sysconf (_SC_PHYS_PAGES);
    double pagesize = sysconf (_SC_PAGESIZE);
    if (0 <= pages && 0 <= pagesize)
      return pages * pagesize;
  }
#endif

#if HAVE_SYSINFO && HAVE_STRUCT_SYSINFO_MEM_UNIT
  {  
    struct sysinfo si;
    if (sysinfo(&si) == 0)
      return (double) si.totalram * si.mem_unit;
  }
#endif

#if HAVE_PSTAT_GETSTATIC
  {  
    struct pst_static pss;
    if (0 <= pstat_getstatic (&pss, sizeof pss, 1, 0))
      {
        double pages = pss.physical_memory;
        double pagesize = pss.page_size;
        if (0 <= pages && 0 <= pagesize)
          return pages * pagesize;
      }
  }
#endif

#if HAVE_SYSMP && defined MP_SAGET && defined MPSA_RMINFO && defined _SC_PAGESIZE
  {  
    struct rminfo realmem;
    if (sysmp (MP_SAGET, MPSA_RMINFO, &realmem, sizeof realmem) == 0)
      {
        double pagesize = sysconf (_SC_PAGESIZE);
        double pages = realmem.physmem;
        if (0 <= pages && 0 <= pagesize)
          return pages * pagesize;
      }
  }
#endif

#if HAVE_GETSYSINFO && defined GSI_PHYSMEM
  {  
    int physmem;

    if (getsysinfo (GSI_PHYSMEM, (caddr_t) &physmem, sizeof (physmem),
                    NULL, NULL, NULL) == 1)
      {
        double kbytes = physmem;

        if (0 <= kbytes)
          return kbytes * 1024.0;
      }
  }
#endif

#if HAVE_SYSCTL && !(defined __GLIBC__ && defined __linux__) && defined HW_PHYSMEM
  {  
    unsigned int physmem;
    size_t len = sizeof physmem;
    static int mib[2] = { CTL_HW, HW_PHYSMEM };

    if (sysctl (mib, ARRAY_SIZE (mib), &physmem, &len, NULL, 0) == 0
        && len == sizeof (physmem))
      return (double) physmem;
  }
#endif

#if HAVE__SYSTEM_CONFIGURATION
   
  return _system_configuration.physmem;
#endif

#if defined _WIN32
  {  
    PFN_MS_EX pfnex;
    HMODULE h = GetModuleHandle ("kernel32.dll");

    if (!h)
      return 0.0;

     
    if ((pfnex = (PFN_MS_EX) GetProcAddress (h, "GlobalMemoryStatusEx")))
      {
        lMEMORYSTATUSEX lms_ex;
        lms_ex.dwLength = sizeof lms_ex;
        if (!pfnex (&lms_ex))
          return 0.0;
        return (double) lms_ex.ullTotalPhys;
      }

     
    else
      {
        MEMORYSTATUS ms;
        GlobalMemoryStatus (&ms);
        return (double) ms.dwTotalPhys;
      }
  }
#endif

   
  return 64 * 1024 * 1024;
}

 
double
physmem_available (void)
{
#if defined _SC_AVPHYS_PAGES && defined _SC_PAGESIZE
  {  
    double pages = sysconf (_SC_AVPHYS_PAGES);
    double pagesize = sysconf (_SC_PAGESIZE);
    if (0 <= pages && 0 <= pagesize)
      return pages * pagesize;
  }
#endif

#if HAVE_SYSINFO && HAVE_STRUCT_SYSINFO_MEM_UNIT
  {  
    struct sysinfo si;
    if (sysinfo(&si) == 0)
      return ((double) si.freeram + si.bufferram) * si.mem_unit;
  }
#endif

#if HAVE_PSTAT_GETSTATIC && HAVE_PSTAT_GETDYNAMIC
  {  
    struct pst_static pss;
    struct pst_dynamic psd;
    if (0 <= pstat_getstatic (&pss, sizeof pss, 1, 0)
        && 0 <= pstat_getdynamic (&psd, sizeof psd, 1, 0))
      {
        double pages = psd.psd_free;
        double pagesize = pss.page_size;
        if (0 <= pages && 0 <= pagesize)
          return pages * pagesize;
      }
  }
#endif

#if HAVE_SYSMP && defined MP_SAGET && defined MPSA_RMINFO && defined _SC_PAGESIZE
  {  
    struct rminfo realmem;
    if (sysmp (MP_SAGET, MPSA_RMINFO, &realmem, sizeof realmem) == 0)
      {
        double pagesize = sysconf (_SC_PAGESIZE);
        double pages = realmem.availrmem;
        if (0 <= pages && 0 <= pagesize)
          return pages * pagesize;
      }
  }
#endif

#if HAVE_TABLE && defined TBL_VMSTATS
  {  
    struct tbl_vmstats vmstats;

    if (table (TBL_VMSTATS, 0, &vmstats, 1, sizeof (vmstats)) == 1)
      {
        double pages = vmstats.free_count;
        double pagesize = vmstats.pagesize;

        if (0 <= pages && 0 <= pagesize)
          return pages * pagesize;
      }
  }
#endif

#if HAVE_SYSCTL && !(defined __GLIBC__ && defined __linux__) && defined HW_USERMEM
  {  
    unsigned int usermem;
    size_t len = sizeof usermem;
    static int mib[2] = { CTL_HW, HW_USERMEM };

    if (sysctl (mib, ARRAY_SIZE (mib), &usermem, &len, NULL, 0) == 0
        && len == sizeof (usermem))
      return (double) usermem;
  }
#endif

#if defined _WIN32
  {  
    PFN_MS_EX pfnex;
    HMODULE h = GetModuleHandle ("kernel32.dll");

    if (!h)
      return 0.0;

     
    if ((pfnex = (PFN_MS_EX) GetProcAddress (h, "GlobalMemoryStatusEx")))
      {
        lMEMORYSTATUSEX lms_ex;
        lms_ex.dwLength = sizeof lms_ex;
        if (!pfnex (&lms_ex))
          return 0.0;
        return (double) lms_ex.ullAvailPhys;
      }

     
    else
      {
        MEMORYSTATUS ms;
        GlobalMemoryStatus (&ms);
        return (double) ms.dwAvailPhys;
      }
  }
#endif

   
  return physmem_total () / 4;
}


#if DEBUG

# include <stdio.h>
# include <stdlib.h>

int
main (void)
{
  printf ("%12.f %12.f\n", physmem_total (), physmem_available ());
  exit (0);
}

#endif  

 
