/*
** $Id: virtualMachine.h $
** Nebula virtual machine
** See Copyright Notice in nebula.h
*/

#ifndef virtualMachine_h
#define virtualMachine_h


#include "do.h"
#include "object.h"
#include "tagMethods.h"


#if !defined(NEBULA_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(NEBULA_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define NEBULA_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(NEBULA_FLOORN2I)
#define NEBULA_FLOORN2I		F2Ieq
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
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : nebulaV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : nebulaV_tointeger(o,i,NEBULA_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : nebulaV_tointegerns(o,i,NEBULA_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define nebulaV_rawequalobj(t1,t2)		nebulaV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define nebulaV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'nebulaV_fastget' for integers, inlining the fast case
** of 'nebulaH_getint'.
*/
#define nebulaV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : nebulaH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define nebulaV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      nebulaC_barrierback(L, gcvalue(t), v); }




NEBULAI_FUNC int nebulaV_equalobj (nebula_State *L, const TValue *t1, const TValue *t2);
NEBULAI_FUNC int nebulaV_lessthan (nebula_State *L, const TValue *l, const TValue *r);
NEBULAI_FUNC int nebulaV_lessequal (nebula_State *L, const TValue *l, const TValue *r);
NEBULAI_FUNC int nebulaV_tonumber_ (const TValue *obj, nebula_Number *n);
NEBULAI_FUNC int nebulaV_tointeger (const TValue *obj, nebula_Integer *p, F2Imod mode);
NEBULAI_FUNC int nebulaV_tointegerns (const TValue *obj, nebula_Integer *p,
                                F2Imod mode);
NEBULAI_FUNC int nebulaV_flttointeger (nebula_Number n, nebula_Integer *p, F2Imod mode);
NEBULAI_FUNC void nebulaV_finishget (nebula_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
NEBULAI_FUNC void nebulaV_finishset (nebula_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
NEBULAI_FUNC void nebulaV_finishOp (nebula_State *L);
NEBULAI_FUNC void nebulaV_execute (nebula_State *L, CallInfo *ci);
NEBULAI_FUNC void nebulaV_concat (nebula_State *L, int total);
NEBULAI_FUNC nebula_Integer nebulaV_idiv (nebula_State *L, nebula_Integer x, nebula_Integer y);
NEBULAI_FUNC nebula_Integer nebulaV_mod (nebula_State *L, nebula_Integer x, nebula_Integer y);
NEBULAI_FUNC nebula_Number nebulaV_modf (nebula_State *L, nebula_Number x, nebula_Number y);
NEBULAI_FUNC nebula_Integer nebulaV_shiftl (nebula_Integer x, nebula_Integer y);
NEBULAI_FUNC void nebulaV_objlen (nebula_State *L, StkId ra, const TValue *rb);

#endif
