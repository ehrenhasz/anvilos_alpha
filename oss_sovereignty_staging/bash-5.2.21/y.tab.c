 

 

 

 

 

 
#define YYBISON 30802

 
#define YYBISON_VERSION "3.8.2"

 
#define YYSKELETON_NAME "yacc.c"

 
#define YYPURE 0

 
#define YYPUSH 0

 
#define YYPULL 1




 
#line 21 "/usr/local/src/chet/src/bash/src/parse.y"

#include "config.h"

#include "bashtypes.h"
#include "bashansi.h"

#include "filecntl.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_LOCALE_H)
#  include <locale.h>
#endif

#include <stdio.h>
#include "chartypes.h"
#include <signal.h>

#include "memalloc.h"

#include "bashintl.h"

#define NEED_STRFTIME_DECL	 

#include "shell.h"
#include "execute_cmd.h"
#include "typemax.h"		 
#include "trap.h"
#include "flags.h"
#include "parser.h"
#include "mailcheck.h"
#include "test.h"
#include "builtins.h"
#include "builtins/common.h"
#include "builtins/builtext.h"

#include "shmbutil.h"

#if defined (READLINE)
#  include "bashline.h"
#  include <readline/readline.h>
#endif  

#if defined (HISTORY)
#  include "bashhist.h"
#  include <readline/history.h>
#endif  

#if defined (JOB_CONTROL)
#  include "jobs.h"
#else
extern int cleanup_dead_jobs PARAMS((void));
#endif  

#if defined (ALIAS)
#  include "alias.h"
#else
typedef void *alias_t;
#endif  

#if defined (PROMPT_STRING_DECODE)
#  ifndef _MINIX
#    include <sys/param.h>
#  endif
#  include <time.h>
#  if defined (TM_IN_SYS_TIME)
#    include <sys/types.h>
#    include <sys/time.h>
#  endif  
#  include "maxpath.h"
#endif  

#define RE_READ_TOKEN	-99
#define NO_EXPANSION	-100

#define END_ALIAS	-2

#ifdef DEBUG
#  define YYDEBUG 1
#else
#  define YYDEBUG 0
#endif

#if defined (HANDLE_MULTIBYTE)
#  define last_shell_getc_is_singlebyte \
	((shell_input_line_index > 1) \
		? shell_input_line_property[shell_input_line_index - 1] \
		: 1)
#  define MBTEST(x)	((x) && last_shell_getc_is_singlebyte)
#else
#  define last_shell_getc_is_singlebyte	1
#  define MBTEST(x)	((x))
#endif

#define EXTEND_SHELL_INPUT_LINE_PROPERTY() \
do { \
    if (shell_input_line_len + 2 > shell_input_line_propsize) \
      { \
	shell_input_line_propsize = shell_input_line_len + 2; \
	shell_input_line_property = (char *)xrealloc (shell_input_line_property, \
				    shell_input_line_propsize); \
      } \
} while (0)

#if defined (EXTENDED_GLOB)
extern int extended_glob, extglob_flag;
#endif

#if defined (TRANSLATABLE_STRINGS)
extern int dump_translatable_strings, dump_po_strings;
extern int singlequote_translations;
#endif  

#if !defined (errno)
extern int errno;
#endif

 
 
 
 
 

#ifdef DEBUG
static void debug_parser PARAMS((int));
#endif

static int yy_getc PARAMS((void));
static int yy_ungetc PARAMS((int));

#if defined (READLINE)
static int yy_readline_get PARAMS((void));
static int yy_readline_unget PARAMS((int));
#endif

static int yy_string_get PARAMS((void));
static int yy_string_unget PARAMS((int));
static int yy_stream_get PARAMS((void));
static int yy_stream_unget PARAMS((int));

static int shell_getc PARAMS((int));
static void shell_ungetc PARAMS((int));
static void discard_until PARAMS((int));

static void push_string PARAMS((char *, int, alias_t *));
static void pop_string PARAMS((void));
static void free_string_list PARAMS((void));

static char *read_a_line PARAMS((int));

static int reserved_word_acceptable PARAMS((int));
static int yylex PARAMS((void));

static void push_heredoc PARAMS((REDIRECT *));
static char *mk_alexpansion PARAMS((char *));
static int alias_expand_token PARAMS((char *));
static int time_command_acceptable PARAMS((void));
static int special_case_tokens PARAMS((char *));
static int read_token PARAMS((int));
static char *parse_matched_pair PARAMS((int, int, int, int *, int));
static char *parse_comsub PARAMS((int, int, int, int *, int));
#if defined (ARRAY_VARS)
static char *parse_compound_assignment PARAMS((int *));
#endif
#if defined (DPAREN_ARITHMETIC) || defined (ARITH_FOR_COMMAND)
static int parse_dparen PARAMS((int));
static int parse_arith_cmd PARAMS((char **, int));
#endif
#if defined (COND_COMMAND)
static void cond_error PARAMS((void));
static COND_COM *cond_expr PARAMS((void));
static COND_COM *cond_or PARAMS((void));
static COND_COM *cond_and PARAMS((void));
static COND_COM *cond_term PARAMS((void));
static int cond_skip_newlines PARAMS((void));
static COMMAND *parse_cond_command PARAMS((void));
#endif
#if defined (ARRAY_VARS)
static int token_is_assignment PARAMS((char *, int));
static int token_is_ident PARAMS((char *, int));
#endif
static int read_token_word PARAMS((int));
static void discard_parser_constructs PARAMS((int));

static char *error_token_from_token PARAMS((int));
static char *error_token_from_text PARAMS((void));
static void print_offending_line PARAMS((void));
static void report_syntax_error PARAMS((char *));

static void handle_eof_input_unit PARAMS((void));
static void prompt_again PARAMS((int));
#if 0
static void reset_readline_prompt PARAMS((void));
#endif
static void print_prompt PARAMS((void));

#if defined (HANDLE_MULTIBYTE)
static void set_line_mbstate PARAMS((void));
static char *shell_input_line_property = NULL;
static size_t shell_input_line_propsize = 0;
#else
#  define set_line_mbstate()
#endif

extern int yyerror PARAMS((const char *));

#ifdef DEBUG
extern int yydebug;
#endif

 
char *primary_prompt = PPROMPT;
char *secondary_prompt = SPROMPT;

 
char *ps1_prompt, *ps2_prompt;

 
char *ps0_prompt;

 
char **prompt_string_pointer = (char **)NULL;
char *current_prompt_string;

 
int expand_aliases = 0;

 
int promptvars = 1;

 
int extended_quote = 1;

 
int current_command_line_count;

 
int saved_command_line_count;

 
int shell_eof_token;

 
int current_token;

 
int parser_state;

 
static REDIRECT *redir_stack[HEREDOC_MAX];
int need_here_doc;

 
static char *shell_input_line = (char *)NULL;
static size_t shell_input_line_index;
static size_t shell_input_line_size;	 
static size_t shell_input_line_len;	 

 
static int shell_input_line_terminator;

 
static int function_dstart;

 
static int function_bstart;

 
static int arith_for_lineno;

 
static char *current_decoded_prompt;

 
static int last_read_token;

 
static int token_before_that;

 
static int two_tokens_ago;

static int global_extglob;

 
#define MAX_CASE_NEST	128
static int word_lineno[MAX_CASE_NEST+1];
static int word_top = -1;

 
static int token_to_read;
static WORD_DESC *word_desc_to_read;

static REDIRECTEE source;
static REDIRECTEE redir;

static FILE *yyoutstream;
static FILE *yyerrstream;

#line 388 "y.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

 
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
 
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

 
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                      
    YYerror = 256,                  
    YYUNDEF = 257,                  
    IF = 258,                       
    THEN = 259,                     
    ELSE = 260,                     
    ELIF = 261,                     
    FI = 262,                       
    CASE = 263,                     
    ESAC = 264,                     
    FOR = 265,                      
    SELECT = 266,                   
    WHILE = 267,                    
    UNTIL = 268,                    
    DO = 269,                       
    DONE = 270,                     
    FUNCTION = 271,                 
    COPROC = 272,                   
    COND_START = 273,               
    COND_END = 274,                 
    COND_ERROR = 275,               
    IN = 276,                       
    BANG = 277,                     
    TIME = 278,                     
    TIMEOPT = 279,                  
    TIMEIGN = 280,                  
    WORD = 281,                     
    ASSIGNMENT_WORD = 282,          
    REDIR_WORD = 283,               
    NUMBER = 284,                   
    ARITH_CMD = 285,                
    ARITH_FOR_EXPRS = 286,          
    COND_CMD = 287,                 
    AND_AND = 288,                  
    OR_OR = 289,                    
    GREATER_GREATER = 290,          
    LESS_LESS = 291,                
    LESS_AND = 292,                 
    LESS_LESS_LESS = 293,           
    GREATER_AND = 294,              
    SEMI_SEMI = 295,                
    SEMI_AND = 296,                 
    SEMI_SEMI_AND = 297,            
    LESS_LESS_MINUS = 298,          
    AND_GREATER = 299,              
    AND_GREATER_GREATER = 300,      
    LESS_GREATER = 301,             
    GREATER_BAR = 302,              
    BAR_AND = 303,                  
    DOLPAREN = 304,                 
    yacc_EOF = 305                  
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
 
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define IF 258
#define THEN 259
#define ELSE 260
#define ELIF 261
#define FI 262
#define CASE 263
#define ESAC 264
#define FOR 265
#define SELECT 266
#define WHILE 267
#define UNTIL 268
#define DO 269
#define DONE 270
#define FUNCTION 271
#define COPROC 272
#define COND_START 273
#define COND_END 274
#define COND_ERROR 275
#define IN 276
#define BANG 277
#define TIME 278
#define TIMEOPT 279
#define TIMEIGN 280
#define WORD 281
#define ASSIGNMENT_WORD 282
#define REDIR_WORD 283
#define NUMBER 284
#define ARITH_CMD 285
#define ARITH_FOR_EXPRS 286
#define COND_CMD 287
#define AND_AND 288
#define OR_OR 289
#define GREATER_GREATER 290
#define LESS_LESS 291
#define LESS_AND 292
#define LESS_LESS_LESS 293
#define GREATER_AND 294
#define SEMI_SEMI 295
#define SEMI_AND 296
#define SEMI_SEMI_AND 297
#define LESS_LESS_MINUS 298
#define AND_GREATER 299
#define AND_GREATER_GREATER 300
#define LESS_GREATER 301
#define GREATER_BAR 302
#define BAR_AND 303
#define DOLPAREN 304
#define yacc_EOF 305

 
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 338 "/usr/local/src/chet/src/bash/src/parse.y"

  WORD_DESC *word;		 
  int number;			 
  WORD_LIST *word_list;
  COMMAND *command;
  REDIRECT *redirect;
  ELEMENT element;
  PATTERN_LIST *pattern;

#line 551 "y.tab.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif  
 
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                       
  YYSYMBOL_YYerror = 1,                     
  YYSYMBOL_YYUNDEF = 2,                     
  YYSYMBOL_IF = 3,                          
  YYSYMBOL_THEN = 4,                        
  YYSYMBOL_ELSE = 5,                        
  YYSYMBOL_ELIF = 6,                        
  YYSYMBOL_FI = 7,                          
  YYSYMBOL_CASE = 8,                        
  YYSYMBOL_ESAC = 9,                        
  YYSYMBOL_FOR = 10,                        
  YYSYMBOL_SELECT = 11,                     
  YYSYMBOL_WHILE = 12,                      
  YYSYMBOL_UNTIL = 13,                      
  YYSYMBOL_DO = 14,                         
  YYSYMBOL_DONE = 15,                       
  YYSYMBOL_FUNCTION = 16,                   
  YYSYMBOL_COPROC = 17,                     
  YYSYMBOL_COND_START = 18,                 
  YYSYMBOL_COND_END = 19,                   
  YYSYMBOL_COND_ERROR = 20,                 
  YYSYMBOL_IN = 21,                         
  YYSYMBOL_BANG = 22,                       
  YYSYMBOL_TIME = 23,                       
  YYSYMBOL_TIMEOPT = 24,                    
  YYSYMBOL_TIMEIGN = 25,                    
  YYSYMBOL_WORD = 26,                       
  YYSYMBOL_ASSIGNMENT_WORD = 27,            
  YYSYMBOL_REDIR_WORD = 28,                 
  YYSYMBOL_NUMBER = 29,                     
  YYSYMBOL_ARITH_CMD = 30,                  
  YYSYMBOL_ARITH_FOR_EXPRS = 31,            
  YYSYMBOL_COND_CMD = 32,                   
  YYSYMBOL_AND_AND = 33,                    
  YYSYMBOL_OR_OR = 34,                      
  YYSYMBOL_GREATER_GREATER = 35,            
  YYSYMBOL_LESS_LESS = 36,                  
  YYSYMBOL_LESS_AND = 37,                   
  YYSYMBOL_LESS_LESS_LESS = 38,             
  YYSYMBOL_GREATER_AND = 39,                
  YYSYMBOL_SEMI_SEMI = 40,                  
  YYSYMBOL_SEMI_AND = 41,                   
  YYSYMBOL_SEMI_SEMI_AND = 42,              
  YYSYMBOL_LESS_LESS_MINUS = 43,            
  YYSYMBOL_AND_GREATER = 44,                
  YYSYMBOL_AND_GREATER_GREATER = 45,        
  YYSYMBOL_LESS_GREATER = 46,               
  YYSYMBOL_GREATER_BAR = 47,                
  YYSYMBOL_BAR_AND = 48,                    
  YYSYMBOL_DOLPAREN = 49,                   
  YYSYMBOL_50_ = 50,                        
  YYSYMBOL_51_ = 51,                        
  YYSYMBOL_52_n_ = 52,                      
  YYSYMBOL_yacc_EOF = 53,                   
  YYSYMBOL_54_ = 54,                        
  YYSYMBOL_55_ = 55,                        
  YYSYMBOL_56_ = 56,                        
  YYSYMBOL_57_ = 57,                        
  YYSYMBOL_58_ = 58,                        
  YYSYMBOL_59_ = 59,                        
  YYSYMBOL_60_ = 60,                        
  YYSYMBOL_61_ = 61,                        
  YYSYMBOL_YYACCEPT = 62,                   
  YYSYMBOL_inputunit = 63,                  
  YYSYMBOL_word_list = 64,                  
  YYSYMBOL_redirection = 65,                
  YYSYMBOL_simple_command_element = 66,     
  YYSYMBOL_redirection_list = 67,           
  YYSYMBOL_simple_command = 68,             
  YYSYMBOL_command = 69,                    
  YYSYMBOL_shell_command = 70,              
  YYSYMBOL_for_command = 71,                
  YYSYMBOL_arith_for_command = 72,          
  YYSYMBOL_select_command = 73,             
  YYSYMBOL_case_command = 74,               
  YYSYMBOL_function_def = 75,               
  YYSYMBOL_function_body = 76,              
  YYSYMBOL_subshell = 77,                   
  YYSYMBOL_comsub = 78,                     
  YYSYMBOL_coproc = 79,                     
  YYSYMBOL_if_command = 80,                 
  YYSYMBOL_group_command = 81,              
  YYSYMBOL_arith_command = 82,              
  YYSYMBOL_cond_command = 83,               
  YYSYMBOL_elif_clause = 84,                
  YYSYMBOL_case_clause = 85,                
  YYSYMBOL_pattern_list = 86,               
  YYSYMBOL_case_clause_sequence = 87,       
  YYSYMBOL_pattern = 88,                    
  YYSYMBOL_compound_list = 89,              
  YYSYMBOL_list0 = 90,                      
  YYSYMBOL_list1 = 91,                      
  YYSYMBOL_simple_list_terminator = 92,     
  YYSYMBOL_list_terminator = 93,            
  YYSYMBOL_newline_list = 94,               
  YYSYMBOL_simple_list = 95,                
  YYSYMBOL_simple_list1 = 96,               
  YYSYMBOL_pipeline_command = 97,           
  YYSYMBOL_pipeline = 98,                   
  YYSYMBOL_timespec = 99                    
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

 

#ifndef __PTRDIFF_MAX__
# include <limits.h>  
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h>  
#  define YY_STDINT_H
# endif
#endif

 

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

 
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h>  
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


 
typedef yytype_int16 yy_state_t;

 
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h>  
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

 
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E)  
#endif

 
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value)  
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

 

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h>  
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h>  
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h>  
       
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
    
#  define YYSTACK_FREE(Ptr) do {  ; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
     
#   define YYSTACK_ALLOC_MAXIMUM 4032  
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h>  
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T);  
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *);  
#   endif
#  endif
# endif
#endif  

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

 
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

 
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

 
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

 
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
 
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif  

 
#define YYFINAL  122
 
#define YYLAST   740

 
#define YYNTOKENS  62
 
#define YYNNTS  38
 
#define YYNRULES  175
 
#define YYNSTATES  350

 
#define YYMAXUTOK   305


 
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

 
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      52,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    50,     2,
      60,    61,     2,     2,     2,    57,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    51,
      56,     2,    55,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    58,    54,    59,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    53
};

#if YYDEBUG
 
static const yytype_int16 yyrline[] =
{
       0,   395,   395,   406,   414,   423,   438,   455,   470,   480,
     482,   486,   492,   498,   504,   510,   516,   522,   528,   534,
     540,   546,   552,   558,   564,   570,   576,   583,   590,   597,
     604,   611,   618,   624,   630,   636,   642,   648,   654,   660,
     666,   672,   678,   684,   690,   696,   702,   708,   714,   720,
     726,   732,   738,   744,   750,   758,   760,   762,   766,   770,
     781,   783,   787,   789,   791,   807,   809,   813,   815,   817,
     819,   821,   823,   825,   827,   829,   831,   833,   837,   842,
     847,   852,   857,   862,   867,   872,   879,   885,   891,   897,
     905,   910,   915,   920,   925,   930,   935,   940,   947,   952,
     957,   964,   966,   968,   970,   974,   976,  1007,  1014,  1018,
    1024,  1029,  1046,  1051,  1068,  1075,  1077,  1079,  1084,  1088,
    1092,  1096,  1098,  1100,  1104,  1105,  1109,  1111,  1113,  1115,
    1119,  1121,  1123,  1125,  1127,  1129,  1133,  1135,  1144,  1150,
    1156,  1157,  1164,  1168,  1170,  1172,  1179,  1181,  1188,  1192,
    1193,  1196,  1198,  1200,  1204,  1205,  1214,  1229,  1247,  1264,
    1266,  1268,  1275,  1278,  1282,  1284,  1290,  1296,  1316,  1339,
    1341,  1364,  1368,  1370,  1372,  1374
};
#endif

 
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
 
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

 
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "IF", "THEN", "ELSE",
  "ELIF", "FI", "CASE", "ESAC", "FOR", "SELECT", "WHILE", "UNTIL", "DO",
  "DONE", "FUNCTION", "COPROC", "COND_START", "COND_END", "COND_ERROR",
  "IN", "BANG", "TIME", "TIMEOPT", "TIMEIGN", "WORD", "ASSIGNMENT_WORD",
  "REDIR_WORD", "NUMBER", "ARITH_CMD", "ARITH_FOR_EXPRS", "COND_CMD",
  "AND_AND", "OR_OR", "GREATER_GREATER", "LESS_LESS", "LESS_AND",
  "LESS_LESS_LESS", "GREATER_AND", "SEMI_SEMI", "SEMI_AND",
  "SEMI_SEMI_AND", "LESS_LESS_MINUS", "AND_GREATER", "AND_GREATER_GREATER",
  "LESS_GREATER", "GREATER_BAR", "BAR_AND", "DOLPAREN", "'&'", "';'",
  "'\\n'", "yacc_EOF", "'|'", "'>'", "'<'", "'-'", "'{'", "'}'", "'('",
  "')'", "$accept", "inputunit", "word_list", "redirection",
  "simple_command_element", "redirection_list", "simple_command",
  "command", "shell_command", "for_command", "arith_for_command",
  "select_command", "case_command", "function_def", "function_body",
  "subshell", "comsub", "coproc", "if_command", "group_command",
  "arith_command", "cond_command", "elif_clause", "case_clause",
  "pattern_list", "case_clause_sequence", "pattern", "compound_list",
  "list0", "list1", "simple_list_terminator", "list_terminator",
  "newline_list", "simple_list", "simple_list1", "pipeline_command",
  "pipeline", "timespec", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-125)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

 
static const yytype_int16 yypact[] =
{
     329,    27,  -125,     8,    81,    10,  -125,  -125,    16,    38,
       0,   434,    -5,   -16,  -125,   670,   684,  -125,    33,    43,
      62,    63,    71,    69,    94,   105,   108,   116,  -125,  -125,
    -125,   125,   139,  -125,  -125,   111,  -125,  -125,   626,  -125,
     648,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,  -125,  -125,     5,   -21,  -125,   -15,   434,  -125,  -125,
    -125,   196,   485,  -125,   157,     2,   180,   207,   222,   227,
     638,   626,   648,   224,  -125,  -125,  -125,  -125,  -125,   219,
    -125,   185,   223,   228,   140,   230,   161,   232,   233,   234,
     236,   241,   248,   249,   162,   250,   163,   251,   254,   256,
     257,   258,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,  -125,  -125,  -125,  -125,  -125,   225,   380,  -125,  -125,
     229,   231,  -125,  -125,  -125,  -125,   648,  -125,  -125,  -125,
    -125,  -125,   536,   536,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,   214,  -125,    -7,  -125,    85,  -125,  -125,  -125,  -125,
      89,  -125,  -125,  -125,   235,   648,  -125,   648,   648,  -125,
    -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,  -125,  -125,  -125,  -125,   485,   485,   138,   138,   587,
     587,    17,  -125,  -125,  -125,  -125,  -125,  -125,    88,  -125,
     122,  -125,   274,   238,   100,   101,  -125,   122,  -125,   276,
     278,   260,  -125,   648,   648,   260,  -125,  -125,   -15,   -15,
    -125,  -125,  -125,   287,   485,   485,   485,   485,   485,   290,
     164,  -125,    26,  -125,  -125,   285,  -125,   131,  -125,   242,
    -125,  -125,  -125,  -125,  -125,  -125,   288,   131,  -125,   243,
    -125,  -125,  -125,   260,  -125,   297,   302,  -125,  -125,  -125,
     152,   152,   152,  -125,  -125,  -125,  -125,   170,    61,  -125,
    -125,   281,   -36,   293,   252,  -125,  -125,  -125,   102,  -125,
     298,   255,   300,   262,  -125,  -125,   103,  -125,  -125,  -125,
    -125,  -125,  -125,  -125,  -125,   -33,   296,  -125,  -125,  -125,
     110,  -125,  -125,  -125,  -125,  -125,  -125,   112,  -125,  -125,
     189,  -125,  -125,  -125,   485,  -125,  -125,   310,   267,  -125,
    -125,   314,   275,  -125,  -125,  -125,   485,   318,   277,  -125,
    -125,   320,   279,  -125,  -125,  -125,  -125,  -125,  -125,  -125
};

 
static const yytype_uint8 yydefact[] =
{
       0,     0,   154,     0,     0,     0,   154,   154,     0,     0,
       0,     0,   172,    55,    56,     0,     0,   119,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   154,     4,
       8,     0,     0,   154,   154,     0,    57,    60,    62,   171,
      63,    67,    77,    71,    68,    65,    73,     3,    66,    72,
      74,    75,    76,     0,   156,   163,   164,     0,     7,     5,
       6,     0,     0,   154,   154,     0,   154,     0,     0,     0,
      55,   114,   110,     0,   152,   151,   153,   168,   165,   173,
     174,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    17,    26,    41,    35,    50,    32,    44,    38,
      47,    29,    53,    54,    23,    20,     0,     0,    11,    12,
       0,     0,     1,    55,    61,    58,    64,   149,   150,     2,
     154,   154,   157,   158,   154,   154,   167,   166,   154,   155,
     138,   139,   148,     0,   154,     0,   154,   154,   154,   154,
       0,   154,   154,   154,   154,   105,   103,   112,   111,   120,
     175,   154,    19,    28,    43,    37,    52,    34,    46,    40,
      49,    31,    25,    22,    15,    16,    18,    27,    42,    36,
      51,    33,    45,    39,    48,    30,    24,    21,    13,    14,
     108,   109,   118,   107,    59,     0,     0,   161,   162,     0,
       0,     0,   154,   154,   154,   154,   154,   154,     0,   154,
       0,   154,     0,     0,     0,     0,   154,     0,   154,     0,
       0,     0,   154,   106,   113,     0,   159,   160,   170,   169,
     154,   154,   115,     0,     0,     0,   141,   142,   140,     0,
     124,   154,     0,   154,   154,     0,     9,     0,   154,     0,
      88,    89,   154,   154,   154,   154,     0,     0,   154,     0,
      69,    70,   104,     0,   101,     0,     0,   117,   143,   144,
     145,   146,   147,   100,   130,   132,   134,   125,     0,    98,
     136,     0,     0,     0,     0,    78,    10,   154,     0,    79,
       0,     0,     0,     0,    90,   154,     0,    91,   102,   116,
     154,   131,   133,   135,    99,     0,     0,   154,    80,    81,
       0,   154,   154,    86,    87,    92,    93,     0,   154,   154,
     121,   154,   137,   126,   127,   154,   154,     0,     0,   154,
     154,     0,     0,   154,   123,   128,   129,     0,     0,    84,
      85,     0,     0,    96,    97,   122,    82,    83,    94,    95
};

 
static const yytype_int16 yypgoto[] =
{
    -125,  -125,   126,   -25,   -28,   -65,   335,  -125,    -8,  -125,
    -125,  -125,  -125,  -125,   -96,  -125,  -125,  -125,  -125,  -125,
    -125,  -125,    28,  -125,   109,  -125,    68,    -2,  -125,   -11,
    -125,   -54,   -26,  -125,  -124,     6,    34,  -125
};

 
static const yytype_int16 yydefgoto[] =
{
       0,    35,   247,    36,    37,   126,    38,    39,    40,    41,
      42,    43,    44,    45,   156,    46,    47,    48,    49,    50,
      51,    52,   233,   239,   240,   241,   282,   121,   140,   141,
     129,    77,    62,    53,    54,   142,    56,    57
};

 
static const yytype_int16 yytable[] =
{
      61,    72,   117,   136,    67,    68,    55,   158,   197,   198,
     124,   148,   130,   131,   207,   125,   146,    78,   306,    79,
      80,   306,   230,   231,   232,   307,   116,    58,   321,   132,
     133,   120,    73,   134,    63,   279,    66,   143,   145,   135,
     150,     2,    69,   124,    81,   139,     3,   125,     4,     5,
       6,     7,   280,    74,    75,    76,    10,   127,   128,   102,
     147,   155,   157,   137,    70,    14,    15,    16,    17,   103,
     304,   226,   227,    18,    19,    20,    21,    22,   139,    59,
      60,    23,    24,    25,    26,    27,   281,   280,   104,   107,
     223,   105,   224,    31,    32,   111,    33,   108,    34,   209,
     109,   194,   243,   216,   195,   196,   210,    64,   199,   200,
     217,   122,    65,   139,   252,   254,   311,   318,   208,   106,
     112,   281,   214,   215,   325,   262,   329,   221,   110,   264,
     125,   113,   125,   194,   114,   225,   201,   139,    55,    55,
     139,   139,   115,   211,   212,   213,   244,   218,   246,   219,
     220,   118,   139,   139,   139,   139,   248,   286,   253,   255,
     312,   319,   139,   258,   139,   119,   164,   298,   326,   165,
     330,   130,   131,    74,    75,    76,   234,   235,   236,   237,
     238,   242,    74,    75,    76,   202,   203,   168,   178,   182,
     169,   179,   183,   287,   333,   231,   263,   166,   194,   194,
     138,    55,    55,   295,   274,   275,   276,   245,   144,   249,
     301,   302,   303,   155,   256,   278,   259,   155,   170,   180,
     184,   151,   288,   268,   269,   270,   271,   272,   265,   266,
       2,   149,   296,   228,   229,     3,   152,     4,     5,     6,
       7,   283,   284,   159,   160,    10,   161,   202,   203,   162,
     290,   291,   292,   293,   163,   155,   167,    17,   171,   172,
     173,   310,   174,     2,   204,   205,   206,   175,     3,   317,
       4,     5,     6,     7,   176,   177,   181,   185,    10,   153,
     186,   324,   187,   188,   189,    33,   190,   154,   192,   250,
      17,   260,   193,   261,   267,   336,   222,   251,   320,   273,
     285,   289,   297,   294,   299,   323,   300,   280,   308,   327,
     328,   309,   139,   313,   314,   315,   331,   332,    33,   335,
      34,   316,   322,   337,   338,   339,   340,   341,   342,   343,
       1,   345,     2,   346,   344,   348,   347,     3,   349,     4,
       5,     6,     7,   257,    71,     8,     9,    10,   334,   305,
     277,    11,    12,     0,     0,    13,    14,    15,    16,    17,
       0,     0,     0,     0,    18,    19,    20,    21,    22,     0,
       0,     0,    23,    24,    25,    26,    27,     0,    28,     0,
       0,    29,    30,     2,    31,    32,     0,    33,     3,    34,
       4,     5,     6,     7,     0,     0,     8,     9,    10,     0,
       0,     0,    11,    12,     0,     0,    13,    14,    15,    16,
      17,     0,     0,     0,     0,    18,    19,    20,    21,    22,
       0,     0,     0,    23,    24,    25,    26,    27,     0,     0,
       0,     0,   139,     0,     0,    31,    32,     2,    33,     0,
      34,   191,     3,     0,     4,     5,     6,     7,     0,     0,
       8,     9,    10,     0,     0,     0,    11,    12,     0,     0,
      13,    14,    15,    16,    17,     0,     0,     0,     0,    18,
      19,    20,    21,    22,     0,     0,     0,    23,    24,    25,
      26,    27,     0,     0,     0,    74,    75,    76,     2,    31,
      32,     0,    33,     3,    34,     4,     5,     6,     7,     0,
       0,     8,     9,    10,     0,     0,     0,    11,    12,     0,
       0,    13,    14,    15,    16,    17,     0,     0,     0,     0,
      18,    19,    20,    21,    22,     0,     0,     0,    23,    24,
      25,    26,    27,     0,     0,     0,     0,   139,     0,     2,
      31,    32,     0,    33,     3,    34,     4,     5,     6,     7,
       0,     0,     8,     9,    10,     0,     0,     0,    11,    12,
       0,     0,    13,    14,    15,    16,    17,     0,     0,     0,
       0,    18,    19,    20,    21,    22,     0,     0,     0,    23,
      24,    25,    26,    27,     0,     0,     0,     0,     0,     0,
       2,    31,    32,     0,    33,     3,    34,     4,     5,     6,
       7,     0,     0,     8,     9,    10,     0,     0,     0,     0,
       0,     0,     0,    13,    14,    15,    16,    17,     0,     0,
       0,     0,    18,    19,    20,    21,    22,     0,     0,     0,
      23,    24,    25,    26,    27,     0,     0,     0,     0,   139,
       0,     2,    31,    32,     0,    33,     3,    34,     4,     5,
       6,     7,   123,    14,    15,    16,    10,     0,     0,     0,
       0,    18,    19,    20,    21,    22,     0,     0,    17,    23,
      24,    25,    26,    27,     0,     0,    15,    16,     0,     0,
       0,    31,    32,    18,    19,    20,    21,    22,     0,     0,
       0,    23,    24,    25,    26,    27,    33,     0,    34,     0,
       0,     0,     0,    31,    32,    82,    83,    84,    85,    86,
       0,     0,     0,    87,     0,     0,    88,    89,     0,    92,
      93,    94,    95,    96,     0,    90,    91,    97,     0,     0,
      98,    99,     0,     0,     0,     0,     0,     0,     0,   100,
     101
};

static const yytype_int16 yycheck[] =
{
       2,     9,    28,    57,     6,     7,     0,    72,   132,   133,
      38,    65,    33,    34,    21,    40,    14,    11,    54,    24,
      25,    54,     5,     6,     7,    61,    28,     0,    61,    50,
      51,    33,    32,    48,    26,     9,    26,    63,    64,    54,
      66,     3,    26,    71,    60,    52,     8,    72,    10,    11,
      12,    13,    26,    51,    52,    53,    18,    52,    53,    26,
      58,    69,    70,    57,    26,    27,    28,    29,    30,    26,
       9,   195,   196,    35,    36,    37,    38,    39,    52,    52,
      53,    43,    44,    45,    46,    47,    60,    26,    26,    26,
     155,    29,   157,    55,    56,    26,    58,    26,    60,    14,
      29,   126,    14,    14,   130,   131,    21,    26,   134,   135,
      21,     0,    31,    52,    14,    14,    14,    14,   144,    57,
      26,    60,   148,   149,    14,   221,    14,   153,    57,   225,
     155,    26,   157,   158,    26,   161,   138,    52,   132,   133,
      52,    52,    26,    58,   146,   147,    58,    58,    26,   151,
     152,    26,    52,    52,    52,    52,   210,    26,    58,    58,
      58,    58,    52,   217,    52,    26,    26,   263,    58,    29,
      58,    33,    34,    51,    52,    53,   202,   203,   204,   205,
     206,   207,    51,    52,    53,    33,    34,    26,    26,    26,
      29,    29,    29,   247,     5,     6,   222,    57,   223,   224,
       4,   195,   196,   257,    40,    41,    42,   209,    51,   211,
      40,    41,    42,   221,   216,   241,   218,   225,    57,    57,
      57,    14,   248,   234,   235,   236,   237,   238,   230,   231,
       3,    51,   258,   199,   200,     8,    14,    10,    11,    12,
      13,   243,   244,    19,    25,    18,    61,    33,    34,    26,
     252,   253,   254,   255,    26,   263,    26,    30,    26,    26,
      26,   287,    26,     3,    50,    51,    52,    26,     8,   295,
      10,    11,    12,    13,    26,    26,    26,    26,    18,    52,
      26,   307,    26,    26,    26,    58,    61,    60,    59,    15,
      30,    15,    61,    15,     7,   321,    61,    59,   300,     9,
      15,    59,    59,    15,     7,   307,     4,    26,    15,   311,
     312,    59,    52,    15,    59,    15,   318,   319,    58,   321,
      60,    59,    26,   325,   326,    15,    59,   329,   330,    15,
       1,   333,     3,    15,    59,    15,    59,     8,    59,    10,
      11,    12,    13,   217,     9,    16,    17,    18,   320,   281,
     241,    22,    23,    -1,    -1,    26,    27,    28,    29,    30,
      -1,    -1,    -1,    -1,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    43,    44,    45,    46,    47,    -1,    49,    -1,
      -1,    52,    53,     3,    55,    56,    -1,    58,     8,    60,
      10,    11,    12,    13,    -1,    -1,    16,    17,    18,    -1,
      -1,    -1,    22,    23,    -1,    -1,    26,    27,    28,    29,
      30,    -1,    -1,    -1,    -1,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    43,    44,    45,    46,    47,    -1,    -1,
      -1,    -1,    52,    -1,    -1,    55,    56,     3,    58,    -1,
      60,    61,     8,    -1,    10,    11,    12,    13,    -1,    -1,
      16,    17,    18,    -1,    -1,    -1,    22,    23,    -1,    -1,
      26,    27,    28,    29,    30,    -1,    -1,    -1,    -1,    35,
      36,    37,    38,    39,    -1,    -1,    -1,    43,    44,    45,
      46,    47,    -1,    -1,    -1,    51,    52,    53,     3,    55,
      56,    -1,    58,     8,    60,    10,    11,    12,    13,    -1,
      -1,    16,    17,    18,    -1,    -1,    -1,    22,    23,    -1,
      -1,    26,    27,    28,    29,    30,    -1,    -1,    -1,    -1,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    52,    -1,     3,
      55,    56,    -1,    58,     8,    60,    10,    11,    12,    13,
      -1,    -1,    16,    17,    18,    -1,    -1,    -1,    22,    23,
      -1,    -1,    26,    27,    28,    29,    30,    -1,    -1,    -1,
      -1,    35,    36,    37,    38,    39,    -1,    -1,    -1,    43,
      44,    45,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,
       3,    55,    56,    -1,    58,     8,    60,    10,    11,    12,
      13,    -1,    -1,    16,    17,    18,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    26,    27,    28,    29,    30,    -1,    -1,
      -1,    -1,    35,    36,    37,    38,    39,    -1,    -1,    -1,
      43,    44,    45,    46,    47,    -1,    -1,    -1,    -1,    52,
      -1,     3,    55,    56,    -1,    58,     8,    60,    10,    11,
      12,    13,    26,    27,    28,    29,    18,    -1,    -1,    -1,
      -1,    35,    36,    37,    38,    39,    -1,    -1,    30,    43,
      44,    45,    46,    47,    -1,    -1,    28,    29,    -1,    -1,
      -1,    55,    56,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    43,    44,    45,    46,    47,    58,    -1,    60,    -1,
      -1,    -1,    -1,    55,    56,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    43,    -1,    -1,    46,    47,    -1,    35,
      36,    37,    38,    39,    -1,    55,    56,    43,    -1,    -1,
      46,    47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    55,
      56
};

 
static const yytype_int8 yystos[] =
{
       0,     1,     3,     8,    10,    11,    12,    13,    16,    17,
      18,    22,    23,    26,    27,    28,    29,    30,    35,    36,
      37,    38,    39,    43,    44,    45,    46,    47,    49,    52,
      53,    55,    56,    58,    60,    63,    65,    66,    68,    69,
      70,    71,    72,    73,    74,    75,    77,    78,    79,    80,
      81,    82,    83,    95,    96,    97,    98,    99,     0,    52,
      53,    89,    94,    26,    26,    31,    26,    89,    89,    26,
      26,    68,    70,    32,    51,    52,    53,    93,    97,    24,
      25,    60,    35,    36,    37,    38,    39,    43,    46,    47,
      55,    56,    35,    36,    37,    38,    39,    43,    46,    47,
      55,    56,    26,    26,    26,    29,    57,    26,    26,    29,
      57,    26,    26,    26,    26,    26,    89,    94,    26,    26,
      89,    89,     0,    26,    66,    65,    67,    52,    53,    92,
      33,    34,    50,    51,    48,    54,    93,    97,     4,    52,
      90,    91,    97,    94,    51,    94,    14,    58,    93,    51,
      94,    14,    14,    52,    60,    70,    76,    70,    67,    19,
      25,    61,    26,    26,    26,    29,    57,    26,    26,    29,
      57,    26,    26,    26,    26,    26,    26,    26,    26,    29,
      57,    26,    26,    29,    57,    26,    26,    26,    26,    26,
      61,    61,    59,    61,    65,    94,    94,    96,    96,    94,
      94,    89,    33,    34,    50,    51,    52,    21,    94,    14,
      21,    58,    89,    89,    94,    94,    14,    21,    58,    89,
      89,    94,    61,    67,    67,    94,    96,    96,    98,    98,
       5,     6,     7,    84,    94,    94,    94,    94,    94,    85,
      86,    87,    94,    14,    58,    89,    26,    64,    93,    89,
      15,    59,    14,    58,    14,    58,    89,    64,    93,    89,
      15,    15,    76,    94,    76,    89,    89,     7,    91,    91,
      91,    91,    91,     9,    40,    41,    42,    86,    94,     9,
      26,    60,    88,    89,    89,    15,    26,    93,    94,    59,
      89,    89,    89,    89,    15,    93,    94,    59,    76,     7,
       4,    40,    41,    42,     9,    88,    54,    61,    15,    59,
      94,    14,    58,    15,    59,    15,    59,    94,    14,    58,
      89,    61,    26,    89,    94,    14,    58,    89,    89,    14,
      58,    89,    89,     5,    84,    89,    94,    89,    89,    15,
      59,    89,    89,    15,    59,    89,    15,    59,    15,    59
};

 
static const yytype_int8 yyr1[] =
{
       0,    62,    63,    63,    63,    63,    63,    63,    63,    64,
      64,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    66,    66,    66,    67,    67,
      68,    68,    69,    69,    69,    69,    69,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    71,    71,
      71,    71,    71,    71,    71,    71,    72,    72,    72,    72,
      73,    73,    73,    73,    73,    73,    73,    73,    74,    74,
      74,    75,    75,    75,    75,    76,    76,    77,    78,    78,
      79,    79,    79,    79,    79,    80,    80,    80,    81,    82,
      83,    84,    84,    84,    85,    85,    86,    86,    86,    86,
      87,    87,    87,    87,    87,    87,    88,    88,    89,    89,
      90,    90,    90,    91,    91,    91,    91,    91,    91,    92,
      92,    93,    93,    93,    94,    94,    95,    95,    95,    96,
      96,    96,    96,    96,    97,    97,    97,    97,    97,    98,
      98,    98,    99,    99,    99,    99
};

 
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     1,     2,     2,     2,     1,     1,
       2,     2,     2,     3,     3,     3,     3,     2,     3,     3,
       2,     3,     3,     2,     3,     3,     2,     3,     3,     2,
       3,     3,     2,     3,     3,     2,     3,     3,     2,     3,
       3,     2,     3,     3,     2,     3,     3,     2,     3,     3,
       2,     3,     3,     2,     2,     1,     1,     1,     1,     2,
       1,     2,     1,     1,     2,     1,     1,     1,     1,     5,
       5,     1,     1,     1,     1,     1,     1,     1,     6,     6,
       7,     7,    10,    10,     9,     9,     7,     7,     5,     5,
       6,     6,     7,     7,    10,    10,     9,     9,     6,     7,
       6,     5,     6,     3,     5,     1,     2,     3,     3,     3,
       2,     3,     3,     4,     2,     5,     7,     6,     3,     1,
       3,     4,     6,     5,     1,     2,     4,     4,     5,     5,
       2,     3,     2,     3,     2,     3,     1,     3,     2,     2,
       3,     3,     3,     4,     4,     4,     4,     4,     1,     1,
       1,     1,     1,     1,     0,     2,     1,     2,     2,     4,
       4,     3,     3,     1,     1,     2,     2,     2,     2,     4,
       4,     1,     1,     2,     2,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

 
#define YYERRCODE YYUNDEF


 
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h>  
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


 

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


 

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

 

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


 

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
   
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

 
int yydebug;
#else  
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif  


 
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

 

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






 

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


 
int yychar;

 
YYSTYPE yylval;
 
int yynerrs;




 

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
     
    int yyerrstatus = 0;

     

     
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

     
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

     
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
   
  int yyresult;
   
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
   
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

   
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY;  

  goto yysetstate;


 
yynewstate:
   
  yyssp++;


 
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
       
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
         
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

         
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else  
       
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif  


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


 
yybackup:
   

   
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

   

   
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
       
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

   
  if (yyerrstatus)
    yyerrstatus--;

   
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

   
  yychar = YYEMPTY;
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
#line 396 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  global_command = (yyvsp[-1].command);
			  eof_encountered = 0;
			   
			  if (parser_state & PST_CMDSUBST)
			    parser_state |= PST_EOFTOKEN;
			  YYACCEPT;
			}
#line 1954 "y.tab.c"
    break;

  case 3:  
#line 407 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  global_command = (yyvsp[0].command);
			  eof_encountered = 0;
			  YYACCEPT;
			}
#line 1966 "y.tab.c"
    break;

  case 4:  
#line 415 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  global_command = (COMMAND *)NULL;
			  if (parser_state & PST_CMDSUBST)
			    parser_state |= PST_EOFTOKEN;
			  YYACCEPT;
			}
#line 1979 "y.tab.c"
    break;

  case 5:  
#line 424 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  global_command = (COMMAND *)NULL;
			  eof_encountered = 0;
			   
			  if (interactive && parse_and_execute_level == 0)
			    {
			      YYACCEPT;
			    }
			  else
			    {
			      YYABORT;
			    }
			}
#line 1998 "y.tab.c"
    break;

  case 6:  
#line 439 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  global_command = (COMMAND *)NULL;
			  if (last_command_exit_value == 0)
			    last_command_exit_value = EX_BADUSAGE;	 
			  if (interactive && parse_and_execute_level == 0)
			    {
			      handle_eof_input_unit ();
			      YYACCEPT;
			    }
			  else
			    {
			      YYABORT;
			    }
			}
#line 2019 "y.tab.c"
    break;

  case 7:  
#line 456 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  global_command = (COMMAND *)NULL;
			  if (last_command_exit_value == 0)
			    last_command_exit_value = EX_BADUSAGE;	 
			  if (interactive && parse_and_execute_level == 0)
			    {
			      handle_eof_input_unit ();
			      YYACCEPT;
			    }
			  else
			    {
			      YYABORT;
			    }
			}
#line 2038 "y.tab.c"
    break;

  case 8:  
#line 471 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  global_command = (COMMAND *)NULL;
			  handle_eof_input_unit ();
			  YYACCEPT;
			}
#line 2050 "y.tab.c"
    break;

  case 9:  
#line 481 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.word_list) = make_word_list ((yyvsp[0].word), (WORD_LIST *)NULL); }
#line 2056 "y.tab.c"
    break;

  case 10:  
#line 483 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.word_list) = make_word_list ((yyvsp[0].word), (yyvsp[-1].word_list)); }
#line 2062 "y.tab.c"
    break;

  case 11:  
#line 487 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_output_direction, redir, 0);
			}
#line 2072 "y.tab.c"
    break;

  case 12:  
#line 493 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_input_direction, redir, 0);
			}
#line 2082 "y.tab.c"
    break;

  case 13:  
#line 499 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_output_direction, redir, 0);
			}
#line 2092 "y.tab.c"
    break;

  case 14:  
#line 505 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_input_direction, redir, 0);
			}
#line 2102 "y.tab.c"
    break;

  case 15:  
#line 511 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_output_direction, redir, REDIR_VARASSIGN);
			}
#line 2112 "y.tab.c"
    break;

  case 16:  
#line 517 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_input_direction, redir, REDIR_VARASSIGN);
			}
#line 2122 "y.tab.c"
    break;

  case 17:  
#line 523 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_appending_to, redir, 0);
			}
#line 2132 "y.tab.c"
    break;

  case 18:  
#line 529 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_appending_to, redir, 0);
			}
#line 2142 "y.tab.c"
    break;

  case 19:  
#line 535 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_appending_to, redir, REDIR_VARASSIGN);
			}
#line 2152 "y.tab.c"
    break;

  case 20:  
#line 541 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_output_force, redir, 0);
			}
#line 2162 "y.tab.c"
    break;

  case 21:  
#line 547 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_output_force, redir, 0);
			}
#line 2172 "y.tab.c"
    break;

  case 22:  
#line 553 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_output_force, redir, REDIR_VARASSIGN);
			}
#line 2182 "y.tab.c"
    break;

  case 23:  
#line 559 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_input_output, redir, 0);
			}
#line 2192 "y.tab.c"
    break;

  case 24:  
#line 565 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_input_output, redir, 0);
			}
#line 2202 "y.tab.c"
    break;

  case 25:  
#line 571 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_input_output, redir, REDIR_VARASSIGN);
			}
#line 2212 "y.tab.c"
    break;

  case 26:  
#line 577 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_reading_until, redir, 0);
			  push_heredoc ((yyval.redirect));
			}
#line 2223 "y.tab.c"
    break;

  case 27:  
#line 584 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_reading_until, redir, 0);
			  push_heredoc ((yyval.redirect));
			}
#line 2234 "y.tab.c"
    break;

  case 28:  
#line 591 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_reading_until, redir, REDIR_VARASSIGN);
			  push_heredoc ((yyval.redirect));
			}
#line 2245 "y.tab.c"
    break;

  case 29:  
#line 598 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_deblank_reading_until, redir, 0);
			  push_heredoc ((yyval.redirect));
			}
#line 2256 "y.tab.c"
    break;

  case 30:  
#line 605 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_deblank_reading_until, redir, 0);
			  push_heredoc ((yyval.redirect));
			}
#line 2267 "y.tab.c"
    break;

  case 31:  
#line 612 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_deblank_reading_until, redir, REDIR_VARASSIGN);
			  push_heredoc ((yyval.redirect));
			}
#line 2278 "y.tab.c"
    break;

  case 32:  
#line 619 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_reading_string, redir, 0);
			}
#line 2288 "y.tab.c"
    break;

  case 33:  
#line 625 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_reading_string, redir, 0);
			}
#line 2298 "y.tab.c"
    break;

  case 34:  
#line 631 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_reading_string, redir, REDIR_VARASSIGN);
			}
#line 2308 "y.tab.c"
    break;

  case 35:  
#line 637 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.dest = (yyvsp[0].number);
			  (yyval.redirect) = make_redirection (source, r_duplicating_input, redir, 0);
			}
#line 2318 "y.tab.c"
    break;

  case 36:  
#line 643 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.dest = (yyvsp[0].number);
			  (yyval.redirect) = make_redirection (source, r_duplicating_input, redir, 0);
			}
#line 2328 "y.tab.c"
    break;

  case 37:  
#line 649 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.dest = (yyvsp[0].number);
			  (yyval.redirect) = make_redirection (source, r_duplicating_input, redir, REDIR_VARASSIGN);
			}
#line 2338 "y.tab.c"
    break;

  case 38:  
#line 655 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.dest = (yyvsp[0].number);
			  (yyval.redirect) = make_redirection (source, r_duplicating_output, redir, 0);
			}
#line 2348 "y.tab.c"
    break;

  case 39:  
#line 661 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.dest = (yyvsp[0].number);
			  (yyval.redirect) = make_redirection (source, r_duplicating_output, redir, 0);
			}
#line 2358 "y.tab.c"
    break;

  case 40:  
#line 667 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.dest = (yyvsp[0].number);
			  (yyval.redirect) = make_redirection (source, r_duplicating_output, redir, REDIR_VARASSIGN);
			}
#line 2368 "y.tab.c"
    break;

  case 41:  
#line 673 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_duplicating_input_word, redir, 0);
			}
#line 2378 "y.tab.c"
    break;

  case 42:  
#line 679 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_duplicating_input_word, redir, 0);
			}
#line 2388 "y.tab.c"
    break;

  case 43:  
#line 685 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_duplicating_input_word, redir, REDIR_VARASSIGN);
			}
#line 2398 "y.tab.c"
    break;

  case 44:  
#line 691 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_duplicating_output_word, redir, 0);
			}
#line 2408 "y.tab.c"
    break;

  case 45:  
#line 697 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_duplicating_output_word, redir, 0);
			}
#line 2418 "y.tab.c"
    break;

  case 46:  
#line 703 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_duplicating_output_word, redir, REDIR_VARASSIGN);
			}
#line 2428 "y.tab.c"
    break;

  case 47:  
#line 709 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.dest = 0;
			  (yyval.redirect) = make_redirection (source, r_close_this, redir, 0);
			}
#line 2438 "y.tab.c"
    break;

  case 48:  
#line 715 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.dest = 0;
			  (yyval.redirect) = make_redirection (source, r_close_this, redir, 0);
			}
#line 2448 "y.tab.c"
    break;

  case 49:  
#line 721 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.dest = 0;
			  (yyval.redirect) = make_redirection (source, r_close_this, redir, REDIR_VARASSIGN);
			}
#line 2458 "y.tab.c"
    break;

  case 50:  
#line 727 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 0;
			  redir.dest = 0;
			  (yyval.redirect) = make_redirection (source, r_close_this, redir, 0);
			}
#line 2468 "y.tab.c"
    break;

  case 51:  
#line 733 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = (yyvsp[-2].number);
			  redir.dest = 0;
			  (yyval.redirect) = make_redirection (source, r_close_this, redir, 0);
			}
#line 2478 "y.tab.c"
    break;

  case 52:  
#line 739 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.filename = (yyvsp[-2].word);
			  redir.dest = 0;
			  (yyval.redirect) = make_redirection (source, r_close_this, redir, REDIR_VARASSIGN);
			}
#line 2488 "y.tab.c"
    break;

  case 53:  
#line 745 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_err_and_out, redir, 0);
			}
#line 2498 "y.tab.c"
    break;

  case 54:  
#line 751 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  source.dest = 1;
			  redir.filename = (yyvsp[0].word);
			  (yyval.redirect) = make_redirection (source, r_append_err_and_out, redir, 0);
			}
#line 2508 "y.tab.c"
    break;

  case 55:  
#line 759 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.element).word = (yyvsp[0].word); (yyval.element).redirect = 0; }
#line 2514 "y.tab.c"
    break;

  case 56:  
#line 761 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.element).word = (yyvsp[0].word); (yyval.element).redirect = 0; }
#line 2520 "y.tab.c"
    break;

  case 57:  
#line 763 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.element).redirect = (yyvsp[0].redirect); (yyval.element).word = 0; }
#line 2526 "y.tab.c"
    break;

  case 58:  
#line 767 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.redirect) = (yyvsp[0].redirect);
			}
#line 2534 "y.tab.c"
    break;

  case 59:  
#line 771 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  register REDIRECT *t;

			  for (t = (yyvsp[-1].redirect); t->next; t = t->next)
			    ;
			  t->next = (yyvsp[0].redirect);
			  (yyval.redirect) = (yyvsp[-1].redirect);
			}
#line 2547 "y.tab.c"
    break;

  case 60:  
#line 782 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_simple_command ((yyvsp[0].element), (COMMAND *)NULL); }
#line 2553 "y.tab.c"
    break;

  case 61:  
#line 784 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_simple_command ((yyvsp[0].element), (yyvsp[-1].command)); }
#line 2559 "y.tab.c"
    break;

  case 62:  
#line 788 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = clean_simple_command ((yyvsp[0].command)); }
#line 2565 "y.tab.c"
    break;

  case 63:  
#line 790 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2571 "y.tab.c"
    break;

  case 64:  
#line 792 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  COMMAND *tc;

			  tc = (yyvsp[-1].command);
			  if (tc && tc->redirects)
			    {
			      register REDIRECT *t;
			      for (t = tc->redirects; t->next; t = t->next)
				;
			      t->next = (yyvsp[0].redirect);
			    }
			  else if (tc)
			    tc->redirects = (yyvsp[0].redirect);
			  (yyval.command) = (yyvsp[-1].command);
			}
#line 2591 "y.tab.c"
    break;

  case 65:  
#line 808 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2597 "y.tab.c"
    break;

  case 66:  
#line 810 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2603 "y.tab.c"
    break;

  case 67:  
#line 814 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2609 "y.tab.c"
    break;

  case 68:  
#line 816 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2615 "y.tab.c"
    break;

  case 69:  
#line 818 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_while_command ((yyvsp[-3].command), (yyvsp[-1].command)); }
#line 2621 "y.tab.c"
    break;

  case 70:  
#line 820 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_until_command ((yyvsp[-3].command), (yyvsp[-1].command)); }
#line 2627 "y.tab.c"
    break;

  case 71:  
#line 822 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2633 "y.tab.c"
    break;

  case 72:  
#line 824 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2639 "y.tab.c"
    break;

  case 73:  
#line 826 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2645 "y.tab.c"
    break;

  case 74:  
#line 828 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2651 "y.tab.c"
    break;

  case 75:  
#line 830 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2657 "y.tab.c"
    break;

  case 76:  
#line 832 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2663 "y.tab.c"
    break;

  case 77:  
#line 834 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2669 "y.tab.c"
    break;

  case 78:  
#line 838 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-4].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2678 "y.tab.c"
    break;

  case 79:  
#line 843 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-4].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2687 "y.tab.c"
    break;

  case 80:  
#line 848 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-5].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2696 "y.tab.c"
    break;

  case 81:  
#line 853 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-5].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2705 "y.tab.c"
    break;

  case 82:  
#line 858 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-8].word), REVERSE_LIST ((yyvsp[-5].word_list), WORD_LIST *), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2714 "y.tab.c"
    break;

  case 83:  
#line 863 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-8].word), REVERSE_LIST ((yyvsp[-5].word_list), WORD_LIST *), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2723 "y.tab.c"
    break;

  case 84:  
#line 868 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-7].word), (WORD_LIST *)NULL, (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2732 "y.tab.c"
    break;

  case 85:  
#line 873 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_for_command ((yyvsp[-7].word), (WORD_LIST *)NULL, (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2741 "y.tab.c"
    break;

  case 86:  
#line 880 "/usr/local/src/chet/src/bash/src/parse.y"
                                {
				  (yyval.command) = make_arith_for_command ((yyvsp[-5].word_list), (yyvsp[-1].command), arith_for_lineno);
				  if ((yyval.command) == 0) YYERROR;
				  if (word_top > 0) word_top--;
				}
#line 2751 "y.tab.c"
    break;

  case 87:  
#line 886 "/usr/local/src/chet/src/bash/src/parse.y"
                                {
				  (yyval.command) = make_arith_for_command ((yyvsp[-5].word_list), (yyvsp[-1].command), arith_for_lineno);
				  if ((yyval.command) == 0) YYERROR;
				  if (word_top > 0) word_top--;
				}
#line 2761 "y.tab.c"
    break;

  case 88:  
#line 892 "/usr/local/src/chet/src/bash/src/parse.y"
                                {
				  (yyval.command) = make_arith_for_command ((yyvsp[-3].word_list), (yyvsp[-1].command), arith_for_lineno);
				  if ((yyval.command) == 0) YYERROR;
				  if (word_top > 0) word_top--;
				}
#line 2771 "y.tab.c"
    break;

  case 89:  
#line 898 "/usr/local/src/chet/src/bash/src/parse.y"
                                {
				  (yyval.command) = make_arith_for_command ((yyvsp[-3].word_list), (yyvsp[-1].command), arith_for_lineno);
				  if ((yyval.command) == 0) YYERROR;
				  if (word_top > 0) word_top--;
				}
#line 2781 "y.tab.c"
    break;

  case 90:  
#line 906 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-4].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2790 "y.tab.c"
    break;

  case 91:  
#line 911 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-4].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2799 "y.tab.c"
    break;

  case 92:  
#line 916 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-5].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2808 "y.tab.c"
    break;

  case 93:  
#line 921 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-5].word), add_string_to_list ("\"$@\"", (WORD_LIST *)NULL), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2817 "y.tab.c"
    break;

  case 94:  
#line 926 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-8].word), REVERSE_LIST ((yyvsp[-5].word_list), WORD_LIST *), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2826 "y.tab.c"
    break;

  case 95:  
#line 931 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-8].word), REVERSE_LIST ((yyvsp[-5].word_list), WORD_LIST *), (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2835 "y.tab.c"
    break;

  case 96:  
#line 936 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-7].word), (WORD_LIST *)NULL, (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2844 "y.tab.c"
    break;

  case 97:  
#line 941 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_select_command ((yyvsp[-7].word), (WORD_LIST *)NULL, (yyvsp[-1].command), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2853 "y.tab.c"
    break;

  case 98:  
#line 948 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_case_command ((yyvsp[-4].word), (PATTERN_LIST *)NULL, word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2862 "y.tab.c"
    break;

  case 99:  
#line 953 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_case_command ((yyvsp[-5].word), (yyvsp[-2].pattern), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2871 "y.tab.c"
    break;

  case 100:  
#line 958 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_case_command ((yyvsp[-4].word), (yyvsp[-1].pattern), word_lineno[word_top]);
			  if (word_top > 0) word_top--;
			}
#line 2880 "y.tab.c"
    break;

  case 101:  
#line 965 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_function_def ((yyvsp[-4].word), (yyvsp[0].command), function_dstart, function_bstart); }
#line 2886 "y.tab.c"
    break;

  case 102:  
#line 967 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_function_def ((yyvsp[-4].word), (yyvsp[0].command), function_dstart, function_bstart); }
#line 2892 "y.tab.c"
    break;

  case 103:  
#line 969 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_function_def ((yyvsp[-1].word), (yyvsp[0].command), function_dstart, function_bstart); }
#line 2898 "y.tab.c"
    break;

  case 104:  
#line 971 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_function_def ((yyvsp[-3].word), (yyvsp[0].command), function_dstart, function_bstart); }
#line 2904 "y.tab.c"
    break;

  case 105:  
#line 975 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 2910 "y.tab.c"
    break;

  case 106:  
#line 977 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  COMMAND *tc;

			  tc = (yyvsp[-1].command);
			   
			   
			  if (tc && tc->redirects)
			    {
			      register REDIRECT *t;
			      for (t = tc->redirects; t->next; t = t->next)
				;
			      t->next = (yyvsp[0].redirect);
			    }
			  else if (tc)
			    tc->redirects = (yyvsp[0].redirect);
			  (yyval.command) = (yyvsp[-1].command);
			}
#line 2943 "y.tab.c"
    break;

  case 107:  
#line 1008 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_subshell_command ((yyvsp[-1].command));
			  (yyval.command)->flags |= CMD_WANT_SUBSHELL;
			}
#line 2952 "y.tab.c"
    break;

  case 108:  
#line 1015 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = (yyvsp[-1].command);
			}
#line 2960 "y.tab.c"
    break;

  case 109:  
#line 1019 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = (COMMAND *)NULL;
			}
#line 2968 "y.tab.c"
    break;

  case 110:  
#line 1025 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_coproc_command ("COPROC", (yyvsp[0].command));
			  (yyval.command)->flags |= CMD_WANT_SUBSHELL|CMD_COPROC_SUBSHELL;
			}
#line 2977 "y.tab.c"
    break;

  case 111:  
#line 1030 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  COMMAND *tc;

			  tc = (yyvsp[-1].command);
			  if (tc && tc->redirects)
			    {
			      register REDIRECT *t;
			      for (t = tc->redirects; t->next; t = t->next)
				;
			      t->next = (yyvsp[0].redirect);
			    }
			  else if (tc)
			    tc->redirects = (yyvsp[0].redirect);
			  (yyval.command) = make_coproc_command ("COPROC", (yyvsp[-1].command));
			  (yyval.command)->flags |= CMD_WANT_SUBSHELL|CMD_COPROC_SUBSHELL;
			}
#line 2998 "y.tab.c"
    break;

  case 112:  
#line 1047 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_coproc_command ((yyvsp[-1].word)->word, (yyvsp[0].command));
			  (yyval.command)->flags |= CMD_WANT_SUBSHELL|CMD_COPROC_SUBSHELL;
			}
#line 3007 "y.tab.c"
    break;

  case 113:  
#line 1052 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  COMMAND *tc;

			  tc = (yyvsp[-1].command);
			  if (tc && tc->redirects)
			    {
			      register REDIRECT *t;
			      for (t = tc->redirects; t->next; t = t->next)
				;
			      t->next = (yyvsp[0].redirect);
			    }
			  else if (tc)
			    tc->redirects = (yyvsp[0].redirect);
			  (yyval.command) = make_coproc_command ((yyvsp[-2].word)->word, (yyvsp[-1].command));
			  (yyval.command)->flags |= CMD_WANT_SUBSHELL|CMD_COPROC_SUBSHELL;
			}
#line 3028 "y.tab.c"
    break;

  case 114:  
#line 1069 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = make_coproc_command ("COPROC", clean_simple_command ((yyvsp[0].command)));
			  (yyval.command)->flags |= CMD_WANT_SUBSHELL|CMD_COPROC_SUBSHELL;
			}
#line 3037 "y.tab.c"
    break;

  case 115:  
#line 1076 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_if_command ((yyvsp[-3].command), (yyvsp[-1].command), (COMMAND *)NULL); }
#line 3043 "y.tab.c"
    break;

  case 116:  
#line 1078 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_if_command ((yyvsp[-5].command), (yyvsp[-3].command), (yyvsp[-1].command)); }
#line 3049 "y.tab.c"
    break;

  case 117:  
#line 1080 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_if_command ((yyvsp[-4].command), (yyvsp[-2].command), (yyvsp[-1].command)); }
#line 3055 "y.tab.c"
    break;

  case 118:  
#line 1085 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_group_command ((yyvsp[-1].command)); }
#line 3061 "y.tab.c"
    break;

  case 119:  
#line 1089 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_arith_command ((yyvsp[0].word_list)); }
#line 3067 "y.tab.c"
    break;

  case 120:  
#line 1093 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[-1].command); }
#line 3073 "y.tab.c"
    break;

  case 121:  
#line 1097 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_if_command ((yyvsp[-2].command), (yyvsp[0].command), (COMMAND *)NULL); }
#line 3079 "y.tab.c"
    break;

  case 122:  
#line 1099 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_if_command ((yyvsp[-4].command), (yyvsp[-2].command), (yyvsp[0].command)); }
#line 3085 "y.tab.c"
    break;

  case 123:  
#line 1101 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = make_if_command ((yyvsp[-3].command), (yyvsp[-1].command), (yyvsp[0].command)); }
#line 3091 "y.tab.c"
    break;

  case 125:  
#line 1106 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyvsp[0].pattern)->next = (yyvsp[-1].pattern); (yyval.pattern) = (yyvsp[0].pattern); }
#line 3097 "y.tab.c"
    break;

  case 126:  
#line 1110 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.pattern) = make_pattern_list ((yyvsp[-2].word_list), (yyvsp[0].command)); }
#line 3103 "y.tab.c"
    break;

  case 127:  
#line 1112 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.pattern) = make_pattern_list ((yyvsp[-2].word_list), (COMMAND *)NULL); }
#line 3109 "y.tab.c"
    break;

  case 128:  
#line 1114 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.pattern) = make_pattern_list ((yyvsp[-2].word_list), (yyvsp[0].command)); }
#line 3115 "y.tab.c"
    break;

  case 129:  
#line 1116 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.pattern) = make_pattern_list ((yyvsp[-2].word_list), (COMMAND *)NULL); }
#line 3121 "y.tab.c"
    break;

  case 130:  
#line 1120 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.pattern) = (yyvsp[-1].pattern); }
#line 3127 "y.tab.c"
    break;

  case 131:  
#line 1122 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyvsp[-1].pattern)->next = (yyvsp[-2].pattern); (yyval.pattern) = (yyvsp[-1].pattern); }
#line 3133 "y.tab.c"
    break;

  case 132:  
#line 1124 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyvsp[-1].pattern)->flags |= CASEPAT_FALLTHROUGH; (yyval.pattern) = (yyvsp[-1].pattern); }
#line 3139 "y.tab.c"
    break;

  case 133:  
#line 1126 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyvsp[-1].pattern)->flags |= CASEPAT_FALLTHROUGH; (yyvsp[-1].pattern)->next = (yyvsp[-2].pattern); (yyval.pattern) = (yyvsp[-1].pattern); }
#line 3145 "y.tab.c"
    break;

  case 134:  
#line 1128 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyvsp[-1].pattern)->flags |= CASEPAT_TESTNEXT; (yyval.pattern) = (yyvsp[-1].pattern); }
#line 3151 "y.tab.c"
    break;

  case 135:  
#line 1130 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyvsp[-1].pattern)->flags |= CASEPAT_TESTNEXT; (yyvsp[-1].pattern)->next = (yyvsp[-2].pattern); (yyval.pattern) = (yyvsp[-1].pattern); }
#line 3157 "y.tab.c"
    break;

  case 136:  
#line 1134 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.word_list) = make_word_list ((yyvsp[0].word), (WORD_LIST *)NULL); }
#line 3163 "y.tab.c"
    break;

  case 137:  
#line 1136 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.word_list) = make_word_list ((yyvsp[0].word), (yyvsp[-2].word_list)); }
#line 3169 "y.tab.c"
    break;

  case 138:  
#line 1145 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = (yyvsp[0].command);
			  if (need_here_doc && last_read_token == '\n')
			    gather_here_documents ();
			 }
#line 3179 "y.tab.c"
    break;

  case 139:  
#line 1151 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = (yyvsp[0].command);
			}
#line 3187 "y.tab.c"
    break;

  case 141:  
#line 1158 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if ((yyvsp[-2].command)->type == cm_connection)
			    (yyval.command) = connect_async_list ((yyvsp[-2].command), (COMMAND *)NULL, '&');
			  else
			    (yyval.command) = command_connect ((yyvsp[-2].command), (COMMAND *)NULL, '&');
			}
#line 3198 "y.tab.c"
    break;

  case 143:  
#line 1169 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), AND_AND); }
#line 3204 "y.tab.c"
    break;

  case 144:  
#line 1171 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), OR_OR); }
#line 3210 "y.tab.c"
    break;

  case 145:  
#line 1173 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if ((yyvsp[-3].command)->type == cm_connection)
			    (yyval.command) = connect_async_list ((yyvsp[-3].command), (yyvsp[0].command), '&');
			  else
			    (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), '&');
			}
#line 3221 "y.tab.c"
    break;

  case 146:  
#line 1180 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), ';'); }
#line 3227 "y.tab.c"
    break;

  case 147:  
#line 1182 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if (parser_state & PST_CMDSUBST)
			    (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), '\n');
			  else
			    (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), ';');
			}
#line 3238 "y.tab.c"
    break;

  case 148:  
#line 1189 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 3244 "y.tab.c"
    break;

  case 151:  
#line 1197 "/usr/local/src/chet/src/bash/src/parse.y"
                { (yyval.number) = '\n'; }
#line 3250 "y.tab.c"
    break;

  case 152:  
#line 1199 "/usr/local/src/chet/src/bash/src/parse.y"
                { (yyval.number) = ';'; }
#line 3256 "y.tab.c"
    break;

  case 153:  
#line 1201 "/usr/local/src/chet/src/bash/src/parse.y"
                { (yyval.number) = yacc_EOF; }
#line 3262 "y.tab.c"
    break;

  case 156:  
#line 1215 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = (yyvsp[0].command);
			  if (need_here_doc)
			    gather_here_documents ();	 
			  if ((parser_state & PST_CMDSUBST) && current_token == shell_eof_token)
			    {
INTERNAL_DEBUG (("LEGACY: parser: command substitution simple_list1 -> simple_list"));
			      global_command = (yyvsp[0].command);
			      eof_encountered = 0;
			      if (bash_input.type == st_string)
				rewind_input_string ();
			      YYACCEPT;
			    }
			}
#line 3281 "y.tab.c"
    break;

  case 157:  
#line 1230 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if ((yyvsp[-1].command)->type == cm_connection)
			    (yyval.command) = connect_async_list ((yyvsp[-1].command), (COMMAND *)NULL, '&');
			  else
			    (yyval.command) = command_connect ((yyvsp[-1].command), (COMMAND *)NULL, '&');
			  if (need_here_doc)
			    gather_here_documents ();  
			  if ((parser_state & PST_CMDSUBST) && current_token == shell_eof_token)
			    {
INTERNAL_DEBUG (("LEGACY: parser: command substitution simple_list1 '&' -> simple_list"));
			      global_command = (yyvsp[-1].command);
			      eof_encountered = 0;
			      if (bash_input.type == st_string)
				rewind_input_string ();
			      YYACCEPT;
			    }
			}
#line 3303 "y.tab.c"
    break;

  case 158:  
#line 1248 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  (yyval.command) = (yyvsp[-1].command);
			  if (need_here_doc)
			    gather_here_documents ();	 
			  if ((parser_state & PST_CMDSUBST) && current_token == shell_eof_token)
			    {
INTERNAL_DEBUG (("LEGACY: parser: command substitution simple_list1 ';' -> simple_list"));
			      global_command = (yyvsp[-1].command);
			      eof_encountered = 0;
			      if (bash_input.type == st_string)
				rewind_input_string ();
			      YYACCEPT;
			    }
			}
#line 3322 "y.tab.c"
    break;

  case 159:  
#line 1265 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), AND_AND); }
#line 3328 "y.tab.c"
    break;

  case 160:  
#line 1267 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), OR_OR); }
#line 3334 "y.tab.c"
    break;

  case 161:  
#line 1269 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if ((yyvsp[-2].command)->type == cm_connection)
			    (yyval.command) = connect_async_list ((yyvsp[-2].command), (yyvsp[0].command), '&');
			  else
			    (yyval.command) = command_connect ((yyvsp[-2].command), (yyvsp[0].command), '&');
			}
#line 3345 "y.tab.c"
    break;

  case 162:  
#line 1276 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-2].command), (yyvsp[0].command), ';'); }
#line 3351 "y.tab.c"
    break;

  case 163:  
#line 1279 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 3357 "y.tab.c"
    break;

  case 164:  
#line 1283 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 3363 "y.tab.c"
    break;

  case 165:  
#line 1285 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if ((yyvsp[0].command))
			    (yyvsp[0].command)->flags ^= CMD_INVERT_RETURN;	 
			  (yyval.command) = (yyvsp[0].command);
			}
#line 3373 "y.tab.c"
    break;

  case 166:  
#line 1291 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  if ((yyvsp[0].command))
			    (yyvsp[0].command)->flags |= (yyvsp[-1].number);
			  (yyval.command) = (yyvsp[0].command);
			}
#line 3383 "y.tab.c"
    break;

  case 167:  
#line 1297 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  ELEMENT x;

			   
			  x.word = 0;
			  x.redirect = 0;
			  (yyval.command) = make_simple_command (x, (COMMAND *)NULL);
			  (yyval.command)->flags |= (yyvsp[-1].number);
			   
			  if ((yyvsp[0].number) == '\n')
			    token_to_read = '\n';
			  else if ((yyvsp[0].number) == ';')
			    token_to_read = ';';
			  parser_state &= ~PST_REDIRLIST;	 
			}
#line 3407 "y.tab.c"
    break;

  case 168:  
#line 1317 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			  ELEMENT x;

			   
			  x.word = 0;
			  x.redirect = 0;
			  (yyval.command) = make_simple_command (x, (COMMAND *)NULL);
			  (yyval.command)->flags |= CMD_INVERT_RETURN;
			   
			  if ((yyvsp[0].number) == '\n')
			    token_to_read = '\n';
			  if ((yyvsp[0].number) == ';')
			    token_to_read = ';';
			  parser_state &= ~PST_REDIRLIST;	 
			}
#line 3432 "y.tab.c"
    break;

  case 169:  
#line 1340 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), '|'); }
#line 3438 "y.tab.c"
    break;

  case 170:  
#line 1342 "/usr/local/src/chet/src/bash/src/parse.y"
                        {
			   
			  COMMAND *tc;
			  REDIRECTEE rd, sd;
			  REDIRECT *r;

			  tc = (yyvsp[-3].command)->type == cm_simple ? (COMMAND *)(yyvsp[-3].command)->value.Simple : (yyvsp[-3].command);
			  sd.dest = 2;
			  rd.dest = 1;
			  r = make_redirection (sd, r_duplicating_output, rd, 0);
			  if (tc->redirects)
			    {
			      register REDIRECT *t;
			      for (t = tc->redirects; t->next; t = t->next)
				;
			      t->next = r;
			    }
			  else
			    tc->redirects = r;

			  (yyval.command) = command_connect ((yyvsp[-3].command), (yyvsp[0].command), '|');
			}
#line 3465 "y.tab.c"
    break;

  case 171:  
#line 1365 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.command) = (yyvsp[0].command); }
#line 3471 "y.tab.c"
    break;

  case 172:  
#line 1369 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.number) = CMD_TIME_PIPELINE; }
#line 3477 "y.tab.c"
    break;

  case 173:  
#line 1371 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.number) = CMD_TIME_PIPELINE|CMD_TIME_POSIX; }
#line 3483 "y.tab.c"
    break;

  case 174:  
#line 1373 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.number) = CMD_TIME_PIPELINE|CMD_TIME_POSIX; }
#line 3489 "y.tab.c"
    break;

  case 175:  
#line 1375 "/usr/local/src/chet/src/bash/src/parse.y"
                        { (yyval.number) = CMD_TIME_PIPELINE|CMD_TIME_POSIX; }
#line 3495 "y.tab.c"
    break;


#line 3499 "y.tab.c"

      default: break;
    }
   
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

   
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


 
yyerrlab:
   
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
   
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
       

      if (yychar <= YYEOF)
        {
           
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

   
  goto yyerrlab1;


 
yyerrorlab:
   
  if (0)
    YYERROR;
  ++yynerrs;

   
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


 
yyerrlab1:
  yyerrstatus = 3;       

   
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

       
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


   
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


 
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


 
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


 
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


 
yyreturnlab:
  if (yychar != YYEMPTY)
    {
       
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
   
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1377 "/usr/local/src/chet/src/bash/src/parse.y"


 
#define TOKEN_DEFAULT_INITIAL_SIZE 496
#define TOKEN_DEFAULT_GROW_SIZE 512

 
#define SHOULD_PROMPT() \
  (interactive && (bash_input.type == st_stdin || bash_input.type == st_stream))

#if defined (ALIAS)
#  define expanding_alias() (pushed_string_list && pushed_string_list->expander)
#else
#  define expanding_alias() 0
#endif

 
int EOF_Reached = 0;

#ifdef DEBUG
static void
debug_parser (i)
     int i;
{
#if YYDEBUG != 0
  yydebug = i;
  yyoutstream = stdout;
  yyerrstream = stderr;
#endif
}
#endif

 

 
int
return_EOF ()
{
  return (EOF);
}

 
BASH_INPUT bash_input;

 
void
initialize_bash_input ()
{
  bash_input.type = st_none;
  FREE (bash_input.name);
  bash_input.name = (char *)NULL;
  bash_input.location.file = (FILE *)NULL;
  bash_input.location.string = (char *)NULL;
  bash_input.getter = (sh_cget_func_t *)NULL;
  bash_input.ungetter = (sh_cunget_func_t *)NULL;
}

 
void
init_yy_io (get, unget, type, name, location)
     sh_cget_func_t *get;
     sh_cunget_func_t *unget;
     enum stream_type type;
     const char *name;
     INPUT_STREAM location;
{
  bash_input.type = type;
  FREE (bash_input.name);
  bash_input.name = name ? savestring (name) : (char *)NULL;

   
#if defined (CRAY)
  memcpy((char *)&bash_input.location.string, (char *)&location.string, sizeof(location));
#else
  bash_input.location = location;
#endif
  bash_input.getter = get;
  bash_input.ungetter = unget;
}

char *
yy_input_name ()
{
  return (bash_input.name ? bash_input.name : "stdin");
}

 
static int
yy_getc ()
{
  return (*(bash_input.getter)) ();
}

 
static int
yy_ungetc (c)
     int c;
{
  return (*(bash_input.ungetter)) (c);
}

#if defined (BUFFERED_INPUT)
#ifdef INCLUDE_UNUSED
int
input_file_descriptor ()
{
  switch (bash_input.type)
    {
    case st_stream:
      return (fileno (bash_input.location.file));
    case st_bstream:
      return (bash_input.location.buffered_fd);
    case st_stdin:
    default:
      return (fileno (stdin));
    }
}
#endif
#endif  

 
 
 
 
 

#if defined (READLINE)
char *current_readline_prompt = (char *)NULL;
char *current_readline_line = (char *)NULL;
int current_readline_line_index = 0;

static int
yy_readline_get ()
{
  SigHandler *old_sigint;
  int line_len;
  unsigned char c;

  if (current_readline_line == 0)
    {
      if (bash_readline_initialized == 0)
	initialize_readline ();

#if defined (JOB_CONTROL)
      if (job_control)
	give_terminal_to (shell_pgrp, 0);
#endif  

      old_sigint = IMPOSSIBLE_TRAP_HANDLER;
      if (signal_is_ignored (SIGINT) == 0)
	{
	  old_sigint = (SigHandler *)set_signal_handler (SIGINT, sigint_sighandler);
	}

      sh_unset_nodelay_mode (fileno (rl_instream));	 
      current_readline_line = readline (current_readline_prompt ?
      					  current_readline_prompt : "");

      CHECK_TERMSIG;
      if (signal_is_ignored (SIGINT) == 0)
	{
	  if (old_sigint != IMPOSSIBLE_TRAP_HANDLER)
	    set_signal_handler (SIGINT, old_sigint);
	}

#if 0
       
      reset_readline_prompt ();
#endif

      if (current_readline_line == 0)
	return (EOF);

      current_readline_line_index = 0;
      line_len = strlen (current_readline_line);

      current_readline_line = (char *)xrealloc (current_readline_line, 2 + line_len);
      current_readline_line[line_len++] = '\n';
      current_readline_line[line_len] = '\0';
    }

  if (current_readline_line[current_readline_line_index] == 0)
    {
      free (current_readline_line);
      current_readline_line = (char *)NULL;
      return (yy_readline_get ());
    }
  else
    {
      c = current_readline_line[current_readline_line_index++];
      return (c);
    }
}

static int
yy_readline_unget (c)
     int c;
{
  if (current_readline_line_index && current_readline_line)
    current_readline_line[--current_readline_line_index] = c;
  return (c);
}

void
with_input_from_stdin ()
{
  INPUT_STREAM location;

  if (bash_input.type != st_stdin && stream_on_stack (st_stdin) == 0)
    {
      location.string = current_readline_line;
      init_yy_io (yy_readline_get, yy_readline_unget,
		  st_stdin, "readline stdin", location);
    }
}

 
int
parser_will_prompt ()
{
  return (current_readline_line == 0 || current_readline_line[current_readline_line_index] == 0);
}
  
#else   

void
with_input_from_stdin ()
{
  with_input_from_stream (stdin, "stdin");
}
#endif	 

 
 
 
 
 

static int
yy_string_get ()
{
  register char *string;
  register unsigned char c;

  string = bash_input.location.string;

   
  if (string && *string)
    {
      c = *string++;
      bash_input.location.string = string;
      return (c);
    }
  else
    return (EOF);
}

static int
yy_string_unget (c)
     int c;
{
  *(--bash_input.location.string) = c;
  return (c);
}

void
with_input_from_string (string, name)
     char *string;
     const char *name;
{
  INPUT_STREAM location;

  location.string = string;
  init_yy_io (yy_string_get, yy_string_unget, st_string, name, location);
}

 
void
rewind_input_string ()
{
  int xchars;

   
  xchars = shell_input_line_len - shell_input_line_index;
  if (bash_input.location.string[-1] == '\n')
    xchars++;

   

   
   
  bash_input.location.string -= xchars;
}

 
 
 
 
 

 

static int
yy_stream_get ()
{
  int result;

  result = EOF;
  if (bash_input.location.file)
    {
       
      result = getc_with_restart (bash_input.location.file);
    }
  return (result);
}

static int
yy_stream_unget (c)
     int c;
{
  return (ungetc_with_restart (c, bash_input.location.file));
}

void
with_input_from_stream (stream, name)
     FILE *stream;
     const char *name;
{
  INPUT_STREAM location;

  location.file = stream;
  init_yy_io (yy_stream_get, yy_stream_unget, st_stream, name, location);
}

typedef struct stream_saver {
  struct stream_saver *next;
  BASH_INPUT bash_input;
  int line;
#if defined (BUFFERED_INPUT)
  BUFFERED_STREAM *bstream;
#endif  
} STREAM_SAVER;

 
int line_number = 0;

 
int line_number_base = 0;

#if defined (COND_COMMAND)
static int cond_lineno;
static int cond_token;
#endif

STREAM_SAVER *stream_list = (STREAM_SAVER *)NULL;

void
push_stream (reset_lineno)
     int reset_lineno;
{
  STREAM_SAVER *saver = (STREAM_SAVER *)xmalloc (sizeof (STREAM_SAVER));

  xbcopy ((char *)&bash_input, (char *)&(saver->bash_input), sizeof (BASH_INPUT));

#if defined (BUFFERED_INPUT)
  saver->bstream = (BUFFERED_STREAM *)NULL;
   
  if (bash_input.type == st_bstream && bash_input.location.buffered_fd >= 0)
    saver->bstream = set_buffered_stream (bash_input.location.buffered_fd,
    					  (BUFFERED_STREAM *)NULL);
#endif  

  saver->line = line_number;
  bash_input.name = (char *)NULL;
  saver->next = stream_list;
  stream_list = saver;
  EOF_Reached = 0;
  if (reset_lineno)
    line_number = 0;
}

void
pop_stream ()
{
  if (!stream_list)
    EOF_Reached = 1;
  else
    {
      STREAM_SAVER *saver = stream_list;

      EOF_Reached = 0;
      stream_list = stream_list->next;

      init_yy_io (saver->bash_input.getter,
		  saver->bash_input.ungetter,
		  saver->bash_input.type,
		  saver->bash_input.name,
		  saver->bash_input.location);

#if defined (BUFFERED_INPUT)
       
       
      if (bash_input.type == st_bstream && bash_input.location.buffered_fd >= 0)
	{
	  if (bash_input_fd_changed)
	    {
	      bash_input_fd_changed = 0;
	      if (default_buffered_input >= 0)
		{
		  bash_input.location.buffered_fd = default_buffered_input;
		  saver->bstream->b_fd = default_buffered_input;
		  SET_CLOSE_ON_EXEC (default_buffered_input);
		}
	    }
	   
	  set_buffered_stream (bash_input.location.buffered_fd, saver->bstream);
	}
#endif  

      line_number = saver->line;

      FREE (saver->bash_input.name);
      free (saver);
    }
}

 
int
stream_on_stack (type)
     enum stream_type type;
{
  register STREAM_SAVER *s;

  for (s = stream_list; s; s = s->next)
    if (s->bash_input.type == type)
      return 1;
  return 0;
}

 
int *
save_token_state ()
{
  int *ret;

  ret = (int *)xmalloc (4 * sizeof (int));
  ret[0] = last_read_token;
  ret[1] = token_before_that;
  ret[2] = two_tokens_ago;
  ret[3] = current_token;
  return ret;
}

void
restore_token_state (ts)
     int *ts;
{
  if (ts == 0)
    return;
  last_read_token = ts[0];
  token_before_that = ts[1];
  two_tokens_ago = ts[2];
  current_token = ts[3];
}

 

#define END_OF_ALIAS 0

 

 

#define PSH_ALIAS	0x01
#define PSH_DPAREN	0x02
#define PSH_SOURCE	0x04
#define PSH_ARRAY	0x08

typedef struct string_saver {
  struct string_saver *next;
  int expand_alias;   
  char *saved_line;
#if defined (ALIAS)
  alias_t *expander;    
#endif
  size_t saved_line_size, saved_line_index, saved_line_len;
  int saved_line_terminator;
  int flags;
} STRING_SAVER;

STRING_SAVER *pushed_string_list = (STRING_SAVER *)NULL;

 
static void
push_string (s, expand, ap)
     char *s;
     int expand;
     alias_t *ap;
{
  STRING_SAVER *temp = (STRING_SAVER *)xmalloc (sizeof (STRING_SAVER));

  temp->expand_alias = expand;
  temp->saved_line = shell_input_line;
  temp->saved_line_size = shell_input_line_size;
  temp->saved_line_len = shell_input_line_len;
  temp->saved_line_index = shell_input_line_index;
  temp->saved_line_terminator = shell_input_line_terminator;
  temp->flags = 0;
#if defined (ALIAS)
  temp->expander = ap;
  if (ap)
    temp->flags = PSH_ALIAS;
#endif
  temp->next = pushed_string_list;
  pushed_string_list = temp;

#if defined (ALIAS)
  if (ap)
    ap->flags |= AL_BEINGEXPANDED;
#endif

  shell_input_line = s;
  shell_input_line_size = shell_input_line_len = STRLEN (s);
  shell_input_line_index = 0;
  shell_input_line_terminator = '\0';
#if 0
  parser_state &= ~PST_ALEXPNEXT;	 
#endif

  set_line_mbstate ();
}

 
static void
pop_string ()
{
  STRING_SAVER *t;

  FREE (shell_input_line);
  shell_input_line = pushed_string_list->saved_line;
  shell_input_line_index = pushed_string_list->saved_line_index;
  shell_input_line_size = pushed_string_list->saved_line_size;
  shell_input_line_len = pushed_string_list->saved_line_len;
  shell_input_line_terminator = pushed_string_list->saved_line_terminator;

#if defined (ALIAS)
  if (pushed_string_list->expand_alias)
    parser_state |= PST_ALEXPNEXT;
  else
    parser_state &= ~PST_ALEXPNEXT;
#endif

  t = pushed_string_list;
  pushed_string_list = pushed_string_list->next;

#if defined (ALIAS)
  if (t->expander)
    t->expander->flags &= ~AL_BEINGEXPANDED;
#endif

  free ((char *)t);

  set_line_mbstate ();
}

static void
free_string_list ()
{
  register STRING_SAVER *t, *t1;

  for (t = pushed_string_list; t; )
    {
      t1 = t->next;
      FREE (t->saved_line);
#if defined (ALIAS)
      if (t->expander)
	t->expander->flags &= ~AL_BEINGEXPANDED;
#endif
      free ((char *)t);
      t = t1;
    }
  pushed_string_list = (STRING_SAVER *)NULL;
}

void
free_pushed_string_input ()
{
#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  free_string_list ();
#endif
}

int
parser_expanding_alias ()
{
  return (expanding_alias ());
}

void
parser_save_alias ()
{
#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  push_string ((char *)NULL, 0, (alias_t *)NULL);
  pushed_string_list->flags = PSH_SOURCE;	 
#else
  ;
#endif
}

void
parser_restore_alias ()
{
#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  if (pushed_string_list)
    pop_string ();
#else
  ;
#endif
}

#if defined (ALIAS)
 
void
clear_string_list_expander (ap)
     alias_t *ap;
{
  register STRING_SAVER *t;

  for (t = pushed_string_list; t; t = t->next)
    {
      if (t->expander && t->expander == ap)
	t->expander = 0;
    }
}
#endif

void
clear_shell_input_line ()
{
  if (shell_input_line)
    shell_input_line[shell_input_line_index = 0] = '\0';
}

 
static char *
read_a_line (remove_quoted_newline)
     int remove_quoted_newline;
{
  static char *line_buffer = (char *)NULL;
  static int buffer_size = 0;
  int indx, c, peekc, pass_next;

#if defined (READLINE)
  if (no_line_editing && SHOULD_PROMPT ())
#else
  if (SHOULD_PROMPT ())
#endif
    print_prompt ();

  pass_next = indx = 0;
  while (1)
    {
       
      QUIT;

      c = yy_getc ();

       
      if (c == 0)
	continue;

       
      if (c == EOF)
	{
	  if (interactive && bash_input.type == st_stream)
	    clearerr (stdin);
	  if (indx == 0)
	    return ((char *)NULL);
	  c = '\n';
	}

       
      RESIZE_MALLOCED_BUFFER (line_buffer, indx, 2, buffer_size, 128);

       
      if (pass_next)
	{
	  line_buffer[indx++] = c;
	  pass_next = 0;
	}
      else if (c == '\\' && remove_quoted_newline)
	{
	  QUIT;
	  peekc = yy_getc ();
	  if (peekc == '\n')
	    {
	      line_number++;
	      continue;	 
	    }
	  else
	    {
	      yy_ungetc (peekc);
	      pass_next = 1;
	      line_buffer[indx++] = c;		 
	    }
	}
      else
	{
	   
	  if (remove_quoted_newline && (c == CTLESC || c == CTLNUL))
	    line_buffer[indx++] = CTLESC;
	  line_buffer[indx++] = c;
	}

      if (c == '\n')
	{
	  line_buffer[indx] = '\0';
	  return (line_buffer);
	}
    }
}

 
char *
read_secondary_line (remove_quoted_newline)
     int remove_quoted_newline;
{
  char *ret;
  int n, c;

  prompt_string_pointer = &ps2_prompt;
  if (SHOULD_PROMPT ())
    prompt_again (0);
  ret = read_a_line (remove_quoted_newline);
#if defined (HISTORY)
  if (ret && remember_on_history && (parser_state & PST_HEREDOC))
    {
       

      current_command_line_count++;
      maybe_add_history (ret);
    }
#endif  
  return ret;
}

 
 
 
 
 

 
STRING_INT_ALIST word_token_alist[] = {
  { "if", IF },
  { "then", THEN },
  { "else", ELSE },
  { "elif", ELIF },
  { "fi", FI },
  { "case", CASE },
  { "esac", ESAC },
  { "for", FOR },
#if defined (SELECT_COMMAND)
  { "select", SELECT },
#endif
  { "while", WHILE },
  { "until", UNTIL },
  { "do", DO },
  { "done", DONE },
  { "in", IN },
  { "function", FUNCTION },
#if defined (COMMAND_TIMING)
  { "time", TIME },
#endif
  { "{", '{' },
  { "}", '}' },
  { "!", BANG },
#if defined (COND_COMMAND)
  { "[[", COND_START },
  { "]]", COND_END },
#endif
#if defined (COPROCESS_SUPPORT)
  { "coproc", COPROC },
#endif
  { (char *)NULL, 0}
};

 
STRING_INT_ALIST other_token_alist[] = {
   
  { "--", TIMEIGN },
  { "-p", TIMEOPT },
  { "&&", AND_AND },
  { "||", OR_OR },
  { ">>", GREATER_GREATER },
  { "<<", LESS_LESS },
  { "<&", LESS_AND },
  { ">&", GREATER_AND },
  { ";;", SEMI_SEMI },
  { ";&", SEMI_AND },
  { ";;&", SEMI_SEMI_AND },
  { "<<-", LESS_LESS_MINUS },
  { "<<<", LESS_LESS_LESS },
  { "&>", AND_GREATER },
  { "&>>", AND_GREATER_GREATER },
  { "<>", LESS_GREATER },
  { ">|", GREATER_BAR },
  { "|&", BAR_AND },
  { "EOF", yacc_EOF },
   
  { ">", '>' },
  { "<", '<' },
  { "-", '-' },
  { "{", '{' },
  { "}", '}' },
  { ";", ';' },
  { "(", '(' },
  { ")", ')' },
  { "|", '|' },
  { "&", '&' },
  { "newline", '\n' },
  { (char *)NULL, 0}
};

 

 

 
struct dstack dstack = {  (char *)NULL, 0, 0 };

 
static struct dstack temp_dstack = { (char *)NULL, 0, 0 };

 
#define current_delimiter(ds) \
  (ds.delimiter_depth ? ds.delimiters[ds.delimiter_depth - 1] : 0)

#define push_delimiter(ds, character) \
  do \
    { \
      if (ds.delimiter_depth + 2 > ds.delimiter_space) \
	ds.delimiters = (char *)xrealloc \
	  (ds.delimiters, (ds.delimiter_space += 10) * sizeof (char)); \
      ds.delimiters[ds.delimiter_depth] = character; \
      ds.delimiter_depth++; \
    } \
  while (0)

#define pop_delimiter(ds)	ds.delimiter_depth--

 

 
static int eol_ungetc_lookahead = 0;

static int unquoted_backslash = 0;

static int
shell_getc (remove_quoted_newline)
     int remove_quoted_newline;
{
  register int i;
  int c, truncating, last_was_backslash;
  unsigned char uc;

  QUIT;

  last_was_backslash = 0;
  if (sigwinch_received)
    {
      sigwinch_received = 0;
      get_new_window_size (0, (int *)0, (int *)0);
    }
      
  if (eol_ungetc_lookahead)
    {
      c = eol_ungetc_lookahead;
      eol_ungetc_lookahead = 0;
      return (c);
    }

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
   

  if (!shell_input_line || ((!shell_input_line[shell_input_line_index]) &&
			    (pushed_string_list == (STRING_SAVER *)NULL)))
#else  
  if (!shell_input_line || !shell_input_line[shell_input_line_index])
#endif  
    {
      line_number++;

       
      if (shell_input_line && shell_input_line_size >= 32768)
	{
	  free (shell_input_line);
	  shell_input_line = 0;
	  shell_input_line_size = 0;
	}

    restart_read:

       
      QUIT;

      i = truncating = 0;
      shell_input_line_terminator = 0;

       
      if (interactive_shell == 0 || SHOULD_PROMPT())
	{
#if defined (JOB_CONTROL)
       
	  notify_and_cleanup ();
#else  
	  cleanup_dead_jobs ();
#endif  
	}

#if defined (READLINE)
      if (no_line_editing && SHOULD_PROMPT())
#else
      if (SHOULD_PROMPT())
#endif
	print_prompt ();

      if (bash_input.type == st_stream)
	clearerr (stdin);

      while (1)
	{
	  c = yy_getc ();

	   
	  QUIT;

	  if (c == '\0')
	    {
	       
	      if (bash_input.type == st_string)
		{
		  if (i == 0)
		    shell_input_line_terminator = EOF;
		  shell_input_line[i] = '\0';
		  c = EOF;
		  break;
		}
	      continue;
	    }

	   
	   
	   
	  if (shell_input_line_size > (SIZE_MAX - 256))
	    {
	      size_t n;

	      n = SIZE_MAX - i;	 
	      if (n <= 2)	 
		{
		  if (truncating == 0)
		    internal_warning(_("shell_getc: shell_input_line_size (%zu) exceeds SIZE_MAX (%lu): line truncated"), shell_input_line_size, (unsigned long)SIZE_MAX);
		  shell_input_line[i] = '\0';
		  truncating = 1;
		}
	      if (shell_input_line_size < SIZE_MAX)
		{
		  shell_input_line_size = SIZE_MAX;
		  shell_input_line = xrealloc (shell_input_line, shell_input_line_size);
		}
	    }
	  else
	    RESIZE_MALLOCED_BUFFER (shell_input_line, i, 2, shell_input_line_size, 256);

	  if (c == EOF)
	    {
	      if (bash_input.type == st_stream)
		clearerr (stdin);

	      if (i == 0)
		shell_input_line_terminator = EOF;

	      shell_input_line[i] = '\0';
	      break;
	    }

	  if (truncating == 0 || c == '\n')
	    shell_input_line[i++] = c;

	  if (c == '\n')
	    {
	      shell_input_line[--i] = '\0';
	      current_command_line_count++;
	      break;
	    }

	  last_was_backslash = last_was_backslash == 0 && c == '\\';
	}

      shell_input_line_index = 0;
      shell_input_line_len = i;		 

      set_line_mbstate ();

#if defined (HISTORY)
      if (remember_on_history && shell_input_line && shell_input_line[0])
	{
	  char *expansions;
#  if defined (BANG_HISTORY)
	   
	  if (current_delimiter (dstack) == '\'')
	    history_quoting_state = '\'';
	  else if (current_delimiter (dstack) == '"')
	    history_quoting_state = '"';
	  else
	    history_quoting_state = 0;
#  endif
	   
	  expansions = pre_process_line (shell_input_line, 1, 1);
#  if defined (BANG_HISTORY)
	  history_quoting_state = 0;
#  endif
	  if (expansions != shell_input_line)
	    {
	      free (shell_input_line);
	      shell_input_line = expansions;
	      shell_input_line_len = shell_input_line ?
					strlen (shell_input_line) : 0;
	      if (shell_input_line_len == 0)
		current_command_line_count--;

	       
	      shell_input_line_size = shell_input_line_len;

	      set_line_mbstate ();
	    }
	}
       
      else if (remember_on_history && shell_input_line &&
	       shell_input_line[0] == '\0' &&
	       current_command_line_count > 1)
	{
	  if (current_delimiter (dstack))
	     
	    maybe_add_history (shell_input_line);
	  else
	    {
	      char *hdcs;
	      hdcs = history_delimiting_chars (shell_input_line);
	      if (hdcs && hdcs[0] == ';')
		maybe_add_history (shell_input_line);
	    }
	}

#endif  

      if (shell_input_line)
	{
	   
	  if (echo_input_at_read && (shell_input_line[0] ||
				       shell_input_line_terminator != EOF) &&
				     shell_eof_token == 0)
	    fprintf (stderr, "%s\n", shell_input_line);
	}
      else
	{
	  shell_input_line_size = 0;
	  prompt_string_pointer = &current_prompt_string;
	  if (SHOULD_PROMPT ())
	    prompt_again (0);
	  goto restart_read;
	}

       
      if (shell_input_line_terminator != EOF)
	{
	  if (shell_input_line_size < SIZE_MAX-3 && (shell_input_line_len+3 > shell_input_line_size))
	    shell_input_line = (char *)xrealloc (shell_input_line,
					1 + (shell_input_line_size += 2));

	   
	  if (bash_input.type == st_string && expanding_alias() == 0 && last_was_backslash && c == EOF && remove_quoted_newline)
	    shell_input_line[shell_input_line_len] = '\\';
	  else
	    shell_input_line[shell_input_line_len] = '\n';
	  shell_input_line[shell_input_line_len + 1] = '\0';

#if defined (HANDLE_MULTIBYTE)
	   
	  EXTEND_SHELL_INPUT_LINE_PROPERTY();
	  shell_input_line_property[shell_input_line_len] = 1;
#endif
	}
    }

next_alias_char:
  if (shell_input_line_index == 0)
    unquoted_backslash = 0;

  uc = shell_input_line[shell_input_line_index];

  if (uc)
    {
      unquoted_backslash = unquoted_backslash == 0 && uc == '\\';
      shell_input_line_index++;
    }

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
   
   

   
#ifndef OLD_ALIAS_HACK
  if (uc == 0 && pushed_string_list && pushed_string_list->flags != PSH_SOURCE &&
      pushed_string_list->flags != PSH_DPAREN &&
      (parser_state & PST_COMMENT) == 0 &&
      (parser_state & PST_ENDALIAS) == 0 &&	 
      shell_input_line_index > 0 &&
      shellblank (shell_input_line[shell_input_line_index-1]) == 0 &&
      shell_input_line[shell_input_line_index-1] != '\n' &&
      unquoted_backslash == 0 &&
      shellmeta (shell_input_line[shell_input_line_index-1]) == 0 &&
      (current_delimiter (dstack) != '\'' && current_delimiter (dstack) != '"'))
    {
      parser_state |= PST_ENDALIAS;
       
      if (shell_input_line_index == shell_input_line_len && last_shell_getc_is_singlebyte == 0)
	{
#if 0
	  EXTEND_SHELL_INPUT_LINE_PROPERTY();
	  shell_input_line_property[shell_input_line_len++] = 1;
	   
	  RESIZE_MALLOCED_BUFFER (shell_input_line, shell_input_line_index, 2, shell_input_line_size, 16);
          shell_input_line[++shell_input_line_index] = '\0';	 
#else
	  shell_input_line_property[shell_input_line_index - 1] = 1;
#endif
	}
      return ' ';	 
    }
#endif

pop_alias:
#endif  
   
  if (uc == 0 && pushed_string_list && pushed_string_list->flags != PSH_SOURCE)
    {
      parser_state &= ~PST_ENDALIAS;
      pop_string ();
      uc = shell_input_line[shell_input_line_index];
      if (uc)
	shell_input_line_index++;
    }

  if MBTEST(uc == '\\' && remove_quoted_newline && shell_input_line[shell_input_line_index] == '\n')
    {
	if (SHOULD_PROMPT ())
	  prompt_again (0);
	line_number++;

	 
#if defined (ALIAS)
	if (expanding_alias () && shell_input_line[shell_input_line_index+1] == '\0')
	  {
	    uc = 0;
	    goto pop_alias;
	  }
	else if (expanding_alias () && shell_input_line[shell_input_line_index+1] != '\0')
	  {
	    shell_input_line_index++;	 
	    goto next_alias_char;	 
	  }
	else
#endif 
	  goto restart_read;
    }

  if (uc == 0 && shell_input_line_terminator == EOF)
    return ((shell_input_line_index != 0) ? '\n' : EOF);

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
   
  if (uc == 0 && bash_input.type == st_string && *bash_input.location.string &&
      pushed_string_list && pushed_string_list->flags == PSH_SOURCE &&
      shell_input_line_terminator == 0)
    {
      shell_input_line_index = 0;
      goto restart_read;
    }
#endif

  return (uc);
}

 
static void
shell_ungetc (c)
     int c;
{
  if (shell_input_line && shell_input_line_index)
    shell_input_line[--shell_input_line_index] = c;
  else
    eol_ungetc_lookahead = c;
}

 
void
shell_ungets (s)
     char *s;
{
  size_t slen, chars_left;

  slen = strlen (s);

  if (shell_input_line[shell_input_line_index] == '\0')
    {
       
      if (shell_input_line_size <= slen)
	RESIZE_MALLOCED_BUFFER (shell_input_line, shell_input_line_index, slen + 1, shell_input_line_size, 64);
      strcpy (shell_input_line, s);
      shell_input_line_index = 0;
      shell_input_line_len = slen;
      shell_input_line_terminator = 0;
    }
  else if (shell_input_line_index >= slen)
    {
       
      while (slen > 0)
        shell_input_line[--shell_input_line_index] = s[--slen];
    }
  else if (s[slen - 1] == '\n')
    {
      push_string (savestring (s), 0, (alias_t *)NULL);
       
      return;
    }
  else
    {
       
      INTERNAL_DEBUG (("shell_ungets: not at end of shell_input_line"));

      chars_left = shell_input_line_len - shell_input_line_index;
      if (shell_input_line_size <= (slen + chars_left))
	RESIZE_MALLOCED_BUFFER (shell_input_line, shell_input_line_index, chars_left + slen + 1, shell_input_line_size, 64);
      memmove (shell_input_line + slen, shell_input_line + shell_input_line_index, shell_input_line_len - shell_input_line_index);
      strcpy (shell_input_line, s);
      shell_input_line_index = 0;
      shell_input_line_len = strlen (shell_input_line);	 
    }

#if defined (HANDLE_MULTIBYTE)
  set_line_mbstate ();	 
#endif
}

char *
parser_remaining_input ()
{
  if (shell_input_line == 0)
    return 0;
  if ((int)shell_input_line_index < 0 || shell_input_line_index >= shell_input_line_len)
    return "";	 
  return (shell_input_line + shell_input_line_index);
}

#ifdef INCLUDE_UNUSED
 
static void
shell_ungetchar ()
{
  if (shell_input_line && shell_input_line_index)
    shell_input_line_index--;
}
#endif

 
static void
discard_until (character)
     int character;
{
  int c;

  while ((c = shell_getc (0)) != EOF && c != character)
    ;

  if (c != EOF)
    shell_ungetc (c);
}

void
execute_variable_command (command, vname)
     char *command, *vname;
{
  char *last_lastarg;
  sh_parser_state_t ps;

  save_parser_state (&ps);
  last_lastarg = get_string_value ("_");
  if (last_lastarg)
    last_lastarg = savestring (last_lastarg);

  parse_and_execute (savestring (command), vname, SEVAL_NONINT|SEVAL_NOHIST|SEVAL_NOOPTIMIZE);

  restore_parser_state (&ps);
  bind_variable ("_", last_lastarg, 0);
  FREE (last_lastarg);

  if (token_to_read == '\n')	 
    token_to_read = 0;
}

void
push_token (x)
     int x;
{
  two_tokens_ago = token_before_that;
  token_before_that = last_read_token;
  last_read_token = current_token;

  current_token = x;
}

 
static char *token = (char *)NULL;

 
static size_t token_buffer_size;

 
#define READ 0
#define RESET 1
#define prompt_is_ps1 \
      (!prompt_string_pointer || prompt_string_pointer == &ps1_prompt)

 
static int
yylex ()
{
  if (interactive && (current_token == 0 || current_token == '\n'))
    {
       
      if (prompt_is_ps1 && parse_and_execute_level == 0 && time_to_check_mail ())
	{
	  check_mail ();
	  reset_mail_timer ();
	}

       
      if (token_to_read == 0 && SHOULD_PROMPT ())
	prompt_again (0);
    }

  two_tokens_ago = token_before_that;
  token_before_that = last_read_token;
  last_read_token = current_token;
  current_token = read_token (READ);

  if ((parser_state & PST_EOFTOKEN) && current_token == shell_eof_token)
    {
       
      return (current_token);
    }

  if (current_token < 0)
#if defined (YYERRCODE) && !defined (YYUNDEF)
    current_token = EOF_Reached ? YYEOF : YYERRCODE;
#else
    current_token = EOF_Reached ? YYEOF : YYUNDEF;
#endif

  return (current_token);
}

 
static int esacs_needed_count;

 
static int expecting_in_token;

static void
push_heredoc (r)
     REDIRECT *r;
{
  if (need_here_doc >= HEREDOC_MAX)
    {
      last_command_exit_value = EX_BADUSAGE;
      need_here_doc = 0;
      report_syntax_error (_("maximum here-document count exceeded"));
      reset_parser ();
      exit_shell (last_command_exit_value);
    }
  redir_stack[need_here_doc++] = r;
}

void
gather_here_documents ()
{
  int r;

  r = 0;
  here_doc_first_line = 1;
  while (need_here_doc > 0)
    {
      parser_state |= PST_HEREDOC;
      make_here_document (redir_stack[r++], line_number);
      parser_state &= ~PST_HEREDOC;
      need_here_doc--;
      redir_stack[r - 1] = 0;		 
    }
  here_doc_first_line = 0;		 
}

 
static int open_brace_count;

 

 
#define parsing_redirection(token) \
  (token == '<' || token == '>' || \
   token == GREATER_GREATER || token == GREATER_BAR || \
   token == LESS_GREATER || token == LESS_LESS_MINUS || \
   token == LESS_LESS || token == LESS_LESS_LESS || \
   token == LESS_AND || token == GREATER_AND || token == AND_GREATER)

 
#define command_token_position(token) \
  (((token) == ASSIGNMENT_WORD) || \
   ((parser_state&PST_REDIRLIST) && parsing_redirection(token) == 0) || \
   ((token) != SEMI_SEMI && (token) != SEMI_AND && (token) != SEMI_SEMI_AND && reserved_word_acceptable(token)))

 
#define assignment_acceptable(token) \
  (command_token_position(token) && ((parser_state & PST_CASEPAT) == 0))

 
#define CHECK_FOR_RESERVED_WORD(tok) \
  do { \
    if (!dollar_present && !quoted && \
	reserved_word_acceptable (last_read_token)) \
      { \
	int i; \
	for (i = 0; word_token_alist[i].word != (char *)NULL; i++) \
	  if (STREQ (tok, word_token_alist[i].word)) \
	    { \
	      if ((parser_state & PST_CASEPAT) && (word_token_alist[i].token != ESAC)) \
		break; \
	      if (word_token_alist[i].token == TIME && time_command_acceptable () == 0) \
		break; \
	      if ((parser_state & PST_CASEPAT) && last_read_token == '|' && word_token_alist[i].token == ESAC) \
		break;   \
	      if ((parser_state & PST_CASEPAT) && last_read_token == '(' && word_token_alist[i].token == ESAC)   \
		break;   \
	      if (word_token_alist[i].token == ESAC) { \
		parser_state &= ~(PST_CASEPAT|PST_CASESTMT); \
		esacs_needed_count--; \
	      } else if (word_token_alist[i].token == CASE) \
		parser_state |= PST_CASESTMT; \
	      else if (word_token_alist[i].token == COND_END) \
		parser_state &= ~(PST_CONDCMD|PST_CONDEXPR); \
	      else if (word_token_alist[i].token == COND_START) \
		parser_state |= PST_CONDCMD; \
	      else if (word_token_alist[i].token == '{') \
		open_brace_count++; \
	      else if (word_token_alist[i].token == '}' && open_brace_count) \
		open_brace_count--; \
	      return (word_token_alist[i].token); \
	    } \
      } \
  } while (0)

#if defined (ALIAS)

     

static char *
mk_alexpansion (s)
     char *s;
{
  int l;
  char *r;

  l = strlen (s);
  r = xmalloc (l + 2);
  strcpy (r, s);
#ifdef OLD_ALIAS_HACK
   
   
  if (l > 0 && r[l - 1] != ' ' && r[l - 1] != '\n' && shellmeta(r[l - 1]) == 0)
    r[l++] = ' ';
#endif
  r[l] = '\0';
  return r;
}

static int
alias_expand_token (tokstr)
     char *tokstr;
{
  char *expanded;
  alias_t *ap;

#if 0
  if (((parser_state & PST_ALEXPNEXT) || command_token_position (last_read_token)) &&
	(parser_state & PST_CASEPAT) == 0)
#else
  if ((parser_state & PST_ALEXPNEXT) || assignment_acceptable (last_read_token))
#endif
    {
      ap = find_alias (tokstr);

       
      if (ap && (ap->flags & AL_BEINGEXPANDED))
	return (NO_EXPANSION);

#ifdef OLD_ALIAS_HACK
       
#endif
      expanded = ap ? mk_alexpansion (ap->value) : (char *)NULL;

      if (expanded)
	{
	  push_string (expanded, ap->flags & AL_EXPANDNEXT, ap);
	  return (RE_READ_TOKEN);
	}
      else
	 
	return (NO_EXPANSION);
    }
  return (NO_EXPANSION);
}
#endif  

static int
time_command_acceptable ()
{
#if defined (COMMAND_TIMING)
  int i;

  if (posixly_correct && shell_compatibility_level > 41)
    {
       
      i = shell_input_line_index;
      while (i < shell_input_line_len && (shell_input_line[i] == ' ' || shell_input_line[i] == '\t'))
        i++;
      if (shell_input_line[i] == '-')
	return 0;
    }

  switch (last_read_token)
    {
    case 0:
    case ';':
    case '\n':
      if (token_before_that == '|')
	return (0);
       
    case AND_AND:
    case OR_OR:
    case '&':
    case WHILE:
    case DO:
    case UNTIL:
    case IF:
    case THEN:
    case ELIF:
    case ELSE:
    case '{':		 
    case '(':		 
    case ')':		 
    case BANG:		 
    case TIME:		 
    case TIMEOPT:	 
    case TIMEIGN:	 
    case DOLPAREN:
      return 1;
    default:
      return 0;
    }
#else
  return 0;
#endif  
}

 

static int
special_case_tokens (tokstr)
     char *tokstr;
{
   
  if ((last_read_token == WORD) &&
#if defined (SELECT_COMMAND)
      ((token_before_that == FOR) || (token_before_that == CASE) || (token_before_that == SELECT)) &&
#else
      ((token_before_that == FOR) || (token_before_that == CASE)) &&
#endif
      (tokstr[0] == 'i' && tokstr[1] == 'n' && tokstr[2] == 0))
    {
      if (token_before_that == CASE)
	{
	  parser_state |= PST_CASEPAT;
	  esacs_needed_count++;
	}
      if (expecting_in_token)
	expecting_in_token--;
      return (IN);
    }

   
   
  if (expecting_in_token && (last_read_token == WORD || last_read_token == '\n') &&
      (tokstr[0] == 'i' && tokstr[1] == 'n' && tokstr[2] == 0))
    {
      if (parser_state & PST_CASESTMT)
	{
	  parser_state |= PST_CASEPAT;
	  esacs_needed_count++;
	}
      expecting_in_token--;
      return (IN);
    }
   
  else if (expecting_in_token && (last_read_token == '\n' || last_read_token == ';') &&
    (tokstr[0] == 'd' && tokstr[1] == 'o' && tokstr[2] == '\0'))
    {
      expecting_in_token--;
      return (DO);
    }

   
  if (last_read_token == WORD &&
#if defined (SELECT_COMMAND)
      (token_before_that == FOR || token_before_that == SELECT) &&
#else
      (token_before_that == FOR) &&
#endif
      (tokstr[0] == 'd' && tokstr[1] == 'o' && tokstr[2] == '\0'))
    {
      if (expecting_in_token)
	expecting_in_token--;
      return (DO);
    }

   
  if (esacs_needed_count)
    {
      if (last_read_token == IN && STREQ (tokstr, "esac"))
	{
	  esacs_needed_count--;
	  parser_state &= ~PST_CASEPAT;
	  return (ESAC);
	}
    }

   
  if (parser_state & PST_ALLOWOPNBRC)
    {
      parser_state &= ~PST_ALLOWOPNBRC;
      if (tokstr[0] == '{' && tokstr[1] == '\0')		 
	{
	  open_brace_count++;
	  function_bstart = line_number;
	  return ('{');					 
	}
    }

   
  if (last_read_token == ARITH_FOR_EXPRS && tokstr[0] == 'd' && tokstr[1] == 'o' && !tokstr[2])
    return (DO);
  if (last_read_token == ARITH_FOR_EXPRS && tokstr[0] == '{' && tokstr[1] == '\0')	 
    {
      open_brace_count++;
      return ('{');			 
    }

  if (open_brace_count && reserved_word_acceptable (last_read_token) && tokstr[0] == '}' && !tokstr[1])
    {
      open_brace_count--;		 
      return ('}');
    }

#if defined (COMMAND_TIMING)
   
  if (last_read_token == TIME && tokstr[0] == '-' && tokstr[1] == 'p' && !tokstr[2])
    return (TIMEOPT);
   
  if (last_read_token == TIME && tokstr[0] == '-' && tokstr[1] == '-' && !tokstr[2])
    return (TIMEIGN);
   
  if (last_read_token == TIMEOPT && tokstr[0] == '-' && tokstr[1] == '-' && !tokstr[2])
    return (TIMEIGN);
#endif

#if defined (COND_COMMAND)  
  if ((parser_state & PST_CONDEXPR) && tokstr[0] == ']' && tokstr[1] == ']' && tokstr[2] == '\0')
    return (COND_END);
#endif

  return (-1);
}

 
void
reset_parser ()
{
  dstack.delimiter_depth = 0;	 
  open_brace_count = 0;

#if defined (EXTENDED_GLOB)
   
  if (parser_state & (PST_EXTPAT|PST_CMDSUBST))
    extended_glob = extglob_flag;
#endif
  if (parser_state & (PST_CMDSUBST|PST_STRING))
    expand_aliases = expaliases_flag;

  parser_state = 0;
  here_doc_first_line = 0;

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  if (pushed_string_list)
    free_string_list ();
#endif  

   
  if (shell_input_line)
    {
      free (shell_input_line);
      shell_input_line = (char *)NULL;
      shell_input_line_size = shell_input_line_index = 0;
    }

  FREE (word_desc_to_read);
  word_desc_to_read = (WORD_DESC *)NULL;

  eol_ungetc_lookahead = 0;

   
  need_here_doc = 0;
  redir_stack[0] = 0;
  esacs_needed_count = expecting_in_token = 0;

  current_token = '\n';		 
  last_read_token = '\n';
  token_to_read = '\n';
}

void
reset_readahead_token ()
{
  if (token_to_read == '\n')
    token_to_read = 0;
}

 
static int
read_token (command)
     int command;
{
  int character;		 
  int peek_char;		 
  int result;			 

  if (command == RESET)
    {
      reset_parser ();
      return ('\n');
    }

  if (token_to_read)
    {
      result = token_to_read;
      if (token_to_read == WORD || token_to_read == ASSIGNMENT_WORD)
	{
	  yylval.word = word_desc_to_read;
	  word_desc_to_read = (WORD_DESC *)NULL;
	}
      token_to_read = 0;
      return (result);
    }

#if defined (COND_COMMAND)
  if ((parser_state & (PST_CONDCMD|PST_CONDEXPR)) == PST_CONDCMD)
    {
      cond_lineno = line_number;
      parser_state |= PST_CONDEXPR;
      yylval.command = parse_cond_command ();
      if (cond_token != COND_END)
	{
	  cond_error ();
	  return (-1);
	}
      token_to_read = COND_END;
      parser_state &= ~(PST_CONDEXPR|PST_CONDCMD);
      return (COND_CMD);
    }
#endif

#if defined (ALIAS)
   
 re_read_token:
#endif  

   
  while ((character = shell_getc (1)) != EOF && shellblank (character))
    ;

  if (character == EOF)
    {
      EOF_Reached = 1;
      return (yacc_EOF);
    }

   
  if (character == '\0' && bash_input.type == st_string && expanding_alias() == 0)
    {
      INTERNAL_DEBUG (("shell_getc: bash_input.location.string = `%s'", bash_input.location.string));
      EOF_Reached = 1;
      return (yacc_EOF);
    }

  if MBTEST(character == '#' && (!interactive || interactive_comments))
    {
       
      parser_state |= PST_COMMENT;
      discard_until ('\n');
      shell_getc (0);
      parser_state &= ~PST_COMMENT;
      character = '\n';	 
    }

  if MBTEST(character == '\n')
    {
       
      if (need_here_doc)
	gather_here_documents ();

#if defined (ALIAS)
      parser_state &= ~PST_ALEXPNEXT;
#endif  

      parser_state &= ~PST_ASSIGNOK;

      return (character);
    }

  if (parser_state & PST_REGEXP)
    goto tokword;

   
  if MBTEST(shellmeta (character))
    {
#if defined (ALIAS)
       
      if (character == '<' || character == '>')
	parser_state &= ~PST_ALEXPNEXT;
#endif  

      parser_state &= ~PST_ASSIGNOK;

       
      if ((parser_state & PST_CMDSUBST) && character == shell_eof_token)
	peek_char = shell_getc (0);
      else
	peek_char = shell_getc (1);

      if MBTEST(character == peek_char)
	{
	  switch (character)
	    {
	    case '<':
	       
	      peek_char = shell_getc (1);
	      if MBTEST(peek_char == '-')
		return (LESS_LESS_MINUS);
	      else if MBTEST(peek_char == '<')
		return (LESS_LESS_LESS);
	      else
		{
		  shell_ungetc (peek_char);
		  return (LESS_LESS);
		}

	    case '>':
	      return (GREATER_GREATER);

	    case ';':
	      parser_state |= PST_CASEPAT;
#if defined (ALIAS)
	      parser_state &= ~PST_ALEXPNEXT;
#endif  

	      peek_char = shell_getc (1);
	      if MBTEST(peek_char == '&')
		return (SEMI_SEMI_AND);
	      else
		{
		  shell_ungetc (peek_char);
		  return (SEMI_SEMI);
		}

	    case '&':
	      return (AND_AND);

	    case '|':
	      return (OR_OR);

#if defined (DPAREN_ARITHMETIC) || defined (ARITH_FOR_COMMAND)
	    case '(':		 
	      result = parse_dparen (character);
	      if (result == -2)
	        break;
	      else
	        return result;
#endif
	    }
	}
      else if MBTEST(character == '<' && peek_char == '&')
	return (LESS_AND);
      else if MBTEST(character == '>' && peek_char == '&')
	return (GREATER_AND);
      else if MBTEST(character == '<' && peek_char == '>')
	return (LESS_GREATER);
      else if MBTEST(character == '>' && peek_char == '|')
	return (GREATER_BAR);
      else if MBTEST(character == '&' && peek_char == '>')
	{
	  peek_char = shell_getc (1);
	  if MBTEST(peek_char == '>')
	    return (AND_GREATER_GREATER);
	  else
	    {
	      shell_ungetc (peek_char);
	      return (AND_GREATER);
	    }
	}
      else if MBTEST(character == '|' && peek_char == '&')
	return (BAR_AND);
      else if MBTEST(character == ';' && peek_char == '&')
	{
	  parser_state |= PST_CASEPAT;
#if defined (ALIAS)
	  parser_state &= ~PST_ALEXPNEXT;
#endif  
	  return (SEMI_AND);
	}

      shell_ungetc (peek_char);

       
      if MBTEST(character == ')' && last_read_token == '(' && token_before_that == WORD)
	{
	  parser_state |= PST_ALLOWOPNBRC;
#if defined (ALIAS)
	  parser_state &= ~PST_ALEXPNEXT;
#endif  
	  function_dstart = line_number;
	}

       
      if MBTEST(character == '(' && (parser_state & PST_CASEPAT) == 0)  
	parser_state |= PST_SUBSHELL;
       
      else if MBTEST((parser_state & PST_CASEPAT) && character == ')')
	parser_state &= ~PST_CASEPAT;
       
      else if MBTEST((parser_state & PST_SUBSHELL) && character == ')')
	parser_state &= ~PST_SUBSHELL;

#if defined (PROCESS_SUBSTITUTION)
       
      if MBTEST((character != '>' && character != '<') || peek_char != '(')  
#endif  
	return (character);
    }

   
  if MBTEST(character == '-' && (last_read_token == LESS_AND || last_read_token == GREATER_AND))
    return (character);

tokword:
   
  result = read_token_word (character);
#if defined (ALIAS)
  if (result == RE_READ_TOKEN)
    goto re_read_token;
#endif
  return result;
}

 
#define P_FIRSTCLOSE	0x0001
#define P_ALLOWESC	0x0002
#define P_DQUOTE	0x0004
#define P_COMMAND	0x0008	 
#define P_BACKQUOTE	0x0010	 
#define P_ARRAYSUB	0x0020	 
#define P_DOLBRACE	0x0040	 
#define P_ARITH		0x0080	 

 
#define LEX_WASDOL	0x0001
#define LEX_CKCOMMENT	0x0002
#define LEX_INCOMMENT	0x0004
#define LEX_PASSNEXT	0x0008
#define LEX_RESWDOK	0x0010
#define LEX_CKCASE	0x0020
#define LEX_INCASE	0x0040
#define LEX_INHEREDOC	0x0080
#define LEX_HEREDELIM	0x0100		 
#define LEX_STRIPDOC	0x0200		 
#define LEX_QUOTEDDOC	0x0400		 
#define LEX_INWORD	0x0800
#define LEX_GTLT	0x1000
#define LEX_CKESAC	0x2000		 
#define LEX_CASEWD	0x4000		 
#define LEX_PATLIST	0x8000		 

#define COMSUB_META(ch)		((ch) == ';' || (ch) == '&' || (ch) == '|')

#define CHECK_NESTRET_ERROR() \
  do { \
    if (nestret == &matched_pair_error) \
      { \
	free (ret); \
	return &matched_pair_error; \
      } \
  } while (0)

#define APPEND_NESTRET() \
  do { \
    if (nestlen) \
      { \
	RESIZE_MALLOCED_BUFFER (ret, retind, nestlen, retsize, 64); \
	strcpy (ret + retind, nestret); \
	retind += nestlen; \
      } \
  } while (0)

static char matched_pair_error;

static char *
parse_matched_pair (qc, open, close, lenp, flags)
     int qc;	 
     int open, close;
     int *lenp, flags;
{
  int count, ch, prevch, tflags;
  int nestlen, ttranslen, start_lineno;
  char *ret, *nestret, *ttrans;
  int retind, retsize, rflags;
  int dolbrace_state;

  dolbrace_state = (flags & P_DOLBRACE) ? DOLBRACE_PARAM : 0;

 
  count = 1;
  tflags = 0;

  if ((flags & P_COMMAND) && qc != '`' && qc != '\'' && qc != '"' && (flags & P_DQUOTE) == 0)
    tflags |= LEX_CKCOMMENT;

   
  rflags = (qc == '"') ? P_DQUOTE : (flags & P_DQUOTE);

  ret = (char *)xmalloc (retsize = 64);
  retind = 0;

  start_lineno = line_number;
  ch = EOF;		 
  while (count)
    {
      prevch = ch;
      ch = shell_getc (qc != '\'' && (tflags & (LEX_PASSNEXT)) == 0);

      if (ch == EOF)
	{
	  free (ret);
	  parser_error (start_lineno, _("unexpected EOF while looking for matching `%c'"), close);
	  EOF_Reached = 1;	 
	  parser_state |= PST_NOERROR;   
	  return (&matched_pair_error);
	}

       
      if MBTEST(ch == '\n' && SHOULD_PROMPT ())
	prompt_again (0);

       
      if (tflags & LEX_INCOMMENT)
	{
	   
	  RESIZE_MALLOCED_BUFFER (ret, retind, 1, retsize, 64);
	  ret[retind++] = ch;

	  if MBTEST(ch == '\n')
	    tflags &= ~LEX_INCOMMENT;

	  continue;
	}

       
      else if MBTEST((tflags & LEX_CKCOMMENT) && (tflags & LEX_INCOMMENT) == 0 && ch == '#' && (retind == 0 || ret[retind-1] == '\n' || shellblank (ret[retind - 1])))
	tflags |= LEX_INCOMMENT;

      if (tflags & LEX_PASSNEXT)		 
	{
	  tflags &= ~LEX_PASSNEXT;
	   
	  if MBTEST(qc != '\'' && ch == '\n')	 
	    {
	      if (retind > 0)
		retind--;	 
	      continue;
	    }

	  RESIZE_MALLOCED_BUFFER (ret, retind, 2, retsize, 64);
	  if MBTEST(ch == CTLESC)
	    ret[retind++] = CTLESC;
	  ret[retind++] = ch;
	  continue;
	}
       
      else if MBTEST((parser_state & PST_REPARSE) && open == '\'' && (ch == CTLESC || ch == CTLNUL))
	{
	  RESIZE_MALLOCED_BUFFER (ret, retind, 1, retsize, 64);
	  ret[retind++] = ch;
	  continue;
	}
      else if MBTEST(ch == CTLESC || ch == CTLNUL)	 
	{
	  RESIZE_MALLOCED_BUFFER (ret, retind, 2, retsize, 64);
	  ret[retind++] = CTLESC;
	  ret[retind++] = ch;
	  continue;
	}
      else if MBTEST(ch == close)		 
	count--;
       
      else if MBTEST(open != close && (tflags & LEX_WASDOL) && open == '{' && ch == open)  
	count++;
      else if MBTEST(((flags & P_FIRSTCLOSE) == 0) && ch == open)	 
	count++;

       
      RESIZE_MALLOCED_BUFFER (ret, retind, 1, retsize, 64);
      ret[retind++] = ch;

       
      if (count == 0)
	break;

      if (open == '\'')			 
	{
	  if MBTEST((flags & P_ALLOWESC) && ch == '\\')
	    tflags |= LEX_PASSNEXT;
	  continue;
	}

      if MBTEST(ch == '\\')			 
	tflags |= LEX_PASSNEXT;

       
       
      if (flags & P_DOLBRACE)
        {
           
	  if MBTEST(dolbrace_state == DOLBRACE_PARAM && ch == '%' && retind > 1)
	    dolbrace_state = DOLBRACE_QUOTE;
           
	  else if MBTEST(dolbrace_state == DOLBRACE_PARAM && ch == '#' && retind > 1)
	    dolbrace_state = DOLBRACE_QUOTE;
           
	  else if MBTEST(dolbrace_state == DOLBRACE_PARAM && ch == '/' && retind > 1)
	    dolbrace_state = DOLBRACE_QUOTE2;	 
           
	  else if MBTEST(dolbrace_state == DOLBRACE_PARAM && ch == '^' && retind > 1)
	    dolbrace_state = DOLBRACE_QUOTE;
           
	  else if MBTEST(dolbrace_state == DOLBRACE_PARAM && ch == ',' && retind > 1)
	    dolbrace_state = DOLBRACE_QUOTE;
	  else if MBTEST(dolbrace_state == DOLBRACE_PARAM && strchr ("#%^,~:-=?+/", ch) != 0)
	    dolbrace_state = DOLBRACE_OP;
	  else if MBTEST(dolbrace_state == DOLBRACE_OP && strchr ("#%^,~:-=?+/", ch) == 0)
	    dolbrace_state = DOLBRACE_WORD;
        }

       
       
      if MBTEST(posixly_correct && shell_compatibility_level > 41 && dolbrace_state != DOLBRACE_QUOTE && dolbrace_state != DOLBRACE_QUOTE2 && (flags & P_DQUOTE) && (flags & P_DOLBRACE) && ch == '\'')
	continue;

       
      if (open != close)		 
	{
	  if MBTEST(shellquote (ch))
	    {
	       
	      push_delimiter (dstack, ch);
	      if MBTEST((tflags & LEX_WASDOL) && ch == '\'')	 
		nestret = parse_matched_pair (ch, ch, ch, &nestlen, P_ALLOWESC|rflags);
	      else
		nestret = parse_matched_pair (ch, ch, ch, &nestlen, rflags);
	      pop_delimiter (dstack);
	      CHECK_NESTRET_ERROR ();

	      if MBTEST((tflags & LEX_WASDOL) && ch == '\'' && (extended_quote || (rflags & P_DQUOTE) == 0 || dolbrace_state == DOLBRACE_QUOTE || dolbrace_state == DOLBRACE_QUOTE2))
		{
		   
		   
		  ttrans = ansiexpand (nestret, 0, nestlen - 1, &ttranslen);
		  free (nestret);

		   
		   
		  if ((shell_compatibility_level > 42) && (rflags & P_DQUOTE) && (dolbrace_state == DOLBRACE_QUOTE2 || dolbrace_state == DOLBRACE_QUOTE) && (flags & P_DOLBRACE))
		    {
		      nestret = sh_single_quote (ttrans);
		      free (ttrans);
		      nestlen = strlen (nestret);
		    }
#if 0  
		   
		  else if ((rflags & P_DQUOTE) && (dolbrace_state == DOLBRACE_PARAM) && (flags & P_DOLBRACE))
		    {
		      nestret = sh_single_quote (ttrans);
		      free (ttrans);
		      nestlen = strlen (nestret);
		    }
#endif
		  else if ((rflags & P_DQUOTE) == 0)
		    {
		      nestret = sh_single_quote (ttrans);
		      free (ttrans);
		      nestlen = strlen (nestret);
		    }
		  else
		    {
		       
		      nestret = ttrans;
		      nestlen = ttranslen;
		    }
		  retind -= 2;		 
		}
#if defined (TRANSLATABLE_STRINGS)
	      else if MBTEST((tflags & LEX_WASDOL) && ch == '"' && (extended_quote || (rflags & P_DQUOTE) == 0))
		{
		   
		   
		  ttrans = locale_expand (nestret, 0, nestlen - 1, start_lineno, &ttranslen);
		  free (nestret);

		   
		  if (singlequote_translations &&
		        ((nestlen - 1) != ttranslen || STREQN (nestret, ttrans, ttranslen) == 0))
		    {
		      if ((rflags & P_DQUOTE) == 0)
			nestret = sh_single_quote (ttrans);
		      else if ((rflags & P_DQUOTE) && (dolbrace_state == DOLBRACE_QUOTE2) && (flags & P_DOLBRACE))
			nestret = sh_single_quote (ttrans);
		      else
			 
			nestret = sh_backslash_quote_for_double_quotes (ttrans, 0);
		    }
		  else
		    nestret = sh_mkdoublequoted (ttrans, ttranslen, 0);
		  free (ttrans);
		  nestlen = strlen (nestret);
		  retind -= 2;		 
		}
#endif  

	      APPEND_NESTRET ();
	      FREE (nestret);
	    }
	  else if ((flags & (P_ARRAYSUB|P_DOLBRACE)) && (tflags & LEX_WASDOL) && (ch == '(' || ch == '{' || ch == '['))	 
	    goto parse_dollar_word;
	  else if ((flags & P_ARITH) && (tflags & LEX_WASDOL) && ch == '(')  
	     
	    goto parse_dollar_word;
#if defined (PROCESS_SUBSTITUTION)
	   
	  else if ((flags & (P_ARRAYSUB|P_DOLBRACE)) && (tflags & LEX_GTLT) && (ch == '('))	 
	    goto parse_dollar_word;
#endif
	}
       
       
      else if MBTEST(open == '"' && ch == '`')
	{
	  nestret = parse_matched_pair (0, '`', '`', &nestlen, rflags);

	  CHECK_NESTRET_ERROR ();
	  APPEND_NESTRET ();

	  FREE (nestret);
	}
      else if MBTEST(open != '`' && (tflags & LEX_WASDOL) && (ch == '(' || ch == '{' || ch == '['))	 
	 
	{
parse_dollar_word:
	  if (open == ch)	 
	    count--;
	  if (ch == '(')		 
	    nestret = parse_comsub (0, '(', ')', &nestlen, (rflags|P_COMMAND) & ~P_DQUOTE);
	  else if (ch == '{')		 
	    nestret = parse_matched_pair (0, '{', '}', &nestlen, P_FIRSTCLOSE|P_DOLBRACE|rflags);
	  else if (ch == '[')		 
	    nestret = parse_matched_pair (0, '[', ']', &nestlen, rflags|P_ARITH);

	  CHECK_NESTRET_ERROR ();
	  APPEND_NESTRET ();

	  FREE (nestret);
	}
#if defined (PROCESS_SUBSTITUTION)
      if MBTEST((ch == '<' || ch == '>') && (tflags & LEX_GTLT) == 0)
	tflags |= LEX_GTLT;
      else
	tflags &= ~LEX_GTLT;
#endif
      if MBTEST(ch == '$' && (tflags & LEX_WASDOL) == 0)
	tflags |= LEX_WASDOL;
      else
	tflags &= ~LEX_WASDOL;
    }

  ret[retind] = '\0';
  if (lenp)
    *lenp = retind;
 
  return ret;
}

#if defined (DEBUG)
static void
dump_tflags (flags)
     int flags;
{
  int f;

  f = flags;
  fprintf (stderr, "%d -> ", f);
  if (f & LEX_WASDOL)
    {
      f &= ~LEX_WASDOL;
      fprintf (stderr, "LEX_WASDOL%s", f ? "|" : "");
    }
  if (f & LEX_CKCOMMENT)
    {
      f &= ~LEX_CKCOMMENT;
      fprintf (stderr, "LEX_CKCOMMENT%s", f ? "|" : "");
    }
  if (f & LEX_INCOMMENT)
    {
      f &= ~LEX_INCOMMENT;
      fprintf (stderr, "LEX_INCOMMENT%s", f ? "|" : "");
    }
  if (f & LEX_PASSNEXT)
    {
      f &= ~LEX_PASSNEXT;
      fprintf (stderr, "LEX_PASSNEXT%s", f ? "|" : "");
    }
  if (f & LEX_RESWDOK)
    {
      f &= ~LEX_RESWDOK;
      fprintf (stderr, "LEX_RESWDOK%s", f ? "|" : "");
    }
  if (f & LEX_CKCASE)
    {
      f &= ~LEX_CKCASE;
      fprintf (stderr, "LEX_CKCASE%s", f ? "|" : "");
    }
  if (f & LEX_CKESAC)
    {
      f &= ~LEX_CKESAC;
      fprintf (stderr, "LEX_CKESAC%s", f ? "|" : "");
    }
  if (f & LEX_INCASE)
    {
      f &= ~LEX_INCASE;
      fprintf (stderr, "LEX_INCASE%s", f ? "|" : "");
    }
  if (f & LEX_CASEWD)
    {
      f &= ~LEX_CASEWD;
      fprintf (stderr, "LEX_CASEWD%s", f ? "|" : "");
    }
  if (f & LEX_PATLIST)
    {
      f &= ~LEX_PATLIST;
      fprintf (stderr, "LEX_PATLIST%s", f ? "|" : "");
    }
  if (f & LEX_INHEREDOC)
    {
      f &= ~LEX_INHEREDOC;
      fprintf (stderr, "LEX_INHEREDOC%s", f ? "|" : "");
    }
  if (f & LEX_HEREDELIM)
    {
      f &= ~LEX_HEREDELIM;
      fprintf (stderr, "LEX_HEREDELIM%s", f ? "|" : "");
    }
  if (f & LEX_STRIPDOC)
    {
      f &= ~LEX_STRIPDOC;
      fprintf (stderr, "LEX_WASDOL%s", f ? "|" : "");
    }
  if (f & LEX_QUOTEDDOC)
    {
      f &= ~LEX_QUOTEDDOC;
      fprintf (stderr, "LEX_QUOTEDDOC%s", f ? "|" : "");
    }
  if (f & LEX_INWORD)
    {
      f &= ~LEX_INWORD;
      fprintf (stderr, "LEX_INWORD%s", f ? "|" : "");
    }

  fprintf (stderr, "\n");
  fflush (stderr);
}
#endif

 
static char *
parse_comsub (qc, open, close, lenp, flags)
     int qc;	 
     int open, close;
     int *lenp, flags;
{
  int peekc, r;
  int start_lineno, local_extglob, was_extpat;
  char *ret, *tcmd;
  int retlen;
  sh_parser_state_t ps;
  STRING_SAVER *saved_strings;
  COMMAND *saved_global, *parsed_command;

   
  if (open == '(')		 
    {
      peekc = shell_getc (1);
      shell_ungetc (peekc);
      if (peekc == '(')		 
	return (parse_matched_pair (qc, open, close, lenp, P_ARITH));
    }

 

   
  start_lineno = line_number;

  save_parser_state (&ps);

  was_extpat = (parser_state & PST_EXTPAT);

   
  parser_state &= ~(PST_REGEXP|PST_EXTPAT|PST_CONDCMD|PST_CONDEXPR|PST_COMPASSIGN);
   
  parser_state &= ~(PST_CASEPAT|PST_ALEXPNEXT|PST_SUBSHELL|PST_REDIRLIST);
   
  parser_state |= PST_CMDSUBST|PST_EOFTOKEN|PST_NOEXPAND;

   
  shell_eof_token = close;

  saved_global = global_command;		 
  global_command = (COMMAND *)NULL;

   
  need_here_doc = 0;
  esacs_needed_count = expecting_in_token = 0;

   
  if (expand_aliases)
    expand_aliases = posixly_correct != 0;
#if defined (EXTENDED_GLOB)
   
  if (shell_compatibility_level <= 51 && was_extpat == 0)
    {
      local_extglob = extended_glob;
      extended_glob = 1;
    }
#endif

  current_token = '\n';				 
  token_to_read = DOLPAREN;			 

  r = yyparse ();

  if (need_here_doc > 0)
    {
      internal_warning ("command substitution: %d unterminated here-document%s", need_here_doc, (need_here_doc == 1) ? "" : "s");
      gather_here_documents ();	 
    }

#if defined (EXTENDED_GLOB)
  if (shell_compatibility_level <= 51 && was_extpat == 0)
    extended_glob = local_extglob;
#endif

  parsed_command = global_command;

  if (EOF_Reached)
    {
      shell_eof_token = ps.eof_token;
      expand_aliases = ps.expand_aliases;

       
      parser_state |= PST_NOERROR;
      return (&matched_pair_error);
    }
  else if (r != 0)
    {
       
       
      if (last_command_exit_value == 0)
	last_command_exit_value = EXECUTION_FAILURE;
      set_exit_status (last_command_exit_value);
      if (interactive_shell == 0)
	jump_to_top_level (FORCE_EOF);	 
      else
	{
	  shell_eof_token = ps.eof_token;
	  expand_aliases = ps.expand_aliases;

	  jump_to_top_level (DISCARD);	 
	}
    }

  if (current_token != shell_eof_token)
    {
INTERNAL_DEBUG(("current_token (%d) != shell_eof_token (%c)", current_token, shell_eof_token));
      token_to_read = current_token;

       
      shell_eof_token = ps.eof_token;
      expand_aliases = ps.expand_aliases;

      return (&matched_pair_error);
    }

   
  saved_strings = pushed_string_list;
  restore_parser_state (&ps);
  pushed_string_list = saved_strings;

  tcmd = print_comsub (parsed_command);		 
  retlen = strlen (tcmd);
  if (tcmd[0] == '(')			 
    retlen++;
  ret = xmalloc (retlen + 2);
  if (tcmd[0] == '(')			 
    {
      ret[0] = ' ';
      strcpy (ret + 1, tcmd);
    }
  else
    strcpy (ret, tcmd);
  ret[retlen++] = ')';
  ret[retlen] = '\0';

  dispose_command (parsed_command);
  global_command = saved_global;

  if (lenp)
    *lenp = retlen;

 
  return ret;
}

 
char *
xparse_dolparen (base, string, indp, flags)
     char *base;
     char *string;
     int *indp;
     int flags;
{
  sh_parser_state_t ps;
  sh_input_line_state_t ls;
  int orig_ind, nc, sflags, start_lineno, local_extglob;
  char *ret, *ep, *ostring;

 
  orig_ind = *indp;
  ostring = string;
  start_lineno = line_number;

  if (*string == 0)
    {
      if (flags & SX_NOALLOC) 
	return (char *)NULL;

      ret = xmalloc (1);
      ret[0] = '\0';
      return ret;
    }

 

  sflags = SEVAL_NONINT|SEVAL_NOHIST|SEVAL_NOFREE;
  if (flags & SX_NOLONGJMP)
    sflags |= SEVAL_NOLONGJMP;

  save_parser_state (&ps);
  save_input_line_state (&ls);

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  pushed_string_list = (STRING_SAVER *)NULL;
#endif
   
  parser_state |= PST_CMDSUBST|PST_EOFTOKEN;	   
  shell_eof_token = ')';
  if (flags & SX_COMPLETE)
    parser_state |= PST_NOERROR;

   
  expand_aliases = 0;
#if defined (EXTENDED_GLOB)
  local_extglob = extended_glob;
#endif

  token_to_read = DOLPAREN;			 

  nc = parse_string (string, "command substitution", sflags, (COMMAND **)NULL, &ep);

   
  if (current_token == shell_eof_token)
    yyclearin;		 

  reset_parser ();	 
   
  restore_input_line_state (&ls);
  restore_parser_state (&ps);

#if defined (EXTENDED_GLOB)
  extended_glob = local_extglob;
#endif
  token_to_read = 0;

   
  if (nc < 0)
    {
      clear_shell_input_line ();	 
      if (bash_input.type != st_string)	 
	parser_state &= ~(PST_CMDSUBST|PST_EOFTOKEN);
      if ((flags & SX_NOLONGJMP) == 0)
	jump_to_top_level (-nc);	 
    }

   

   
  if (ep[-1] != ')')
    {
#if 0
      if (ep[-1] != '\n')
	itrace("xparse_dolparen:%d: ep[-1] != RPAREN (%d), ep = `%s'", line_number, ep[-1], ep);
#endif

      while (ep > ostring && ep[-1] == '\n') ep--;
    }

  nc = ep - ostring;
  *indp = ep - base - 1;

   
#if 0
  if (base[*indp] != ')')
    itrace("xparse_dolparen:%d: base[%d] != RPAREN (%d), base = `%s'", line_number, *indp, base[*indp], base);
  if (*indp < orig_ind)
    itrace("xparse_dolparen:%d: *indp (%d) < orig_ind (%d), orig_string = `%s'", line_number, *indp, orig_ind, ostring);
#endif

  if (base[*indp] != ')' && (flags & SX_NOLONGJMP) == 0)
    {
       
      if ((flags & SX_NOERROR) == 0)
	parser_error (start_lineno, _("unexpected EOF while looking for matching `%c'"), ')');
      jump_to_top_level (DISCARD);
    }

  if (flags & SX_NOALLOC) 
    return (char *)NULL;

  if (nc == 0)
    {
      ret = xmalloc (1);
      ret[0] = '\0';
    }
  else
    ret = substring (ostring, 0, nc - 1);

  return ret;
}

 
COMMAND *
parse_string_to_command (string, flags)
     char *string;
     int flags;
{
  sh_parser_state_t ps;
  sh_input_line_state_t ls;
  int nc, sflags;
  size_t slen;
  char *ret, *ep;
  COMMAND *cmd;

  if (*string == 0)
    return (COMMAND *)NULL;

  ep = string;
  slen = STRLEN (string);

 

  sflags = SEVAL_NONINT|SEVAL_NOHIST|SEVAL_NOFREE;
  if (flags & SX_NOLONGJMP)
    sflags |= SEVAL_NOLONGJMP;

  save_parser_state (&ps);
  save_input_line_state (&ls);

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  pushed_string_list = (STRING_SAVER *)NULL;
#endif
  if (flags & SX_COMPLETE)
    parser_state |= PST_NOERROR;

  parser_state |= PST_STRING;
  expand_aliases = 0;

  cmd = 0;
  nc = parse_string (string, "command substitution", sflags, &cmd, &ep);

  reset_parser ();
   
  restore_input_line_state (&ls);
  restore_parser_state (&ps);

   
  if (nc < 0)
    {
      clear_shell_input_line ();	 
      if ((flags & SX_NOLONGJMP) == 0)
        jump_to_top_level (-nc);	 
    }

   
  if (nc < slen)
    {
      dispose_command (cmd);
      return (COMMAND *)NULL;
    }

  return cmd;
}

#if defined (DPAREN_ARITHMETIC) || defined (ARITH_FOR_COMMAND)
 
static int
parse_dparen (c)
     int c;
{
  int cmdtyp, sline;
  char *wval;
  WORD_DESC *wd;

#if defined (ARITH_FOR_COMMAND)
  if (last_read_token == FOR)
    {
      if (word_top < MAX_CASE_NEST)
	word_top++;
      arith_for_lineno = word_lineno[word_top] = line_number;
      cmdtyp = parse_arith_cmd (&wval, 0);
      if (cmdtyp == 1)
	{
	  wd = alloc_word_desc ();
	  wd->word = wval;
	  yylval.word_list = make_word_list (wd, (WORD_LIST *)NULL);
	  return (ARITH_FOR_EXPRS);
	}
      else
	return -1;		 
    }
#endif

#if defined (DPAREN_ARITHMETIC)
  if (reserved_word_acceptable (last_read_token))
    {
      sline = line_number;

      cmdtyp = parse_arith_cmd (&wval, 0);
      if (cmdtyp == 1)	 
	{
	  wd = alloc_word_desc ();
	  wd->word = wval;
	  wd->flags = W_QUOTED|W_NOSPLIT|W_NOGLOB|W_NOTILDE|W_NOPROCSUB;
	  yylval.word_list = make_word_list (wd, (WORD_LIST *)NULL);
	  return (ARITH_CMD);
	}
      else if (cmdtyp == 0)	 
	{
	  push_string (wval, 0, (alias_t *)NULL);
	  pushed_string_list->flags = PSH_DPAREN;
	  if ((parser_state & PST_CASEPAT) == 0)
	    parser_state |= PST_SUBSHELL;
	  return (c);
	}
      else			 
	return -1;
    }
#endif

  return -2;			 
}

 
static int
parse_arith_cmd (ep, adddq)
     char **ep;
     int adddq;
{
  int exp_lineno, rval, c;
  char *ttok, *tokstr;
  int ttoklen;

  exp_lineno = line_number;
  ttok = parse_matched_pair (0, '(', ')', &ttoklen, P_ARITH);
  rval = 1;
  if (ttok == &matched_pair_error)
    return -1;
   
  c = shell_getc (0);
  if MBTEST(c != ')')
    rval = 0;

  tokstr = (char *)xmalloc (ttoklen + 4);

   
  if (rval == 1 && adddq)	 
    {
      tokstr[0] = '"';
      strncpy (tokstr + 1, ttok, ttoklen - 1);
      tokstr[ttoklen] = '"';
      tokstr[ttoklen+1] = '\0';
    }
  else if (rval == 1)		 
    {
      strncpy (tokstr, ttok, ttoklen - 1);
      tokstr[ttoklen-1] = '\0';
    }
  else				 
    {
      tokstr[0] = '(';
      strncpy (tokstr + 1, ttok, ttoklen - 1);
      tokstr[ttoklen] = ')';
      tokstr[ttoklen+1] = c;
      tokstr[ttoklen+2] = '\0';
    }

  *ep = tokstr;
  FREE (ttok);
  return rval;
}
#endif  

#if defined (COND_COMMAND)
static void
cond_error ()
{
  char *etext;

  if (EOF_Reached && cond_token != COND_ERROR)		 
    parser_error (cond_lineno, _("unexpected EOF while looking for `]]'"));
  else if (cond_token != COND_ERROR)
    {
      if (etext = error_token_from_token (cond_token))
	{
	  parser_error (cond_lineno, _("syntax error in conditional expression: unexpected token `%s'"), etext);
	  free (etext);
	}
      else
	parser_error (cond_lineno, _("syntax error in conditional expression"));
    }
}

static COND_COM *
cond_expr ()
{
  return (cond_or ());  
}

static COND_COM *
cond_or ()
{
  COND_COM *l, *r;

  l = cond_and ();
  if (cond_token == OR_OR)
    {
      r = cond_or ();
      l = make_cond_node (COND_OR, (WORD_DESC *)NULL, l, r);
    }
  return l;
}

static COND_COM *
cond_and ()
{
  COND_COM *l, *r;

  l = cond_term ();
  if (cond_token == AND_AND)
    {
      r = cond_and ();
      l = make_cond_node (COND_AND, (WORD_DESC *)NULL, l, r);
    }
  return l;
}

static int
cond_skip_newlines ()
{
  while ((cond_token = read_token (READ)) == '\n')
    {
      if (SHOULD_PROMPT ())
	prompt_again (0);
    }
  return (cond_token);
}

#define COND_RETURN_ERROR() \
  do { cond_token = COND_ERROR; return ((COND_COM *)NULL); } while (0)

static COND_COM *
cond_term ()
{
  WORD_DESC *op;
  COND_COM *term, *tleft, *tright;
  int tok, lineno, local_extglob;
  char *etext;

   
  tok = cond_skip_newlines ();
  lineno = line_number;
  if (tok == COND_END)
    {
      COND_RETURN_ERROR ();
    }
  else if (tok == '(')
    {
      term = cond_expr ();
      if (cond_token != ')')
	{
	  if (term)
	    dispose_cond_node (term);		 
	  if (etext = error_token_from_token (cond_token))
	    {
	      parser_error (lineno, _("unexpected token `%s', expected `)'"), etext);
	      free (etext);
	    }
	  else
	    parser_error (lineno, _("expected `)'"));
	  COND_RETURN_ERROR ();
	}
      term = make_cond_node (COND_EXPR, (WORD_DESC *)NULL, term, (COND_COM *)NULL);
      (void)cond_skip_newlines ();
    }
  else if (tok == BANG || (tok == WORD && (yylval.word->word[0] == '!' && yylval.word->word[1] == '\0')))
    {
      if (tok == WORD)
	dispose_word (yylval.word);	 
      term = cond_term ();
      if (term)
	term->flags ^= CMD_INVERT_RETURN;
    }
  else if (tok == WORD && yylval.word->word[0] == '-' && yylval.word->word[1] && yylval.word->word[2] == 0 && test_unop (yylval.word->word))
    {
      op = yylval.word;
      tok = read_token (READ);
      if (tok == WORD)
	{
	  tleft = make_cond_node (COND_TERM, yylval.word, (COND_COM *)NULL, (COND_COM *)NULL);
	  term = make_cond_node (COND_UNARY, op, tleft, (COND_COM *)NULL);
	}
      else
	{
	  dispose_word (op);
	  if (etext = error_token_from_token (tok))
	    {
	      parser_error (line_number, _("unexpected argument `%s' to conditional unary operator"), etext);
	      free (etext);
	    }
	  else
	    parser_error (line_number, _("unexpected argument to conditional unary operator"));
	  COND_RETURN_ERROR ();
	}

      (void)cond_skip_newlines ();
    }
  else if (tok == WORD)		 
    {
       
      tleft = make_cond_node (COND_TERM, yylval.word, (COND_COM *)NULL, (COND_COM *)NULL);

       
       
      tok = read_token (READ);
      if (tok == WORD && test_binop (yylval.word->word))
	{
	  op = yylval.word;
	  if (op->word[0] == '=' && (op->word[1] == '\0' || (op->word[1] == '=' && op->word[2] == '\0')))
	    parser_state |= PST_EXTPAT;
	  else if (op->word[0] == '!' && op->word[1] == '=' && op->word[2] == '\0')
	    parser_state |= PST_EXTPAT;
	}
#if defined (COND_REGEXP)
      else if (tok == WORD && STREQ (yylval.word->word, "=~"))
	{
	  op = yylval.word;
	  parser_state |= PST_REGEXP;
	}
#endif
      else if (tok == '<' || tok == '>')
	op = make_word_from_token (tok);   
       
      else if (tok == COND_END || tok == AND_AND || tok == OR_OR || tok == ')')
	{
	   
	  op = make_word ("-n");
	  term = make_cond_node (COND_UNARY, op, tleft, (COND_COM *)NULL);
	  cond_token = tok;
	  return (term);
	}
      else
	{
	  if (etext = error_token_from_token (tok))
	    {
	      parser_error (line_number, _("unexpected token `%s', conditional binary operator expected"), etext);
	      free (etext);
	    }
	  else
	    parser_error (line_number, _("conditional binary operator expected"));
	  dispose_cond_node (tleft);
	  COND_RETURN_ERROR ();
	}

       
#if defined (EXTENDED_GLOB)
      local_extglob = extended_glob;
      if (parser_state & PST_EXTPAT)
	extended_glob = 1;
#endif
      tok = read_token (READ);
#if defined (EXTENDED_GLOB)
      if (parser_state & PST_EXTPAT)
	extended_glob = local_extglob;
#endif
      parser_state &= ~(PST_REGEXP|PST_EXTPAT);

      if (tok == WORD)
	{
	  tright = make_cond_node (COND_TERM, yylval.word, (COND_COM *)NULL, (COND_COM *)NULL);
	  term = make_cond_node (COND_BINARY, op, tleft, tright);
	}
      else
	{
	  if (etext = error_token_from_token (tok))
	    {
	      parser_error (line_number, _("unexpected argument `%s' to conditional binary operator"), etext);
	      free (etext);
	    }
	  else
	    parser_error (line_number, _("unexpected argument to conditional binary operator"));
	  dispose_cond_node (tleft);
	  dispose_word (op);
	  COND_RETURN_ERROR ();
	}

      (void)cond_skip_newlines ();
    }
  else
    {
      if (tok < 256)
	parser_error (line_number, _("unexpected token `%c' in conditional command"), tok);
      else if (etext = error_token_from_token (tok))
	{
	  parser_error (line_number, _("unexpected token `%s' in conditional command"), etext);
	  free (etext);
	}
      else
	parser_error (line_number, _("unexpected token %d in conditional command"), tok);
      COND_RETURN_ERROR ();
    }
  return (term);
}      

 
static COMMAND *
parse_cond_command ()
{
  COND_COM *cexp;

  cexp = cond_expr ();
  return (make_cond_command (cexp));
}
#endif

#if defined (ARRAY_VARS)
 
static int
token_is_assignment (t, i)
     char *t;
     int i;
{
  int r;
  char *atoken;

  atoken = xmalloc (i + 3);
  memcpy (atoken, t, i);
  atoken[i] = '=';
  atoken[i+1] = '\0';

  r = assignment (atoken, (parser_state & PST_COMPASSIGN) != 0);

  free (atoken);

   
  return (r > 0 && r == i);
}

 
static int
token_is_ident (t, i)
     char *t;
     int i;
{
  unsigned char c;
  int r;

  c = t[i];
  t[i] = '\0';
  r = legal_identifier (t);
  t[i] = c;
  return r;
}
#endif

static int
read_token_word (character)
     int character;
{
   
  WORD_DESC *the_word;

   
  int token_index;

   
  int all_digit_token;

   
  int dollar_present;

   
  int compound_assignment;

   
  int quoted;

   
  int pass_next_character;

   
  int cd;
  int result, peek_char;
  char *ttok, *ttrans;
  int ttoklen, ttranslen;
  intmax_t lvalue;

  if (token_buffer_size < TOKEN_DEFAULT_INITIAL_SIZE)
    token = (char *)xrealloc (token, token_buffer_size = TOKEN_DEFAULT_INITIAL_SIZE);

  token_index = 0;
  all_digit_token = DIGIT (character);
  dollar_present = quoted = pass_next_character = compound_assignment = 0;

  for (;;)
    {
      if (character == EOF)
	goto got_token;

      if (pass_next_character)
	{
	  pass_next_character = 0;
	  goto got_escaped_character;
	}

      cd = current_delimiter (dstack);

       
      if MBTEST(character == '\\')
	{
	  if (parser_state & PST_NOEXPAND)
	    {
	      pass_next_character++;
	      quoted = 1;
	      goto got_character;
	    }
	      
	  peek_char = shell_getc (0);

	   
	  if MBTEST(peek_char == '\n')
	    {
	      character = '\n';
	      goto next_character;
	    }
	  else
	    {
	      shell_ungetc (peek_char);

	       
	      if MBTEST(cd == 0 || cd == '`' ||
		  (cd == '"' && peek_char >= 0 && (sh_syntaxtab[peek_char] & CBSDQUOTE)))
		pass_next_character++;

	      quoted = 1;
	      goto got_character;
	    }
	}

       
      if MBTEST(shellquote (character))
	{
	  push_delimiter (dstack, character);
	  ttok = parse_matched_pair (character, character, character, &ttoklen, (character == '`') ? P_COMMAND : 0);
	  pop_delimiter (dstack);
	  if (ttok == &matched_pair_error)
	    return -1;		 
	  RESIZE_MALLOCED_BUFFER (token, token_index, ttoklen + 2,
				  token_buffer_size, TOKEN_DEFAULT_GROW_SIZE);
	  token[token_index++] = character;
	  strcpy (token + token_index, ttok);
	  token_index += ttoklen;
	  all_digit_token = 0;
	  if (character != '`')
	    quoted = 1;
	  dollar_present |= (character == '"' && strchr (ttok, '$') != 0);
	  FREE (ttok);
	  goto next_character;
	}

#ifdef COND_REGEXP
         
      if MBTEST((parser_state & PST_REGEXP) && (character == '(' || character == '|'))		 
	{
	  if (character == '|')
	    goto got_character;

	  push_delimiter (dstack, character);
	  ttok = parse_matched_pair (cd, '(', ')', &ttoklen, 0);
	  pop_delimiter (dstack);
	  if (ttok == &matched_pair_error)
	    return -1;		 
	  RESIZE_MALLOCED_BUFFER (token, token_index, ttoklen + 2,
				  token_buffer_size, TOKEN_DEFAULT_GROW_SIZE);
	  token[token_index++] = character;
	  strcpy (token + token_index, ttok);
	  token_index += ttoklen;
	  FREE (ttok);
	  dollar_present = all_digit_token = 0;
	  goto next_character;
	}
#endif  

#ifdef EXTENDED_GLOB
       
      if MBTEST(extended_glob && PATTERN_CHAR (character))
	{
	  peek_char = shell_getc (1);
	  if MBTEST(peek_char == '(')		 
	    {
	      push_delimiter (dstack, peek_char);
	      ttok = parse_matched_pair (cd, '(', ')', &ttoklen, 0);
	      pop_delimiter (dstack);
	      if (ttok == &matched_pair_error)
		return -1;		 
	      RESIZE_MALLOCED_BUFFER (token, token_index, ttoklen + 3,
				      token_buffer_size,
				      TOKEN_DEFAULT_GROW_SIZE);
	      token[token_index++] = character;
	      token[token_index++] = peek_char;
	      strcpy (token + token_index, ttok);
	      token_index += ttoklen;
	      FREE (ttok);
	      dollar_present = all_digit_token = 0;
	      goto next_character;
	    }
	  else
	    shell_ungetc (peek_char);
	}
#endif  

       
      if MBTEST(shellexp (character))
	{
	  peek_char = shell_getc (1);
	   
	  if MBTEST(peek_char == '(' ||
		((peek_char == '{' || peek_char == '[') && character == '$'))	 
	    {
	      if (peek_char == '{')		 
		ttok = parse_matched_pair (cd, '{', '}', &ttoklen, P_FIRSTCLOSE|P_DOLBRACE);
	      else if (peek_char == '(')		 
		{
		   
		  push_delimiter (dstack, peek_char);
		  ttok = parse_comsub (cd, '(', ')', &ttoklen, P_COMMAND);
		  pop_delimiter (dstack);
		}
	      else
		ttok = parse_matched_pair (cd, '[', ']', &ttoklen, P_ARITH);
	      if (ttok == &matched_pair_error)
		return -1;		 
	      RESIZE_MALLOCED_BUFFER (token, token_index, ttoklen + 3,
				      token_buffer_size,
				      TOKEN_DEFAULT_GROW_SIZE);
	      token[token_index++] = character;
	      token[token_index++] = peek_char;
	      strcpy (token + token_index, ttok);
	      token_index += ttoklen;
	      FREE (ttok);
	      dollar_present = 1;
	      all_digit_token = 0;
	      goto next_character;
	    }
	   
#if defined (TRANSLATABLE_STRINGS)
	  else if MBTEST(character == '$' && (peek_char == '\'' || peek_char == '"'))
#else
	  else if MBTEST(character == '$' && peek_char == '\'')
#endif
	    {
	      int first_line;

	      first_line = line_number;
	      push_delimiter (dstack, peek_char);
	      ttok = parse_matched_pair (peek_char, peek_char, peek_char,
					 &ttoklen,
					 (peek_char == '\'') ? P_ALLOWESC : 0);
	      pop_delimiter (dstack);
	      if (ttok == &matched_pair_error)
		return -1;
	      if (peek_char == '\'')
		{
		   
		  ttrans = ansiexpand (ttok, 0, ttoklen - 1, &ttranslen);
		  free (ttok);

		   
		  ttok = sh_single_quote (ttrans);
		  free (ttrans);
		  ttranslen = strlen (ttok);
		  ttrans = ttok;
		}
#if defined (TRANSLATABLE_STRINGS)
	      else
		{
		   
		   
		  ttrans = locale_expand (ttok, 0, ttoklen - 1, first_line, &ttranslen);
		  free (ttok);

		   
		  if (singlequote_translations &&
		        ((ttoklen - 1) != ttranslen || STREQN (ttok, ttrans, ttranslen) == 0))
		    ttok = sh_single_quote (ttrans);
		  else
		    ttok = sh_mkdoublequoted (ttrans, ttranslen, 0);

		  free (ttrans);
		  ttrans = ttok;
		  ttranslen = strlen (ttrans);
		}
#endif  

	      RESIZE_MALLOCED_BUFFER (token, token_index, ttranslen + 1,
				      token_buffer_size,
				      TOKEN_DEFAULT_GROW_SIZE);
	      strcpy (token + token_index, ttrans);
	      token_index += ttranslen;
	      FREE (ttrans);
	      quoted = 1;
	      all_digit_token = 0;
	      goto next_character;
	    }
	   
	  else if MBTEST(character == '$' && peek_char == '$')
	    {
	      RESIZE_MALLOCED_BUFFER (token, token_index, 3,
				      token_buffer_size,
				      TOKEN_DEFAULT_GROW_SIZE);
	      token[token_index++] = '$';
	      token[token_index++] = peek_char;
	      dollar_present = 1;
	      all_digit_token = 0;
	      goto next_character;
	    }
	  else
	    shell_ungetc (peek_char);
	}

#if defined (ARRAY_VARS)
       
      else if MBTEST(character == '[' &&		 
		     ((token_index > 0 && assignment_acceptable (last_read_token) && token_is_ident (token, token_index)) ||
		      (token_index == 0 && (parser_state&PST_COMPASSIGN))))
        {
	  ttok = parse_matched_pair (cd, '[', ']', &ttoklen, P_ARRAYSUB);
	  if (ttok == &matched_pair_error)
	    return -1;		 
	  RESIZE_MALLOCED_BUFFER (token, token_index, ttoklen + 2,
				  token_buffer_size,
				  TOKEN_DEFAULT_GROW_SIZE);
	  token[token_index++] = character;
	  strcpy (token + token_index, ttok);
	  token_index += ttoklen;
	  FREE (ttok);
	  all_digit_token = 0;
	  goto next_character;
        }
       
      else if MBTEST(character == '=' && token_index > 0 && (assignment_acceptable (last_read_token) || (parser_state & PST_ASSIGNOK)) && token_is_assignment (token, token_index))
	{
	  peek_char = shell_getc (1);
	  if MBTEST(peek_char == '(')		 
	    {
	      ttok = parse_compound_assignment (&ttoklen);

	      RESIZE_MALLOCED_BUFFER (token, token_index, ttoklen + 4,
				      token_buffer_size,
				      TOKEN_DEFAULT_GROW_SIZE);

	      token[token_index++] = '=';
	      token[token_index++] = '(';
	      if (ttok)
		{
		  strcpy (token + token_index, ttok);
		  token_index += ttoklen;
		}
	      token[token_index++] = ')';
	      FREE (ttok);
	      all_digit_token = 0;
	      compound_assignment = 1;
#if 1
	      goto next_character;
#else
	      goto got_token;		 
#endif
	    }
	  else
	    shell_ungetc (peek_char);
	}
#endif

       
      if MBTEST(shellbreak (character))
	{
	  shell_ungetc (character);
	  goto got_token;
	}

got_character:
      if MBTEST(character == CTLESC || character == CTLNUL)
	{
	  RESIZE_MALLOCED_BUFFER (token, token_index, 2, token_buffer_size,
				  TOKEN_DEFAULT_GROW_SIZE);
	  token[token_index++] = CTLESC;
	}
      else
got_escaped_character:
	RESIZE_MALLOCED_BUFFER (token, token_index, 1, token_buffer_size,
				TOKEN_DEFAULT_GROW_SIZE);

      token[token_index++] = character;

      all_digit_token &= DIGIT (character);
      dollar_present |= character == '$';

    next_character:
      if (character == '\n' && SHOULD_PROMPT ())
	prompt_again (0);

       
      cd = current_delimiter (dstack);
      character = shell_getc (cd != '\'' && pass_next_character == 0);
    }	 

got_token:

   
  token[token_index] = '\0';

   
  if MBTEST(all_digit_token && (character == '<' || character == '>' ||
		    last_read_token == LESS_AND ||
		    last_read_token == GREATER_AND))
      {
	if (legal_number (token, &lvalue) && (int)lvalue == lvalue)
	  {
	    yylval.number = lvalue;
	    return (NUMBER);
	  }
      }

   
  result = (last_shell_getc_is_singlebyte) ? special_case_tokens (token) : -1;
  if (result >= 0)
    return result;

#if defined (ALIAS)
   
  if MBTEST(posixly_correct)
    CHECK_FOR_RESERVED_WORD (token);

   
  if (expand_aliases && quoted == 0)
    {
      result = alias_expand_token (token);
      if (result == RE_READ_TOKEN)
	return (RE_READ_TOKEN);
      else if (result == NO_EXPANSION)
	parser_state &= ~PST_ALEXPNEXT;
    }

   
  if MBTEST(posixly_correct == 0)
#endif
    CHECK_FOR_RESERVED_WORD (token);

  the_word = alloc_word_desc ();
  the_word->word = (char *)xmalloc (1 + token_index);
  the_word->flags = 0;
  strcpy (the_word->word, token);
  if (dollar_present)
    the_word->flags |= W_HASDOLLAR;
  if (quoted)
    the_word->flags |= W_QUOTED;		 
  if (compound_assignment && token[token_index-1] == ')')
    the_word->flags |= W_COMPASSIGN;
   
  if (assignment (token, (parser_state & PST_COMPASSIGN) != 0))
    {
      the_word->flags |= W_ASSIGNMENT;
       
      if (assignment_acceptable (last_read_token) || (parser_state & PST_COMPASSIGN) != 0)
	{
	  the_word->flags |= W_NOSPLIT;
	  if (parser_state & PST_COMPASSIGN)
	    the_word->flags |= W_NOGLOB;	 
	}
    }

  if (command_token_position (last_read_token))
    {
      struct builtin *b;
      b = builtin_address_internal (token, 0);
      if (b && (b->flags & ASSIGNMENT_BUILTIN))
	parser_state |= PST_ASSIGNOK;
      else if (STREQ (token, "eval") || STREQ (token, "let"))
	parser_state |= PST_ASSIGNOK;
    }

  yylval.word = the_word;

   
  if MBTEST(token[0] == '{' && token[token_index-1] == '}' &&
      (character == '<' || character == '>'))
    {
       
      token[token_index-1] = '\0';
#if defined (ARRAY_VARS)
      if (legal_identifier (token+1) || valid_array_reference (token+1, 0))
#else
      if (legal_identifier (token+1))
#endif
	{
	  strcpy (the_word->word, token+1);
 
	  yylval.word = the_word;	 
	  return (REDIR_WORD);
	}
      else
         
        yylval.word = the_word;
    }

  result = ((the_word->flags & (W_ASSIGNMENT|W_NOSPLIT)) == (W_ASSIGNMENT|W_NOSPLIT))
		? ASSIGNMENT_WORD : WORD;

  switch (last_read_token)
    {
    case FUNCTION:
      parser_state |= PST_ALLOWOPNBRC;
      function_dstart = line_number;
      break;
    case CASE:
    case SELECT:
    case FOR:
      if (word_top < MAX_CASE_NEST)
	word_top++;
      word_lineno[word_top] = line_number;
      expecting_in_token++;
      break;
    }

  return (result);
}

 
static int
reserved_word_acceptable (toksym)
     int toksym;
{
  switch (toksym)
    {
    case '\n':
    case ';':
    case '(':
    case ')':
    case '|':
    case '&':
    case '{':
    case '}':		 
    case AND_AND:
    case ARITH_CMD:
    case BANG:
    case BAR_AND:
    case COND_END:
    case DO:
    case DONE:
    case ELIF:
    case ELSE:
    case ESAC:
    case FI:
    case IF:
    case OR_OR:
    case SEMI_SEMI:
    case SEMI_AND:
    case SEMI_SEMI_AND:
    case THEN:
    case TIME:
    case TIMEOPT:
    case TIMEIGN:
    case COPROC:
    case UNTIL:
    case WHILE:
    case 0:
    case DOLPAREN:
      return 1;
    default:
#if defined (COPROCESS_SUPPORT)
      if (last_read_token == WORD && token_before_that == COPROC)
	return 1;
#endif
      if (last_read_token == WORD && token_before_that == FUNCTION)
	return 1;
      return 0;
    }
}
    
 
int
find_reserved_word (tokstr)
     char *tokstr;
{
  int i;
  for (i = 0; word_token_alist[i].word; i++)
    if (STREQ (tokstr, word_token_alist[i].word))
      return i;
  return -1;
}

 
int
parser_in_command_position ()
{
  return (command_token_position (last_read_token));
}

#if 0
#if defined (READLINE)
 
static void
reset_readline_prompt ()
{
  char *temp_prompt;

  if (prompt_string_pointer)
    {
      temp_prompt = (*prompt_string_pointer)
			? decode_prompt_string (*prompt_string_pointer)
			: (char *)NULL;

      if (temp_prompt == 0)
	{
	  temp_prompt = (char *)xmalloc (1);
	  temp_prompt[0] = '\0';
	}

      FREE (current_readline_prompt);
      current_readline_prompt = temp_prompt;
    }
}
#endif  
#endif  

#if defined (HISTORY)
 
static const int no_semi_successors[] = {
  '\n', '{', '(', ')', ';', '&', '|',
  CASE, DO, ELSE, IF, SEMI_SEMI, SEMI_AND, SEMI_SEMI_AND, THEN, UNTIL,
  WHILE, AND_AND, OR_OR, IN,
  0
};

 
char *
history_delimiting_chars (line)
     const char *line;
{
  static int last_was_heredoc = 0;	 
  register int i;

  if ((parser_state & PST_HEREDOC) == 0)
    last_was_heredoc = 0;

  if (dstack.delimiter_depth != 0)
    return ("\n");

   
  if (parser_state & PST_HEREDOC)
    {
      if (last_was_heredoc)
	{
	  last_was_heredoc = 0;
	  return "\n";
	}
      return (here_doc_first_line ? "\n" : "");
    }

  if (parser_state & PST_COMPASSIGN)
    return (" ");

   
   
   
   
  if (token_before_that == ')')
    {
      if (two_tokens_ago == '(')	 	 
	return " ";
       
      else if (parser_state & PST_CASESTMT)	 
	return " ";
      else
	return "; ";				 
    }
  else if (token_before_that == WORD && two_tokens_ago == FUNCTION)
    return " ";		 

   
  else if ((parser_state & PST_HEREDOC) == 0 && current_command_line_count > 1 && last_read_token == '\n' && strstr (line, "<<"))
    {
      last_was_heredoc = 1;
      return "\n";
    }
  else if ((parser_state & PST_HEREDOC) == 0 && current_command_line_count > 1 && need_here_doc > 0)
    return "\n";
  else if (token_before_that == WORD && two_tokens_ago == FOR)
    {
       
      for (i = shell_input_line_index; whitespace (shell_input_line[i]); i++)
	;
      if (shell_input_line[i] && shell_input_line[i] == 'i' && shell_input_line[i+1] == 'n')
	return " ";
      return ";";
    }
  else if (two_tokens_ago == CASE && token_before_that == WORD && (parser_state & PST_CASESTMT))
    return " ";

  for (i = 0; no_semi_successors[i]; i++)
    {
      if (token_before_that == no_semi_successors[i])
	return (" ");
    }

   
  if (line_isblank (line))
    return (current_command_line_count > 1 && last_read_token == '\n' && token_before_that != '\n') ? "; " : "";

  return ("; ");
}
#endif  

 
static void
prompt_again (force)
     int force;
{
  char *temp_prompt;

  if (interactive == 0 || expanding_alias ())	 
    return;

  ps1_prompt = get_string_value ("PS1");
  ps2_prompt = get_string_value ("PS2");

  ps0_prompt = get_string_value ("PS0");

  if (!prompt_string_pointer)
    prompt_string_pointer = &ps1_prompt;

  temp_prompt = *prompt_string_pointer
			? decode_prompt_string (*prompt_string_pointer)
			: (char *)NULL;

  if (temp_prompt == 0)
    {
      temp_prompt = (char *)xmalloc (1);
      temp_prompt[0] = '\0';
    }

  current_prompt_string = *prompt_string_pointer;
  prompt_string_pointer = &ps2_prompt;

#if defined (READLINE)
  if (!no_line_editing)
    {
      FREE (current_readline_prompt);
      current_readline_prompt = temp_prompt;
    }
  else
#endif	 
    {
      FREE (current_decoded_prompt);
      current_decoded_prompt = temp_prompt;
    }
}

int
get_current_prompt_level ()
{
  return ((current_prompt_string && current_prompt_string == ps2_prompt) ? 2 : 1);
}

void
set_current_prompt_level (x)
     int x;
{
  prompt_string_pointer = (x == 2) ? &ps2_prompt : &ps1_prompt;
  current_prompt_string = *prompt_string_pointer;
}
      
static void
print_prompt ()
{
  fprintf (stderr, "%s", current_decoded_prompt);
  fflush (stderr);
}

#if defined (HISTORY)
   
static int
prompt_history_number (pmt)
     char *pmt;
{
  int ret;

  ret = history_number ();
  if (ret == 1)
    return ret;

  if (pmt == ps1_prompt)	 
    return ret;
  else if (pmt == ps2_prompt && command_oriented_history == 0)
    return ret;			 
  else if (pmt == ps2_prompt && command_oriented_history && current_command_first_line_saved)
    return ret - 1;
  else
    return ret - 1;		 
}
#endif

 
#define PROMPT_GROWTH 48
char *
decode_prompt_string (string)
     char *string;
{
  WORD_LIST *list;
  char *result, *t, *orig_string;
  struct dstack save_dstack;
  int last_exit_value, last_comsub_pid;
#if defined (PROMPT_STRING_DECODE)
  size_t result_size;
  size_t result_index;
  int c, n, i;
  char *temp, *t_host, octal_string[4];
  struct tm *tm;  
  time_t the_time;
  char timebuf[128];
  char *timefmt;

  result = (char *)xmalloc (result_size = PROMPT_GROWTH);
  result[result_index = 0] = 0;
  temp = (char *)NULL;
  orig_string = string;

  while (c = *string++)
    {
      if (posixly_correct && c == '!')
	{
	  if (*string == '!')
	    {
	      temp = savestring ("!");
	      goto add_string;
	    }
	  else
	    {
#if !defined (HISTORY)
		temp = savestring ("1");
#else  
		temp = itos (prompt_history_number (orig_string));
#endif  
		string--;	 
		goto add_string;
	    }
	}
      if (c == '\\')
	{
	  c = *string;

	  switch (c)
	    {
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	      strncpy (octal_string, string, 3);
	      octal_string[3] = '\0';

	      n = read_octal (octal_string);
	      temp = (char *)xmalloc (3);

	      if (n == CTLESC || n == CTLNUL)
		{
		  temp[0] = CTLESC;
		  temp[1] = n;
		  temp[2] = '\0';
		}
	      else if (n == -1)
		{
		  temp[0] = '\\';
		  temp[1] = '\0';
		}
	      else
		{
		  temp[0] = n;
		  temp[1] = '\0';
		}

	      for (c = 0; n != -1 && c < 3 && ISOCTAL (*string); c++)
		string++;

	      c = 0;		 
	      goto add_string;

	    case 'd':
	    case 't':
	    case 'T':
	    case '@':
	    case 'A':
	       
	      (void) time (&the_time);
#if defined (HAVE_TZSET)
	      sv_tz ("TZ");		 
#endif
	      tm = localtime (&the_time);

	      if (c == 'd')
		n = strftime (timebuf, sizeof (timebuf), "%a %b %d", tm);
	      else if (c == 't')
		n = strftime (timebuf, sizeof (timebuf), "%H:%M:%S", tm);
	      else if (c == 'T')
		n = strftime (timebuf, sizeof (timebuf), "%I:%M:%S", tm);
	      else if (c == '@')
		n = strftime (timebuf, sizeof (timebuf), "%I:%M %p", tm);
	      else if (c == 'A')
		n = strftime (timebuf, sizeof (timebuf), "%H:%M", tm);

	      if (n == 0)
		timebuf[0] = '\0';
	      else
		timebuf[sizeof(timebuf) - 1] = '\0';

	      temp = savestring (timebuf);
	      goto add_string;

	    case 'D':		 
	      if (string[1] != '{')		 
		goto not_escape;

	      (void) time (&the_time);
	      tm = localtime (&the_time);
	      string += 2;			 
	      timefmt = xmalloc (strlen (string) + 3);
	      for (t = timefmt; *string && *string != '}'; )
		*t++ = *string++;
	      *t = '\0';
	      c = *string;	 
	      if (timefmt[0] == '\0')
		{
		  timefmt[0] = '%';
		  timefmt[1] = 'X';	 
		  timefmt[2] = '\0';
		}
	      n = strftime (timebuf, sizeof (timebuf), timefmt, tm);
	      free (timefmt);

	      if (n == 0)
		timebuf[0] = '\0';
	      else
		timebuf[sizeof(timebuf) - 1] = '\0';

	      if (promptvars || posixly_correct)
		 
		temp = sh_backslash_quote_for_double_quotes (timebuf, 0);
	      else
		temp = savestring (timebuf);
	      goto add_string;
	      
	    case 'n':
	      temp = (char *)xmalloc (3);
	      temp[0] = no_line_editing ? '\n' : '\r';
	      temp[1] = no_line_editing ? '\0' : '\n';
	      temp[2] = '\0';
	      goto add_string;

	    case 's':
	      temp = base_pathname (shell_name);
	       
	      if (promptvars || posixly_correct)
		{
		  char *t;
		  t = sh_strvis (temp);
		  temp = sh_backslash_quote_for_double_quotes (t, 0);
		  free (t);
		}
	      else
		temp = sh_strvis (temp);
	      goto add_string;

	    case 'v':
	    case 'V':
	      temp = (char *)xmalloc (16);
	      if (c == 'v')
		strcpy (temp, dist_version);
	      else
		sprintf (temp, "%s.%d", dist_version, patch_level);
	      goto add_string;

	    case 'w':
	    case 'W':
	      {
		 
		char t_string[PATH_MAX];
		int tlen;

		temp = get_string_value ("PWD");

		if (temp == 0)
		  {
		    if (getcwd (t_string, sizeof(t_string)) == 0)
		      {
			t_string[0] = '.';
			tlen = 1;
		      }
		    else
		      tlen = strlen (t_string);
		  }
		else
		  {
		    tlen = sizeof (t_string) - 1;
		    strncpy (t_string, temp, tlen);
		  }
		t_string[tlen] = '\0';

#if defined (MACOSX)
		 
		temp = fnx_fromfs (t_string, strlen (t_string));
		if (temp != t_string)
		  strcpy (t_string, temp);
#endif

#define ROOT_PATH(x)	((x)[0] == '/' && (x)[1] == 0)
#define DOUBLE_SLASH_ROOT(x)	((x)[0] == '/' && (x)[1] == '/' && (x)[2] == 0)
		 
		if (c == 'W' && (((t = get_string_value ("HOME")) == 0) || STREQ (t, t_string) == 0))
		  {
		    if (ROOT_PATH (t_string) == 0 && DOUBLE_SLASH_ROOT (t_string) == 0)
		      {
			t = strrchr (t_string, '/');
			if (t)
			  memmove (t_string, t + 1, strlen (t));	 
		      }
		  }
#undef ROOT_PATH
#undef DOUBLE_SLASH_ROOT
		else
		  {
		     
		    temp = polite_directory_format (t_string);
		    if (temp != t_string)
		      strcpy (t_string, temp);
		  }

		temp = trim_pathname (t_string, PATH_MAX - 1);
		 
		if (promptvars || posixly_correct)
		   
		  {
		    char *t;
		    t = sh_strvis (t_string);
		    temp = sh_backslash_quote_for_double_quotes (t, 0);
		    free (t);
		  }
		else
		  temp = sh_strvis (t_string);

		goto add_string;
	      }

	    case 'u':
	      if (current_user.user_name == 0)
		get_current_user_info ();
	      temp = savestring (current_user.user_name);
	      goto add_string;

	    case 'h':
	    case 'H':
	      t_host = savestring (current_host_name);
	      if (c == 'h' && (t = (char *)strchr (t_host, '.')))
		*t = '\0';
	      if (promptvars || posixly_correct)
		 
		temp = sh_backslash_quote_for_double_quotes (t_host, 0);
	      else
		temp = savestring (t_host);
	      free (t_host);
	      goto add_string;

	    case '#':
	      n = current_command_number;
	       
	      if (orig_string != ps0_prompt && orig_string != ps1_prompt && orig_string != ps2_prompt)
		n--;
	      temp = itos (n);
	      goto add_string;

	    case '!':
#if !defined (HISTORY)
	      temp = savestring ("1");
#else  
	      temp = itos (prompt_history_number (orig_string));
#endif  
	      goto add_string;

	    case '$':
	      t = temp = (char *)xmalloc (3);
	      if ((promptvars || posixly_correct) && (current_user.euid != 0))
		*t++ = '\\';
	      *t++ = current_user.euid == 0 ? '#' : '$';
	      *t = '\0';
	      goto add_string;

	    case 'j':
	      temp = itos (count_all_jobs ());
	      goto add_string;

	    case 'l':
#if defined (HAVE_TTYNAME)
	      temp = (char *)ttyname (fileno (stdin));
	      t = temp ? base_pathname (temp) : "tty";
	      temp = savestring (t);
#else
	      temp = savestring ("tty");
#endif  
	      goto add_string;

#if defined (READLINE)
	    case '[':
	    case ']':
	      if (no_line_editing)
		{
		  string++;
		  break;
		}
	      temp = (char *)xmalloc (3);
	      n = (c == '[') ? RL_PROMPT_START_IGNORE : RL_PROMPT_END_IGNORE;
	      i = 0;
	      if (n == CTLESC || n == CTLNUL)
		temp[i++] = CTLESC;
	      temp[i++] = n;
	      temp[i] = '\0';
	      goto add_string;
#endif  

	    case '\\':
	    case 'a':
	    case 'e':
	    case 'r':
	      temp = (char *)xmalloc (2);
	      if (c == 'a')
		temp[0] = '\07';
	      else if (c == 'e')
		temp[0] = '\033';
	      else if (c == 'r')
		temp[0] = '\r';
	      else			 
	        temp[0] = c;
	      temp[1] = '\0';
	      goto add_string;

	    default:
not_escape:
	      temp = (char *)xmalloc (3);
	      temp[0] = '\\';
	      temp[1] = c;
	      temp[2] = '\0';

	    add_string:
	      if (c)
		string++;
	      result =
		sub_append_string (temp, result, &result_index, &result_size);
	      temp = (char *)NULL;  
	      result[result_index] = '\0';
	      break;
	    }
	}
      else
	{
	  RESIZE_MALLOCED_BUFFER (result, result_index, 3, result_size, PROMPT_GROWTH);
	   
	  if (c == CTLESC || c == CTLNUL)
	    result[result_index++] = CTLESC;
	  result[result_index++] = c;
	  result[result_index] = '\0';
	}
    }
#else  
  result = savestring (string);
#endif  

   
  save_dstack = dstack;
  dstack = temp_dstack;
  dstack.delimiter_depth = 0;

   
  if (promptvars || posixly_correct)
    {
      last_exit_value = last_command_exit_value;
      last_comsub_pid = last_command_subst_pid;
      list = expand_prompt_string (result, Q_DOUBLE_QUOTES, 0);
      free (result);
      result = string_list (list);
      dispose_words (list);
      last_command_exit_value = last_exit_value;
      last_command_subst_pid = last_comsub_pid;
    }
  else
    {
      t = dequote_string (result);
      free (result);
      result = t;
    }

  dstack = save_dstack;

  return (result);
}

 

 
int
yyerror (msg)
     const char *msg;
{
  if ((parser_state & PST_NOERROR) == 0)
    report_syntax_error ((char *)NULL);
  reset_parser ();
  return (0);
}

static char *
error_token_from_token (tok)
     int tok;
{
  char *t;

  if (t = find_token_in_alist (tok, word_token_alist, 0))
    return t;

  if (t = find_token_in_alist (tok, other_token_alist, 0))
    return t;

  t = (char *)NULL;
   
  switch (current_token)
    {
    case WORD:
    case ASSIGNMENT_WORD:
      if (yylval.word)
	t = savestring (yylval.word->word);
      break;
    case NUMBER:
      t = itos (yylval.number);
      break;
    case ARITH_CMD:
      if (yylval.word_list)
        t = string_list (yylval.word_list);
      break;
    case ARITH_FOR_EXPRS:
      if (yylval.word_list)
	t = string_list_internal (yylval.word_list, " ; ");
      break;
    case COND_CMD:
      t = (char *)NULL;		 
      break;
    }

  return t;
}

static char *
error_token_from_text ()
{
  char *msg, *t;
  int token_end, i;

  t = shell_input_line;
  i = shell_input_line_index;
  token_end = 0;
  msg = (char *)NULL;

  if (i && t[i] == '\0')
    i--;

  while (i && (whitespace (t[i]) || t[i] == '\n'))
    i--;

  if (i)
    token_end = i + 1;

  while (i && (member (t[i], " \n\t;|&") == 0))
    i--;

  while (i != token_end && (whitespace (t[i]) || t[i] == '\n'))
    i++;

   
  if (token_end || (i == 0 && token_end == 0))
    {
      if (token_end)
	msg = substring (t, i, token_end);
      else	 
	{
	  msg = (char *)xmalloc (2);
	  msg[0] = t[i];
	  msg[1] = '\0';
	}
    }

  return (msg);
}

static void
print_offending_line ()
{
  char *msg;
  int token_end;

  msg = savestring (shell_input_line);
  token_end = strlen (msg);
  while (token_end && msg[token_end - 1] == '\n')
    msg[--token_end] = '\0';

  parser_error (line_number, "`%s'", msg);
  free (msg);
}

 
static void
report_syntax_error (message)
     char *message;
{
  char *msg, *p;

  if (message)
    {
      parser_error (line_number, "%s", message);
      if (interactive && EOF_Reached)
	EOF_Reached = 0;
      last_command_exit_value = (executing_builtin && parse_and_execute_level) ? EX_BADSYNTAX : EX_BADUSAGE;
      set_pipestatus_from_exit (last_command_exit_value);
      return;
    }

   
  if (current_token != 0 && EOF_Reached == 0 && (msg = error_token_from_token (current_token)))
    {
      if (ansic_shouldquote (msg))
	{
	  p = ansic_quote (msg, 0, NULL);
	  free (msg);
	  msg = p;
	}
      parser_error (line_number, _("syntax error near unexpected token `%s'"), msg);
      free (msg);

      if (interactive == 0)
	print_offending_line ();

      last_command_exit_value = (executing_builtin && parse_and_execute_level) ? EX_BADSYNTAX : EX_BADUSAGE;
      set_pipestatus_from_exit (last_command_exit_value);
      return;
    }

   
  if (shell_input_line && *shell_input_line)
    {
      msg = error_token_from_text ();
      if (msg)
	{
	  parser_error (line_number, _("syntax error near `%s'"), msg);
	  free (msg);
	}

       
      if (interactive == 0)
        print_offending_line ();
    }
  else
    {
      if (EOF_Reached && shell_eof_token && current_token != shell_eof_token)
	parser_error (line_number, _("unexpected EOF while looking for matching `%c'"), shell_eof_token);
      else
	{
	  msg = EOF_Reached ? _("syntax error: unexpected end of file") : _("syntax error");
	  parser_error (line_number, "%s", msg);
	}

       
      if (interactive && EOF_Reached)
	EOF_Reached = 0;
    }

  last_command_exit_value = (executing_builtin && parse_and_execute_level) ? EX_BADSYNTAX : EX_BADUSAGE;
  set_pipestatus_from_exit (last_command_exit_value);
}

 
static void
discard_parser_constructs (error_p)
     int error_p;
{
}

 

 

 
int ignoreeof = 0;

 
int eof_encountered = 0;

 
int eof_encountered_limit = 10;

 
static void
handle_eof_input_unit ()
{
  if (interactive)
    {
       
      if (EOF_Reached)
	EOF_Reached = 0;

       
      if (ignoreeof)
	{
	  if (eof_encountered < eof_encountered_limit)
	    {
	      fprintf (stderr, _("Use \"%s\" to leave the shell.\n"),
		       login_shell ? "logout" : "exit");
	      eof_encountered++;
	       
	      last_read_token = current_token = '\n';
	       
	      prompt_string_pointer = (char **)NULL;
	      prompt_again (0);
	      return;
	    }
	}

       
      reset_parser ();

      last_shell_builtin = this_shell_builtin;
      this_shell_builtin = exit_builtin;
      exit_builtin ((WORD_LIST *)NULL);
    }
  else
    {
       
      EOF_Reached = 1;
    }
}

 

 

static WORD_LIST parse_string_error;

 
WORD_LIST *
parse_string_to_word_list (s, flags, whom)
     char *s;
     int flags;
     const char *whom;
{
  WORD_LIST *wl;
  int tok, orig_current_token, orig_line_number;
  int orig_parser_state;
  sh_parser_state_t ps;
  int ea;

  orig_line_number = line_number;
  save_parser_state (&ps);

#if defined (HISTORY)
  bash_history_disable ();
#endif

  push_stream (1);
  if (ea = expanding_alias ())
    parser_save_alias ();

   
  last_read_token = WORD;

  current_command_line_count = 0;
  echo_input_at_read = expand_aliases = 0;

  with_input_from_string (s, whom);
  wl = (WORD_LIST *)NULL;

  if (flags & 1)
    {
      orig_parser_state = parser_state;		 
       
      parser_state &= ~PST_NOEXPAND;	 
       
      parser_state |= PST_COMPASSIGN|PST_REPARSE|PST_STRING;
    }

  while ((tok = read_token (READ)) != yacc_EOF)
    {
      if (tok == '\n' && *bash_input.location.string == '\0')
	break;
      if (tok == '\n')		 
	continue;
      if (tok != WORD && tok != ASSIGNMENT_WORD)
	{
	  line_number = orig_line_number + line_number - 1;
	  orig_current_token = current_token;
	  current_token = tok;
	  yyerror (NULL);	 
	  current_token = orig_current_token;
	  if (wl)
	    dispose_words (wl);
	  wl = &parse_string_error;
	  break;
	}
      wl = make_word_list (yylval.word, wl);
    }
  
  last_read_token = '\n';
  pop_stream ();

  if (ea)
    parser_restore_alias ();

  restore_parser_state (&ps);

  if (flags & 1)
    parser_state = orig_parser_state;	 

  if (wl == &parse_string_error)
    {
      set_exit_status (EXECUTION_FAILURE);
      if (interactive_shell == 0 && posixly_correct)
	jump_to_top_level (FORCE_EOF);
      else
	jump_to_top_level (DISCARD);
    }

  return (REVERSE_LIST (wl, WORD_LIST *));
}

static char *
parse_compound_assignment (retlenp)
     int *retlenp;
{
  WORD_LIST *wl, *rl;
  int tok, orig_line_number, assignok;
  sh_parser_state_t ps;
  char *ret;

  orig_line_number = line_number;
  save_parser_state (&ps);

   
  last_read_token = WORD;

  token = (char *)NULL;
  token_buffer_size = 0;
  wl = (WORD_LIST *)NULL;	 

  assignok = parser_state&PST_ASSIGNOK;		 

   
  parser_state &= ~(PST_NOEXPAND|PST_CONDCMD|PST_CONDEXPR|PST_REGEXP|PST_EXTPAT);
   
  parser_state |= PST_COMPASSIGN;

  esacs_needed_count = expecting_in_token = 0;

  while ((tok = read_token (READ)) != ')')
    {
      if (tok == '\n')			 
	{
	  if (SHOULD_PROMPT ())
	    prompt_again (0);
	  continue;
	}
      if (tok != WORD && tok != ASSIGNMENT_WORD)
	{
	  current_token = tok;	 
	  if (tok == yacc_EOF)	 
	    parser_error (orig_line_number, _("unexpected EOF while looking for matching `)'"));
	  else
	    yyerror(NULL);	 
	  if (wl)
	    dispose_words (wl);
	  wl = &parse_string_error;
	  break;
	}
      wl = make_word_list (yylval.word, wl);
    }

  restore_parser_state (&ps);

  if (wl == &parse_string_error)
    {
      set_exit_status (EXECUTION_FAILURE);
      last_read_token = '\n';	 
      if (interactive_shell == 0 && posixly_correct)
	jump_to_top_level (FORCE_EOF);
      else
	jump_to_top_level (DISCARD);
    }

  if (wl)
    {
      rl = REVERSE_LIST (wl, WORD_LIST *);
      ret = string_list (rl);
      dispose_words (rl);
    }
  else
    ret = (char *)NULL;

  if (retlenp)
    *retlenp = (ret && *ret) ? strlen (ret) : 0;

  if (assignok)
    parser_state |= PST_ASSIGNOK;

  return ret;
}

 

sh_parser_state_t *
save_parser_state (ps)
     sh_parser_state_t *ps;
{
  if (ps == 0)
    ps = (sh_parser_state_t *)xmalloc (sizeof (sh_parser_state_t));
  if (ps == 0)
    return ((sh_parser_state_t *)NULL);

  ps->parser_state = parser_state;
  ps->token_state = save_token_state ();

  ps->input_line_terminator = shell_input_line_terminator;
  ps->eof_encountered = eof_encountered;
  ps->eol_lookahead = eol_ungetc_lookahead;

  ps->prompt_string_pointer = prompt_string_pointer;

  ps->current_command_line_count = current_command_line_count;

#if defined (HISTORY)
  ps->remember_on_history = remember_on_history;
#  if defined (BANG_HISTORY)
  ps->history_expansion_inhibited = history_expansion_inhibited;
#  endif
#endif

  ps->last_command_exit_value = last_command_exit_value;
#if defined (ARRAY_VARS)
  ps->pipestatus = save_pipestatus_array ();
#endif
    
  ps->last_shell_builtin = last_shell_builtin;
  ps->this_shell_builtin = this_shell_builtin;

  ps->expand_aliases = expand_aliases;
  ps->echo_input_at_read = echo_input_at_read;
  ps->need_here_doc = need_here_doc;
  ps->here_doc_first_line = here_doc_first_line;

  ps->esacs_needed = esacs_needed_count;
  ps->expecting_in = expecting_in_token;

  if (need_here_doc == 0)
    ps->redir_stack[0] = 0;
  else
    memcpy (ps->redir_stack, redir_stack, sizeof (redir_stack[0]) * HEREDOC_MAX);

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  ps->pushed_strings = pushed_string_list;
#endif

  ps->eof_token = shell_eof_token;
  ps->token = token;
  ps->token_buffer_size = token_buffer_size;
   
  token = 0;
  token_buffer_size = 0;

  return (ps);
}

void
restore_parser_state (ps)
     sh_parser_state_t *ps;
{
  int i;

  if (ps == 0)
    return;

  parser_state = ps->parser_state;
  if (ps->token_state)
    {
      restore_token_state (ps->token_state);
      free (ps->token_state);
    }

  shell_input_line_terminator = ps->input_line_terminator;
  eof_encountered = ps->eof_encountered;
  eol_ungetc_lookahead = ps->eol_lookahead;

  prompt_string_pointer = ps->prompt_string_pointer;

  current_command_line_count = ps->current_command_line_count;

#if defined (HISTORY)
  remember_on_history = ps->remember_on_history;
#  if defined (BANG_HISTORY)
  history_expansion_inhibited = ps->history_expansion_inhibited;
#  endif
#endif

  last_command_exit_value = ps->last_command_exit_value;
#if defined (ARRAY_VARS)
  restore_pipestatus_array (ps->pipestatus);
#endif

  last_shell_builtin = ps->last_shell_builtin;
  this_shell_builtin = ps->this_shell_builtin;

  expand_aliases = ps->expand_aliases;
  echo_input_at_read = ps->echo_input_at_read;
  need_here_doc = ps->need_here_doc;
  here_doc_first_line = ps->here_doc_first_line;

  esacs_needed_count = ps->esacs_needed;
  expecting_in_token = ps->expecting_in;

#if 0
  for (i = 0; i < HEREDOC_MAX; i++)
    redir_stack[i] = ps->redir_stack[i];
#else
  if (need_here_doc == 0)
    redir_stack[0] = 0;
  else
    memcpy (redir_stack, ps->redir_stack, sizeof (redir_stack[0]) * HEREDOC_MAX);
#endif

#if defined (ALIAS) || defined (DPAREN_ARITHMETIC)
  pushed_string_list = (STRING_SAVER *)ps->pushed_strings;
#endif

  FREE (token);
  token = ps->token;
  token_buffer_size = ps->token_buffer_size;
  shell_eof_token = ps->eof_token;
}

sh_input_line_state_t *
save_input_line_state (ls)
     sh_input_line_state_t *ls;
{
  if (ls == 0)
    ls = (sh_input_line_state_t *)xmalloc (sizeof (sh_input_line_state_t));
  if (ls == 0)
    return ((sh_input_line_state_t *)NULL);

  ls->input_line = shell_input_line;
  ls->input_line_size = shell_input_line_size;
  ls->input_line_len = shell_input_line_len;
  ls->input_line_index = shell_input_line_index;

#if defined (HANDLE_MULTIBYTE)
  ls->input_property = shell_input_line_property;
  ls->input_propsize = shell_input_line_propsize;
#endif

   
  shell_input_line = 0;
  shell_input_line_size = shell_input_line_len = shell_input_line_index = 0;

#if defined (HANDLE_MULTIBYTE)
  shell_input_line_property = 0;
  shell_input_line_propsize = 0;
#endif

  return ls;
}

void
restore_input_line_state (ls)
     sh_input_line_state_t *ls;
{
  FREE (shell_input_line);
  shell_input_line = ls->input_line;
  shell_input_line_size = ls->input_line_size;
  shell_input_line_len = ls->input_line_len;
  shell_input_line_index = ls->input_line_index;

#if defined (HANDLE_MULTIBYTE)
  FREE (shell_input_line_property);
  shell_input_line_property = ls->input_property;
  shell_input_line_propsize = ls->input_propsize;
#endif

#if 0
  set_line_mbstate ();
#endif
}

 

#if defined (HANDLE_MULTIBYTE)

 
#define MAX_PROPSIZE 32768

static void
set_line_mbstate ()
{
  int c;
  size_t i, previ, len;
  mbstate_t mbs, prevs;
  size_t mbclen;
  int ilen;

  if (shell_input_line == NULL)
    return;
  len = STRLEN (shell_input_line);	 
  if (len == 0)
    return;
  if (shell_input_line_propsize >= MAX_PROPSIZE && len < MAX_PROPSIZE>>1)
    {
      free (shell_input_line_property);
      shell_input_line_property = 0;
      shell_input_line_propsize = 0;
    }
  if (len+1 > shell_input_line_propsize)
    {
      shell_input_line_propsize = len + 1;
      shell_input_line_property = (char *)xrealloc (shell_input_line_property, shell_input_line_propsize);
    }

  if (locale_mb_cur_max == 1)
    {
      memset (shell_input_line_property, 1, len);
      return;
    }

   
  if (locale_utf8locale == 0)
    memset (&prevs, '\0', sizeof (mbstate_t));

  for (i = previ = 0; i < len; i++)
    {
      if (locale_utf8locale == 0)
	mbs = prevs;

      c = shell_input_line[i];
      if (c == EOF)
	{
	  size_t j;
	  for (j = i; j < len; j++)
	    shell_input_line_property[j] = 1;
	  break;
	}

      if (locale_utf8locale)
	{
	  if ((unsigned char)shell_input_line[previ] < 128)	 
	    mbclen = 1;
	  else
	    {
	      ilen = utf8_mblen (shell_input_line + previ, i - previ + 1);
	      mbclen = (ilen == -1) ? (size_t)-1
				    : ((ilen == -2) ? (size_t)-2 : (size_t)ilen);
	    }
	}
      else
	mbclen = mbrlen (shell_input_line + previ, i - previ + 1, &mbs);

      if (mbclen == 1 || mbclen == (size_t)-1)
	{
	  mbclen = 1;
	  previ = i + 1;
	}
      else if (mbclen == (size_t)-2)
        mbclen = 0;
      else if (mbclen > 1)
	{
	  mbclen = 0;
	  previ = i + 1;
	  if (locale_utf8locale == 0)
	    prevs = mbs;
	}
      else
	{
	  size_t j;
	  for (j = i; j < len; j++)
	    shell_input_line_property[j] = 1;
	  break;
	}

      shell_input_line_property[i] = mbclen;
    }
}
#endif  
