 

#include <config.h>

#include "glthread/lock.h"

#include "macros.h"

gl_once_define(static, a_once)

static int a;

static void
a_init (void)
{
  a = 42;
}

int
main ()
{
  gl_once (a_once, a_init);

  ASSERT (a == 42);

  return 0;
}
