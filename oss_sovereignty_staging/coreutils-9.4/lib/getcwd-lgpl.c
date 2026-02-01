 
#include <unistd.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if GNULIB_GETCWD
 
typedef int dummy;
#else

 

# undef getcwd
# if defined _WIN32 && !defined __CYGWIN__
#  define getcwd _getcwd
# endif

char *
rpl_getcwd (char *buf, size_t size)
{
  char *ptr;
  char *result;

   
  if (buf)
    {
      if (!size)
        {
          errno = EINVAL;
          return NULL;
        }
      return getcwd (buf, size);
    }

  if (size)
    {
      buf = malloc (size);
      if (!buf)
        {
          errno = ENOMEM;
          return NULL;
        }
      result = getcwd (buf, size);
      if (!result)
        free (buf);
      return result;
    }

   
  {
    char tmp[4032];
    size = sizeof tmp;
    ptr = getcwd (tmp, size);
    if (ptr)
      {
        result = strdup (ptr);
        if (!result)
          errno = ENOMEM;
        return result;
      }
    if (errno != ERANGE)
      return NULL;
  }

   
  do
    {
      size <<= 1;
      ptr = realloc (buf, size);
      if (ptr == NULL)
        {
          free (buf);
          errno = ENOMEM;
          return NULL;
        }
      buf = ptr;
      result = getcwd (buf, size);
    }
  while (!result && errno == ERANGE);

  if (!result)
    free (buf);
  else
    {
       
       
      size_t actual_size = strlen (result) + 1;
      if (actual_size < size)
        {
          char *shrinked_result = realloc (result, actual_size);
          if (shrinked_result != NULL)
            result = shrinked_result;
        }
    }
  return result;
}

#endif
