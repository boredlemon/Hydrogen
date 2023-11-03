/*
** $Id: table.h $
** Nebula tables (hash)
** See Copyright Notice in nebula.h
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


NEBULAI_FUNC const TValue *nebulaH_getint (Table *t, nebula_Integer key);
NEBULAI_FUNC void nebulaH_setint (nebula_State *L, Table *t, nebula_Integer key,
                                                    TValue *value);
NEBULAI_FUNC const TValue *nebulaH_getshortstr (Table *t, TString *key);
NEBULAI_FUNC const TValue *nebulaH_getstr (Table *t, TString *key);
NEBULAI_FUNC const TValue *nebulaH_get (Table *t, const TValue *key);
NEBULAI_FUNC void nebulaH_newkey (nebula_State *L, Table *t, const TValue *key,
                                                    TValue *value);
NEBULAI_FUNC void nebulaH_set (nebula_State *L, Table *t, const TValue *key,
                                                 TValue *value);
NEBULAI_FUNC void nebulaH_finishset (nebula_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
NEBULAI_FUNC Table *nebulaH_new (nebula_State *L);
NEBULAI_FUNC void nebulaH_resize (nebula_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
NEBULAI_FUNC void nebulaH_resizearray (nebula_State *L, Table *t, unsigned int nasize);
NEBULAI_FUNC void nebulaH_free (nebula_State *L, Table *t);
NEBULAI_FUNC int nebulaH_next (nebula_State *L, Table *t, StkId key);
NEBULAI_FUNC nebula_Unsigned nebulaH_getn (Table *t);
NEBULAI_FUNC unsigned int nebulaH_realasize (const Table *t);


#if defined(NEBULA_DEBUG)
NEBULAI_FUNC Node *nebulaH_mainposition (const Table *t, const TValue *key);
NEBULAI_FUNC int nebulaH_isdummy (const Table *t);
#endif


#endif
