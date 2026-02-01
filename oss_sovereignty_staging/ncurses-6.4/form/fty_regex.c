 

 

#include "form.priv.h"

MODULE_ID("$Id: fty_regex.c,v 1.33 2021/08/14 15:01:52 tom Exp $")

#if HAVE_REGEX_H_FUNCS || HAVE_LIB_PCRE2	 

#if HAVE_PCRE2POSIX_H
#include <pcre2posix.h>

 
#if !defined(PCRE2regcomp) && defined(HAVE_PCRE2REGCOMP)

#undef regcomp
#undef regexec
#undef regfree

#ifdef __cplusplus
extern "C"
{
#endif
  PCRE2POSIX_EXP_DECL int PCRE2regcomp(regex_t *, const char *, int);
  PCRE2POSIX_EXP_DECL int PCRE2regexec(const regex_t *, const char *, size_t,
				       regmatch_t *, int);
  PCRE2POSIX_EXP_DECL void PCRE2regfree(regex_t *);
#ifdef __cplusplus
}				 
#endif
#define regcomp(r,s,n)          PCRE2regcomp(r,s,n)
#define regexec(r,s,n,m,x)      PCRE2regexec(r,s,n,m,x)
#define regfree(r)              PCRE2regfree(r)
#endif
 
#elif HAVE_PCREPOSIX_H
#include <pcreposix.h>
#else
#include <regex.h>
#endif

typedef struct
  {
    regex_t *pRegExp;
    unsigned long *refCount;
  }
RegExp_Arg;

#elif HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS
#undef RETURN
static int reg_errno;

static char *
RegEx_Init(char *instring)
{
  reg_errno = 0;
  return instring;
}

static char *
RegEx_Error(int code)
{
  reg_errno = code;
  return 0;
}

#define INIT 		register char *sp = RegEx_Init(instring);
#define GETC()		(*sp++)
#define PEEKC()		(*sp)
#define UNGETC(c)	(--sp)
#define RETURN(c)	return(c)
#define ERROR(c)	return RegEx_Error(c)

#if HAVE_REGEXP_H_FUNCS
#include <regexp.h>
#else
#include <regexpr.h>
#endif

typedef struct
{
  char *compiled_expression;
  unsigned long *refCount;
}
RegExp_Arg;

 
#define MAX_RX_LEN   (2048)
#define RX_INCREMENT (256)

#endif

#if HAVE_REGEX_H_FUNCS | HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS
# define MAYBE_UNUSED
#else
# define MAYBE_UNUSED GCC_UNUSED
#endif

 
static void *
Generic_RegularExpression_Type(void *arg MAYBE_UNUSED)
{
#if HAVE_REGEX_H_FUNCS
  char *rx = (char *)arg;
  RegExp_Arg *preg = (RegExp_Arg *)0;

  if (rx)
    {
      preg = typeCalloc(RegExp_Arg, 1);

      if (preg)
	{
	  T((T_CREATE("RegExp_Arg %p"), (void *)preg));
	  if (((preg->pRegExp = typeMalloc(regex_t, 1)) != 0)
	      && !regcomp(preg->pRegExp, rx,
			  (REG_EXTENDED | REG_NOSUB | REG_NEWLINE)))
	    {
	      T((T_CREATE("regex_t %p"), (void *)preg->pRegExp));
	      if ((preg->refCount = typeMalloc(unsigned long, 1)) != 0)
		 *(preg->refCount) = 1;
	    }
	  else
	    {
	      if (preg->pRegExp)
		free(preg->pRegExp);
	      free(preg);
	      preg = (RegExp_Arg *)0;
	    }
	}
    }
  return ((void *)preg);
#elif HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS
  char *rx = (char *)arg;
  RegExp_Arg *pArg = (RegExp_Arg *)0;

  if (rx)
    {
      pArg = typeMalloc(RegExp_Arg, 1);

      if (pArg)
	{
	  int blen = RX_INCREMENT;

	  T((T_CREATE("RegExp_Arg %p"), pArg));
	  pArg->compiled_expression = NULL;
	  if ((pArg->refCount = typeMalloc(unsigned long, 1)) != 0)
	     *(pArg->refCount) = 1;

	  do
	    {
	      char *buf = typeMalloc(char, blen);

	      if (buf)
		{
#if HAVE_REGEXP_H_FUNCS
		  char *last_pos = compile(rx, buf, &buf[blen], '\0');

#else  
		  char *last_pos = compile(rx, buf, &buf[blen]);
#endif
		  if (reg_errno)
		    {
		      free(buf);
		      if (reg_errno == 50)
			blen += RX_INCREMENT;
		      else
			{
			  free(pArg);
			  pArg = NULL;
			  break;
			}
		    }
		  else
		    {
		      pArg->compiled_expression = buf;
		      break;
		    }
		}
	    }
	  while (blen <= MAX_RX_LEN);
	}
      if (pArg && !pArg->compiled_expression)
	{
	  free(pArg);
	  pArg = NULL;
	}
    }
  return (void *)pArg;
#else
  return 0;
#endif
}

 
static void *
Make_RegularExpression_Type(va_list *ap)
{
  char *rx = va_arg(*ap, char *);

  return Generic_RegularExpression_Type((void *)rx);
}

 
static void *
Copy_RegularExpression_Type(const void *argp MAYBE_UNUSED)
{
#if (HAVE_REGEX_H_FUNCS | HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS)
  const RegExp_Arg *ap = (const RegExp_Arg *)argp;
  const RegExp_Arg *result = (const RegExp_Arg *)0;

  if (ap)
    {
      *(ap->refCount) += 1;
      result = ap;
    }
  return (void *)result;
#else
  return 0;
#endif
}

 
static void
Free_RegularExpression_Type(void *argp MAYBE_UNUSED)
{
#if HAVE_REGEX_H_FUNCS | HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS
  RegExp_Arg *ap = (RegExp_Arg *)argp;

  if (ap)
    {
      if (--(*(ap->refCount)) == 0)
	{
#if HAVE_REGEX_H_FUNCS
	  if (ap->pRegExp)
	    {
	      free(ap->refCount);
	      regfree(ap->pRegExp);
	      free(ap->pRegExp);
	    }
#elif HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS
	  if (ap->compiled_expression)
	    {
	      free(ap->refCount);
	      free(ap->compiled_expression);
	    }
#endif
	  free(ap);
	}
    }
#endif
}

 
static bool
Check_RegularExpression_Field(FIELD *field MAYBE_UNUSED,
			      const void *argp MAYBE_UNUSED)
{
  bool match = FALSE;

#if HAVE_REGEX_H_FUNCS
  const RegExp_Arg *ap = (const RegExp_Arg *)argp;

  if (ap && ap->pRegExp)
    match = (regexec(ap->pRegExp, field_buffer(field, 0), 0, NULL, 0)
	     ? FALSE
	     : TRUE);
#elif HAVE_REGEXP_H_FUNCS | HAVE_REGEXPR_H_FUNCS
  RegExp_Arg *ap = (RegExp_Arg *)argp;

  if (ap && ap->compiled_expression)
    match = (step(field_buffer(field, 0), ap->compiled_expression)
	     ? TRUE
	     : FALSE);
#endif
  return match;
}

static FIELDTYPE typeREGEXP =
{
  _HAS_ARGS | _RESIDENT,
  1,				 
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_RegularExpression_Type,
  Copy_RegularExpression_Type,
  Free_RegularExpression_Type,
  INIT_FT_FUNC(Check_RegularExpression_Field),
  INIT_FT_FUNC(NULL),
  INIT_FT_FUNC(NULL),
  INIT_FT_FUNC(NULL),
#if NCURSES_INTEROP_FUNCS
  Generic_RegularExpression_Type
#endif
};

FORM_EXPORT_VAR(FIELDTYPE *) TYPE_REGEXP = &typeREGEXP;

#if NCURSES_INTEROP_FUNCS
 
FORM_EXPORT(FIELDTYPE *)
_nc_TYPE_REGEXP(void)
{
  return TYPE_REGEXP;
}
#endif

 
