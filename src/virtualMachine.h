/*
** $Id: virtualMachine.h $
** Hydrogen virtual machine
** See Copyright Notice in hydrogen.h
*/

#ifndef virtualMachine_h
#define virtualMachine_h


#include "do.h"
#include "object.h"
#include "tagMethods.h"


#if !defined(HYDROGEN_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(HYDROGEN_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define HYDROGEN_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(HYDROGEN_FLOORN2I)
#define HYDROGEN_FLOORN2I		F2Ieq
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
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : hydrogenV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : hydrogenV_tointeger(o,i,HYDROGEN_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : hydrogenV_tointegerns(o,i,HYDROGEN_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define hydrogenV_rawequalobj(t1,t2)		hydrogenV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define hydrogenV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'hydrogenV_fastget' for integers, inlining the fast case
** of 'hydrogenH_getint'.
*/
#define hydrogenV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : hydrogenH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define hydrogenV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      hydrogenC_barrierback(L, gcvalue(t), v); }




HYDROGENI_FUNC int hydrogenV_equalobj (hydrogen_State *L, const TValue *t1, const TValue *t2);
HYDROGENI_FUNC int hydrogenV_lessthan (hydrogen_State *L, const TValue *l, const TValue *r);
HYDROGENI_FUNC int hydrogenV_lessequal (hydrogen_State *L, const TValue *l, const TValue *r);
HYDROGENI_FUNC int hydrogenV_tonumber_ (const TValue *obj, hydrogen_Number *n);
HYDROGENI_FUNC int hydrogenV_tointeger (const TValue *obj, hydrogen_Integer *p, F2Imod mode);
HYDROGENI_FUNC int hydrogenV_tointegerns (const TValue *obj, hydrogen_Integer *p,
                                F2Imod mode);
HYDROGENI_FUNC int hydrogenV_flttointeger (hydrogen_Number n, hydrogen_Integer *p, F2Imod mode);
HYDROGENI_FUNC void hydrogenV_finishget (hydrogen_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
HYDROGENI_FUNC void hydrogenV_finishset (hydrogen_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
HYDROGENI_FUNC void hydrogenV_finishOp (hydrogen_State *L);
HYDROGENI_FUNC void hydrogenV_execute (hydrogen_State *L, CallInfo *ci);
HYDROGENI_FUNC void hydrogenV_concat (hydrogen_State *L, int total);
HYDROGENI_FUNC hydrogen_Integer hydrogenV_idiv (hydrogen_State *L, hydrogen_Integer x, hydrogen_Integer y);
HYDROGENI_FUNC hydrogen_Integer hydrogenV_mod (hydrogen_State *L, hydrogen_Integer x, hydrogen_Integer y);
HYDROGENI_FUNC hydrogen_Number hydrogenV_modf (hydrogen_State *L, hydrogen_Number x, hydrogen_Number y);
HYDROGENI_FUNC hydrogen_Integer hydrogenV_shiftl (hydrogen_Integer x, hydrogen_Integer y);
HYDROGENI_FUNC void hydrogenV_objlen (hydrogen_State *L, StkId ra, const TValue *rb);

#endif
