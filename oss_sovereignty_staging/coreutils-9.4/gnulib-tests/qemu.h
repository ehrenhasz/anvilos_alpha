 

#ifdef __linux__
# include <fcntl.h>
# include <string.h>
# include <unistd.h>
#endif

 

static bool
is_running_under_qemu_user (void)
{
#ifdef __linux__
  char buf[4096 + 1];
  int fd;

# if defined __m68k__
  fd = open ("/proc/hardware", O_RDONLY);
  if (fd >= 0)
    {
      int n = read (fd, buf, sizeof (buf) - 1);
      close (fd);
      if (n > 0)
        {
          buf[n] = '\0';
          if (strstr (buf, "qemu") != NULL)
            return true;
        }
    }
# endif

  fd = open ("/proc/cpuinfo", O_RDONLY);
  if (fd >= 0)
    {
      int n = read (fd, buf, sizeof (buf) - 1);
      close (fd);
      if (n > 0)
        {
          buf[n] = '\0';
# if defined __hppa__
          if (strstr (buf, "QEMU") != NULL)
            return true;
# endif
# if !(defined __i386__ || defined __x86_64__)
          if (strstr (buf, "AuthenticAMD") != NULL
              || strstr (buf, "GenuineIntel") != NULL)
            return true;
# endif
# if !(defined __arm__ || defined __aarch64__)
          if (strstr (buf, "ARM") != NULL
              || strcasestr (buf, "aarch64") != NULL)
            return true;
# endif
# if !defined __sparc__
          if (strcasestr (buf, "SPARC") != NULL)
            return true;
# endif
# if !defined __powerpc__
          if (strstr (buf, "POWER") != NULL)
            return true;
# endif
        }
    }

   
#endif

  return false;
}
