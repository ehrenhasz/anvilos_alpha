 

 

#ifndef _PLURAL_EXP_H
#define _PLURAL_EXP_H

#ifndef PARAMS
# if __STDC__ || defined __GNUC__ || defined __SUNPRO_C || defined __cplusplus || __PROTOTYPES
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif
#endif

#ifndef internal_function
# define internal_function
#endif

#ifndef attribute_hidden
# define attribute_hidden
#endif


 
struct expression
{
  int nargs;			 
  enum operator
  {
     
    var,			 
    num,			 
     
    lnot,			 
     
    mult,			 
    divide,			 
    module,			 
    plus,			 
    minus,			 
    less_than,			 
    greater_than,		 
    less_or_equal,		 
    greater_or_equal,		 
    equal,			 
    not_equal,			 
    land,			 
    lor,			 
     
    qmop			 
  } operation;
  union
  {
    unsigned long int num;	 
    struct expression *args[3];	 
  } val;
};

 
struct parse_args
{
  const char *cp;
  struct expression *res;
};


 
#ifdef _LIBC
# define FREE_EXPRESSION __gettext_free_exp
# define PLURAL_PARSE __gettextparse
# define GERMANIC_PLURAL __gettext_germanic_plural
# define EXTRACT_PLURAL_EXPRESSION __gettext_extract_plural
#elif defined (IN_LIBINTL)
# define FREE_EXPRESSION libintl_gettext_free_exp
# define PLURAL_PARSE libintl_gettextparse
# define GERMANIC_PLURAL libintl_gettext_germanic_plural
# define EXTRACT_PLURAL_EXPRESSION libintl_gettext_extract_plural
#else
# define FREE_EXPRESSION free_plural_expression
# define PLURAL_PARSE parse_plural_expression
# define GERMANIC_PLURAL germanic_plural
# define EXTRACT_PLURAL_EXPRESSION extract_plural_expression
#endif

extern void FREE_EXPRESSION PARAMS ((struct expression *exp))
     internal_function;
extern int PLURAL_PARSE PARAMS ((void *arg));
extern struct expression GERMANIC_PLURAL attribute_hidden;
extern void EXTRACT_PLURAL_EXPRESSION PARAMS ((const char *nullentry,
					       struct expression **pluralp,
					       unsigned long int *npluralsp))
     internal_function;

#if !defined (_LIBC) && !defined (IN_LIBINTL)
extern unsigned long int plural_eval PARAMS ((struct expression *pexp,
					      unsigned long int n));
#endif

#endif  
