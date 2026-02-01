 

 

 

 
 
 
 
 
#include "config.h"

#include "bashtypes.h"
#include "bashansi.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_STDDEF_H)
#  include <stddef.h>
#endif

#ifndef offsetof
#  define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#include "command.h"
#include "general.h"
#include "unwind_prot.h"
#include "sig.h"
#include "quit.h"
#include "bashintl.h"	 
#include "error.h"	 
#include "ocache.h"

 
typedef struct {
  char *variable;
  int size;
  char desired_setting[1];  
} SAVED_VAR;

 
typedef union uwp {
  struct uwp_head {
    union uwp *next;
    Function *cleanup;
  } head;
  struct {
    struct uwp_head uwp_head;
    char *v;
  } arg;
  struct {
    struct uwp_head uwp_head;
    SAVED_VAR v;
  } sv;
} UNWIND_ELT;

static void without_interrupts PARAMS((VFunction *, char *, char *));
static void unwind_frame_discard_internal PARAMS((char *, char *));
static void unwind_frame_run_internal PARAMS((char *, char *));
static void add_unwind_protect_internal PARAMS((Function *, char *));
static void remove_unwind_protect_internal PARAMS((char *, char *));
static void run_unwind_protects_internal PARAMS((char *, char *));
static void clear_unwind_protects_internal PARAMS((char *, char *));
static inline void restore_variable PARAMS((SAVED_VAR *));
static void unwind_protect_mem_internal PARAMS((char *, char *));

static UNWIND_ELT *unwind_protect_list = (UNWIND_ELT *)NULL;

 
#define UWCACHESIZE	128

sh_obj_cache_t uwcache = {0, 0, 0};

#if 0
#define uwpalloc(elt)	(elt) = (UNWIND_ELT *)xmalloc (sizeof (UNWIND_ELT))
#define uwpfree(elt)	free(elt)
#else
#define uwpalloc(elt)	ocache_alloc (uwcache, UNWIND_ELT, elt)
#define uwpfree(elt)	ocache_free (uwcache, UNWIND_ELT, elt)
#endif

void
uwp_init ()
{
  ocache_create (uwcache, UNWIND_ELT, UWCACHESIZE);
}

 
static void
without_interrupts (function, arg1, arg2)
     VFunction *function;
     char *arg1, *arg2;
{
  (*function)(arg1, arg2);
}

 
void
begin_unwind_frame (tag)
     char *tag;
{
  add_unwind_protect ((Function *)NULL, tag);
}

 
void
discard_unwind_frame (tag)
     char *tag;
{
  if (unwind_protect_list)
    without_interrupts (unwind_frame_discard_internal, tag, (char *)NULL);
}

 
void
run_unwind_frame (tag)
     char *tag;
{
  if (unwind_protect_list)
    without_interrupts (unwind_frame_run_internal, tag, (char *)NULL);
}

 
void
add_unwind_protect (cleanup, arg)
     Function *cleanup;
     char *arg;
{
  without_interrupts (add_unwind_protect_internal, (char *)cleanup, arg);
}

 
void
remove_unwind_protect ()
{
  if (unwind_protect_list)
    without_interrupts
      (remove_unwind_protect_internal, (char *)NULL, (char *)NULL);
}

 
void
run_unwind_protects ()
{
  if (unwind_protect_list)
    without_interrupts
      (run_unwind_protects_internal, (char *)NULL, (char *)NULL);
}

 
void
clear_unwind_protect_list (flags)
     int flags;
{
  char *flag;

  if (unwind_protect_list)
    {
      flag = flags ? "" : (char *)NULL;
      without_interrupts
        (clear_unwind_protects_internal, flag, (char *)NULL);
    }
}

int
have_unwind_protects ()
{
  return (unwind_protect_list != 0);
}

int
unwind_protect_tag_on_stack (tag)
     const char *tag;
{
  UNWIND_ELT *elt;

  elt = unwind_protect_list;
  while (elt)
    {
      if (elt->head.cleanup == 0 && STREQ (elt->arg.v, tag))
	return 1;
      elt = elt->head.next;
    }
  return 0;
}

 
 
 
 
 

static void
add_unwind_protect_internal (cleanup, arg)
     Function *cleanup;
     char *arg;
{
  UNWIND_ELT *elt;

  uwpalloc (elt);
  elt->head.next = unwind_protect_list;
  elt->head.cleanup = cleanup;
  elt->arg.v = arg;
  unwind_protect_list = elt;
}

static void
remove_unwind_protect_internal (ignore1, ignore2)
     char *ignore1, *ignore2;
{
  UNWIND_ELT *elt;

  elt = unwind_protect_list;
  if (elt)
    {
      unwind_protect_list = unwind_protect_list->head.next;
      uwpfree (elt);
    }
}

static void
run_unwind_protects_internal (ignore1, ignore2)
     char *ignore1, *ignore2;
{
  unwind_frame_run_internal ((char *) NULL, (char *) NULL);
}

static void
clear_unwind_protects_internal (flag, ignore)
     char *flag, *ignore;
{
  if (flag)
    {
      while (unwind_protect_list)
	remove_unwind_protect_internal ((char *)NULL, (char *)NULL);
    }
  unwind_protect_list = (UNWIND_ELT *)NULL;
}

static void
unwind_frame_discard_internal (tag, ignore)
     char *tag, *ignore;
{
  UNWIND_ELT *elt;
  int found;

  found = 0;
  while (elt = unwind_protect_list)
    {
      unwind_protect_list = unwind_protect_list->head.next;
      if (elt->head.cleanup == 0 && (STREQ (elt->arg.v, tag)))
	{
	  uwpfree (elt);
	  found = 1;
	  break;
	}
      else
	uwpfree (elt);
    }

  if (found == 0)
    internal_warning (_("unwind_frame_discard: %s: frame not found"), tag);
}

 
static inline void
restore_variable (sv)
     SAVED_VAR *sv;
{
  FASTCOPY (sv->desired_setting, sv->variable, sv->size);
}

static void
unwind_frame_run_internal (tag, ignore)
     char *tag, *ignore;
{
  UNWIND_ELT *elt;
  int found;

  found = 0;
  while (elt = unwind_protect_list)
    {
      unwind_protect_list = elt->head.next;

       
      if (elt->head.cleanup == 0)
	{
	  if (tag && STREQ (elt->arg.v, tag))
	    {
	      uwpfree (elt);
	      found = 1;
	      break;
	    }
	}
      else
	{
	  if (elt->head.cleanup == (Function *) restore_variable)
	    restore_variable (&elt->sv.v);
	  else
	    (*(elt->head.cleanup)) (elt->arg.v);
	}

      uwpfree (elt);
    }
  if (tag && found == 0)
    internal_warning (_("unwind_frame_run: %s: frame not found"), tag);
}

static void
unwind_protect_mem_internal (var, psize)
     char *var;
     char *psize;
{
  int size, allocated;
  UNWIND_ELT *elt;

  size = *(int *) psize;
  allocated = size + offsetof (UNWIND_ELT, sv.v.desired_setting[0]);
  if (allocated < sizeof (UNWIND_ELT))
    allocated = sizeof (UNWIND_ELT);
  elt = (UNWIND_ELT *)xmalloc (allocated);
  elt->head.next = unwind_protect_list;
  elt->head.cleanup = (Function *) restore_variable;
  elt->sv.v.variable = var;
  elt->sv.v.size = size;
  FASTCOPY (var, elt->sv.v.desired_setting, size);
  unwind_protect_list = elt;
}

 
void
unwind_protect_mem (var, size)
     char *var;
     int size;
{
  without_interrupts (unwind_protect_mem_internal, var, (char *) &size);
}

#if defined (DEBUG)
#include <stdio.h>

void
print_unwind_protect_tags ()
{
  UNWIND_ELT *elt;

  elt = unwind_protect_list;
  while (elt)
    {
      if (elt->head.cleanup == 0)
        fprintf(stderr, "tag: %s\n", elt->arg.v);
      elt = elt->head.next;
    }
}
#endif
