

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"




typedef enum {
  VVOID,	
  VNIL,
  VTRUE,
  VFALSE,
  VK,		
  VKNUM,	
  VNONRELOC,	
  VLOCAL,	
  VUPVAL,       
  VINDEXED,	
  VJMP,		
  VRELOCABLE,	
  VCALL,	
  VVARARG	
} expkind;


#define vkisvar(k)	(VLOCAL <= (k) && (k) <= VINDEXED)
#define vkisinreg(k)	((k) == VNONRELOC || (k) == VLOCAL)

typedef struct expdesc {
  expkind k;
  union {
    struct {  
      short idx;  
      lu_byte t;  
      lu_byte vt;  
    } ind;
    int info;  
    lua_Number nval;  
  } u;
  int t;  
  int f;  
} expdesc;



typedef struct Vardesc {
  short idx;  
} Vardesc;



typedef struct Labeldesc {
  TString *name;  
  int pc;  
  int line;  
  lu_byte nactvar;  
} Labeldesc;



typedef struct Labellist {
  Labeldesc *arr;  
  int n;  
  int size;  
} Labellist;



typedef struct Dyndata {
  struct {  
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  Labellist gt;  
  Labellist label;   
} Dyndata;



struct BlockCnt;  



typedef struct FuncState {
  Proto *f;  
  Table *h;  
  struct FuncState *prev;  
  struct LexState *ls;  
  struct BlockCnt *bl;  
  int pc;  
  int lasttarget;   
  int jpc;  
  int nk;  
  int np;  
  int firstlocal;  
  short nlocvars;  
  lu_byte nactvar;  
  lu_byte nups;  
  lu_byte freereg;  
} FuncState;


LUAI_FUNC Closure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                Dyndata *dyd, const char *name, int firstchar);


#endif
