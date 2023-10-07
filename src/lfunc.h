/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in cup.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Cup). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10



/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(-1)


CUPI_FUNC Proto *cupF_newproto (cup_State *L);
CUPI_FUNC CClosure *cupF_newCclosure (cup_State *L, int nupvals);
CUPI_FUNC LClosure *cupF_newLclosure (cup_State *L, int nupvals);
CUPI_FUNC void cupF_initupvals (cup_State *L, LClosure *cl);
CUPI_FUNC UpVal *cupF_findupval (cup_State *L, StkId level);
CUPI_FUNC void cupF_newtbcupval (cup_State *L, StkId level);
CUPI_FUNC void cupF_closeupval (cup_State *L, StkId level);
CUPI_FUNC void cupF_close (cup_State *L, StkId level, int status, int yy);
CUPI_FUNC void cupF_unlinkupval (UpVal *uv);
CUPI_FUNC void cupF_freeproto (cup_State *L, Proto *f);
CUPI_FUNC const char *cupF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
