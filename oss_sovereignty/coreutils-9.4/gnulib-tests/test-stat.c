 

#include <config.h>

#include <sys/stat.h>

 
#include "signature.h"
SIGNATURE_CHECK (stat, int, (char const *, struct stat *));

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "same-inode.h"
#include "macros.h"

#define BASE "test-stat.t"

#include "test-stat.h"

 
static int
do_stat (char const *name, struct stat *st)
{
  return stat (name, st);
}

int
main (void)
{
  return test_stat_func (do_stat, true);
}
