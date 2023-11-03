/*
** $Id: baselib.c $
** Basic library
** See Copyright Notice in nebula.h
*/

#define baselib_c
#define NEBULA_LIB

#include "prefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nebula.h"

#include "auxlib.h"
#include "nebulalib.h"

static int nebulaB_print (nebula_State *L) {
  int n = nebula_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = nebulaL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      nebula_writestring("\t", 1);  /* add a tab before it */
    nebula_writestring(s, l);  /* print it */
    nebula_pop(L, 1);  /* pop result */
  }
  nebula_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int nebulaB_warn (nebula_State *L) {
  int n = nebula_gettop(L);  /* number of arguments */
  int i;
  nebulaL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    nebulaL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    nebula_warning(L, nebula_tostring(L, i), 1);
  nebula_warning(L, nebula_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, nebula_Integer *pn) {
  nebula_Unsigned n = 0;
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
  *pn = (nebula_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int nebulaB_tonumber (nebula_State *L) {
  if (nebula_isnoneornil(L, 2)) {  /* standard conversion? */
    if (nebula_type(L, 1) == NEBULA_TNUMBER) {  /* already a number? */
      nebula_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = nebula_tolstring(L, 1, &l);
      if (s != NULL && nebula_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      nebulaL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    nebula_Integer n = 0;  /* to avoid warnings */
    nebula_Integer base = nebulaL_checkinteger(L, 2);
    nebulaL_checktype(L, 1, NEBULA_TSTRING);  /* no numbers as strings */
    s = nebula_tolstring(L, 1, &l);
    nebulaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      nebula_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  nebulaL_pushfail(L);  /* not a number */
  return 1;
}


static int nebulaB_error (nebula_State *L) {
  int level = (int)nebulaL_optinteger(L, 2, 1);
  nebula_settop(L, 1);
  if (nebula_type(L, 1) == NEBULA_TSTRING && level > 0) {
    nebulaL_where(L, level);   /* add extra information */
    nebula_pushvalue(L, 1);
    nebula_concat(L, 2);
  }
  return nebula_error(L);
}


static int nebulaB_getmetatable (nebula_State *L) {
  nebulaL_checkany(L, 1);
  if (!nebula_getmetatable(L, 1)) {
    nebula_pushnil(L);
    return 1;  /* no metatable */
  }
  nebulaL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int nebulaB_setmetatable (nebula_State *L) {
  int t = nebula_type(L, 2);
  nebulaL_checktype(L, 1, NEBULA_TTABLE);
  nebulaL_argexpected(L, t == NEBULA_TNIL || t == NEBULA_TTABLE, 2, "nil or table");
  if (l_unlikely(nebulaL_getmetafield(L, 1, "__metatable") != NEBULA_TNIL))
    return nebulaL_error(L, "cannot change a protected metatable");
  nebula_settop(L, 2);
  nebula_setmetatable(L, 1);
  return 1;
}


static int nebulaB_rawequal (nebula_State *L) {
  nebulaL_checkany(L, 1);
  nebulaL_checkany(L, 2);
  nebula_pushboolean(L, nebula_rawequal(L, 1, 2));
  return 1;
}


static int nebulaB_rawlen (nebula_State *L) {
  int t = nebula_type(L, 1);
  nebulaL_argexpected(L, t == NEBULA_TTABLE || t == NEBULA_TSTRING, 1,
                      "table or string");
  nebula_pushinteger(L, nebula_rawlen(L, 1));
  return 1;
}


static int nebulaB_rawget (nebula_State *L) {
  nebulaL_checktype(L, 1, NEBULA_TTABLE);
  nebulaL_checkany(L, 2);
  nebula_settop(L, 2);
  nebula_rawget(L, 1);
  return 1;
}

static int nebulaB_rawset (nebula_State *L) {
  nebulaL_checktype(L, 1, NEBULA_TTABLE);
  nebulaL_checkany(L, 2);
  nebulaL_checkany(L, 3);
  nebula_settop(L, 3);
  nebula_rawset(L, 1);
  return 1;
}


static int pushmode (nebula_State *L, int oldmode) {
  if (oldmode == -1)
    nebulaL_pushfail(L);  /* invalid call to 'nebula_gc' */
  else
    nebula_pushstring(L, (oldmode == NEBULA_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'nebula_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int nebulaB_collectgarbage (nebula_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {NEBULA_GCSTOP, NEBULA_GCRESTART, NEBULA_GCCOLLECT,
    NEBULA_GCCOUNT, NEBULA_GCSTEP, NEBULA_GCSETPAUSE, NEBULA_GCSETSTEPMUL,
    NEBULA_GCISRUNNING, NEBULA_GCGEN, NEBULA_GCINC};
  int o = optsnum[nebulaL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case NEBULA_GCCOUNT: {
      int k = nebula_gc(L, o);
      int b = nebula_gc(L, NEBULA_GCCOUNTB);
      checkvalres(k);
      nebula_pushnumber(L, (nebula_Number)k + ((nebula_Number)b/1024));
      return 1;
    }
    case NEBULA_GCSTEP: {
      int step = (int)nebulaL_optinteger(L, 2, 0);
      int res = nebula_gc(L, o, step);
      checkvalres(res);
      nebula_pushboolean(L, res);
      return 1;
    }
    case NEBULA_GCSETPAUSE:
    case NEBULA_GCSETSTEPMUL: {
      int p = (int)nebulaL_optinteger(L, 2, 0);
      int previous = nebula_gc(L, o, p);
      checkvalres(previous);
      nebula_pushinteger(L, previous);
      return 1;
    }
    case NEBULA_GCISRUNNING: {
      int res = nebula_gc(L, o);
      checkvalres(res);
      nebula_pushboolean(L, res);
      return 1;
    }
    case NEBULA_GCGEN: {
      int minormul = (int)nebulaL_optinteger(L, 2, 0);
      int majormul = (int)nebulaL_optinteger(L, 3, 0);
      return pushmode(L, nebula_gc(L, o, minormul, majormul));
    }
    case NEBULA_GCINC: {
      int pause = (int)nebulaL_optinteger(L, 2, 0);
      int stepmul = (int)nebulaL_optinteger(L, 3, 0);
      int stepsize = (int)nebulaL_optinteger(L, 4, 0);
      return pushmode(L, nebula_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = nebula_gc(L, o);
      checkvalres(res);
      nebula_pushinteger(L, res);
      return 1;
    }
  }
  nebulaL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int nebulaB_type (nebula_State *L) {
  int t = nebula_type(L, 1);
  nebulaL_argcheck(L, t != NEBULA_TNONE, 1, "value expected");
  nebula_pushstring(L, nebula_typename(L, t));
  return 1;
}


static int nebulaB_next (nebula_State *L) {
  nebulaL_checktype(L, 1, NEBULA_TTABLE);
  nebula_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (nebula_next(L, 1))
    return 2;
  else {
    nebula_pushnil(L);
    return 1;
  }
}


static int pairscont (nebula_State *L, int status, nebula_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int nebulaB_pairs (nebula_State *L) {
  nebulaL_checkany(L, 1);
  if (nebulaL_getmetafield(L, 1, "__pairs") == NEBULA_TNIL) {  /* no metamethod? */
    nebula_pushcfunction(L, nebulaB_next);  /* will return generator, */
    nebula_pushvalue(L, 1);  /* state, */
    nebula_pushnil(L);  /* and initial value */
  }
  else {
    nebula_pushvalue(L, 1);  /* argument 'self' to metamethod */
    nebula_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (nebula_State *L) {
  nebula_Integer i = nebulaL_checkinteger(L, 2);
  i = nebulaL_intop(+, i, 1);
  nebula_pushinteger(L, i);
  return (nebula_geti(L, 1, i) == NEBULA_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int nebulaB_ipairs (nebula_State *L) {
  nebulaL_checkany(L, 1);
  nebula_pushcfunction(L, ipairsaux);  /* iteration function */
  nebula_pushvalue(L, 1);  /* state */
  nebula_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (nebula_State *L, int status, int envidx) {
  if (l_likely(status == NEBULA_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      nebula_pushvalue(L, envidx);  /* environment for loaded function */
      if (!nebula_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        nebula_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    nebulaL_pushfail(L);
    nebula_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int nebulaB_loadfile (nebula_State *L) {
  const char *fname = nebulaL_optstring(L, 1, NULL);
  const char *mode = nebulaL_optstring(L, 2, NULL);
  int env = (!nebula_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = nebulaL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' function: 'nebula_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (nebula_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  nebulaL_checkstack(L, 2, "too many nested functions");
  nebula_pushvalue(L, 1);  /* get function */
  nebula_call(L, 0, 1);  /* call it */
  if (nebula_isnil(L, -1)) {
    nebula_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!nebula_isstring(L, -1)))
    nebulaL_error(L, "reader function must return a string");
  nebula_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return nebula_tolstring(L, RESERVEDSLOT, size);
}


static int nebulaB_load (nebula_State *L) {
  int status;
  size_t l;
  const char *s = nebula_tolstring(L, 1, &l);
  const char *mode = nebulaL_optstring(L, 3, "bt");
  int env = (!nebula_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = nebulaL_optstring(L, 2, s);
    status = nebulaL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = nebulaL_optstring(L, 2, "=(load)");
    nebulaL_checktype(L, 1, NEBULA_TFUNCTION);
    nebula_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = nebula_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (nebula_State *L, int d1, nebula_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'nebula_Kfunction' prototype */
  return nebula_gettop(L) - 1;
}


static int nebulaB_dofile (nebula_State *L) {
  const char *fname = nebulaL_optstring(L, 1, NULL);
  nebula_settop(L, 1);
  if (l_unlikely(nebulaL_loadfile(L, fname) != NEBULA_OK))
    return nebula_error(L);
  nebula_callk(L, 0, NEBULA_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int nebulaB_assert (nebula_State *L) {
  if (l_likely(nebula_toboolean(L, 1)))  /* condition is true? */
    return nebula_gettop(L);  /* return all arguments */
  else {  /* error */
    nebulaL_checkany(L, 1);  /* there must be a condition */
    nebula_remove(L, 1);  /* remove it */
    nebula_pushliteral(L, "assertion failed!");  /* default message */
    nebula_settop(L, 1);  /* leave only message (default if no other one) */
    return nebulaB_error(L);  /* call 'error' */
  }
}


static int nebulaB_select (nebula_State *L) {
  int n = nebula_gettop(L);
  if (nebula_type(L, 1) == NEBULA_TSTRING && *nebula_tostring(L, 1) == '#') {
    nebula_pushinteger(L, n-1);
    return 1;
  }
  else {
    nebula_Integer i = nebulaL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    nebulaL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (nebula_State *L, int status, nebula_KContext extra) {
  if (l_unlikely(status != NEBULA_OK && status != NEBULA_YIELD)) {  /* error? */
    nebula_pushboolean(L, 0);  /* first result (false) */
    nebula_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return nebula_gettop(L) - (int)extra;  /* return all results */
}


static int nebulaB_pcall (nebula_State *L) {
  int status;
  nebulaL_checkany(L, 1);
  nebula_pushboolean(L, 1);  /* first result if no errors */
  nebula_insert(L, 1);  /* put it in place */
  status = nebula_pcallk(L, nebula_gettop(L) - 2, NEBULA_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'nebula_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int nebulaB_xpcall (nebula_State *L) {
  int status;
  int n = nebula_gettop(L);
  nebulaL_checktype(L, 2, NEBULA_TFUNCTION);  /* check error function */
  nebula_pushboolean(L, 1);  /* first result */
  nebula_pushvalue(L, 1);  /* function */
  nebula_rotate(L, 3, 2);  /* move them below function's arguments */
  status = nebula_pcallk(L, n - 2, NEBULA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int nebulaB_tostring (nebula_State *L) {
  nebulaL_checkany(L, 1);
  nebulaL_tolstring(L, 1, NULL);
  return 1;
}


static const nebulaL_Reg base_funcs[] = {
  {"assert", nebulaB_assert},
  {"collectgarbage", nebulaB_collectgarbage},
  {"dofile", nebulaB_dofile},
  {"error", nebulaB_error},
  {"getmetatable", nebulaB_getmetatable},
  {"ipairs", nebulaB_ipairs},
  {"loadfile", nebulaB_loadfile},
  {"load", nebulaB_load},
  {"next", nebulaB_next},
  {"pairs", nebulaB_pairs},
  {"pcall", nebulaB_pcall},
  {"print", nebulaB_print}, // print or terminal
  {"warn", nebulaB_warn},
  {"rawequal", nebulaB_rawequal},
  {"rawlen", nebulaB_rawlen},
  {"rawget", nebulaB_rawget},
  {"rawset", nebulaB_rawset},
  {"select", nebulaB_select},
  {"setmetatable", nebulaB_setmetatable},
  {"tonumber", nebulaB_tonumber},
  {"tostring", nebulaB_tostring},
  {"type", nebulaB_type},
  {"xpcall", nebulaB_xpcall},
  /* placeholders */
  {NEBULA_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


NEBULAMOD_API int nebulaopen_base (nebula_State *L) {
  /* open lib into global table */
  nebula_pushglobaltable(L);
  nebulaL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  nebula_pushvalue(L, -1);
  nebula_setfield(L, -2, NEBULA_GNAME);
  /* set global _VERSION */
  nebula_pushliteral(L, NEBULA_VERSION);
  nebula_setfield(L, -2, "_VERSION");
  return 1;
}