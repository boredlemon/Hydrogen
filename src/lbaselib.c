/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in acorn.h
*/

#define lbaselib_c
#define ACORN_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acorn.h"

#include "lauxlib.h"
#include "acornlib.h"


static int acornB_console (acorn_State *L) {
  int n = acorn_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = acornL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      acorn_writestring("\t", 1);  /* add a tab before it */
    acorn_writestring(s, l);  /* print it */
    acorn_pop(L, 1);  /* pop result */
  }
  acorn_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int acornB_warn (acorn_State *L) {
  int n = acorn_gettop(L);  /* number of arguments */
  int i;
  acornL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    acornL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    acorn_warning(L, acorn_tostring(L, i), 1);
  acorn_warning(L, acorn_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, acorn_Integer *pn) {
  acorn_Unsigned n = 0;
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
  *pn = (acorn_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int acornB_tonumber (acorn_State *L) {
  if (acorn_isnoneornil(L, 2)) {  /* standard conversion? */
    if (acorn_type(L, 1) == ACORN_TNUMBER) {  /* already a number? */
      acorn_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = acorn_tolstring(L, 1, &l);
      if (s != NULL && acorn_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      acornL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    acorn_Integer n = 0;  /* to avoid warnings */
    acorn_Integer base = acornL_checkinteger(L, 2);
    acornL_checktype(L, 1, ACORN_TSTRING);  /* no numbers as strings */
    s = acorn_tolstring(L, 1, &l);
    acornL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      acorn_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  acornL_pushfail(L);  /* not a number */
  return 1;
}


static int acornB_error (acorn_State *L) {
  int level = (int)acornL_optinteger(L, 2, 1);
  acorn_settop(L, 1);
  if (acorn_type(L, 1) == ACORN_TSTRING && level > 0) {
    acornL_where(L, level);   /* add extra information */
    acorn_pushvalue(L, 1);
    acorn_concat(L, 2);
  }
  return acorn_error(L);
}


static int acornB_getmetatable (acorn_State *L) {
  acornL_checkany(L, 1);
  if (!acorn_getmetatable(L, 1)) {
    acorn_pushnil(L);
    return 1;  /* no metatable */
  }
  acornL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int acornB_setmetatable (acorn_State *L) {
  int t = acorn_type(L, 2);
  acornL_checktype(L, 1, ACORN_TTABLE);
  acornL_argexpected(L, t == ACORN_TNIL || t == ACORN_TTABLE, 2, "nil or table");
  if (l_unlikely(acornL_getmetafield(L, 1, "__metatable") != ACORN_TNIL))
    return acornL_error(L, "cannot change a protected metatable");
  acorn_settop(L, 2);
  acorn_setmetatable(L, 1);
  return 1;
}


static int acornB_rawequal (acorn_State *L) {
  acornL_checkany(L, 1);
  acornL_checkany(L, 2);
  acorn_pushboolean(L, acorn_rawequal(L, 1, 2));
  return 1;
}


static int acornB_rawlen (acorn_State *L) {
  int t = acorn_type(L, 1);
  acornL_argexpected(L, t == ACORN_TTABLE || t == ACORN_TSTRING, 1,
                      "table or string");
  acorn_pushinteger(L, acorn_rawlen(L, 1));
  return 1;
}


static int acornB_rawget (acorn_State *L) {
  acornL_checktype(L, 1, ACORN_TTABLE);
  acornL_checkany(L, 2);
  acorn_settop(L, 2);
  acorn_rawget(L, 1);
  return 1;
}

static int acornB_rawset (acorn_State *L) {
  acornL_checktype(L, 1, ACORN_TTABLE);
  acornL_checkany(L, 2);
  acornL_checkany(L, 3);
  acorn_settop(L, 3);
  acorn_rawset(L, 1);
  return 1;
}


static int pushmode (acorn_State *L, int oldmode) {
  if (oldmode == -1)
    acornL_pushfail(L);  /* invalid call to 'acorn_gc' */
  else
    acorn_pushstring(L, (oldmode == ACORN_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'acorn_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int acornB_collectgarbage (acorn_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {ACORN_GCSTOP, ACORN_GCRESTART, ACORN_GCCOLLECT,
    ACORN_GCCOUNT, ACORN_GCSTEP, ACORN_GCSETPAUSE, ACORN_GCSETSTEPMUL,
    ACORN_GCISRUNNING, ACORN_GCGEN, ACORN_GCINC};
  int o = optsnum[acornL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case ACORN_GCCOUNT: {
      int k = acorn_gc(L, o);
      int b = acorn_gc(L, ACORN_GCCOUNTB);
      checkvalres(k);
      acorn_pushnumber(L, (acorn_Number)k + ((acorn_Number)b/1024));
      return 1;
    }
    case ACORN_GCSTEP: {
      int step = (int)acornL_optinteger(L, 2, 0);
      int res = acorn_gc(L, o, step);
      checkvalres(res);
      acorn_pushboolean(L, res);
      return 1;
    }
    case ACORN_GCSETPAUSE:
    case ACORN_GCSETSTEPMUL: {
      int p = (int)acornL_optinteger(L, 2, 0);
      int previous = acorn_gc(L, o, p);
      checkvalres(previous);
      acorn_pushinteger(L, previous);
      return 1;
    }
    case ACORN_GCISRUNNING: {
      int res = acorn_gc(L, o);
      checkvalres(res);
      acorn_pushboolean(L, res);
      return 1;
    }
    case ACORN_GCGEN: {
      int minormul = (int)acornL_optinteger(L, 2, 0);
      int majormul = (int)acornL_optinteger(L, 3, 0);
      return pushmode(L, acorn_gc(L, o, minormul, majormul));
    }
    case ACORN_GCINC: {
      int pause = (int)acornL_optinteger(L, 2, 0);
      int stepmul = (int)acornL_optinteger(L, 3, 0);
      int stepsize = (int)acornL_optinteger(L, 4, 0);
      return pushmode(L, acorn_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = acorn_gc(L, o);
      checkvalres(res);
      acorn_pushinteger(L, res);
      return 1;
    }
  }
  acornL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int acornB_type (acorn_State *L) {
  int t = acorn_type(L, 1);
  acornL_argcheck(L, t != ACORN_TNONE, 1, "value expected");
  acorn_pushstring(L, acorn_typename(L, t));
  return 1;
}


static int acornB_next (acorn_State *L) {
  acornL_checktype(L, 1, ACORN_TTABLE);
  acorn_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (acorn_next(L, 1))
    return 2;
  else {
    acorn_pushnil(L);
    return 1;
  }
}


static int pairscont (acorn_State *L, int status, acorn_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int acornB_pairs (acorn_State *L) {
  acornL_checkany(L, 1);
  if (acornL_getmetafield(L, 1, "__pairs") == ACORN_TNIL) {  /* no metamethod? */
    acorn_pushcfunction(L, acornB_next);  /* will return generator, */
    acorn_pushvalue(L, 1);  /* state, */
    acorn_pushnil(L);  /* and initial value */
  }
  else {
    acorn_pushvalue(L, 1);  /* argument 'self' to metamethod */
    acorn_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (acorn_State *L) {
  acorn_Integer i = acornL_checkinteger(L, 2);
  i = acornL_intop(+, i, 1);
  acorn_pushinteger(L, i);
  return (acorn_geti(L, 1, i) == ACORN_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int acornB_ipairs (acorn_State *L) {
  acornL_checkany(L, 1);
  acorn_pushcfunction(L, ipairsaux);  /* iteration function */
  acorn_pushvalue(L, 1);  /* state */
  acorn_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (acorn_State *L, int status, int envidx) {
  if (l_likely(status == ACORN_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      acorn_pushvalue(L, envidx);  /* environment for loaded function */
      if (!acorn_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        acorn_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    acornL_pushfail(L);
    acorn_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int acornB_loadfile (acorn_State *L) {
  const char *fname = acornL_optstring(L, 1, NULL);
  const char *mode = acornL_optstring(L, 2, NULL);
  int env = (!acorn_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = acornL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' function: 'acorn_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (acorn_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  acornL_checkstack(L, 2, "too many nested functions");
  acorn_pushvalue(L, 1);  /* get function */
  acorn_call(L, 0, 1);  /* call it */
  if (acorn_isnil(L, -1)) {
    acorn_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!acorn_isstring(L, -1)))
    acornL_error(L, "reader function must return a string");
  acorn_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return acorn_tolstring(L, RESERVEDSLOT, size);
}


static int acornB_load (acorn_State *L) {
  int status;
  size_t l;
  const char *s = acorn_tolstring(L, 1, &l);
  const char *mode = acornL_optstring(L, 3, "bt");
  int env = (!acorn_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = acornL_optstring(L, 2, s);
    status = acornL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = acornL_optstring(L, 2, "=(load)");
    acornL_checktype(L, 1, ACORN_TFUNCTION);
    acorn_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = acorn_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (acorn_State *L, int d1, acorn_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'acorn_Kfunction' prototype */
  return acorn_gettop(L) - 1;
}


static int acornB_dofile (acorn_State *L) {
  const char *fname = acornL_optstring(L, 1, NULL);
  acorn_settop(L, 1);
  if (l_unlikely(acornL_loadfile(L, fname) != ACORN_OK))
    return acorn_error(L);
  acorn_callk(L, 0, ACORN_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int acornB_assert (acorn_State *L) {
  if (l_likely(acorn_toboolean(L, 1)))  /* condition is true? */
    return acorn_gettop(L);  /* return all arguments */
  else {  /* error */
    acornL_checkany(L, 1);  /* there must be a condition */
    acorn_remove(L, 1);  /* remove it */
    acorn_pushliteral(L, "assertion failed!");  /* default message */
    acorn_settop(L, 1);  /* leave only message (default if no other one) */
    return acornB_error(L);  /* call 'error' */
  }
}


static int acornB_select (acorn_State *L) {
  int n = acorn_gettop(L);
  if (acorn_type(L, 1) == ACORN_TSTRING && *acorn_tostring(L, 1) == '#') {
    acorn_pushinteger(L, n-1);
    return 1;
  }
  else {
    acorn_Integer i = acornL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    acornL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (acorn_State *L, int status, acorn_KContext extra) {
  if (l_unlikely(status != ACORN_OK && status != ACORN_YIELD)) {  /* error? */
    acorn_pushboolean(L, 0);  /* first result (false) */
    acorn_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return acorn_gettop(L) - (int)extra;  /* return all results */
}


static int acornB_pcall (acorn_State *L) {
  int status;
  acornL_checkany(L, 1);
  acorn_pushboolean(L, 1);  /* first result if no errors */
  acorn_insert(L, 1);  /* put it in place */
  status = acorn_pcallk(L, acorn_gettop(L) - 2, ACORN_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'acorn_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int acornB_xpcall (acorn_State *L) {
  int status;
  int n = acorn_gettop(L);
  acornL_checktype(L, 2, ACORN_TFUNCTION);  /* check error function */
  acorn_pushboolean(L, 1);  /* first result */
  acorn_pushvalue(L, 1);  /* function */
  acorn_rotate(L, 3, 2);  /* move them below function's arguments */
  status = acorn_pcallk(L, n - 2, ACORN_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int acornB_tostring (acorn_State *L) {
  acornL_checkany(L, 1);
  acornL_tolstring(L, 1, NULL);
  return 1;
}


static const acornL_Reg base_funcs[] = {
  {"assert", acornB_assert},
  {"collectgarbage", acornB_collectgarbage},
  {"dofile", acornB_dofile},
  {"error", acornB_error},
  {"getmetatable", acornB_getmetatable},
  {"ipairs", acornB_ipairs},
  {"loadfile", acornB_loadfile},
  {"load", acornB_load},
  {"next", acornB_next},
  {"pairs", acornB_pairs},
  {"pcall", acornB_pcall},
  {"console", acornB_console}, // print or terminal
  {"warn", acornB_warn},
  {"rawequal", acornB_rawequal},
  {"rawlen", acornB_rawlen},
  {"rawget", acornB_rawget},
  {"rawset", acornB_rawset},
  {"select", acornB_select},
  {"setmetatable", acornB_setmetatable},
  {"tonumber", acornB_tonumber},
  {"tostring", acornB_tostring},
  {"type", acornB_type},
  {"xpcall", acornB_xpcall},
  /* placeholders */
  {ACORN_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


ACORNMOD_API int acornopen_base (acorn_State *L) {
  /* open lib into global table */
  acorn_pushglobaltable(L);
  acornL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  acorn_pushvalue(L, -1);
  acorn_setfield(L, -2, ACORN_GNAME);
  /* set global _VERSION */
  acorn_pushliteral(L, ACORN_VERSION);
  acorn_setfield(L, -2, "_VERSION");
  return 1;
}

