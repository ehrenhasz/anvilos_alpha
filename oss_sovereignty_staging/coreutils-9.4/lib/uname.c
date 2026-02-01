 
#include <sys/utsname.h>

 
#if defined _WIN32 && ! defined __CYGWIN__

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <windows.h>

 
# ifndef VER_PLATFORM_WIN32_CE
#  define VER_PLATFORM_WIN32_CE 3
# endif

 
# ifndef PROCESSOR_ARCHITECTURE_AMD64
#  define PROCESSOR_ARCHITECTURE_AMD64 9
# endif
# ifndef PROCESSOR_ARCHITECTURE_IA32_ON_WIN64
#  define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 10
# endif

 
# ifndef PROCESSOR_AMD_X8664
#  define PROCESSOR_AMD_X8664 8664
# endif

 
# undef OSVERSIONINFO
# define OSVERSIONINFO OSVERSIONINFOA
# undef GetVersionEx
# define GetVersionEx GetVersionExA

int
uname (struct utsname *buf)
{
  OSVERSIONINFO version;
  OSVERSIONINFOEX versionex;
  BOOL have_versionex;  
  const char *super_version;

   
  versionex.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
  have_versionex = GetVersionEx ((OSVERSIONINFO *) &versionex);
  if (have_versionex)
    {
       
      memcpy (&version, &versionex, sizeof (OSVERSIONINFO));
    }
  else
    {
      version.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (!GetVersionEx (&version))
        abort ();
    }

   
  if (gethostname (buf->nodename, sizeof (buf->nodename)) < 0)
    strcpy (buf->nodename, "localhost");

   
  if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
       
      super_version = "NT";
    }
  else if (version.dwPlatformId == VER_PLATFORM_WIN32_CE)
    {
       
      super_version = "CE";
    }
  else if (version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
       
      switch (version.dwMinorVersion)
        {
        case 0:
          super_version = "95";
          break;
        case 10:
          super_version = "98";
          break;
        case 90:
          super_version = "ME";
          break;
        default:
          super_version = "";
          break;
        }
    }
  else
    super_version = "";

   
# ifdef __MINGW32__
   
  sprintf (buf->sysname, "MINGW32_%s-%u.%u", super_version,
           (unsigned int) version.dwMajorVersion,
           (unsigned int) version.dwMinorVersion);
# else
  sprintf (buf->sysname, "Windows%s", super_version);
# endif

   
   
  if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
       
      struct windows_version
        {
          int major;
          int minor;
          unsigned int server_offset;
          const char *name;
        };

       
      #define VERSION1(major, minor, name) \
        { major, minor, 0, name }
      #define VERSION2(major, minor, workstation, server) \
        { major, minor, sizeof workstation, workstation "\0" server }
      static const struct windows_version versions[] =
        {
          VERSION2 (3, -1, "Windows NT Workstation", "Windows NT Server"),
          VERSION2 (4, -1, "Windows NT Workstation", "Windows NT Server"),
          VERSION1 (5, 0, "Windows 2000"),
          VERSION1 (5, 1, "Windows XP"),
          VERSION1 (5, 2, "Windows Server 2003"),
          VERSION2 (6, 0, "Windows Vista", "Windows Server 2008"),
          VERSION2 (6, 1, "Windows 7", "Windows Server 2008 R2"),
          VERSION2 (-1, -1, "Windows", "Windows Server")
        };
      const char *base;
      const struct windows_version *v = versions;

       
      while ((v->major != version.dwMajorVersion && v->major != -1)
             || (v->minor != version.dwMinorVersion && v->minor != -1))
        v++;

      if (have_versionex && versionex.wProductType != VER_NT_WORKSTATION)
        base = v->name + v->server_offset;
      else
        base = v->name;
      if (v->major == -1 || v->minor == -1)
        sprintf (buf->release, "%s %u.%u",
                 base,
                 (unsigned int) version.dwMajorVersion,
                 (unsigned int) version.dwMinorVersion);
      else
        strcpy (buf->release, base);
    }
  else if (version.dwPlatformId == VER_PLATFORM_WIN32_CE)
    {
       
      sprintf (buf->release, "Windows CE %u.%u",
               (unsigned int) version.dwMajorVersion,
               (unsigned int) version.dwMinorVersion);
    }
  else
    {
       
      sprintf (buf->release, "Windows %s", super_version);
    }
  strcpy (buf->version, version.szCSDVersion);

   
  {
    SYSTEM_INFO info;

    GetSystemInfo (&info);
     
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT
        || version.dwPlatformId == VER_PLATFORM_WIN32_CE)
      {
         
        switch (info.wProcessorArchitecture)
          {
          case PROCESSOR_ARCHITECTURE_AMD64:
            strcpy (buf->machine, "x86_64");
            break;
          case PROCESSOR_ARCHITECTURE_IA64:
            strcpy (buf->machine, "ia64");
            break;
          case PROCESSOR_ARCHITECTURE_INTEL:
            strcpy (buf->machine, "i386");
            if (info.wProcessorLevel >= 3)
              buf->machine[1] =
                '0' + (info.wProcessorLevel <= 6 ? info.wProcessorLevel : 6);
            break;
          case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
            strcpy (buf->machine, "i686");
            break;
          case PROCESSOR_ARCHITECTURE_MIPS:
            strcpy (buf->machine, "mips");
            break;
          case PROCESSOR_ARCHITECTURE_ALPHA:
          case PROCESSOR_ARCHITECTURE_ALPHA64:
            strcpy (buf->machine, "alpha");
            break;
          case PROCESSOR_ARCHITECTURE_PPC:
            strcpy (buf->machine, "powerpc");
            break;
          case PROCESSOR_ARCHITECTURE_SHX:
            strcpy (buf->machine, "sh");
            break;
          case PROCESSOR_ARCHITECTURE_ARM:
            strcpy (buf->machine, "arm");
            break;
          default:
            strcpy (buf->machine, "unknown");
            break;
          }
      }
    else
      {
         
        switch (info.dwProcessorType)
          {
          case PROCESSOR_AMD_X8664:
            strcpy (buf->machine, "x86_64");
            break;
          case PROCESSOR_INTEL_IA64:
            strcpy (buf->machine, "ia64");
            break;
          default:
            if (info.dwProcessorType % 100 == 86)
              sprintf (buf->machine, "i%u",
                       (unsigned int) info.dwProcessorType);
            else
              strcpy (buf->machine, "unknown");
            break;
          }
      }
  }

  return 0;
}

#endif
