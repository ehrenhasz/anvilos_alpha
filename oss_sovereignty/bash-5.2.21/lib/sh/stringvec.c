 

 

#include <config.h>

#include <bashtypes.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <bashansi.h>
#include <stdio.h>
#include <chartypes.h>

#include "shell.h"

 
char **
strvec_create (n)
     int n;
{
  return ((char **)xmalloc ((n) * sizeof (char *)));
}

 
char **
strvec_mcreate (n)
     int n;
{
  return ((char **)malloc ((n) * sizeof (char *)));
}

char **
strvec_resize (array, nsize)
     char **array;
     int nsize;
{
  return ((char **)xrealloc (array, nsize * sizeof (char *)));
}

char **
strvec_mresize (array, nsize)
     char **array;
     int nsize;
{
  return ((char **)realloc (array, nsize * sizeof (char *)));
}

 
int
strvec_len (array)
     char **array;
{
  register int i;

  for (i = 0; array[i]; i++);
  return (i);
}

 
void
strvec_flush (array)
     char **array;
{
  register int i;

  if (array == 0)
    return;

  for (i = 0; array[i]; i++)
    free (array[i]);
}

void
strvec_dispose (array)
     char **array;
{
  if (array == 0)
    return;

  strvec_flush (array);
  free (array);
}

int
strvec_remove (array, name)
     char **array, *name;
{
  register int i, j;
  char *x;

  if (array == 0)
    return 0;

  for (i = 0; array[i]; i++)
    if (STREQ (name, array[i]))
      {
	x = array[i];
	for (j = i; array[j]; j++)
	  array[j] = array[j + 1];
	free (x);
	return 1;
      }
  return 0;
}

 
int
strvec_search (array, name)
     char **array, *name;
{
  int i;

  for (i = 0; array[i]; i++)
    if (STREQ (name, array[i]))
      return (i);

  return (-1);
}

 
char **
strvec_copy (array)
     char **array;
{
  register int i;
  int len;
  char **ret;

  len = strvec_len (array);

  ret = (char **)xmalloc ((len + 1) * sizeof (char *));
  for (i = 0; array[i]; i++)
    ret[i] = savestring (array[i]);
  ret[i] = (char *)NULL;

  return (ret);
}

 
int
strvec_posixcmp (s1, s2)
     register char **s1, **s2;
{
  int result;

#if defined (HAVE_STRCOLL)
   result = strcoll (*s1, *s2);
   if (result != 0)
     return result;
#endif

  if ((result = **s1 - **s2) == 0)
    result = strcmp (*s1, *s2);

  return (result);
}

 
int
strvec_strcmp (s1, s2)
     register char **s1, **s2;
{
#if defined (HAVE_STRCOLL)
   return (strcoll (*s1, *s2));
#else  
  int result;

  if ((result = **s1 - **s2) == 0)
    result = strcmp (*s1, *s2);

  return (result);
#endif  
}

 
void
strvec_sort (array, posix)
     char **array;
     int posix;
{
  if (posix)
    qsort (array, strvec_len (array), sizeof (char *), (QSFUNC *)strvec_posixcmp);
  else
    qsort (array, strvec_len (array), sizeof (char *), (QSFUNC *)strvec_strcmp);
}

 

char **
strvec_from_word_list (list, alloc, starting_index, ip)
     WORD_LIST *list;
     int alloc, starting_index, *ip;
{
  int count;
  char **array;

  count = list_length (list);
  array = (char **)xmalloc ((1 + count + starting_index) * sizeof (char *));

  for (count = 0; count < starting_index; count++)
    array[count] = (char *)NULL;
  for (count = starting_index; list; count++, list = list->next)
    array[count] = alloc ? savestring (list->word->word) : list->word->word;
  array[count] = (char *)NULL;

  if (ip)
    *ip = count;
  return (array);
}

 

WORD_LIST *
strvec_to_word_list (array, alloc, starting_index)
     char **array;
     int alloc, starting_index;
{
  WORD_LIST *list;
  WORD_DESC *w;
  int i, count;

  if (array == 0 || array[0] == 0)
    return (WORD_LIST *)NULL;

  for (count = 0; array[count]; count++)
    ;

  for (i = starting_index, list = (WORD_LIST *)NULL; i < count; i++)
    {
      w = make_bare_word (alloc ? array[i] : "");
      if (alloc == 0)
	{
	  free (w->word);
	  w->word = array[i];
	}
      list = make_word_list (w, list);
    }
  return (REVERSE_LIST (list, WORD_LIST *));
}
