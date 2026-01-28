

#ifndef lstate_h
#define lstate_h

#include <sys/lua/lua.h>

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"





struct lua_longjmp;  




#define EXTRA_STACK   5


#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)



#define KGC_NORMAL	0
#define KGC_EMERGENCY	1	
#define KGC_GEN		2	


typedef struct stringtable {
  GCObject **hash;
  lu_int32 nuse;  
  int size;
} stringtable;



typedef struct CallInfo {
  StkId func;  
  StkId	top;  
  struct CallInfo *previous, *next;  
  short nresults;  
  lu_byte callstatus;
  ptrdiff_t extra;
  union {
    struct {  
      StkId base;  
      const Instruction *savedpc;
    } l;
    struct {  
      int ctx;  
      lua_CFunction k;  
      ptrdiff_t old_errfunc;
      lu_byte old_allowhook;
      lu_byte status;
    } c;
  } u;
} CallInfo;



#define CIST_LUA	(1<<0)	
#define CIST_HOOKED	(1<<1)	
#define CIST_REENTRY	(1<<2)	
#define CIST_YIELDED	(1<<3)	
#define CIST_YPCALL	(1<<4)	
#define CIST_STAT	(1<<5)	
#define CIST_TAIL	(1<<6)	
#define CIST_HOOKYIELD	(1<<7)	


#define isLua(ci)	((ci)->callstatus & CIST_LUA)



typedef struct global_State {
  lua_Alloc frealloc;  
  void *ud;         
  lu_mem totalbytes;  
  l_mem GCdebt;  
  lu_mem GCmemtrav;  
  lu_mem GCestimate;  
  stringtable strt;  
  TValue l_registry;
  unsigned int seed;  
  lu_byte currentwhite;
  lu_byte gcstate;  
  lu_byte gckind;  
  lu_byte gcrunning;  
  int sweepstrgc;  
  GCObject *allgc;  
  GCObject *finobj;  
  GCObject **sweepgc;  
  GCObject **sweepfin;  
  GCObject *gray;  
  GCObject *grayagain;  
  GCObject *weak;  
  GCObject *ephemeron;  
  GCObject *allweak;  
  GCObject *tobefnz;  
  UpVal uvhead;  
  Mbuffer buff;  
  int gcpause;  
  int gcmajorinc;  
  int gcstepmul;  
  lua_CFunction panic;  
  struct lua_State *mainthread;
  const lua_Number *version;  
  TString *memerrmsg;  
  TString *tmname[TM_N];  
  struct Table *mt[LUA_NUMTAGS];  
} global_State;



struct lua_State {
  CommonHeader;
  lu_byte status;
  StkId top;  
  global_State *l_G;
  CallInfo *ci;  
  const Instruction *oldpc;  
  StkId stack_last;  
  StkId stack;  
  int stacksize;
  unsigned short nny;  
  unsigned short nCcalls;  
  lu_byte hookmask;
  lu_byte allowhook;
  lu_byte runerror; 
  int basehookcount;
  int hookcount;
  lua_Hook hook;
  GCObject *openupval;  
  GCObject *gclist;
  struct lua_longjmp *errorJmp;  
  ptrdiff_t errfunc;  
  CallInfo base_ci;  
};


#define G(L)	(L->l_G)



union GCObject {
  GCheader gch;  
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;
  struct lua_State th;  
};


#define gch(o)		(&(o)->gch)


#define rawgco2ts(o)  \
	check_exp(novariant((o)->gch.tt) == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2lcl(o)	check_exp((o)->gch.tt == LUA_TLCL, &((o)->cl.l))
#define gco2ccl(o)	check_exp((o)->gch.tt == LUA_TCCL, &((o)->cl.c))
#define gco2cl(o)  \
	check_exp(novariant((o)->gch.tt) == LUA_TFUNCTION, &((o)->cl))
#define gco2t(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))


#define obj2gco(v)	(cast(GCObject *, (v)))



#define gettotalbytes(g)	((g)->totalbytes + (g)->GCdebt)

LUAI_FUNC void luaE_setdebt (global_State *g, l_mem debt);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);
LUAI_FUNC CallInfo *luaE_extendCI (lua_State *L);
LUAI_FUNC void luaE_freeCI (lua_State *L);


#endif
