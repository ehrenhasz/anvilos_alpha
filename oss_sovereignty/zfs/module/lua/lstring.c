#define lstring_c
#define LUA_CORE
#include <sys/lua/lua.h>
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#if !defined(LUAI_HASHLIMIT)
#define LUAI_HASHLIMIT		5
#endif
int luaS_eqlngstr (TString *a, TString *b) {
  size_t len = a->tsv.len;
  lua_assert(a->tsv.tt == LUA_TLNGSTR && b->tsv.tt == LUA_TLNGSTR);
  return (a == b) ||   
    ((len == b->tsv.len) &&   
     (memcmp(getstr(a), getstr(b), len) == 0));   
}
int luaS_eqstr (TString *a, TString *b) {
  return (a->tsv.tt == b->tsv.tt) &&
         (a->tsv.tt == LUA_TSHRSTR ? eqshrstr(a, b) : luaS_eqlngstr(a, b));
}
unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast(unsigned int, l);
  size_t l1;
  size_t step = (l >> LUAI_HASHLIMIT) + 1;
  for (l1 = l; l1 >= step; l1 -= step)
    h = h ^ ((h<<5) + (h>>2) + cast_byte(str[l1 - 1]));
  return h;
}
void luaS_resize (lua_State *L, int newsize) {
  int i;
  stringtable *tb = &G(L)->strt;
  luaC_runtilstate(L, ~bitmask(GCSsweepstring));
  if (newsize > tb->size) {
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
    for (i = tb->size; i < newsize; i++) tb->hash[i] = NULL;
  }
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {   
      GCObject *next = gch(p)->next;   
      unsigned int h = lmod(gco2ts(p)->hash, newsize);   
      gch(p)->next = tb->hash[h];   
      tb->hash[h] = p;
      resetoldbit(p);   
      p = next;
    }
  }
  if (newsize < tb->size) {
    lua_assert(tb->hash[newsize] == NULL && tb->hash[tb->size - 1] == NULL);
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
  }
  tb->size = newsize;
}
static TString *createstrobj (lua_State *L, const char *str, size_t l,
                              int tag, unsigned int h, GCObject **list) {
  TString *ts;
  char *sbuf;
  size_t totalsize;   
  totalsize = sizeof(TString) + ((l + 1) * sizeof(char));
  ts = &luaC_newobj(L, tag, totalsize, list, 0)->ts;
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.extra = 0;
  sbuf = (char *)(TString *)(ts + 1);
  memcpy(sbuf, str, l*sizeof(char));
  sbuf[l] = '\0';   
  return ts;
}
static TString *newshrstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) {
  GCObject **list;   
  stringtable *tb = &G(L)->strt;
  TString *s;
  if (tb->nuse >= cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, tb->size*2);   
  list = &tb->hash[lmod(h, tb->size)];
  s = createstrobj(L, str, l, LUA_TSHRSTR, h, list);
  tb->nuse++;
  return s;
}
static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  global_State *g = G(L);
  unsigned int h = luaS_hash(str, l, g->seed);
  for (o = g->strt.hash[lmod(h, g->strt.size)];
       o != NULL;
       o = gch(o)->next) {
    TString *ts = rawgco2ts(o);
    if (h == ts->tsv.hash &&
        l == ts->tsv.len &&
        (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
      if (isdead(G(L), o))   
        changewhite(o);   
      return ts;
    }
  }
  return newshrstr(L, str, l, h);   
}
TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  if (l <= LUAI_MAXSHORTLEN)   
    return internshrstr(L, str, l);
  else {
    if (l + 1 > (MAX_SIZET - sizeof(TString))/sizeof(char))
      luaM_toobig(L);
    return createstrobj(L, str, l, LUA_TLNGSTR, G(L)->seed, NULL);
  }
}
TString *luaS_new (lua_State *L, const char *str) {
  return luaS_newlstr(L, str, strlen(str));
}
Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = &luaC_newobj(L, LUA_TUSERDATA, sizeof(Udata) + s, NULL, 0)->u;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  return u;
}
