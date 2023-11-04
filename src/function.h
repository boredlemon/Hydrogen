/*
** $Id: function.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in hydrogen.h
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
** maximum number of upvalues in a closure (both C and Hydrogen). (Value
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


HYDROGENI_FUNC Proto *hydrogenF_newproto (hydrogen_State *L);
HYDROGENI_FUNC CClosure *hydrogenF_newCclosure (hydrogen_State *L, int nupvals);
HYDROGENI_FUNC LClosure *hydrogenF_newLclosure (hydrogen_State *L, int nupvals);
HYDROGENI_FUNC void hydrogenF_initupvals (hydrogen_State *L, LClosure *cl);
HYDROGENI_FUNC UpVal *hydrogenF_findupval (hydrogen_State *L, StkId level);
HYDROGENI_FUNC void hydrogenF_newtbhydrogenval (hydrogen_State *L, StkId level);
HYDROGENI_FUNC void hydrogenF_closeupval (hydrogen_State *L, StkId level);
HYDROGENI_FUNC void hydrogenF_close (hydrogen_State *L, StkId level, int status, int yy);
HYDROGENI_FUNC void hydrogenF_unlinkupval (UpVal *uv);
HYDROGENI_FUNC void hydrogenF_freeproto (hydrogen_State *L, Proto *f);
HYDROGENI_FUNC const char *hydrogenF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
