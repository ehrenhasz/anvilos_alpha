 

 

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "shell.h"

 
GENERIC_LIST global_error_list;

#ifdef INCLUDE_UNUSED
 
void
list_walk (list, function)
     GENERIC_LIST *list;
     sh_glist_func_t *function;
{
  for ( ; list; list = list->next)
    if ((*function) (list) < 0)
      return;
}

 
void
wlist_walk (words, function)
     WORD_LIST *words;
     sh_icpfunc_t *function;
{
  for ( ; words; words = words->next)
    if ((*function) (words->word->word) < 0)
      return;
}
#endif  

 
GENERIC_LIST *
list_reverse (list)
     GENERIC_LIST *list;
{
  register GENERIC_LIST *next, *prev;

  for (prev = (GENERIC_LIST *)NULL; list; )
    {
      next = list->next;
      list->next = prev;
      prev = list;
      list = next;
    }
  return (prev);
}

 
int
list_length (list)
     GENERIC_LIST *list;
{
  register int i;

  for (i = 0; list; list = list->next, i++);
  return (i);
}

 
GENERIC_LIST *
list_append (head, tail)
     GENERIC_LIST *head, *tail;
{
  register GENERIC_LIST *t_head;

  if (head == 0)
    return (tail);

  for (t_head = head; t_head->next; t_head = t_head->next)
    ;
  t_head->next = tail;
  return (head);
}

#ifdef INCLUDE_UNUSED
 
GENERIC_LIST *
list_remove (list, comparer, arg)
     GENERIC_LIST **list;
     Function *comparer;
     char *arg;
{
  register GENERIC_LIST *prev, *temp;

  for (prev = (GENERIC_LIST *)NULL, temp = *list; temp; prev = temp, temp = temp->next)
    {
      if ((*comparer) (temp, arg))
	{
	  if (prev)
	    prev->next = temp->next;
	  else
	    *list = temp->next;
	  return (temp);
	}
    }
  return ((GENERIC_LIST *)&global_error_list);
}
#endif
