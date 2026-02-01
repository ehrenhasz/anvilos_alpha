 

 

 
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <errno.h>

#include "history.h"
#include "histlib.h"

#include "xmalloc.h"

#if !defined (errno)
extern int errno;
#endif

 
#define DEFAULT_HISTORY_INITIAL_SIZE	502

#define MAX_HISTORY_INITIAL_SIZE	8192

 
#define DEFAULT_HISTORY_GROW_SIZE 50

static char *hist_inittime (void);

 
 
 
 
 

 
static HIST_ENTRY **the_history = (HIST_ENTRY **)NULL;

 
static int history_stifled;

 
static int history_size;

 
int history_max_entries;
int max_input_history;	 

 
int history_offset;

 
int history_length;

 
int history_base = 1;

 
HISTORY_STATE *
history_get_history_state (void)
{
  HISTORY_STATE *state;

  state = (HISTORY_STATE *)xmalloc (sizeof (HISTORY_STATE));
  state->entries = the_history;
  state->offset = history_offset;
  state->length = history_length;
  state->size = history_size;
  state->flags = 0;
  if (history_stifled)
    state->flags |= HS_STIFLED;

  return (state);
}

 
void
history_set_history_state (HISTORY_STATE *state)
{
  the_history = state->entries;
  history_offset = state->offset;
  history_length = state->length;
  history_size = state->size;
  if (state->flags & HS_STIFLED)
    history_stifled = 1;
}

 
void
using_history (void)
{
  history_offset = history_length;
}

 
int
history_total_bytes (void)
{
  register int i, result;

  for (i = result = 0; the_history && the_history[i]; i++)
    result += HISTENT_BYTES (the_history[i]);

  return (result);
}

 
int
where_history (void)
{
  return (history_offset);
}

 
int
history_set_pos (int pos)
{
  if (pos > history_length || pos < 0 || !the_history)
    return (0);
  history_offset = pos;
  return (1);
}

 
int
_hs_at_end_of_history (void)
{
  return (the_history == 0 || history_offset == history_length);
}
 
 
HIST_ENTRY **
history_list (void)
{
  return (the_history);
}

 
HIST_ENTRY *
current_history (void)
{
  return ((history_offset == history_length) || the_history == 0)
		? (HIST_ENTRY *)NULL
		: the_history[history_offset];
}

 
HIST_ENTRY *
previous_history (void)
{
  return history_offset ? the_history[--history_offset] : (HIST_ENTRY *)NULL;
}

 
HIST_ENTRY *
next_history (void)
{
  return (history_offset == history_length) ? (HIST_ENTRY *)NULL : the_history[++history_offset];
}

 
HIST_ENTRY *
history_get (int offset)
{
  int local_index;

  local_index = offset - history_base;
  return (local_index >= history_length || local_index < 0 || the_history == 0)
		? (HIST_ENTRY *)NULL
		: the_history[local_index];
}

HIST_ENTRY *
alloc_history_entry (char *string, char *ts)
{
  HIST_ENTRY *temp;

  temp = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));

  temp->line = string ? savestring (string) : string;
  temp->data = (char *)NULL;
  temp->timestamp = ts;

  return temp;
}

time_t
history_get_time (HIST_ENTRY *hist)
{
  char *ts;
  time_t t;

  if (hist == 0 || hist->timestamp == 0)
    return 0;
  ts = hist->timestamp;
  if (ts[0] != history_comment_char)
    return 0;
  errno = 0;
  t = (time_t) strtol (ts + 1, (char **)NULL, 10);		 
  if (errno == ERANGE)
    return (time_t)0;
  return t;
}

static char *
hist_inittime (void)
{
  time_t t;
  char ts[64], *ret;

  t = (time_t) time ((time_t *)0);
#if defined (HAVE_VSNPRINTF)		 
  snprintf (ts, sizeof (ts) - 1, "X%lu", (unsigned long) t);
#else
  sprintf (ts, "X%lu", (unsigned long) t);
#endif
  ret = savestring (ts);
  ret[0] = history_comment_char;

  return ret;
}

 
void
add_history (const char *string)
{
  HIST_ENTRY *temp;
  int new_length;

  if (history_stifled && (history_length == history_max_entries))
    {
      register int i;

       
      if (history_length == 0)
	return;

       
      if (the_history[0])
	(void) free_history_entry (the_history[0]);

       
      memmove (the_history, the_history + 1, history_length * sizeof (HIST_ENTRY *));

      new_length = history_length;
      history_base++;
    }
  else
    {
      if (history_size == 0)
	{
	  if (history_stifled && history_max_entries > 0)
	    history_size = (history_max_entries > MAX_HISTORY_INITIAL_SIZE)
				? MAX_HISTORY_INITIAL_SIZE
				: history_max_entries + 2;
	  else
	    history_size = DEFAULT_HISTORY_INITIAL_SIZE;
	  the_history = (HIST_ENTRY **)xmalloc (history_size * sizeof (HIST_ENTRY *));
	  new_length = 1;
	}
      else
	{
	  if (history_length == (history_size - 1))
	    {
	      history_size += DEFAULT_HISTORY_GROW_SIZE;
	      the_history = (HIST_ENTRY **)
		xrealloc (the_history, history_size * sizeof (HIST_ENTRY *));
	    }
	  new_length = history_length + 1;
	}
    }

  temp = alloc_history_entry ((char *)string, hist_inittime ());

  the_history[new_length] = (HIST_ENTRY *)NULL;
  the_history[new_length - 1] = temp;
  history_length = new_length;
}

 
void
add_history_time (const char *string)
{
  HIST_ENTRY *hs;

  if (string == 0 || history_length < 1)
    return;
  hs = the_history[history_length - 1];
  FREE (hs->timestamp);
  hs->timestamp = savestring (string);
}

 
histdata_t
free_history_entry (HIST_ENTRY *hist)
{
  histdata_t x;

  if (hist == 0)
    return ((histdata_t) 0);
  FREE (hist->line);
  FREE (hist->timestamp);
  x = hist->data;
  xfree (hist);
  return (x);
}

HIST_ENTRY *
copy_history_entry (HIST_ENTRY *hist)
{
  HIST_ENTRY *ret;
  char *ts;

  if (hist == 0)
    return hist;

  ret = alloc_history_entry (hist->line, (char *)NULL);

  ts = hist->timestamp ? savestring (hist->timestamp) : hist->timestamp;
  ret->timestamp = ts;

  ret->data = hist->data;

  return ret;
}
  
 
HIST_ENTRY *
replace_history_entry (int which, const char *line, histdata_t data)
{
  HIST_ENTRY *temp, *old_value;

  if (which < 0 || which >= history_length)
    return ((HIST_ENTRY *)NULL);

  temp = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
  old_value = the_history[which];

  temp->line = savestring (line);
  temp->data = data;
  temp->timestamp = old_value->timestamp ? savestring (old_value->timestamp) : 0;
  the_history[which] = temp;

  return (old_value);
}

 
void
_hs_append_history_line (int which, const char *line)
{
  HIST_ENTRY *hent;
  size_t newlen, curlen, minlen;
  char *newline;

  hent = the_history[which];
  curlen = strlen (hent->line);
  minlen = curlen + strlen (line) + 2;	 
  if (curlen > 256)		 
    {
      newlen = 512;		 
       
      while (newlen < minlen)
	newlen <<= 1;
    }
  else
    newlen = minlen;
   
  newline = realloc (hent->line, newlen);
  if (newline)
    {
      hent->line = newline;
      hent->line[curlen++] = '\n';
      strcpy (hent->line + curlen, line);
    }
}

 
void
_hs_replace_history_data (int which, histdata_t *old, histdata_t *new)
{
  HIST_ENTRY *entry;
  register int i, last;

  if (which < -2 || which >= history_length || history_length == 0 || the_history == 0)
    return;

  if (which >= 0)
    {
      entry = the_history[which];
      if (entry && entry->data == old)
	entry->data = new;
      return;
    }

  last = -1;
  for (i = 0; i < history_length; i++)
    {
      entry = the_history[i];
      if (entry == 0)
	continue;
      if (entry->data == old)
	{
	  last = i;
	  if (which == -1)
	    entry->data = new;
	}
    }
  if (which == -2 && last >= 0)
    {
      entry = the_history[last];
      entry->data = new;	 
    }
}      
  
 
HIST_ENTRY *
remove_history (int which)
{
  HIST_ENTRY *return_value;
  register int i;
#if 1
  int nentries;
  HIST_ENTRY **start, **end;
#endif

  if (which < 0 || which >= history_length || history_length ==  0 || the_history == 0)
    return ((HIST_ENTRY *)NULL);

  return_value = the_history[which];

#if 1
   
  nentries = history_length - which;
  start = the_history + which;
  end = start + 1;
  memmove (start, end, nentries * sizeof (HIST_ENTRY *));
#else
  for (i = which; i < history_length; i++)
    the_history[i] = the_history[i + 1];
#endif

  history_length--;

  return (return_value);
}

HIST_ENTRY **
remove_history_range (int first, int last)
{
  HIST_ENTRY **return_value;
  register int i;
  int nentries;
  HIST_ENTRY **start, **end;

  if (the_history == 0 || history_length == 0)
    return ((HIST_ENTRY **)NULL);
  if (first < 0 || first >= history_length || last < 0 || last >= history_length)
    return ((HIST_ENTRY **)NULL);
  if (first > last)
    return (HIST_ENTRY **)NULL;

  nentries = last - first + 1;
  return_value = (HIST_ENTRY **)malloc ((nentries + 1) * sizeof (HIST_ENTRY *));
  if (return_value == 0)
    return return_value;

   
  for (i = first ; i <= last; i++)
    return_value[i - first] = the_history[i];
  return_value[i - first] = (HIST_ENTRY *)NULL;

   
  start = the_history + first;
  end = the_history + last + 1;
  memmove (start, end, (history_length - last) * sizeof (HIST_ENTRY *));

  history_length -= nentries;

  return (return_value);
}

 
void
stifle_history (int max)
{
  register int i, j;

  if (max < 0)
    max = 0;

  if (history_length > max)
    {
       
      for (i = 0, j = history_length - max; i < j; i++)
	free_history_entry (the_history[i]);

      history_base = i;
      for (j = 0, i = history_length - max; j < max; i++, j++)
	the_history[j] = the_history[i];
      the_history[j] = (HIST_ENTRY *)NULL;
      history_length = j;
    }

  history_stifled = 1;
  max_input_history = history_max_entries = max;
}

 
int
unstifle_history (void)
{
  if (history_stifled)
    {
      history_stifled = 0;
      return (history_max_entries);
    }
  else
    return (-history_max_entries);
}

int
history_is_stifled (void)
{
  return (history_stifled);
}

void
clear_history (void)
{
  register int i;

   
  for (i = 0; i < history_length; i++)
    {
      free_history_entry (the_history[i]);
      the_history[i] = (HIST_ENTRY *)NULL;
    }

  history_offset = history_length = 0;
  history_base = 1;		 
}
