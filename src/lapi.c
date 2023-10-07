/*
** $Id: lapi.c $
** Cup API
** See Copyright Notice in cup.h
*/

#define lapi_c
#define CUP_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "cup.h"

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



const char cup_ident[] =
  "$CupVersion: " CUP_COPYRIGHT " $"
  "$CupAuthors: " CUP_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= CUP_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < CUP_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (cup_State *L, int idx) {
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
  else if (idx == CUP_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = CUP_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Cup function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (cup_State *L, int idx) {
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


CUP_API int cup_checkstack (cup_State *L, int n) {
  int res;
  CallInfo *ci;
  cup_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > CUPI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = cupD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  cup_unlock(L);
  return res;
}


CUP_API void cup_xmove (cup_State *from, cup_State *to, int n) {
  int i;
  if (from == to) return;
  cup_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  cup_unlock(to);
}


CUP_API cup_CFunction cup_atpanic (cup_State *L, cup_CFunction panicf) {
  cup_CFunction old;
  cup_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  cup_unlock(L);
  return old;
}


CUP_API cup_Number cup_version (cup_State *L) {
  UNUSED(L);
  return CUP_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
CUP_API int cup_absindex (cup_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


CUP_API int cup_gettop (cup_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


CUP_API void cup_settop (cup_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  cup_lock(L);
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
    cup_assert(hastocloseCfunc(ci->nresults));
    cupF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  cup_unlock(L);
}


CUP_API void cup_closeslot (cup_State *L, int idx) {
  StkId level;
  cup_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  cupF_close(L, level, CLOSEKTOP, 0);
  level = index2stack(L, idx);  /* stack may be moved */
  setnilvalue(s2v(level));
  cup_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'cup_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (cup_State *L, StkId from, StkId to) {
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
CUP_API void cup_rotate (cup_State *L, int idx, int n) {
  StkId p, t, m;
  cup_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  cup_unlock(L);
}


CUP_API void cup_copy (cup_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  cup_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    cupC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* CUP_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  cup_unlock(L);
}


CUP_API void cup_pushvalue (cup_State *L, int idx) {
  cup_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  cup_unlock(L);
}



/*
** access functions (stack -> C)
*/


CUP_API int cup_type (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : CUP_TNONE);
}


CUP_API const char *cup_typename (cup_State *L, int t) {
  UNUSED(L);
  api_check(L, CUP_TNONE <= t && t < CUP_NUMTYPES, "invalid type");
  return ttypename(t);
}


CUP_API int cup_iscfunction (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


CUP_API int cup_isinteger (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


CUP_API int cup_isnumber (cup_State *L, int idx) {
  cup_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


CUP_API int cup_isstring (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


CUP_API int cup_isuserdata (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


CUP_API int cup_rawequal (cup_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? cupV_rawequalobj(o1, o2) : 0;
}


CUP_API void cup_arith (cup_State *L, int op) {
  cup_lock(L);
  if (op != CUP_OPUNM && op != CUP_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result Cup to top - 2 */
  cupO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  cup_unlock(L);
}


CUP_API int cup_compare (cup_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  cup_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case CUP_OPEQ: i = cupV_equalobj(L, o1, o2); break;
      case CUP_OPLT: i = cupV_lessthan(L, o1, o2); break;
      case CUP_OPLE: i = cupV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  cup_unlock(L);
  return i;
}


CUP_API size_t cup_stringtonumber (cup_State *L, const char *s) {
  size_t sz = cupO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


CUP_API cup_Number cup_tonumberx (cup_State *L, int idx, int *pisnum) {
  cup_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


CUP_API cup_Integer cup_tointegerx (cup_State *L, int idx, int *pisnum) {
  cup_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


CUP_API int cup_toboolean (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


CUP_API const char *cup_tolstring (cup_State *L, int idx, size_t *len) {
  TValue *o;
  cup_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      cup_unlock(L);
      return NULL;
    }
    cupO_tostring(L, o);
    cupC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  cup_unlock(L);
  return svalue(o);
}


CUP_API cup_Unsigned cup_rawlen (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case CUP_VSHRSTR: return tsvalue(o)->shrlen;
    case CUP_VLNGSTR: return tsvalue(o)->u.lnglen;
    case CUP_VUSERDATA: return uvalue(o)->len;
    case CUP_VTABLE: return cupH_getn(hvalue(o));
    default: return 0;
  }
}


CUP_API cup_CFunction cup_tocfunction (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case CUP_TUSERDATA: return getudatamem(uvalue(o));
    case CUP_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


CUP_API void *cup_touserdata (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


CUP_API cup_State *cup_tothread (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here Cupes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
CUP_API const void *cup_topointer (cup_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case CUP_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case CUP_VUSERDATA: case CUP_VLIGHTUSERDATA:
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


CUP_API void cup_pushnil (cup_State *L) {
  cup_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  cup_unlock(L);
}


CUP_API void cup_pushnumber (cup_State *L, cup_Number n) {
  cup_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  cup_unlock(L);
}


CUP_API void cup_pushinteger (cup_State *L, cup_Integer n) {
  cup_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  cup_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
CUP_API const char *cup_pushlstring (cup_State *L, const char *s, size_t len) {
  TString *ts;
  cup_lock(L);
  ts = (len == 0) ? cupS_new(L, "") : cupS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  cupC_checkGC(L);
  cup_unlock(L);
  return getstr(ts);
}


CUP_API const char *cup_pushstring (cup_State *L, const char *s) {
  cup_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = cupS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  cupC_checkGC(L);
  cup_unlock(L);
  return s;
}


CUP_API const char *cup_pushvfstring (cup_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  cup_lock(L);
  ret = cupO_pushvfstring(L, fmt, argp);
  cupC_checkGC(L);
  cup_unlock(L);
  return ret;
}


CUP_API const char *cup_pushfstring (cup_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  cup_lock(L);
  va_start(argp, fmt);
  ret = cupO_pushvfstring(L, fmt, argp);
  va_end(argp);
  cupC_checkGC(L);
  cup_unlock(L);
  return ret;
}


CUP_API void cup_pushcclosure (cup_State *L, cup_CFunction fn, int n) {
  cup_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = cupF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      cup_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    cupC_checkGC(L);
  }
  cup_unlock(L);
}


CUP_API void cup_pushboolean (cup_State *L, int b) {
  cup_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  cup_unlock(L);
}


CUP_API void cup_pushlightuserdata (cup_State *L, void *p) {
  cup_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  cup_unlock(L);
}


CUP_API int cup_pushthread (cup_State *L) {
  cup_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  cup_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Cup -> stack)
*/


l_sinline int auxgetstr (cup_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = cupS_new(L, k);
  if (cupV_fastget(L, t, str, slot, cupH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    cupV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  cup_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[CUP_RIDX_GLOBALS - 1])


CUP_API int cup_getglobal (cup_State *L, const char *name) {
  const TValue *G;
  cup_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


CUP_API int cup_gettable (cup_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  cup_lock(L);
  t = index2value(L, idx);
  if (cupV_fastget(L, t, s2v(L->top - 1), slot, cupH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    cupV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  cup_unlock(L);
  return ttype(s2v(L->top - 1));
}


CUP_API int cup_getfield (cup_State *L, int idx, const char *k) {
  cup_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


CUP_API int cup_geti (cup_State *L, int idx, cup_Integer n) {
  TValue *t;
  const TValue *slot;
  cup_lock(L);
  t = index2value(L, idx);
  if (cupV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    cupV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  cup_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (cup_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  cup_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (cup_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


CUP_API int cup_rawget (cup_State *L, int idx) {
  Table *t;
  const TValue *val;
  cup_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = cupH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


CUP_API int cup_rawgeti (cup_State *L, int idx, cup_Integer n) {
  Table *t;
  cup_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, cupH_getint(t, n));
}


CUP_API int cup_rawgetp (cup_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  cup_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, cupH_get(t, &k));
}


CUP_API void cup_createtable (cup_State *L, int narray, int nrec) {
  Table *t;
  cup_lock(L);
  t = cupH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    cupH_resize(L, t, narray, nrec);
  cupC_checkGC(L);
  cup_unlock(L);
}


CUP_API int cup_getmetatable (cup_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  cup_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case CUP_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case CUP_TUSERDATA:
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
  cup_unlock(L);
  return res;
}


CUP_API int cup_getiuservalue (cup_State *L, int idx, int n) {
  TValue *o;
  int t;
  cup_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = CUP_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  cup_unlock(L);
  return t;
}


/*
** set functions (stack -> Cup)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (cup_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = cupS_new(L, k);
  api_checknelems(L, 1);
  if (cupV_fastget(L, t, str, slot, cupH_getstr)) {
    cupV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    cupV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  cup_unlock(L);  /* lock done by caller */
}


CUP_API void cup_setglobal (cup_State *L, const char *name) {
  const TValue *G;
  cup_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


CUP_API void cup_settable (cup_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  cup_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (cupV_fastget(L, t, s2v(L->top - 2), slot, cupH_get)) {
    cupV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    cupV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  cup_unlock(L);
}


CUP_API void cup_setfield (cup_State *L, int idx, const char *k) {
  cup_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


CUP_API void cup_seti (cup_State *L, int idx, cup_Integer n) {
  TValue *t;
  const TValue *slot;
  cup_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (cupV_fastgeti(L, t, n, slot)) {
    cupV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    cupV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  cup_unlock(L);
}


static void aux_rawset (cup_State *L, int idx, TValue *key, int n) {
  Table *t;
  cup_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  cupH_set(L, t, key, s2v(L->top - 1));
  invalidateTMcache(t);
  cupC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  cup_unlock(L);
}


CUP_API void cup_rawset (cup_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


CUP_API void cup_rawsetp (cup_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


CUP_API void cup_rawseti (cup_State *L, int idx, cup_Integer n) {
  Table *t;
  cup_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  cupH_setint(L, t, n, s2v(L->top - 1));
  cupC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  cup_unlock(L);
}


CUP_API int cup_setmetatable (cup_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  cup_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case CUP_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        cupC_objbarrier(L, gcvalue(obj), mt);
        cupC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case CUP_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        cupC_objbarrier(L, uvalue(obj), mt);
        cupC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  cup_unlock(L);
  return 1;
}


CUP_API int cup_setiuservalue (cup_State *L, int idx, int n) {
  TValue *o;
  int res;
  cup_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    cupC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  cup_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Cup code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == CUP_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


CUP_API void cup_callk (cup_State *L, int nargs, int nresults,
                        cup_KContext ctx, cup_KFunction k) {
  StkId func;
  cup_lock(L);
  api_check(L, k == NULL || !isCup(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == CUP_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    cupD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    cupD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  cup_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (cup_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  cupD_callnoyield(L, c->func, c->nresults);
}



CUP_API int cup_pcallk (cup_State *L, int nargs, int nresults, int errfunc,
                        cup_KContext ctx, cup_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  cup_lock(L);
  api_check(L, k == NULL || !isCup(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == CUP_OK, "cannot do calls on non-normal thread");
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
    status = cupD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    cupD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = CUP_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  cup_unlock(L);
  return status;
}


CUP_API int cup_load (cup_State *L, cup_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  cup_lock(L);
  if (!chunkname) chunkname = "?";
  cupZ_init(L, &z, reader, data);
  status = cupD_protectedparser(L, &z, chunkname, mode);
  if (status == CUP_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be CUP_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      cupC_barrier(L, f->upvals[0], gt);
    }
  }
  cup_unlock(L);
  return status;
}


CUP_API int cup_dump (cup_State *L, cup_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  cup_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = cupU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  cup_unlock(L);
  return status;
}


CUP_API int cup_status (cup_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
CUP_API int cup_gc (cup_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  cup_lock(L);
  va_start(argp, what);
  switch (what) {
    case CUP_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case CUP_GCRESTART: {
      cupE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case CUP_GCCOLLECT: {
      cupC_fullgc(L, 0);
      break;
    }
    case CUP_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case CUP_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case CUP_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        cupE_setdebt(g, 0);  /* do a basic step */
        cupC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        cupE_setdebt(g, debt);
        cupC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case CUP_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case CUP_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case CUP_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case CUP_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? CUP_GCGEN : CUP_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      cupC_changemode(L, KGC_GEN);
      break;
    }
    case CUP_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? CUP_GCGEN : CUP_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      cupC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  cup_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


CUP_API int cup_error (cup_State *L) {
  TValue *errobj;
  cup_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    cupM_error(L);  /* raise a memory error */
  else
    cupG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


CUP_API int cup_next (cup_State *L, int idx) {
  Table *t;
  int more;
  cup_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = cupH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  cup_unlock(L);
  return more;
}


CUP_API void cup_toclose (cup_State *L, int idx) {
  int nresults;
  StkId o;
  cup_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  cupF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  cup_assert(hastocloseCfunc(L->ci->nresults));
  cup_unlock(L);
}


CUP_API void cup_concat (cup_State *L, int n) {
  cup_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    cupV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, cupS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  cupC_checkGC(L);
  cup_unlock(L);
}


CUP_API void cup_len (cup_State *L, int idx) {
  TValue *t;
  cup_lock(L);
  t = index2value(L, idx);
  cupV_objlen(L, L->top, t);
  api_incr_top(L);
  cup_unlock(L);
}


CUP_API cup_Alloc cup_getallocf (cup_State *L, void **ud) {
  cup_Alloc f;
  cup_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  cup_unlock(L);
  return f;
}


CUP_API void cup_setallocf (cup_State *L, cup_Alloc f, void *ud) {
  cup_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  cup_unlock(L);
}


void cup_setwarnf (cup_State *L, cup_WarnFunction f, void *ud) {
  cup_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  cup_unlock(L);
}


void cup_warning (cup_State *L, const char *msg, int tocont) {
  cup_lock(L);
  cupE_warning(L, msg, tocont);
  cup_unlock(L);
}



CUP_API void *cup_newuserdatauv (cup_State *L, size_t size, int nuvalue) {
  Udata *u;
  cup_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = cupS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  cupC_checkGC(L);
  cup_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case CUP_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case CUP_VLCL: {  /* Cup closure */
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


CUP_API const char *cup_getupvalue (cup_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  cup_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  cup_unlock(L);
  return name;
}


CUP_API const char *cup_setupvalue (cup_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  cup_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    cupC_barrier(L, owner, val);
  }
  cup_unlock(L);
  return name;
}


static UpVal **getupvalref (cup_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Cup function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


CUP_API void *cup_upvalueid (cup_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case CUP_VLCL: {  /* cup closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case CUP_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case CUP_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


CUP_API void cup_upvaluejoin (cup_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  cupC_objbarrier(L, f1, *up1);
}


