/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in viper.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Viper function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


VIPERI_FUNC int viperG_getfuncline (const Proto *f, int pc);
VIPERI_FUNC const char *viperG_findlocal (viper_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
VIPERI_FUNC l_noret viperG_typeerror (viper_State *L, const TValue *o,
                                                const char *opname);
VIPERI_FUNC l_noret viperG_callerror (viper_State *L, const TValue *o);
VIPERI_FUNC l_noret viperG_forerror (viper_State *L, const TValue *o,
                                               const char *what);
VIPERI_FUNC l_noret viperG_concaterror (viper_State *L, const TValue *p1,
                                                  const TValue *p2);
VIPERI_FUNC l_noret viperG_opinterror (viper_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
VIPERI_FUNC l_noret viperG_tointerror (viper_State *L, const TValue *p1,
                                                 const TValue *p2);
VIPERI_FUNC l_noret viperG_ordererror (viper_State *L, const TValue *p1,
                                                 const TValue *p2);
VIPERI_FUNC l_noret viperG_runerror (viper_State *L, const char *fmt, ...);
VIPERI_FUNC const char *viperG_addinfo (viper_State *L, const char *msg,
                                                  TString *src, int line);
VIPERI_FUNC l_noret viperG_errormsg (viper_State *L);
VIPERI_FUNC int viperG_traceexec (viper_State *L, const Instruction *pc);


#endif
