 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (mknod, int, (char const *, mode_t, dev_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-mknod.t"

#include "test-mkfifo.h"

 
static int
do_mknod (char const *name, mode_t mode)
{
  return mknod (name, mode | S_IFIFO, 0);
}

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

   
  return test_mkfifo (do_mknod, true);
}
