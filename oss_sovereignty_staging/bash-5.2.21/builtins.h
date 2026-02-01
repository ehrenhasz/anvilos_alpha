 

 

#ifndef BUILTINS_H
#define BUILTINS_H

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "command.h"
#include "general.h"

#if defined (ALIAS)
#include "alias.h"
#endif

 
#define BUILTIN_ENABLED 0x01	 
#define BUILTIN_DELETED 0x02	 
#define STATIC_BUILTIN  0x04	 
#define SPECIAL_BUILTIN 0x08	 
#define ASSIGNMENT_BUILTIN 0x10	 
#define POSIX_BUILTIN	0x20	 
#define LOCALVAR_BUILTIN   0x40	 
#define ARRAYREF_BUILTIN 0x80	 

#define BASE_INDENT	4

 
struct builtin {
  char *name;			 
  sh_builtin_func_t *function;	 
  int flags;			 
  char * const *long_doc;	 
  const char *short_doc;	 
  char *handle;			 
};

 
extern int num_shell_builtins;	 
extern struct builtin static_shell_builtins[];
extern struct builtin *shell_builtins;
extern struct builtin *current_builtin;

#endif  
