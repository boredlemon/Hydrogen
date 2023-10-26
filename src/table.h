/*
** $Id: table.h $
** Viper tables (hash)
** See Copyright Notice in viper.h
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


VIPERI_FUNC const TValue *viperH_getint (Table *t, viper_Integer key);
VIPERI_FUNC void viperH_setint (viper_State *L, Table *t, viper_Integer key,
                                                    TValue *value);
VIPERI_FUNC const TValue *viperH_getshortstr (Table *t, TString *key);
VIPERI_FUNC const TValue *viperH_getstr (Table *t, TString *key);
VIPERI_FUNC const TValue *viperH_get (Table *t, const TValue *key);
VIPERI_FUNC void viperH_newkey (viper_State *L, Table *t, const TValue *key,
                                                    TValue *value);
VIPERI_FUNC void viperH_set (viper_State *L, Table *t, const TValue *key,
                                                 TValue *value);
VIPERI_FUNC void viperH_finishset (viper_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
VIPERI_FUNC Table *viperH_new (viper_State *L);
VIPERI_FUNC void viperH_resize (viper_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
VIPERI_FUNC void viperH_resizearray (viper_State *L, Table *t, unsigned int nasize);
VIPERI_FUNC void viperH_free (viper_State *L, Table *t);
VIPERI_FUNC int viperH_next (viper_State *L, Table *t, StkId key);
VIPERI_FUNC viper_Unsigned viperH_getn (Table *t);
VIPERI_FUNC unsigned int viperH_realasize (const Table *t);


#if defined(VIPER_DEBUG)
VIPERI_FUNC Node *viperH_mainposition (const Table *t, const TValue *key);
VIPERI_FUNC int viperH_isdummy (const Table *t);
#endif


#endif
