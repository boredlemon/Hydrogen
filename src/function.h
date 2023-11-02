/*
** $Id: function.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in venom.h
*/

#ifndef function_h
#define function_h


#include "object.h"


#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Venom). (Value
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


VENOMI_FUNC Proto *venomF_newproto (venom_State *L);
VENOMI_FUNC CClosure *venomF_newCclosure (venom_State *L, int nupvals);
VENOMI_FUNC LClosure *venomF_newLclosure (venom_State *L, int nupvals);
VENOMI_FUNC void venomF_initupvals (venom_State *L, LClosure *cl);
VENOMI_FUNC UpVal *venomF_findupval (venom_State *L, StkId level);
VENOMI_FUNC void venomF_newtbvenomval (venom_State *L, StkId level);
VENOMI_FUNC void venomF_closeupval (venom_State *L, StkId level);
VENOMI_FUNC void venomF_close (venom_State *L, StkId level, int status, int yy);
VENOMI_FUNC void venomF_unlinkupval (UpVal *uv);
VENOMI_FUNC void venomF_freeproto (venom_State *L, Proto *f);
VENOMI_FUNC const char *venomF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
