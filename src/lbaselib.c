/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in viper.h
*/

#define lbaselib_c
#define VIPER_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viper.h"

#include "lauxlib.h"
#include "viperlib.h"


static int viperB_console (viper_State *L) {
  int n = viper_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = viperL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      viper_writestring("\t", 1);  /* add a tab before it */
    viper_writestring(s, l);  /* print it */
    viper_pop(L, 1);  /* pop result */
  }
  viper_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int viperB_warn (viper_State *L) {
  int n = viper_gettop(L);  /* number of arguments */
  int i;
  viperL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    viperL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    viper_warning(L, viper_tostring(L, i), 1);
  viper_warning(L, viper_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, viper_Integer *pn) {
  viper_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum((unsigned char)*s))  /* no digit? */
    return NULL;
  do {
    int digit = (isdigit((unsigned char)*s)) ? *s - '0'
                   : (toupper((unsigned char)*s) - 'A') + 10;
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum((unsigned char)*s));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (viper_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int viperB_tonumber (viper_State *L) {
  if (viper_isnoneornil(L, 2)) {  /* standard conversion? */
    if (viper_type(L, 1) == VIPER_TNUMBER) {  /* already a number? */
      viper_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = viper_tolstring(L, 1, &l);
      if (s != NULL && viper_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      viperL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    viper_Integer n = 0;  /* to avoid warnings */
    viper_Integer base = viperL_checkinteger(L, 2);
    viperL_checktype(L, 1, VIPER_TSTRING);  /* no numbers as strings */
    s = viper_tolstring(L, 1, &l);
    viperL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      viper_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  viperL_pushfail(L);  /* not a number */
  return 1;
}


static int viperB_error (viper_State *L) {
  int level = (int)viperL_optinteger(L, 2, 1);
  viper_settop(L, 1);
  if (viper_type(L, 1) == VIPER_TSTRING && level > 0) {
    viperL_where(L, level);   /* add extra information */
    viper_pushvalue(L, 1);
    viper_concat(L, 2);
  }
  return viper_error(L);
}


static int viperB_getmetatable (viper_State *L) {
  viperL_checkany(L, 1);
  if (!viper_getmetatable(L, 1)) {
    viper_pushnil(L);
    return 1;  /* no metatable */
  }
  viperL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int viperB_setmetatable (viper_State *L) {
  int t = viper_type(L, 2);
  viperL_checktype(L, 1, VIPER_TTABLE);
  viperL_argexpected(L, t == VIPER_TNIL || t == VIPER_TTABLE, 2, "nil or table");
  if (l_unlikely(viperL_getmetafield(L, 1, "__metatable") != VIPER_TNIL))
    return viperL_error(L, "cannot change a protected metatable");
  viper_settop(L, 2);
  viper_setmetatable(L, 1);
  return 1;
}


static int viperB_rawequal (viper_State *L) {
  viperL_checkany(L, 1);
  viperL_checkany(L, 2);
  viper_pushboolean(L, viper_rawequal(L, 1, 2));
  return 1;
}


static int viperB_rawlen (viper_State *L) {
  int t = viper_type(L, 1);
  viperL_argexpected(L, t == VIPER_TTABLE || t == VIPER_TSTRING, 1,
                      "table or string");
  viper_pushinteger(L, viper_rawlen(L, 1));
  return 1;
}


static int viperB_rawget (viper_State *L) {
  viperL_checktype(L, 1, VIPER_TTABLE);
  viperL_checkany(L, 2);
  viper_settop(L, 2);
  viper_rawget(L, 1);
  return 1;
}

static int viperB_rawset (viper_State *L) {
  viperL_checktype(L, 1, VIPER_TTABLE);
  viperL_checkany(L, 2);
  viperL_checkany(L, 3);
  viper_settop(L, 3);
  viper_rawset(L, 1);
  return 1;
}


static int pushmode (viper_State *L, int oldmode) {
  if (oldmode == -1)
    viperL_pushfail(L);  /* invalid call to 'viper_gc' */
  else
    viper_pushstring(L, (oldmode == VIPER_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'viper_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int viperB_collectgarbage (viper_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {VIPER_GCSTOP, VIPER_GCRESTART, VIPER_GCCOLLECT,
    VIPER_GCCOUNT, VIPER_GCSTEP, VIPER_GCSETPAUSE, VIPER_GCSETSTEPMUL,
    VIPER_GCISRUNNING, VIPER_GCGEN, VIPER_GCINC};
  int o = optsnum[viperL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case VIPER_GCCOUNT: {
      int k = viper_gc(L, o);
      int b = viper_gc(L, VIPER_GCCOUNTB);
      checkvalres(k);
      viper_pushnumber(L, (viper_Number)k + ((viper_Number)b/1024));
      return 1;
    }
    case VIPER_GCSTEP: {
      int step = (int)viperL_optinteger(L, 2, 0);
      int res = viper_gc(L, o, step);
      checkvalres(res);
      viper_pushboolean(L, res);
      return 1;
    }
    case VIPER_GCSETPAUSE:
    case VIPER_GCSETSTEPMUL: {
      int p = (int)viperL_optinteger(L, 2, 0);
      int previous = viper_gc(L, o, p);
      checkvalres(previous);
      viper_pushinteger(L, previous);
      return 1;
    }
    case VIPER_GCISRUNNING: {
      int res = viper_gc(L, o);
      checkvalres(res);
      viper_pushboolean(L, res);
      return 1;
    }
    case VIPER_GCGEN: {
      int minormul = (int)viperL_optinteger(L, 2, 0);
      int majormul = (int)viperL_optinteger(L, 3, 0);
      return pushmode(L, viper_gc(L, o, minormul, majormul));
    }
    case VIPER_GCINC: {
      int pause = (int)viperL_optinteger(L, 2, 0);
      int stepmul = (int)viperL_optinteger(L, 3, 0);
      int stepsize = (int)viperL_optinteger(L, 4, 0);
      return pushmode(L, viper_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = viper_gc(L, o);
      checkvalres(res);
      viper_pushinteger(L, res);
      return 1;
    }
  }
  viperL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int viperB_type (viper_State *L) {
  int t = viper_type(L, 1);
  viperL_argcheck(L, t != VIPER_TNONE, 1, "value expected");
  viper_pushstring(L, viper_typename(L, t));
  return 1;
}


static int viperB_next (viper_State *L) {
  viperL_checktype(L, 1, VIPER_TTABLE);
  viper_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (viper_next(L, 1))
    return 2;
  else {
    viper_pushnil(L);
    return 1;
  }
}


static int pairscont (viper_State *L, int status, viper_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int viperB_pairs (viper_State *L) {
  viperL_checkany(L, 1);
  if (viperL_getmetafield(L, 1, "__pairs") == VIPER_TNIL) {  /* no metamethod? */
    viper_pushcfunction(L, viperB_next);  /* will return generator, */
    viper_pushvalue(L, 1);  /* state, */
    viper_pushnil(L);  /* and initial value */
  }
  else {
    viper_pushvalue(L, 1);  /* argument 'self' to metamethod */
    viper_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (viper_State *L) {
  viper_Integer i = viperL_checkinteger(L, 2);
  i = viperL_intop(+, i, 1);
  viper_pushinteger(L, i);
  return (viper_geti(L, 1, i) == VIPER_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int viperB_ipairs (viper_State *L) {
  viperL_checkany(L, 1);
  viper_pushcfunction(L, ipairsaux);  /* iteration function */
  viper_pushvalue(L, 1);  /* state */
  viper_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (viper_State *L, int status, int envidx) {
  if (l_likely(status == VIPER_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      viper_pushvalue(L, envidx);  /* environment for loaded function */
      if (!viper_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        viper_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    viperL_pushfail(L);
    viper_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int viperB_loadfile (viper_State *L) {
  const char *fname = viperL_optstring(L, 1, NULL);
  const char *mode = viperL_optstring(L, 2, NULL);
  int env = (!viper_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = viperL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'viper_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (viper_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  viperL_checkstack(L, 2, "too many nested functions");
  viper_pushvalue(L, 1);  /* get function */
  viper_call(L, 0, 1);  /* call it */
  if (viper_isnil(L, -1)) {
    viper_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!viper_isstring(L, -1)))
    viperL_error(L, "reader function must return a string");
  viper_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return viper_tolstring(L, RESERVEDSLOT, size);
}


static int viperB_load (viper_State *L) {
  int status;
  size_t l;
  const char *s = viper_tolstring(L, 1, &l);
  const char *mode = viperL_optstring(L, 3, "bt");
  int env = (!viper_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = viperL_optstring(L, 2, s);
    status = viperL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = viperL_optstring(L, 2, "=(load)");
    viperL_checktype(L, 1, VIPER_TFUNCTION);
    viper_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = viper_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (viper_State *L, int d1, viper_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'viper_Kfunction' prototype */
  return viper_gettop(L) - 1;
}


static int viperB_dofile (viper_State *L) {
  const char *fname = viperL_optstring(L, 1, NULL);
  viper_settop(L, 1);
  if (l_unlikely(viperL_loadfile(L, fname) != VIPER_OK))
    return viper_error(L);
  viper_callk(L, 0, VIPER_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int viperB_assert (viper_State *L) {
  if (l_likely(viper_toboolean(L, 1)))  /* condition is true? */
    return viper_gettop(L);  /* return all arguments */
  else {  /* error */
    viperL_checkany(L, 1);  /* there must be a condition */
    viper_remove(L, 1);  /* remove it */
    viper_pushliteral(L, "assertion failed!");  /* default message */
    viper_settop(L, 1);  /* leave only message (default if no other one) */
    return viperB_error(L);  /* call 'error' */
  }
}


static int viperB_select (viper_State *L) {
  int n = viper_gettop(L);
  if (viper_type(L, 1) == VIPER_TSTRING && *viper_tostring(L, 1) == '#') {
    viper_pushinteger(L, n-1);
    return 1;
  }
  else {
    viper_Integer i = viperL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    viperL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (viper_State *L, int status, viper_KContext extra) {
  if (l_unlikely(status != VIPER_OK && status != VIPER_YIELD)) {  /* error? */
    viper_pushboolean(L, 0);  /* first result (false) */
    viper_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return viper_gettop(L) - (int)extra;  /* return all results */
}


static int viperB_pcall (viper_State *L) {
  int status;
  viperL_checkany(L, 1);
  viper_pushboolean(L, 1);  /* first result if no errors */
  viper_insert(L, 1);  /* put it in place */
  status = viper_pcallk(L, viper_gettop(L) - 2, VIPER_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'viper_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int viperB_xpcall (viper_State *L) {
  int status;
  int n = viper_gettop(L);
  viperL_checktype(L, 2, VIPER_TFUNCTION);  /* check error function */
  viper_pushboolean(L, 1);  /* first result */
  viper_pushvalue(L, 1);  /* function */
  viper_rotate(L, 3, 2);  /* move them below function's arguments */
  status = viper_pcallk(L, n - 2, VIPER_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int viperB_tostring (viper_State *L) {
  viperL_checkany(L, 1);
  viperL_tolstring(L, 1, NULL);
  return 1;
}


static const viperL_Reg base_funcs[] = {
  {"assert", viperB_assert},
  {"collectgarbage", viperB_collectgarbage},
  {"dofile", viperB_dofile},
  {"error", viperB_error},
  {"getmetatable", viperB_getmetatable},
  {"ipairs", viperB_ipairs},
  {"loadfile", viperB_loadfile},
  {"load", viperB_load},
  {"next", viperB_next},
  {"pairs", viperB_pairs},
  {"pcall", viperB_pcall},
  {"console", viperB_console}, // print or terminal
  {"warn", viperB_warn},
  {"rawequal", viperB_rawequal},
  {"rawlen", viperB_rawlen},
  {"rawget", viperB_rawget},
  {"rawset", viperB_rawset},
  {"select", viperB_select},
  {"setmetatable", viperB_setmetatable},
  {"tonumber", viperB_tonumber},
  {"tostring", viperB_tostring},
  {"type", viperB_type},
  {"xpcall", viperB_xpcall},
  /* placeholders */
  {VIPER_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


VIPERMOD_API int viperopen_base (viper_State *L) {
  /* open lib into global table */
  viper_pushglobaltable(L);
  viperL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  viper_pushvalue(L, -1);
  viper_setfield(L, -2, VIPER_GNAME);
  /* set global _VERSION */
  viper_pushliteral(L, VIPER_VERSION);
  viper_setfield(L, -2, "_VERSION");
  return 1;
}

