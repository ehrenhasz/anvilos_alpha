 

#define lgc_c
#define LUA_CORE

#include <sys/lua/lua.h>

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



 
#define GCSWEEPCOST	((sizeof(TString) + 4) / 4)

 
#define GCSWEEPMAX	(cast_int((GCSTEPSIZE / GCSWEEPCOST) / 4))

 
#define GCFINALIZENUM	4


 
#define STEPMULADJ		200


 
#define PAUSEADJ		100


 
#define maskcolors	(~(bit2mask(BLACKBIT, OLDBIT) | WHITEBITS))
#define makewhite(g,x)	\
 (gch(x)->marked = cast_byte((gch(x)->marked & maskcolors) | luaC_white(g)))

#define white2gray(x)	resetbits(gch(x)->marked, WHITEBITS)
#define black2gray(x)	resetbit(gch(x)->marked, BLACKBIT)


#define isfinalized(x)		testbit(gch(x)->marked, FINALIZEDBIT)

#define checkdeadkey(n)	lua_assert(!ttisdeadkey(gkey(n)) || ttisnil(gval(n)))


#define checkconsistency(obj)  \
  lua_longassert(!iscollectable(obj) || righttt(obj))


#define markvalue(g,o) { checkconsistency(o); \
  if (valiswhite(o)) reallymarkobject(g,gcvalue(o)); }

#define markobject(g,t) { if ((t) && iswhite(obj2gco(t))) \
		reallymarkobject(g, obj2gco(t)); }

static void reallymarkobject (global_State *g, GCObject *o);


 


 
#define gnodelast(h)	gnode(h, cast(size_t, sizenode(h)))


 
#define linktable(h,p)	((h)->gclist = *(p), *(p) = obj2gco(h))


 
static void removeentry (Node *n) {
  lua_assert(ttisnil(gval(n)));
  if (valiswhite(gkey(n)))
    setdeadvalue(gkey(n));   
}


 
static int iscleared (global_State *g, const TValue *o) {
  if (!iscollectable(o)) return 0;
  else if (ttisstring(o)) {
    markobject(g, rawtsvalue(o));   
    return 0;
  }
  else return iswhite(gcvalue(o));
}


 
void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v) {
  global_State *g = G(L);
  lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
  lua_assert(g->gcstate != GCSpause);
  lua_assert(gch(o)->tt != LUA_TTABLE);
  if (keepinvariantout(g))   
    reallymarkobject(g, v);   
  else {   
    lua_assert(issweepphase(g));
    makewhite(g, o);   
  }
}


 
void luaC_barrierback_ (lua_State *L, GCObject *o) {
  global_State *g = G(L);
  lua_assert(isblack(o) && !isdead(g, o) && gch(o)->tt == LUA_TTABLE);
  black2gray(o);   
  gco2t(o)->gclist = g->grayagain;
  g->grayagain = o;
}


 
LUAI_FUNC void luaC_barrierproto_ (lua_State *L, Proto *p, Closure *c) {
  global_State *g = G(L);
  lua_assert(isblack(obj2gco(p)));
  if (p->cache == NULL) {   
    luaC_objbarrier(L, p, c);
  }
  else {   
    black2gray(obj2gco(p));   
    p->gclist = g->grayagain;
    g->grayagain = obj2gco(p);
  }
}


 
void luaC_checkupvalcolor (global_State *g, UpVal *uv) {
  GCObject *o = obj2gco(uv);
  lua_assert(!isblack(o));   
  if (isgray(o)) {
    if (keepinvariant(g)) {
      resetoldbit(o);   
      gray2black(o);   
      markvalue(g, uv->v);
    }
    else {
      lua_assert(issweepphase(g));
      makewhite(g, o);
    }
  }
}


 
GCObject *luaC_newobj (lua_State *L, int tt, size_t sz, GCObject **list,
                       int offset) {
  global_State *g = G(L);
  char *raw = cast(char *, luaM_newobject(L, novariant(tt), sz));
  GCObject *o = obj2gco(raw + offset);
  if (list == NULL)
    list = &g->allgc;   
  gch(o)->marked = luaC_white(g);
  gch(o)->tt = tt;
  gch(o)->next = *list;
  *list = o;
  return o;
}

 



 


 
static void reallymarkobject (global_State *g, GCObject *o) {
  lu_mem size;
  white2gray(o);
  switch (gch(o)->tt) {
    case LUA_TSHRSTR:
    case LUA_TLNGSTR: {
      size = sizestring(gco2ts(o));
      break;   
    }
    case LUA_TUSERDATA: {
      Table *mt = gco2u(o)->metatable;
      markobject(g, mt);
      markobject(g, gco2u(o)->env);
      size = sizeudata(gco2u(o));
      break;
    }
    case LUA_TUPVAL: {
      UpVal *uv = gco2uv(o);
      markvalue(g, uv->v);
      if (uv->v != &uv->u.value)   
        return;   
      size = sizeof(UpVal);
      break;
    }
    case LUA_TLCL: {
      gco2lcl(o)->gclist = g->gray;
      g->gray = o;
      return;
    }
    case LUA_TCCL: {
      gco2ccl(o)->gclist = g->gray;
      g->gray = o;
      return;
    }
    case LUA_TTABLE: {
      linktable(gco2t(o), &g->gray);
      return;
    }
    case LUA_TTHREAD: {
      gco2th(o)->gclist = g->gray;
      g->gray = o;
      return;
    }
    case LUA_TPROTO: {
      gco2p(o)->gclist = g->gray;
      g->gray = o;
      return;
    }
    default: lua_assert(0); return;
  }
  gray2black(o);
  g->GCmemtrav += size;
}


 
static void markmt (global_State *g) {
  int i;
  for (i=0; i < LUA_NUMTAGS; i++)
    markobject(g, g->mt[i]);
}


 
static void markbeingfnz (global_State *g) {
  GCObject *o;
  for (o = g->tobefnz; o != NULL; o = gch(o)->next) {
    makewhite(g, o);
    reallymarkobject(g, o);
  }
}


 
static void remarkupvals (global_State *g) {
  UpVal *uv;
  for (uv = g->uvhead.u.l.next; uv != &g->uvhead; uv = uv->u.l.next) {
    if (isgray(obj2gco(uv)))
      markvalue(g, uv->v);
  }
}


 
static void restartcollection (global_State *g) {
  g->gray = g->grayagain = NULL;
  g->weak = g->allweak = g->ephemeron = NULL;
  markobject(g, g->mainthread);
  markvalue(g, &g->l_registry);
  markmt(g);
  markbeingfnz(g);   
}

 


 

static void traverseweakvalue (global_State *g, Table *h) {
  Node *n, *limit = gnodelast(h);
   
  int hasclears = (h->sizearray > 0);
  for (n = gnode(h, 0); n < limit; n++) {
    checkdeadkey(n);
    if (ttisnil(gval(n)))   
      removeentry(n);   
    else {
      lua_assert(!ttisnil(gkey(n)));
      markvalue(g, gkey(n));   
      if (!hasclears && iscleared(g, gval(n)))   
        hasclears = 1;   
    }
  }
  if (hasclears)
    linktable(h, &g->weak);   
  else   
    linktable(h, &g->grayagain);   
}


static int traverseephemeron (global_State *g, Table *h) {
  int marked = 0;   
  int hasclears = 0;   
  int prop = 0;   
  Node *n, *limit = gnodelast(h);
  int i;
   
  for (i = 0; i < h->sizearray; i++) {
    if (valiswhite(&h->array[i])) {
      marked = 1;
      reallymarkobject(g, gcvalue(&h->array[i]));
    }
  }
   
  for (n = gnode(h, 0); n < limit; n++) {
    checkdeadkey(n);
    if (ttisnil(gval(n)))   
      removeentry(n);   
    else if (iscleared(g, gkey(n))) {   
      hasclears = 1;   
      if (valiswhite(gval(n)))   
        prop = 1;   
    }
    else if (valiswhite(gval(n))) {   
      marked = 1;
      reallymarkobject(g, gcvalue(gval(n)));   
    }
  }
  if (g->gcstate != GCSatomic || prop)
    linktable(h, &g->ephemeron);   
  else if (hasclears)   
    linktable(h, &g->allweak);   
  else   
    linktable(h, &g->grayagain);   
  return marked;
}


static void traversestrongtable (global_State *g, Table *h) {
  Node *n, *limit = gnodelast(h);
  int i;
  for (i = 0; i < h->sizearray; i++)   
    markvalue(g, &h->array[i]);
  for (n = gnode(h, 0); n < limit; n++) {   
    checkdeadkey(n);
    if (ttisnil(gval(n)))   
      removeentry(n);   
    else {
      lua_assert(!ttisnil(gkey(n)));
      markvalue(g, gkey(n));   
      markvalue(g, gval(n));   
    }
  }
}


static lu_mem traversetable (global_State *g, Table *h) {
  const char *weakkey, *weakvalue;
  const TValue *mode = gfasttm(g, h->metatable, TM_MODE);
  markobject(g, h->metatable);
  if (mode && ttisstring(mode) &&   
      ((weakkey = strchr(svalue(mode), 'k')),
       (weakvalue = strchr(svalue(mode), 'v')),
       (weakkey || weakvalue))) {   
    black2gray(obj2gco(h));   
    if (!weakkey)   
      traverseweakvalue(g, h);
    else if (!weakvalue)   
      traverseephemeron(g, h);
    else   
      linktable(h, &g->allweak);   
  }
  else   
    traversestrongtable(g, h);
  return sizeof(Table) + sizeof(TValue) * h->sizearray +
                         sizeof(Node) * cast(size_t, sizenode(h));
}


static int traverseproto (global_State *g, Proto *f) {
  int i;
  if (f->cache && iswhite(obj2gco(f->cache)))
    f->cache = NULL;   
  markobject(g, f->source);
  for (i = 0; i < f->sizek; i++)   
    markvalue(g, &f->k[i]);
  for (i = 0; i < f->sizeupvalues; i++)   
    markobject(g, f->upvalues[i].name);
  for (i = 0; i < f->sizep; i++)   
    markobject(g, f->p[i]);
  for (i = 0; i < f->sizelocvars; i++)   
    markobject(g, f->locvars[i].varname);
  return sizeof(Proto) + sizeof(Instruction) * f->sizecode +
                         sizeof(Proto *) * f->sizep +
                         sizeof(TValue) * f->sizek +
                         sizeof(int) * f->sizelineinfo +
                         sizeof(LocVar) * f->sizelocvars +
                         sizeof(Upvaldesc) * f->sizeupvalues;
}


static lu_mem traverseCclosure (global_State *g, CClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++)   
    markvalue(g, &cl->upvalue[i]);
  return sizeCclosure(cl->nupvalues);
}

static lu_mem traverseLclosure (global_State *g, LClosure *cl) {
  int i;
  markobject(g, cl->p);   
  for (i = 0; i < cl->nupvalues; i++)   
    markobject(g, cl->upvals[i]);
  return sizeLclosure(cl->nupvalues);
}


static lu_mem traversestack (global_State *g, lua_State *th) {
  int n = 0;
  StkId o = th->stack;
  if (o == NULL)
    return 1;   
  for (; o < th->top; o++)   
    markvalue(g, o);
  if (g->gcstate == GCSatomic) {   
    StkId lim = th->stack + th->stacksize;   
    for (; o < lim; o++)   
      setnilvalue(o);
  }
  else {   
    CallInfo *ci;
    for (ci = &th->base_ci; ci != th->ci; ci = ci->next)
      n++;
  }
  return sizeof(lua_State) + sizeof(TValue) * th->stacksize +
         sizeof(CallInfo) * n;
}


 
static void propagatemark (global_State *g) {
  lu_mem size;
  GCObject *o = g->gray;
  lua_assert(isgray(o));
  gray2black(o);
  switch (gch(o)->tt) {
    case LUA_TTABLE: {
      Table *h = gco2t(o);
      g->gray = h->gclist;   
      size = traversetable(g, h);
      break;
    }
    case LUA_TLCL: {
      LClosure *cl = gco2lcl(o);
      g->gray = cl->gclist;   
      size = traverseLclosure(g, cl);
      break;
    }
    case LUA_TCCL: {
      CClosure *cl = gco2ccl(o);
      g->gray = cl->gclist;   
      size = traverseCclosure(g, cl);
      break;
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      g->gray = th->gclist;   
      th->gclist = g->grayagain;
      g->grayagain = o;   
      black2gray(o);
      size = traversestack(g, th);
      break;
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      g->gray = p->gclist;   
      size = traverseproto(g, p);
      break;
    }
    default: lua_assert(0); return;
  }
  g->GCmemtrav += size;
}


static void propagateall (global_State *g) {
  while (g->gray) propagatemark(g);
}


static void propagatelist (global_State *g, GCObject *l) {
  lua_assert(g->gray == NULL);   
  g->gray = l;
  propagateall(g);   
}

 
static void retraversegrays (global_State *g) {
  GCObject *weak = g->weak;   
  GCObject *grayagain = g->grayagain;
  GCObject *ephemeron = g->ephemeron;
  g->weak = g->grayagain = g->ephemeron = NULL;
  propagateall(g);   
  propagatelist(g, grayagain);
  propagatelist(g, weak);
  propagatelist(g, ephemeron);
}


static void convergeephemerons (global_State *g) {
  int changed;
  do {
    GCObject *w;
    GCObject *next = g->ephemeron;   
    g->ephemeron = NULL;   
    changed = 0;
    while ((w = next) != NULL) {
      next = gco2t(w)->gclist;
      if (traverseephemeron(g, gco2t(w))) {   
        propagateall(g);   
        changed = 1;   
      }
    }
  } while (changed);
}

 


 


 
static void clearkeys (global_State *g, GCObject *l, GCObject *f) {
  for (; l != f; l = gco2t(l)->gclist) {
    Table *h = gco2t(l);
    Node *n, *limit = gnodelast(h);
    for (n = gnode(h, 0); n < limit; n++) {
      if (!ttisnil(gval(n)) && (iscleared(g, gkey(n)))) {
        setnilvalue(gval(n));   
        removeentry(n);   
      }
    }
  }
}


 
static void clearvalues (global_State *g, GCObject *l, GCObject *f) {
  for (; l != f; l = gco2t(l)->gclist) {
    Table *h = gco2t(l);
    Node *n, *limit = gnodelast(h);
    int i;
    for (i = 0; i < h->sizearray; i++) {
      TValue *o = &h->array[i];
      if (iscleared(g, o))   
        setnilvalue(o);   
    }
    for (n = gnode(h, 0); n < limit; n++) {
      if (!ttisnil(gval(n)) && iscleared(g, gval(n))) {
        setnilvalue(gval(n));   
        removeentry(n);   
      }
    }
  }
}


static void freeobj (lua_State *L, GCObject *o) {
  switch (gch(o)->tt) {
    case LUA_TPROTO: luaF_freeproto(L, gco2p(o)); break;
    case LUA_TLCL: {
      luaM_freemem(L, o, sizeLclosure(gco2lcl(o)->nupvalues));
      break;
    }
    case LUA_TCCL: {
      luaM_freemem(L, o, sizeCclosure(gco2ccl(o)->nupvalues));
      break;
    }
    case LUA_TUPVAL: luaF_freeupval(L, gco2uv(o)); break;
    case LUA_TTABLE: luaH_free(L, gco2t(o)); break;
    case LUA_TTHREAD: luaE_freethread(L, gco2th(o)); break;
    case LUA_TUSERDATA: luaM_freemem(L, o, sizeudata(gco2u(o))); break;
    case LUA_TSHRSTR:
      G(L)->strt.nuse--;
      zfs_fallthrough;
    case LUA_TLNGSTR: {
      luaM_freemem(L, o, sizestring(gco2ts(o)));
      break;
    }
    default: lua_assert(0);
  }
}


#define sweepwholelist(L,p)	sweeplist(L,p,MAX_LUMEM)
static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count);


 
static void sweepthread (lua_State *L, lua_State *L1) {
  if (L1->stack == NULL) return;   
  sweepwholelist(L, &L1->openupval);   
  luaE_freeCI(L1);   
   
  if (G(L)->gckind != KGC_EMERGENCY)
    luaD_shrinkstack(L1);
}


 
static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count) {
  global_State *g = G(L);
  int ow = otherwhite(g);
  int toclear, toset;   
  int tostop;   
  if (isgenerational(g)) {   
    toclear = ~0;   
    toset = bitmask(OLDBIT);   
    tostop = bitmask(OLDBIT);   
  }
  else {   
    toclear = maskcolors;   
    toset = luaC_white(g);   
    tostop = 0;   
  }
  while (*p != NULL && count-- > 0) {
    GCObject *curr = *p;
    int marked = gch(curr)->marked;
    if (isdeadm(ow, marked)) {   
      *p = gch(curr)->next;   
      freeobj(L, curr);   
    }
    else {
      if (testbits(marked, tostop))
        return NULL;   
      if (gch(curr)->tt == LUA_TTHREAD)
        sweepthread(L, gco2th(curr));   
       
      gch(curr)->marked = cast_byte((marked & toclear) | toset);
      p = &gch(curr)->next;   
    }
  }
  return (*p == NULL) ? NULL : p;
}


 
static GCObject **sweeptolive (lua_State *L, GCObject **p, int *n) {
  GCObject ** old = p;
  int i = 0;
  do {
    i++;
    p = sweeplist(L, p, 1);
  } while (p == old);
  if (n) *n += i;
  return p;
}

 


 

static void checkSizes (lua_State *L) {
  global_State *g = G(L);
  if (g->gckind != KGC_EMERGENCY) {   
    int hs = g->strt.size / 2;   
    if (g->strt.nuse < cast(lu_int32, hs))   
      luaS_resize(L, hs);   
    luaZ_freebuffer(L, &g->buff);   
  }
}


static GCObject *udata2finalize (global_State *g) {
  GCObject *o = g->tobefnz;   
  lua_assert(isfinalized(o));
  g->tobefnz = gch(o)->next;   
  gch(o)->next = g->allgc;   
  g->allgc = o;
  resetbit(gch(o)->marked, SEPARATED);   
  lua_assert(!isold(o));   
  if (!keepinvariantout(g))   
    makewhite(g, o);   
  return o;
}


static void dothecall (lua_State *L, void *ud) {
  UNUSED(ud);
  luaD_call(L, L->top - 2, 0, 0);
}


static void GCTM (lua_State *L, int propagateerrors) {
  global_State *g = G(L);
  const TValue *tm;
  TValue v;
  setgcovalue(L, &v, udata2finalize(g));
  tm = luaT_gettmbyobj(L, &v, TM_GC);
  if (tm != NULL && ttisfunction(tm)) {   
    int status;
    lu_byte oldah = L->allowhook;
    int running  = g->gcrunning;
    L->allowhook = 0;   
    g->gcrunning = 0;   
    setobj2s(L, L->top, tm);   
    setobj2s(L, L->top + 1, &v);   
    L->top += 2;   
    status = luaD_pcall(L, dothecall, NULL, savestack(L, L->top - 2), 0);
    L->allowhook = oldah;   
    g->gcrunning = running;   
    if (status != LUA_OK && propagateerrors) {   
      if (status == LUA_ERRRUN) {   
        const char *msg = (ttisstring(L->top - 1))
                            ? svalue(L->top - 1)
                            : "no message";
        luaO_pushfstring(L, "error in __gc metamethod (%s)", msg);
        status = LUA_ERRGCMM;   
      }
      luaD_throw(L, status);   
    }
  }
}


 
static void separatetobefnz (lua_State *L, int all) {
  global_State *g = G(L);
  GCObject **p = &g->finobj;
  GCObject *curr;
  GCObject **lastnext = &g->tobefnz;
   
  while (*lastnext != NULL)
    lastnext = &gch(*lastnext)->next;
  while ((curr = *p) != NULL) {   
    lua_assert(!isfinalized(curr));
    lua_assert(testbit(gch(curr)->marked, SEPARATED));
    if (!(iswhite(curr) || all))   
      p = &gch(curr)->next;   
    else {
      l_setbit(gch(curr)->marked, FINALIZEDBIT);  
      *p = gch(curr)->next;   
      gch(curr)->next = *lastnext;   
      *lastnext = curr;
      lastnext = &gch(curr)->next;
    }
  }
}


 
void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt) {
  global_State *g = G(L);
  if (testbit(gch(o)->marked, SEPARATED) ||  
      isfinalized(o) ||                            
      gfasttm(g, mt, TM_GC) == NULL)                 
    return;   
  else {   
    GCObject **p;
    GCheader *ho = gch(o);
    if (g->sweepgc == &ho->next) {   
      lua_assert(issweepphase(g));
      g->sweepgc = sweeptolive(L, g->sweepgc, NULL);
    }
     
    for (p = &g->allgc; *p != o; p = &gch(*p)->next) {   }
    *p = ho->next;   
    ho->next = g->finobj;   
    g->finobj = o;
    l_setbit(ho->marked, SEPARATED);   
    if (!keepinvariantout(g))   
      makewhite(g, o);   
    else
      resetoldbit(o);   
  }
}

 


 


 
static void setpause (global_State *g, l_mem estimate) {
  l_mem debt, threshold;
  estimate = estimate / PAUSEADJ;   
  threshold = (g->gcpause < MAX_LMEM / estimate)   
            ? estimate * g->gcpause   
            : MAX_LMEM;   
  debt = -cast(l_mem, threshold - gettotalbytes(g));
  luaE_setdebt(g, debt);
}


#define sweepphases  \
	(bitmask(GCSsweepstring) | bitmask(GCSsweepudata) | bitmask(GCSsweep))


 
static int entersweep (lua_State *L) {
  global_State *g = G(L);
  int n = 0;
  g->gcstate = GCSsweepstring;
  lua_assert(g->sweepgc == NULL && g->sweepfin == NULL);
   
  g->sweepstrgc = 0;
  g->sweepfin = sweeptolive(L, &g->finobj, &n);
  g->sweepgc = sweeptolive(L, &g->allgc, &n);
  return n;
}


 
void luaC_changemode (lua_State *L, int mode) {
  global_State *g = G(L);
  if (mode == g->gckind) return;   
  if (mode == KGC_GEN) {   
     
    luaC_runtilstate(L, bitmask(GCSpropagate));
    g->GCestimate = gettotalbytes(g);
    g->gckind = KGC_GEN;
  }
  else {   
     
    g->gckind = KGC_NORMAL;
    entersweep(L);
    luaC_runtilstate(L, ~sweepphases);
  }
}


 
static void callallpendingfinalizers (lua_State *L, int propagateerrors) {
  global_State *g = G(L);
  while (g->tobefnz) {
    resetoldbit(g->tobefnz);
    GCTM(L, propagateerrors);
  }
}


void luaC_freeallobjects (lua_State *L) {
  global_State *g = G(L);
  int i;
  separatetobefnz(L, 1);   
  lua_assert(g->finobj == NULL);
  callallpendingfinalizers(L, 0);
  g->currentwhite = WHITEBITS;  
  g->gckind = KGC_NORMAL;
  sweepwholelist(L, &g->finobj);   
  sweepwholelist(L, &g->allgc);
  for (i = 0; i < g->strt.size; i++)   
    sweepwholelist(L, &g->strt.hash[i]);
  lua_assert(g->strt.nuse == 0);
}


static l_mem atomic (lua_State *L) {
  global_State *g = G(L);
  l_mem work = -cast(l_mem, g->GCmemtrav);   
  GCObject *origweak, *origall;
  lua_assert(!iswhite(obj2gco(g->mainthread)));
  markobject(g, L);   
   
  markvalue(g, &g->l_registry);
  markmt(g);   
   
  remarkupvals(g);
  propagateall(g);   
  work += g->GCmemtrav;   
   
  retraversegrays(g);
  work -= g->GCmemtrav;   
  convergeephemerons(g);
   
   
  clearvalues(g, g->weak, NULL);
  clearvalues(g, g->allweak, NULL);
  origweak = g->weak; origall = g->allweak;
  work += g->GCmemtrav;   
  separatetobefnz(L, 0);   
  markbeingfnz(g);   
  propagateall(g);   
  work -= g->GCmemtrav;   
  convergeephemerons(g);
   
   
  clearkeys(g, g->ephemeron, NULL);   
  clearkeys(g, g->allweak, NULL);   
   
  clearvalues(g, g->weak, origweak);
  clearvalues(g, g->allweak, origall);
  g->currentwhite = cast_byte(otherwhite(g));   
  work += g->GCmemtrav;   
  return work;   
}


static lu_mem singlestep (lua_State *L) {
  global_State *g = G(L);
  switch (g->gcstate) {
    case GCSpause: {
       
      g->GCmemtrav = g->strt.size * sizeof(GCObject*);
      lua_assert(!isgenerational(g));
      restartcollection(g);
      g->gcstate = GCSpropagate;
      return g->GCmemtrav;
    }
    case GCSpropagate: {
      if (g->gray) {
        lu_mem oldtrav = g->GCmemtrav;
        propagatemark(g);
        return g->GCmemtrav - oldtrav;   
      }
      else {   
        lu_mem work;
        int sw;
        g->gcstate = GCSatomic;   
        g->GCestimate = g->GCmemtrav;   
        work = atomic(L);   
        g->GCestimate += work;   
        sw = entersweep(L);
        return work + sw * GCSWEEPCOST;
      }
    }
    case GCSsweepstring: {
      int i;
      for (i = 0; i < GCSWEEPMAX && g->sweepstrgc + i < g->strt.size; i++)
        sweepwholelist(L, &g->strt.hash[g->sweepstrgc + i]);
      g->sweepstrgc += i;
      if (g->sweepstrgc >= g->strt.size)   
        g->gcstate = GCSsweepudata;
      return i * GCSWEEPCOST;
    }
    case GCSsweepudata: {
      if (g->sweepfin) {
        g->sweepfin = sweeplist(L, g->sweepfin, GCSWEEPMAX);
        return GCSWEEPMAX*GCSWEEPCOST;
      }
      else {
        g->gcstate = GCSsweep;
        return 0;
      }
    }
    case GCSsweep: {
      if (g->sweepgc) {
        g->sweepgc = sweeplist(L, g->sweepgc, GCSWEEPMAX);
        return GCSWEEPMAX*GCSWEEPCOST;
      }
      else {
         
        GCObject *mt = obj2gco(g->mainthread);
        sweeplist(L, &mt, 1);
        checkSizes(L);
        g->gcstate = GCSpause;   
        return GCSWEEPCOST;
      }
    }
    default: lua_assert(0); return 0;
  }
}


 
void luaC_runtilstate (lua_State *L, int statesmask) {
  global_State *g = G(L);
  while (!testbit(statesmask, g->gcstate))
    singlestep(L);
}


static void generationalcollection (lua_State *L) {
  global_State *g = G(L);
  lua_assert(g->gcstate == GCSpropagate);
  if (g->GCestimate == 0) {   
    luaC_fullgc(L, 0);   
    g->GCestimate = gettotalbytes(g);   
  }
  else {
    lu_mem estimate = g->GCestimate;
    luaC_runtilstate(L, bitmask(GCSpause));   
    g->gcstate = GCSpropagate;   
    if (gettotalbytes(g) > (estimate / 100) * g->gcmajorinc)
      g->GCestimate = 0;   
    else
      g->GCestimate = estimate;   

  }
  setpause(g, gettotalbytes(g));
  lua_assert(g->gcstate == GCSpropagate);
}


static void incstep (lua_State *L) {
  global_State *g = G(L);
  l_mem debt = g->GCdebt;
  int stepmul = g->gcstepmul;
  if (stepmul < 40) stepmul = 40;   
   
  debt = (debt / STEPMULADJ) + 1;
  debt = (debt < MAX_LMEM / stepmul) ? debt * stepmul : MAX_LMEM;
  do {   
    lu_mem work = singlestep(L);   
    debt -= work;
  } while (debt > -GCSTEPSIZE && g->gcstate != GCSpause);
  if (g->gcstate == GCSpause)
    setpause(g, g->GCestimate);   
  else {
    debt = (debt / stepmul) * STEPMULADJ;   
    luaE_setdebt(g, debt);
  }
}


 
void luaC_forcestep (lua_State *L) {
  global_State *g = G(L);
  int i;
  if (isgenerational(g)) generationalcollection(L);
  else incstep(L);
   
  for (i = 0; g->tobefnz && (i < GCFINALIZENUM || g->gcstate == GCSpause); i++)
    GCTM(L, 1);   
}


 
void luaC_step (lua_State *L) {
  global_State *g = G(L);
  if (g->gcrunning) luaC_forcestep(L);
  else luaE_setdebt(g, -GCSTEPSIZE);   
}



 
void luaC_fullgc (lua_State *L, int isemergency) {
  global_State *g = G(L);
  int origkind = g->gckind;
  lua_assert(origkind != KGC_EMERGENCY);
  if (isemergency)   
    g->gckind = KGC_EMERGENCY;
  else {
    g->gckind = KGC_NORMAL;
    callallpendingfinalizers(L, 1);
  }
  if (keepinvariant(g)) {   
     
    entersweep(L);
  }
   
  luaC_runtilstate(L, bitmask(GCSpause));
  luaC_runtilstate(L, ~bitmask(GCSpause));   
  luaC_runtilstate(L, bitmask(GCSpause));   
  if (origkind == KGC_GEN) {   
     
    luaC_runtilstate(L, bitmask(GCSpropagate));
  }
  g->gckind = origkind;
  setpause(g, gettotalbytes(g));
  if (!isemergency)    
    callallpendingfinalizers(L, 1);
}

 
