/*
** $Id: ltable.h $
** Acorn tables (hash)
** See Copyright Notice in acorn.h
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


ACORNI_FUNC const TValue *acornH_getint (Table *t, acorn_Integer key);
ACORNI_FUNC void acornH_setint (acorn_State *L, Table *t, acorn_Integer key,
                                                    TValue *value);
ACORNI_FUNC const TValue *acornH_getshortstr (Table *t, TString *key);
ACORNI_FUNC const TValue *acornH_getstr (Table *t, TString *key);
ACORNI_FUNC const TValue *acornH_get (Table *t, const TValue *key);
ACORNI_FUNC void acornH_newkey (acorn_State *L, Table *t, const TValue *key,
                                                    TValue *value);
ACORNI_FUNC void acornH_set (acorn_State *L, Table *t, const TValue *key,
                                                 TValue *value);
ACORNI_FUNC void acornH_finishset (acorn_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
ACORNI_FUNC Table *acornH_new (acorn_State *L);
ACORNI_FUNC void acornH_resize (acorn_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
ACORNI_FUNC void acornH_resizearray (acorn_State *L, Table *t, unsigned int nasize);
ACORNI_FUNC void acornH_free (acorn_State *L, Table *t);
ACORNI_FUNC int acornH_next (acorn_State *L, Table *t, StkId key);
ACORNI_FUNC acorn_Unsigned acornH_getn (Table *t);
ACORNI_FUNC unsigned int acornH_realasize (const Table *t);


#if defined(ACORN_DEBUG)
ACORNI_FUNC Node *acornH_mainposition (const Table *t, const TValue *key);
ACORNI_FUNC int acornH_isdummy (const Table *t);
#endif


#endif
