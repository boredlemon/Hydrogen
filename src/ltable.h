/*
** $Id: ltable.h $
** Cup tables (hash)
** See Copyright Notice in cup.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


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


CUPI_FUNC const TValue *cupH_getint (Table *t, cup_Integer key);
CUPI_FUNC void cupH_setint (cup_State *L, Table *t, cup_Integer key,
                                                    TValue *value);
CUPI_FUNC const TValue *cupH_getshortstr (Table *t, TString *key);
CUPI_FUNC const TValue *cupH_getstr (Table *t, TString *key);
CUPI_FUNC const TValue *cupH_get (Table *t, const TValue *key);
CUPI_FUNC void cupH_newkey (cup_State *L, Table *t, const TValue *key,
                                                    TValue *value);
CUPI_FUNC void cupH_set (cup_State *L, Table *t, const TValue *key,
                                                 TValue *value);
CUPI_FUNC void cupH_finishset (cup_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
CUPI_FUNC Table *cupH_new (cup_State *L);
CUPI_FUNC void cupH_resize (cup_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
CUPI_FUNC void cupH_resizearray (cup_State *L, Table *t, unsigned int nasize);
CUPI_FUNC void cupH_free (cup_State *L, Table *t);
CUPI_FUNC int cupH_next (cup_State *L, Table *t, StkId key);
CUPI_FUNC cup_Unsigned cupH_getn (Table *t);
CUPI_FUNC unsigned int cupH_realasize (const Table *t);


#if defined(CUP_DEBUG)
CUPI_FUNC Node *cupH_mainposition (const Table *t, const TValue *key);
CUPI_FUNC int cupH_isdummy (const Table *t);
#endif


#endif
