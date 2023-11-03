/*
** $Id: tagMethods.h $
** Tag methods
** See Copyright Notice in nebula.h
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
  ((et)->flags & (1u<<(e))) ? NULL : nebulaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	nebulaT_typenames_[(x) + 1]

NEBULAI_DDEC(const char *const nebulaT_typenames_[NEBULA_TOTALTYPES];)


NEBULAI_FUNC const char *nebulaT_objtypename (nebula_State *L, const TValue *o);

NEBULAI_FUNC const TValue *nebulaT_gettm (Table *events, TMS event, TString *ename);
NEBULAI_FUNC const TValue *nebulaT_gettmbyobj (nebula_State *L, const TValue *o,
                                                       TMS event);
NEBULAI_FUNC void nebulaT_init (nebula_State *L);

NEBULAI_FUNC void nebulaT_caltagMethods (nebula_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
NEBULAI_FUNC void nebulaT_caltagMethodsres (nebula_State *L, const TValue *f,
                            const TValue *p1, const TValue *p2, StkId p3);
NEBULAI_FUNC void nebulaT_trybinTM (nebula_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
NEBULAI_FUNC void nebulaT_tryconcatTM (nebula_State *L);
NEBULAI_FUNC void nebulaT_trybinassocTM (nebula_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
NEBULAI_FUNC void nebulaT_trybiniTM (nebula_State *L, const TValue *p1, nebula_Integer i2,
                               int inv, StkId res, TMS event);
NEBULAI_FUNC int nebulaT_callorderTM (nebula_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
NEBULAI_FUNC int nebulaT_callorderiTM (nebula_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

NEBULAI_FUNC void nebulaT_adjustvarargs (nebula_State *L, int nfixparams,
                                   struct CallInfo *ci, const Proto *p);
NEBULAI_FUNC void nebulaT_getvarargs (nebula_State *L, struct CallInfo *ci,
                                              StkId where, int wanted);


#endif
