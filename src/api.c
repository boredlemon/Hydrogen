/*
** $Id: lapi.c $
** Venom API
** See Copyright Notice in venom.h
*/

#define api_c
#define VENOM_CORE

#include "prefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "venom.h"

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



const char venom_ident[] =
  "$VenomVersion: " VENOM_COPYRIGHT " $"
  "$VenomAuthors: " VENOM_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= VENOM_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < VENOM_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (venom_State *L, int idx) {
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
  else if (idx == VENOM_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = VENOM_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Venom function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (venom_State *L, int idx) {
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


VENOM_API int venom_checkstack (venom_State *L, int n) {
  int res;
  CallInfo *ci;
  venom_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > VENOMI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = venomD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  venom_unlock(L);
  return res;
}


VENOM_API void venom_xmove (venom_State *from, venom_State *to, int n) {
  int i;
  if (from == to) return;
  venom_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  venom_unlock(to);
}


VENOM_API venom_CFunction venom_atpanic (venom_State *L, venom_CFunction panicf) {
  venom_CFunction old;
  venom_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  venom_unlock(L);
  return old;
}


VENOM_API venom_Number venom_version (venom_State *L) {
  UNUSED(L);
  return VENOM_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
VENOM_API int venom_absindex (venom_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


VENOM_API int venom_gettop (venom_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


VENOM_API void venom_settop (venom_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  venom_lock(L);
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
    venom_assert(hastocloseCfunc(ci->nresults));
    venomF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  venom_unlock(L);
}


VENOM_API void venom_closeslot (venom_State *L, int idx) {
  StkId level;
  venom_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  venomF_close(L, level, CLOSEKTOP, 0);
  level = index2stack(L, idx);  /* stack may be moved */
  setnilvalue(s2v(level));
  venom_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'venom_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (venom_State *L, StkId from, StkId to) {
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
VENOM_API void venom_rotate (venom_State *L, int idx, int n) {
  StkId p, t, m;
  venom_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  venom_unlock(L);
}


VENOM_API void venom_copy (venom_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  venom_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    venomC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* VENOM_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  venom_unlock(L);
}


VENOM_API void venom_pushvalue (venom_State *L, int idx) {
  venom_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  venom_unlock(L);
}



/*
** access functions (stack -> C)
*/


VENOM_API int venom_type (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : VENOM_TNONE);
}


VENOM_API const char *venom_typename (venom_State *L, int t) {
  UNUSED(L);
  api_check(L, VENOM_TNONE <= t && t < VENOM_NUMTYPES, "invalid type");
  return ttypename(t);
}


VENOM_API int venom_iscfunction (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


VENOM_API int venom_isinteger (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


VENOM_API int venom_isnumber (venom_State *L, int idx) {
  venom_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


VENOM_API int venom_isstring (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


VENOM_API int venom_isuserdata (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


VENOM_API int venom_rawequal (venom_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? venomV_rawequalobj(o1, o2) : 0;
}


VENOM_API void venom_arith (venom_State *L, int op) {
  venom_lock(L);
  if (op != VENOM_OPUNM && op != VENOM_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result Venom to top - 2 */
  venomO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  venom_unlock(L);
}


VENOM_API int venom_compare (venom_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  venom_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case VENOM_OPEQ: i = venomV_equalobj(L, o1, o2); break;
      case VENOM_OPLT: i = venomV_lessthan(L, o1, o2); break;
      case VENOM_OPLE: i = venomV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  venom_unlock(L);
  return i;
}


VENOM_API size_t venom_stringtonumber (venom_State *L, const char *s) {
  size_t sz = venomO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


VENOM_API venom_Number venom_tonumberx (venom_State *L, int idx, int *pisnum) {
  venom_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


VENOM_API venom_Integer venom_tointegerx (venom_State *L, int idx, int *pisnum) {
  venom_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


VENOM_API int venom_toboolean (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


VENOM_API const char *venom_tolstring (venom_State *L, int idx, size_t *len) {
  TValue *o;
  venom_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      venom_unlock(L);
      return NULL;
    }
    venomO_tostring(L, o);
    venomC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  venom_unlock(L);
  return svalue(o);
}


VENOM_API venom_Unsigned venom_rawlen (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case VENOM_VSHRSTR: return tsvalue(o)->shrlen;
    case VENOM_VLNGSTR: return tsvalue(o)->u.lnglen;
    case VENOM_VUSERDATA: return uvalue(o)->len;
    case VENOM_VTABLE: return venomH_getn(hvalue(o));
    default: return 0;
  }
}


VENOM_API venom_CFunction venom_tocfunction (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case VENOM_TUSERDATA: return getudatamem(uvalue(o));
    case VENOM_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


VENOM_API void *venom_touserdata (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


VENOM_API venom_State *venom_tothread (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here Venomes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
VENOM_API const void *venom_topointer (venom_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case VENOM_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case VENOM_VUSERDATA: case VENOM_VLIGHTUSERDATA:
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


VENOM_API void venom_pushnil (venom_State *L) {
  venom_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  venom_unlock(L);
}


VENOM_API void venom_pushnumber (venom_State *L, venom_Number n) {
  venom_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  venom_unlock(L);
}


VENOM_API void venom_pushinteger (venom_State *L, venom_Integer n) {
  venom_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  venom_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
VENOM_API const char *venom_pushlstring (venom_State *L, const char *s, size_t len) {
  TString *ts;
  venom_lock(L);
  ts = (len == 0) ? venomS_new(L, "") : venomS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  venomC_checkGC(L);
  venom_unlock(L);
  return getstr(ts);
}


VENOM_API const char *venom_pushstring (venom_State *L, const char *s) {
  venom_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = venomS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  venomC_checkGC(L);
  venom_unlock(L);
  return s;
}


VENOM_API const char *venom_pushvfstring (venom_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  venom_lock(L);
  ret = venomO_pushvfstring(L, fmt, argp);
  venomC_checkGC(L);
  venom_unlock(L);
  return ret;
}


VENOM_API const char *venom_pushfstring (venom_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  venom_lock(L);
  va_start(argp, fmt);
  ret = venomO_pushvfstring(L, fmt, argp);
  va_end(argp);
  venomC_checkGC(L);
  venom_unlock(L);
  return ret;
}


VENOM_API void venom_pushcclosure (venom_State *L, venom_CFunction fn, int n) {
  venom_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = venomF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      venom_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    venomC_checkGC(L);
  }
  venom_unlock(L);
}


VENOM_API void venom_pushboolean (venom_State *L, int b) {
  venom_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  venom_unlock(L);
}


VENOM_API void venom_pushlightuserdata (venom_State *L, void *p) {
  venom_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  venom_unlock(L);
}


VENOM_API int venom_pushthread (venom_State *L) {
  venom_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  venom_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Venom -> stack)
*/


l_sinline int auxgetstr (venom_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = venomS_new(L, k);
  if (venomV_fastget(L, t, str, slot, venomH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    venomV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  venom_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[VENOM_RIDX_GLOBALS - 1])


VENOM_API int venom_getglobal (venom_State *L, const char *name) {
  const TValue *G;
  venom_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


VENOM_API int venom_gettable (venom_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  venom_lock(L);
  t = index2value(L, idx);
  if (venomV_fastget(L, t, s2v(L->top - 1), slot, venomH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    venomV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  venom_unlock(L);
  return ttype(s2v(L->top - 1));
}


VENOM_API int venom_getfield (venom_State *L, int idx, const char *k) {
  venom_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


VENOM_API int venom_geti (venom_State *L, int idx, venom_Integer n) {
  TValue *t;
  const TValue *slot;
  venom_lock(L);
  t = index2value(L, idx);
  if (venomV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    venomV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  venom_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (venom_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  venom_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (venom_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


VENOM_API int venom_rawget (venom_State *L, int idx) {
  Table *t;
  const TValue *val;
  venom_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = venomH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


VENOM_API int venom_rawgeti (venom_State *L, int idx, venom_Integer n) {
  Table *t;
  venom_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, venomH_getint(t, n));
}


VENOM_API int venom_rawgetp (venom_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  venom_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, venomH_get(t, &k));
}


VENOM_API void venom_createtable (venom_State *L, int narray, int nrec) {
  Table *t;
  venom_lock(L);
  t = venomH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    venomH_resize(L, t, narray, nrec);
  venomC_checkGC(L);
  venom_unlock(L);
}


VENOM_API int venom_getmetatable (venom_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  venom_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case VENOM_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case VENOM_TUSERDATA:
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
  venom_unlock(L);
  return res;
}


VENOM_API int venom_getiuservalue (venom_State *L, int idx, int n) {
  TValue *o;
  int t;
  venom_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = VENOM_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  venom_unlock(L);
  return t;
}


/*
** set functions (stack -> Venom)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (venom_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = venomS_new(L, k);
  api_checknelems(L, 1);
  if (venomV_fastget(L, t, str, slot, venomH_getstr)) {
    venomV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    venomV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  venom_unlock(L);  /* lock done by caller */
}


VENOM_API void venom_setglobal (venom_State *L, const char *name) {
  const TValue *G;
  venom_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


VENOM_API void venom_settable (venom_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  venom_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (venomV_fastget(L, t, s2v(L->top - 2), slot, venomH_get)) {
    venomV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    venomV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  venom_unlock(L);
}


VENOM_API void venom_setfield (venom_State *L, int idx, const char *k) {
  venom_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


VENOM_API void venom_seti (venom_State *L, int idx, venom_Integer n) {
  TValue *t;
  const TValue *slot;
  venom_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (venomV_fastgeti(L, t, n, slot)) {
    venomV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    venomV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  venom_unlock(L);
}


static void aux_rawset (venom_State *L, int idx, TValue *key, int n) {
  Table *t;
  venom_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  venomH_set(L, t, key, s2v(L->top - 1));
  invalidateTMcache(t);
  venomC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  venom_unlock(L);
}


VENOM_API void venom_rawset (venom_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


VENOM_API void venom_rawsetp (venom_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


VENOM_API void venom_rawseti (venom_State *L, int idx, venom_Integer n) {
  Table *t;
  venom_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  venomH_setint(L, t, n, s2v(L->top - 1));
  venomC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  venom_unlock(L);
}


VENOM_API int venom_setmetatable (venom_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  venom_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case VENOM_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        venomC_objbarrier(L, gcvalue(obj), mt);
        venomC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case VENOM_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        venomC_objbarrier(L, uvalue(obj), mt);
        venomC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  venom_unlock(L);
  return 1;
}


VENOM_API int venom_setiuservalue (venom_State *L, int idx, int n) {
  TValue *o;
  int res;
  venom_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    venomC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  venom_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Venom code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == VENOM_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


VENOM_API void venom_callk (venom_State *L, int nargs, int nresults,
                        venom_KContext ctx, venom_KFunction k) {
  StkId func;
  venom_lock(L);
  api_check(L, k == NULL || !isVenom(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == VENOM_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    venomD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    venomD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  venom_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (venom_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  venomD_callnoyield(L, c->func, c->nresults);
}



VENOM_API int venom_pcallk (venom_State *L, int nargs, int nresults, int errfunc,
                        venom_KContext ctx, venom_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  venom_lock(L);
  api_check(L, k == NULL || !isVenom(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == VENOM_OK, "cannot do calls on non-normal thread");
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
    status = venomD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    venomD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = VENOM_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  venom_unlock(L);
  return status;
}


VENOM_API int venom_load (venom_State *L, venom_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  venom_lock(L);
  if (!chunkname) chunkname = "?";
  venomZ_init(L, &z, reader, data);
  status = venomD_protectedparser(L, &z, chunkname, mode);
  if (status == VENOM_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be VENOM_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      venomC_barrier(L, f->upvals[0], gt);
    }
  }
  venom_unlock(L);
  return status;
}


VENOM_API int venom_dump (venom_State *L, venom_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  venom_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (ttisfunction(o))
    status = venomU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  venom_unlock(L);
  return status;
}


VENOM_API int venom_status (venom_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
VENOM_API int venom_gc (venom_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  venom_lock(L);
  va_start(argp, what);
  switch (what) {
    case VENOM_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case VENOM_GCRESTART: {
      venomE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case VENOM_GCCOLLECT: {
      venomC_fulgarbageCollection(L, 0);
      break;
    }
    case VENOM_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case VENOM_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case VENOM_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        venomE_setdebt(g, 0);  /* do a basic step */
        venomC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        venomE_setdebt(g, debt);
        venomC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case VENOM_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case VENOM_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case VENOM_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case VENOM_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? VENOM_GCGEN : VENOM_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      venomC_changemode(L, KGC_GEN);
      break;
    }
    case VENOM_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? VENOM_GCGEN : VENOM_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      venomC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  venom_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


VENOM_API int venom_error (venom_State *L) {
  TValue *errobj;
  venom_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    venomM_error(L);  /* raise a memory error */
  else
    venomG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


VENOM_API int venom_next (venom_State *L, int idx) {
  Table *t;
  int more;
  venom_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = venomH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  venom_unlock(L);
  return more;
}


VENOM_API void venom_toclose (venom_State *L, int idx) {
  int nresults;
  StkId o;
  venom_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  venomF_newtbvenomval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  venom_assert(hastocloseCfunc(L->ci->nresults));
  venom_unlock(L);
}


VENOM_API void venom_concat (venom_State *L, int n) {
  venom_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    venomV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, venomS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  venomC_checkGC(L);
  venom_unlock(L);
}


VENOM_API void venom_len (venom_State *L, int idx) {
  TValue *t;
  venom_lock(L);
  t = index2value(L, idx);
  venomV_objlen(L, L->top, t);
  api_incr_top(L);
  venom_unlock(L);
}


VENOM_API venom_Alloc venom_getallocf (venom_State *L, void **ud) {
  venom_Alloc f;
  venom_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  venom_unlock(L);
  return f;
}


VENOM_API void venom_setallocf (venom_State *L, venom_Alloc f, void *ud) {
  venom_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  venom_unlock(L);
}


void venom_setwarnf (venom_State *L, venom_WarnFunction f, void *ud) {
  venom_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  venom_unlock(L);
}


void venom_warning (venom_State *L, const char *msg, int tocont) {
  venom_lock(L);
  venomE_warning(L, msg, tocont);
  venom_unlock(L);
}



VENOM_API void *venom_newuserdatauv (venom_State *L, size_t size, int nuvalue) {
  Udata *u;
  venom_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = venomS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  venomC_checkGC(L);
  venom_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case VENOM_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case VENOM_VLCL: {  /* Venom closure */
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


VENOM_API const char *venom_getupvalue (venom_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  venom_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  venom_unlock(L);
  return name;
}


VENOM_API const char *venom_setupvalue (venom_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  venom_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    venomC_barrier(L, owner, val);
  }
  venom_unlock(L);
  return name;
}


static UpVal **getupvalref (venom_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Venom function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


VENOM_API void *venom_upvalueid (venom_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case VENOM_VLCL: {  /* venom closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case VENOM_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case VENOM_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


VENOM_API void venom_upvaluejoin (venom_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  venomC_objbarrier(L, f1, *up1);
}

