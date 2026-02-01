 

#include <config.h>

int *my_null = nullptr;

#if 0  
 
# include <stddef.h>
#endif

#include <stdarg.h>

#include "macros.h"

#if 0  

static void
simple_callee (nullptr_t x)
{
  ASSERT (x == NULL);
}

#endif

static void
varargs_callee (const char *first, ...)
{
  va_list args;
  const char *arg;

  ASSERT (first[0] == 't');
  va_start (args, first);

  arg = va_arg (args, const char *);
  ASSERT (arg == NULL);

  arg = va_arg (args, const char *);
  ASSERT (arg[0] == 'f');

  arg = va_arg (args, const char *);
  ASSERT (arg == NULL);

  va_end (args);
}

int
main ()
{
  varargs_callee ("type", nullptr, "foo", nullptr);

  return 0;
}
