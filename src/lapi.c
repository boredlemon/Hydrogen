/*
** $Id: lapi.c $
** Acorn API
** See Copyright Notice in acorn.h
*/

#define lapi_c
#define ACORN_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "acorn.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char acorn_ident[] =
  "$AcornVersion: " ACORN_COPYRIGHT " $"
  "$AcornAuthors: " ACORN_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= ACORN_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < ACORN_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (acorn_State *L, int idx) {
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
  else if (idx == ACORN_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = ACORN_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Acorn function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (acorn_State *L, int idx) {
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


ACORN_API int acorn_checkstack (acorn_State *L, int n) {
  int res;
  CallInfo *ci;
  acorn_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > ACORNI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = acornD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  acorn_unlock(L);
  return res;
}


ACORN_API void acorn_xmove (acorn_State *from, acorn_State *to, int n) {
  int i;
  if (from == to) return;
  acorn_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  acorn_unlock(to);
}


ACORN_API acorn_CFunction acorn_atpanic (acorn_State *L, acorn_CFunction panicf) {
  acorn_CFunction old;
  acorn_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  acorn_unlock(L);
  return old;
}


ACORN_API acorn_Number acorn_version (acorn_State *L) {
  UNUSED(L);
  return ACORN_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
ACORN_API int acorn_absindex (acorn_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


ACORN_API int acorn_gettop (acorn_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


ACORN_API void acorn_settop (acorn_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  acorn_lock(L);
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
    acorn_assert(hastocloseCfunc(ci->nresults));
    acornF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  acorn_unlock(L);
}


ACORN_API void acorn_closeslot (acorn_State *L, int idx) {
  StkId level;
  acorn_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  acornF_close(L, level, CLOSEKTOP, 0);
  level = index2stack(L, idx);  /* stack may be moved */
  setnilvalue(s2v(level));
  acorn_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'acorn_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (acorn_State *L, StkId from, StkId to) {
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
ACORN_API void acorn_rotate (acorn_State *L, int idx, int n) {
  StkId p, t, m;
  acorn_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  acorn_unlock(L);
}


ACORN_API void acorn_copy (acorn_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  acorn_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    acornC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* ACORN_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  acorn_unlock(L);
}


ACORN_API void acorn_pushvalue (acorn_State *L, int idx) {
  acorn_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  acorn_unlock(L);
}



/*
** access functions (stack -> C)
*/


ACORN_API int acorn_type (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : ACORN_TNONE);
}


ACORN_API const char *acorn_typename (acorn_State *L, int t) {
  UNUSED(L);
  api_check(L, ACORN_TNONE <= t && t < ACORN_NUMTYPES, "invalid type");
  return ttypename(t);
}


ACORN_API int acorn_iscfunction (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


ACORN_API int acorn_isinteger (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


ACORN_API int acorn_isnumber (acorn_State *L, int idx) {
  acorn_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


ACORN_API int acorn_isstring (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


ACORN_API int acorn_isuserdata (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


ACORN_API int acorn_rawequal (acorn_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? acornV_rawequalobj(o1, o2) : 0;
}


ACORN_API void acorn_arith (acorn_State *L, int op) {
  acorn_lock(L);
  if (op != ACORN_OPUNM && op != ACORN_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result Acorn to top - 2 */
  acornO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  acorn_unlock(L);
}


ACORN_API int acorn_compare (acorn_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  acorn_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case ACORN_OPEQ: i = acornV_equalobj(L, o1, o2); break;
      case ACORN_OPLT: i = acornV_lessthan(L, o1, o2); break;
      case ACORN_OPLE: i = acornV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  acorn_unlock(L);
  return i;
}


ACORN_API size_t acorn_stringtonumber (acorn_State *L, const char *s) {
  size_t sz = acornO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


ACORN_API acorn_Number acorn_tonumberx (acorn_State *L, int idx, int *pisnum) {
  acorn_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


ACORN_API acorn_Integer acorn_tointegerx (acorn_State *L, int idx, int *pisnum) {
  acorn_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


ACORN_API int acorn_toboolean (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


ACORN_API const char *acorn_tolstring (acorn_State *L, int idx, size_t *len) {
  TValue *o;
  acorn_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      acorn_unlock(L);
      return NULL;
    }
    acornO_tostring(L, o);
    acornC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  acorn_unlock(L);
  return svalue(o);
}


ACORN_API acorn_Unsigned acorn_rawlen (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case ACORN_VSHRSTR: return tsvalue(o)->shrlen;
    case ACORN_VLNGSTR: return tsvalue(o)->u.lnglen;
    case ACORN_VUSERDATA: return uvalue(o)->len;
    case ACORN_VTABLE: return acornH_getn(hvalue(o));
    default: return 0;
  }
}


ACORN_API acorn_CFunction acorn_tocfunction (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case ACORN_TUSERDATA: return getudatamem(uvalue(o));
    case ACORN_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


ACORN_API void *acorn_touserdata (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


ACORN_API acorn_State *acorn_tothread (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here Acornes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
ACORN_API const void *acorn_topointer (acorn_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case ACORN_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case ACORN_VUSERDATA: case ACORN_VLIGHTUSERDATA:
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


ACORN_API void acorn_pushnil (acorn_State *L) {
  acorn_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  acorn_unlock(L);
}


ACORN_API void acorn_pushnumber (acorn_State *L, acorn_Number n) {
  acorn_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  acorn_unlock(L);
}


ACORN_API void acorn_pushinteger (acorn_State *L, acorn_Integer n) {
  acorn_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  acorn_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
ACORN_API const char *acorn_pushlstring (acorn_State *L, const char *s, size_t len) {
  TString *ts;
  acorn_lock(L);
  ts = (len == 0) ? acornS_new(L, "") : acornS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  acornC_checkGC(L);
  acorn_unlock(L);
  return getstr(ts);
}


ACORN_API const char *acorn_pushstring (acorn_State *L, const char *s) {
  acorn_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = acornS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  acornC_checkGC(L);
  acorn_unlock(L);
  return s;
}


ACORN_API const char *acorn_pushvfstring (acorn_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  acorn_lock(L);
  ret = acornO_pushvfstring(L, fmt, argp);
  acornC_checkGC(L);
  acorn_unlock(L);
  return ret;
}


ACORN_API const char *acorn_pushfstring (acorn_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  acorn_lock(L);
  va_start(argp, fmt);
  ret = acornO_pushvfstring(L, fmt, argp);
  va_end(argp);
  acornC_checkGC(L);
  acorn_unlock(L);
  return ret;
}


ACORN_API void acorn_pushcclosure (acorn_State *L, acorn_CFunction fn, int n) {
  acorn_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = acornF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      acorn_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    acornC_checkGC(L);
  }
  acorn_unlock(L);
}


ACORN_API void acorn_pushboolean (acorn_State *L, int b) {
  acorn_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  acorn_unlock(L);
}


ACORN_API void acorn_pushlightuserdata (acorn_State *L, void *p) {
  acorn_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  acorn_unlock(L);
}


ACORN_API int acorn_pushthread (acorn_State *L) {
  acorn_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  acorn_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Acorn -> stack)
*/


l_sinline int auxgetstr (acorn_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = acornS_new(L, k);
  if (acornV_fastget(L, t, str, slot, acornH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    acornV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  acorn_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[ACORN_RIDX_GLOBALS - 1])


ACORN_API int acorn_getglobal (acorn_State *L, const char *name) {
  const TValue *G;
  acorn_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


ACORN_API int acorn_gettable (acorn_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  acorn_lock(L);
  t = index2value(L, idx);
  if (acornV_fastget(L, t, s2v(L->top - 1), slot, acornH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    acornV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  acorn_unlock(L);
  return ttype(s2v(L->top - 1));
}


ACORN_API int acorn_getfield (acorn_State *L, int idx, const char *k) {
  acorn_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


ACORN_API int acorn_geti (acorn_State *L, int idx, acorn_Integer n) {
  TValue *t;
  const TValue *slot;
  acorn_lock(L);
  t = index2value(L, idx);
  if (acornV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    acornV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  acorn_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (acorn_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  acorn_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (acorn_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


ACORN_API int acorn_rawget (acorn_State *L, int idx) {
  Table *t;
  const TValue *val;
  acorn_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = acornH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


ACORN_API int acorn_rawgeti (acorn_State *L, int idx, acorn_Integer n) {
  Table *t;
  acorn_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, acornH_getint(t, n));
}


ACORN_API int acorn_rawgetp (acorn_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  acorn_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, acornH_get(t, &k));
}


ACORN_API void acorn_createtable (acorn_State *L, int narray, int nrec) {
  Table *t;
  acorn_lock(L);
  t = acornH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    acornH_resize(L, t, narray, nrec);
  acornC_checkGC(L);
  acorn_unlock(L);
}


ACORN_API int acorn_getmetatable (acorn_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  acorn_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case ACORN_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case ACORN_TUSERDATA:
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
  acorn_unlock(L);
  return res;
}


ACORN_API int acorn_getiuservalue (acorn_State *L, int idx, int n) {
  TValue *o;
  int t;
  acorn_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = ACORN_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  acorn_unlock(L);
  return t;
}


/*
** set functions (stack -> Acorn)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (acorn_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = acornS_new(L, k);
  api_checknelems(L, 1);
  if (acornV_fastget(L, t, str, slot, acornH_getstr)) {
    acornV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    acornV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  acorn_unlock(L);  /* lock done by caller */
}


ACORN_API void acorn_setglobal (acorn_State *L, const char *name) {
  const TValue *G;
  acorn_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


ACORN_API void acorn_settable (acorn_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  acorn_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (acornV_fastget(L, t, s2v(L->top - 2), slot, acornH_get)) {
    acornV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    acornV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  acorn_unlock(L);
}


ACORN_API void acorn_setfield (acorn_State *L, int idx, const char *k) {
  acorn_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


ACORN_API void acorn_seti (acorn_State *L, int idx, acorn_Integer n) {
  TValue *t;
  const TValue *slot;
  acorn_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (acornV_fastgeti(L, t, n, slot)) {
    acornV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    acornV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  acorn_unlock(L);
}


static void aux_rawset (acorn_State *L, int idx, TValue *key, int n) {
  Table *t;
  acorn_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  acornH_set(L, t, key, s2v(L->top - 1));
  invalidateTMcache(t);
  acornC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  acorn_unlock(L);
}


ACORN_API void acorn_rawset (acorn_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


ACORN_API void acorn_rawsetp (acorn_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


ACORN_API void acorn_rawseti (acorn_State *L, int idx, acorn_Integer n) {
  Table *t;
  acorn_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  acornH_setint(L, t, n, s2v(L->top - 1));
  acornC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  acorn_unlock(L);
}


ACORN_API int acorn_setmetatable (acorn_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  acorn_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case ACORN_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        acornC_objbarrier(L, gcvalue(obj), mt);
        acornC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case ACORN_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        acornC_objbarrier(L, uvalue(obj), mt);
        acornC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  acorn_unlock(L);
  return 1;
}


ACORN_API int acorn_setiuservalue (acorn_State *L, int idx, int n) {
  TValue *o;
  int res;
  acorn_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    acornC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  acorn_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Acorn code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == ACORN_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


ACORN_API void acorn_callk (acorn_State *L, int nargs, int nresults,
                        acorn_KContext ctx, acorn_KFunction k) {
  StkId func;
  acorn_lock(L);
  api_check(L, k == NULL || !isAcorn(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == ACORN_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    acornD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    acornD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  acorn_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (acorn_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  acornD_callnoyield(L, c->func, c->nresults);
}



ACORN_API int acorn_pcallk (acorn_State *L, int nargs, int nresults, int errfunc,
                        acorn_KContext ctx, acorn_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  acorn_lock(L);
  api_check(L, k == NULL || !isAcorn(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == ACORN_OK, "cannot do calls on non-normal thread");
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
    status = acornD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    acornD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = ACORN_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  acorn_unlock(L);
  return status;
}


ACORN_API int acorn_load (acorn_State *L, acorn_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  acorn_lock(L);
  if (!chunkname) chunkname = "?";
  acornZ_init(L, &z, reader, data);
  status = acornD_protectedparser(L, &z, chunkname, mode);
  if (status == ACORN_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be ACORN_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      acornC_barrier(L, f->upvals[0], gt);
    }
  }
  acorn_unlock(L);
  return status;
}


ACORN_API int acorn_dump (acorn_State *L, acorn_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  acorn_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = acornU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  acorn_unlock(L);
  return status;
}


ACORN_API int acorn_status (acorn_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
ACORN_API int acorn_gc (acorn_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  acorn_lock(L);
  va_start(argp, what);
  switch (what) {
    case ACORN_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case ACORN_GCRESTART: {
      acornE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case ACORN_GCCOLLECT: {
      acornC_fullgc(L, 0);
      break;
    }
    case ACORN_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case ACORN_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case ACORN_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        acornE_setdebt(g, 0);  /* do a basic step */
        acornC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        acornE_setdebt(g, debt);
        acornC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case ACORN_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case ACORN_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case ACORN_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case ACORN_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? ACORN_GCGEN : ACORN_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      acornC_changemode(L, KGC_GEN);
      break;
    }
    case ACORN_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? ACORN_GCGEN : ACORN_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      acornC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  acorn_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


ACORN_API int acorn_error (acorn_State *L) {
  TValue *errobj;
  acorn_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    acornM_error(L);  /* raise a memory error */
  else
    acornG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


ACORN_API int acorn_next (acorn_State *L, int idx) {
  Table *t;
  int more;
  acorn_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = acornH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  acorn_unlock(L);
  return more;
}


ACORN_API void acorn_toclose (acorn_State *L, int idx) {
  int nresults;
  StkId o;
  acorn_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  acornF_newtbacornval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  acorn_assert(hastocloseCfunc(L->ci->nresults));
  acorn_unlock(L);
}


ACORN_API void acorn_concat (acorn_State *L, int n) {
  acorn_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    acornV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, acornS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  acornC_checkGC(L);
  acorn_unlock(L);
}


ACORN_API void acorn_len (acorn_State *L, int idx) {
  TValue *t;
  acorn_lock(L);
  t = index2value(L, idx);
  acornV_objlen(L, L->top, t);
  api_incr_top(L);
  acorn_unlock(L);
}


ACORN_API acorn_Alloc acorn_getallocf (acorn_State *L, void **ud) {
  acorn_Alloc f;
  acorn_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  acorn_unlock(L);
  return f;
}


ACORN_API void acorn_setallocf (acorn_State *L, acorn_Alloc f, void *ud) {
  acorn_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  acorn_unlock(L);
}


void acorn_setwarnf (acorn_State *L, acorn_WarnFunction f, void *ud) {
  acorn_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  acorn_unlock(L);
}


void acorn_warning (acorn_State *L, const char *msg, int tocont) {
  acorn_lock(L);
  acornE_warning(L, msg, tocont);
  acorn_unlock(L);
}



ACORN_API void *acorn_newuserdatauv (acorn_State *L, size_t size, int nuvalue) {
  Udata *u;
  acorn_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = acornS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  acornC_checkGC(L);
  acorn_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case ACORN_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case ACORN_VLCL: {  /* Acorn closure */
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


ACORN_API const char *acorn_getupvalue (acorn_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  acorn_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  acorn_unlock(L);
  return name;
}


ACORN_API const char *acorn_setupvalue (acorn_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  acorn_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    acornC_barrier(L, owner, val);
  }
  acorn_unlock(L);
  return name;
}


static UpVal **getupvalref (acorn_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Acorn function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


ACORN_API void *acorn_upvalueid (acorn_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case ACORN_VLCL: {  /* acorn closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case ACORN_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case ACORN_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


ACORN_API void acorn_upvaluejoin (acorn_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  acornC_objbarrier(L, f1, *up1);
}


