 
#define TARGET_LEN (5 * 1024)

#if defined HAVE_OPENAT || (defined GNULIB_OPENAT && defined HAVE_FDOPENDIR)
# define HAVE_OPENAT_SUPPORT 1
#else
# define HAVE_OPENAT_SUPPORT 0
#endif

 
static int
test_abort_bug (void)
{
  char *cwd;
  size_t initial_cwd_len;
  int fail = 0;

   
#ifdef PATH_MAX
  int bug_possible = PATH_MAX < getpagesize ();
#else
  int bug_possible = 0;
#endif
  if (! bug_possible)
    return 0;

  cwd = getcwd (NULL, 0);
  if (cwd == NULL)
    return 2;

  initial_cwd_len = strlen (cwd);
  free (cwd);

  if (HAVE_OPENAT_SUPPORT)
    {
      static char const dir_name[] = "confdir-14B---";
      size_t desired_depth = ((TARGET_LEN - 1 - initial_cwd_len)
                              / sizeof dir_name);
      size_t d;
      for (d = 0; d < desired_depth; d++)
        {
          if (mkdir (dir_name, S_IRWXU) < 0 || chdir (dir_name) < 0)
            {
              if (! (errno == ERANGE || errno == ENAMETOOLONG
                     || errno == ENOENT))
                fail = 3;  
              break;
            }
        }

       
      cwd = getcwd (NULL, 0);
      if (cwd == NULL)
        fail = 4;  
      free (cwd);

       
      rmdir (dir_name);
      while (0 < d--)
        {
          if (chdir ("..") < 0)
            {
              fail = 5;
              break;
            }
          rmdir (dir_name);
        }
    }

  return fail;
}

 
#define DIR_NAME "confdir3"
#define DIR_NAME_LEN 8
#define DIR_NAME_SIZE (DIR_NAME_LEN + 1)

 
#define DOTDOTSLASH_LEN 3

 
#define BUF_SLOP 20

 
static int
test_long_name (void)
{
#ifndef PATH_MAX
   
  return 0;
#elif ((INT_MAX / (DIR_NAME_SIZE / DOTDOTSLASH_LEN + 1) \
        - DIR_NAME_SIZE - BUF_SLOP) \
       <= PATH_MAX)
   
  return 0;
#else
   
  if (is_running_under_qemu_user ())
    return 77;

  char buf[PATH_MAX * (DIR_NAME_SIZE / DOTDOTSLASH_LEN + 1)
           + DIR_NAME_SIZE + BUF_SLOP];
  char *cwd = getcwd (buf, PATH_MAX);
  size_t initial_cwd_len;
  size_t cwd_len;
  int fail = 0;
  size_t n_chdirs = 0;

  if (cwd == NULL)
    return 1;

  cwd_len = initial_cwd_len = strlen (cwd);

  while (1)
    {
# ifdef HAVE_GETCWD_SHORTER
       
      size_t dotdot_max = PATH_MAX * 2;
# else
      size_t dotdot_max = PATH_MAX * (DIR_NAME_SIZE / DOTDOTSLASH_LEN);
# endif
      char *c = NULL;

      cwd_len += DIR_NAME_SIZE;
       
      if (mkdir (DIR_NAME, S_IRWXU) < 0 || chdir (DIR_NAME) < 0)
        {
          if (! (errno == ERANGE || errno == ENAMETOOLONG || errno == ENOENT))
            #ifdef __linux__
            if (! (errno == EINVAL))
            #endif
              fail = 2;
          break;
        }

      if (PATH_MAX <= cwd_len && cwd_len < PATH_MAX + DIR_NAME_SIZE)
        {
          c = getcwd (buf, PATH_MAX);
          if (!c && errno == ENOENT)
            {
              fail = 3;
              break;
            }
          if (c)
            {
              fail = 4;
              break;
            }
          if (! (errno == ERANGE || errno == ENAMETOOLONG))
            {
              fail = 5;
              break;
            }
        }

      if (dotdot_max <= cwd_len - initial_cwd_len)
        {
          if (dotdot_max + DIR_NAME_SIZE < cwd_len - initial_cwd_len)
            break;
          c = getcwd (buf, cwd_len + 1);
          if (!c)
            {
              if (! (errno == ERANGE || errno == ENOENT
                     || errno == ENAMETOOLONG))
                {
                  fail = 6;
                  break;
                }
              if (HAVE_OPENAT_SUPPORT || errno == ERANGE || errno == ENOENT)
                {
                  fail = 7;
                  break;
                }
            }
        }

      if (c && strlen (c) != cwd_len)
        {
          fail = 8;
          break;
        }
      ++n_chdirs;
    }

   
  {
    size_t i;

     
    rmdir (DIR_NAME);
    for (i = 0; i <= n_chdirs; i++)
      {
        if (chdir ("..") < 0)
          break;
        if (rmdir (DIR_NAME) != 0)
          break;
      }
  }

  return fail;
#endif
}

int
main (int argc, char **argv)
{
  int err1 = test_abort_bug ();
  int err2 = test_long_name ();
  return err1 * 10 + (err1 != 0 && err2 == 77 ? 0 : err2);
}
