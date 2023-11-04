/*
** $Id: memory.h $
** Interface to Memory Manager
** See Copyright Notice in hydrogen.h
*/

#ifndef memory_h
#define memory_h


#include <stddef.h>

#include "limits.h"
#include "hydrogen.h"


#define hydrogenM_error(L)	hydrogenD_throw(L, HYDROGEN_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define hydrogenM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define hydrogenM_checksize(L,n,e)  \
	(hydrogenM_testsize(n,e) ? hydrogenM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define hydrogenM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define hydrogenM_reallocvchar(L,b,on,n)  \
  cast_charp(hydrogenM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define hydrogenM_freemem(L, b, s)	hydrogenM_free_(L, (b), (s))
#define hydrogenM_free(L, b)		hydrogenM_free_(L, (b), sizeof(*(b)))
#define hydrogenM_freearray(L, b, n)   hydrogenM_free_(L, (b), (n)*sizeof(*(b)))

#define hydrogenM_new(L,t)		cast(t*, hydrogenM_malloc_(L, sizeof(t), 0))
#define hydrogenM_newvector(L,n,t)	cast(t*, hydrogenM_malloc_(L, (n)*sizeof(t), 0))
#define hydrogenM_newvectorchecked(L,n,t) \
  (hydrogenM_checksize(L,n,sizeof(t)), hydrogenM_newvector(L,n,t))

#define hydrogenM_newobject(L,tag,s)	hydrogenM_malloc_(L, (s), tag)

#define hydrogenM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, hydrogenM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         hydrogenM_limitN(limit,t),e)))

#define hydrogenM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, hydrogenM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define hydrogenM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, hydrogenM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

HYDROGENI_FUNC l_noret hydrogenM_toobig (hydrogen_State *L);

/* not to be called directly */
HYDROGENI_FUNC void *hydrogenM_realloc_ (hydrogen_State *L, void *block, size_t oldsize,
                                                          size_t size);
HYDROGENI_FUNC void *hydrogenM_saferealloc_ (hydrogen_State *L, void *block, size_t oldsize,
                                                              size_t size);
HYDROGENI_FUNC void hydrogenM_free_ (hydrogen_State *L, void *block, size_t osize);
HYDROGENI_FUNC void *hydrogenM_growaux_ (hydrogen_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
HYDROGENI_FUNC void *hydrogenM_shrinkvector_ (hydrogen_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
HYDROGENI_FUNC void *hydrogenM_malloc_ (hydrogen_State *L, size_t size, int tag);

#endif

