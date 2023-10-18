/*
** $Id: lvm.h $
** Acorn virtual machine
** See Copyright Notice in acorn.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(ACORN_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(ACORN_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define ACORN_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(ACORN_FLOORN2I)
#define ACORN_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : acornV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : acornV_tointeger(o,i,ACORN_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : acornV_tointegerns(o,i,ACORN_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define acornV_rawequalobj(t1,t2)		acornV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define acornV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'acornV_fastget' for integers, inlining the fast case
** of 'acornH_getint'.
*/
#define acornV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : acornH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define acornV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      acornC_barrierback(L, gcvalue(t), v); }




ACORNI_FUNC int acornV_equalobj (acorn_State *L, const TValue *t1, const TValue *t2);
ACORNI_FUNC int acornV_lessthan (acorn_State *L, const TValue *l, const TValue *r);
ACORNI_FUNC int acornV_lessequal (acorn_State *L, const TValue *l, const TValue *r);
ACORNI_FUNC int acornV_tonumber_ (const TValue *obj, acorn_Number *n);
ACORNI_FUNC int acornV_tointeger (const TValue *obj, acorn_Integer *p, F2Imod mode);
ACORNI_FUNC int acornV_tointegerns (const TValue *obj, acorn_Integer *p,
                                F2Imod mode);
ACORNI_FUNC int acornV_flttointeger (acorn_Number n, acorn_Integer *p, F2Imod mode);
ACORNI_FUNC void acornV_finishget (acorn_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
ACORNI_FUNC void acornV_finishset (acorn_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
ACORNI_FUNC void acornV_finishOp (acorn_State *L);
ACORNI_FUNC void acornV_execute (acorn_State *L, CallInfo *ci);
ACORNI_FUNC void acornV_concat (acorn_State *L, int total);
ACORNI_FUNC acorn_Integer acornV_idiv (acorn_State *L, acorn_Integer x, acorn_Integer y);
ACORNI_FUNC acorn_Integer acornV_mod (acorn_State *L, acorn_Integer x, acorn_Integer y);
ACORNI_FUNC acorn_Number acornV_modf (acorn_State *L, acorn_Number x, acorn_Number y);
ACORNI_FUNC acorn_Integer acornV_shiftl (acorn_Integer x, acorn_Integer y);
ACORNI_FUNC void acornV_objlen (acorn_State *L, StkId ra, const TValue *rb);

#endif
