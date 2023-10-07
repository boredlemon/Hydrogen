/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in cup.h
*/

#define lbaselib_c
#define CUP_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


static int cupB_console (cup_State *L) {
  int n = cup_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = cupL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      cup_writestring("\t", 1);  /* add a tab before it */
    cup_writestring(s, l);  /* print it */
    cup_pop(L, 1);  /* pop result */
  }
  cup_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int cupB_warn (cup_State *L) {
  int n = cup_gettop(L);  /* number of arguments */
  int i;
  cupL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    cupL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    cup_warning(L, cup_tostring(L, i), 1);
  cup_warning(L, cup_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, cup_Integer *pn) {
  cup_Unsigned n = 0;
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
  *pn = (cup_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int cupB_tonumber (cup_State *L) {
  if (cup_isnoneornil(L, 2)) {  /* standard conversion? */
    if (cup_type(L, 1) == CUP_TNUMBER) {  /* already a number? */
      cup_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = cup_tolstring(L, 1, &l);
      if (s != NULL && cup_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      cupL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    cup_Integer n = 0;  /* to avoid warnings */
    cup_Integer base = cupL_checkinteger(L, 2);
    cupL_checktype(L, 1, CUP_TSTRING);  /* no numbers as strings */
    s = cup_tolstring(L, 1, &l);
    cupL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      cup_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  cupL_pushfail(L);  /* not a number */
  return 1;
}


static int cupB_error (cup_State *L) {
  int level = (int)cupL_optinteger(L, 2, 1);
  cup_settop(L, 1);
  if (cup_type(L, 1) == CUP_TSTRING && level > 0) {
    cupL_where(L, level);   /* add extra information */
    cup_pushvalue(L, 1);
    cup_concat(L, 2);
  }
  return cup_error(L);
}


static int cupB_getmetatable (cup_State *L) {
  cupL_checkany(L, 1);
  if (!cup_getmetatable(L, 1)) {
    cup_pushnil(L);
    return 1;  /* no metatable */
  }
  cupL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int cupB_setmetatable (cup_State *L) {
  int t = cup_type(L, 2);
  cupL_checktype(L, 1, CUP_TTABLE);
  cupL_argexpected(L, t == CUP_TNIL || t == CUP_TTABLE, 2, "nil or table");
  if (l_unlikely(cupL_getmetafield(L, 1, "__metatable") != CUP_TNIL))
    return cupL_error(L, "cannot change a protected metatable");
  cup_settop(L, 2);
  cup_setmetatable(L, 1);
  return 1;
}


static int cupB_rawequal (cup_State *L) {
  cupL_checkany(L, 1);
  cupL_checkany(L, 2);
  cup_pushboolean(L, cup_rawequal(L, 1, 2));
  return 1;
}


static int cupB_rawlen (cup_State *L) {
  int t = cup_type(L, 1);
  cupL_argexpected(L, t == CUP_TTABLE || t == CUP_TSTRING, 1,
                      "table or string");
  cup_pushinteger(L, cup_rawlen(L, 1));
  return 1;
}


static int cupB_rawget (cup_State *L) {
  cupL_checktype(L, 1, CUP_TTABLE);
  cupL_checkany(L, 2);
  cup_settop(L, 2);
  cup_rawget(L, 1);
  return 1;
}

static int cupB_rawset (cup_State *L) {
  cupL_checktype(L, 1, CUP_TTABLE);
  cupL_checkany(L, 2);
  cupL_checkany(L, 3);
  cup_settop(L, 3);
  cup_rawset(L, 1);
  return 1;
}


static int pushmode (cup_State *L, int oldmode) {
  if (oldmode == -1)
    cupL_pushfail(L);  /* invalid call to 'cup_gc' */
  else
    cup_pushstring(L, (oldmode == CUP_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'cup_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int cupB_collectgarbage (cup_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {CUP_GCSTOP, CUP_GCRESTART, CUP_GCCOLLECT,
    CUP_GCCOUNT, CUP_GCSTEP, CUP_GCSETPAUSE, CUP_GCSETSTEPMUL,
    CUP_GCISRUNNING, CUP_GCGEN, CUP_GCINC};
  int o = optsnum[cupL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case CUP_GCCOUNT: {
      int k = cup_gc(L, o);
      int b = cup_gc(L, CUP_GCCOUNTB);
      checkvalres(k);
      cup_pushnumber(L, (cup_Number)k + ((cup_Number)b/1024));
      return 1;
    }
    case CUP_GCSTEP: {
      int step = (int)cupL_optinteger(L, 2, 0);
      int res = cup_gc(L, o, step);
      checkvalres(res);
      cup_pushboolean(L, res);
      return 1;
    }
    case CUP_GCSETPAUSE:
    case CUP_GCSETSTEPMUL: {
      int p = (int)cupL_optinteger(L, 2, 0);
      int previous = cup_gc(L, o, p);
      checkvalres(previous);
      cup_pushinteger(L, previous);
      return 1;
    }
    case CUP_GCISRUNNING: {
      int res = cup_gc(L, o);
      checkvalres(res);
      cup_pushboolean(L, res);
      return 1;
    }
    case CUP_GCGEN: {
      int minormul = (int)cupL_optinteger(L, 2, 0);
      int majormul = (int)cupL_optinteger(L, 3, 0);
      return pushmode(L, cup_gc(L, o, minormul, majormul));
    }
    case CUP_GCINC: {
      int pause = (int)cupL_optinteger(L, 2, 0);
      int stepmul = (int)cupL_optinteger(L, 3, 0);
      int stepsize = (int)cupL_optinteger(L, 4, 0);
      return pushmode(L, cup_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = cup_gc(L, o);
      checkvalres(res);
      cup_pushinteger(L, res);
      return 1;
    }
  }
  cupL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int cupB_type (cup_State *L) {
  int t = cup_type(L, 1);
  cupL_argcheck(L, t != CUP_TNONE, 1, "value expected");
  cup_pushstring(L, cup_typename(L, t));
  return 1;
}


static int cupB_next (cup_State *L) {
  cupL_checktype(L, 1, CUP_TTABLE);
  cup_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (cup_next(L, 1))
    return 2;
  else {
    cup_pushnil(L);
    return 1;
  }
}


static int pairscont (cup_State *L, int status, cup_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int cupB_pairs (cup_State *L) {
  cupL_checkany(L, 1);
  if (cupL_getmetafield(L, 1, "__pairs") == CUP_TNIL) {  /* no metamethod? */
    cup_pushcfunction(L, cupB_next);  /* will return generator, */
    cup_pushvalue(L, 1);  /* state, */
    cup_pushnil(L);  /* and initial value */
  }
  else {
    cup_pushvalue(L, 1);  /* argument 'self' to metamethod */
    cup_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (cup_State *L) {
  cup_Integer i = cupL_checkinteger(L, 2);
  i = cupL_intop(+, i, 1);
  cup_pushinteger(L, i);
  return (cup_geti(L, 1, i) == CUP_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int cupB_ipairs (cup_State *L) {
  cupL_checkany(L, 1);
  cup_pushcfunction(L, ipairsaux);  /* iteration function */
  cup_pushvalue(L, 1);  /* state */
  cup_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (cup_State *L, int status, int envidx) {
  if (l_likely(status == CUP_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      cup_pushvalue(L, envidx);  /* environment for loaded function */
      if (!cup_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        cup_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    cupL_pushfail(L);
    cup_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int cupB_loadfile (cup_State *L) {
  const char *fname = cupL_optstring(L, 1, NULL);
  const char *mode = cupL_optstring(L, 2, NULL);
  int env = (!cup_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = cupL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' function: 'cup_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (cup_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  cupL_checkstack(L, 2, "too many nested functions");
  cup_pushvalue(L, 1);  /* get function */
  cup_call(L, 0, 1);  /* call it */
  if (cup_isnil(L, -1)) {
    cup_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!cup_isstring(L, -1)))
    cupL_error(L, "reader function must return a string");
  cup_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return cup_tolstring(L, RESERVEDSLOT, size);
}


static int cupB_load (cup_State *L) {
  int status;
  size_t l;
  const char *s = cup_tolstring(L, 1, &l);
  const char *mode = cupL_optstring(L, 3, "bt");
  int env = (!cup_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = cupL_optstring(L, 2, s);
    status = cupL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = cupL_optstring(L, 2, "=(load)");
    cupL_checktype(L, 1, CUP_TFUNCTION);
    cup_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = cup_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (cup_State *L, int d1, cup_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'cup_Kfunction' prototype */
  return cup_gettop(L) - 1;
}


static int cupB_dofile (cup_State *L) {
  const char *fname = cupL_optstring(L, 1, NULL);
  cup_settop(L, 1);
  if (l_unlikely(cupL_loadfile(L, fname) != CUP_OK))
    return cup_error(L);
  cup_callk(L, 0, CUP_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int cupB_assert (cup_State *L) {
  if (l_likely(cup_toboolean(L, 1)))  /* condition is true? */
    return cup_gettop(L);  /* return all arguments */
  else {  /* error */
    cupL_checkany(L, 1);  /* there must be a condition */
    cup_remove(L, 1);  /* remove it */
    cup_pushliteral(L, "assertion failed!");  /* default message */
    cup_settop(L, 1);  /* leave only message (default if no other one) */
    return cupB_error(L);  /* call 'error' */
  }
}


static int cupB_select (cup_State *L) {
  int n = cup_gettop(L);
  if (cup_type(L, 1) == CUP_TSTRING && *cup_tostring(L, 1) == '#') {
    cup_pushinteger(L, n-1);
    return 1;
  }
  else {
    cup_Integer i = cupL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    cupL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (cup_State *L, int status, cup_KContext extra) {
  if (l_unlikely(status != CUP_OK && status != CUP_YIELD)) {  /* error? */
    cup_pushboolean(L, 0);  /* first result (false) */
    cup_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return cup_gettop(L) - (int)extra;  /* return all results */
}


static int cupB_pcall (cup_State *L) {
  int status;
  cupL_checkany(L, 1);
  cup_pushboolean(L, 1);  /* first result if no errors */
  cup_insert(L, 1);  /* put it in place */
  status = cup_pcallk(L, cup_gettop(L) - 2, CUP_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'cup_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int cupB_xpcall (cup_State *L) {
  int status;
  int n = cup_gettop(L);
  cupL_checktype(L, 2, CUP_TFUNCTION);  /* check error function */
  cup_pushboolean(L, 1);  /* first result */
  cup_pushvalue(L, 1);  /* function */
  cup_rotate(L, 3, 2);  /* move them below function's arguments */
  status = cup_pcallk(L, n - 2, CUP_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int cupB_tostring (cup_State *L) {
  cupL_checkany(L, 1);
  cupL_tolstring(L, 1, NULL);
  return 1;
}


static const cupL_Reg base_funcs[] = {
  {"assert", cupB_assert},
  {"collectgarbage", cupB_collectgarbage},
  {"dofile", cupB_dofile},
  {"error", cupB_error},
  {"getmetatable", cupB_getmetatable},
  {"ipairs", cupB_ipairs},
  {"loadfile", cupB_loadfile},
  {"load", cupB_load},
  {"next", cupB_next},
  {"pairs", cupB_pairs},
  {"pcall", cupB_pcall},
  {"console", cupB_console}, // print or terminal
  {"warn", cupB_warn},
  {"rawequal", cupB_rawequal},
  {"rawlen", cupB_rawlen},
  {"rawget", cupB_rawget},
  {"rawset", cupB_rawset},
  {"select", cupB_select},
  {"setmetatable", cupB_setmetatable},
  {"tonumber", cupB_tonumber},
  {"tostring", cupB_tostring},
  {"type", cupB_type},
  {"xpcall", cupB_xpcall},
  /* placeholders */
  {CUP_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


CUPMOD_API int cupopen_base (cup_State *L) {
  /* open lib into global table */
  cup_pushglobaltable(L);
  cupL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  cup_pushvalue(L, -1);
  cup_setfield(L, -2, CUP_GNAME);
  /* set global _VERSION */
  cup_pushliteral(L, CUP_VERSION);
  cup_setfield(L, -2, "_VERSION");
  return 1;
}

