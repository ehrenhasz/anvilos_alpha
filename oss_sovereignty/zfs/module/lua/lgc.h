#ifndef lgc_h
#define lgc_h
#include "lobject.h"
#include "lstate.h"
#if !defined(GCSTEPSIZE)
#define GCSTEPSIZE	(cast_int(100 * sizeof(TString)))
#endif
#define GCSpropagate	0
#define GCSatomic	1
#define GCSsweepstring	2
#define GCSsweepudata	3
#define GCSsweep	4
#define GCSpause	5
#define issweepphase(g)  \
	(GCSsweepstring <= (g)->gcstate && (g)->gcstate <= GCSsweep)
#define isgenerational(g)	((g)->gckind == KGC_GEN)
#define keepinvariant(g)	(isgenerational(g) || g->gcstate <= GCSatomic)
#define keepinvariantout(g)  \
  check_exp(g->gcstate == GCSpropagate || !isgenerational(g),  \
            g->gcstate <= GCSatomic)
#define resetbits(x,m)		((x) &= cast(lu_byte, ~(m)))
#define setbits(x,m)		((x) |= (m))
#define testbits(x,m)		((x) & (m))
#define bitmask(b)		(1<<(b))
#define bit2mask(b1,b2)		(bitmask(b1) | bitmask(b2))
#define l_setbit(x,b)		setbits(x, bitmask(b))
#define resetbit(x,b)		resetbits(x, bitmask(b))
#define testbit(x,b)		testbits(x, bitmask(b))
#define WHITE0BIT	0   
#define WHITE1BIT	1   
#define BLACKBIT	2   
#define FINALIZEDBIT	3   
#define SEPARATED	4   
#define FIXEDBIT	5   
#define OLDBIT		6   
#define WHITEBITS	bit2mask(WHITE0BIT, WHITE1BIT)
#define iswhite(x)      testbits((x)->gch.marked, WHITEBITS)
#define isblack(x)      testbit((x)->gch.marked, BLACKBIT)
#define isgray(x)     \
	(!testbits((x)->gch.marked, WHITEBITS | bitmask(BLACKBIT)))
#define isold(x)	testbit((x)->gch.marked, OLDBIT)
#define resetoldbit(o)	resetbit((o)->gch.marked, OLDBIT)
#define otherwhite(g)	(g->currentwhite ^ WHITEBITS)
#define isdeadm(ow,m)	(!(((m) ^ WHITEBITS) & (ow)))
#define isdead(g,v)	isdeadm(otherwhite(g), (v)->gch.marked)
#define changewhite(x)	((x)->gch.marked ^= WHITEBITS)
#define gray2black(x)	l_setbit((x)->gch.marked, BLACKBIT)
#define valiswhite(x)	(iscollectable(x) && iswhite(gcvalue(x)))
#define luaC_white(g)	cast(lu_byte, (g)->currentwhite & WHITEBITS)
#define luaC_condGC(L,c) \
	{if (G(L)->GCdebt > 0) {c;} condchangemem(L);}
#define luaC_checkGC(L)		luaC_condGC(L, luaC_step(L);)
#define luaC_barrier(L,p,v) { if (valiswhite(v) && isblack(obj2gco(p)))  \
	luaC_barrier_(L,obj2gco(p),gcvalue(v)); }
#define luaC_barrierback(L,p,v) { if (valiswhite(v) && isblack(obj2gco(p)))  \
	luaC_barrierback_(L,p); }
#define luaC_objbarrier(L,p,o)  \
	{ if (iswhite(obj2gco(o)) && isblack(obj2gco(p))) \
		luaC_barrier_(L,obj2gco(p),obj2gco(o)); }
#define luaC_objbarrierback(L,p,o)  \
   { if (iswhite(obj2gco(o)) && isblack(obj2gco(p))) luaC_barrierback_(L,p); }
#define luaC_barrierproto(L,p,c) \
   { if (isblack(obj2gco(p))) luaC_barrierproto_(L,p,c); }
LUAI_FUNC void luaC_freeallobjects (lua_State *L);
LUAI_FUNC void luaC_step (lua_State *L);
LUAI_FUNC void luaC_forcestep (lua_State *L);
LUAI_FUNC void luaC_runtilstate (lua_State *L, int statesmask);
LUAI_FUNC void luaC_fullgc (lua_State *L, int isemergency);
LUAI_FUNC GCObject *luaC_newobj (lua_State *L, int tt, size_t sz,
                                 GCObject **list, int offset);
LUAI_FUNC void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v);
LUAI_FUNC void luaC_barrierback_ (lua_State *L, GCObject *o);
LUAI_FUNC void luaC_barrierproto_ (lua_State *L, Proto *p, Closure *c);
LUAI_FUNC void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt);
LUAI_FUNC void luaC_checkupvalcolor (global_State *g, UpVal *uv);
LUAI_FUNC void luaC_changemode (lua_State *L, int mode);
#endif
