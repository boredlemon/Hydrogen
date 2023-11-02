/*
** $Id: virtualMachine.h $
** Venom virtual machine
** See Copyright Notice in venom.h
*/

#ifndef virtualMachine_h
#define virtualMachine_h


#include "do.h"
#include "object.h"
#include "tagMethods.h"


#if !defined(VENOM_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(VENOM_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define VENOM_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(VENOM_FLOORN2I)
#define VENOM_FLOORN2I		F2Ieq
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
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : venomV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : venomV_tointeger(o,i,VENOM_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : venomV_tointegerns(o,i,VENOM_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define venomV_rawequalobj(t1,t2)		venomV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define venomV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'venomV_fastget' for integers, inlining the fast case
** of 'venomH_getint'.
*/
#define venomV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : venomH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define venomV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      venomC_barrierback(L, gcvalue(t), v); }




VENOMI_FUNC int venomV_equalobj (venom_State *L, const TValue *t1, const TValue *t2);
VENOMI_FUNC int venomV_lessthan (venom_State *L, const TValue *l, const TValue *r);
VENOMI_FUNC int venomV_lessequal (venom_State *L, const TValue *l, const TValue *r);
VENOMI_FUNC int venomV_tonumber_ (const TValue *obj, venom_Number *n);
VENOMI_FUNC int venomV_tointeger (const TValue *obj, venom_Integer *p, F2Imod mode);
VENOMI_FUNC int venomV_tointegerns (const TValue *obj, venom_Integer *p,
                                F2Imod mode);
VENOMI_FUNC int venomV_flttointeger (venom_Number n, venom_Integer *p, F2Imod mode);
VENOMI_FUNC void venomV_finishget (venom_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
VENOMI_FUNC void venomV_finishset (venom_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
VENOMI_FUNC void venomV_finishOp (venom_State *L);
VENOMI_FUNC void venomV_execute (venom_State *L, CallInfo *ci);
VENOMI_FUNC void venomV_concat (venom_State *L, int total);
VENOMI_FUNC venom_Integer venomV_idiv (venom_State *L, venom_Integer x, venom_Integer y);
VENOMI_FUNC venom_Integer venomV_mod (venom_State *L, venom_Integer x, venom_Integer y);
VENOMI_FUNC venom_Number venomV_modf (venom_State *L, venom_Number x, venom_Number y);
VENOMI_FUNC venom_Integer venomV_shiftl (venom_Integer x, venom_Integer y);
VENOMI_FUNC void venomV_objlen (venom_State *L, StkId ra, const TValue *rb);

#endif
