 

 

 

 

 

 
#define YYBISON 1

 
#define YYSKELETON_NAME "yacc.c"

 
#define YYPURE 1

 
#define YYLSP_NEEDED 0

 
#define yyparse __gettextparse
#define yylex   __gettextlex
#define yyerror __gettexterror
#define yylval  __gettextlval
#define yychar  __gettextchar
#define yydebug __gettextdebug
#define yynerrs __gettextnerrs


 
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
    
   enum yytokentype {
     EQUOP2 = 258,
     CMPOP2 = 259,
     ADDOP2 = 260,
     MULOP2 = 261,
     NUMBER = 262
   };
#endif
#define EQUOP2 258
#define CMPOP2 259
#define ADDOP2 260
#define MULOP2 261
#define NUMBER 262




 
#line 1 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"

 

 

 
#if defined _AIX && !defined __GNUC__
 #pragma alloca
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include "plural-exp.h"

 
#ifndef _LIBC
# define __gettextparse PLURAL_PARSE
#endif

#define YYLEX_PARAM	&((struct parse_args *) arg)->cp
#define YYPARSE_PARAM	arg


 
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

 
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 51 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
typedef union YYSTYPE {
  unsigned long int num;
  enum operator op;
  struct expression *exp;
} YYSTYPE;
 
#line 152 "/usr/src/local/bash/bash-20080814/lib/intl/plural.c"
# define yystype YYSTYPE  
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



 
#line 57 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"

 
static struct expression *new_exp PARAMS ((int nargs, enum operator op,
					   struct expression * const *args));
static inline struct expression *new_exp_0 PARAMS ((enum operator op));
static inline struct expression *new_exp_1 PARAMS ((enum operator op,
						   struct expression *right));
static struct expression *new_exp_2 PARAMS ((enum operator op,
					     struct expression *left,
					     struct expression *right));
static inline struct expression *new_exp_3 PARAMS ((enum operator op,
						   struct expression *bexp,
						   struct expression *tbranch,
						   struct expression *fbranch));
static int yylex PARAMS ((YYSTYPE *lval, const char **pexp));
static void yyerror PARAMS ((const char *str));

 

static struct expression *
new_exp (nargs, op, args)
     int nargs;
     enum operator op;
     struct expression * const *args;
{
  int i;
  struct expression *newp;

   
  for (i = nargs - 1; i >= 0; i--)
    if (args[i] == NULL)
      goto fail;

   
  newp = (struct expression *) malloc (sizeof (*newp));
  if (newp != NULL)
    {
      newp->nargs = nargs;
      newp->operation = op;
      for (i = nargs - 1; i >= 0; i--)
	newp->val.args[i] = args[i];
      return newp;
    }

 fail:
  for (i = nargs - 1; i >= 0; i--)
    FREE_EXPRESSION (args[i]);

  return NULL;
}

static inline struct expression *
new_exp_0 (op)
     enum operator op;
{
  return new_exp (0, op, NULL);
}

static inline struct expression *
new_exp_1 (op, right)
     enum operator op;
     struct expression *right;
{
  struct expression *args[1];

  args[0] = right;
  return new_exp (1, op, args);
}

static struct expression *
new_exp_2 (op, left, right)
     enum operator op;
     struct expression *left;
     struct expression *right;
{
  struct expression *args[2];

  args[0] = left;
  args[1] = right;
  return new_exp (2, op, args);
}

static inline struct expression *
new_exp_3 (op, bexp, tbranch, fbranch)
     enum operator op;
     struct expression *bexp;
     struct expression *tbranch;
     struct expression *fbranch;
{
  struct expression *args[3];

  args[0] = bexp;
  args[1] = tbranch;
  args[2] = fbranch;
  return new_exp (3, op, args);
}



 
#line 262 "/usr/src/local/bash/bash-20080814/lib/intl/plural.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC malloc
# endif

 

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
    
#  define YYSTACK_FREE(Ptr) do {  ; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h>  
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
# endif
#endif  


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

 
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

 
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

 
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

 
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

 
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

 
#define YYFINAL  9
 
#define YYLAST   54

 
#define YYNTOKENS  16
 
#define YYNNTS  3
 
#define YYNRULES  13
 
#define YYNSTATES  27

 
#define YYUNDEFTOK  2
#define YYMAXUTOK   262

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

 
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    10,     2,     2,     2,     2,     5,     2,
      14,    15,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    12,     2,
       2,     2,     2,     3,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      13,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     4,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     6,     7,
       8,     9,    11
};

#if YYDEBUG
 
static const unsigned char yyprhs[] =
{
       0,     0,     3,     5,    11,    15,    19,    23,    27,    31,
      35,    38,    40,    42
};

 
static const yysigned_char yyrhs[] =
{
      17,     0,    -1,    18,    -1,    18,     3,    18,    12,    18,
      -1,    18,     4,    18,    -1,    18,     5,    18,    -1,    18,
       6,    18,    -1,    18,     7,    18,    -1,    18,     8,    18,
      -1,    18,     9,    18,    -1,    10,    18,    -1,    13,    -1,
      11,    -1,    14,    18,    15,    -1
};

 
static const unsigned char yyrline[] =
{
       0,   176,   176,   184,   188,   192,   196,   200,   204,   208,
     212,   216,   220,   225
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
 
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "'?'", "'|'", "'&'", "EQUOP2", "CMPOP2",
  "ADDOP2", "MULOP2", "'!'", "NUMBER", "':'", "'n'", "'('", "')'",
  "$accept", "start", "exp", 0
};
#endif

# ifdef YYPRINT
 
static const unsigned short int yytoknum[] =
{
       0,   256,   257,    63,   124,    38,   258,   259,   260,   261,
      33,   262,    58,   110,    40,    41
};
# endif

 
static const unsigned char yyr1[] =
{
       0,    16,    17,    18,    18,    18,    18,    18,    18,    18,
      18,    18,    18,    18
};

 
static const unsigned char yyr2[] =
{
       0,     2,     1,     5,     3,     3,     3,     3,     3,     3,
       2,     1,     1,     3
};

 
static const unsigned char yydefact[] =
{
       0,     0,    12,    11,     0,     0,     2,    10,     0,     1,
       0,     0,     0,     0,     0,     0,     0,    13,     0,     4,
       5,     6,     7,     8,     9,     0,     3
};

 
static const yysigned_char yydefgoto[] =
{
      -1,     5,     6
};

 
#define YYPACT_NINF -10
static const yysigned_char yypact[] =
{
      -9,    -9,   -10,   -10,    -9,     8,    36,   -10,    13,   -10,
      -9,    -9,    -9,    -9,    -9,    -9,    -9,   -10,    26,    41,
      45,    18,    -2,    14,   -10,    -9,    36
};

 
static const yysigned_char yypgoto[] =
{
     -10,   -10,    -1
};

 
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
       7,     1,     2,     8,     3,     4,    15,    16,     9,    18,
      19,    20,    21,    22,    23,    24,    10,    11,    12,    13,
      14,    15,    16,    16,    26,    14,    15,    16,    17,    10,
      11,    12,    13,    14,    15,    16,     0,     0,    25,    10,
      11,    12,    13,    14,    15,    16,    12,    13,    14,    15,
      16,    13,    14,    15,    16
};

static const yysigned_char yycheck[] =
{
       1,    10,    11,     4,    13,    14,     8,     9,     0,    10,
      11,    12,    13,    14,    15,    16,     3,     4,     5,     6,
       7,     8,     9,     9,    25,     7,     8,     9,    15,     3,
       4,     5,     6,     7,     8,     9,    -1,    -1,    12,     3,
       4,     5,     6,     7,     8,     9,     5,     6,     7,     8,
       9,     6,     7,     8,     9
};

 
static const unsigned char yystos[] =
{
       0,    10,    11,    13,    14,    17,    18,    18,    18,     0,
       3,     4,     5,     6,     7,     8,     9,    15,    18,    18,
      18,    18,    18,    18,    18,    12,    18
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h>  
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


 

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


 

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


 

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


 

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
#endif

 
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h>  
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Type, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

 

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for ( ; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


 

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
   
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

 
int yydebug;
#else  
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif  


 
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

 

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
 
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
 
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif  



#if YYDEBUG
 

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
   
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif  
 

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
   
  (void) yyvaluep;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}


 

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else  
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif  






 

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else  
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
   
int yychar;

 
YYSTYPE yylval;

 
int yynerrs;

  register int yystate;
  register int yyn;
  int yyresult;
   
  int yyerrstatus;
   
  int yytoken = 0;

   

   
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  register short int *yyssp;

   
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

   
  YYSTYPE yyval;


   
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		 

   

  yyssp = yyss;
  yyvsp = yyvs;


  yyvsp[0] = yylval;

  goto yysetstate;

 
 yynewstate:
   
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
       
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	 
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


	 
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else  
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
       
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif  

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

 
yybackup:

 
 
 

   

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

   

   
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

   
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

   
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

   
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


   
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


 
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


 
yyreduce:
   
  yylen = yyr2[yyn];

   
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 177 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    if ((yyvsp[0].exp) == NULL)
	      YYABORT;
	    ((struct parse_args *) arg)->res = (yyvsp[0].exp);
	  }
    break;

  case 3:
#line 185 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_3 (qmop, (yyvsp[-4].exp), (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 4:
#line 189 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_2 (lor, (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 5:
#line 193 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_2 (land, (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 6:
#line 197 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_2 ((yyvsp[-1].op), (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 7:
#line 201 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_2 ((yyvsp[-1].op), (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 8:
#line 205 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_2 ((yyvsp[-1].op), (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 9:
#line 209 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_2 ((yyvsp[-1].op), (yyvsp[-2].exp), (yyvsp[0].exp));
	  }
    break;

  case 10:
#line 213 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_1 (lnot, (yyvsp[0].exp));
	  }
    break;

  case 11:
#line 217 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = new_exp_0 (var);
	  }
    break;

  case 12:
#line 221 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    if (((yyval.exp) = new_exp_0 (num)) != NULL)
	      (yyval.exp)->val.num = (yyvsp[0].num);
	  }
    break;

  case 13:
#line 226 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"
    {
	    (yyval.exp) = (yyvsp[-1].exp);
	  }
    break;


    }

 
#line 1270 "/usr/src/local/bash/bash-20080814/lib/intl/plural.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


   

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


 
yyerrlab:
   
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	   
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	   
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif  
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
       

      if (yychar <= YYEOF)
        {
           
	  if (yychar == YYEOF)
	     for (;;)
	       {

		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 yydestruct ("Error: popping",
                             yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

   
  goto yyerrlab1;


 
yyerrorlab:

#ifdef __GNUC__
   
  if (0)
     goto yyerrorlab;
#endif

yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


 
yyerrlab1:
  yyerrstatus = 3;	 

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

       
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


   
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


 
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

 
yyabortlab:
  yydestruct ("Error: discarding lookahead",
              yytoken, &yylval);
  yychar = YYEMPTY;
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
 
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
   
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 231 "/usr/src/local/bash/bash-20080814/lib/intl/plural.y"


void
internal_function
FREE_EXPRESSION (exp)
     struct expression *exp;
{
  if (exp == NULL)
    return;

   
  switch (exp->nargs)
    {
    case 3:
      FREE_EXPRESSION (exp->val.args[2]);
       
    case 2:
      FREE_EXPRESSION (exp->val.args[1]);
       
    case 1:
      FREE_EXPRESSION (exp->val.args[0]);
       
    default:
      break;
    }

  free (exp);
}


static int
yylex (lval, pexp)
     YYSTYPE *lval;
     const char **pexp;
{
  const char *exp = *pexp;
  int result;

  while (1)
    {
      if (exp[0] == '\0')
	{
	  *pexp = exp;
	  return YYEOF;
	}

      if (exp[0] != ' ' && exp[0] != '\t')
	break;

      ++exp;
    }

  result = *exp++;
  switch (result)
    {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      {
	unsigned long int n = result - '0';
	while (exp[0] >= '0' && exp[0] <= '9')
	  {
	    n *= 10;
	    n += exp[0] - '0';
	    ++exp;
	  }
	lval->num = n;
	result = NUMBER;
      }
      break;

    case '=':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = equal;
	  result = EQUOP2;
	}
      else
	result = YYERRCODE;
      break;

    case '!':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = not_equal;
	  result = EQUOP2;
	}
      break;

    case '&':
    case '|':
      if (exp[0] == result)
	++exp;
      else
	result = YYERRCODE;
      break;

    case '<':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = less_or_equal;
	}
      else
	lval->op = less_than;
      result = CMPOP2;
      break;

    case '>':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = greater_or_equal;
	}
      else
	lval->op = greater_than;
      result = CMPOP2;
      break;

    case '*':
      lval->op = mult;
      result = MULOP2;
      break;

    case '/':
      lval->op = divide;
      result = MULOP2;
      break;

    case '%':
      lval->op = module;
      result = MULOP2;
      break;

    case '+':
      lval->op = plus;
      result = ADDOP2;
      break;

    case '-':
      lval->op = minus;
      result = ADDOP2;
      break;

    case 'n':
    case '?':
    case ':':
    case '(':
    case ')':
       
      break;

    case ';':
    case '\n':
    case '\0':
       
      --exp;
      result = YYEOF;
      break;

    default:
      result = YYERRCODE;
#if YYDEBUG != 0
      --exp;
#endif
      break;
    }

  *pexp = exp;

  return result;
}


static void
yyerror (str)
     const char *str;
{
   
}
