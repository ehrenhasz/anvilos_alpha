#define ltablib_c
#define LUA_LIB
#include <sys/lua/lua.h>
#include <sys/lua/lauxlib.h>
#include <sys/lua/lualib.h>
#define aux_getn(L,n)	(luaL_checktype(L, n, LUA_TTABLE), luaL_len(L, n))
#if defined(LUA_COMPAT_MAXN)
static int maxn (lua_State *L) {
  lua_Number max = 0;
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushnil(L);   
  while (lua_next(L, 1)) {
    lua_pop(L, 1);   
    if (lua_type(L, -1) == LUA_TNUMBER) {
      lua_Number v = lua_tonumber(L, -1);
      if (v > max) max = v;
    }
  }
  lua_pushnumber(L, max);
  return 1;
}
#endif
static int tinsert (lua_State *L) {
  int e = aux_getn(L, 1) + 1;   
  int pos;   
  switch (lua_gettop(L)) {
    case 2: {   
      pos = e;   
      break;
    }
    case 3: {
      int i;
      pos = luaL_checkint(L, 2);   
      luaL_argcheck(L, 1 <= pos && pos <= e, 2, "position out of bounds");
      for (i = e; i > pos; i--) {   
        lua_rawgeti(L, 1, i-1);
        lua_rawseti(L, 1, i);   
      }
      break;
    }
    default: {
      return luaL_error(L, "wrong number of arguments to " LUA_QL("insert"));
    }
  }
  lua_rawseti(L, 1, pos);   
  return 0;
}
static int tremove (lua_State *L) {
  int size = aux_getn(L, 1);
  int pos = luaL_optint(L, 2, size);
  if (pos != size)   
    luaL_argcheck(L, 1 <= pos && pos <= size + 1, 1, "position out of bounds");
  lua_rawgeti(L, 1, pos);   
  for ( ; pos < size; pos++) {
    lua_rawgeti(L, 1, pos+1);
    lua_rawseti(L, 1, pos);   
  }
  lua_pushnil(L);
  lua_rawseti(L, 1, pos);   
  return 1;
}
static void addfield (lua_State *L, luaL_Buffer *b, int i) {
  lua_rawgeti(L, 1, i);
  if (!lua_isstring(L, -1))
    luaL_error(L, "invalid value (%s) at index %d in table for "
                  LUA_QL("concat"), luaL_typename(L, -1), i);
  luaL_addvalue(b);
}
static int tconcat (lua_State *L) {
  luaL_Buffer b;
  size_t lsep;
  int i, last;
  const char *sep = luaL_optlstring(L, 2, "", &lsep);
  luaL_checktype(L, 1, LUA_TTABLE);
  i = luaL_optint(L, 3, 1);
  last = luaL_opt(L, luaL_checkint, 4, luaL_len(L, 1));
  luaL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    luaL_addlstring(&b, sep, lsep);
  }
  if (i == last)   
    addfield(L, &b, i);
  luaL_pushresult(&b);
  return 1;
}
static int pack (lua_State *L) {
  int n = lua_gettop(L);   
  lua_createtable(L, n, 1);   
  lua_pushinteger(L, n);
  lua_setfield(L, -2, "n");   
  if (n > 0) {   
    int i;
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);   
    lua_replace(L, 1);   
    for (i = n; i >= 2; i--)   
      lua_rawseti(L, 1, i);
  }
  return 1;   
}
static int unpack (lua_State *L) {
  int i, e;
  unsigned int n;
  luaL_checktype(L, 1, LUA_TTABLE);
  i = luaL_optint(L, 2, 1);
  e = luaL_opt(L, luaL_checkint, 3, luaL_len(L, 1));
  if (i > e) return 0;   
  n = (unsigned int)e - (unsigned int)i;   
  if (n > (INT_MAX - 10) || !lua_checkstack(L, ++n))
    return luaL_error(L, "too many results to unpack");
  lua_rawgeti(L, 1, i);   
  while (i++ < e)   
    lua_rawgeti(L, 1, i);
  return n;
}
static void set2 (lua_State *L, int i, int j) {
  lua_rawseti(L, 1, i);
  lua_rawseti(L, 1, j);
}
static int sort_comp (lua_State *L, int a, int b) {
  if (!lua_isnil(L, 2)) {   
    int res;
    lua_pushvalue(L, 2);
    lua_pushvalue(L, a-1);   
    lua_pushvalue(L, b-2);   
    lua_call(L, 2, 1);
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
  }
  else   
    return lua_compare(L, a, b, LUA_OPLT);
}
static void auxsort (lua_State *L, int l, int u) {
  while (l < u) {   
    int i, j;
    lua_rawgeti(L, 1, l);
    lua_rawgeti(L, 1, u);
    if (sort_comp(L, -1, -2))   
      set2(L, l, u);   
    else
      lua_pop(L, 2);
    if (u-l == 1) break;   
    i = (l+u)/2;
    lua_rawgeti(L, 1, i);
    lua_rawgeti(L, 1, l);
    if (sort_comp(L, -2, -1))   
      set2(L, i, l);
    else {
      lua_pop(L, 1);   
      lua_rawgeti(L, 1, u);
      if (sort_comp(L, -1, -2))   
        set2(L, i, u);
      else
        lua_pop(L, 2);
    }
    if (u-l == 2) break;   
    lua_rawgeti(L, 1, i);   
    lua_pushvalue(L, -1);
    lua_rawgeti(L, 1, u-1);
    set2(L, i, u-1);
    i = l; j = u-1;
    for (;;) {   
      while (lua_rawgeti(L, 1, ++i), sort_comp(L, -1, -2)) {
        if (i>=u) luaL_error(L, "invalid order function for sorting");
        lua_pop(L, 1);   
      }
      while (lua_rawgeti(L, 1, --j), sort_comp(L, -3, -1)) {
        if (j<=l) luaL_error(L, "invalid order function for sorting");
        lua_pop(L, 1);   
      }
      if (j<i) {
        lua_pop(L, 3);   
        break;
      }
      set2(L, i, j);
    }
    lua_rawgeti(L, 1, u-1);
    lua_rawgeti(L, 1, i);
    set2(L, u-1, i);   
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    }
    else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(L, j, i);   
  }   
}
static int tsort (lua_State *L) {
  int n = aux_getn(L, 1);
  luaL_checkstack(L, 40, "");   
  if (!lua_isnoneornil(L, 2))   
    luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_settop(L, 2);   
  auxsort(L, 1, n);
  return 0;
}
static const luaL_Reg tab_funcs[] = {
  {"concat", tconcat},
#if defined(LUA_COMPAT_MAXN)
  {"maxn", maxn},
#endif
  {"insert", tinsert},
  {"pack", pack},
  {"unpack", unpack},
  {"remove", tremove},
  {"sort", tsort},
  {NULL, NULL}
};
LUAMOD_API int luaopen_table (lua_State *L) {
  luaL_newlib(L, tab_funcs);
#if defined(LUA_COMPAT_UNPACK)
  lua_getfield(L, -1, "unpack");
  lua_setglobal(L, "unpack");
#endif
  return 1;
}
#if defined(_KERNEL)
EXPORT_SYMBOL(luaopen_table);
#endif
