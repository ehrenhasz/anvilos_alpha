#ifndef llex_h
#define llex_h
#include "lobject.h"
#include "lzio.h"
#define FIRST_RESERVED	257
enum RESERVED {
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_DBCOLON, TK_EOS,
  TK_NUMBER, TK_NAME, TK_STRING
};
#define NUM_RESERVED	(cast(int, TK_WHILE-FIRST_RESERVED+1))
typedef union {
  lua_Number r;
  TString *ts;
} SemInfo;   
typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;
#ifdef current
#undef current
#endif
typedef struct LexState {
  int current;   
  int linenumber;   
  int lastline;   
  Token t;   
  Token lookahead;   
  struct FuncState *fs;   
  struct lua_State *L;
  ZIO *z;   
  Mbuffer *buff;   
  struct Dyndata *dyd;   
  TString *source;   
  TString *envn;   
  char decpoint;   
} LexState;
LUAI_FUNC void luaX_init (lua_State *L);
LUAI_FUNC void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
                              TString *source, int firstchar);
LUAI_FUNC TString *luaX_newstring (LexState *ls, const char *str, size_t l);
LUAI_FUNC void luaX_next (LexState *ls);
LUAI_FUNC int luaX_lookahead (LexState *ls);
LUAI_FUNC l_noret luaX_syntaxerror (LexState *ls, const char *s);
LUAI_FUNC const char *luaX_token2str (LexState *ls, int token);
#endif
