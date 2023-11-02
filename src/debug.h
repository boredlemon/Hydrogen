/*
** $Id: debug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in venom.h
*/

#ifndef debug_h
#define debug_h


#include "state.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Venom function (given call info) */
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


VENOMI_FUNC int venomG_getfuncline (const Proto *f, int pc);
VENOMI_FUNC const char *venomG_findlocal (venom_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
VENOMI_FUNC l_noret venomG_typeerror (venom_State *L, const TValue *o,
                                                const char *opname);
VENOMI_FUNC l_noret venomG_callerror (venom_State *L, const TValue *o);
VENOMI_FUNC l_noret venomG_forerror (venom_State *L, const TValue *o,
                                               const char *what);
VENOMI_FUNC l_noret venomG_concaterror (venom_State *L, const TValue *p1,
                                                  const TValue *p2);
VENOMI_FUNC l_noret venomG_opinterror (venom_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
VENOMI_FUNC l_noret venomG_tointerror (venom_State *L, const TValue *p1,
                                                 const TValue *p2);
VENOMI_FUNC l_noret venomG_ordererror (venom_State *L, const TValue *p1,
                                                 const TValue *p2);
VENOMI_FUNC l_noret venomG_runerror (venom_State *L, const char *fmt, ...);
VENOMI_FUNC const char *venomG_addinfo (venom_State *L, const char *msg,
                                                  TString *src, int line);
VENOMI_FUNC l_noret venomG_errormsg (venom_State *L);
VENOMI_FUNC int venomG_traceexec (venom_State *L, const Instruction *pc);


#endif
