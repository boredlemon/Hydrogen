/*
** $Id: table.h $
** Venom tables (hash)
** See Copyright Notice in venom.h
*/

#ifndef table_h
#define table_h

#include "object.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= ~maskflags)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))


VENOMI_FUNC const TValue *venomH_getint (Table *t, venom_Integer key);
VENOMI_FUNC void venomH_setint (venom_State *L, Table *t, venom_Integer key,
                                                    TValue *value);
VENOMI_FUNC const TValue *venomH_getshortstr (Table *t, TString *key);
VENOMI_FUNC const TValue *venomH_getstr (Table *t, TString *key);
VENOMI_FUNC const TValue *venomH_get (Table *t, const TValue *key);
VENOMI_FUNC void venomH_newkey (venom_State *L, Table *t, const TValue *key,
                                                    TValue *value);
VENOMI_FUNC void venomH_set (venom_State *L, Table *t, const TValue *key,
                                                 TValue *value);
VENOMI_FUNC void venomH_finishset (venom_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
VENOMI_FUNC Table *venomH_new (venom_State *L);
VENOMI_FUNC void venomH_resize (venom_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
VENOMI_FUNC void venomH_resizearray (venom_State *L, Table *t, unsigned int nasize);
VENOMI_FUNC void venomH_free (venom_State *L, Table *t);
VENOMI_FUNC int venomH_next (venom_State *L, Table *t, StkId key);
VENOMI_FUNC venom_Unsigned venomH_getn (Table *t);
VENOMI_FUNC unsigned int venomH_realasize (const Table *t);


#if defined(VENOM_DEBUG)
VENOMI_FUNC Node *venomH_mainposition (const Table *t, const TValue *key);
VENOMI_FUNC int venomH_isdummy (const Table *t);
#endif


#endif
