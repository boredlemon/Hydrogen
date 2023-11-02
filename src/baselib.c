/*
** $Id: baselib.c $
** Basic library
** See Copyright Notice in venom.h
*/

#define baselib_c
#define VENOM_LIB

#include "prefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "venom.h"

#include "auxlib.h"
#include "venomlib.h"

static int venomB_print (venom_State *L) {
  int n = venom_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = venomL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      venom_writestring("\t", 1);  /* add a tab before it */
    venom_writestring(s, l);  /* print it */
    venom_pop(L, 1);  /* pop result */
  }
  venom_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int venomB_warn (venom_State *L) {
  int n = venom_gettop(L);  /* number of arguments */
  int i;
  venomL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    venomL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    venom_warning(L, venom_tostring(L, i), 1);
  venom_warning(L, venom_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, venom_Integer *pn) {
  venom_Unsigned n = 0;
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
  *pn = (venom_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int venomB_tonumber (venom_State *L) {
  if (venom_isnoneornil(L, 2)) {  /* standard conversion? */
    if (venom_type(L, 1) == VENOM_TNUMBER) {  /* already a number? */
      venom_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = venom_tolstring(L, 1, &l);
      if (s != NULL && venom_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      venomL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    venom_Integer n = 0;  /* to avoid warnings */
    venom_Integer base = venomL_checkinteger(L, 2);
    venomL_checktype(L, 1, VENOM_TSTRING);  /* no numbers as strings */
    s = venom_tolstring(L, 1, &l);
    venomL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      venom_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  venomL_pushfail(L);  /* not a number */
  return 1;
}


static int venomB_error (venom_State *L) {
  int level = (int)venomL_optinteger(L, 2, 1);
  venom_settop(L, 1);
  if (venom_type(L, 1) == VENOM_TSTRING && level > 0) {
    venomL_where(L, level);   /* add extra information */
    venom_pushvalue(L, 1);
    venom_concat(L, 2);
  }
  return venom_error(L);
}


static int venomB_getmetatable (venom_State *L) {
  venomL_checkany(L, 1);
  if (!venom_getmetatable(L, 1)) {
    venom_pushnil(L);
    return 1;  /* no metatable */
  }
  venomL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int venomB_setmetatable (venom_State *L) {
  int t = venom_type(L, 2);
  venomL_checktype(L, 1, VENOM_TTABLE);
  venomL_argexpected(L, t == VENOM_TNIL || t == VENOM_TTABLE, 2, "nil or table");
  if (l_unlikely(venomL_getmetafield(L, 1, "__metatable") != VENOM_TNIL))
    return venomL_error(L, "cannot change a protected metatable");
  venom_settop(L, 2);
  venom_setmetatable(L, 1);
  return 1;
}


static int venomB_rawequal (venom_State *L) {
  venomL_checkany(L, 1);
  venomL_checkany(L, 2);
  venom_pushboolean(L, venom_rawequal(L, 1, 2));
  return 1;
}


static int venomB_rawlen (venom_State *L) {
  int t = venom_type(L, 1);
  venomL_argexpected(L, t == VENOM_TTABLE || t == VENOM_TSTRING, 1,
                      "table or string");
  venom_pushinteger(L, venom_rawlen(L, 1));
  return 1;
}


static int venomB_rawget (venom_State *L) {
  venomL_checktype(L, 1, VENOM_TTABLE);
  venomL_checkany(L, 2);
  venom_settop(L, 2);
  venom_rawget(L, 1);
  return 1;
}

static int venomB_rawset (venom_State *L) {
  venomL_checktype(L, 1, VENOM_TTABLE);
  venomL_checkany(L, 2);
  venomL_checkany(L, 3);
  venom_settop(L, 3);
  venom_rawset(L, 1);
  return 1;
}


static int pushmode (venom_State *L, int oldmode) {
  if (oldmode == -1)
    venomL_pushfail(L);  /* invalid call to 'venom_gc' */
  else
    venom_pushstring(L, (oldmode == VENOM_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'venom_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int venomB_collectgarbage (venom_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {VENOM_GCSTOP, VENOM_GCRESTART, VENOM_GCCOLLECT,
    VENOM_GCCOUNT, VENOM_GCSTEP, VENOM_GCSETPAUSE, VENOM_GCSETSTEPMUL,
    VENOM_GCISRUNNING, VENOM_GCGEN, VENOM_GCINC};
  int o = optsnum[venomL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case VENOM_GCCOUNT: {
      int k = venom_gc(L, o);
      int b = venom_gc(L, VENOM_GCCOUNTB);
      checkvalres(k);
      venom_pushnumber(L, (venom_Number)k + ((venom_Number)b/1024));
      return 1;
    }
    case VENOM_GCSTEP: {
      int step = (int)venomL_optinteger(L, 2, 0);
      int res = venom_gc(L, o, step);
      checkvalres(res);
      venom_pushboolean(L, res);
      return 1;
    }
    case VENOM_GCSETPAUSE:
    case VENOM_GCSETSTEPMUL: {
      int p = (int)venomL_optinteger(L, 2, 0);
      int previous = venom_gc(L, o, p);
      checkvalres(previous);
      venom_pushinteger(L, previous);
      return 1;
    }
    case VENOM_GCISRUNNING: {
      int res = venom_gc(L, o);
      checkvalres(res);
      venom_pushboolean(L, res);
      return 1;
    }
    case VENOM_GCGEN: {
      int minormul = (int)venomL_optinteger(L, 2, 0);
      int majormul = (int)venomL_optinteger(L, 3, 0);
      return pushmode(L, venom_gc(L, o, minormul, majormul));
    }
    case VENOM_GCINC: {
      int pause = (int)venomL_optinteger(L, 2, 0);
      int stepmul = (int)venomL_optinteger(L, 3, 0);
      int stepsize = (int)venomL_optinteger(L, 4, 0);
      return pushmode(L, venom_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = venom_gc(L, o);
      checkvalres(res);
      venom_pushinteger(L, res);
      return 1;
    }
  }
  venomL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int venomB_type (venom_State *L) {
  int t = venom_type(L, 1);
  venomL_argcheck(L, t != VENOM_TNONE, 1, "value expected");
  venom_pushstring(L, venom_typename(L, t));
  return 1;
}


static int venomB_next (venom_State *L) {
  venomL_checktype(L, 1, VENOM_TTABLE);
  venom_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (venom_next(L, 1))
    return 2;
  else {
    venom_pushnil(L);
    return 1;
  }
}


static int pairscont (venom_State *L, int status, venom_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int venomB_pairs (venom_State *L) {
  venomL_checkany(L, 1);
  if (venomL_getmetafield(L, 1, "__pairs") == VENOM_TNIL) {  /* no metamethod? */
    venom_pushcfunction(L, venomB_next);  /* will return generator, */
    venom_pushvalue(L, 1);  /* state, */
    venom_pushnil(L);  /* and initial value */
  }
  else {
    venom_pushvalue(L, 1);  /* argument 'self' to metamethod */
    venom_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (venom_State *L) {
  venom_Integer i = venomL_checkinteger(L, 2);
  i = venomL_intop(+, i, 1);
  venom_pushinteger(L, i);
  return (venom_geti(L, 1, i) == VENOM_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int venomB_ipairs (venom_State *L) {
  venomL_checkany(L, 1);
  venom_pushcfunction(L, ipairsaux);  /* iteration function */
  venom_pushvalue(L, 1);  /* state */
  venom_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (venom_State *L, int status, int envidx) {
  if (l_likely(status == VENOM_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      venom_pushvalue(L, envidx);  /* environment for loaded function */
      if (!venom_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        venom_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    venomL_pushfail(L);
    venom_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int venomB_loadfile (venom_State *L) {
  const char *fname = venomL_optstring(L, 1, NULL);
  const char *mode = venomL_optstring(L, 2, NULL);
  int env = (!venom_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = venomL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' function: 'venom_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (venom_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  venomL_checkstack(L, 2, "too many nested functions");
  venom_pushvalue(L, 1);  /* get function */
  venom_call(L, 0, 1);  /* call it */
  if (venom_isnil(L, -1)) {
    venom_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!venom_isstring(L, -1)))
    venomL_error(L, "reader function must return a string");
  venom_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return venom_tolstring(L, RESERVEDSLOT, size);
}


static int venomB_load (venom_State *L) {
  int status;
  size_t l;
  const char *s = venom_tolstring(L, 1, &l);
  const char *mode = venomL_optstring(L, 3, "bt");
  int env = (!venom_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = venomL_optstring(L, 2, s);
    status = venomL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = venomL_optstring(L, 2, "=(load)");
    venomL_checktype(L, 1, VENOM_TFUNCTION);
    venom_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = venom_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (venom_State *L, int d1, venom_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'venom_Kfunction' prototype */
  return venom_gettop(L) - 1;
}


static int venomB_dofile (venom_State *L) {
  const char *fname = venomL_optstring(L, 1, NULL);
  venom_settop(L, 1);
  if (l_unlikely(venomL_loadfile(L, fname) != VENOM_OK))
    return venom_error(L);
  venom_callk(L, 0, VENOM_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int venomB_assert (venom_State *L) {
  if (l_likely(venom_toboolean(L, 1)))  /* condition is true? */
    return venom_gettop(L);  /* return all arguments */
  else {  /* error */
    venomL_checkany(L, 1);  /* there must be a condition */
    venom_remove(L, 1);  /* remove it */
    venom_pushliteral(L, "assertion failed!");  /* default message */
    venom_settop(L, 1);  /* leave only message (default if no other one) */
    return venomB_error(L);  /* call 'error' */
  }
}


static int venomB_select (venom_State *L) {
  int n = venom_gettop(L);
  if (venom_type(L, 1) == VENOM_TSTRING && *venom_tostring(L, 1) == '#') {
    venom_pushinteger(L, n-1);
    return 1;
  }
  else {
    venom_Integer i = venomL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    venomL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (venom_State *L, int status, venom_KContext extra) {
  if (l_unlikely(status != VENOM_OK && status != VENOM_YIELD)) {  /* error? */
    venom_pushboolean(L, 0);  /* first result (false) */
    venom_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return venom_gettop(L) - (int)extra;  /* return all results */
}


static int venomB_pcall (venom_State *L) {
  int status;
  venomL_checkany(L, 1);
  venom_pushboolean(L, 1);  /* first result if no errors */
  venom_insert(L, 1);  /* put it in place */
  status = venom_pcallk(L, venom_gettop(L) - 2, VENOM_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'venom_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int venomB_xpcall (venom_State *L) {
  int status;
  int n = venom_gettop(L);
  venomL_checktype(L, 2, VENOM_TFUNCTION);  /* check error function */
  venom_pushboolean(L, 1);  /* first result */
  venom_pushvalue(L, 1);  /* function */
  venom_rotate(L, 3, 2);  /* move them below function's arguments */
  status = venom_pcallk(L, n - 2, VENOM_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int venomB_tostring (venom_State *L) {
  venomL_checkany(L, 1);
  venomL_tolstring(L, 1, NULL);
  return 1;
}


static const venomL_Reg base_funcs[] = {
  {"assert", venomB_assert},
  {"collectgarbage", venomB_collectgarbage},
  {"dofile", venomB_dofile},
  {"error", venomB_error},
  {"getmetatable", venomB_getmetatable},
  {"ipairs", venomB_ipairs},
  {"loadfile", venomB_loadfile},
  {"load", venomB_load},
  {"next", venomB_next},
  {"pairs", venomB_pairs},
  {"pcall", venomB_pcall},
  {"print", venomB_print}, // print or terminal
  {"warn", venomB_warn},
  {"rawequal", venomB_rawequal},
  {"rawlen", venomB_rawlen},
  {"rawget", venomB_rawget},
  {"rawset", venomB_rawset},
  {"select", venomB_select},
  {"setmetatable", venomB_setmetatable},
  {"tonumber", venomB_tonumber},
  {"tostring", venomB_tostring},
  {"type", venomB_type},
  {"xpcall", venomB_xpcall},
  /* placeholders */
  {VENOM_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


VENOMMOD_API int venomopen_base (venom_State *L) {
  /* open lib into global table */
  venom_pushglobaltable(L);
  venomL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  venom_pushvalue(L, -1);
  venom_setfield(L, -2, VENOM_GNAME);
  /* set global _VERSION */
  venom_pushliteral(L, VENOM_VERSION);
  venom_setfield(L, -2, "_VERSION");
  return 1;
}