 

#include <config.h>

#include "filenamecat.h"

#include "idx.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main (_GL_UNUSED int argc, char *argv[])
{
  static char const *const tests[][3] =
    {
      {"a", "b",   "a/b"},
      {"a/", "b",  "a/b"},
      {"a/", "/b", "a/b"},
      {"a", "/b",  "a/b"},

      {"/", "b",  "/b"},
      {"/", "/b", "/./b"},  
      {"/", "/",  "/./"},   
      {"a", "/",  "a/"},    
      {"/a", "/", "/a/"},   
      {"a", "//b",  "a//b"},
      {"", "a", "a"},   
    };
  unsigned int i;
  bool fail = false;

  for (i = 0; i < sizeof tests / sizeof tests[0]; i++)
    {
      char *base_in_result;
      char const *const *t = tests[i];
      char *res = file_name_concat (t[0], t[1], &base_in_result);
      idx_t prefixlen = base_in_result - res;
      size_t t0len = strlen (t[0]);
      size_t reslen = strlen (res);
      if (strcmp (res, t[2]) != 0)
        {
          fprintf (stderr, "test #%u: got %s, expected %s\n", i, res, t[2]);
          fail = true;
        }
      if (strcmp (t[1], base_in_result) != 0)
        {
          fprintf (stderr, "test #%u: base %s != base_in_result %s\n",
                   i, t[1], base_in_result);
          fail = true;
        }
      if (! (0 <= prefixlen && prefixlen <= reslen))
        {
          fprintf (stderr, "test #%u: base_in_result is not in result\n", i);
          fail = true;
        }
      if (reslen < t0len || memcmp (res, t[0], t0len) != 0)
        {
          fprintf (stderr, "test #%u: %s is not a prefix of %s\n",
                   i, t[0], res);
          fail = true;
        }
      free (res);
    }
  exit (fail ? EXIT_FAILURE : EXIT_SUCCESS);
}
