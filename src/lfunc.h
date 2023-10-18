/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in acorn.h
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
** maximum number of upvalues in a closure (both C and Acorn). (Value
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


ACORNI_FUNC Proto *acornF_newproto (acorn_State *L);
ACORNI_FUNC CClosure *acornF_newCclosure (acorn_State *L, int nupvals);
ACORNI_FUNC LClosure *acornF_newLclosure (acorn_State *L, int nupvals);
ACORNI_FUNC void acornF_initupvals (acorn_State *L, LClosure *cl);
ACORNI_FUNC UpVal *acornF_findupval (acorn_State *L, StkId level);
ACORNI_FUNC void acornF_newtbacornval (acorn_State *L, StkId level);
ACORNI_FUNC void acornF_closeupval (acorn_State *L, StkId level);
ACORNI_FUNC void acornF_close (acorn_State *L, StkId level, int status, int yy);
ACORNI_FUNC void acornF_unlinkupval (UpVal *uv);
ACORNI_FUNC void acornF_freeproto (acorn_State *L, Proto *f);
ACORNI_FUNC const char *acornF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
