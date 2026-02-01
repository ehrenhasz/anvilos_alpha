 

 

#include "config.h"

#if defined (ALIAS)

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include "chartypes.h"
#include "bashansi.h"
#include "command.h"
#include "general.h"
#include "externs.h"
#include "alias.h"

#if defined (PROGRAMMABLE_COMPLETION)
#  include "pcomplete.h"
#endif

#if defined (HAVE_MBSTR_H) && defined (HAVE_MBSCHR)
#  include <mbstr.h>		 
#endif

#define ALIAS_HASH_BUCKETS	64	 

typedef int sh_alias_map_func_t PARAMS((alias_t *));

static void free_alias_data PARAMS((PTR_T));
static alias_t **map_over_aliases PARAMS((sh_alias_map_func_t *));
static void sort_aliases PARAMS((alias_t **));
static int qsort_alias_compare PARAMS((alias_t **, alias_t **));

#if defined (READLINE)
static int skipquotes PARAMS((char *, int));
static int skipws PARAMS((char *, int));
static int rd_token PARAMS((char *, int));
#endif

 
int alias_expand_all = 0;

 
HASH_TABLE *aliases = (HASH_TABLE *)NULL;

void
initialize_aliases ()
{
  if (aliases == 0)
    aliases = hash_create (ALIAS_HASH_BUCKETS);
}

 
alias_t *
find_alias (name)
     char *name;
{
  BUCKET_CONTENTS *al;

  if (aliases == 0)
    return ((alias_t *)NULL);

  al = hash_search (name, aliases, 0);
  return (al ? (alias_t *)al->data : (alias_t *)NULL);
}

 
char *
get_alias_value (name)
     char *name;
{
  alias_t *alias;

  if (aliases == 0)
    return ((char *)NULL);

  alias = find_alias (name);
  return (alias ? alias->value : (char *)NULL);
}

 
void
add_alias (name, value)
     char *name, *value;
{
  BUCKET_CONTENTS *elt;
  alias_t *temp;
  int n;

  if (aliases == 0)
    {
      initialize_aliases ();
      temp = (alias_t *)NULL;
    }
  else
    temp = find_alias (name);

  if (temp)
    {
      free (temp->value);
      temp->value = savestring (value);
      temp->flags &= ~AL_EXPANDNEXT;
      if (value[0])
	{
	  n = value[strlen (value) - 1];
	  if (n == ' ' || n == '\t')
	    temp->flags |= AL_EXPANDNEXT;
	}
    }
  else
    {
      temp = (alias_t *)xmalloc (sizeof (alias_t));
      temp->name = savestring (name);
      temp->value = savestring (value);
      temp->flags = 0;

      if (value[0])
	{
	  n = value[strlen (value) - 1];
	  if (n == ' ' || n == '\t')
	    temp->flags |= AL_EXPANDNEXT;
	}

      elt = hash_insert (savestring (name), aliases, HASH_NOSRCH);
      elt->data = temp;
#if defined (PROGRAMMABLE_COMPLETION)
      set_itemlist_dirty (&it_aliases);
#endif
    }
}

 
static void
free_alias_data (data)
     PTR_T data;
{
  register alias_t *a;

  a = (alias_t *)data;

  if (a->flags & AL_BEINGEXPANDED)
    clear_string_list_expander (a);	 

  free (a->value);
  free (a->name);
  free (data);
}

 
int
remove_alias (name)
     char *name;
{
  BUCKET_CONTENTS *elt;

  if (aliases == 0)
    return (-1);

  elt = hash_remove (name, aliases, 0);
  if (elt)
    {
      free_alias_data (elt->data);
      free (elt->key);		 
      free (elt);		 
#if defined (PROGRAMMABLE_COMPLETION)
      set_itemlist_dirty (&it_aliases);
#endif
      return (aliases->nentries);
    }
  return (-1);
}

 
void
delete_all_aliases ()
{
  if (aliases == 0)
    return;

  hash_flush (aliases, free_alias_data);
  hash_dispose (aliases);
  aliases = (HASH_TABLE *)NULL;
#if defined (PROGRAMMABLE_COMPLETION)
  set_itemlist_dirty (&it_aliases);
#endif
}

 
static alias_t **
map_over_aliases (function)
     sh_alias_map_func_t *function;
{
  register int i;
  register BUCKET_CONTENTS *tlist;
  alias_t *alias, **list;
  int list_index;

  i = HASH_ENTRIES (aliases);
  if (i == 0)
    return ((alias_t **)NULL);

  list = (alias_t **)xmalloc ((i + 1) * sizeof (alias_t *));
  for (i = list_index = 0; i < aliases->nbuckets; i++)
    {
      for (tlist = hash_items (i, aliases); tlist; tlist = tlist->next)
	{
	  alias = (alias_t *)tlist->data;

	  if (!function || (*function) (alias))
	    {
	      list[list_index++] = alias;
	      list[list_index] = (alias_t *)NULL;
	    }
	}
    }
  return (list);
}

static void
sort_aliases (array)
     alias_t **array;
{
  qsort (array, strvec_len ((char **)array), sizeof (alias_t *), (QSFUNC *)qsort_alias_compare);
}

static int
qsort_alias_compare (as1, as2)
     alias_t **as1, **as2;
{
  int result;

  if ((result = (*as1)->name[0] - (*as2)->name[0]) == 0)
    result = strcmp ((*as1)->name, (*as2)->name);

  return (result);
}

 
alias_t **
all_aliases ()
{
  alias_t **list;

  if (aliases == 0 || HASH_ENTRIES (aliases) == 0)
    return ((alias_t **)NULL);

  list = map_over_aliases ((sh_alias_map_func_t *)NULL);
  if (list)
    sort_aliases (list);
  return (list);
}

char *
alias_expand_word (s)
     char *s;
{
  alias_t *r;

  r = find_alias (s);
  return (r ? savestring (r->value) : (char *)NULL);
}

 

#if defined (READLINE)

 
#define self_delimiting(character) (member ((character), " \t\n\r;|&()"))

 
#define command_separator(character) (member ((character), "\r\n;|&("))

 
static int command_word;

 
#define quote_char(c)  (((c) == '\'') || ((c) == '"'))

 

static int
skipquotes (string, start)
     char *string;
     int start;
{
  register int i;
  int delimiter = string[start];

   
  for (i = start + 1 ; string[i] ; i++)
    {
      if (string[i] == '\\')
	{
	  i++;		 
	  if (string[i] == 0)
	    break;
	  continue;
	}

      if (string[i] == delimiter)
	return i;
    }
  return (i);
}

 
static int
skipws (string, start)
     char *string;
     int start;
{
  register int i;
  int pass_next, backslash_quoted_word;
  unsigned char peekc;

   
  i = backslash_quoted_word = pass_next = 0;

   

  for (i = start; string[i]; i++)
    {
      if (pass_next)
	{
	  pass_next = 0;
	  continue;
	}

      if (whitespace (string[i]))
	{
	  backslash_quoted_word = 0;  
	  continue;
	}

      if (string[i] == '\\')
	{
	  peekc = string[i+1];
	  if (peekc == 0)
	    break;
	  if (ISLETTER (peekc))
	    backslash_quoted_word++;	 
	  else
	    pass_next++;
	  continue;
	}

       
      if (quote_char(string[i]))
	{
	  i = skipquotes (string, i);
	   
	  if (string[i] == '\0')
	    break;

	  peekc = string[i + 1];
	  if (ISLETTER (peekc))
	    backslash_quoted_word++;
	  continue;
	}

       
      if (backslash_quoted_word)
	continue;

       

      if (command_separator (string[i]))
	{
	  command_word++;
	  continue;
	}
      break;
    }
  return (i);
}

 
#define token_char(c)	(!((whitespace (string[i]) || self_delimiting (string[i]))))

 
static int
rd_token (string, start)
     char *string;
     int start;
{
  register int i;

   
  for (i = start; string[i] && token_char (string[i]); i++)
    {
      if (string[i] == '\\')
	{
	  i++;	 
	  if (string[i] == 0)
	    break;
	  continue;
	}

       
      if (quote_char (string[i]))
	{
	  i = skipquotes (string, i);
	   
	  if (string[i] == '\0')
	    break;

	   
	  continue;
	}
    }
  return (i);
}

 
char *
alias_expand (string)
     char *string;
{
  register int i, j, start;
  char *line, *token;
  int line_len, tl, real_start, expand_next, expand_this_token;
  alias_t *alias;

  line_len = strlen (string) + 1;
  line = (char *)xmalloc (line_len);
  token = (char *)xmalloc (line_len);

  line[0] = i = 0;
  expand_next = 0;
  command_word = 1;  

   

  for (;;)
    {

      token[0] = 0;
      start = i;

       
      i = skipws (string, start);

      if (start == i && string[i] == '\0')
	{
	  free (token);
	  return (line);
	}

       
      j = strlen (line);
      tl = i - start;	 
      RESIZE_MALLOCED_BUFFER (line, j, (tl + 1), line_len, (tl + 50));
      strncpy (line + j, string + start, tl);
      line[j + tl] = '\0';

      real_start = i;

      command_word = command_word || (command_separator (string[i]));
      expand_this_token = (command_word || expand_next);
      expand_next = 0;

       
      start = i;
      i = rd_token (string, start);

      tl = i - start;	 

       
      if (tl == 0 && string[i] != '\0')
	{
	  tl = 1;
	  i++;		 
	}

      strncpy (token, string + start, tl);
      token [tl] = '\0';

       
      if (mbschr (token, '\\'))
	expand_this_token = 0;

       

      if ((token[0]) &&
	  (expand_this_token || alias_expand_all) &&
	  (alias = find_alias (token)))
	{
	  char *v;
	  int vlen, llen;

	  v = alias->value;
	  vlen = strlen (v);
	  llen = strlen (line);

	   
	  RESIZE_MALLOCED_BUFFER (line, llen, (vlen + 3), line_len, (vlen + 50));

	  strcpy (line + llen, v);

	  if ((expand_this_token && vlen && whitespace (v[vlen - 1])) ||
	      alias_expand_all)
	    expand_next = 1;
	}
      else
	{
	  int llen, tlen;

	  llen = strlen (line);
	  tlen = i - real_start;  

	  RESIZE_MALLOCED_BUFFER (line, llen, (tlen + 1), line_len, (llen + tlen + 50));

	  strncpy (line + llen, string + real_start, tlen);
	  line[llen + tlen] = '\0';
	}
      command_word = 0;
    }
}
#endif  
#endif  
