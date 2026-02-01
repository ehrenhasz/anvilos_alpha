 

#include <config.h>

 
#include "long-options.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "version-etc.h"
#include "exitfail.h"

static struct option const long_options[] =
{
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

 

void
parse_long_options (int argc,
                    char **argv,
                    const char *command_name,
                    const char *package,
                    const char *version,
                    void (*usage_func) (int),
                      ...)
{
  int c;
  int saved_opterr;

  saved_opterr = opterr;

   
  opterr = 0;

  if (argc == 2
      && (c = getopt_long (argc, argv, "+", long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'h':
          (*usage_func) (EXIT_SUCCESS);
          break;

        case 'v':
          {
            va_list authors;
            va_start (authors, usage_func);
            version_etc_va (stdout, command_name, package, version, authors);
            exit (EXIT_SUCCESS);
          }

        default:
           
          break;
        }
    }

   
  opterr = saved_opterr;

   
  optind = 0;
}

 
void
parse_gnu_standard_options_only (int argc,
                                 char **argv,
                                 const char *command_name,
                                 const char *package,
                                 const char *version,
                                 bool scan_all,
                                 void (*usage_func) (int),
                                   ...)
{
  int c;
  int saved_opterr = opterr;

   
  opterr = 1;

  const char *optstring = scan_all ? "" : "+";

  if ((c = getopt_long (argc, argv, optstring, long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'h':
          (*usage_func) (EXIT_SUCCESS);
          break;

        case 'v':
          {
            va_list authors;
            va_start (authors, usage_func);
            version_etc_va (stdout, command_name, package, version, authors);
            exit (EXIT_SUCCESS);
          }

        default:
          (*usage_func) (exit_failure);
          break;
        }
    }

   
  opterr = saved_opterr;
}
