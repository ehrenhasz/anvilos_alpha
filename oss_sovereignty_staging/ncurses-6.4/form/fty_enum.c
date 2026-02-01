 

 

#include "form.priv.h"

MODULE_ID("$Id: fty_enum.c,v 1.33 2021/06/17 21:11:08 tom Exp $")

typedef struct
  {
    char **kwds;
    int count;
    bool checkcase;
    bool checkunique;
  }
enumARG;

typedef struct
  {
    char **kwds;
    int ccase;
    int cunique;
  }
enumParams;

 
static void *
Generic_Enum_Type(void *arg)
{
  enumARG *argp = (enumARG *)0;
  enumParams *params = (enumParams *)arg;

  if (params)
    {
      argp = typeMalloc(enumARG, 1);

      if (argp)
	{
	  int cnt = 0;
	  char **kp = (char **)0;
	  char **kwds = (char **)0;
	  int ccase, cunique;

	  T((T_CREATE("enumARG %p"), (void *)argp));
	  kwds = params->kwds;
	  ccase = params->ccase;
	  cunique = params->cunique;

	  argp->checkcase = ccase ? TRUE : FALSE;
	  argp->checkunique = cunique ? TRUE : FALSE;
	  argp->kwds = (char **)0;

	  kp = kwds;
	  while (kp && (*kp++))
	    cnt++;
	  argp->count = cnt;

	  if (cnt > 0)
	    {
	      char **kptarget;

	       
	      argp->kwds = typeMalloc(char *, cnt + 1);

	      kp = kwds;
	      if ((kptarget = argp->kwds) != 0)
		{
		  while (kp && (*kp))
		    {
		      (*kptarget++) = strdup(*kp++);
		    }
		  *kptarget = (char *)0;
		}
	    }
	}
    }
  return (void *)argp;
}

 
static void *
Make_Enum_Type(va_list *ap)
{
  enumParams params;

  params.kwds = va_arg(*ap, char **);
  params.ccase = va_arg(*ap, int);
  params.cunique = va_arg(*ap, int);

  return Generic_Enum_Type((void *)&params);
}

 
static void *
Copy_Enum_Type(const void *argp)
{
  enumARG *result = (enumARG *)0;

  if (argp)
    {
      const enumARG *ap = (const enumARG *)argp;

      result = typeMalloc(enumARG, 1);

      if (result)
	{
	  T((T_CREATE("enumARG %p"), (void *)result));
	  *result = *ap;

	  if (ap->count > 0)
	    {
	      char **kptarget;
	      char **kp = ap->kwds;
	      result->kwds = typeMalloc(char *, 1 + ap->count);

	      if ((kptarget = result->kwds) != 0)
		{
		  while (kp && (*kp))
		    {
		      (*kptarget++) = strdup(*kp++);
		    }
		  *kptarget = (char *)0;
		}
	    }
	}
    }
  return (void *)result;
}

 
static void
Free_Enum_Type(void *argp)
{
  if (argp)
    {
      const enumARG *ap = (const enumARG *)argp;

      if (ap->kwds && ap->count > 0)
	{
	  char **kp = ap->kwds;
	  int cnt = 0;

	  while (kp && (*kp))
	    {
	      free(*kp++);
	      cnt++;
	    }
	  assert(cnt == ap->count);
	  free(ap->kwds);
	}
      free(argp);
    }
}

#define SKIP_SPACE(x) while(((*(x))!='\0') && (is_blank(*(x)))) (x)++
#define NOMATCH 0
#define PARTIAL 1
#define EXACT   2

 
static int
Compare(const unsigned char *s, const unsigned char *buf,
	bool ccase)
{
  SKIP_SPACE(buf);		 
  SKIP_SPACE(s);

  if (*buf == '\0')
    {
      return (((*s) != '\0') ? NOMATCH : EXACT);
    }
  else
    {
      if (ccase)
	{
	  while (*s++ == *buf)
	    {
	      if (*buf++ == '\0')
		return EXACT;
	    }
	}
      else
	{
	  while (toupper(*s++) == toupper(*buf))
	    {
	      if (*buf++ == '\0')
		return EXACT;
	    }
	}
    }
   
  SKIP_SPACE(buf);
  if (*buf)
    return NOMATCH;

   
  return ((s[-1] != '\0') ? PARTIAL : EXACT);
}

 
static bool
Check_Enum_Field(FIELD *field, const void *argp)
{
  char **kwds = ((const enumARG *)argp)->kwds;
  bool ccase = ((const enumARG *)argp)->checkcase;
  bool unique = ((const enumARG *)argp)->checkunique;
  unsigned char *bp = (unsigned char *)field_buffer(field, 0);
  char *s, *t, *p;

  while (kwds && (s = (*kwds++)))
    {
      int res;

      if ((res = Compare((unsigned char *)s, bp, ccase)) != NOMATCH)
	{
	  p = t = s;		 
	  if ((unique && res != EXACT))
	    {
	      while (kwds && (p = *kwds++))
		{
		  if ((res = Compare((unsigned char *)p, bp, ccase)) != NOMATCH)
		    {
		      if (res == EXACT)
			{
			  t = p;
			  break;
			}
		      else
			t = (char *)0;
		    }
		}
	    }
	  if (t)
	    {
	      set_field_buffer(field, 0, t);
	      return TRUE;
	    }
	  if (!p)
	    break;
	}
    }
  return FALSE;
}

static const char *dummy[] =
{(char *)0};

 
static bool
Next_Enum(FIELD *field, const void *argp)
{
  const enumARG *args = (const enumARG *)argp;
  char **kwds = args->kwds;
  bool ccase = args->checkcase;
  int cnt = args->count;
  unsigned char *bp = (unsigned char *)field_buffer(field, 0);

  if (kwds)
    {
      while (cnt--)
	{
	  if (Compare((unsigned char *)(*kwds++), bp, ccase) == EXACT)
	    break;
	}
      if (cnt <= 0)
	kwds = args->kwds;
      if ((cnt >= 0) || (Compare((const unsigned char *)dummy, bp, ccase) == EXACT))
	{
	  set_field_buffer(field, 0, *kwds);
	  return TRUE;
	}
    }
  return FALSE;
}

 
static bool
Previous_Enum(FIELD *field, const void *argp)
{
  const enumARG *args = (const enumARG *)argp;
  int cnt = args->count;
  char **kwds = &args->kwds[cnt - 1];
  bool ccase = args->checkcase;
  unsigned char *bp = (unsigned char *)field_buffer(field, 0);

  if (kwds)
    {
      while (cnt--)
	{
	  if (Compare((unsigned char *)(*kwds--), bp, ccase) == EXACT)
	    break;
	}

      if (cnt <= 0)
	kwds = &args->kwds[args->count - 1];

      if ((cnt >= 0) || (Compare((const unsigned char *)dummy, bp, ccase) == EXACT))
	{
	  set_field_buffer(field, 0, *kwds);
	  return TRUE;
	}
    }
  return FALSE;
}

static FIELDTYPE typeENUM =
{
  _HAS_ARGS | _HAS_CHOICE | _RESIDENT,
  1,				 
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_Enum_Type,
  Copy_Enum_Type,
  Free_Enum_Type,
  INIT_FT_FUNC(Check_Enum_Field),
  INIT_FT_FUNC(NULL),
  INIT_FT_FUNC(Next_Enum),
  INIT_FT_FUNC(Previous_Enum),
#if NCURSES_INTEROP_FUNCS
  Generic_Enum_Type
#endif
};

FORM_EXPORT_VAR(FIELDTYPE *) TYPE_ENUM = &typeENUM;

#if NCURSES_INTEROP_FUNCS
 
FORM_EXPORT(FIELDTYPE *)
_nc_TYPE_ENUM(void)
{
  return TYPE_ENUM;
}
#endif

 
