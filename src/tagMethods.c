/*
** $Id: tagMethods.c $
** Tag methods
** See Copyright Notice in hydrogen.h
*/

#define tagMethods_c
#define HYDROGEN_CORE

#include "prefix.h"


#include <string.h>

#include "hydrogen.h"

#include "debug.h"
#include "do.h"
#include "garbageCollection.h"
#include "object.h"
#include "state.h"
#include "string.h"
#include "table.h"
#include "tagMethods.h"
#include "virtualMachine.h"


static const char udatatypename[] = "userdata";

HYDROGENI_DDEF const char *const hydrogenT_typenames_[HYDROGEN_TOTALTYPES] = {
  "no value",
  "nil", "boolean", udatatypename, "number",
  "string", "table", "function", udatatypename, "thread",
  "upvalue", "proto" /* these last cases are used for tests only */
};


void hydrogenT_init (hydrogen_State *L) {
  static const char *const hydrogenT_eventname[] = {  /* ORDER TM */
    "__index", "__newindex",
    "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__mod", "__pow",
    "__div", "__idiv",
    "__band", "__bor", "__bxor", "__shl", "__shr",
    "__unm", "__bnot", "__lt", "__le",
    "__concat", "__call", "__close"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = hydrogenS_new(L, hydrogenT_eventname[i]);
    hydrogenC_fix(L, obj2gco(G(L)->tmname[i]));  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TValue *hydrogenT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = hydrogenH_getshortstr(events, ename);
  hydrogen_assert(event <= TM_EQ);
  if (notm(tm)) {  /* no tag method? */
    events->flags |= cast_byte(1u<<event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}


const TValue *hydrogenT_gettmbyobj (hydrogen_State *L, const TValue *o, TMS event) {
  Table *mt;
  switch (ttype(o)) {
    case HYDROGEN_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case HYDROGEN_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(o)];
  }
  return (mt ? hydrogenH_getshortstr(mt, G(L)->tmname[event]) : &G(L)->nilvalue);
}


/*
** Return the name of the type of an object. For tables and userdata
** with metatable, use their '__name' metafield, if present.
*/
const char *hydrogenT_objtypename (hydrogen_State *L, const TValue *o) {
  Table *mt;
  if ((ttistable(o) && (mt = hvalue(o)->metatable) != NULL) ||
      (ttisfulluserdata(o) && (mt = uvalue(o)->metatable) != NULL)) {
    const TValue *name = hydrogenH_getshortstr(mt, hydrogenS_new(L, "__name"));
    if (ttisstring(name))  /* is '__name' a string? */
      return getstr(tsvalue(name));  /* use it as type name */
  }
  return ttypename(ttype(o));  /* else use standard type name */
}


void hydrogenT_caltagMethods (hydrogen_State *L, const TValue *f, const TValue *p1,
                  const TValue *p2, const TValue *p3) {
  StkId func = L->top;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  setobj2s(L, func + 3, p3);  /* 3rd argument */
  L->top = func + 4;
  /* metamethod may yield only when called from Hydrogen code */
  if (isHydrogencode(L->ci))
    hydrogenD_call(L, func, 0);
  else
    hydrogenD_callnoyield(L, func, 0);
}


void hydrogenT_caltagMethodsres (hydrogen_State *L, const TValue *f, const TValue *p1,
                     const TValue *p2, StkId res) {
  ptrdiff_t result = savestack(L, res);
  StkId func = L->top;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  L->top += 3;
  /* metamethod may yield only when called from Hydrogen code */
  if (isHydrogencode(L->ci))
    hydrogenD_call(L, func, 1);
  else
    hydrogenD_callnoyield(L, func, 1);
  res = restorestack(L, result);
  setobjs2s(L, res, --L->top);  /* move result to its place */
}


static int callbinTM (hydrogen_State *L, const TValue *p1, const TValue *p2,
                      StkId res, TMS event) {
  const TValue *tm = hydrogenT_gettmbyobj(L, p1, event);  /* try first operand */
  if (notm(tm))
    tm = hydrogenT_gettmbyobj(L, p2, event);  /* try second operand */
  if (notm(tm)) return 0;
  hydrogenT_caltagMethodsres(L, tm, p1, p2, res);
  return 1;
}


void hydrogenT_trybinTM (hydrogen_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  if (l_unlikely(!callbinTM(L, p1, p2, res, event))) {
    switch (event) {
      case TM_BAND: case TM_BOR: case TM_BXOR:
      case TM_SHL: case TM_SHR: case TM_BNOT: {
        if (ttisnumber(p1) && ttisnumber(p2))
          hydrogenG_tointerror(L, p1, p2);
        else
          hydrogenG_opinterror(L, p1, p2, "perform bitwise operation on");
      }
      /* calls never return, but to avoid warnings: *//* FALLTHROUGH */
      default:
        hydrogenG_opinterror(L, p1, p2, "perform arithmetic on");
    }
  }
}


void hydrogenT_tryconcatTM (hydrogen_State *L) {
  StkId top = L->top;
  if (l_unlikely(!callbinTM(L, s2v(top - 2), s2v(top - 1), top - 2,
                               TM_CONCAT)))
    hydrogenG_concaterror(L, s2v(top - 2), s2v(top - 1));
}


void hydrogenT_trybinassocTM (hydrogen_State *L, const TValue *p1, const TValue *p2,
                                       int flip, StkId res, TMS event) {
  if (flip)
    hydrogenT_trybinTM(L, p2, p1, res, event);
  else
    hydrogenT_trybinTM(L, p1, p2, res, event);
}


void hydrogenT_trybiniTM (hydrogen_State *L, const TValue *p1, hydrogen_Integer i2,
                                   int flip, StkId res, TMS event) {
  TValue aux;
  setivalue(&aux, i2);
  hydrogenT_trybinassocTM(L, p1, &aux, flip, res, event);
}


/*
** Calls an order tag method.
** For lessequal, HYDROGEN_COMPAT_LT_LE keeps compatibility with old
** behavior: if there is no '__le', try '__lt', based on l <= r iff
** !(r < l) (assuming a total order). If the metamethod yields during
** this substitution, the continuation has to know about it (to negate
** the result of r<l); bit CIST_LEQ in the call status keeps that
** information.
*/
int hydrogenT_callorderTM (hydrogen_State *L, const TValue *p1, const TValue *p2,
                      TMS event) {
  if (callbinTM(L, p1, p2, L->top, event))  /* try original event */
    return !l_isfalse(s2v(L->top));
#if defined(HYDROGEN_COMPAT_LT_LE)
  else if (event == TM_LE) {
      /* try '!(p2 < p1)' for '(p1 <= p2)' */
      L->ci->callstatus |= CIST_LEQ;  /* mark it is doing 'lt' for 'le' */
      if (callbinTM(L, p2, p1, L->top, TM_LT)) {
        L->ci->callstatus ^= CIST_LEQ;  /* clear mark */
        return l_isfalse(s2v(L->top));
      }
      /* else error will remove this 'ci'; no need to clear mark */
  }
#endif
  hydrogenG_ordererror(L, p1, p2);  /* no metamethod found */
  return 0;  /* to avoid warnings */
}


int hydrogenT_callorderiTM (hydrogen_State *L, const TValue *p1, int v2,
                       int flip, int isfloat, TMS event) {
  TValue aux; const TValue *p2;
  if (isfloat) {
    setfltvalue(&aux, cast_num(v2));
  }
  else
    setivalue(&aux, v2);
  if (flip) {  /* arguments were exchanged? */
    p2 = p1; p1 = &aux;  /* correct them */
  }
  else
    p2 = &aux;
  return hydrogenT_callorderTM(L, p1, p2, event);
}


void hydrogenT_adjustvarargs (hydrogen_State *L, int nfixparams, CallInfo *ci,
                         const Proto *p) {
  int i;
  int actual = cast_int(L->top - ci->func) - 1;  /* number of arguments */
  int nextra = actual - nfixparams;  /* number of extra arguments */
  ci->u.l.nextraargs = nextra;
  hydrogenD_checkstack(L, p->maxstacksize + 1);
  /* copy function to the top of the stack */
  setobjs2s(L, L->top++, ci->func);
  /* move fixed parameters to the top of the stack */
  for (i = 1; i <= nfixparams; i++) {
    setobjs2s(L, L->top++, ci->func + i);
    setnilvalue(s2v(ci->func + i));  /* erase original parameter (for GC) */
  }
  ci->func += actual + 1;
  ci->top += actual + 1;
  hydrogen_assert(L->top <= ci->top && ci->top <= L->stack_last);
}


void hydrogenT_getvarargs (hydrogen_State *L, CallInfo *ci, StkId where, int wanted) {
  int i;
  int nextra = ci->u.l.nextraargs;
  if (wanted < 0) {
    wanted = nextra;  /* get all extra arguments available */
    checkstackGCp(L, nextra, where);  /* ensure stack space */
    L->top = where + nextra;  /* next instruction will need top */
  }
  for (i = 0; i < wanted && i < nextra; i++)
    setobjs2s(L, where + i, ci->func - nextra + i);
  for (; i < wanted; i++)   /* complete required results with nil */
    setnilvalue(s2v(where + i));
}

