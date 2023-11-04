/*
** $Id: tagMethods.h $
** Tag methods
** See Copyright Notice in hydrogen.h
*/

#ifndef tagMethods_h
#define tagMethods_h


#include "object.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_IDIV,
  TM_BAND,
  TM_BOR,
  TM_BXOR,
  TM_SHL,
  TM_SHR,
  TM_UNM,
  TM_BNOT,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_CLOSE,
  TM_N		/* number of elements in the enum */
} TMS;


/*
** Mask with 1 in all fast-access methods. A 1 in any of these bits
** in the flag of a (meta)table means the metatable does not have the
** corresponding metamethod field. (Bit 7 of the flag is used for
** 'isrealasize'.)
*/
#define maskflags	(~(~0u << (TM_EQ + 1)))


/*
** Test whether there is no tagmethod.
** (Because tagmethods use raw accesses, the result may be an "empty" nil.)
*/
#define notm(tm)	ttisnil(tm)


#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : hydrogenT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	hydrogenT_typenames_[(x) + 1]

HYDROGENI_DDEC(const char *const hydrogenT_typenames_[HYDROGEN_TOTALTYPES];)


HYDROGENI_FUNC const char *hydrogenT_objtypename (hydrogen_State *L, const TValue *o);

HYDROGENI_FUNC const TValue *hydrogenT_gettm (Table *events, TMS event, TString *ename);
HYDROGENI_FUNC const TValue *hydrogenT_gettmbyobj (hydrogen_State *L, const TValue *o,
                                                       TMS event);
HYDROGENI_FUNC void hydrogenT_init (hydrogen_State *L);

HYDROGENI_FUNC void hydrogenT_caltagMethods (hydrogen_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
HYDROGENI_FUNC void hydrogenT_caltagMethodsres (hydrogen_State *L, const TValue *f,
                            const TValue *p1, const TValue *p2, StkId p3);
HYDROGENI_FUNC void hydrogenT_trybinTM (hydrogen_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
HYDROGENI_FUNC void hydrogenT_tryconcatTM (hydrogen_State *L);
HYDROGENI_FUNC void hydrogenT_trybinassocTM (hydrogen_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
HYDROGENI_FUNC void hydrogenT_trybiniTM (hydrogen_State *L, const TValue *p1, hydrogen_Integer i2,
                               int inv, StkId res, TMS event);
HYDROGENI_FUNC int hydrogenT_callorderTM (hydrogen_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
HYDROGENI_FUNC int hydrogenT_callorderiTM (hydrogen_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

HYDROGENI_FUNC void hydrogenT_adjustvarargs (hydrogen_State *L, int nfixparams,
                                   struct CallInfo *ci, const Proto *p);
HYDROGENI_FUNC void hydrogenT_getvarargs (hydrogen_State *L, struct CallInfo *ci,
                                              StkId where, int wanted);


#endif
