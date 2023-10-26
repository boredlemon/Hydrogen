/*
** $Id: function.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in viper.h
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
** maximum number of upvalues in a closure (both C and Viper). (Value
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


VIPERI_FUNC Proto *viperF_newproto (viper_State *L);
VIPERI_FUNC CClosure *viperF_newCclosure (viper_State *L, int nupvals);
VIPERI_FUNC LClosure *viperF_newLclosure (viper_State *L, int nupvals);
VIPERI_FUNC void viperF_initupvals (viper_State *L, LClosure *cl);
VIPERI_FUNC UpVal *viperF_findupval (viper_State *L, StkId level);
VIPERI_FUNC void viperF_newtbviperval (viper_State *L, StkId level);
VIPERI_FUNC void viperF_closeupval (viper_State *L, StkId level);
VIPERI_FUNC void viperF_close (viper_State *L, StkId level, int status, int yy);
VIPERI_FUNC void viperF_unlinkupval (UpVal *uv);
VIPERI_FUNC void viperF_freeproto (viper_State *L, Proto *f);
VIPERI_FUNC const char *viperF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
