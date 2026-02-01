 

#include <config.h>

 
#define _GL_NO_LARGE_FILES

 
#include <getopt.h>

#ifndef __getopt_argv_const
# define __getopt_argv_const const
#endif
#include "signature.h"
SIGNATURE_CHECK (getopt_long, int, (int, char *__getopt_argv_const *,
                                    char const *, struct option const *,
                                    int *));
SIGNATURE_CHECK (getopt_long_only, int, (int, char *__getopt_argv_const *,
                                         char const *, struct option const *,
                                         int *));

#define TEST_GETOPT_GNU 1
#define TEST_GETOPT_TMP_NAME "test-getopt-gnu.tmp"
#include "test-getopt-main.h"
