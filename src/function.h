/*
** $Id: function.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in nebula.h
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
** maximum number of upvalues in a closure (both C and Nebula). (Value
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


NEBULAI_FUNC Proto *nebulaF_newproto (nebula_State *L);
NEBULAI_FUNC CClosure *nebulaF_newCclosure (nebula_State *L, int nupvals);
NEBULAI_FUNC LClosure *nebulaF_newLclosure (nebula_State *L, int nupvals);
NEBULAI_FUNC void nebulaF_initupvals (nebula_State *L, LClosure *cl);
NEBULAI_FUNC UpVal *nebulaF_findupval (nebula_State *L, StkId level);
NEBULAI_FUNC void nebulaF_newtbnebulaval (nebula_State *L, StkId level);
NEBULAI_FUNC void nebulaF_closeupval (nebula_State *L, StkId level);
NEBULAI_FUNC void nebulaF_close (nebula_State *L, StkId level, int status, int yy);
NEBULAI_FUNC void nebulaF_unlinkupval (UpVal *uv);
NEBULAI_FUNC void nebulaF_freeproto (nebula_State *L, Proto *f);
NEBULAI_FUNC const char *nebulaF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
