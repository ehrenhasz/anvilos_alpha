 
#if defined __sun && !defined _LP64 && _FILE_OFFSET_BITS == 64
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#ifdef __ANDROID__
# undef _FILE_OFFSET_BITS
#endif

 
#include "vma-iter.h"

#include <errno.h>  
#include <stdlib.h>  
#include <fcntl.h>  
#include <unistd.h>  

#if defined __linux__ || defined __ANDROID__
# include <limits.h>  
#endif

#if defined __linux__ || defined __ANDROID__ || defined __FreeBSD_kernel__ || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined __minix  
# include <sys/types.h>
# include <sys/mman.h>  
#endif
#if defined __minix
# include <string.h>  
#endif

#if defined __FreeBSD__ || defined __FreeBSD_kernel__  
# include <sys/types.h>
# include <sys/mman.h>  
# include <sys/user.h>  
# include <sys/sysctl.h>  
#endif
#if defined __NetBSD__ || defined __OpenBSD__  
# include <sys/types.h>
# include <sys/mman.h>  
# include <sys/sysctl.h>  
#endif

#if defined _AIX  
# include <string.h>  
# include <sys/types.h>
# include <sys/mman.h>  
# include <sys/procfs.h>  
#endif

#if defined __sgi || defined __osf__  
# include <string.h>  
# include <sys/types.h>
# include <sys/mman.h>  
# include <sys/procfs.h>  
#endif

#if defined __sun  
# include <string.h>  
# include <sys/types.h>
# include <sys/mman.h>  
 
# define _STRUCTURED_PROC 1
# include <sys/procfs.h>  
#endif

#if HAVE_PSTAT_GETPROCVM  
# include <sys/pstat.h>  
#endif

#if defined __APPLE__ && defined __MACH__  
# include <mach/mach.h>
#endif

#if defined __GNU__  
# include <mach/mach.h>
#endif

#if defined _WIN32 || defined __CYGWIN__  
# include <windows.h>
#endif

#if defined __BEOS__ || defined __HAIKU__  
# include <OS.h>
#endif

#if HAVE_MQUERY  
# include <sys/types.h>
# include <sys/mman.h>  
#endif


 

#if defined __linux__ || defined __ANDROID__ || defined __FreeBSD_kernel__ || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined __minix  

 

# if defined __linux__ || defined __ANDROID__
   
#  define MIN_LEFTOVER (73 + PATH_MAX)
# else
#  define MIN_LEFTOVER 1
# endif

# ifdef TEST
 
#  define STACK_ALLOCATED_BUFFER_SIZE 32
# else
#  if MIN_LEFTOVER < 1024
#   define STACK_ALLOCATED_BUFFER_SIZE 1024
#  else
     
#   define STACK_ALLOCATED_BUFFER_SIZE 1
#  endif
# endif

struct rofile
  {
    size_t position;
    size_t filled;
    int eof_seen;
     
    char *buffer;
    char *auxmap;
    size_t auxmap_length;
    unsigned long auxmap_start;
    unsigned long auxmap_end;
    char stack_allocated_buffer[STACK_ALLOCATED_BUFFER_SIZE];
  };

 
static int
rof_open (struct rofile *rof, const char *filename)
{
  int fd;
  unsigned long pagesize;
  size_t size;

  fd = open (filename, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;
  rof->position = 0;
  rof->eof_seen = 0;
   
  pagesize = 0;
  rof->buffer = rof->stack_allocated_buffer;
  size = sizeof (rof->stack_allocated_buffer);
  rof->auxmap = NULL;
  rof->auxmap_start = 0;
  rof->auxmap_end = 0;
  for (;;)
    {
       
      if (size > MIN_LEFTOVER)
        {
          int n = read (fd, rof->buffer, size);
          if (n < 0 && errno == EINTR)
            goto retry;
# if defined __DragonFly__
          if (!(n < 0 && errno == EFBIG))
# endif
            {
              if (n <= 0)
                 
                goto fail1;
              if (n + MIN_LEFTOVER <= size)
                {
                   
                  rof->filled = n;
# if defined __linux__ || defined __ANDROID__
                   
                  for (;;)
                    {
                      n = read (fd, rof->buffer + rof->filled, size - rof->filled);
                      if (n < 0 && errno == EINTR)
                        goto retry;
                      if (n < 0)
                         
                        goto fail1;
                      if (n + MIN_LEFTOVER > size - rof->filled)
                         
                        break;
                      if (n == 0)
                        {
                           
                          close (fd);
                          return 0;
                        }
                      rof->filled += n;
                    }
# else
                  close (fd);
                  return 0;
# endif
                }
            }
        }
       
      if (pagesize == 0)
        {
          pagesize = getpagesize ();
          size = pagesize;
          while (size <= MIN_LEFTOVER)
            size = 2 * size;
        }
      else
        {
          size = 2 * size;
          if (size == 0)
             
            goto fail1;
          if (rof->auxmap != NULL)
            munmap (rof->auxmap, rof->auxmap_length);
        }
      rof->auxmap = (void *) mmap ((void *) 0, size, PROT_READ | PROT_WRITE,
                                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      if (rof->auxmap == (void *) -1)
        {
          close (fd);
          return -1;
        }
      rof->auxmap_length = size;
      rof->auxmap_start = (unsigned long) rof->auxmap;
      rof->auxmap_end = rof->auxmap_start + size;
      rof->buffer = (char *) rof->auxmap;
     retry:
       
      if (lseek (fd, 0, SEEK_SET) < 0)
        {
          close (fd);
          fd = open (filename, O_RDONLY | O_CLOEXEC);
          if (fd < 0)
            goto fail2;
        }
    }
 fail1:
  close (fd);
 fail2:
  if (rof->auxmap != NULL)
    munmap (rof->auxmap, rof->auxmap_length);
  return -1;
}

 
static int
rof_peekchar (struct rofile *rof)
{
  if (rof->position == rof->filled)
    {
      rof->eof_seen = 1;
      return -1;
    }
  return (unsigned char) rof->buffer[rof->position];
}

 
static int
rof_getchar (struct rofile *rof)
{
  int c = rof_peekchar (rof);
  if (c >= 0)
    rof->position++;
  return c;
}

 
static int
rof_scanf_lx (struct rofile *rof, unsigned long *valuep)
{
  unsigned long value = 0;
  unsigned int numdigits = 0;
  for (;;)
    {
      int c = rof_peekchar (rof);
      if (c >= '0' && c <= '9')
        value = (value << 4) + (c - '0');
      else if (c >= 'A' && c <= 'F')
        value = (value << 4) + (c - 'A' + 10);
      else if (c >= 'a' && c <= 'f')
        value = (value << 4) + (c - 'a' + 10);
      else
        break;
      rof_getchar (rof);
      numdigits++;
    }
  if (numdigits == 0)
    return -1;
  *valuep = value;
  return 0;
}

 
static void
rof_close (struct rofile *rof)
{
  if (rof->auxmap != NULL)
    munmap (rof->auxmap, rof->auxmap_length);
}

#endif


 

#if defined __linux__ || defined __ANDROID__ || (defined __FreeBSD_kernel__ && !defined __FreeBSD__)  
 

static int
vma_iterate_proc (vma_iterate_callback_fn callback, void *data)
{
  struct rofile rof;

   
  if (rof_open (&rof, "/proc/self/maps") >= 0)
    {
      unsigned long auxmap_start = rof.auxmap_start;
      unsigned long auxmap_end = rof.auxmap_end;

      for (;;)
        {
          unsigned long start, end;
          unsigned int flags;
          int c;

           
          if (!(rof_scanf_lx (&rof, &start) >= 0
                && rof_getchar (&rof) == '-'
                && rof_scanf_lx (&rof, &end) >= 0))
            break;
           
          do
            c = rof_getchar (&rof);
          while (c == ' ');
          flags = 0;
          if (c == 'r')
            flags |= VMA_PROT_READ;
          c = rof_getchar (&rof);
          if (c == 'w')
            flags |= VMA_PROT_WRITE;
          c = rof_getchar (&rof);
          if (c == 'x')
            flags |= VMA_PROT_EXECUTE;
          while (c = rof_getchar (&rof), c != -1 && c != '\n')
            ;

          if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
            {
               
              if (start < auxmap_start)
                if (callback (data, start, auxmap_start, flags))
                  break;
              if (auxmap_end - 1 < end - 1)
                if (callback (data, auxmap_end, end, flags))
                  break;
            }
          else
            {
              if (callback (data, start, end, flags))
                break;
            }
        }
      rof_close (&rof);
      return 0;
    }

  return -1;
}

#elif defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__

static int
vma_iterate_proc (vma_iterate_callback_fn callback, void *data)
{
  struct rofile rof;

   
  if (rof_open (&rof, "/proc/curproc/map") >= 0)
    {
      unsigned long auxmap_start = rof.auxmap_start;
      unsigned long auxmap_end = rof.auxmap_end;

      for (;;)
        {
          unsigned long start, end;
          unsigned int flags;
          int c;

           
          if (!(rof_getchar (&rof) == '0'
                && rof_getchar (&rof) == 'x'
                && rof_scanf_lx (&rof, &start) >= 0))
            break;
          while (c = rof_peekchar (&rof), c == ' ' || c == '\t')
            rof_getchar (&rof);
           
          if (!(rof_getchar (&rof) == '0'
                && rof_getchar (&rof) == 'x'
                && rof_scanf_lx (&rof, &end) >= 0))
            break;
# if defined __FreeBSD__ || defined __DragonFly__
           
          do
            c = rof_getchar (&rof);
          while (c == ' ');
          do
            c = rof_getchar (&rof);
          while (c != -1 && c != '\n' && c != ' ');
           
          do
            c = rof_getchar (&rof);
          while (c == ' ');
          do
            c = rof_getchar (&rof);
          while (c != -1 && c != '\n' && c != ' ');
           
          do
            c = rof_getchar (&rof);
          while (c == ' ');
          do
            c = rof_getchar (&rof);
          while (c != -1 && c != '\n' && c != ' ');
# endif
           
          do
            c = rof_getchar (&rof);
          while (c == ' ');
          flags = 0;
          if (c == 'r')
            flags |= VMA_PROT_READ;
          c = rof_getchar (&rof);
          if (c == 'w')
            flags |= VMA_PROT_WRITE;
          c = rof_getchar (&rof);
          if (c == 'x')
            flags |= VMA_PROT_EXECUTE;
          while (c = rof_getchar (&rof), c != -1 && c != '\n')
            ;

          if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
            {
               
              if (start < auxmap_start)
                if (callback (data, start, auxmap_start, flags))
                  break;
              if (auxmap_end - 1 < end - 1)
                if (callback (data, auxmap_end, end, flags))
                  break;
            }
          else
            {
              if (callback (data, start, end, flags))
                break;
            }
        }
      rof_close (&rof);
      return 0;
    }

  return -1;
}

#elif defined __minix

static int
vma_iterate_proc (vma_iterate_callback_fn callback, void *data)
{
  char fnamebuf[6+10+4+1];
  char *fname;
  struct rofile rof;

   
  fname = fnamebuf + sizeof (fnamebuf) - (4 + 1);
  memcpy (fname, "/map", 4 + 1);
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

   
  if (rof_open (&rof, fname) >= 0)
    {
      unsigned long auxmap_start = rof.auxmap_start;
      unsigned long auxmap_end = rof.auxmap_end;

      for (;;)
        {
          unsigned long start, end;
          unsigned int flags;
          int c;

           
          if (!(rof_scanf_lx (&rof, &start) >= 0
                && rof_getchar (&rof) == '-'
                && rof_scanf_lx (&rof, &end) >= 0))
            break;
           
          do
            c = rof_getchar (&rof);
          while (c == ' ');
          flags = 0;
          if (c == 'r')
            flags |= VMA_PROT_READ;
          c = rof_getchar (&rof);
          if (c == 'w')
            flags |= VMA_PROT_WRITE;
          c = rof_getchar (&rof);
          if (c == 'x')
            flags |= VMA_PROT_EXECUTE;
          while (c = rof_getchar (&rof), c != -1 && c != '\n')
            ;

          if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
            {
               
              if (start < auxmap_start)
                if (callback (data, start, auxmap_start, flags))
                  break;
              if (auxmap_end - 1 < end - 1)
                if (callback (data, auxmap_end, end, flags))
                  break;
            }
          else
            {
              if (callback (data, start, end, flags))
                break;
            }
        }
      rof_close (&rof);
      return 0;
    }

  return -1;
}

#else

static inline int
vma_iterate_proc (vma_iterate_callback_fn callback, void *data)
{
  return -1;
}

#endif


 

#if (defined __FreeBSD__ || defined __FreeBSD_kernel__) && defined KERN_PROC_VMMAP  

static int
vma_iterate_bsd (vma_iterate_callback_fn callback, void *data)
{
   
  int info_path[] = { CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid () };
  size_t len;
  size_t pagesize;
  size_t memneed;
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  char *mem;
  char *p;
  char *p_end;

  len = 0;
  if (sysctl (info_path, 4, NULL, &len, NULL, 0) < 0)
    return -1;
   
  len = 2 * len + 200;
   
  pagesize = getpagesize ();
  memneed = len;
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (auxmap == (void *) -1)
    return -1;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  mem = (char *) auxmap;
  if (sysctl (info_path, 4, mem, &len, NULL, 0) < 0)
    {
      munmap (auxmap, memneed);
      return -1;
    }
  p = mem;
  p_end = mem + len;
  while (p < p_end)
    {
      struct kinfo_vmentry *kve = (struct kinfo_vmentry *) p;
      unsigned long start = kve->kve_start;
      unsigned long end = kve->kve_end;
      unsigned int flags = 0;
      if (kve->kve_protection & KVME_PROT_READ)
        flags |= VMA_PROT_READ;
      if (kve->kve_protection & KVME_PROT_WRITE)
        flags |= VMA_PROT_WRITE;
      if (kve->kve_protection & KVME_PROT_EXEC)
        flags |= VMA_PROT_EXECUTE;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
           
          if (start < auxmap_start)
            if (callback (data, start, auxmap_start, flags))
              break;
          if (auxmap_end - 1 < end - 1)
            if (callback (data, auxmap_end, end, flags))
              break;
        }
      else
        {
          if (callback (data, start, end, flags))
            break;
        }
      p += kve->kve_structsize;
    }
  munmap (auxmap, memneed);
  return 0;
}

#elif defined __NetBSD__ && defined VM_PROC_MAP  

static int
vma_iterate_bsd (vma_iterate_callback_fn callback, void *data)
{
   
  unsigned int entry_size =
     
    offsetof (struct kinfo_vmentry, kve_path);
  int info_path[] = { CTL_VM, VM_PROC, VM_PROC_MAP, getpid (), entry_size };
  size_t len;
  size_t pagesize;
  size_t memneed;
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  char *mem;
  char *p;
  char *p_end;

  len = 0;
  if (sysctl (info_path, 5, NULL, &len, NULL, 0) < 0)
    return -1;
   
  len = 2 * len + 10 * entry_size;
   
  if (len > 0x100000)
    len = 0x100000;
   
  len = (len / entry_size) * entry_size;
   
  pagesize = getpagesize ();
  memneed = len;
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (auxmap == (void *) -1)
    return -1;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  mem = (char *) auxmap;
  if (sysctl (info_path, 5, mem, &len, NULL, 0) < 0
      || len > 0x100000 - entry_size)
    {
       
      munmap (auxmap, memneed);
      return -1;
    }
  p = mem;
  p_end = mem + len;
  while (p < p_end)
    {
      struct kinfo_vmentry *kve = (struct kinfo_vmentry *) p;
      unsigned long start = kve->kve_start;
      unsigned long end = kve->kve_end;
      unsigned int flags = 0;
      if (kve->kve_protection & KVME_PROT_READ)
        flags |= VMA_PROT_READ;
      if (kve->kve_protection & KVME_PROT_WRITE)
        flags |= VMA_PROT_WRITE;
      if (kve->kve_protection & KVME_PROT_EXEC)
        flags |= VMA_PROT_EXECUTE;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
           
          if (start < auxmap_start)
            if (callback (data, start, auxmap_start, flags))
              break;
          if (auxmap_end - 1 < end - 1)
            if (callback (data, auxmap_end, end, flags))
              break;
        }
      else
        {
          if (callback (data, start, end, flags))
            break;
        }
      p += entry_size;
    }
  munmap (auxmap, memneed);
  return 0;
}

#elif defined __OpenBSD__ && defined KERN_PROC_VMMAP  

static int
vma_iterate_bsd (vma_iterate_callback_fn callback, void *data)
{
   
  int info_path[] = { CTL_KERN, KERN_PROC_VMMAP, getpid () };
  size_t len;
  size_t pagesize;
  size_t memneed;
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  char *mem;
  char *p;
  char *p_end;

  len = 0;
  if (sysctl (info_path, 3, NULL, &len, NULL, 0) < 0)
    return -1;
   
  len = 2 * len + 10 * sizeof (struct kinfo_vmentry);
   
  if (len > 0x10000)
    len = 0x10000;
   
  len = (len / sizeof (struct kinfo_vmentry)) * sizeof (struct kinfo_vmentry);
   
  pagesize = getpagesize ();
  memneed = len;
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (auxmap == (void *) -1)
    return -1;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  mem = (char *) auxmap;
  if (sysctl (info_path, 3, mem, &len, NULL, 0) < 0
      || len > 0x10000 - sizeof (struct kinfo_vmentry))
    {
       
      munmap (auxmap, memneed);
      return -1;
    }
  p = mem;
  p_end = mem + len;
  while (p < p_end)
    {
      struct kinfo_vmentry *kve = (struct kinfo_vmentry *) p;
      unsigned long start = kve->kve_start;
      unsigned long end = kve->kve_end;
      unsigned int flags = 0;
      if (kve->kve_protection & KVE_PROT_READ)
        flags |= VMA_PROT_READ;
      if (kve->kve_protection & KVE_PROT_WRITE)
        flags |= VMA_PROT_WRITE;
      if (kve->kve_protection & KVE_PROT_EXEC)
        flags |= VMA_PROT_EXECUTE;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
           
          if (start < auxmap_start)
            if (callback (data, start, auxmap_start, flags))
              break;
          if (auxmap_end - 1 < end - 1)
            if (callback (data, auxmap_end, end, flags))
              break;
        }
      else
        {
          if (start != end)
            if (callback (data, start, end, flags))
              break;
        }
      p += sizeof (struct kinfo_vmentry);
    }
  munmap (auxmap, memneed);
  return 0;
}

#else

static inline int
vma_iterate_bsd (vma_iterate_callback_fn callback, void *data)
{
  return -1;
}

#endif


int
vma_iterate (vma_iterate_callback_fn callback, void *data)
{
#if defined __linux__ || defined __ANDROID__ || defined __FreeBSD_kernel__ || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined __minix  

# if defined __FreeBSD__
   
  int retval = vma_iterate_bsd (callback, data);
  if (retval == 0)
      return 0;

  return vma_iterate_proc (callback, data);
# else
   
  int retval = vma_iterate_proc (callback, data);
  if (retval == 0)
      return 0;

  return vma_iterate_bsd (callback, data);
# endif

#elif defined _AIX  

   

  size_t pagesize;
  char fnamebuf[6+10+4+1];
  char *fname;
  int fd;
  size_t memneed;

  pagesize = getpagesize ();

   
  fname = fnamebuf + sizeof (fnamebuf) - (4+1);
  memcpy (fname, "/map", 4+1);
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;

   

  for (memneed = 2 * pagesize; ; memneed = 2 * memneed)
    {
       
      void *auxmap;
      unsigned long auxmap_start;
      unsigned long auxmap_end;
      ssize_t nbytes;

      auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      if (auxmap == (void *) -1)
        {
          close (fd);
          return -1;
        }
      auxmap_start = (unsigned long) auxmap;
      auxmap_end = auxmap_start + memneed;

       
     retry:
      do
        nbytes = read (fd, auxmap, memneed);
      while (nbytes < 0 && errno == EINTR);
      if (nbytes <= 0)
        {
          munmap (auxmap, memneed);
          close (fd);
          return -1;
        }
      if (nbytes == memneed)
        {
           
          munmap (auxmap, memneed);
          if (lseek (fd, 0, SEEK_SET) < 0)
            {
              close (fd);
              return -1;
            }
        }
      else
        {
          if (read (fd, (char *) auxmap + nbytes, 1) > 0)
            {
               
              if (lseek (fd, 0, SEEK_SET) < 0)
                {
                  munmap (auxmap, memneed);
                  close (fd);
                  return -1;
                }
              goto retry;
            }

           
          prmap_t* maps = (prmap_t *) auxmap;

           
          typedef struct
            {
              uintptr_t start;
              uintptr_t end;
              unsigned int flags;
            }
          vma_t;
           
          vma_t *vmas = (vma_t *) auxmap;

          vma_t *vp = vmas;
          {
            prmap_t* mp;
            for (mp = maps;;)
              {
                unsigned long start, end;

                start = (unsigned long) mp->pr_vaddr;
                end = start + mp->pr_size;
                if (start == 0 && end == 0 && mp->pr_mflags == 0)
                  break;
                 
                if (start < end && (mp->pr_mflags & MA_KERNTEXT) == 0)
                  {
                    unsigned int flags;
                    flags = 0;
                    if (mp->pr_mflags & MA_READ)
                      flags |= VMA_PROT_READ;
                    if (mp->pr_mflags & MA_WRITE)
                      flags |= VMA_PROT_WRITE;
                    if (mp->pr_mflags & MA_EXEC)
                      flags |= VMA_PROT_EXECUTE;

                    if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
                      {
                         
                        if (start < auxmap_start)
                          {
                            vp->start = start;
                            vp->end = auxmap_start;
                            vp->flags = flags;
                            vp++;
                          }
                        if (auxmap_end - 1 < end - 1)
                          {
                            vp->start = auxmap_end;
                            vp->end = end;
                            vp->flags = flags;
                            vp++;
                          }
                      }
                    else
                      {
                        vp->start = start;
                        vp->end = end;
                        vp->flags = flags;
                        vp++;
                      }
                  }
                mp++;
              }
          }

          size_t nvmas = vp - vmas;
           
          {
            size_t i;
            for (i = 1; i < nvmas; i++)
              {
                 
                size_t j;
                for (j = i; j > 0 && vmas[j - 1].start > vmas[j].start; j--)
                  {
                    vma_t tmp = vmas[j - 1];
                    vmas[j - 1] = vmas[j];
                    vmas[j] = tmp;
                  }
                 
              }
          }

           
          {
            size_t i;
            for (i = 0; i < nvmas; i++)
              {
                vma_t *vpi = &vmas[i];
                if (callback (data, vpi->start, vpi->end, vpi->flags))
                  break;
              }
          }

          munmap (auxmap, memneed);
          break;
        }
    }

  close (fd);
  return 0;

#elif defined __sgi || defined __osf__  

  size_t pagesize;
  char fnamebuf[6+10+1];
  char *fname;
  int fd;
  int nmaps;
  size_t memneed;
# if HAVE_MAP_ANONYMOUS
#  define zero_fd -1
#  define map_flags MAP_ANONYMOUS
# else
  int zero_fd;
#  define map_flags 0
# endif
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  prmap_t* maps;
  prmap_t* mp;

  pagesize = getpagesize ();

   
  fname = fnamebuf + sizeof (fnamebuf) - 1;
  *fname = '\0';
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;

  if (ioctl (fd, PIOCNMAP, &nmaps) < 0)
    goto fail2;

  memneed = (nmaps + 10) * sizeof (prmap_t);
   
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
# if !HAVE_MAP_ANONYMOUS
  zero_fd = open ("/dev/zero", O_RDONLY | O_CLOEXEC, 0644);
  if (zero_fd < 0)
    goto fail2;
# endif
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          map_flags | MAP_PRIVATE, zero_fd, 0);
# if !HAVE_MAP_ANONYMOUS
  close (zero_fd);
# endif
  if (auxmap == (void *) -1)
    goto fail2;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  maps = (prmap_t *) auxmap;

  if (ioctl (fd, PIOCMAP, maps) < 0)
    goto fail1;

  for (mp = maps;;)
    {
      unsigned long start, end;
      unsigned int flags;

      start = (unsigned long) mp->pr_vaddr;
      end = start + mp->pr_size;
      if (start == 0 && end == 0)
        break;
      flags = 0;
      if (mp->pr_mflags & MA_READ)
        flags |= VMA_PROT_READ;
      if (mp->pr_mflags & MA_WRITE)
        flags |= VMA_PROT_WRITE;
      if (mp->pr_mflags & MA_EXEC)
        flags |= VMA_PROT_EXECUTE;
      mp++;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
           
          if (start < auxmap_start)
            if (callback (data, start, auxmap_start, flags))
              break;
          if (auxmap_end - 1 < end - 1)
            if (callback (data, auxmap_end, end, flags))
              break;
        }
      else
        {
          if (callback (data, start, end, flags))
            break;
        }
    }
  munmap (auxmap, memneed);
  close (fd);
  return 0;

 fail1:
  munmap (auxmap, memneed);
 fail2:
  close (fd);
  return -1;

#elif defined __sun  

   

# if defined PIOCNMAP && defined PIOCMAP
   

  size_t pagesize;
  char fnamebuf[6+10+1];
  char *fname;
  int fd;
  int nmaps;
  size_t memneed;
#  if HAVE_MAP_ANONYMOUS
#   define zero_fd -1
#   define map_flags MAP_ANONYMOUS
#  else  
  int zero_fd;
#   define map_flags 0
#  endif
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  prmap_t* maps;
  prmap_t* mp;

  pagesize = getpagesize ();

   
  fname = fnamebuf + sizeof (fnamebuf) - 1;
  *fname = '\0';
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;

  if (ioctl (fd, PIOCNMAP, &nmaps) < 0)
    goto fail2;

  memneed = (nmaps + 10) * sizeof (prmap_t);
   
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
#  if !HAVE_MAP_ANONYMOUS
  zero_fd = open ("/dev/zero", O_RDONLY | O_CLOEXEC, 0644);
  if (zero_fd < 0)
    goto fail2;
#  endif
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          map_flags | MAP_PRIVATE, zero_fd, 0);
#  if !HAVE_MAP_ANONYMOUS
  close (zero_fd);
#  endif
  if (auxmap == (void *) -1)
    goto fail2;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  maps = (prmap_t *) auxmap;

  if (ioctl (fd, PIOCMAP, maps) < 0)
    goto fail1;

  for (mp = maps;;)
    {
      unsigned long start, end;
      unsigned int flags;

      start = (unsigned long) mp->pr_vaddr;
      end = start + mp->pr_size;
      if (start == 0 && end == 0)
        break;
      flags = 0;
      if (mp->pr_mflags & MA_READ)
        flags |= VMA_PROT_READ;
      if (mp->pr_mflags & MA_WRITE)
        flags |= VMA_PROT_WRITE;
      if (mp->pr_mflags & MA_EXEC)
        flags |= VMA_PROT_EXECUTE;
      mp++;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
           
          if (start < auxmap_start)
            if (callback (data, start, auxmap_start, flags))
              break;
          if (auxmap_end - 1 < end - 1)
            if (callback (data, auxmap_end, end, flags))
              break;
        }
      else
        {
          if (callback (data, start, end, flags))
            break;
        }
    }
  munmap (auxmap, memneed);
  close (fd);
  return 0;

 fail1:
  munmap (auxmap, memneed);
 fail2:
  close (fd);
  return -1;

# else
   

  size_t pagesize;
  char fnamebuf[6+10+4+1];
  char *fname;
  int fd;
  int nmaps;
  size_t memneed;
#  if HAVE_MAP_ANONYMOUS
#   define zero_fd -1
#   define map_flags MAP_ANONYMOUS
#  else  
  int zero_fd;
#   define map_flags 0
#  endif
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  prmap_t* maps;
  prmap_t* maps_end;
  prmap_t* mp;

  pagesize = getpagesize ();

   
  fname = fnamebuf + sizeof (fnamebuf) - 1 - 4;
  memcpy (fname, "/map", 4 + 1);
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;

  {
    struct stat statbuf;
    if (fstat (fd, &statbuf) < 0)
      goto fail2;
    nmaps = statbuf.st_size / sizeof (prmap_t);
  }

  memneed = (nmaps + 10) * sizeof (prmap_t);
   
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
#  if !HAVE_MAP_ANONYMOUS
  zero_fd = open ("/dev/zero", O_RDONLY | O_CLOEXEC, 0644);
  if (zero_fd < 0)
    goto fail2;
#  endif
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          map_flags | MAP_PRIVATE, zero_fd, 0);
#  if !HAVE_MAP_ANONYMOUS
  close (zero_fd);
#  endif
  if (auxmap == (void *) -1)
    goto fail2;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  maps = (prmap_t *) auxmap;

   
  {
    size_t remaining = memneed;
    size_t total_read = 0;
    char *ptr = (char *) maps;

    do
      {
        size_t nread = read (fd, ptr, remaining);
        if (nread == (size_t)-1)
          {
            if (errno == EINTR)
              continue;
            goto fail1;
          }
        if (nread == 0)
           
          break;
        total_read += nread;
        ptr += nread;
        remaining -= nread;
      }
    while (remaining > 0);

    nmaps = (memneed - remaining) / sizeof (prmap_t);
    maps_end = maps + nmaps;
  }

  for (mp = maps; mp < maps_end; mp++)
    {
      unsigned long start, end;
      unsigned int flags;

      start = (unsigned long) mp->pr_vaddr;
      end = start + mp->pr_size;
      flags = 0;
      if (mp->pr_mflags & MA_READ)
        flags |= VMA_PROT_READ;
      if (mp->pr_mflags & MA_WRITE)
        flags |= VMA_PROT_WRITE;
      if (mp->pr_mflags & MA_EXEC)
        flags |= VMA_PROT_EXECUTE;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
           
          if (start < auxmap_start)
            if (callback (data, start, auxmap_start, flags))
              break;
          if (auxmap_end - 1 < end - 1)
            if (callback (data, auxmap_end, end, flags))
              break;
        }
      else
        {
          if (callback (data, start, end, flags))
            break;
        }
    }
  munmap (auxmap, memneed);
  close (fd);
  return 0;

 fail1:
  munmap (auxmap, memneed);
 fail2:
  close (fd);
  return -1;

# endif

#elif HAVE_PSTAT_GETPROCVM  

  unsigned long pagesize = getpagesize ();
  int i;

  for (i = 0; ; i++)
    {
      struct pst_vm_status info;
      int ret = pstat_getprocvm (&info, sizeof (info), 0, i);
      if (ret < 0)
        return -1;
      if (ret == 0)
        break;
      {
        unsigned long start = info.pst_vaddr;
        unsigned long end = start + info.pst_length * pagesize;
        unsigned int flags = 0;
        if (info.pst_permission & PS_PROT_READ)
          flags |= VMA_PROT_READ;
        if (info.pst_permission & PS_PROT_WRITE)
          flags |= VMA_PROT_WRITE;
        if (info.pst_permission & PS_PROT_EXECUTE)
          flags |= VMA_PROT_EXECUTE;

        if (callback (data, start, end, flags))
          break;
      }
    }

#elif defined __APPLE__ && defined __MACH__  

  task_t task = mach_task_self ();
  vm_address_t address;
  vm_size_t size;

  for (address = VM_MIN_ADDRESS;; address += size)
    {
      int more;
      mach_port_t object_name;
      unsigned int flags;
       
# if defined __aarch64__ || defined __ppc64__ || defined __x86_64__
      struct vm_region_basic_info_64 info;
      mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

      more = (vm_region_64 (task, &address, &size, VM_REGION_BASIC_INFO_64,
                            (vm_region_info_t)&info, &info_count, &object_name)
              == KERN_SUCCESS);
# else
      struct vm_region_basic_info info;
      mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT;

      more = (vm_region (task, &address, &size, VM_REGION_BASIC_INFO,
                         (vm_region_info_t)&info, &info_count, &object_name)
              == KERN_SUCCESS);
# endif
      if (object_name != MACH_PORT_NULL)
        mach_port_deallocate (mach_task_self (), object_name);
      if (!more)
        break;
      flags = 0;
      if (info.protection & VM_PROT_READ)
        flags |= VMA_PROT_READ;
      if (info.protection & VM_PROT_WRITE)
        flags |= VMA_PROT_WRITE;
      if (info.protection & VM_PROT_EXECUTE)
        flags |= VMA_PROT_EXECUTE;
      if (callback (data, address, address + size, flags))
        break;
    }
  return 0;

#elif defined __GNU__  

   

  MEMORY_BASIC_INFORMATION info;
  uintptr_t address = 0;

  while (VirtualQuery ((void*)address, &info, sizeof(info)) == sizeof(info))
    {
      if (info.State != MEM_FREE)
         
        if (info.State != MEM_RESERVE)
          {
            uintptr_t start, end;
            unsigned int flags;

            start = (uintptr_t)info.BaseAddress;
            end = start + info.RegionSize;
            switch (info.Protect & ~(PAGE_GUARD|PAGE_NOCACHE))
              {
              case PAGE_READONLY:
                flags = VMA_PROT_READ;
                break;
              case PAGE_READWRITE:
              case PAGE_WRITECOPY:
                flags = VMA_PROT_READ | VMA_PROT_WRITE;
                break;
              case PAGE_EXECUTE:
                flags = VMA_PROT_EXECUTE;
                break;
              case PAGE_EXECUTE_READ:
                flags = VMA_PROT_READ | VMA_PROT_EXECUTE;
                break;
              case PAGE_EXECUTE_READWRITE:
              case PAGE_EXECUTE_WRITECOPY:
                flags = VMA_PROT_READ | VMA_PROT_WRITE | VMA_PROT_EXECUTE;
                break;
              case PAGE_NOACCESS:
              default:
                flags = 0;
                break;
              }

            if (callback (data, start, end, flags))
              break;
          }
      address = (uintptr_t)info.BaseAddress + info.RegionSize;
    }
  return 0;

#elif defined __BEOS__ || defined __HAIKU__
   

  area_info info;
  ssize_t cookie;

  cookie = 0;
  while (get_next_area_info (0, &cookie, &info) == B_OK)
    {
      unsigned long start, end;
      unsigned int flags;

      start = (unsigned long) info.address;
      end = start + info.size;
      flags = 0;
      if (info.protection & B_READ_AREA)
        flags |= VMA_PROT_READ | VMA_PROT_EXECUTE;
      if (info.protection & B_WRITE_AREA)
        flags |= VMA_PROT_WRITE;

      if (callback (data, start, end, flags))
        break;
    }
  return 0;

#elif HAVE_MQUERY  

# if defined __OpenBSD__
   
  {
    int retval = vma_iterate_bsd (callback, data);
    if (retval == 0)
      return 0;
  }
# endif

  {
    uintptr_t pagesize;
    uintptr_t address;
    int   address_known_mapped;

    pagesize = getpagesize ();
     
    address = pagesize;
    address_known_mapped = 0;
    for (;;)
      {
         
        if (address_known_mapped
            || mquery ((void *) address, pagesize, 0, MAP_FIXED, -1, 0)
               == (void *) -1)
          {
             
            uintptr_t start = address;
            uintptr_t end;

             
            end = (uintptr_t) mquery ((void *) address, pagesize, 0, 0, -1, 0);
            if (end == (uintptr_t) (void *) -1)
              end = 0;  
            address = end;

             
            if (callback (data, start, end, 0))
              break;

            if (address < pagesize)  
              break;
          }
         
        {
          uintptr_t query_size = pagesize;

          address += pagesize;

           
          for (;;)
            {
              if (2 * query_size > query_size)
                query_size = 2 * query_size;
              if (address + query_size - 1 < query_size)  
                {
                  address_known_mapped = 0;
                  break;
                }
              if (mquery ((void *) address, query_size, 0, MAP_FIXED, -1, 0)
                  == (void *) -1)
                {
                   
                  address_known_mapped = (query_size == pagesize);
                  break;
                }
               
              address += query_size;
            }
           
          while (query_size > pagesize)
            {
              query_size = query_size / 2;
              if (address + query_size - 1 >= query_size)
                {
                  if (mquery ((void *) address, query_size, 0, MAP_FIXED, -1, 0)
                      != (void *) -1)
                    {
                       
                      address += query_size;
                      address_known_mapped = 0;
                    }
                  else
                    address_known_mapped = (query_size == pagesize);
                }
            }
           
        }
        if (address + pagesize - 1 < pagesize)  
          break;
      }
    return 0;
  }

#else

   
  return -1;

#endif
}


#ifdef TEST

#include <stdio.h>

 

static int
vma_iterate_callback (void *data, uintptr_t start, uintptr_t end,
                      unsigned int flags)
{
  printf ("%08lx-%08lx %c%c%c\n",
          (unsigned long) start, (unsigned long) end,
          flags & VMA_PROT_READ ? 'r' : '-',
          flags & VMA_PROT_WRITE ? 'w' : '-',
          flags & VMA_PROT_EXECUTE ? 'x' : '-');
  return 0;
}

int
main ()
{
  vma_iterate (vma_iterate_callback, NULL);

   
  sleep (10);

  return 0;
}

 

#endif  
