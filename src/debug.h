/*
** $Id: debug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in hydrogen.h
*/

#ifndef debug_h
#define debug_h


#include "state.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Hydrogen function (given call info) */
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


HYDROGENI_FUNC int hydrogenG_getfuncline (const Proto *f, int pc);
HYDROGENI_FUNC const char *hydrogenG_findlocal (hydrogen_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
HYDROGENI_FUNC l_noret hydrogenG_typeerror (hydrogen_State *L, const TValue *o,
                                                const char *opname);
HYDROGENI_FUNC l_noret hydrogenG_callerror (hydrogen_State *L, const TValue *o);
HYDROGENI_FUNC l_noret hydrogenG_forerror (hydrogen_State *L, const TValue *o,
                                               const char *what);
HYDROGENI_FUNC l_noret hydrogenG_concaterror (hydrogen_State *L, const TValue *p1,
                                                  const TValue *p2);
HYDROGENI_FUNC l_noret hydrogenG_opinterror (hydrogen_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
HYDROGENI_FUNC l_noret hydrogenG_tointerror (hydrogen_State *L, const TValue *p1,
                                                 const TValue *p2);
HYDROGENI_FUNC l_noret hydrogenG_ordererror (hydrogen_State *L, const TValue *p1,
                                                 const TValue *p2);
HYDROGENI_FUNC l_noret hydrogenG_runerror (hydrogen_State *L, const char *fmt, ...);
HYDROGENI_FUNC const char *hydrogenG_addinfo (hydrogen_State *L, const char *msg,
                                                  TString *src, int line);
HYDROGENI_FUNC l_noret hydrogenG_errormsg (hydrogen_State *L);
HYDROGENI_FUNC int hydrogenG_traceexec (hydrogen_State *L, const Instruction *pc);


#endif
