 

#include <config.h>

#include "utimecmp.h"

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "dirname.h"
#include "hash.h"
#include "intprops.h"
#include "stat-time.h"

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define BILLION (1000 * 1000 * 1000)

 
#if HAVE_UTIMENSAT
enum { SYSCALL_RESOLUTION = 1 };
#elif defined _WIN32 && ! defined __CYGWIN__
 
struct fs_res
{
   
  dev_t dev;

   
  int resolution;

   
  bool exact;
};

 
static size_t
dev_info_hash (void const *x, size_t table_size)
{
  struct fs_res const *p = x;

   
  if (TYPE_SIGNED (dev_t) && SIZE_MAX < MAX (INT_MAX, TYPE_MAXIMUM (dev_t)))
    {
      uintmax_t dev = p->dev;
      return dev % table_size;
    }

  return p->dev % table_size;
}

 
static bool
dev_info_compare (void const *x, void const *y)
{
  struct fs_res const *a = x;
  struct fs_res const *b = y;
  return a->dev == b->dev;
}

 

int
utimecmp (char const *dst_name,
          struct stat const *dst_stat,
          struct stat const *src_stat,
          int options)
{
  return utimecmpat (AT_FDCWD, dst_name, dst_stat, src_stat, options);
}

int
utimecmpat (int dfd, char const *dst_name,
            struct stat const *dst_stat,
            struct stat const *src_stat,
            int options)
{
   

  static_assert (TYPE_IS_INTEGER (time_t));

   
  time_t dst_s = dst_stat->st_mtime;
  time_t src_s = src_stat->st_mtime;
  int dst_ns = get_stat_mtime_ns (dst_stat);
  int src_ns = get_stat_mtime_ns (src_stat);

  if (options & UTIMECMP_TRUNCATE_SOURCE)
    {
#if defined _AIX
       
      long long difference =
        ((long long) dst_s - (long long) src_s) * BILLION
        + ((long long) dst_ns - (long long) src_ns);
      if (difference < 10000000 && difference > -10000000)
        return 0;
#endif

       

       
      static Hash_table *ht;

       
      static struct fs_res *new_dst_res;
      struct fs_res *dst_res = NULL;
      struct fs_res tmp_dst_res;

       
      int res;

       
      if (dst_s == src_s && dst_ns == src_ns)
        return 0;
      if (dst_s <= src_s - 2)
        return -1;
      if (src_s <= dst_s - 2)
        return 1;

       
      if (! ht)
        ht = hash_initialize (16, NULL, dev_info_hash, dev_info_compare, free);
      if (ht)
        {
          if (! new_dst_res)
            {
              new_dst_res = malloc (sizeof *new_dst_res);
              if (!new_dst_res)
                goto low_memory;
              new_dst_res->resolution = 2 * BILLION;
              new_dst_res->exact = false;
            }
          new_dst_res->dev = dst_stat->st_dev;
          dst_res = hash_insert (ht, new_dst_res);
          if (! dst_res)
            goto low_memory;

          if (dst_res == new_dst_res)
            {
               
              new_dst_res = NULL;
            }
        }
      else
        {
        low_memory:
          if (ht)
            {
              tmp_dst_res.dev = dst_stat->st_dev;
              dst_res = hash_lookup (ht, &tmp_dst_res);
            }
          if (!dst_res)
            {
              dst_res = &tmp_dst_res;
              dst_res->resolution = 2 * BILLION;
              dst_res->exact = false;
            }
        }

      res = dst_res->resolution;

#ifdef _PC_TIMESTAMP_RESOLUTION
       
      if (! dst_res->exact)
        {
          res = -1;
          if (dfd == AT_FDCWD)
            res = pathconf (dst_name, _PC_TIMESTAMP_RESOLUTION);
          else
            {
              char *dstdir = mdir_name (dst_name);
              if (dstdir)
                {
                  int destdirfd = openat (dfd, dstdir,
                                          O_SEARCH | O_CLOEXEC | O_DIRECTORY);
                  if (0 <= destdirfd)
                    {
                      res = fpathconf (destdirfd, _PC_TIMESTAMP_RESOLUTION);
                      close (destdirfd);
                    }
                  free (dstdir);
                }
            }
          if (0 < res)
            {
              dst_res->resolution = res;
              dst_res->exact = true;
            }
        }
#endif

      if (! dst_res->exact)
        {
           

          time_t dst_a_s = dst_stat->st_atime;
          time_t dst_c_s = dst_stat->st_ctime;
          time_t dst_m_s = dst_s;
          int dst_a_ns = get_stat_atime_ns (dst_stat);
          int dst_c_ns = get_stat_ctime_ns (dst_stat);
          int dst_m_ns = dst_ns;

           
          {
            bool odd_second = (dst_a_s | dst_c_s | dst_m_s) & 1;

            if (SYSCALL_RESOLUTION == BILLION)
              {
                if (odd_second | dst_a_ns | dst_c_ns | dst_m_ns)
                  res = BILLION;
              }
            else
              {
                int a = dst_a_ns;
                int c = dst_c_ns;
                int m = dst_m_ns;

                 
                int SR10 = SYSCALL_RESOLUTION;  SR10 *= 10;

                if ((a % SR10 | c % SR10 | m % SR10) != 0)
                  res = SYSCALL_RESOLUTION;
                else
                  for (res = SR10, a /= SR10, c /= SR10, m /= SR10;
                       (res < dst_res->resolution
                        && (a % 10 | c % 10 | m % 10) == 0);
                       res *= 10, a /= 10, c /= 10, m /= 10)
                    if (res == BILLION)
                      {
                        if (! odd_second)
                          res *= 2;
                        break;
                      }
              }

            dst_res->resolution = res;
          }

          if (SYSCALL_RESOLUTION < res)
            {
              struct stat dst_status;

               
              src_ns -= src_ns % SYSCALL_RESOLUTION;

               
              {
                time_t s = src_s & ~ (res == 2 * BILLION ? 1 : 0);
                if (src_s < dst_s || (src_s == dst_s && src_ns <= dst_ns))
                  return 1;
                if (dst_s < s
                    || (dst_s == s && dst_ns < src_ns - src_ns % res))
                  return -1;
              }

               

              struct timespec timespec[2] = {
                [0].tv_sec = dst_a_s,
                [0].tv_nsec = dst_a_ns,
                [1].tv_sec = dst_m_s | (res == 2 * BILLION),
                [1].tv_nsec = dst_m_ns + res / 9
              };

              if (utimensat (dfd, dst_name, timespec, AT_SYMLINK_NOFOLLOW))
                return -2;

               
              {
                int stat_result
                  = fstatat (dfd, dst_name, &dst_status, AT_SYMLINK_NOFOLLOW);

                if (stat_result
                    | (dst_status.st_mtime ^ dst_m_s)
                    | (get_stat_mtime_ns (&dst_status) ^ dst_m_ns))
                  {
                     
                    timespec[1].tv_sec = dst_m_s;
                    timespec[1].tv_nsec = dst_m_ns;
                    utimensat (dfd, dst_name, timespec, AT_SYMLINK_NOFOLLOW);
                  }

                if (stat_result != 0)
                  return -2;
              }

               
              {
                int old_res = res;
                int a = (BILLION * (dst_status.st_mtime & 1)
                         + get_stat_mtime_ns (&dst_status));

                res = SYSCALL_RESOLUTION;

                for (a /= res; a % 10 == 0; a /= 10)
                  {
                    if (res == BILLION)
                      {
                        res *= 2;
                        break;
                      }
                    res *= 10;
                    if (res == old_res)
                      break;
                  }
              }
            }

          dst_res->resolution = res;
          dst_res->exact = true;
        }

       
      src_s &= ~ (res == 2 * BILLION ? 1 : 0);
      src_ns -= src_ns % res;
    }

   
  return (_GL_CMP (dst_s, src_s)
          + ((dst_s == src_s ? ~0 : 0) & _GL_CMP (dst_ns, src_ns)));
}
