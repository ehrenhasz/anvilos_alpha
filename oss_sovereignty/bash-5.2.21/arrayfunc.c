 

 

#include "config.h"

#if defined (ARRAY_VARS)

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif
#include <stdio.h>

#include "bashintl.h"

#include "shell.h"
#include "execute_cmd.h"
#include "pathexp.h"

#include "shmbutil.h"
#if defined (HAVE_MBSTR_H) && defined (HAVE_MBSCHR)
#  include <mbstr.h>		 
#endif

#include "builtins/common.h"

#ifndef LBRACK
#  define LBRACK '['
#  define RBRACK ']'
#endif

 
int assoc_expand_once = 0;

 
int array_expand_once = 0;

static SHELL_VAR *bind_array_var_internal PARAMS((SHELL_VAR *, arrayind_t, char *, char *, int));
static SHELL_VAR *assign_array_element_internal PARAMS((SHELL_VAR *, char *, char *, char *, int, char *, int, array_eltstate_t *));

static void assign_assoc_from_kvlist PARAMS((SHELL_VAR *, WORD_LIST *, HASH_TABLE *, int));

static char *quote_assign PARAMS((const char *));
static void quote_array_assignment_chars PARAMS((WORD_LIST *));
static char *quote_compound_array_word PARAMS((char *, int));
static char *array_value_internal PARAMS((const char *, int, int, array_eltstate_t *));

 
const char * const bash_badsub_errmsg = N_("bad array subscript");

 
 
 
 
 

 
SHELL_VAR *
convert_var_to_array (var)
     SHELL_VAR *var;
{
  char *oldval;
  ARRAY *array;

  oldval = value_cell (var);
  array = array_create ();
  if (oldval)
    array_insert (array, 0, oldval);

  FREE (value_cell (var));
  var_setarray (var, array);

   
  var->dynamic_value = (sh_var_value_func_t *)NULL;
  var->assign_func = (sh_var_assign_func_t *)NULL;

  INVALIDATE_EXPORTSTR (var);
  if (exported_p (var))
    array_needs_making++;

  VSETATTR (var, att_array);
  if (oldval)
    VUNSETATTR (var, att_invisible);

   
  VUNSETATTR (var, att_assoc);

   
  VUNSETATTR (var, att_nameref);

  return var;
}

 
SHELL_VAR *
convert_var_to_assoc (var)
     SHELL_VAR *var;
{
  char *oldval;
  HASH_TABLE *hash;

  oldval = value_cell (var);
  hash = assoc_create (0);
  if (oldval)
    assoc_insert (hash, savestring ("0"), oldval);

  FREE (value_cell (var));
  var_setassoc (var, hash);

   
  var->dynamic_value = (sh_var_value_func_t *)NULL;
  var->assign_func = (sh_var_assign_func_t *)NULL;

  INVALIDATE_EXPORTSTR (var);
  if (exported_p (var))
    array_needs_making++;

  VSETATTR (var, att_assoc);
  if (oldval)
    VUNSETATTR (var, att_invisible);

   
  VUNSETATTR (var, att_array);

   
  VUNSETATTR (var, att_nameref);

  return var;
}

char *
make_array_variable_value (entry, ind, key, value, flags)
     SHELL_VAR *entry;
     arrayind_t ind;
     char *key;
     char *value;
     int flags;
{
  SHELL_VAR *dentry;
  char *newval;

   
  if (flags & ASS_APPEND)
    {
      dentry = (SHELL_VAR *)xmalloc (sizeof (SHELL_VAR));
      dentry->name = savestring (entry->name);
      if (assoc_p (entry))
	newval = assoc_reference (assoc_cell (entry), key);
      else
	newval = array_reference (array_cell (entry), ind);
      if (newval)
	dentry->value = savestring (newval);
      else
	{
	  dentry->value = (char *)xmalloc (1);
	  dentry->value[0] = '\0';
	}
      dentry->exportstr = 0;
      dentry->attributes = entry->attributes & ~(att_array|att_assoc|att_exported);
       
      newval = make_variable_value (dentry, value, flags);	 
      dispose_variable (dentry);
    }
  else
    newval = make_variable_value (entry, value, flags);

  return newval;
}

 
static SHELL_VAR *
bind_assoc_var_internal (entry, hash, key, value, flags)
     SHELL_VAR *entry;
     HASH_TABLE *hash;
     char *key;
     char *value;
     int flags;
{
  char *newval;

   
  newval = make_array_variable_value (entry, 0, key, value, flags);

  if (entry->assign_func)
    (*entry->assign_func) (entry, newval, 0, key);
  else
    assoc_insert (hash, key, newval);

  FREE (newval);

  VUNSETATTR (entry, att_invisible);	 

   
  return (entry);
}

 
static SHELL_VAR *
bind_array_var_internal (entry, ind, key, value, flags)
     SHELL_VAR *entry;
     arrayind_t ind;
     char *key;
     char *value;
     int flags;
{
  char *newval;

  newval = make_array_variable_value (entry, ind, key, value, flags);

  if (entry->assign_func)
    (*entry->assign_func) (entry, newval, ind, key);
  else if (assoc_p (entry))
    assoc_insert (assoc_cell (entry), key, newval);
  else
    array_insert (array_cell (entry), ind, newval);
  FREE (newval);

  VUNSETATTR (entry, att_invisible);	 

   
  return (entry);
}

 
SHELL_VAR *
bind_array_variable (name, ind, value, flags)
     char *name;
     arrayind_t ind;
     char *value;
     int flags;
{
  SHELL_VAR *entry;

  entry = find_shell_variable (name);

  if (entry == (SHELL_VAR *) 0)
    {
       
      entry = find_variable_nameref_for_create (name, 0);
      if (entry == INVALID_NAMEREF_VALUE)
	return ((SHELL_VAR *)0);
      if (entry && nameref_p (entry))
	entry = make_new_array_variable (nameref_cell (entry));
    }
  if (entry == (SHELL_VAR *) 0)
    entry = make_new_array_variable (name);
  else if ((readonly_p (entry) && (flags&ASS_FORCE) == 0) || noassign_p (entry))
    {
      if (readonly_p (entry))
	err_readonly (name);
      return (entry);
    }
  else if (array_p (entry) == 0)
    entry = convert_var_to_array (entry);

   
  return (bind_array_var_internal (entry, ind, 0, value, flags));
}

SHELL_VAR *
bind_array_element (entry, ind, value, flags)
     SHELL_VAR *entry;
     arrayind_t ind;
     char *value;
     int flags;
{
  return (bind_array_var_internal (entry, ind, 0, value, flags));
}
                    
SHELL_VAR *
bind_assoc_variable (entry, name, key, value, flags)
     SHELL_VAR *entry;
     char *name;
     char *key;
     char *value;
     int flags;
{
  if ((readonly_p (entry) && (flags&ASS_FORCE) == 0) || noassign_p (entry))
    {
      if (readonly_p (entry))
	err_readonly (name);
      return (entry);
    }

  return (bind_assoc_var_internal (entry, assoc_cell (entry), key, value, flags));
}

inline void
init_eltstate (array_eltstate_t *estatep)
{
  if (estatep)
    {
      estatep->type = ARRAY_INVALID;
      estatep->subtype = 0;
      estatep->key = estatep->value = 0;
      estatep->ind = INTMAX_MIN;
    }
}

inline void
flush_eltstate (array_eltstate_t *estatep)
{
  if (estatep)
    FREE (estatep->key);
}

 
SHELL_VAR *
assign_array_element (name, value, flags, estatep)
     char *name, *value;
     int flags;
     array_eltstate_t *estatep;
{
  char *sub, *vname;
  int sublen, isassoc, avflags;
  SHELL_VAR *entry;

  avflags = 0;
  if (flags & ASS_NOEXPAND)
    avflags |= AV_NOEXPAND;
  if (flags & ASS_ONEWORD)
    avflags |= AV_ONEWORD;
  vname = array_variable_name (name, avflags, &sub, &sublen);

  if (vname == 0)
    return ((SHELL_VAR *)NULL);

  entry = find_variable (vname);
  isassoc = entry && assoc_p (entry);

   
  if (((isassoc == 0 || (flags & (ASS_NOEXPAND|ASS_ALLOWALLSUB)) == 0) &&
	(ALL_ELEMENT_SUB (sub[0]) && sub[1] == ']')) ||
      (sublen <= 1) ||
      (sub[sublen] != '\0'))		 
    {
      free (vname);
      err_badarraysub (name);
      return ((SHELL_VAR *)NULL);
    }

  entry = assign_array_element_internal (entry, name, vname, sub, sublen, value, flags, estatep);

#if ARRAY_EXPORT
  if (entry && exported_p (entry))
    {
      INVALIDATE_EXPORTSTR (entry);
      array_needs_making = 1;
    }
#endif

  free (vname);
  return entry;
}

static SHELL_VAR *
assign_array_element_internal (entry, name, vname, sub, sublen, value, flags, estatep)
     SHELL_VAR *entry;
     char *name;		 
     char *vname;
     char *sub;
     int sublen;
     char *value;
     int flags;
     array_eltstate_t *estatep;
{
  char *akey, *nkey;
  arrayind_t ind;
  char *newval;

   

  if (entry && assoc_p (entry))
    {
      sub[sublen-1] = '\0';
      if ((flags & ASS_NOEXPAND) == 0)
	akey = expand_subscript_string (sub, 0);	 
      else
	akey = savestring (sub);
      sub[sublen-1] = ']';
      if (akey == 0 || *akey == 0)
	{
	  err_badarraysub (name);
	  FREE (akey);
	  return ((SHELL_VAR *)NULL);
	}
      if (estatep)
	nkey = savestring (akey);	 
      entry = bind_assoc_variable (entry, vname, akey, value, flags);
      if (estatep)
	{
	  estatep->type = ARRAY_ASSOC;
	  estatep->key = nkey;
	  estatep->value = entry ? assoc_reference (assoc_cell (entry), nkey) : 0;
	}
    }
  else
    {
      ind = array_expand_index (entry, sub, sublen, 0);
       
      if (entry && ind < 0)
	ind = (array_p (entry) ? array_max_index (array_cell (entry)) : 0) + 1 + ind;
      if (ind < 0)
	{
	  err_badarraysub (name);
	  return ((SHELL_VAR *)NULL);
	}
      entry = bind_array_variable (vname, ind, value, flags);
      if (estatep)
	{
	  estatep->type = ARRAY_INDEXED;
	  estatep->ind = ind;
	  estatep->value = entry ? array_reference (array_cell (entry), ind) : 0;
	}
    }

  return (entry);
}

 
SHELL_VAR *
find_or_make_array_variable (name, flags)
     char *name;
     int flags;
{
  SHELL_VAR *var;

  var = find_variable (name);
  if (var == 0)
    {
       
      var = find_variable_last_nameref (name, 1);
      if (var && nameref_p (var) && invisible_p (var))
	{
	  internal_warning (_("%s: removing nameref attribute"), name);
	  VUNSETATTR (var, att_nameref);
	}
      if (var && nameref_p (var))
	{
	  if (valid_nameref_value (nameref_cell (var), 2) == 0)
	    {
	      sh_invalidid (nameref_cell (var));
	      return ((SHELL_VAR *)NULL);
	    }
	  var = (flags & 2) ? make_new_assoc_variable (nameref_cell (var)) : make_new_array_variable (nameref_cell (var));
	}
    }

  if (var == 0)
    var = (flags & 2) ? make_new_assoc_variable (name) : make_new_array_variable (name);
  else if ((flags & 1) && (readonly_p (var) || noassign_p (var)))
    {
      if (readonly_p (var))
	err_readonly (name);
      return ((SHELL_VAR *)NULL);
    }
  else if ((flags & 2) && array_p (var))
    {
      set_exit_status (EXECUTION_FAILURE);
      report_error (_("%s: cannot convert indexed to associative array"), name);
      return ((SHELL_VAR *)NULL);
    }
  else if (flags & 2)
    var = assoc_p (var) ? var : convert_var_to_assoc (var);
  else if (array_p (var) == 0 && assoc_p (var) == 0)
    var = convert_var_to_array (var);

  return (var);
}
  
 
SHELL_VAR *
assign_array_from_string (name, value, flags)
     char *name, *value;
     int flags;
{
  SHELL_VAR *var;
  int vflags;

  vflags = 1;
  if (flags & ASS_MKASSOC)
    vflags |= 2;

  var = find_or_make_array_variable (name, vflags);
  if (var == 0)
    return ((SHELL_VAR *)NULL);

  return (assign_array_var_from_string (var, value, flags));
}

 
SHELL_VAR *
assign_array_var_from_word_list (var, list, flags)
     SHELL_VAR *var;
     WORD_LIST *list;
     int flags;
{
  register arrayind_t i;
  register WORD_LIST *l;
  ARRAY *a;

  a = array_cell (var);
  i = (flags & ASS_APPEND) ? array_max_index (a) + 1 : 0;

  for (l = list; l; l = l->next, i++)
    bind_array_var_internal (var, i, 0, l->word->word, flags & ~ASS_APPEND);

  VUNSETATTR (var, att_invisible);	 

  return var;
}

WORD_LIST *
expand_compound_array_assignment (var, value, flags)
     SHELL_VAR *var;
     char *value;
     int flags;
{
  WORD_LIST *list, *nlist;
  char *val;
  int ni;

   
  if (*value == '(')	 
    {
      ni = 1;
      val = extract_array_assignment_list (value, &ni);
      if (val == 0)
	return (WORD_LIST *)NULL;
    }
  else
    val = value;

   
   
   
  list = parse_string_to_word_list (val, 1, "array assign");

   
   
  for (nlist = list; nlist; nlist = nlist->next)
    if ((nlist->word->flags & W_QUOTED) == 0)
      remove_quoted_escapes (nlist->word->word);

   
  if (var && assoc_p (var))
    {
      if (val != value)
	free (val);
      return list;
    }

   
  if (list)
    quote_array_assignment_chars (list);

   
  nlist = list ? expand_words_no_vars (list) : (WORD_LIST *)NULL;

  dispose_words (list);

  if (val != value)
    free (val);

  return nlist;
}

#if ASSOC_KVPAIR_ASSIGNMENT
static void
assign_assoc_from_kvlist (var, nlist, h, flags)
     SHELL_VAR *var;
     WORD_LIST *nlist;
     HASH_TABLE *h;
     int flags;
{
  WORD_LIST *list;
  char *akey, *aval, *k, *v;

  for (list = nlist; list; list = list->next)
    {
      k = list->word->word;
      v = list->next ? list->next->word->word : 0;

      if (list->next)
        list = list->next;

      akey = expand_subscript_string (k, 0);
      if (akey == 0 || *akey == 0)
	{
	  err_badarraysub (k);
	  FREE (akey);
	  continue;
	}	      

      aval = expand_subscript_string (v, 0);
      if (aval == 0)
	{
	  aval = (char *)xmalloc (1);
	  aval[0] = '\0';	 
	}

      bind_assoc_var_internal (var, h, akey, aval, flags);
      free (aval);
    }
}

  
int
kvpair_assignment_p (l)
     WORD_LIST *l;
{
  return (l && (l->word->flags & W_ASSIGNMENT) == 0 && l->word->word[0] != '[');	 
}

char *
expand_and_quote_kvpair_word (w)
     char *w;
{
  char *r, *s, *t;

  t = w ? expand_subscript_string (w, 0) : 0;
  s = (t && strchr (t, CTLESC)) ? quote_escapes (t) : t;
  r = sh_single_quote (s ? s : "");
  if (s != t)
    free (s);
  free (t);
  return r;
}
#endif
     
 
void
assign_compound_array_list (var, nlist, flags)
     SHELL_VAR *var;
     WORD_LIST *nlist;
     int flags;
{
  ARRAY *a;
  HASH_TABLE *h, *nhash;
  WORD_LIST *list;
  char *w, *val, *nval, *savecmd;
  int len, iflags, free_val;
  arrayind_t ind, last_ind;
  char *akey;

  a = (var && array_p (var)) ? array_cell (var) : (ARRAY *)0;
  nhash = h = (var && assoc_p (var)) ? assoc_cell (var) : (HASH_TABLE *)0;

  akey = (char *)0;
  ind = 0;

   
  if ((flags & ASS_APPEND) == 0)
    {
      if (a && array_p (var))
	array_flush (a);
      else if (h && assoc_p (var))
	nhash = assoc_create (h->nbuckets);
    }

  last_ind = (a && (flags & ASS_APPEND)) ? array_max_index (a) + 1 : 0;

#if ASSOC_KVPAIR_ASSIGNMENT
  if (assoc_p (var) && kvpair_assignment_p (nlist))
    {
      iflags = flags & ~ASS_APPEND;
      assign_assoc_from_kvlist (var, nlist, nhash, iflags);
      if (nhash && nhash != h)
	{
	  h = assoc_cell (var);
	  var_setassoc (var, nhash);
	  assoc_dispose (h);
	}
      return;
    }
#endif

  for (list = nlist; list; list = list->next)
    {
       
      iflags = flags & ~ASS_APPEND;
      w = list->word->word;

       
      if ((list->word->flags & W_ASSIGNMENT) && w[0] == '[')
	{
	   
	  len = skipsubscript (w, 0, 0);

	   
 	  if (w[len] != ']' || (w[len+1] != '=' && (w[len+1] != '+' || w[len+2] != '=')))
	    {
	      if (assoc_p (var))
		{
		  err_badarraysub (w);
		  continue;
		}
	      nval = make_variable_value (var, w, flags);
	      if (var->assign_func)
		(*var->assign_func) (var, nval, last_ind, 0);
	      else
		array_insert (a, last_ind, nval);
	      FREE (nval);
	      last_ind++;
	      continue;
	    }

	  if (len == 1)
	    {
	      err_badarraysub (w);
	      continue;
	    }

	  if (ALL_ELEMENT_SUB (w[1]) && len == 2 && array_p (var))
	    {
	      set_exit_status (EXECUTION_FAILURE);
	      report_error (_("%s: cannot assign to non-numeric index"), w);
	      continue;
	    }

	  if (array_p (var))
	    {
	      ind = array_expand_index (var, w + 1, len, 0);
	       
	      if (ind < 0)
		ind = array_max_index (array_cell (var)) + 1 + ind;
	      if (ind < 0)
		{
		  err_badarraysub (w);
		  continue;
		}

	      last_ind = ind;
	    }
	  else if (assoc_p (var))
	    {
	       
	      w[len] = '\0';	 
	      akey = expand_subscript_string (w+1, 0);
	      w[len] = ']';
	       
	      if (akey == 0 || *akey == 0)
		{
		  err_badarraysub (w);
		  FREE (akey);
		  continue;
		}
	    }

	   
	  if (w[len + 1] == '+' && w[len + 2] == '=')
	    {
	      iflags |= ASS_APPEND;
	      val = w + len + 3;
	    }
	  else
	    val = w + len + 2;	    
	}
      else if (assoc_p (var))
	{
	  set_exit_status (EXECUTION_FAILURE);
	  report_error (_("%s: %s: must use subscript when assigning associative array"), var->name, w);
	  continue;
	}
      else		 
	{
	  ind = last_ind;
	  val = w;
	}

      free_val = 0;
       
      if (assoc_p (var))
	{
	  val = expand_subscript_string (val, 0);
	  if (val == 0)
	    {
	      val = (char *)xmalloc (1);
	      val[0] = '\0';	 
	    }
	  free_val = 1;
	}

      savecmd = this_command_name;
      if (integer_p (var))
	this_command_name = (char *)NULL;	 
      if (assoc_p (var))
	bind_assoc_var_internal (var, nhash, akey, val, iflags);
      else
	bind_array_var_internal (var, ind, akey, val, iflags);
      last_ind++;
      this_command_name = savecmd;

      if (free_val)
	free (val);
    }

  if (assoc_p (var) && nhash && nhash != h)
    {
      h = assoc_cell (var);
      var_setassoc (var, nhash);
      assoc_dispose (h);
    }
}

 
SHELL_VAR *
assign_array_var_from_string (var, value, flags)
     SHELL_VAR *var;
     char *value;
     int flags;
{
  WORD_LIST *nlist;

  if (value == 0)
    return var;

  nlist = expand_compound_array_assignment (var, value, flags);
  assign_compound_array_list (var, nlist, flags);

  if (nlist)
    dispose_words (nlist);

  if (var)
    VUNSETATTR (var, att_invisible);	 

  return (var);
}

 
static char *
quote_assign (string)
     const char *string;
{
  size_t slen;
  int saw_eq;
  char *temp, *t, *subs;
  const char *s, *send;
  int ss, se;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;

  t = temp = (char *)xmalloc (slen * 2 + 1);
  saw_eq = 0;
  for (s = string; *s; )
    {
      if (*s == '=')
	saw_eq = 1;
      if (saw_eq == 0 && *s == '[')		 
	{
	  ss = s - string;
	  se = skipsubscript (string, ss, 0);
	  subs = substring (s, ss, se);
	  *t++ = '\\';
	  strcpy (t, subs);
	  t += se - ss;
	  *t++ = '\\';
	  *t++ = ']';
	  s += se + 1;
	  free (subs);
	  continue;
	}
      if (saw_eq == 0 && (glob_char_p (s) || isifs (*s)))
	*t++ = '\\';

      COPY_CHAR_P (t, s, send);
    }
  *t = '\0';
  return temp;
}

 
static char *
quote_compound_array_word (w, type)
     char *w;
     int type;
{
  char *nword, *sub, *value, *t;
  int ind, wlen, i;

  if (w[0] != LBRACK)
    return (sh_single_quote (w));	 
  ind = skipsubscript (w, 0, 0);
  if (w[ind] != RBRACK)
    return (sh_single_quote (w));	 

  wlen = strlen (w);
  w[ind] = '\0';
  t = (strchr (w+1, CTLESC)) ? quote_escapes (w+1) : w+1;
  sub = sh_single_quote (t);
  if (t != w+1)
   free (t);
  w[ind] = RBRACK;

  nword = xmalloc (wlen * 4 + 5);	 
  nword[0] = LBRACK;
  i = STRLEN (sub);
  memcpy (nword+1, sub, i);
  free (sub);
  i++;				 
  nword[i++] = w[ind++];	 
  if (w[ind] == '+')
    nword[i++] = w[ind++];
  nword[i++] = w[ind++];
  t = (strchr (w+ind, CTLESC)) ? quote_escapes (w+ind) : w+ind;
  value = sh_single_quote (t);
  if (t != w+ind)
   free (t);
  strcpy (nword + i, value);

  return nword;
}

 
char *
expand_and_quote_assoc_word (w, type)
     char *w;
     int type;
{
  char *nword, *key, *value, *s, *t;
  int ind, wlen, i;

  if (w[0] != LBRACK)
    return (sh_single_quote (w));	 
  ind = skipsubscript (w, 0, 0);
  if (w[ind] != RBRACK)
    return (sh_single_quote (w));	 

  w[ind] = '\0';
  t = expand_subscript_string (w+1, 0);
  s = (t && strchr (t, CTLESC)) ? quote_escapes (t) : t;
  key = sh_single_quote (s ? s : "");
  if (s != t)
    free (s);
  w[ind] = RBRACK;
  free (t);

  wlen = STRLEN (key);
  nword = xmalloc (wlen + 5);
  nword[0] = LBRACK;
  memcpy (nword+1, key, wlen);
  i = wlen + 1;			 

  nword[i++] = w[ind++];	 
  if (w[ind] == '+')
    nword[i++] = w[ind++];
  nword[i++] = w[ind++];

  t = expand_subscript_string (w+ind, 0);
  s = (t && strchr (t, CTLESC)) ? quote_escapes (t) : t;
  value = sh_single_quote (s ? s : "");
  if (s != t)
    free (s);
  free (t);
  nword = xrealloc (nword, wlen + 5 + STRLEN (value));
  strcpy (nword + i, value);

  free (key);
  free (value);

  return nword;
}

 
void
quote_compound_array_list (list, type)
     WORD_LIST *list;
     int type;
{
  char *s, *t;
  WORD_LIST *l;

  for (l = list; l; l = l->next)
    {
      if (l->word == 0 || l->word->word == 0)
	continue;	 
      if ((l->word->flags & W_ASSIGNMENT) == 0)
	{
	  s = (strchr (l->word->word, CTLESC)) ? quote_escapes (l->word->word) : l->word->word;
	  t = sh_single_quote (s);
	  if (s != l->word->word)
	    free (s);
	}
      else 
	t = quote_compound_array_word (l->word->word, type);
      free (l->word->word);
      l->word->word = t;
    }
}

 
static void
quote_array_assignment_chars (list)
     WORD_LIST *list;
{
  char *nword;
  WORD_LIST *l;

  for (l = list; l; l = l->next)
    {
      if (l->word == 0 || l->word->word == 0 || l->word->word[0] == '\0')
	continue;	 
       
      if ((l->word->flags & W_ASSIGNMENT) == 0)
	continue;
      if (l->word->word[0] != '[' || mbschr (l->word->word, '=') == 0)  
	continue;

      nword = quote_assign (l->word->word);
      free (l->word->word);
      l->word->word = nword;
      l->word->flags |= W_NOGLOB;	 
    }
}

 

 
 
int
unbind_array_element (var, sub, flags)
     SHELL_VAR *var;
     char *sub;
     int flags;
{
  arrayind_t ind;
  char *akey;
  ARRAY_ELEMENT *ae;

   

  if (ALL_ELEMENT_SUB (sub[0]) && sub[1] == 0)
    {
      if (array_p (var) || assoc_p (var))
	{
	  if (flags & VA_ALLOWALL)
	    {
	      unbind_variable (var->name);	 
	      return (0);
	    }
	   
	}
      else
	return -2;	 
    }

  if (assoc_p (var))
    {
      akey = (flags & VA_NOEXPAND) ? sub : expand_subscript_string (sub, 0);
      if (akey == 0 || *akey == 0)
	{
	  builtin_error ("[%s]: %s", sub, _(bash_badsub_errmsg));
	  FREE (akey);
	  return -1;
	}
      assoc_remove (assoc_cell (var), akey);
      if (akey != sub)
	free (akey);
    }
  else if (array_p (var))
    {
      if (ALL_ELEMENT_SUB (sub[0]) && sub[1] == 0)
	{
	   
	   
	  if (shell_compatibility_level <= 51)
	    {
	      unbind_variable (name_cell (var));
	      return 0;
	    }
	  else  
	    {
	      array_flush (array_cell (var));
	      return 0;
	    }
	   
	}
      ind = array_expand_index (var, sub, strlen (sub) + 1, 0);
       
      if (ind < 0)
	ind = array_max_index (array_cell (var)) + 1 + ind;
      if (ind < 0)
	{
	  builtin_error ("[%s]: %s", sub, _(bash_badsub_errmsg));
	  return -1;
	}
      ae = array_remove (array_cell (var), ind);
      if (ae)
	array_dispose_element (ae);
    }
  else	 
    {
      akey = this_command_name;
      ind = array_expand_index (var, sub, strlen (sub) + 1, 0);
      this_command_name = akey;
      if (ind == 0)
	{
	  unbind_variable (var->name);
	  return (0);
	}
      else
	return -2;	 
    }

  return 0;
}

 
void
print_array_assignment (var, quoted)
     SHELL_VAR *var;
     int quoted;
{
  char *vstr;

  vstr = array_to_assign (array_cell (var), quoted);

  if (vstr == 0)
    printf ("%s=%s\n", var->name, quoted ? "'()'" : "()");
  else
    {
      printf ("%s=%s\n", var->name, vstr);
      free (vstr);
    }
}

 
void
print_assoc_assignment (var, quoted)
     SHELL_VAR *var;
     int quoted;
{
  char *vstr;

  vstr = assoc_to_assign (assoc_cell (var), quoted);

  if (vstr == 0)
    printf ("%s=%s\n", var->name, quoted ? "'()'" : "()");
  else
    {
      printf ("%s=%s\n", var->name, vstr);
      free (vstr);
    }
}

 
 
 
 
 

 

 

 
 
int
tokenize_array_reference (name, flags, subp)
     char *name;
     int flags;
     char **subp;
{
  char *t;
  int r, len, isassoc, ssflags;
  SHELL_VAR *entry;

  t = mbschr (name, '[');	 
  isassoc = 0;
  if (t)
    {
      *t = '\0';
      r = legal_identifier (name);
      if (flags & VA_NOEXPAND)	 
	isassoc = (entry = find_variable (name)) && assoc_p (entry);      
      *t = '[';
      if (r == 0)
	return 0;

      ssflags = 0;
      if (isassoc && ((flags & (VA_NOEXPAND|VA_ONEWORD)) == (VA_NOEXPAND|VA_ONEWORD)))
	len = strlen (t) - 1;
      else if (isassoc)
	{
	  if (flags & VA_NOEXPAND)
	    ssflags |= 1;
	  len = skipsubscript (t, 0, ssflags);
	}
      else
	 
	len = skipsubscript (t, 0, 0);		 

      if (t[len] != ']' || len == 1 || t[len+1] != '\0')
	return 0;

#if 0
       
      for (r = 1; r < len; r++)
	if (whitespace (t[r]) == 0)
	  break;
      if (r == len)
	return 0;  
#endif

      if (subp)
	{
	  t[0] = t[len] = '\0';
	  *subp = t + 1;
	}

       
      return 1;
    }
  return 0;
}

 

 
int
valid_array_reference (name, flags)
     const char *name;
     int flags;
{
  return tokenize_array_reference ((char *)name, flags, (char **)NULL);
}

 
arrayind_t
array_expand_index (var, s, len, flags)
     SHELL_VAR *var;
     char *s;
     int len;
     int flags;
{
  char *exp, *t, *savecmd;
  int expok, eflag;
  arrayind_t val;

  exp = (char *)xmalloc (len);
  strncpy (exp, s, len - 1);
  exp[len - 1] = '\0';
#if 0	 
  if ((flags & AV_NOEXPAND) == 0)
    t = expand_arith_string (exp, Q_DOUBLE_QUOTES|Q_ARITH|Q_ARRAYSUB);	 
  else
    t = exp;
#else
  t = expand_arith_string (exp, Q_DOUBLE_QUOTES|Q_ARITH|Q_ARRAYSUB);	 
#endif
  savecmd = this_command_name;
  this_command_name = (char *)NULL;
  eflag = (shell_compatibility_level > 51) ? 0 : EXP_EXPANDED;
  val = evalexp (t, eflag, &expok);	 
  this_command_name = savecmd;
  if (t != exp)
    free (t);
  free (exp);
  if (expok == 0)
    {
      set_exit_status (EXECUTION_FAILURE);

      if (no_longjmp_on_fatal_error)
	return 0;
      top_level_cleanup ();      
      jump_to_top_level (DISCARD);
    }
  return val;
}

 
char *
array_variable_name (s, flags, subp, lenp)
     const char *s;
     int flags;
     char **subp;
     int *lenp;
{
  char *t, *ret;
  int ind, ni, ssflags;

  t = mbschr (s, '[');
  if (t == 0)
    {
      if (subp)
      	*subp = t;
      if (lenp)
	*lenp = 0;
      return ((char *)NULL);
    }
  ind = t - s;
  if ((flags & (AV_NOEXPAND|AV_ONEWORD)) == (AV_NOEXPAND|AV_ONEWORD))
    ni = strlen (s) - 1;
  else
    {
      ssflags = 0;
      if (flags & AV_NOEXPAND)
	ssflags |= 1;
      ni = skipsubscript (s, ind, ssflags);
    }
  if (ni <= ind + 1 || s[ni] != ']')
    {
      err_badarraysub (s);
      if (subp)
      	*subp = t;
      if (lenp)
	*lenp = 0;
      return ((char *)NULL);
    }

  *t = '\0';
  ret = savestring (s);
  *t++ = '[';		 

  if (subp)
    *subp = t;
  if (lenp)
    *lenp = ni - ind;

  return ret;
}

 
SHELL_VAR *
array_variable_part (s, flags, subp, lenp)
     const char *s;
     int flags;
     char **subp;
     int *lenp;
{
  char *t;
  SHELL_VAR *var;

  t = array_variable_name (s, flags, subp, lenp);
  if (t == 0)
    return ((SHELL_VAR *)NULL);
  var = find_variable (t);		 

  free (t);
  return var;	 
}

#define INDEX_ERROR() \
  do \
    { \
      if (var) \
	err_badarraysub (var->name); \
      else \
	{ \
	  t[-1] = '\0'; \
	  err_badarraysub (s); \
	  t[-1] = '[';	 \
	} \
      return ((char *)NULL); \
    } \
  while (0)

 
static char *
array_value_internal (s, quoted, flags, estatep)
     const char *s;
     int quoted, flags;
     array_eltstate_t *estatep;
{
  int len, isassoc, subtype;
  arrayind_t ind;
  char *akey;
  char *retval, *t, *temp;
  WORD_LIST *l;
  SHELL_VAR *var;

  var = array_variable_part (s, flags, &t, &len);	 

   
#if 0
  if (var == 0)
    return (char *)NULL;
#endif

  if (len == 0)
    return ((char *)NULL);	 

  isassoc = var && assoc_p (var);
   
  akey = 0;
  subtype = 0;
  if (estatep)
    estatep->value = (char *)NULL;

   
  if ((isassoc == 0 || (flags & AV_ATSTARKEYS) == 0) && ALL_ELEMENT_SUB (t[0]) && t[1] == ']')
    {
      if (estatep)
	estatep->subtype = (t[0] == '*') ? 1 : 2;
      if ((flags & AV_ALLOWALL) == 0)
	{
	  err_badarraysub (s);
	  return ((char *)NULL);
	}
      else if (var == 0 || value_cell (var) == 0)
	return ((char *)NULL);
      else if (invisible_p (var))
	return ((char *)NULL);
      else if (array_p (var) == 0 && assoc_p (var) == 0)
        {
          if (estatep)
	    estatep->type = ARRAY_SCALAR;
	  l = add_string_to_list (value_cell (var), (WORD_LIST *)NULL);
        }
      else if (assoc_p (var))
	{
	  if (estatep)
	    estatep->type = ARRAY_ASSOC;
	  l = assoc_to_word_list (assoc_cell (var));
	  if (l == (WORD_LIST *)NULL)
	    return ((char *)NULL);
	}
      else
	{
	  if (estatep)
	    estatep->type = ARRAY_INDEXED;
	  l = array_to_word_list (array_cell (var));
	  if (l == (WORD_LIST *)NULL)
	    return ((char *) NULL);
	}

       
      if (t[0] == '*' && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	{
	  temp = string_list_dollar_star (l, quoted, (flags & AV_ASSIGNRHS) ? PF_ASSIGNRHS : 0);
	  retval = quote_string (temp);
	  free (temp);
	}
      else	 
	retval = string_list_dollar_at (l, quoted, (flags & AV_ASSIGNRHS) ? PF_ASSIGNRHS : 0);

      dispose_words (l);
    }
  else
    {
      if (estatep)
	estatep->subtype = 0;
      if (var == 0 || array_p (var) || assoc_p (var) == 0)
	{
	  if ((flags & AV_USEIND) == 0 || estatep == 0)
	    {
	      ind = array_expand_index (var, t, len, flags);
	      if (ind < 0)
		{
		   
		  if (var && array_p (var))
		    ind = array_max_index (array_cell (var)) + 1 + ind;
		  if (ind < 0)
		    INDEX_ERROR();
		}
	      if (estatep)
		estatep->ind = ind;
	    }
	  else if (estatep && (flags & AV_USEIND))
	    ind = estatep->ind;
	  if (estatep && var)
	    estatep->type = array_p (var) ? ARRAY_INDEXED : ARRAY_SCALAR;
	}
      else if (assoc_p (var))
	{
	  t[len - 1] = '\0';
	  if (estatep)
	    estatep->type = ARRAY_ASSOC;
	  if ((flags & AV_USEIND) && estatep && estatep->key)
	    akey = savestring (estatep->key);
	  else if ((flags & AV_NOEXPAND) == 0)
	    akey = expand_subscript_string (t, 0);	 
	  else
	    akey = savestring (t);
	  t[len - 1] = ']';
	  if (akey == 0 || *akey == 0)
	    {
	      FREE (akey);
	      INDEX_ERROR();
	    }
	}

      if (var == 0 || value_cell (var) == 0)
	{
	  FREE (akey);
	  return ((char *)NULL);
	}
      else if (invisible_p (var))
	{
	  FREE (akey);
	  return ((char *)NULL);
	}
      if (array_p (var) == 0 && assoc_p (var) == 0)
	retval = (ind == 0) ? value_cell (var) : (char *)NULL;
      else if (assoc_p (var))
        {
	  retval = assoc_reference (assoc_cell (var), akey);
	  if (estatep && estatep->key && (flags & AV_USEIND))
	    free (akey);		 
	  else if (estatep)
	    estatep->key = akey;	 
	  else				 
	    free (akey);
        }
      else
	retval = array_reference (array_cell (var), ind);

      if (estatep)
	estatep->value = retval;
    }

  return retval;
}

 
char *
array_value (s, quoted, flags, estatep)
     const char *s;
     int quoted, flags;
     array_eltstate_t *estatep;
{
  char *retval;

  retval = array_value_internal (s, quoted, flags|AV_ALLOWALL, estatep);
  return retval;
}

 
char *
get_array_value (s, flags, estatep)
     const char *s;
     int flags;
     array_eltstate_t *estatep;
{
  char *retval;

  retval = array_value_internal (s, 0, flags, estatep);
  return retval;
}

char *
array_keys (s, quoted, pflags)
     char *s;
     int quoted, pflags;
{
  int len;
  char *retval, *t, *temp;
  WORD_LIST *l;
  SHELL_VAR *var;

  var = array_variable_part (s, 0, &t, &len);

   
  if (var == 0 || ALL_ELEMENT_SUB (t[0]) == 0 || t[1] != ']')
    return (char *)NULL;

  if (var_isset (var) == 0 || invisible_p (var))
    return (char *)NULL;

  if (array_p (var) == 0 && assoc_p (var) == 0)
    l = add_string_to_list ("0", (WORD_LIST *)NULL);
  else if (assoc_p (var))
    l = assoc_keys_to_word_list (assoc_cell (var));
  else
    l = array_keys_to_word_list (array_cell (var));
  if (l == (WORD_LIST *)NULL)
    return ((char *) NULL);

  retval = string_list_pos_params (t[0], l, quoted, pflags);

  dispose_words (l);
  return retval;
}
#endif  
