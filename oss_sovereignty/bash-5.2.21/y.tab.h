 

 

 

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

#line 177 "y.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif  
