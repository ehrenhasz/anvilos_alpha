 
#include "system.h"
#include "tmpdir.h"

#include "temp-stream.h"


#if defined __MSDOS__ || defined _WIN32
 
# define DONT_UNLINK_WHILE_OPEN 1
#endif

#if DONT_UNLINK_WHILE_OPEN

 

static char const *file_to_remove;
static FILE *fp_to_close;

static void
unlink_tempfile (void)
{
  fclose (fp_to_close);
  unlink (file_to_remove);
}

static void
record_or_unlink_tempfile (char const *fn, FILE *fp)
{
  if (!file_to_remove)
    {
      file_to_remove = fn;
      fp_to_close = fp;
      atexit (unlink_tempfile);
    }
}

#else

static void
record_or_unlink_tempfile (char const *fn, MAYBE_UNUSED FILE *fp)
{
  unlink (fn);
}

#endif

 
bool
temp_stream (FILE **fp, char **file_name)
{
  static char *tempfile = nullptr;
  static FILE *tmp_fp;
  if (tempfile == nullptr)
    {
      char *tempbuf = nullptr;
      size_t tempbuf_len = 128;

      while (true)
        {
          if (! (tempbuf = realloc (tempbuf, tempbuf_len)))
            {
              error (0, errno, _("failed to make temporary file name"));
              return false;
            }

          if (path_search (tempbuf, tempbuf_len, nullptr, "cutmp", true) == 0)
            break;

          if (errno != EINVAL || PATH_MAX / 2 < tempbuf_len)
            {
              error (0, errno == EINVAL ? ENAMETOOLONG : errno,
                     _("failed to make temporary file name"));
              return false;
            }

          tempbuf_len *= 2;
        }

      tempfile = tempbuf;

       

      int fd = mkstemp (tempfile);
      if (fd < 0)
        {
          error (0, errno, _("failed to create temporary file %s"),
                 quoteaf (tempfile));
          goto Reset;
        }

      tmp_fp = fdopen (fd, (O_BINARY ? "w+b" : "w+"));
      if (! tmp_fp)
        {
          error (0, errno, _("failed to open %s for writing"),
                 quoteaf (tempfile));
          close (fd);
          unlink (tempfile);
        Reset:
          free (tempfile);
          tempfile = nullptr;
          return false;
        }

      record_or_unlink_tempfile (tempfile, tmp_fp);
    }
  else
    {
      clearerr (tmp_fp);
      if (fseeko (tmp_fp, 0, SEEK_SET) < 0
          || ftruncate (fileno (tmp_fp), 0) < 0)
        {
          error (0, errno, _("failed to rewind stream for %s"),
                 quoteaf (tempfile));
          return false;
        }
    }

  *fp = tmp_fp;
  if (file_name)
    *file_name = tempfile;
  return true;
}
