 

 

#include "config.h"

#if defined (ARRAY_VARS)

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include "bashansi.h"

#include "shell.h"
#include "array.h"
#include "assoc.h"
#include "builtins/common.h"

static WORD_LIST *assoc_to_word_list_internal PARAMS((HASH_TABLE *, int));

 

void
assoc_dispose (hash)
     HASH_TABLE *hash;
{
  if (hash)
    {
      hash_flush (hash, 0);
      hash_dispose (hash);
    }
}

void
assoc_flush (hash)
     HASH_TABLE *hash;
{
  hash_flush (hash, 0);
}

int
assoc_insert (hash, key, value)
     HASH_TABLE *hash;
     char *key;
     char *value;
{
  BUCKET_CONTENTS *b;

  b = hash_search (key, hash, HASH_CREATE);
  if (b == 0)
    return -1;
   
  if (b->key != key)
    free (key);
  FREE (b->data);
  b->data = value ? savestring (value) : (char *)0;
  return (0);
}

 
PTR_T
assoc_replace (hash, key, value)
     HASH_TABLE *hash;
     char *key;
     char *value;
{
  BUCKET_CONTENTS *b;
  PTR_T t;

  b = hash_search (key, hash, HASH_CREATE);
  if (b == 0)
    return (PTR_T)0;
   
  if (b->key != key)
    free (key);
  t = b->data;
  b->data = value ? savestring (value) : (char *)0;
  return t;
}

void
assoc_remove (hash, string)
     HASH_TABLE *hash;
     char *string;
{
  BUCKET_CONTENTS *b;

  b = hash_remove (string, hash, 0);
  if (b)
    {
      free ((char *)b->data);
      free (b->key);
      free (b);
    }
}

char *
assoc_reference (hash, string)
     HASH_TABLE *hash;
     char *string;
{
  BUCKET_CONTENTS *b;

  if (hash == 0)
    return (char *)0;

  b = hash_search (string, hash, 0);
  return (b ? (char *)b->data : 0);
}

 
HASH_TABLE *
assoc_quote (h)
     HASH_TABLE *h;
{
  int i;
  BUCKET_CONTENTS *tlist;
  char *t;

  if (h == 0 || assoc_empty (h))
    return ((HASH_TABLE *)NULL);
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	t = quote_string ((char *)tlist->data);
	FREE (tlist->data);
	tlist->data = t;
      }

  return h;
}

 
HASH_TABLE *
assoc_quote_escapes (h)
     HASH_TABLE *h;
{
  int i;
  BUCKET_CONTENTS *tlist;
  char *t;

  if (h == 0 || assoc_empty (h))
    return ((HASH_TABLE *)NULL);
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	t = quote_escapes ((char *)tlist->data);
	FREE (tlist->data);
	tlist->data = t;
      }

  return h;
}

HASH_TABLE *
assoc_dequote (h)
     HASH_TABLE *h;
{
  int i;
  BUCKET_CONTENTS *tlist;
  char *t;

  if (h == 0 || assoc_empty (h))
    return ((HASH_TABLE *)NULL);
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	t = dequote_string ((char *)tlist->data);
	FREE (tlist->data);
	tlist->data = t;
      }

  return h;
}

HASH_TABLE *
assoc_dequote_escapes (h)
     HASH_TABLE *h;
{
  int i;
  BUCKET_CONTENTS *tlist;
  char *t;

  if (h == 0 || assoc_empty (h))
    return ((HASH_TABLE *)NULL);
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	t = dequote_escapes ((char *)tlist->data);
	FREE (tlist->data);
	tlist->data = t;
      }

  return h;
}

HASH_TABLE *
assoc_remove_quoted_nulls (h)
     HASH_TABLE *h;
{
  int i;
  BUCKET_CONTENTS *tlist;
  char *t;

  if (h == 0 || assoc_empty (h))
    return ((HASH_TABLE *)NULL);
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	t = remove_quoted_nulls ((char *)tlist->data);
	tlist->data = t;
      }

  return h;
}

 
char *
assoc_subrange (hash, start, nelem, starsub, quoted, pflags)
     HASH_TABLE *hash;
     arrayind_t start, nelem;
     int starsub, quoted, pflags;
{
  WORD_LIST *l, *save, *h, *t;
  int i, j;
  char *ret;

  if (assoc_empty (hash))
    return ((char *)NULL);

  save = l = assoc_to_word_list (hash);
  if (save == 0)
    return ((char *)NULL);

  for (i = 1; l && i < start; i++)
    l = l->next;
  if (l == 0)
    {
      dispose_words (save);
      return ((char *)NULL);
    }
  for (j = 0,h = t = l; l && j < nelem; j++)
    {
      t = l;
      l = l->next;
    }

  t->next = (WORD_LIST *)NULL;

  ret = string_list_pos_params (starsub ? '*' : '@', h, quoted, pflags);

  if (t != l)
    t->next = l;

  dispose_words (save);
  return (ret);

}

char *
assoc_patsub (h, pat, rep, mflags)
     HASH_TABLE *h;
     char *pat, *rep;
     int mflags;
{
  char	*t;
  int pchar, qflags, pflags;
  WORD_LIST *wl, *save;

  if (h == 0 || assoc_empty (h))
    return ((char *)NULL);

  wl = assoc_to_word_list (h);
  if (wl == 0)
    return (char *)NULL;

  for (save = wl; wl; wl = wl->next)
    {
      t = pat_subst (wl->word->word, pat, rep, mflags);
      FREE (wl->word->word);
      wl->word->word = t;
    }

  pchar = (mflags & MATCH_STARSUB) == MATCH_STARSUB ? '*' : '@';
  qflags = (mflags & MATCH_QUOTED) == MATCH_QUOTED ? Q_DOUBLE_QUOTES : 0;
  pflags = (mflags & MATCH_ASSIGNRHS) == MATCH_ASSIGNRHS ? PF_ASSIGNRHS : 0;

  t = string_list_pos_params (pchar, save, qflags, pflags);
  dispose_words (save);

  return t;
}

char *
assoc_modcase (h, pat, modop, mflags)
     HASH_TABLE *h;
     char *pat;
     int modop;
     int mflags;
{
  char	*t;
  int pchar, qflags, pflags;
  WORD_LIST *wl, *save;

  if (h == 0 || assoc_empty (h))
    return ((char *)NULL);

  wl = assoc_to_word_list (h);
  if (wl == 0)
    return ((char *)NULL);

  for (save = wl; wl; wl = wl->next)
    {
      t = sh_modcase (wl->word->word, pat, modop);
      FREE (wl->word->word);
      wl->word->word = t;
    }

  pchar = (mflags & MATCH_STARSUB) == MATCH_STARSUB ? '*' : '@';
  qflags = (mflags & MATCH_QUOTED) == MATCH_QUOTED ? Q_DOUBLE_QUOTES : 0;
  pflags = (mflags & MATCH_ASSIGNRHS) == MATCH_ASSIGNRHS ? PF_ASSIGNRHS : 0;

  t = string_list_pos_params (pchar, save, qflags, pflags);
  dispose_words (save);

  return t;
}

char *
assoc_to_kvpair (hash, quoted)
     HASH_TABLE *hash;
     int quoted;
{
  char *ret;
  char *istr, *vstr;
  int i, rsize, rlen, elen;
  BUCKET_CONTENTS *tlist;

  if (hash == 0 || assoc_empty (hash))
    return (char *)0;

  ret = xmalloc (rsize = 128);
  ret[rlen = 0] = '\0';

  for (i = 0; i < hash->nbuckets; i++)
    for (tlist = hash_items (i, hash); tlist; tlist = tlist->next)
      {
	if (ansic_shouldquote (tlist->key))
	  istr = ansic_quote (tlist->key, 0, (int *)0);
	else if (sh_contains_shell_metas (tlist->key))
	  istr = sh_double_quote (tlist->key);
	else if (ALL_ELEMENT_SUB (tlist->key[0]) && tlist->key[1] == '\0')
	  istr = sh_double_quote (tlist->key);	
	else
	  istr = tlist->key;	

	vstr = tlist->data ? (ansic_shouldquote ((char *)tlist->data) ?
				ansic_quote ((char *)tlist->data, 0, (int *)0) :
				sh_double_quote ((char *)tlist->data))
			   : (char *)0;

	elen = STRLEN (istr) + 4 + STRLEN (vstr);
	RESIZE_MALLOCED_BUFFER (ret, rlen, (elen+1), rsize, rsize);

	strcpy (ret+rlen, istr);
	rlen += STRLEN (istr);
	ret[rlen++] = ' ';
	if (vstr)
	  {
	    strcpy (ret + rlen, vstr);
	    rlen += STRLEN (vstr);
	  }
	else
	  {
	    strcpy (ret + rlen, "\"\"");
	    rlen += 2;
	  }
	ret[rlen++] = ' ';

	if (istr != tlist->key)
	  FREE (istr);

	FREE (vstr);
    }

  RESIZE_MALLOCED_BUFFER (ret, rlen, 1, rsize, 8);
  ret[rlen] = '\0';

  if (quoted)
    {
      vstr = sh_single_quote (ret);
      free (ret);
      ret = vstr;
    }

  return ret;
}

char *
assoc_to_assign (hash, quoted)
     HASH_TABLE *hash;
     int quoted;
{
  char *ret;
  char *istr, *vstr;
  int i, rsize, rlen, elen;
  BUCKET_CONTENTS *tlist;

  if (hash == 0 || assoc_empty (hash))
    return (char *)0;

  ret = xmalloc (rsize = 128);
  ret[0] = '(';
  rlen = 1;

  for (i = 0; i < hash->nbuckets; i++)
    for (tlist = hash_items (i, hash); tlist; tlist = tlist->next)
      {
	if (ansic_shouldquote (tlist->key))
	  istr = ansic_quote (tlist->key, 0, (int *)0);
	else if (sh_contains_shell_metas (tlist->key))
	  istr = sh_double_quote (tlist->key);
	else if (ALL_ELEMENT_SUB (tlist->key[0]) && tlist->key[1] == '\0')
	  istr = sh_double_quote (tlist->key);	
	else
	  istr = tlist->key;	

	vstr = tlist->data ? (ansic_shouldquote ((char *)tlist->data) ?
				ansic_quote ((char *)tlist->data, 0, (int *)0) :
				sh_double_quote ((char *)tlist->data))
			   : (char *)0;

	elen = STRLEN (istr) + 8 + STRLEN (vstr);
	RESIZE_MALLOCED_BUFFER (ret, rlen, (elen+1), rsize, rsize);

	ret[rlen++] = '[';
	strcpy (ret+rlen, istr);
	rlen += STRLEN (istr);
	ret[rlen++] = ']';
	ret[rlen++] = '=';
	if (vstr)
	  {
	    strcpy (ret + rlen, vstr);
	    rlen += STRLEN (vstr);
	  }
	ret[rlen++] = ' ';

	if (istr != tlist->key)
	  FREE (istr);

	FREE (vstr);
    }

  RESIZE_MALLOCED_BUFFER (ret, rlen, 1, rsize, 8);
  ret[rlen++] = ')';
  ret[rlen] = '\0';

  if (quoted)
    {
      vstr = sh_single_quote (ret);
      free (ret);
      ret = vstr;
    }

  return ret;
}

static WORD_LIST *
assoc_to_word_list_internal (h, t)
     HASH_TABLE *h;
     int t;
{
  WORD_LIST *list;
  int i;
  BUCKET_CONTENTS *tlist;
  char *w;

  if (h == 0 || assoc_empty (h))
    return((WORD_LIST *)NULL);
  list = (WORD_LIST *)NULL;
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	w = (t == 0) ? (char *)tlist->data : (char *)tlist->key;
	list = make_word_list (make_bare_word(w), list);
      }
  return (REVERSE_LIST(list, WORD_LIST *));
}

WORD_LIST *
assoc_to_word_list (h)
     HASH_TABLE *h;
{
  return (assoc_to_word_list_internal (h, 0));
}

WORD_LIST *
assoc_keys_to_word_list (h)
     HASH_TABLE *h;
{
  return (assoc_to_word_list_internal (h, 1));
}

WORD_LIST *
assoc_to_kvpair_list (h)
     HASH_TABLE *h;
{
  WORD_LIST *list;
  int i;
  BUCKET_CONTENTS *tlist;
  char *k, *v;

  if (h == 0 || assoc_empty (h))
    return((WORD_LIST *)NULL);
  list = (WORD_LIST *)NULL;
  
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
      	k = (char *)tlist->key;
      	v = (char *)tlist->data;
	list = make_word_list (make_bare_word (k), list);
	list = make_word_list (make_bare_word (v), list);
      }
  return (REVERSE_LIST(list, WORD_LIST *));
}

char *
assoc_to_string (h, sep, quoted)
     HASH_TABLE *h;
     char *sep;
     int quoted;
{
  BUCKET_CONTENTS *tlist;
  int i;
  char *result, *t, *w;
  WORD_LIST *list, *l;

  if (h == 0)
    return ((char *)NULL);
  if (assoc_empty (h))
    return (savestring (""));

  result = NULL;
  l = list = NULL;
   
  for (i = 0; i < h->nbuckets; i++)
    for (tlist = hash_items (i, h); tlist; tlist = tlist->next)
      {
	w = (char *)tlist->data;
	if (w == 0)
	  continue;
	t = quoted ? quote_string (w) : savestring (w);
	list = make_word_list (make_bare_word(t), list);
	FREE (t);
      }

  l = REVERSE_LIST(list, WORD_LIST *);

  result = l ? string_list_internal (l, sep) : savestring ("");
  dispose_words (l);  

  return result;
}

#endif  
