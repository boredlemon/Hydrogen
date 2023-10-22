/*
** $Id: lapi.c $
** Viper API
** See Copyright Notice in viper.h
*/

#define lapi_c
#define VIPER_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "viper.h"

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



const char viper_ident[] =
  "$ViperVersion: " VIPER_COPYRIGHT " $"
  "$ViperAuthors: " VIPER_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= VIPER_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < VIPER_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (viper_State *L, int idx) {
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
  else if (idx == VIPER_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = VIPER_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Viper function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (viper_State *L, int idx) {
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


VIPER_API int viper_checkstack (viper_State *L, int n) {
  int res;
  CallInfo *ci;
  viper_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > VIPERI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = viperD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  viper_unlock(L);
  return res;
}


VIPER_API void viper_xmove (viper_State *from, viper_State *to, int n) {
  int i;
  if (from == to) return;
  viper_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  viper_unlock(to);
}


VIPER_API viper_CFunction viper_atpanic (viper_State *L, viper_CFunction panicf) {
  viper_CFunction old;
  viper_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  viper_unlock(L);
  return old;
}


VIPER_API viper_Number viper_version (viper_State *L) {
  UNUSED(L);
  return VIPER_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
VIPER_API int viper_absindex (viper_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


VIPER_API int viper_gettop (viper_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


VIPER_API void viper_settop (viper_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  viper_lock(L);
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
    viper_assert(hastocloseCfunc(ci->nresults));
    viperF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  viper_unlock(L);
}


VIPER_API void viper_closeslot (viper_State *L, int idx) {
  StkId level;
  viper_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  viperF_close(L, level, CLOSEKTOP, 0);
  level = index2stack(L, idx);  /* stack may be moved */
  setnilvalue(s2v(level));
  viper_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'viper_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (viper_State *L, StkId from, StkId to) {
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
VIPER_API void viper_rotate (viper_State *L, int idx, int n) {
  StkId p, t, m;
  viper_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  viper_unlock(L);
}


VIPER_API void viper_copy (viper_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  viper_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    viperC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* VIPER_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  viper_unlock(L);
}


VIPER_API void viper_pushvalue (viper_State *L, int idx) {
  viper_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  viper_unlock(L);
}



/*
** access functions (stack -> C)
*/


VIPER_API int viper_type (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : VIPER_TNONE);
}


VIPER_API const char *viper_typename (viper_State *L, int t) {
  UNUSED(L);
  api_check(L, VIPER_TNONE <= t && t < VIPER_NUMTYPES, "invalid type");
  return ttypename(t);
}


VIPER_API int viper_iscfunction (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


VIPER_API int viper_isinteger (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


VIPER_API int viper_isnumber (viper_State *L, int idx) {
  viper_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


VIPER_API int viper_isstring (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


VIPER_API int viper_isuserdata (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


VIPER_API int viper_rawequal (viper_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? viperV_rawequalobj(o1, o2) : 0;
}


VIPER_API void viper_arith (viper_State *L, int op) {
  viper_lock(L);
  if (op != VIPER_OPUNM && op != VIPER_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result Viper to top - 2 */
  viperO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  viper_unlock(L);
}


VIPER_API int viper_compare (viper_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  viper_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case VIPER_OPEQ: i = viperV_equalobj(L, o1, o2); break;
      case VIPER_OPLT: i = viperV_lessthan(L, o1, o2); break;
      case VIPER_OPLE: i = viperV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  viper_unlock(L);
  return i;
}


VIPER_API size_t viper_stringtonumber (viper_State *L, const char *s) {
  size_t sz = viperO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


VIPER_API viper_Number viper_tonumberx (viper_State *L, int idx, int *pisnum) {
  viper_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


VIPER_API viper_Integer viper_tointegerx (viper_State *L, int idx, int *pisnum) {
  viper_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


VIPER_API int viper_toboolean (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


VIPER_API const char *viper_tolstring (viper_State *L, int idx, size_t *len) {
  TValue *o;
  viper_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      viper_unlock(L);
      return NULL;
    }
    viperO_tostring(L, o);
    viperC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  viper_unlock(L);
  return svalue(o);
}


VIPER_API viper_Unsigned viper_rawlen (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case VIPER_VSHRSTR: return tsvalue(o)->shrlen;
    case VIPER_VLNGSTR: return tsvalue(o)->u.lnglen;
    case VIPER_VUSERDATA: return uvalue(o)->len;
    case VIPER_VTABLE: return viperH_getn(hvalue(o));
    default: return 0;
  }
}


VIPER_API viper_CFunction viper_tocfunction (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case VIPER_TUSERDATA: return getudatamem(uvalue(o));
    case VIPER_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


VIPER_API void *viper_touserdata (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


VIPER_API viper_State *viper_tothread (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here Viperes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
VIPER_API const void *viper_topointer (viper_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case VIPER_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case VIPER_VUSERDATA: case VIPER_VLIGHTUSERDATA:
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


VIPER_API void viper_pushnil (viper_State *L) {
  viper_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  viper_unlock(L);
}


VIPER_API void viper_pushnumber (viper_State *L, viper_Number n) {
  viper_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  viper_unlock(L);
}


VIPER_API void viper_pushinteger (viper_State *L, viper_Integer n) {
  viper_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  viper_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
VIPER_API const char *viper_pushlstring (viper_State *L, const char *s, size_t len) {
  TString *ts;
  viper_lock(L);
  ts = (len == 0) ? viperS_new(L, "") : viperS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  viperC_checkGC(L);
  viper_unlock(L);
  return getstr(ts);
}


VIPER_API const char *viper_pushstring (viper_State *L, const char *s) {
  viper_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = viperS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  viperC_checkGC(L);
  viper_unlock(L);
  return s;
}


VIPER_API const char *viper_pushvfstring (viper_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  viper_lock(L);
  ret = viperO_pushvfstring(L, fmt, argp);
  viperC_checkGC(L);
  viper_unlock(L);
  return ret;
}


VIPER_API const char *viper_pushfstring (viper_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  viper_lock(L);
  va_start(argp, fmt);
  ret = viperO_pushvfstring(L, fmt, argp);
  va_end(argp);
  viperC_checkGC(L);
  viper_unlock(L);
  return ret;
}


VIPER_API void viper_pushcclosure (viper_State *L, viper_CFunction fn, int n) {
  viper_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = viperF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      viper_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    viperC_checkGC(L);
  }
  viper_unlock(L);
}


VIPER_API void viper_pushboolean (viper_State *L, int b) {
  viper_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  viper_unlock(L);
}


VIPER_API void viper_pushlightuserdata (viper_State *L, void *p) {
  viper_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  viper_unlock(L);
}


VIPER_API int viper_pushthread (viper_State *L) {
  viper_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  viper_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Viper -> stack)
*/


l_sinline int auxgetstr (viper_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = viperS_new(L, k);
  if (viperV_fastget(L, t, str, slot, viperH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    viperV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  viper_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[VIPER_RIDX_GLOBALS - 1])


VIPER_API int viper_getglobal (viper_State *L, const char *name) {
  const TValue *G;
  viper_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


VIPER_API int viper_gettable (viper_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  viper_lock(L);
  t = index2value(L, idx);
  if (viperV_fastget(L, t, s2v(L->top - 1), slot, viperH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    viperV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  viper_unlock(L);
  return ttype(s2v(L->top - 1));
}


VIPER_API int viper_getfield (viper_State *L, int idx, const char *k) {
  viper_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


VIPER_API int viper_geti (viper_State *L, int idx, viper_Integer n) {
  TValue *t;
  const TValue *slot;
  viper_lock(L);
  t = index2value(L, idx);
  if (viperV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    viperV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  viper_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (viper_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  viper_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (viper_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


VIPER_API int viper_rawget (viper_State *L, int idx) {
  Table *t;
  const TValue *val;
  viper_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = viperH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


VIPER_API int viper_rawgeti (viper_State *L, int idx, viper_Integer n) {
  Table *t;
  viper_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, viperH_getint(t, n));
}


VIPER_API int viper_rawgetp (viper_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  viper_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, viperH_get(t, &k));
}


VIPER_API void viper_createtable (viper_State *L, int narray, int nrec) {
  Table *t;
  viper_lock(L);
  t = viperH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    viperH_resize(L, t, narray, nrec);
  viperC_checkGC(L);
  viper_unlock(L);
}


VIPER_API int viper_getmetatable (viper_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  viper_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case VIPER_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case VIPER_TUSERDATA:
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
  viper_unlock(L);
  return res;
}


VIPER_API int viper_getiuservalue (viper_State *L, int idx, int n) {
  TValue *o;
  int t;
  viper_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = VIPER_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  viper_unlock(L);
  return t;
}


/*
** set functions (stack -> Viper)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (viper_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = viperS_new(L, k);
  api_checknelems(L, 1);
  if (viperV_fastget(L, t, str, slot, viperH_getstr)) {
    viperV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    viperV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  viper_unlock(L);  /* lock done by caller */
}


VIPER_API void viper_setglobal (viper_State *L, const char *name) {
  const TValue *G;
  viper_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


VIPER_API void viper_settable (viper_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  viper_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (viperV_fastget(L, t, s2v(L->top - 2), slot, viperH_get)) {
    viperV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    viperV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  viper_unlock(L);
}


VIPER_API void viper_setfield (viper_State *L, int idx, const char *k) {
  viper_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


VIPER_API void viper_seti (viper_State *L, int idx, viper_Integer n) {
  TValue *t;
  const TValue *slot;
  viper_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (viperV_fastgeti(L, t, n, slot)) {
    viperV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    viperV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  viper_unlock(L);
}


static void aux_rawset (viper_State *L, int idx, TValue *key, int n) {
  Table *t;
  viper_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  viperH_set(L, t, key, s2v(L->top - 1));
  invalidateTMcache(t);
  viperC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  viper_unlock(L);
}


VIPER_API void viper_rawset (viper_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


VIPER_API void viper_rawsetp (viper_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


VIPER_API void viper_rawseti (viper_State *L, int idx, viper_Integer n) {
  Table *t;
  viper_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  viperH_setint(L, t, n, s2v(L->top - 1));
  viperC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  viper_unlock(L);
}


VIPER_API int viper_setmetatable (viper_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  viper_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case VIPER_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        viperC_objbarrier(L, gcvalue(obj), mt);
        viperC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case VIPER_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        viperC_objbarrier(L, uvalue(obj), mt);
        viperC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  viper_unlock(L);
  return 1;
}


VIPER_API int viper_setiuservalue (viper_State *L, int idx, int n) {
  TValue *o;
  int res;
  viper_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    viperC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  viper_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Viper code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == VIPER_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


VIPER_API void viper_callk (viper_State *L, int nargs, int nresults,
                        viper_KContext ctx, viper_KFunction k) {
  StkId func;
  viper_lock(L);
  api_check(L, k == NULL || !isViper(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == VIPER_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    viperD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    viperD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  viper_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (viper_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  viperD_callnoyield(L, c->func, c->nresults);
}



VIPER_API int viper_pcallk (viper_State *L, int nargs, int nresults, int errfunc,
                        viper_KContext ctx, viper_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  viper_lock(L);
  api_check(L, k == NULL || !isViper(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == VIPER_OK, "cannot do calls on non-normal thread");
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
    status = viperD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    viperD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = VIPER_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  viper_unlock(L);
  return status;
}


VIPER_API int viper_load (viper_State *L, viper_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  viper_lock(L);
  if (!chunkname) chunkname = "?";
  viperZ_init(L, &z, reader, data);
  status = viperD_protectedparser(L, &z, chunkname, mode);
  if (status == VIPER_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be VIPER_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      viperC_barrier(L, f->upvals[0], gt);
    }
  }
  viper_unlock(L);
  return status;
}


VIPER_API int viper_dump (viper_State *L, viper_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  viper_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = viperU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  viper_unlock(L);
  return status;
}


VIPER_API int viper_status (viper_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
VIPER_API int viper_gc (viper_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  viper_lock(L);
  va_start(argp, what);
  switch (what) {
    case VIPER_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case VIPER_GCRESTART: {
      viperE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case VIPER_GCCOLLECT: {
      viperC_fullgc(L, 0);
      break;
    }
    case VIPER_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case VIPER_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case VIPER_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        viperE_setdebt(g, 0);  /* do a basic step */
        viperC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        viperE_setdebt(g, debt);
        viperC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case VIPER_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case VIPER_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case VIPER_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case VIPER_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? VIPER_GCGEN : VIPER_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      viperC_changemode(L, KGC_GEN);
      break;
    }
    case VIPER_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? VIPER_GCGEN : VIPER_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      viperC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  viper_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


VIPER_API int viper_error (viper_State *L) {
  TValue *errobj;
  viper_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    viperM_error(L);  /* raise a memory error */
  else
    viperG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


VIPER_API int viper_next (viper_State *L, int idx) {
  Table *t;
  int more;
  viper_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = viperH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  viper_unlock(L);
  return more;
}


VIPER_API void viper_toclose (viper_State *L, int idx) {
  int nresults;
  StkId o;
  viper_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  viperF_newtbviperval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  viper_assert(hastocloseCfunc(L->ci->nresults));
  viper_unlock(L);
}


VIPER_API void viper_concat (viper_State *L, int n) {
  viper_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    viperV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, viperS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  viperC_checkGC(L);
  viper_unlock(L);
}


VIPER_API void viper_len (viper_State *L, int idx) {
  TValue *t;
  viper_lock(L);
  t = index2value(L, idx);
  viperV_objlen(L, L->top, t);
  api_incr_top(L);
  viper_unlock(L);
}


VIPER_API viper_Alloc viper_getallocf (viper_State *L, void **ud) {
  viper_Alloc f;
  viper_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  viper_unlock(L);
  return f;
}


VIPER_API void viper_setallocf (viper_State *L, viper_Alloc f, void *ud) {
  viper_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  viper_unlock(L);
}


void viper_setwarnf (viper_State *L, viper_WarnFunction f, void *ud) {
  viper_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  viper_unlock(L);
}


void viper_warning (viper_State *L, const char *msg, int tocont) {
  viper_lock(L);
  viperE_warning(L, msg, tocont);
  viper_unlock(L);
}



VIPER_API void *viper_newuserdatauv (viper_State *L, size_t size, int nuvalue) {
  Udata *u;
  viper_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = viperS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  viperC_checkGC(L);
  viper_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case VIPER_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case VIPER_VLCL: {  /* Viper closure */
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


VIPER_API const char *viper_getupvalue (viper_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  viper_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  viper_unlock(L);
  return name;
}


VIPER_API const char *viper_setupvalue (viper_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  viper_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    viperC_barrier(L, owner, val);
  }
  viper_unlock(L);
  return name;
}


static UpVal **getupvalref (viper_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Viper function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


VIPER_API void *viper_upvalueid (viper_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case VIPER_VLCL: {  /* viper closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case VIPER_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case VIPER_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


VIPER_API void viper_upvaluejoin (viper_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  viperC_objbarrier(L, f1, *up1);
}


