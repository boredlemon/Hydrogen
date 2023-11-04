/*
** $Id: table.h $
** Hydrogen tables (hash)
** See Copyright Notice in hydrogen.h
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


HYDROGENI_FUNC const TValue *hydrogenH_getint (Table *t, hydrogen_Integer key);
HYDROGENI_FUNC void hydrogenH_setint (hydrogen_State *L, Table *t, hydrogen_Integer key,
                                                    TValue *value);
HYDROGENI_FUNC const TValue *hydrogenH_getshortstr (Table *t, TString *key);
HYDROGENI_FUNC const TValue *hydrogenH_getstr (Table *t, TString *key);
HYDROGENI_FUNC const TValue *hydrogenH_get (Table *t, const TValue *key);
HYDROGENI_FUNC void hydrogenH_newkey (hydrogen_State *L, Table *t, const TValue *key,
                                                    TValue *value);
HYDROGENI_FUNC void hydrogenH_set (hydrogen_State *L, Table *t, const TValue *key,
                                                 TValue *value);
HYDROGENI_FUNC void hydrogenH_finishset (hydrogen_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
HYDROGENI_FUNC Table *hydrogenH_new (hydrogen_State *L);
HYDROGENI_FUNC void hydrogenH_resize (hydrogen_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
HYDROGENI_FUNC void hydrogenH_resizearray (hydrogen_State *L, Table *t, unsigned int nasize);
HYDROGENI_FUNC void hydrogenH_free (hydrogen_State *L, Table *t);
HYDROGENI_FUNC int hydrogenH_next (hydrogen_State *L, Table *t, StkId key);
HYDROGENI_FUNC hydrogen_Unsigned hydrogenH_getn (Table *t);
HYDROGENI_FUNC unsigned int hydrogenH_realasize (const Table *t);


#if defined(HYDROGEN_DEBUG)
HYDROGENI_FUNC Node *hydrogenH_mainposition (const Table *t, const TValue *key);
HYDROGENI_FUNC int hydrogenH_isdummy (const Table *t);
#endif


#endif
