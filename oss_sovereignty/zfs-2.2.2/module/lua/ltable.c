 


 


#define ltable_c
#define LUA_CORE

#include <sys/lua/lua.h>

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"


 
#if LUAI_BITSINT >= 32
#define MAXBITS		30
#else
#define MAXBITS		(LUAI_BITSINT-2)
#endif

#define MAXASIZE	(1 << MAXBITS)


#define hashpow2(t,n)		(gnode(t, lmod((n), sizenode(t))))

#define hashstr(t,str)		hashpow2(t, (str)->tsv.hash)
#define hashboolean(t,p)	hashpow2(t, p)


 
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))


#define hashpointer(t,p)	hashmod(t, IntPoint(p))


#define dummynode		(&dummynode_)

#define isdummy(n)		((n) == dummynode)

static const Node dummynode_ = {
  {NILCONSTANT},   
  {{NILCONSTANT, NULL}}   
};


 
static Node *hashnum (const Table *t, lua_Number n) {
  int i;
  luai_hashnum(i, n);
  if (i < 0) {
    if (cast(unsigned int, i) == 0u - i)   
      i = 0;   
    i = -i;   
  }
  return hashmod(t, i);
}



 
static Node *mainposition (const Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TLNGSTR: {
      TString *s = rawtsvalue(key);
      if (s->tsv.extra == 0) {   
        s->tsv.hash = luaS_hash(getstr(s), s->tsv.len, s->tsv.hash);
        s->tsv.extra = 1;   
      }
      return hashstr(t, rawtsvalue(key));
    }
    case LUA_TSHRSTR:
      return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
    case LUA_TLCF:
      return hashpointer(t, fvalue(key));
    default:
      return hashpointer(t, gcvalue(key));
  }
}


 
static int arrayindex (const TValue *key) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;   
}


 
static int findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;   
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)   
    return i-1;   
  else {
    Node *n = mainposition(t, key);
    for (;;) {   
       
      if (luaV_rawequalobj(gkey(n), key) ||
            (ttisdeadkey(gkey(n)) && iscollectable(key) &&
             deadvalue(gkey(n)) == gcvalue(key))) {
        i = cast_int(n - gnode(t, 0));   
         
        return i + t->sizearray;
      }
      else n = gnext(n);
      if (n == NULL)
        luaG_runerror(L, "invalid key to " LUA_QL("next"));   
    }
  }
}


int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = findindex(L, t, key);   
  for (i++; i < t->sizearray; i++) {   
    if (!ttisnil(&t->array[i])) {   
      setnvalue(key, cast_num(i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {   
    if (!ttisnil(gval(gnode(t, i)))) {   
      setobj2s(L, key, gkey(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;   
}


 


static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;   
  int a = 0;   
  int na = 0;   
  int n = 0;   
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a > twotoi/2) {   
        n = twotoi;   
        na = a;   
      }
    }
    if (a == *narray) break;   
  }
  *narray = n;
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}


static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {   
    nums[luaO_ceillog2(k)]++;   
    return 1;
  }
  else
    return 0;
}


static int numusearray (const Table *t, int *nums) {
  int lg;
  int ttlg;   
  int ause = 0;   
  int i = 1;   
  for (lg=0, ttlg=1; lg<=MAXBITS; lg++, ttlg*=2) {   
    int lc = 0;   
    int lim = ttlg;
    if (lim > t->sizearray) {
      lim = t->sizearray;   
      if (i > lim)
        break;   
    }
     
    for (; i <= lim; i++) {
      if (!ttisnil(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}


static int numusehash (const Table *t, int *nums, int *pnasize) {
  int totaluse = 0;   
  int ause = 0;   
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      ause += countint(gkey(n), nums);
      totaluse++;
    }
  }
  *pnasize += ause;
  return totaluse;
}


static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}


static void setnodevector (lua_State *L, Table *t, int size) {
  int lsize;
  if (size == 0) {   
    t->node = cast(Node *, dummynode);   
    lsize = 0;
  }
  else {
    int i;
    lsize = luaO_ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, Node);
    for (i=0; i<size; i++) {
      Node *n = gnode(t, i);
      gnext(n) = NULL;
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
    }
  }
  t->lsizenode = cast_byte(lsize);
  t->lastfree = gnode(t, size);   
}


void luaH_resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize = t->sizearray;
  int oldhsize = t->lsizenode;
  Node *nold = t->node;   
  if (nasize > oldasize)   
    setarrayvector(L, t, nasize);
   
  setnodevector(L, t, nhsize);
  if (nasize < oldasize) {   
    t->sizearray = nasize;
     
    for (i=nasize; i<oldasize; i++) {
      if (!ttisnil(&t->array[i]))
        luaH_setint(L, t, i + 1, &t->array[i]);
    }
     
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
   
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold+i;
    if (!ttisnil(gval(old))) {
       
      setobjt2t(L, luaH_set(L, t, gkey(old)), gval(old));
    }
  }
  if (!isdummy(nold))
    luaM_freearray(L, nold, cast(size_t, twoto(oldhsize)));  
}


void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nsize = isdummy(t->node) ? 0 : sizenode(t);
  luaH_resize(L, t, nasize, nsize);
}


static void rehash (lua_State *L, Table *t, const TValue *ek) {
  int nasize, na;
  int nums[MAXBITS+1];   
  int i;
  int totaluse;
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;   
  nasize = numusearray(t, nums);   
  totaluse = nasize;   
  totaluse += numusehash(t, nums, &nasize);   
   
  nasize += countint(ek, nums);
  totaluse++;
   
  na = computesizes(nums, &nasize);
   
  luaH_resize(L, t, nasize, totaluse - na);
}



 


Table *luaH_new (lua_State *L) {
  Table *t = &luaC_newobj(L, LUA_TTABLE, sizeof(Table), NULL, 0)->h;
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  t->array = NULL;
  t->sizearray = 0;
  setnodevector(L, t, 0);
  return t;
}


void luaH_free (lua_State *L, Table *t) {
  if (!isdummy(t->node))
    luaM_freearray(L, t->node, cast(size_t, sizenode(t)));
  luaM_freearray(L, t->array, t->sizearray);
  luaM_free(L, t);
}


static Node *getfreepos (Table *t) {
  while (t->lastfree > t->node) {
    t->lastfree--;
    if (ttisnil(gkey(t->lastfree)))
      return t->lastfree;
  }
  return NULL;   
}



 
TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key) {
  Node *mp;
  if (ttisnil(key)) luaG_runerror(L, "table index is nil");
#if defined LUA_HAS_FLOAT_NUMBERS
  else if (ttisnumber(key) && luai_numisnan(L, nvalue(key)))
    luaG_runerror(L, "table index is NaN");
#endif
  mp = mainposition(t, key);
  if (!ttisnil(gval(mp)) || isdummy(mp)) {   
    Node *othern;
    Node *n = getfreepos(t);   
    if (n == NULL) {   
      rehash(L, t, key);   
       
      return luaH_set(L, t, key);   
    }
    lua_assert(!isdummy(n));
    othern = mainposition(t, gkey(mp));
    if (othern != mp) {   
       
      while (gnext(othern) != mp) othern = gnext(othern);   
      gnext(othern) = n;   
      *n = *mp;   
      gnext(mp) = NULL;   
      setnilvalue(gval(mp));
    }
    else {   
       
      gnext(n) = gnext(mp);   
      gnext(mp) = n;
      mp = n;
    }
  }
  setobj2t(L, gkey(mp), key);
  luaC_barrierback(L, obj2gco(t), key);
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


 
const TValue *luaH_getint (Table *t, int key) {
   
  if (cast(unsigned int, key-1) < cast(unsigned int, t->sizearray))
    return &t->array[key-1];
  else {
    lua_Number nk = cast_num(key);
    Node *n = hashnum(t, nk);
    do {   
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
        return gval(n);   
      else n = gnext(n);
    } while (n);
    return luaO_nilobject;
  }
}


 
const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  lua_assert(key->tsv.tt == LUA_TSHRSTR);
  do {   
    if (ttisshrstring(gkey(n)) && eqshrstr(rawtsvalue(gkey(n)), key))
      return gval(n);   
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


 
const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TSHRSTR: return luaH_getstr(t, rawtsvalue(key));
    case LUA_TNIL: return luaO_nilobject;
    case LUA_TNUMBER: {
      int k;
      lua_Number n = nvalue(key);
      lua_number2int(k, n);
      if (luai_numeq(cast_num(k), n))  
        return luaH_getint(t, k);   
       
    }
      zfs_fallthrough;
    default: {
      Node *n = mainposition(t, key);
      do {   
        if (luaV_rawequalobj(gkey(n), key))
          return gval(n);   
        else n = gnext(n);
      } while (n);
      return luaO_nilobject;
    }
  }
}


 
TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
  const TValue *p = luaH_get(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else return luaH_newkey(L, t, key);
}


void luaH_setint (lua_State *L, Table *t, int key, TValue *value) {
  const TValue *p = luaH_getint(t, key);
  TValue *cell;
  if (p != luaO_nilobject)
    cell = cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    cell = luaH_newkey(L, t, &k);
  }
  setobj2t(L, cell, value);
}


static int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;   
  j++;
   
  while (!ttisnil(luaH_getint(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {   
       
      i = 1;
      while (!ttisnil(luaH_getint(t, i))) i++;
      return i - 1;
    }
  }
   
  while (j - i > 1) {
    unsigned int m = (i+j)/2;
    if (ttisnil(luaH_getint(t, m))) j = m;
    else i = m;
  }
  return i;
}


 
int luaH_getn (Table *t) {
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
     
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = (i+j)/2;
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
   
  else if (isdummy(t->node))   
    return j;   
  else return unbound_search(t, j);
}



#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainposition(t, key);
}

int luaH_isdummy (Node *n) { return isdummy(n); }

#endif
