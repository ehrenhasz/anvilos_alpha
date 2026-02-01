 

 

#include "config.h"

#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include "bashansi.h"
#include "bashintl.h"

#define NEED_XTRACE_SET_DECL

#include "shell.h"
#include "flags.h"
#include <y.tab.h>	 
#include "input.h"

#include "shmbutil.h"

#include "builtins/common.h"

#if !HAVE_DECL_PRINTF
extern int printf PARAMS((const char *, ...));	 
#endif

static int indentation;
static int indentation_amount = 4;

#if defined (PREFER_STDARG)
typedef void PFUNC PARAMS((const char *, ...));

static void cprintf PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));
static void xprintf PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));
#else
#define PFUNC VFunction
static void cprintf ();
static void xprintf ();
#endif

static void reset_locals PARAMS((void));
static void newline PARAMS((char *));
static void indent PARAMS((int));
static void semicolon PARAMS((void));
static void the_printed_command_resize PARAMS((int));

static void make_command_string_internal PARAMS((COMMAND *));
static void _print_word_list PARAMS((WORD_LIST *, char *, PFUNC *));
static void command_print_word_list PARAMS((WORD_LIST *, char *));
static void print_case_clauses PARAMS((PATTERN_LIST *));
static void print_redirection_list PARAMS((REDIRECT *));
static void print_redirection PARAMS((REDIRECT *));
static void print_heredoc_header PARAMS((REDIRECT *));
static void print_heredoc_body PARAMS((REDIRECT *));
static void print_heredocs PARAMS((REDIRECT *));
static void print_heredoc_bodies PARAMS((REDIRECT *));
static void print_deferred_heredocs PARAMS((const char *));

static void print_for_command PARAMS((FOR_COM *));
#if defined (ARITH_FOR_COMMAND)
static void print_arith_for_command PARAMS((ARITH_FOR_COM *));
#endif
#if defined (SELECT_COMMAND)
static void print_select_command PARAMS((SELECT_COM *));
#endif
static void print_group_command PARAMS((GROUP_COM *));
static void print_case_command PARAMS((CASE_COM *));
static void print_while_command PARAMS((WHILE_COM *));
static void print_until_command PARAMS((WHILE_COM *));
static void print_until_or_while PARAMS((WHILE_COM *, char *));
static void print_if_command PARAMS((IF_COM *));
#if defined (COND_COMMAND)
static void print_cond_node PARAMS((COND_COM *));
#endif
static void print_function_def PARAMS((FUNCTION_DEF *));

#define PRINTED_COMMAND_INITIAL_SIZE 64
#define PRINTED_COMMAND_GROW_SIZE 128

char *the_printed_command = (char *)NULL;
int the_printed_command_size = 0;
int command_string_index = 0;

int xtrace_fd = -1;
FILE *xtrace_fp = 0;

#define CHECK_XTRACE_FP	xtrace_fp = (xtrace_fp ? xtrace_fp : stderr)

 
#define EXPCHAR(c) ((c) == '{' || (c) == '~' || (c) == '$' || (c) == '`')

#define PRINT_DEFERRED_HEREDOCS(x) \
  do { \
    if (deferred_heredocs) \
      print_deferred_heredocs (x); \
  } while (0)

 
static int inside_function_def;
static int skip_this_indent;
static int was_heredoc;
static int printing_connection;
static int printing_comsub;
static REDIRECT *deferred_heredocs;

 
static int group_command_nesting;

 
static char *indirection_string = 0;
static int indirection_stringsiz = 0;

 
void
print_command (command)
     COMMAND *command;
{
  command_string_index = 0;
  printf ("%s", make_command_string (command));
}

 
char *
make_command_string (command)
     COMMAND *command;
{
  command_string_index = was_heredoc = 0;
  deferred_heredocs = 0;
  make_command_string_internal (command);
  return (the_printed_command);
}

 
char *
print_comsub (command)
     COMMAND *command;
{
  char *ret;

  printing_comsub++;
  ret = make_command_string (command);
  printing_comsub--;
  return ret;
}

 
static void
make_command_string_internal (command)
     COMMAND *command;
{
  char s[3];

  if (command == 0)
    cprintf ("");
  else
    {
      if (skip_this_indent)
	skip_this_indent--;
      else
	indent (indentation);

      if (command->flags & CMD_TIME_PIPELINE)
	{
	  cprintf ("time ");
	  if (command->flags & CMD_TIME_POSIX)
	    cprintf ("-p ");
	}

      if (command->flags & CMD_INVERT_RETURN)
	cprintf ("! ");

      switch (command->type)
	{
	case cm_for:
	  print_for_command (command->value.For);
	  break;

#if defined (ARITH_FOR_COMMAND)
	case cm_arith_for:
	  print_arith_for_command (command->value.ArithFor);
	  break;
#endif

#if defined (SELECT_COMMAND)
	case cm_select:
	  print_select_command (command->value.Select);
	  break;
#endif

	case cm_case:
	  print_case_command (command->value.Case);
	  break;

	case cm_while:
	  print_while_command (command->value.While);
	  break;

	case cm_until:
	  print_until_command (command->value.While);
	  break;

	case cm_if:
	  print_if_command (command->value.If);
	  break;

#if defined (DPAREN_ARITHMETIC)
	case cm_arith:
	  print_arith_command (command->value.Arith->exp);
	  break;
#endif

#if defined (COND_COMMAND)
	case cm_cond:
	  print_cond_command (command->value.Cond);
	  break;
#endif

	case cm_simple:
	  print_simple_command (command->value.Simple);
	  break;

	case cm_connection:

	  skip_this_indent++;
	  printing_connection++;
	  make_command_string_internal (command->value.Connection->first);

	  switch (command->value.Connection->connector)
	    {
	    case '&':
	    case '|':
	      {
		char c = command->value.Connection->connector;

		s[0] = ' ';
		s[1] = c;
		s[2] = '\0';
		
		print_deferred_heredocs (s);

		if (c != '&' || command->value.Connection->second)
		  {
		    cprintf (" ");
		    skip_this_indent++;
		  }
	      }
	      break;

	    case AND_AND:
	      print_deferred_heredocs (" && ");
	      if (command->value.Connection->second)
		skip_this_indent++;
	      break;

	    case OR_OR:
	      print_deferred_heredocs (" || ");
	      if (command->value.Connection->second)
		skip_this_indent++;
	      break;

	    case ';':
	    case '\n':				 
	      {
		char c = command->value.Connection->connector;
		int was_newline;

		s[0] = printing_comsub ? c : ';';
		s[1] = '\0';

		was_newline = deferred_heredocs == 0 && was_heredoc == 0 && c == '\n';
		if (deferred_heredocs == 0)
		  {
		    if (was_heredoc == 0)
		      cprintf ("%s", s);	 
		    else
		      was_heredoc = 0;
		  }
		else
		   
		  print_deferred_heredocs (inside_function_def ? "" : ";");

		if (inside_function_def)
		  cprintf ("\n");
		else if (printing_comsub && c == '\n' && was_newline == 0)
		  cprintf ("\n");	 
		else
		  {
		    if (c == ';')
		      cprintf (" ");
		    if (command->value.Connection->second)
		      skip_this_indent++;
		  }
		break;
	      }

	    default:
	      cprintf (_("print_command: bad connector `%d'"),
		       command->value.Connection->connector);
	      break;
	    }

	  make_command_string_internal (command->value.Connection->second);
	  PRINT_DEFERRED_HEREDOCS ("");
	  printing_connection--;	  	  
	  break;

	case cm_function_def:
	  print_function_def (command->value.Function_def);
	  break;

	case cm_group:
	  print_group_command (command->value.Group);
	  break;

	case cm_subshell:
	  cprintf ("( ");
	  skip_this_indent++;
	  make_command_string_internal (command->value.Subshell->command);
	  PRINT_DEFERRED_HEREDOCS ("");
	  cprintf (" )");
	  break;

	case cm_coproc:
	  cprintf ("coproc %s ", command->value.Coproc->name);
	  skip_this_indent++;
	  make_command_string_internal (command->value.Coproc->command);
	  break;

	default:
	  command_error ("print_command", CMDERR_BADTYPE, command->type, 0);
	  break;
	}


      if (command->redirects)
	{
	  cprintf (" ");
	  print_redirection_list (command->redirects);
	}
    }
}

static void
_print_word_list (list, separator, pfunc)
     WORD_LIST *list;
     char *separator;
     PFUNC *pfunc;
{
  WORD_LIST *w;

  for (w = list; w; w = w->next)
    (*pfunc) ("%s%s", w->word->word, w->next ? separator : "");
}

void
print_word_list (list, separator)
     WORD_LIST *list;
     char *separator;
{
  _print_word_list (list, separator, xprintf);
}

void
xtrace_set (fd, fp)
     int fd;
     FILE *fp;
{
  if (fd >= 0 && sh_validfd (fd) == 0)
    {
      internal_error (_("xtrace_set: %d: invalid file descriptor"), fd);
      return;
    }
  if (fp == 0)
    {
      internal_error (_("xtrace_set: NULL file pointer"));
      return;
    }
  if (fd >= 0 && fileno (fp) != fd)
    internal_warning (_("xtrace fd (%d) != fileno xtrace fp (%d)"), fd, fileno (fp));
  
  xtrace_fd = fd;
  xtrace_fp = fp;
}

void
xtrace_init ()
{
  xtrace_set (-1, stderr);
}

void
xtrace_reset ()
{
  if (xtrace_fd >= 0 && xtrace_fp)
    {
      fflush (xtrace_fp);
      fclose (xtrace_fp);
    }
  else if (xtrace_fd >= 0)
    close (xtrace_fd);

  xtrace_fd = -1;
  xtrace_fp = stderr;
}

void
xtrace_fdchk (fd)
     int fd;
{
  if (fd == xtrace_fd)
    xtrace_reset ();
}

 

char *
indirection_level_string ()
{
  register int i, j;
  char *ps4;
  char ps4_firstc[MB_LEN_MAX+1];
  int ps4_firstc_len, ps4_len, ineed, old;

  ps4 = get_string_value ("PS4");
  if (indirection_string == 0)
    indirection_string = xmalloc (indirection_stringsiz = 100);
  indirection_string[0] = '\0';

  if (ps4 == 0 || *ps4 == '\0')
    return (indirection_string);

  old = change_flag ('x', FLAG_OFF);
  ps4 = decode_prompt_string (ps4);
  if (old)
    change_flag ('x', FLAG_ON);

  if (ps4 == 0 || *ps4 == '\0')
    {
      FREE (ps4);
      return (indirection_string);
    }

#if defined (HANDLE_MULTIBYTE)
  ps4_len = strnlen (ps4, MB_CUR_MAX);
  ps4_firstc_len = MBLEN (ps4, ps4_len);
  if (ps4_firstc_len == 1 || ps4_firstc_len == 0 || ps4_firstc_len < 0)
    {
      ps4_firstc[0] = ps4[0];
      ps4_firstc[ps4_firstc_len = 1] = '\0';
    }
  else
    memcpy (ps4_firstc, ps4, ps4_firstc_len);
#else
  ps4_firstc[0] = ps4[0];
  ps4_firstc[ps4_firstc_len = 1] = '\0';
#endif

   
  ineed = (ps4_firstc_len * indirection_level) + strlen (ps4);
  if (ineed > indirection_stringsiz - 1)
    {
      indirection_stringsiz = ineed + 1;
      indirection_string = xrealloc (indirection_string, indirection_stringsiz);
    }

  for (i = j = 0; ps4_firstc[0] && j < indirection_level && i < indirection_stringsiz - 1; i += ps4_firstc_len, j++)
    {
      if (ps4_firstc_len == 1)
	indirection_string[i] = ps4_firstc[0];
      else
	memcpy (indirection_string+i, ps4_firstc, ps4_firstc_len);
    }      

  for (j = ps4_firstc_len; *ps4 && ps4[j] && i < indirection_stringsiz - 1; i++, j++)
    indirection_string[i] = ps4[j];

  indirection_string[i] = '\0';
  free (ps4);
  return (indirection_string);
}

void
xtrace_print_assignment (name, value, assign_list, xflags)
     char *name, *value;
     int assign_list, xflags;
{
  char *nval;

  CHECK_XTRACE_FP;

  if (xflags)
    fprintf (xtrace_fp, "%s", indirection_level_string ());

   
  if (*value == '\0' || assign_list)
    nval = value;
  else if (sh_contains_shell_metas (value))
    nval = sh_single_quote (value);
  else if (ansic_shouldquote (value))
    nval = ansic_quote (value, 0, (int *)0);
  else
    nval = value;

  if (assign_list)
    fprintf (xtrace_fp, "%s=(%s)\n", name, nval);
  else
    fprintf (xtrace_fp, "%s=%s\n", name, nval);

  if (nval != value)
    FREE (nval);

  fflush (xtrace_fp);
}

 
void
xtrace_print_word_list (list, xtflags)
     WORD_LIST *list;
     int xtflags;
{
  WORD_LIST *w;
  char *t, *x;

  CHECK_XTRACE_FP;

  if (xtflags&1)
    fprintf (xtrace_fp, "%s", indirection_level_string ());

  for (w = list; w; w = w->next)
    {
      t = w->word->word;
      if (t == 0 || *t == '\0')
	fprintf (xtrace_fp, "''%s", w->next ? " " : "");
      else if (xtflags & 2)
	fprintf (xtrace_fp, "%s%s", t, w->next ? " " : "");
      else if (sh_contains_shell_metas (t))
	{
	  x = sh_single_quote (t);
	  fprintf (xtrace_fp, "%s%s", x, w->next ? " " : "");
	  free (x);
	}
      else if (ansic_shouldquote (t))
	{
	  x = ansic_quote (t, 0, (int *)0);
	  fprintf (xtrace_fp, "%s%s", x, w->next ? " " : "");
	  free (x);
	}
      else
	fprintf (xtrace_fp, "%s%s", t, w->next ? " " : "");
    }
  fprintf (xtrace_fp, "\n");
  fflush (xtrace_fp);
}

static void
command_print_word_list (list, separator)
     WORD_LIST *list;
     char *separator;
{
  _print_word_list (list, separator, cprintf);
}

void
print_for_command_head (for_command)
     FOR_COM *for_command;
{
  cprintf ("for %s in ", for_command->name->word);
  command_print_word_list (for_command->map_list, " ");
}

void
xtrace_print_for_command_head (for_command)
     FOR_COM *for_command;
{
  CHECK_XTRACE_FP;
  fprintf (xtrace_fp, "%s", indirection_level_string ());
  fprintf (xtrace_fp, "for %s in ", for_command->name->word);
  xtrace_print_word_list (for_command->map_list, 2);
}

static void
print_for_command (for_command)
     FOR_COM *for_command;
{
  print_for_command_head (for_command);
  cprintf (";");
  newline ("do\n");

  indentation += indentation_amount;
  make_command_string_internal (for_command->action);
  PRINT_DEFERRED_HEREDOCS ("");
  semicolon ();
  indentation -= indentation_amount;

  newline ("done");
}

#if defined (ARITH_FOR_COMMAND)
static void
print_arith_for_command (arith_for_command)
     ARITH_FOR_COM *arith_for_command;
{
  cprintf ("for ((");
  command_print_word_list (arith_for_command->init, " ");
  cprintf ("; ");
  command_print_word_list (arith_for_command->test, " ");
  cprintf ("; ");
  command_print_word_list (arith_for_command->step, " ");
  cprintf ("))");
  newline ("do\n");
  indentation += indentation_amount;
  make_command_string_internal (arith_for_command->action);
  PRINT_DEFERRED_HEREDOCS ("");
  semicolon ();
  indentation -= indentation_amount;
  newline ("done");
}
#endif  

#if defined (SELECT_COMMAND)
void
print_select_command_head (select_command)
     SELECT_COM *select_command;
{
  cprintf ("select %s in ", select_command->name->word);
  command_print_word_list (select_command->map_list, " ");
}

void
xtrace_print_select_command_head (select_command)
     SELECT_COM *select_command;
{
  CHECK_XTRACE_FP;
  fprintf (xtrace_fp, "%s", indirection_level_string ());
  fprintf (xtrace_fp, "select %s in ", select_command->name->word);
  xtrace_print_word_list (select_command->map_list, 2);
}

static void
print_select_command (select_command)
     SELECT_COM *select_command;
{
  print_select_command_head (select_command);

  cprintf (";");
  newline ("do\n");
  indentation += indentation_amount;
  make_command_string_internal (select_command->action);
  PRINT_DEFERRED_HEREDOCS ("");
  semicolon ();
  indentation -= indentation_amount;
  newline ("done");
}
#endif  

static void
print_group_command (group_command)
     GROUP_COM *group_command;
{
  group_command_nesting++;
  cprintf ("{ ");

  if (inside_function_def == 0)
    skip_this_indent++;
  else
    {
       
      cprintf ("\n");
      indentation += indentation_amount;
    }

  make_command_string_internal (group_command->command);
  PRINT_DEFERRED_HEREDOCS ("");

  if (inside_function_def)
    {
      cprintf ("\n");
      indentation -= indentation_amount;
      indent (indentation);
    }
  else
    {
      semicolon ();
      cprintf (" ");
    }

  cprintf ("}");

  group_command_nesting--;
}

void
print_case_command_head (case_command)
     CASE_COM *case_command;
{
  cprintf ("case %s in ", case_command->word->word);
}

void
xtrace_print_case_command_head (case_command)
     CASE_COM *case_command;
{
  CHECK_XTRACE_FP;
  fprintf (xtrace_fp, "%s", indirection_level_string ());
  fprintf (xtrace_fp, "case %s in\n", case_command->word->word);
}

static void
print_case_command (case_command)
     CASE_COM *case_command;
{
  print_case_command_head (case_command);

  if (case_command->clauses)
    print_case_clauses (case_command->clauses);
  newline ("esac");
}

static void
print_case_clauses (clauses)
     PATTERN_LIST *clauses;
{
  indentation += indentation_amount;
  while (clauses)
    {
      newline ("");
      command_print_word_list (clauses->patterns, " | ");
      cprintf (")\n");
      indentation += indentation_amount;
      make_command_string_internal (clauses->action);
      indentation -= indentation_amount;
      PRINT_DEFERRED_HEREDOCS ("");
      if (clauses->flags & CASEPAT_FALLTHROUGH)
	newline (";&");
      else if (clauses->flags & CASEPAT_TESTNEXT)
	newline (";;&");
      else
	newline (";;");
      clauses = clauses->next;
    }
  indentation -= indentation_amount;
}

static void
print_while_command (while_command)
     WHILE_COM *while_command;
{
  print_until_or_while (while_command, "while");
}

static void
print_until_command (while_command)
     WHILE_COM *while_command;
{
  print_until_or_while (while_command, "until");
}

static void
print_until_or_while (while_command, which)
     WHILE_COM *while_command;
     char *which;
{
  cprintf ("%s ", which);
  skip_this_indent++;
  make_command_string_internal (while_command->test);
  PRINT_DEFERRED_HEREDOCS ("");
  semicolon ();
  cprintf (" do\n");	 
  indentation += indentation_amount;
  make_command_string_internal (while_command->action);
  PRINT_DEFERRED_HEREDOCS ("");
  indentation -= indentation_amount;
  semicolon ();
  newline ("done");
}

static void
print_if_command (if_command)
     IF_COM *if_command;
{
  cprintf ("if ");
  skip_this_indent++;
  make_command_string_internal (if_command->test);
  semicolon ();
  cprintf (" then\n");
  indentation += indentation_amount;
  make_command_string_internal (if_command->true_case);
  PRINT_DEFERRED_HEREDOCS ("");
  indentation -= indentation_amount;

  if (if_command->false_case)
    {
      semicolon ();
      newline ("else\n");
      indentation += indentation_amount;
      make_command_string_internal (if_command->false_case);
      PRINT_DEFERRED_HEREDOCS ("");
      indentation -= indentation_amount;
    }
  semicolon ();
  newline ("fi");
}

#if defined (DPAREN_ARITHMETIC) || defined (ARITH_FOR_COMMAND)
void
print_arith_command (arith_cmd_list)
     WORD_LIST *arith_cmd_list;
{
  cprintf ("((");
  command_print_word_list (arith_cmd_list, " ");
  cprintf ("))");
}
#endif

#if defined (COND_COMMAND)
static void
print_cond_node (cond)
     COND_COM *cond;
{
  if (cond->flags & CMD_INVERT_RETURN)
    cprintf ("! ");

  if (cond->type == COND_EXPR)
    {
      cprintf ("( ");
      print_cond_node (cond->left);
      cprintf (" )");
    }
  else if (cond->type == COND_AND)
    {
      print_cond_node (cond->left);
      cprintf (" && ");
      print_cond_node (cond->right);
    }
  else if (cond->type == COND_OR)
    {
      print_cond_node (cond->left);
      cprintf (" || ");
      print_cond_node (cond->right);
    }
  else if (cond->type == COND_UNARY)
    {
      cprintf ("%s", cond->op->word);
      cprintf (" ");
      print_cond_node (cond->left);
    }
  else if (cond->type == COND_BINARY)
    {
      print_cond_node (cond->left);
      cprintf (" ");
      cprintf ("%s", cond->op->word);
      cprintf (" ");
      print_cond_node (cond->right);
    }
  else if (cond->type == COND_TERM)
    {
      cprintf ("%s", cond->op->word);		 
    }
}

void
print_cond_command (cond)
     COND_COM *cond;
{
  cprintf ("[[ ");
  print_cond_node (cond);
  cprintf (" ]]");
}

#ifdef DEBUG
void
debug_print_word_list (s, list, sep)
     char *s;
     WORD_LIST *list;
     char *sep;
{
  WORD_LIST *w;

  if (s)
    fprintf (stderr, "%s: ", s);
  for (w = list; w; w = w->next)
    fprintf (stderr, "%s%s", w->word->word, w->next ? sep : "");
  fprintf (stderr, "\n");
}

void
debug_print_cond_command (cond)
     COND_COM *cond;
{
  fprintf (stderr, "DEBUG: ");
  command_string_index = 0;
  print_cond_command (cond);
  fprintf (stderr, "%s\n", the_printed_command);
}
#endif

void
xtrace_print_cond_term (type, invert, op, arg1, arg2)
     int type, invert;
     WORD_DESC *op;
     char *arg1, *arg2;
{
  CHECK_XTRACE_FP;
  command_string_index = 0;
  fprintf (xtrace_fp, "%s", indirection_level_string ());
  fprintf (xtrace_fp, "[[ ");
  if (invert)
    fprintf (xtrace_fp, "! ");

  if (type == COND_UNARY)
    {
      fprintf (xtrace_fp, "%s ", op->word);
      fprintf (xtrace_fp, "%s", (arg1 && *arg1) ? arg1 : "''");
    }
  else if (type == COND_BINARY)
    {
      fprintf (xtrace_fp, "%s", (arg1 && *arg1) ? arg1 : "''");
      fprintf (xtrace_fp, " %s ", op->word);
      fprintf (xtrace_fp, "%s", (arg2 && *arg2) ? arg2 : "''");
    }

  fprintf (xtrace_fp, " ]]\n");

  fflush (xtrace_fp);
}	  
#endif  

#if defined (DPAREN_ARITHMETIC) || defined (ARITH_FOR_COMMAND)
 
void
xtrace_print_arith_cmd (list)
     WORD_LIST *list;
{
  WORD_LIST *w;

  CHECK_XTRACE_FP;
  fprintf (xtrace_fp, "%s", indirection_level_string ());
  fprintf (xtrace_fp, "(( ");
  for (w = list; w; w = w->next)
    fprintf (xtrace_fp, "%s%s", w->word->word, w->next ? " " : "");
  fprintf (xtrace_fp, " ))\n");

  fflush (xtrace_fp);
}
#endif

void
print_simple_command (simple_command)
     SIMPLE_COM *simple_command;
{
  if (simple_command->words)
    command_print_word_list (simple_command->words, " ");

  if (simple_command->redirects)
    {
      if (simple_command->words)
	cprintf (" ");
      print_redirection_list (simple_command->redirects);
    }
}

static void
print_heredocs (heredocs)
     REDIRECT *heredocs;
{
  REDIRECT *hdtail;

  cprintf (" "); 
  for (hdtail = heredocs; hdtail; hdtail = hdtail->next)
    {
      print_redirection (hdtail);
      cprintf ("\n");
    }
  was_heredoc = 1;
}

static void
print_heredoc_bodies (heredocs)
     REDIRECT *heredocs;
{
  REDIRECT *hdtail;

  cprintf ("\n"); 
  for (hdtail = heredocs; hdtail; hdtail = hdtail->next)
    {
      print_heredoc_body (hdtail);
      cprintf ("\n");
    }
  was_heredoc = 1;
}

 
static void
print_deferred_heredocs (cstring)
     const char *cstring;
{
   
  if (cstring && cstring[0] && (cstring[0] != ';' || cstring[1]))
    cprintf ("%s", cstring); 
  if (deferred_heredocs)
    {
      print_heredoc_bodies (deferred_heredocs);
      if (cstring && cstring[0] && (cstring[0] != ';' || cstring[1]))
	cprintf (" ");	 
      dispose_redirects (deferred_heredocs);
      was_heredoc = 1;
    }
  deferred_heredocs = (REDIRECT *)NULL;
}
      
static void
print_redirection_list (redirects)
     REDIRECT *redirects;
{
  REDIRECT *heredocs, *hdtail, *newredir;
  char *rw;

  heredocs = (REDIRECT *)NULL;
  hdtail = heredocs;

  was_heredoc = 0;
  while (redirects)
    {
       
      if (redirects->instruction == r_reading_until || redirects->instruction == r_deblank_reading_until)
	{
	  newredir = copy_redirect (redirects);
	  newredir->next = (REDIRECT *)NULL;

	  print_heredoc_header (newredir);

	  if (heredocs)
	    {
	      hdtail->next = newredir;
	      hdtail = newredir;
	    }
	  else
	    hdtail = heredocs = newredir;
	}
#if 0
       
      else if (redirects->instruction == r_duplicating_output_word && (redirects->flags & REDIR_VARASSIGN) == 0 && redirects->redirector.dest == 1)
	{
	   
	  rw = redirects->redirectee.filename->word;
	  if (rw && *rw != '-' && DIGIT (*rw) == 0 && EXPCHAR (*rw) == 0)
	    redirects->instruction = r_err_and_out;
	  print_redirection (redirects);
	  redirects->instruction = r_duplicating_output_word;
	}
#endif
      else
	print_redirection (redirects);

      redirects = redirects->next;
      if (redirects)
	cprintf (" ");
    }

   
  if (heredocs && printing_connection)
    deferred_heredocs = heredocs;
  else if (heredocs)
    {
      print_heredoc_bodies (heredocs);
      dispose_redirects (heredocs);
    }
}

static void
print_heredoc_header (redirect)
     REDIRECT *redirect;
{
  int kill_leading;
  char *x;

  kill_leading = redirect->instruction == r_deblank_reading_until;

   
  if (redirect->rflags & REDIR_VARASSIGN)
    cprintf ("{%s}", redirect->redirector.filename->word);
  else if (redirect->redirector.dest != 0)
    cprintf ("%d", redirect->redirector.dest);

   
  if (redirect->redirectee.filename->flags & W_QUOTED)
    {
      x = sh_single_quote (redirect->here_doc_eof);
      cprintf ("<<%s%s", kill_leading ? "-" : "", x);
      free (x);
    }
  else
    cprintf ("<<%s%s", kill_leading ? "-" : "", redirect->here_doc_eof);
}

static void
print_heredoc_body (redirect)
     REDIRECT *redirect;
{
   
  cprintf ("%s%s", redirect->redirectee.filename->word, redirect->here_doc_eof);
}

static void
print_redirection (redirect)
     REDIRECT *redirect;
{
  int redirector, redir_fd;
  WORD_DESC *redirectee, *redir_word;

  redirectee = redirect->redirectee.filename;
  redir_fd = redirect->redirectee.dest;

  redir_word = redirect->redirector.filename;
  redirector = redirect->redirector.dest;

  switch (redirect->instruction)
    {
    case r_input_direction:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}", redir_word->word);
      else if (redirector != 0)
	cprintf ("%d", redirector);
      cprintf ("< %s", redirectee->word);
      break;

    case r_output_direction:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}", redir_word->word);
      else if (redirector != 1)
	cprintf ("%d", redirector);
      cprintf ("> %s", redirectee->word);
      break;

    case r_inputa_direction:	 
      cprintf ("&");
      break;

    case r_output_force:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}", redir_word->word);
      else if (redirector != 1)
	cprintf ("%d", redirector);
      cprintf (">| %s", redirectee->word);
      break;

    case r_appending_to:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}", redir_word->word);
      else if (redirector != 1)
	cprintf ("%d", redirector);
      cprintf (">> %s", redirectee->word);
      break;

    case r_input_output:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}", redir_word->word);
      else if (redirector != 1)
	cprintf ("%d", redirector);
      cprintf ("<> %s", redirectee->word);
      break;

    case r_deblank_reading_until:
    case r_reading_until:
      print_heredoc_header (redirect);
      cprintf ("\n");
      print_heredoc_body (redirect);
      break;

    case r_reading_string:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}", redir_word->word);
      else if (redirector != 0)
	cprintf ("%d", redirector);
#if 0
       
      if (ansic_shouldquote (redirect->redirectee.filename->word))
	{
	  char *x;
	  x = ansic_quote (redirect->redirectee.filename->word, 0, (int *)0);
	  cprintf ("<<< %s", x);
	  free (x);
	}
      else
#endif
	cprintf ("<<< %s", redirect->redirectee.filename->word);
      break;

    case r_duplicating_input:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}<&%d", redir_word->word, redir_fd);
      else
	cprintf ("%d<&%d", redirector, redir_fd);
      break;

    case r_duplicating_output:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}>&%d", redir_word->word, redir_fd);
      else
	cprintf ("%d>&%d", redirector, redir_fd);
      break;

    case r_duplicating_input_word:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}<&%s", redir_word->word, redirectee->word);
      else if (redirector == 0)
	cprintf ("<&%s", redirectee->word);
      else
	cprintf ("%d<&%s", redirector, redirectee->word);
      break;

    case r_duplicating_output_word:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}>&%s", redir_word->word, redirectee->word);
      else if (redirector == 1)
	cprintf (">&%s", redirectee->word);
      else
	cprintf ("%d>&%s", redirector, redirectee->word);
      break;

    case r_move_input:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}<&%d-", redir_word->word, redir_fd);
      else
	cprintf ("%d<&%d-", redirector, redir_fd);
      break;

    case r_move_output:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}>&%d-", redir_word->word, redir_fd);
      else
	cprintf ("%d>&%d-", redirector, redir_fd);
      break;

    case r_move_input_word:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}<&%s-", redir_word->word, redirectee->word);
      else
	cprintf ("%d<&%s-", redirector, redirectee->word);
      break;

    case r_move_output_word:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}>&%s-", redir_word->word, redirectee->word);
      else
	cprintf ("%d>&%s-", redirector, redirectee->word);
      break;

    case r_close_this:
      if (redirect->rflags & REDIR_VARASSIGN)
	cprintf ("{%s}>&-", redir_word->word);
      else
	cprintf ("%d>&-", redirector);
      break;

    case r_err_and_out:
      cprintf ("&> %s", redirectee->word);
      break;

    case r_append_err_and_out:
      cprintf ("&>> %s", redirectee->word);
      break;
    }
}

static void
reset_locals ()
{
  inside_function_def = 0;
  indentation = 0;
  printing_connection = 0;
  deferred_heredocs = 0;
  printing_comsub = 0;
}

static void
print_function_def (func)
     FUNCTION_DEF *func;
{
  COMMAND *cmdcopy;
  REDIRECT *func_redirects;

  func_redirects = NULL;
   
  if (posixly_correct == 0)
    cprintf ("function %s () \n", func->name->word);
  else
    cprintf ("%s () \n", func->name->word);
  add_unwind_protect (reset_locals, 0);

  indent (indentation);
  cprintf ("{ \n");

  inside_function_def++;
  indentation += indentation_amount;

  cmdcopy = copy_command (func->command);
  if (cmdcopy->type == cm_group)
    {
      func_redirects = cmdcopy->redirects;
      cmdcopy->redirects = (REDIRECT *)NULL;
    }
  make_command_string_internal (cmdcopy->type == cm_group
					? cmdcopy->value.Group->command
					: cmdcopy);
  PRINT_DEFERRED_HEREDOCS ("");

  remove_unwind_protect ();
  indentation -= indentation_amount;
  inside_function_def--;

  if (func_redirects)
    {  
      newline ("} ");
      print_redirection_list (func_redirects);
      cmdcopy->redirects = func_redirects;
    }
  else
    {
       
      newline ("}");
      was_heredoc = 0;		 
    }

  dispose_command (cmdcopy);
}

 
char *
named_function_string (name, command, flags)
     char *name;
     COMMAND *command;
     int flags;
{
  char *result;
  int old_indent, old_amount;
  COMMAND *cmdcopy;
  REDIRECT *func_redirects;

  old_indent = indentation;
  old_amount = indentation_amount;
  command_string_index = was_heredoc = 0;
  deferred_heredocs = 0;
  printing_comsub = 0;

  if (name && *name)
    {
      if (find_reserved_word (name) >= 0)
	cprintf ("function ");
      cprintf ("%s ", name);
    }

  cprintf ("() ");

  if ((flags & FUNC_MULTILINE) == 0)
    {
      indentation = 1;
      indentation_amount = 0;
    }
  else
    {
      cprintf ("\n");
      indentation += indentation_amount;
    }

  inside_function_def++;

  cprintf ((flags & FUNC_MULTILINE) ? "{ \n" : "{ ");

  cmdcopy = copy_command (command);
   
  func_redirects = (REDIRECT *)NULL;
  if (cmdcopy->type == cm_group)
    {
      func_redirects = cmdcopy->redirects;
      cmdcopy->redirects = (REDIRECT *)NULL;
    }
  make_command_string_internal (cmdcopy->type == cm_group
					? cmdcopy->value.Group->command
					: cmdcopy);
  PRINT_DEFERRED_HEREDOCS ("");

  indentation = old_indent;
  indentation_amount = old_amount;
  inside_function_def--;

  if (func_redirects)
    {  
      newline ("} ");
      print_redirection_list (func_redirects);
      cmdcopy->redirects = func_redirects;
    }
  else
    {	 
      newline ("}");
      was_heredoc = 0;
    }

  result = the_printed_command;

  if ((flags & FUNC_MULTILINE) == 0)
    {
#if 0
      register int i;
      for (i = 0; result[i]; i++)
	if (result[i] == '\n')
	  {
	    strcpy (result + i, result + i + 1);
	    --i;
	  }
#else
      if (result[2] == '\n')	 
	memmove (result + 2, result + 3, strlen (result) - 2);	
#endif
    }

  dispose_command (cmdcopy);

  if (flags & FUNC_EXTERNAL)
    result = remove_quoted_escapes (result);

  return (result);
}

static void
newline (string)
     char *string;
{
  cprintf ("\n");
  indent (indentation);
  if (string && *string)
    cprintf ("%s", string);
}

static char *indentation_string;
static int indentation_size;

static void
indent (amount)
     int amount;
{
  register int i;

  RESIZE_MALLOCED_BUFFER (indentation_string, 0, amount, indentation_size, 16);

  for (i = 0; amount > 0; amount--)
    indentation_string[i++] = ' ';
  indentation_string[i] = '\0';
  cprintf ("%s", indentation_string);
}

static void
semicolon ()
{
  if (command_string_index > 0 &&
       (the_printed_command[command_string_index - 1] == '&' ||
	the_printed_command[command_string_index - 1] == '\n'))
    return;
  cprintf (";");
}

 
static void
#if defined (PREFER_STDARG)
cprintf (const char *control, ...)
#else
cprintf (control, va_alist)
     const char *control;
     va_dcl
#endif
{
  register const char *s;
  char char_arg[2], *argp, intbuf[INT_STRLEN_BOUND (unsigned int) + 1];
  int digit_arg, arg_len, c;
  va_list args;

  SH_VA_START (args, control);

  arg_len = strlen (control);
  the_printed_command_resize (arg_len + 1);

  char_arg[1] = '\0';
  s = control;
  while (s && *s)
    {
      c = *s++;
      argp = (char *)NULL;
      if (c != '%' || !*s)
	{
	  char_arg[0] = c;
	  argp = char_arg;
	  arg_len = 1;
	}
      else
	{
	  c = *s++;
	  switch (c)
	    {
	    case '%':
	      char_arg[0] = c;
	      argp = char_arg;
	      arg_len = 1;
	      break;

	    case 's':
	      argp = va_arg (args, char *);
	      arg_len = strlen (argp);
	      break;

	    case 'd':
	       
	      digit_arg = va_arg (args, int);
	      if (digit_arg < 0)
		{
		  sprintf (intbuf, "%u", (unsigned int)-1);
		  argp = intbuf;
		}
	      else
		argp = inttostr (digit_arg, intbuf, sizeof (intbuf));
	      arg_len = strlen (argp);
	      break;

	    case 'c':
	      char_arg[0] = va_arg (args, int);
	      argp = char_arg;
	      arg_len = 1;
	      break;

	    default:
	      programming_error (_("cprintf: `%c': invalid format character"), c);
	       
	    }
	}

      if (argp && arg_len)
	{
	  the_printed_command_resize (arg_len + 1);
	  FASTCOPY (argp, the_printed_command + command_string_index, arg_len);
	  command_string_index += arg_len;
	}
    }

  va_end (args);

  the_printed_command[command_string_index] = '\0';
}

 
static void
the_printed_command_resize (length)
     int length;
{
  if (the_printed_command == 0)
    {
      the_printed_command_size = (length + PRINTED_COMMAND_INITIAL_SIZE - 1) & ~(PRINTED_COMMAND_INITIAL_SIZE - 1);
      the_printed_command = (char *)xmalloc (the_printed_command_size);
      command_string_index = 0;
    }
  else if ((command_string_index + length) >= the_printed_command_size)
    {
      int new;
      new = command_string_index + length + 1;

       
      new = (new + PRINTED_COMMAND_GROW_SIZE - 1) & ~(PRINTED_COMMAND_GROW_SIZE - 1);
      the_printed_command_size = new;

      the_printed_command = (char *)xrealloc (the_printed_command, the_printed_command_size);
    }
}

#if defined (HAVE_VPRINTF)
 

static void
#if defined (PREFER_STDARG)
xprintf (const char *format, ...)
#else
xprintf (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  SH_VA_START (args, format);

  vfprintf (stdout, format, args);
  va_end (args);
}

#else

static void
xprintf (format, arg1, arg2, arg3, arg4, arg5)
     const char *format;
{
  printf (format, arg1, arg2, arg3, arg4, arg5);
}

#endif  
