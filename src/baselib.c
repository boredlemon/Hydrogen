/*
** $Id: baselib.c $
** Basic library
** See Copyright Notice in hydrogen.h
*/

#define baselib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydrogen.h"

#include "auxlib.h"
#include "hydrogenlib.h"

static int hydrogenB_print (hydrogen_State *L) {
  int n = hydrogen_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = hydrogenL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      hydrogen_writestring("\t", 1);  /* add a tab before it */
    hydrogen_writestring(s, l);  /* print it */
    hydrogen_pop(L, 1);  /* pop result */
  }
  hydrogen_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int hydrogenB_warn (hydrogen_State *L) {
  int n = hydrogen_gettop(L);  /* number of arguments */
  int i;
  hydrogenL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    hydrogenL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    hydrogen_warning(L, hydrogen_tostring(L, i), 1);
  hydrogen_warning(L, hydrogen_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, hydrogen_Integer *pn) {
  hydrogen_Unsigned n = 0;
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
  *pn = (hydrogen_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int hydrogenB_tonumber (hydrogen_State *L) {
  if (hydrogen_isnoneornil(L, 2)) {  /* standard conversion? */
    if (hydrogen_type(L, 1) == HYDROGEN_TNUMBER) {  /* already a number? */
      hydrogen_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = hydrogen_tolstring(L, 1, &l);
      if (s != NULL && hydrogen_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      hydrogenL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    hydrogen_Integer n = 0;  /* to avoid warnings */
    hydrogen_Integer base = hydrogenL_checkinteger(L, 2);
    hydrogenL_checktype(L, 1, HYDROGEN_TSTRING);  /* no numbers as strings */
    s = hydrogen_tolstring(L, 1, &l);
    hydrogenL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      hydrogen_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  hydrogenL_pushfail(L);  /* not a number */
  return 1;
}


static int hydrogenB_error (hydrogen_State *L) {
  int level = (int)hydrogenL_optinteger(L, 2, 1);
  hydrogen_settop(L, 1);
  if (hydrogen_type(L, 1) == HYDROGEN_TSTRING && level > 0) {
    hydrogenL_where(L, level);   /* add extra information */
    hydrogen_pushvalue(L, 1);
    hydrogen_concat(L, 2);
  }
  return hydrogen_error(L);
}


static int hydrogenB_getmetatable (hydrogen_State *L) {
  hydrogenL_checkany(L, 1);
  if (!hydrogen_getmetatable(L, 1)) {
    hydrogen_pushnil(L);
    return 1;  /* no metatable */
  }
  hydrogenL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int hydrogenB_setmetatable (hydrogen_State *L) {
  int t = hydrogen_type(L, 2);
  hydrogenL_checktype(L, 1, HYDROGEN_TTABLE);
  hydrogenL_argexpected(L, t == HYDROGEN_TNIL || t == HYDROGEN_TTABLE, 2, "nil or table");
  if (l_unlikely(hydrogenL_getmetafield(L, 1, "__metatable") != HYDROGEN_TNIL))
    return hydrogenL_error(L, "cannot change a protected metatable");
  hydrogen_settop(L, 2);
  hydrogen_setmetatable(L, 1);
  return 1;
}


static int hydrogenB_rawequal (hydrogen_State *L) {
  hydrogenL_checkany(L, 1);
  hydrogenL_checkany(L, 2);
  hydrogen_pushboolean(L, hydrogen_rawequal(L, 1, 2));
  return 1;
}


static int hydrogenB_rawlen (hydrogen_State *L) {
  int t = hydrogen_type(L, 1);
  hydrogenL_argexpected(L, t == HYDROGEN_TTABLE || t == HYDROGEN_TSTRING, 1,
                      "table or string");
  hydrogen_pushinteger(L, hydrogen_rawlen(L, 1));
  return 1;
}


static int hydrogenB_rawget (hydrogen_State *L) {
  hydrogenL_checktype(L, 1, HYDROGEN_TTABLE);
  hydrogenL_checkany(L, 2);
  hydrogen_settop(L, 2);
  hydrogen_rawget(L, 1);
  return 1;
}

static int hydrogenB_rawset (hydrogen_State *L) {
  hydrogenL_checktype(L, 1, HYDROGEN_TTABLE);
  hydrogenL_checkany(L, 2);
  hydrogenL_checkany(L, 3);
  hydrogen_settop(L, 3);
  hydrogen_rawset(L, 1);
  return 1;
}


static int pushmode (hydrogen_State *L, int oldmode) {
  if (oldmode == -1)
    hydrogenL_pushfail(L);  /* invalid call to 'hydrogen_gc' */
  else
    hydrogen_pushstring(L, (oldmode == HYDROGEN_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'hydrogen_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int hydrogenB_collectgarbage (hydrogen_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {HYDROGEN_GCSTOP, HYDROGEN_GCRESTART, HYDROGEN_GCCOLLECT,
    HYDROGEN_GCCOUNT, HYDROGEN_GCSTEP, HYDROGEN_GCSETPAUSE, HYDROGEN_GCSETSTEPMUL,
    HYDROGEN_GCISRUNNING, HYDROGEN_GCGEN, HYDROGEN_GCINC};
  int o = optsnum[hydrogenL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case HYDROGEN_GCCOUNT: {
      int k = hydrogen_gc(L, o);
      int b = hydrogen_gc(L, HYDROGEN_GCCOUNTB);
      checkvalres(k);
      hydrogen_pushnumber(L, (hydrogen_Number)k + ((hydrogen_Number)b/1024));
      return 1;
    }
    case HYDROGEN_GCSTEP: {
      int step = (int)hydrogenL_optinteger(L, 2, 0);
      int res = hydrogen_gc(L, o, step);
      checkvalres(res);
      hydrogen_pushboolean(L, res);
      return 1;
    }
    case HYDROGEN_GCSETPAUSE:
    case HYDROGEN_GCSETSTEPMUL: {
      int p = (int)hydrogenL_optinteger(L, 2, 0);
      int previous = hydrogen_gc(L, o, p);
      checkvalres(previous);
      hydrogen_pushinteger(L, previous);
      return 1;
    }
    case HYDROGEN_GCISRUNNING: {
      int res = hydrogen_gc(L, o);
      checkvalres(res);
      hydrogen_pushboolean(L, res);
      return 1;
    }
    case HYDROGEN_GCGEN: {
      int minormul = (int)hydrogenL_optinteger(L, 2, 0);
      int majormul = (int)hydrogenL_optinteger(L, 3, 0);
      return pushmode(L, hydrogen_gc(L, o, minormul, majormul));
    }
    case HYDROGEN_GCINC: {
      int pause = (int)hydrogenL_optinteger(L, 2, 0);
      int stepmul = (int)hydrogenL_optinteger(L, 3, 0);
      int stepsize = (int)hydrogenL_optinteger(L, 4, 0);
      return pushmode(L, hydrogen_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = hydrogen_gc(L, o);
      checkvalres(res);
      hydrogen_pushinteger(L, res);
      return 1;
    }
  }
  hydrogenL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int hydrogenB_type (hydrogen_State *L) {
  int t = hydrogen_type(L, 1);
  hydrogenL_argcheck(L, t != HYDROGEN_TNONE, 1, "value expected");
  hydrogen_pushstring(L, hydrogen_typename(L, t));
  return 1;
}


static int hydrogenB_next (hydrogen_State *L) {
  hydrogenL_checktype(L, 1, HYDROGEN_TTABLE);
  hydrogen_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (hydrogen_next(L, 1))
    return 2;
  else {
    hydrogen_pushnil(L);
    return 1;
  }
}


static int pairscont (hydrogen_State *L, int status, hydrogen_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int hydrogenB_pairs (hydrogen_State *L) {
  hydrogenL_checkany(L, 1);
  if (hydrogenL_getmetafield(L, 1, "__pairs") == HYDROGEN_TNIL) {  /* no metamethod? */
    hydrogen_pushcfunction(L, hydrogenB_next);  /* will return generator, */
    hydrogen_pushvalue(L, 1);  /* state, */
    hydrogen_pushnil(L);  /* and initial value */
  }
  else {
    hydrogen_pushvalue(L, 1);  /* argument 'self' to metamethod */
    hydrogen_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (hydrogen_State *L) {
  hydrogen_Integer i = hydrogenL_checkinteger(L, 2);
  i = hydrogenL_intop(+, i, 1);
  hydrogen_pushinteger(L, i);
  return (hydrogen_geti(L, 1, i) == HYDROGEN_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int hydrogenB_ipairs (hydrogen_State *L) {
  hydrogenL_checkany(L, 1);
  hydrogen_pushcfunction(L, ipairsaux);  /* iteration function */
  hydrogen_pushvalue(L, 1);  /* state */
  hydrogen_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (hydrogen_State *L, int status, int envidx) {
  if (l_likely(status == HYDROGEN_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      hydrogen_pushvalue(L, envidx);  /* environment for loaded function */
      if (!hydrogen_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        hydrogen_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    hydrogenL_pushfail(L);
    hydrogen_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int hydrogenB_loadfile (hydrogen_State *L) {
  const char *fname = hydrogenL_optstring(L, 1, NULL);
  const char *mode = hydrogenL_optstring(L, 2, NULL);
  int env = (!hydrogen_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = hydrogenL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' function: 'hydrogen_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (hydrogen_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  hydrogenL_checkstack(L, 2, "too many nested functions");
  hydrogen_pushvalue(L, 1);  /* get function */
  hydrogen_call(L, 0, 1);  /* call it */
  if (hydrogen_isnil(L, -1)) {
    hydrogen_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!hydrogen_isstring(L, -1)))
    hydrogenL_error(L, "reader function must return a string");
  hydrogen_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return hydrogen_tolstring(L, RESERVEDSLOT, size);
}


static int hydrogenB_load (hydrogen_State *L) {
  int status;
  size_t l;
  const char *s = hydrogen_tolstring(L, 1, &l);
  const char *mode = hydrogenL_optstring(L, 3, "bt");
  int env = (!hydrogen_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = hydrogenL_optstring(L, 2, s);
    status = hydrogenL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = hydrogenL_optstring(L, 2, "=(load)");
    hydrogenL_checktype(L, 1, HYDROGEN_TFUNCTION);
    hydrogen_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = hydrogen_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (hydrogen_State *L, int d1, hydrogen_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'hydrogen_Kfunction' prototype */
  return hydrogen_gettop(L) - 1;
}


static int hydrogenB_dofile (hydrogen_State *L) {
  const char *fname = hydrogenL_optstring(L, 1, NULL);
  hydrogen_settop(L, 1);
  if (l_unlikely(hydrogenL_loadfile(L, fname) != HYDROGEN_OK))
    return hydrogen_error(L);
  hydrogen_callk(L, 0, HYDROGEN_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int hydrogenB_assert (hydrogen_State *L) {
  if (l_likely(hydrogen_toboolean(L, 1)))  /* condition is true? */
    return hydrogen_gettop(L);  /* return all arguments */
  else {  /* error */
    hydrogenL_checkany(L, 1);  /* there must be a condition */
    hydrogen_remove(L, 1);  /* remove it */
    hydrogen_pushliteral(L, "assertion failed!");  /* default message */
    hydrogen_settop(L, 1);  /* leave only message (default if no other one) */
    return hydrogenB_error(L);  /* call 'error' */
  }
}


static int hydrogenB_select (hydrogen_State *L) {
  int n = hydrogen_gettop(L);
  if (hydrogen_type(L, 1) == HYDROGEN_TSTRING && *hydrogen_tostring(L, 1) == '#') {
    hydrogen_pushinteger(L, n-1);
    return 1;
  }
  else {
    hydrogen_Integer i = hydrogenL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    hydrogenL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (hydrogen_State *L, int status, hydrogen_KContext extra) {
  if (l_unlikely(status != HYDROGEN_OK && status != HYDROGEN_YIELD)) {  /* error? */
    hydrogen_pushboolean(L, 0);  /* first result (false) */
    hydrogen_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return hydrogen_gettop(L) - (int)extra;  /* return all results */
}


static int hydrogenB_pcall (hydrogen_State *L) {
  int status;
  hydrogenL_checkany(L, 1);
  hydrogen_pushboolean(L, 1);  /* first result if no errors */
  hydrogen_insert(L, 1);  /* put it in place */
  status = hydrogen_pcallk(L, hydrogen_gettop(L) - 2, HYDROGEN_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'hydrogen_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int hydrogenB_xpcall (hydrogen_State *L) {
  int status;
  int n = hydrogen_gettop(L);
  hydrogenL_checktype(L, 2, HYDROGEN_TFUNCTION);  /* check error function */
  hydrogen_pushboolean(L, 1);  /* first result */
  hydrogen_pushvalue(L, 1);  /* function */
  hydrogen_rotate(L, 3, 2);  /* move them below function's arguments */
  status = hydrogen_pcallk(L, n - 2, HYDROGEN_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int hydrogenB_tostring (hydrogen_State *L) {
  hydrogenL_checkany(L, 1);
  hydrogenL_tolstring(L, 1, NULL);
  return 1;
}


static const hydrogenL_Reg base_funcs[] = {
  {"assert", hydrogenB_assert},
  {"collectgarbage", hydrogenB_collectgarbage},
  {"dofile", hydrogenB_dofile},
  {"error", hydrogenB_error},
  {"getmetatable", hydrogenB_getmetatable},
  {"ipairs", hydrogenB_ipairs},
  {"loadfile", hydrogenB_loadfile},
  {"load", hydrogenB_load},
  {"next", hydrogenB_next},
  {"pairs", hydrogenB_pairs},
  {"pcall", hydrogenB_pcall},
  {"print", hydrogenB_print}, // print or terminal
  {"warn", hydrogenB_warn},
  {"rawequal", hydrogenB_rawequal},
  {"rawlen", hydrogenB_rawlen},
  {"rawget", hydrogenB_rawget},
  {"rawset", hydrogenB_rawset},
  {"select", hydrogenB_select},
  {"setmetatable", hydrogenB_setmetatable},
  {"tonumber", hydrogenB_tonumber},
  {"tostring", hydrogenB_tostring},
  {"type", hydrogenB_type},
  {"xpcall", hydrogenB_xpcall},
  /* placeholders */
  {HYDROGEN_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


HYDROGENMOD_API int hydrogenopen_base (hydrogen_State *L) {
  /* open lib into global table */
  hydrogen_pushglobaltable(L);
  hydrogenL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  hydrogen_pushvalue(L, -1);
  hydrogen_setfield(L, -2, HYDROGEN_GNAME);
  /* set global _VERSION */
  hydrogen_pushliteral(L, HYDROGEN_VERSION);
  hydrogen_setfield(L, -2, "_VERSION");
  return 1;
}