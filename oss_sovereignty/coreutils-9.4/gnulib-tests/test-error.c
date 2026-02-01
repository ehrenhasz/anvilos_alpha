 

#include <config.h>

#include "error.h"

#include <errno.h>

#include "macros.h"

 
static void
print_no_progname (void)
{
}

int
main (int argc, char *argv[])
{
   
  error (0, 0, "bummer");
   
  errno = EINVAL;  
  error (0, 0, "Zonk %d%d%d is too large", 1, 2, 3);
   
  error (0, 0, "Pokémon started");
   
  ASSERT (error_message_count == 3);

   
  error_at_line (0, 0, "d1/foo.c", 10, "invalid blub");
  error_at_line (0, 0, "d1/foo.c", 10, "invalid blarn");
   
  ASSERT (error_message_count == 5);

   
  error_one_per_line = 1;
  error_at_line (0, 0, "d1/foo.c", 10, "unsupported glink");
   
  error_at_line (0, 0, "d1/foo.c", 13, "invalid brump");
   
  error_at_line (0, 0, "d2/foo.c", 13, "unsupported flinge");
   
  error_at_line (0, 0, "d2/foo.c", 13, "invalid bark");
   
  ASSERT (error_message_count == 8);
  error_one_per_line = 0;

   
  error_print_progname = print_no_progname;
  error (0, 0, "hammer");
  error (0, 0, "boing %d%d%d is too large", 1, 2, 3);
  #if 0
   
  error_at_line (0, 0, NULL, 42, "drummer too loud");
  #endif
  error_at_line (0, 0, "d2/bar.c", 11, "bark too loud");
   
  ASSERT (error_message_count == 11);
  error_print_progname = NULL;

   
  errno = EINVAL;  
  error (0, EACCES, "can't steal");
   
  ASSERT (error_message_count == 12);

   
  error (4, 0, "fatal error");

  return 0;
}
