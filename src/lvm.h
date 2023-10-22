/*
** $Id: lvm.h $
** Viper virtual machine
** See Copyright Notice in viper.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(VIPER_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(VIPER_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define VIPER_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(VIPER_FLOORN2I)
#define VIPER_FLOORN2I		F2Ieq
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
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : viperV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : viperV_tointeger(o,i,VIPER_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : viperV_tointegerns(o,i,VIPER_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define viperV_rawequalobj(t1,t2)		viperV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define viperV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'viperV_fastget' for integers, inlining the fast case
** of 'viperH_getint'.
*/
#define viperV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : viperH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define viperV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      viperC_barrierback(L, gcvalue(t), v); }




VIPERI_FUNC int viperV_equalobj (viper_State *L, const TValue *t1, const TValue *t2);
VIPERI_FUNC int viperV_lessthan (viper_State *L, const TValue *l, const TValue *r);
VIPERI_FUNC int viperV_lessequal (viper_State *L, const TValue *l, const TValue *r);
VIPERI_FUNC int viperV_tonumber_ (const TValue *obj, viper_Number *n);
VIPERI_FUNC int viperV_tointeger (const TValue *obj, viper_Integer *p, F2Imod mode);
VIPERI_FUNC int viperV_tointegerns (const TValue *obj, viper_Integer *p,
                                F2Imod mode);
VIPERI_FUNC int viperV_flttointeger (viper_Number n, viper_Integer *p, F2Imod mode);
VIPERI_FUNC void viperV_finishget (viper_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
VIPERI_FUNC void viperV_finishset (viper_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
VIPERI_FUNC void viperV_finishOp (viper_State *L);
VIPERI_FUNC void viperV_execute (viper_State *L, CallInfo *ci);
VIPERI_FUNC void viperV_concat (viper_State *L, int total);
VIPERI_FUNC viper_Integer viperV_idiv (viper_State *L, viper_Integer x, viper_Integer y);
VIPERI_FUNC viper_Integer viperV_mod (viper_State *L, viper_Integer x, viper_Integer y);
VIPERI_FUNC viper_Number viperV_modf (viper_State *L, viper_Number x, viper_Number y);
VIPERI_FUNC viper_Integer viperV_shiftl (viper_Integer x, viper_Integer y);
VIPERI_FUNC void viperV_objlen (viper_State *L, StkId ra, const TValue *rb);

#endif
