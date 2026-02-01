 

 
#define _GL_ARG_NONNULL(params)

#include <config.h>

#include "canonicalize.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "same-inode.h"
#include "ignore-value.h"

#if GNULIB_defined_canonicalize_file_name
# include "null-ptr.h"
#endif

#include "macros.h"

#define BASE "t-can.tmp"

int
main (void)
{
   
  {
    int fd;
    ignore_value (system ("rm -rf " BASE " ise"));
    ASSERT (mkdir (BASE, 0700) == 0);
    fd = creat (BASE "/tra", 0600);
    ASSERT (0 <= fd);
    ASSERT (close (fd) == 0);
  }

   
  {
    char *result0 = canonicalize_file_name ("/etc/passwd");
    if (result0 != NULL)  
      {
        char *result;

        result = canonicalize_filename_mode ("/etc/passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

        result = canonicalize_filename_mode ("/etc//passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

        result = canonicalize_filename_mode ("/etc///passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

         
#if !(defined _WIN32 || defined __CYGWIN__)
        result = canonicalize_filename_mode ("//etc/passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

        result = canonicalize_filename_mode ("//etc//passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

        result = canonicalize_filename_mode ("//etc///passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);
#endif

        result = canonicalize_filename_mode ("///etc/passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

        result = canonicalize_filename_mode ("///etc//passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);

        result = canonicalize_filename_mode ("///etc///passwd", CAN_MISSING);
        ASSERT (result != NULL && strcmp (result, result0) == 0);
      }
  }

   
  {
    char *result1;
    char *result2;
    errno = 0;
    result1 = canonicalize_file_name (BASE "/tra/");
    ASSERT (result1 == NULL);
    ASSERT (errno == ENOTDIR);
    errno = 0;
    result2 = canonicalize_filename_mode (BASE "/tra/", CAN_EXISTING);
    ASSERT (result2 == NULL);
    ASSERT (errno == ENOTDIR);
  }

   
  {
    char *result1;
    char *result2;
    errno = 0;
    result1 = canonicalize_file_name (BASE "/zzz/..");
    ASSERT (result1 == NULL);
    ASSERT (errno == ENOENT);
    errno = 0;
    result2 = canonicalize_filename_mode (BASE "/zzz/..", CAN_EXISTING);
    ASSERT (result2 == NULL);
    ASSERT (errno == ENOENT);
  }

   
  if (symlink (BASE "/ket", "ise") != 0)
    {
      ASSERT (remove (BASE "/tra") == 0);
      ASSERT (rmdir (BASE) == 0);
      fputs ("skipping test: symlinks not supported on this file system\n",
             stderr);
      return 77;
    }
  ASSERT (symlink ("bef", BASE "/plo") == 0);
  ASSERT (symlink ("tra", BASE "/huk") == 0);
  ASSERT (symlink ("lum", BASE "/bef") == 0);
  ASSERT (symlink ("wum", BASE "/ouk") == 0);
  ASSERT (symlink ("../ise", BASE "/ket") == 0);
  ASSERT (mkdir (BASE "/lum", 0700) == 0);
  ASSERT (symlink ("s", BASE "/p") == 0);
  ASSERT (symlink ("d", BASE "/s") == 0);
  ASSERT (mkdir (BASE "/d", 0700) == 0);
  ASSERT (close (creat (BASE "/d/2", 0600)) == 0);
  ASSERT (symlink ("../s/2", BASE "/d/1") == 0);
  ASSERT (symlink ("//.//../..", BASE "/droot") == 0);

   
  {
    char *result1 = canonicalize_filename_mode (BASE "/huk", CAN_NOLINKS);
    ASSERT (result1 != NULL);
    ASSERT (strcmp (result1 + strlen (result1) - strlen ("/" BASE "/huk"),
                    "/" BASE "/huk") == 0);
    free (result1);
  }

   
  {
    char *result1 = canonicalize_file_name (BASE "/huk");
    char *result2 = canonicalize_file_name (BASE "/tra");
    char *result3 = canonicalize_filename_mode (BASE "/huk", CAN_EXISTING);
    ASSERT (result1 != NULL);
    ASSERT (result2 != NULL);
    ASSERT (result3 != NULL);
    ASSERT (strcmp (result1, result2) == 0);
    ASSERT (strcmp (result2, result3) == 0);
    ASSERT (strcmp (result1 + strlen (result1) - strlen ("/" BASE "/tra"),
                    "/" BASE "/tra") == 0);
    free (result1);
    free (result2);
    free (result3);
  }

   
  {
    char *result1 = canonicalize_file_name (BASE "/plo");
    char *result2 = canonicalize_file_name (BASE "/bef");
    char *result3 = canonicalize_file_name (BASE "/lum");
    char *result4 = canonicalize_filename_mode (BASE "/plo", CAN_EXISTING);
    ASSERT (result1 != NULL);
    ASSERT (result2 != NULL);
    ASSERT (result3 != NULL);
    ASSERT (result4 != NULL);
    ASSERT (strcmp (result1, result2) == 0);
    ASSERT (strcmp (result2, result3) == 0);
    ASSERT (strcmp (result3, result4) == 0);
    ASSERT (strcmp (result1 + strlen (result1) - strlen ("/" BASE "/lum"),
                    "/" BASE "/lum") == 0);
    free (result1);
    free (result2);
    free (result3);
    free (result4);
  }

   
  {
    char *result1;
    char *result2;
    errno = 0;
    result1 = canonicalize_file_name (BASE "/ouk");
    ASSERT (result1 == NULL);
    ASSERT (errno == ENOENT);
    errno = 0;
    result2 = canonicalize_filename_mode (BASE "/ouk", CAN_EXISTING);
    ASSERT (result2 == NULL);
    ASSERT (errno == ENOENT);
  }

   
  {
    char const *const file_name[]
      = {
         BASE "/huk/",
         BASE "/huk/.",
         BASE "/huk/./",
         BASE "/huk/./.",
         BASE "/huk/x",
         BASE "/huk/..",
         BASE "/huk/../",
         BASE "/huk/../.",
         BASE "/huk/../x",
         BASE "/huk/./..",
         BASE "/huk/././../x",
        };
    for (int i = 0; i < sizeof file_name / sizeof *file_name; i++)
      {
        errno = 0;
        ASSERT (!canonicalize_file_name (file_name[i]));
        ASSERT (errno == ENOTDIR);
        errno = 0;
        ASSERT (!canonicalize_filename_mode (file_name[i], CAN_EXISTING));
        ASSERT (errno == ENOTDIR);
      }
  }

   
  {
    char *result1;
    char *result2;
    errno = 0;
    result1 = canonicalize_file_name (BASE "/ouk/..");
    ASSERT (result1 == NULL);
    ASSERT (errno == ENOENT);
    errno = 0;
    result2 = canonicalize_filename_mode (BASE "/ouk/..", CAN_EXISTING);
    ASSERT (result2 == NULL);
    ASSERT (errno == ENOENT);
  }

   
  {
    char *result1;
    char *result2;
    errno = 0;
    result1 = canonicalize_file_name ("ise");
    ASSERT (result1 == NULL);
    ASSERT (errno == ELOOP);
    errno = 0;
    result2 = canonicalize_filename_mode ("ise", CAN_EXISTING);
    ASSERT (result2 == NULL);
    ASSERT (errno == ELOOP);
  }

   
  {
    char *result1 = canonicalize_filename_mode (BASE "/zzz", CAN_ALL_BUT_LAST);
    char *result2 = canonicalize_filename_mode (BASE "/zzz", CAN_MISSING);
    char *result3 = canonicalize_filename_mode (BASE "/zzz/", CAN_ALL_BUT_LAST);
    char *result4 = canonicalize_filename_mode (BASE "/zzz/", CAN_MISSING);
    ASSERT (result1 != NULL);
    ASSERT (result2 != NULL);
    ASSERT (result3 != NULL);
    ASSERT (result4 != NULL);
    ASSERT (strcmp (result1, result2) == 0);
    ASSERT (strcmp (result2, result3) == 0);
    ASSERT (strcmp (result3, result4) == 0);
    ASSERT (strcmp (result1 + strlen (result1) - strlen ("/" BASE "/zzz"),
                    "/" BASE "/zzz") == 0);
    free (result1);
    free (result2);
    free (result3);
    free (result4);
  }

   
  {
    char *result1 = canonicalize_filename_mode (BASE "/ouk", CAN_ALL_BUT_LAST);
    char *result2 = canonicalize_filename_mode (BASE "/ouk", CAN_MISSING);
    char *result3 = canonicalize_filename_mode (BASE "/ouk/", CAN_ALL_BUT_LAST);
    char *result4 = canonicalize_filename_mode (BASE "/ouk/", CAN_MISSING);
    ASSERT (result1 != NULL);
    ASSERT (result2 != NULL);
    ASSERT (result3 != NULL);
    ASSERT (result4 != NULL);
    ASSERT (strcmp (result1, result2) == 0);
    ASSERT (strcmp (result2, result3) == 0);
    ASSERT (strcmp (result3, result4) == 0);
    ASSERT (strcmp (result1 + strlen (result1) - strlen ("/" BASE "/wum"),
                    "/" BASE "/wum") == 0);
    free (result1);
    free (result2);
    free (result3);
    free (result4);
  }

   
  {
    char *result1 = canonicalize_filename_mode ("t-can.zzz/zzz", CAN_ALL_BUT_LAST);
    char *result2 = canonicalize_filename_mode ("t-can.zzz/zzz", CAN_MISSING);
    ASSERT (result1 == NULL);
    ASSERT (result2 != NULL);
    ASSERT (strcmp (result2 + strlen (result2) - 14, "/t-can.zzz/zzz") == 0);
    free (result2);
  }

   
  {
    char *result1 = canonicalize_filename_mode (BASE, CAN_EXISTING);
    char *result2 = canonicalize_filename_mode (BASE "/p/1", CAN_EXISTING);
    ASSERT (result1 != NULL);
    ASSERT (result2 != NULL);
    ASSERT (strcmp (result2 + strlen (result1), "/d/2") == 0);
    free (result1);
    free (result2);
  }

   
  ASSERT (remove (BASE "/droot") == 0);
  ASSERT (remove (BASE "/d/1") == 0);
  ASSERT (remove (BASE "/d/2") == 0);
  ASSERT (remove (BASE "/d") == 0);
  ASSERT (remove (BASE "/s") == 0);
  ASSERT (remove (BASE "/p") == 0);
  ASSERT (remove (BASE "/plo") == 0);
  ASSERT (remove (BASE "/huk") == 0);
  ASSERT (remove (BASE "/bef") == 0);
  ASSERT (remove (BASE "/ouk") == 0);
  ASSERT (remove (BASE "/ket") == 0);
  ASSERT (remove (BASE "/lum") == 0);
  ASSERT (remove (BASE "/tra") == 0);
  ASSERT (remove (BASE) == 0);
  ASSERT (remove ("ise") == 0);

  return 0;
}
