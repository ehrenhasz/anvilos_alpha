 

 

 

#ifndef YY_YY_PARSE_DATETIME_TAB_H_INCLUDED
# define YY_YY_PARSE_DATETIME_TAB_H_INCLUDED
 
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
    tAGO = 258,                     
    tDST = 259,                     
    tYEAR_UNIT = 260,               
    tMONTH_UNIT = 261,              
    tHOUR_UNIT = 262,               
    tMINUTE_UNIT = 263,             
    tSEC_UNIT = 264,                
    tDAY_UNIT = 265,                
    tDAY_SHIFT = 266,               
    tDAY = 267,                     
    tDAYZONE = 268,                 
    tLOCAL_ZONE = 269,              
    tMERIDIAN = 270,                
    tMONTH = 271,                   
    tORDINAL = 272,                 
    tZONE = 273,                    
    tSNUMBER = 274,                 
    tUNUMBER = 275,                 
    tSDECIMAL_NUMBER = 276,         
    tUDECIMAL_NUMBER = 277          
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

 
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 565 "parse-datetime.y"

  intmax_t intval;
  textint textintval;
  struct timespec timespec;
  relative_time rel;

#line 93 "parse-datetime-gen.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif




int yyparse (parser_control *pc);


#endif  
