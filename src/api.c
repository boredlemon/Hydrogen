/*
** $Id: lapi.c $
** Nebula API
** See Copyright Notice in nebula.h
*/

#define api_c
#define NEBULA_CORE

#include "prefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "nebula.h"

#include "api.h"
#include "debug.h"
#include "do.h"
#include "function.h"
#include "garbageCollection.h"
#include "memory.h"
#include "object.h"
#include "state.h"
#include "string.h"
#include "table.h"
#include "tagMethods.h"
#include "undump.h"
#include "virtualMachine.h"



const char nebula_ident[] =
  "$NebulaVersion: " NEBULA_COPYRIGHT " $"
  "$NebulaAuthors: " NEBULA_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= NEBULA_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < NEBULA_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (nebula_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, idx <= L->ci->top - (ci->func + 1), "unacceptable index");
    if (o >= L->top) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    return s2v(L->top + idx);
  }
  else if (idx == NEBULA_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = NEBULA_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Nebula function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (nebula_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, o < L->top, "invalid index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top + idx;
  }
}


NEBULA_API int nebula_checkstack (nebula_State *L, int n) {
  int res;
  CallInfo *ci;
  nebula_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > NEBULAI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = nebulaD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  nebula_unlock(L);
  return res;
}


NEBULA_API void nebula_xmove (nebula_State *from, nebula_State *to, int n) {
  int i;
  if (from == to) return;
  nebula_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  nebula_unlock(to);
}


NEBULA_API nebula_CFunction nebula_atpanic (nebula_State *L, nebula_CFunction panicf) {
  nebula_CFunction old;
  nebula_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  nebula_unlock(L);
  return old;
}


NEBULA_API nebula_Number nebula_version (nebula_State *L) {
  UNUSED(L);
  return NEBULA_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
NEBULA_API int nebula_absindex (nebula_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


NEBULA_API int nebula_gettop (nebula_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


NEBULA_API void nebula_settop (nebula_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  nebula_lock(L);
  ci = L->ci;
  func = ci->func;
  if (idx >= 0) {
    api_check(L, idx <= ci->top - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top;
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top++));  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  api_check(L, L->tbclist < L->top, "previous pop of an unclosed slot");
  newtop = L->top + diff;
  if (diff < 0 && L->tbclist >= newtop) {
    nebula_assert(hastocloseCfunc(ci->nresults));
    nebulaF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  nebula_unlock(L);
}


NEBULA_API void nebula_closeslot (nebula_State *L, int idx) {
  StkId level;
  nebula_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  nebulaF_close(L, level, CLOSEKTOP, 0);
  level = index2stack(L, idx);  /* stack may be moved */
  setnilvalue(s2v(level));
  nebula_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'nebula_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (nebula_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
NEBULA_API void nebula_rotate (nebula_State *L, int idx, int n) {
  StkId p, t, m;
  nebula_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  nebula_unlock(L);
}


NEBULA_API void nebula_copy (nebula_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  nebula_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    nebulaC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* NEBULA_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  nebula_unlock(L);
}


NEBULA_API void nebula_pushvalue (nebula_State *L, int idx) {
  nebula_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  nebula_unlock(L);
}



/*
** access functions (stack -> C)
*/


NEBULA_API int nebula_type (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : NEBULA_TNONE);
}


NEBULA_API const char *nebula_typename (nebula_State *L, int t) {
  UNUSED(L);
  api_check(L, NEBULA_TNONE <= t && t < NEBULA_NUMTYPES, "invalid type");
  return ttypename(t);
}


NEBULA_API int nebula_iscfunction (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


NEBULA_API int nebula_isinteger (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


NEBULA_API int nebula_isnumber (nebula_State *L, int idx) {
  nebula_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


NEBULA_API int nebula_isstring (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


NEBULA_API int nebula_isuserdata (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


NEBULA_API int nebula_rawequal (nebula_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? nebulaV_rawequalobj(o1, o2) : 0;
}


NEBULA_API void nebula_arith (nebula_State *L, int op) {
  nebula_lock(L);
  if (op != NEBULA_OPUNM && op != NEBULA_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result Nebula to top - 2 */
  nebulaO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  nebula_unlock(L);
}


NEBULA_API int nebula_compare (nebula_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  nebula_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case NEBULA_OPEQ: i = nebulaV_equalobj(L, o1, o2); break;
      case NEBULA_OPLT: i = nebulaV_lessthan(L, o1, o2); break;
      case NEBULA_OPLE: i = nebulaV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  nebula_unlock(L);
  return i;
}


NEBULA_API size_t nebula_stringtonumber (nebula_State *L, const char *s) {
  size_t sz = nebulaO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


NEBULA_API nebula_Number nebula_tonumberx (nebula_State *L, int idx, int *pisnum) {
  nebula_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


NEBULA_API nebula_Integer nebula_tointegerx (nebula_State *L, int idx, int *pisnum) {
  nebula_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


NEBULA_API int nebula_toboolean (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


NEBULA_API const char *nebula_tolstring (nebula_State *L, int idx, size_t *len) {
  TValue *o;
  nebula_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      nebula_unlock(L);
      return NULL;
    }
    nebulaO_tostring(L, o);
    nebulaC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  nebula_unlock(L);
  return svalue(o);
}


NEBULA_API nebula_Unsigned nebula_rawlen (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case NEBULA_VSHRSTR: return tsvalue(o)->shrlen;
    case NEBULA_VLNGSTR: return tsvalue(o)->u.lnglen;
    case NEBULA_VUSERDATA: return uvalue(o)->len;
    case NEBULA_VTABLE: return nebulaH_getn(hvalue(o));
    default: return 0;
  }
}


NEBULA_API nebula_CFunction nebula_tocfunction (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case NEBULA_TUSERDATA: return getudatamem(uvalue(o));
    case NEBULA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


NEBULA_API void *nebula_touserdata (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


NEBULA_API nebula_State *nebula_tothread (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here Nebulaes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
NEBULA_API const void *nebula_topointer (nebula_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case NEBULA_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case NEBULA_VUSERDATA: case NEBULA_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


NEBULA_API void nebula_pushnil (nebula_State *L) {
  nebula_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  nebula_unlock(L);
}


NEBULA_API void nebula_pushnumber (nebula_State *L, nebula_Number n) {
  nebula_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  nebula_unlock(L);
}


NEBULA_API void nebula_pushinteger (nebula_State *L, nebula_Integer n) {
  nebula_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  nebula_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
NEBULA_API const char *nebula_pushlstring (nebula_State *L, const char *s, size_t len) {
  TString *ts;
  nebula_lock(L);
  ts = (len == 0) ? nebulaS_new(L, "") : nebulaS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  nebulaC_checkGC(L);
  nebula_unlock(L);
  return getstr(ts);
}


NEBULA_API const char *nebula_pushstring (nebula_State *L, const char *s) {
  nebula_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = nebulaS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  nebulaC_checkGC(L);
  nebula_unlock(L);
  return s;
}


NEBULA_API const char *nebula_pushvfstring (nebula_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  nebula_lock(L);
  ret = nebulaO_pushvfstring(L, fmt, argp);
  nebulaC_checkGC(L);
  nebula_unlock(L);
  return ret;
}


NEBULA_API const char *nebula_pushfstring (nebula_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  nebula_lock(L);
  va_start(argp, fmt);
  ret = nebulaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  nebulaC_checkGC(L);
  nebula_unlock(L);
  return ret;
}


NEBULA_API void nebula_pushcclosure (nebula_State *L, nebula_CFunction fn, int n) {
  nebula_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = nebulaF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      nebula_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    nebulaC_checkGC(L);
  }
  nebula_unlock(L);
}


NEBULA_API void nebula_pushboolean (nebula_State *L, int b) {
  nebula_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  nebula_unlock(L);
}


NEBULA_API void nebula_pushlightuserdata (nebula_State *L, void *p) {
  nebula_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  nebula_unlock(L);
}


NEBULA_API int nebula_pushthread (nebula_State *L) {
  nebula_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  nebula_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Nebula -> stack)
*/


l_sinline int auxgetstr (nebula_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = nebulaS_new(L, k);
  if (nebulaV_fastget(L, t, str, slot, nebulaH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    nebulaV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  nebula_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[NEBULA_RIDX_GLOBALS - 1])


NEBULA_API int nebula_getglobal (nebula_State *L, const char *name) {
  const TValue *G;
  nebula_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


NEBULA_API int nebula_gettable (nebula_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  nebula_lock(L);
  t = index2value(L, idx);
  if (nebulaV_fastget(L, t, s2v(L->top - 1), slot, nebulaH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    nebulaV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  nebula_unlock(L);
  return ttype(s2v(L->top - 1));
}


NEBULA_API int nebula_getfield (nebula_State *L, int idx, const char *k) {
  nebula_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


NEBULA_API int nebula_geti (nebula_State *L, int idx, nebula_Integer n) {
  TValue *t;
  const TValue *slot;
  nebula_lock(L);
  t = index2value(L, idx);
  if (nebulaV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    nebulaV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  nebula_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (nebula_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  nebula_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (nebula_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


NEBULA_API int nebula_rawget (nebula_State *L, int idx) {
  Table *t;
  const TValue *val;
  nebula_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = nebulaH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


NEBULA_API int nebula_rawgeti (nebula_State *L, int idx, nebula_Integer n) {
  Table *t;
  nebula_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, nebulaH_getint(t, n));
}


NEBULA_API int nebula_rawgetp (nebula_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  nebula_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, nebulaH_get(t, &k));
}


NEBULA_API void nebula_createtable (nebula_State *L, int narray, int nrec) {
  Table *t;
  nebula_lock(L);
  t = nebulaH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    nebulaH_resize(L, t, narray, nrec);
  nebulaC_checkGC(L);
  nebula_unlock(L);
}


NEBULA_API int nebula_getmetatable (nebula_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  nebula_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case NEBULA_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case NEBULA_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  nebula_unlock(L);
  return res;
}


NEBULA_API int nebula_getiuservalue (nebula_State *L, int idx, int n) {
  TValue *o;
  int t;
  nebula_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = NEBULA_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  nebula_unlock(L);
  return t;
}


/*
** set functions (stack -> Nebula)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (nebula_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = nebulaS_new(L, k);
  api_checknelems(L, 1);
  if (nebulaV_fastget(L, t, str, slot, nebulaH_getstr)) {
    nebulaV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    nebulaV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  nebula_unlock(L);  /* lock done by caller */
}


NEBULA_API void nebula_setglobal (nebula_State *L, const char *name) {
  const TValue *G;
  nebula_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


NEBULA_API void nebula_settable (nebula_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  nebula_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (nebulaV_fastget(L, t, s2v(L->top - 2), slot, nebulaH_get)) {
    nebulaV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    nebulaV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  nebula_unlock(L);
}


NEBULA_API void nebula_setfield (nebula_State *L, int idx, const char *k) {
  nebula_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


NEBULA_API void nebula_seti (nebula_State *L, int idx, nebula_Integer n) {
  TValue *t;
  const TValue *slot;
  nebula_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (nebulaV_fastgeti(L, t, n, slot)) {
    nebulaV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    nebulaV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  nebula_unlock(L);
}


static void aux_rawset (nebula_State *L, int idx, TValue *key, int n) {
  Table *t;
  nebula_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  nebulaH_set(L, t, key, s2v(L->top - 1));
  invalidateTMcache(t);
  nebulaC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  nebula_unlock(L);
}


NEBULA_API void nebula_rawset (nebula_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


NEBULA_API void nebula_rawsetp (nebula_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


NEBULA_API void nebula_rawseti (nebula_State *L, int idx, nebula_Integer n) {
  Table *t;
  nebula_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  nebulaH_setint(L, t, n, s2v(L->top - 1));
  nebulaC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  nebula_unlock(L);
}


NEBULA_API int nebula_setmetatable (nebula_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  nebula_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case NEBULA_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        nebulaC_objbarrier(L, gcvalue(obj), mt);
        nebulaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case NEBULA_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        nebulaC_objbarrier(L, uvalue(obj), mt);
        nebulaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  nebula_unlock(L);
  return 1;
}


NEBULA_API int nebula_setiuservalue (nebula_State *L, int idx, int n) {
  TValue *o;
  int res;
  nebula_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    nebulaC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  nebula_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Nebula code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == NEBULA_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


NEBULA_API void nebula_callk (nebula_State *L, int nargs, int nresults,
                        nebula_KContext ctx, nebula_KFunction k) {
  StkId func;
  nebula_lock(L);
  api_check(L, k == NULL || !isNebula(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == NEBULA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    nebulaD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    nebulaD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  nebula_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (nebula_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  nebulaD_callnoyield(L, c->func, c->nresults);
}



NEBULA_API int nebula_pcallk (nebula_State *L, int nargs, int nresults, int errfunc,
                        nebula_KContext ctx, nebula_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  nebula_lock(L);
  api_check(L, k == NULL || !isNebula(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == NEBULA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o);
  }
  c.func = L->top - (nargs+1);  /* function to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = nebulaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci->callstatus, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    nebulaD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = NEBULA_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  nebula_unlock(L);
  return status;
}


NEBULA_API int nebula_load (nebula_State *L, nebula_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  nebula_lock(L);
  if (!chunkname) chunkname = "?";
  nebulaZ_init(L, &z, reader, data);
  status = nebulaD_protectedparser(L, &z, chunkname, mode);
  if (status == NEBULA_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be NEBULA_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      nebulaC_barrier(L, f->upvals[0], gt);
    }
  }
  nebula_unlock(L);
  return status;
}


NEBULA_API int nebula_dump (nebula_State *L, nebula_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  nebula_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (ttisfunction(o))
    status = nebulaU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  nebula_unlock(L);
  return status;
}


NEBULA_API int nebula_status (nebula_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
NEBULA_API int nebula_gc (nebula_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  nebula_lock(L);
  va_start(argp, what);
  switch (what) {
    case NEBULA_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case NEBULA_GCRESTART: {
      nebulaE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case NEBULA_GCCOLLECT: {
      nebulaC_fulgarbageCollection(L, 0);
      break;
    }
    case NEBULA_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case NEBULA_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case NEBULA_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        nebulaE_setdebt(g, 0);  /* do a basic step */
        nebulaC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        nebulaE_setdebt(g, debt);
        nebulaC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case NEBULA_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case NEBULA_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case NEBULA_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case NEBULA_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? NEBULA_GCGEN : NEBULA_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      nebulaC_changemode(L, KGC_GEN);
      break;
    }
    case NEBULA_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? NEBULA_GCGEN : NEBULA_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      nebulaC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  nebula_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


NEBULA_API int nebula_error (nebula_State *L) {
  TValue *errobj;
  nebula_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    nebulaM_error(L);  /* raise a memory error */
  else
    nebulaG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


NEBULA_API int nebula_next (nebula_State *L, int idx) {
  Table *t;
  int more;
  nebula_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = nebulaH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  nebula_unlock(L);
  return more;
}


NEBULA_API void nebula_toclose (nebula_State *L, int idx) {
  int nresults;
  StkId o;
  nebula_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  nebulaF_newtbnebulaval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  nebula_assert(hastocloseCfunc(L->ci->nresults));
  nebula_unlock(L);
}


NEBULA_API void nebula_concat (nebula_State *L, int n) {
  nebula_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    nebulaV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, nebulaS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  nebulaC_checkGC(L);
  nebula_unlock(L);
}


NEBULA_API void nebula_len (nebula_State *L, int idx) {
  TValue *t;
  nebula_lock(L);
  t = index2value(L, idx);
  nebulaV_objlen(L, L->top, t);
  api_incr_top(L);
  nebula_unlock(L);
}


NEBULA_API nebula_Alloc nebula_getallocf (nebula_State *L, void **ud) {
  nebula_Alloc f;
  nebula_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  nebula_unlock(L);
  return f;
}


NEBULA_API void nebula_setallocf (nebula_State *L, nebula_Alloc f, void *ud) {
  nebula_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  nebula_unlock(L);
}


void nebula_setwarnf (nebula_State *L, nebula_WarnFunction f, void *ud) {
  nebula_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  nebula_unlock(L);
}


void nebula_warning (nebula_State *L, const char *msg, int tocont) {
  nebula_lock(L);
  nebulaE_warning(L, msg, tocont);
  nebula_unlock(L);
}



NEBULA_API void *nebula_newuserdatauv (nebula_State *L, size_t size, int nuvalue) {
  Udata *u;
  nebula_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = nebulaS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  nebulaC_checkGC(L);
  nebula_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case NEBULA_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case NEBULA_VLCL: {  /* Nebula closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


NEBULA_API const char *nebula_getupvalue (nebula_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  nebula_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  nebula_unlock(L);
  return name;
}


NEBULA_API const char *nebula_setupvalue (nebula_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  nebula_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    nebulaC_barrier(L, owner, val);
  }
  nebula_unlock(L);
  return name;
}


static UpVal **getupvalref (nebula_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Nebula function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


NEBULA_API void *nebula_upvalueid (nebula_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case NEBULA_VLCL: {  /* nebula closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case NEBULA_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case NEBULA_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


NEBULA_API void nebula_upvaluejoin (nebula_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  nebulaC_objbarrier(L, f1, *up1);
}

