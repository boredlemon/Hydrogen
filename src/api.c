/*
** $Id: lapi.c $
** Hydrogen API
** See Copyright Notice in hydrogen.h
*/

#define api_c
#define HYDROGEN_CORE

#include "prefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "hydrogen.h"

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



const char hydrogen_ident[] =
  "$HydrogenVersion: " HYDROGEN_COPYRIGHT " $"
  "$HydrogenAuthors: " HYDROGEN_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= HYDROGEN_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < HYDROGEN_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (hydrogen_State *L, int idx) {
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
  else if (idx == HYDROGEN_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = HYDROGEN_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Hydrogen function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (hydrogen_State *L, int idx) {
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


HYDROGEN_API int hydrogen_checkstack (hydrogen_State *L, int n) {
  int res;
  CallInfo *ci;
  hydrogen_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > HYDROGENI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = hydrogenD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  hydrogen_unlock(L);
  return res;
}


HYDROGEN_API void hydrogen_xmove (hydrogen_State *from, hydrogen_State *to, int n) {
  int i;
  if (from == to) return;
  hydrogen_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  hydrogen_unlock(to);
}


HYDROGEN_API hydrogen_CFunction hydrogen_atpanic (hydrogen_State *L, hydrogen_CFunction panicf) {
  hydrogen_CFunction old;
  hydrogen_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  hydrogen_unlock(L);
  return old;
}


HYDROGEN_API hydrogen_Number hydrogen_version (hydrogen_State *L) {
  UNUSED(L);
  return HYDROGEN_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
HYDROGEN_API int hydrogen_absindex (hydrogen_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


HYDROGEN_API int hydrogen_gettop (hydrogen_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


HYDROGEN_API void hydrogen_settop (hydrogen_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  hydrogen_lock(L);
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
    hydrogen_assert(hastocloseCfunc(ci->nresults));
    hydrogenF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_closeslot (hydrogen_State *L, int idx) {
  StkId level;
  hydrogen_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  hydrogenF_close(L, level, CLOSEKTOP, 0);
  level = index2stack(L, idx);  /* stack may be moved */
  setnilvalue(s2v(level));
  hydrogen_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'hydrogen_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (hydrogen_State *L, StkId from, StkId to) {
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
HYDROGEN_API void hydrogen_rotate (hydrogen_State *L, int idx, int n) {
  StkId p, t, m;
  hydrogen_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_copy (hydrogen_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  hydrogen_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    hydrogenC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* HYDROGEN_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_pushvalue (hydrogen_State *L, int idx) {
  hydrogen_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  hydrogen_unlock(L);
}



/*
** access functions (stack -> C)
*/


HYDROGEN_API int hydrogen_type (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : HYDROGEN_TNONE);
}


HYDROGEN_API const char *hydrogen_typename (hydrogen_State *L, int t) {
  UNUSED(L);
  api_check(L, HYDROGEN_TNONE <= t && t < HYDROGEN_NUMTYPES, "invalid type");
  return ttypename(t);
}


HYDROGEN_API int hydrogen_iscfunction (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


HYDROGEN_API int hydrogen_isinteger (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


HYDROGEN_API int hydrogen_isnumber (hydrogen_State *L, int idx) {
  hydrogen_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


HYDROGEN_API int hydrogen_isstring (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


HYDROGEN_API int hydrogen_isuserdata (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


HYDROGEN_API int hydrogen_rawequal (hydrogen_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? hydrogenV_rawequalobj(o1, o2) : 0;
}


HYDROGEN_API void hydrogen_arith (hydrogen_State *L, int op) {
  hydrogen_lock(L);
  if (op != HYDROGEN_OPUNM && op != HYDROGEN_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result Hydrogen to top - 2 */
  hydrogenO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  hydrogen_unlock(L);
}


HYDROGEN_API int hydrogen_compare (hydrogen_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  hydrogen_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case HYDROGEN_OPEQ: i = hydrogenV_equalobj(L, o1, o2); break;
      case HYDROGEN_OPLT: i = hydrogenV_lessthan(L, o1, o2); break;
      case HYDROGEN_OPLE: i = hydrogenV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  hydrogen_unlock(L);
  return i;
}


HYDROGEN_API size_t hydrogen_stringtonumber (hydrogen_State *L, const char *s) {
  size_t sz = hydrogenO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


HYDROGEN_API hydrogen_Number hydrogen_tonumberx (hydrogen_State *L, int idx, int *pisnum) {
  hydrogen_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


HYDROGEN_API hydrogen_Integer hydrogen_tointegerx (hydrogen_State *L, int idx, int *pisnum) {
  hydrogen_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


HYDROGEN_API int hydrogen_toboolean (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


HYDROGEN_API const char *hydrogen_tolstring (hydrogen_State *L, int idx, size_t *len) {
  TValue *o;
  hydrogen_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      hydrogen_unlock(L);
      return NULL;
    }
    hydrogenO_tostring(L, o);
    hydrogenC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  hydrogen_unlock(L);
  return svalue(o);
}


HYDROGEN_API hydrogen_Unsigned hydrogen_rawlen (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case HYDROGEN_VSHRSTR: return tsvalue(o)->shrlen;
    case HYDROGEN_VLNGSTR: return tsvalue(o)->u.lnglen;
    case HYDROGEN_VUSERDATA: return uvalue(o)->len;
    case HYDROGEN_VTABLE: return hydrogenH_getn(hvalue(o));
    default: return 0;
  }
}


HYDROGEN_API hydrogen_CFunction hydrogen_tocfunction (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case HYDROGEN_TUSERDATA: return getudatamem(uvalue(o));
    case HYDROGEN_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


HYDROGEN_API void *hydrogen_touserdata (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


HYDROGEN_API hydrogen_State *hydrogen_tothread (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here Hydrogenes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
HYDROGEN_API const void *hydrogen_topointer (hydrogen_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case HYDROGEN_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case HYDROGEN_VUSERDATA: case HYDROGEN_VLIGHTUSERDATA:
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


HYDROGEN_API void hydrogen_pushnil (hydrogen_State *L) {
  hydrogen_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_pushnumber (hydrogen_State *L, hydrogen_Number n) {
  hydrogen_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_pushinteger (hydrogen_State *L, hydrogen_Integer n) {
  hydrogen_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  hydrogen_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
HYDROGEN_API const char *hydrogen_pushlstring (hydrogen_State *L, const char *s, size_t len) {
  TString *ts;
  hydrogen_lock(L);
  ts = (len == 0) ? hydrogenS_new(L, "") : hydrogenS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
  return getstr(ts);
}


HYDROGEN_API const char *hydrogen_pushstring (hydrogen_State *L, const char *s) {
  hydrogen_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = hydrogenS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
  return s;
}


HYDROGEN_API const char *hydrogen_pushvfstring (hydrogen_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  hydrogen_lock(L);
  ret = hydrogenO_pushvfstring(L, fmt, argp);
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
  return ret;
}


HYDROGEN_API const char *hydrogen_pushfstring (hydrogen_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  hydrogen_lock(L);
  va_start(argp, fmt);
  ret = hydrogenO_pushvfstring(L, fmt, argp);
  va_end(argp);
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
  return ret;
}


HYDROGEN_API void hydrogen_pushcclosure (hydrogen_State *L, hydrogen_CFunction fn, int n) {
  hydrogen_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = hydrogenF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      hydrogen_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    hydrogenC_checkGC(L);
  }
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_pushboolean (hydrogen_State *L, int b) {
  hydrogen_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_pushlightuserdata (hydrogen_State *L, void *p) {
  hydrogen_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  hydrogen_unlock(L);
}


HYDROGEN_API int hydrogen_pushthread (hydrogen_State *L) {
  hydrogen_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  hydrogen_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Hydrogen -> stack)
*/


l_sinline int auxgetstr (hydrogen_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = hydrogenS_new(L, k);
  if (hydrogenV_fastget(L, t, str, slot, hydrogenH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    hydrogenV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  hydrogen_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[HYDROGEN_RIDX_GLOBALS - 1])


HYDROGEN_API int hydrogen_getglobal (hydrogen_State *L, const char *name) {
  const TValue *G;
  hydrogen_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


HYDROGEN_API int hydrogen_gettable (hydrogen_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  hydrogen_lock(L);
  t = index2value(L, idx);
  if (hydrogenV_fastget(L, t, s2v(L->top - 1), slot, hydrogenH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    hydrogenV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  hydrogen_unlock(L);
  return ttype(s2v(L->top - 1));
}


HYDROGEN_API int hydrogen_getfield (hydrogen_State *L, int idx, const char *k) {
  hydrogen_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


HYDROGEN_API int hydrogen_geti (hydrogen_State *L, int idx, hydrogen_Integer n) {
  TValue *t;
  const TValue *slot;
  hydrogen_lock(L);
  t = index2value(L, idx);
  if (hydrogenV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    hydrogenV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  hydrogen_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (hydrogen_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  hydrogen_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (hydrogen_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


HYDROGEN_API int hydrogen_rawget (hydrogen_State *L, int idx) {
  Table *t;
  const TValue *val;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = hydrogenH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


HYDROGEN_API int hydrogen_rawgeti (hydrogen_State *L, int idx, hydrogen_Integer n) {
  Table *t;
  hydrogen_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, hydrogenH_getint(t, n));
}


HYDROGEN_API int hydrogen_rawgetp (hydrogen_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  hydrogen_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, hydrogenH_get(t, &k));
}


HYDROGEN_API void hydrogen_createtable (hydrogen_State *L, int narray, int nrec) {
  Table *t;
  hydrogen_lock(L);
  t = hydrogenH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    hydrogenH_resize(L, t, narray, nrec);
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
}


HYDROGEN_API int hydrogen_getmetatable (hydrogen_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  hydrogen_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case HYDROGEN_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case HYDROGEN_TUSERDATA:
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
  hydrogen_unlock(L);
  return res;
}


HYDROGEN_API int hydrogen_getiuservalue (hydrogen_State *L, int idx, int n) {
  TValue *o;
  int t;
  hydrogen_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = HYDROGEN_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  hydrogen_unlock(L);
  return t;
}


/*
** set functions (stack -> Hydrogen)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (hydrogen_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = hydrogenS_new(L, k);
  api_checknelems(L, 1);
  if (hydrogenV_fastget(L, t, str, slot, hydrogenH_getstr)) {
    hydrogenV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    hydrogenV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  hydrogen_unlock(L);  /* lock done by caller */
}


HYDROGEN_API void hydrogen_setglobal (hydrogen_State *L, const char *name) {
  const TValue *G;
  hydrogen_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


HYDROGEN_API void hydrogen_settable (hydrogen_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  hydrogen_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (hydrogenV_fastget(L, t, s2v(L->top - 2), slot, hydrogenH_get)) {
    hydrogenV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    hydrogenV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_setfield (hydrogen_State *L, int idx, const char *k) {
  hydrogen_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


HYDROGEN_API void hydrogen_seti (hydrogen_State *L, int idx, hydrogen_Integer n) {
  TValue *t;
  const TValue *slot;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (hydrogenV_fastgeti(L, t, n, slot)) {
    hydrogenV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    hydrogenV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  hydrogen_unlock(L);
}


static void aux_rawset (hydrogen_State *L, int idx, TValue *key, int n) {
  Table *t;
  hydrogen_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  hydrogenH_set(L, t, key, s2v(L->top - 1));
  invalidateTMcache(t);
  hydrogenC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_rawset (hydrogen_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


HYDROGEN_API void hydrogen_rawsetp (hydrogen_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


HYDROGEN_API void hydrogen_rawseti (hydrogen_State *L, int idx, hydrogen_Integer n) {
  Table *t;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  hydrogenH_setint(L, t, n, s2v(L->top - 1));
  hydrogenC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  hydrogen_unlock(L);
}


HYDROGEN_API int hydrogen_setmetatable (hydrogen_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case HYDROGEN_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        hydrogenC_objbarrier(L, gcvalue(obj), mt);
        hydrogenC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case HYDROGEN_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        hydrogenC_objbarrier(L, uvalue(obj), mt);
        hydrogenC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  hydrogen_unlock(L);
  return 1;
}


HYDROGEN_API int hydrogen_setiuservalue (hydrogen_State *L, int idx, int n) {
  TValue *o;
  int res;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    hydrogenC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  hydrogen_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Hydrogen code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == HYDROGEN_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


HYDROGEN_API void hydrogen_callk (hydrogen_State *L, int nargs, int nresults,
                        hydrogen_KContext ctx, hydrogen_KFunction k) {
  StkId func;
  hydrogen_lock(L);
  api_check(L, k == NULL || !isHydrogen(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == HYDROGEN_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    hydrogenD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    hydrogenD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  hydrogen_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (hydrogen_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  hydrogenD_callnoyield(L, c->func, c->nresults);
}



HYDROGEN_API int hydrogen_pcallk (hydrogen_State *L, int nargs, int nresults, int errfunc,
                        hydrogen_KContext ctx, hydrogen_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  hydrogen_lock(L);
  api_check(L, k == NULL || !isHydrogen(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == HYDROGEN_OK, "cannot do calls on non-normal thread");
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
    status = hydrogenD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    hydrogenD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = HYDROGEN_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  hydrogen_unlock(L);
  return status;
}


HYDROGEN_API int hydrogen_load (hydrogen_State *L, hydrogen_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  hydrogen_lock(L);
  if (!chunkname) chunkname = "?";
  hydrogenZ_init(L, &z, reader, data);
  status = hydrogenD_protectedparser(L, &z, chunkname, mode);
  if (status == HYDROGEN_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be HYDROGEN_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      hydrogenC_barrier(L, f->upvals[0], gt);
    }
  }
  hydrogen_unlock(L);
  return status;
}


HYDROGEN_API int hydrogen_dump (hydrogen_State *L, hydrogen_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (ttisfunction(o))
    status = hydrogenU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  hydrogen_unlock(L);
  return status;
}


HYDROGEN_API int hydrogen_status (hydrogen_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
HYDROGEN_API int hydrogen_gc (hydrogen_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  hydrogen_lock(L);
  va_start(argp, what);
  switch (what) {
    case HYDROGEN_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case HYDROGEN_GCRESTART: {
      hydrogenE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case HYDROGEN_GCCOLLECT: {
      hydrogenC_fulgarbageCollection(L, 0);
      break;
    }
    case HYDROGEN_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case HYDROGEN_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case HYDROGEN_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        hydrogenE_setdebt(g, 0);  /* do a basic step */
        hydrogenC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        hydrogenE_setdebt(g, debt);
        hydrogenC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case HYDROGEN_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case HYDROGEN_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case HYDROGEN_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case HYDROGEN_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? HYDROGEN_GCGEN : HYDROGEN_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      hydrogenC_changemode(L, KGC_GEN);
      break;
    }
    case HYDROGEN_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? HYDROGEN_GCGEN : HYDROGEN_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      hydrogenC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  hydrogen_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


HYDROGEN_API int hydrogen_error (hydrogen_State *L) {
  TValue *errobj;
  hydrogen_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    hydrogenM_error(L);  /* raise a memory error */
  else
    hydrogenG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


HYDROGEN_API int hydrogen_next (hydrogen_State *L, int idx) {
  Table *t;
  int more;
  hydrogen_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = hydrogenH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  hydrogen_unlock(L);
  return more;
}


HYDROGEN_API void hydrogen_toclose (hydrogen_State *L, int idx) {
  int nresults;
  StkId o;
  hydrogen_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  hydrogenF_newtbhydrogenval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  hydrogen_assert(hastocloseCfunc(L->ci->nresults));
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_concat (hydrogen_State *L, int n) {
  hydrogen_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    hydrogenV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, hydrogenS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
}


HYDROGEN_API void hydrogen_len (hydrogen_State *L, int idx) {
  TValue *t;
  hydrogen_lock(L);
  t = index2value(L, idx);
  hydrogenV_objlen(L, L->top, t);
  api_incr_top(L);
  hydrogen_unlock(L);
}


HYDROGEN_API hydrogen_Alloc hydrogen_getallocf (hydrogen_State *L, void **ud) {
  hydrogen_Alloc f;
  hydrogen_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  hydrogen_unlock(L);
  return f;
}


HYDROGEN_API void hydrogen_setallocf (hydrogen_State *L, hydrogen_Alloc f, void *ud) {
  hydrogen_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  hydrogen_unlock(L);
}


void hydrogen_setwarnf (hydrogen_State *L, hydrogen_WarnFunction f, void *ud) {
  hydrogen_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  hydrogen_unlock(L);
}


void hydrogen_warning (hydrogen_State *L, const char *msg, int tocont) {
  hydrogen_lock(L);
  hydrogenE_warning(L, msg, tocont);
  hydrogen_unlock(L);
}



HYDROGEN_API void *hydrogen_newuserdatauv (hydrogen_State *L, size_t size, int nuvalue) {
  Udata *u;
  hydrogen_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = hydrogenS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  hydrogenC_checkGC(L);
  hydrogen_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case HYDROGEN_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case HYDROGEN_VLCL: {  /* Hydrogen closure */
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


HYDROGEN_API const char *hydrogen_getupvalue (hydrogen_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  hydrogen_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  hydrogen_unlock(L);
  return name;
}


HYDROGEN_API const char *hydrogen_setupvalue (hydrogen_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  hydrogen_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    hydrogenC_barrier(L, owner, val);
  }
  hydrogen_unlock(L);
  return name;
}


static UpVal **getupvalref (hydrogen_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Hydrogen function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


HYDROGEN_API void *hydrogen_upvalueid (hydrogen_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case HYDROGEN_VLCL: {  /* hydrogen closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case HYDROGEN_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case HYDROGEN_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


HYDROGEN_API void hydrogen_upvaluejoin (hydrogen_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  hydrogenC_objbarrier(L, f1, *up1);
}

