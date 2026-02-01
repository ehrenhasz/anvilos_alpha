 

#include <config.h>

#include <sys/stat.h>

 
#include "signature.h"
SIGNATURE_CHECK (lstat, int, (char const *, struct stat *));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "same-inode.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-lstat.t"

#include "test-lstat.h"

 
static int
do_lstat (char const *name, struct stat *st)
{
  return lstat (name, st);
}

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_lstat_func (do_lstat, true);
}
