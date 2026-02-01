 

#include <config.h>

#include "verror.h"

#include <errno.h>
#include <stdarg.h>

#include "error.h"
#include "macros.h"

 
static void
print_no_progname (void)
{
}

static void
test_zero (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror (0, 0, format, args);
  va_end (args);
}

static void
test_zero_at_line (const char *filename, unsigned int lineno,
                   const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror_at_line (0, 0, filename, lineno, format, args);
  va_end (args);
}

static void
test_errnum (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror (0, EACCES, format, args);
  va_end (args);
}

static void
test_fatal (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror (4, 0, format, args);
  va_end (args);
}

int
main (int argc, char *argv[])
{
   
  test_zero ("bummer");
   
  errno = EINVAL;  
  test_zero ("Zonk %d%d%d is too large", 1, 2, 3);
   
  test_zero ("Pokémon started");
   
  ASSERT (error_message_count == 3);

   
  test_zero_at_line ("d1/foo.c", 10, "invalid blub");
  test_zero_at_line ("d1/foo.c", 10, "invalid blarn");
   
  ASSERT (error_message_count == 5);

   
  error_one_per_line = 1;
  test_zero_at_line ("d1/foo.c", 10, "unsupported glink");
   
  test_zero_at_line ("d1/foo.c", 13, "invalid brump");
   
  test_zero_at_line ("d2/foo.c", 13, "unsupported flinge");
   
  test_zero_at_line ("d2/foo.c", 13, "invalid bark");
   
  ASSERT (error_message_count == 8);
  error_one_per_line = 0;

   
  error_print_progname = print_no_progname;
  test_zero ("hammer");
  test_zero ("boing %d%d%d is too large", 1, 2, 3);
  #if 0
   
  test_zero_at_line (NULL, 42, "drummer too loud");
  #endif
  test_zero_at_line ("d2/bar.c", 11, "bark too loud");
   
  ASSERT (error_message_count == 11);
  error_print_progname = NULL;

   
  errno = EINVAL;  
  test_errnum ("can't steal");
   
  ASSERT (error_message_count == 12);

   
  test_fatal ("fatal error");

  return 0;
}
