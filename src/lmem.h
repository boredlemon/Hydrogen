/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in acorn.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "acorn.h"


#define acornM_error(L)	acornD_throw(L, ACORN_ERRMEM)


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
#define acornM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define acornM_checksize(L,n,e)  \
	(acornM_testsize(n,e) ? acornM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define acornM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define acornM_reallocvchar(L,b,on,n)  \
  cast_charp(acornM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define acornM_freemem(L, b, s)	acornM_free_(L, (b), (s))
#define acornM_free(L, b)		acornM_free_(L, (b), sizeof(*(b)))
#define acornM_freearray(L, b, n)   acornM_free_(L, (b), (n)*sizeof(*(b)))

#define acornM_new(L,t)		cast(t*, acornM_malloc_(L, sizeof(t), 0))
#define acornM_newvector(L,n,t)	cast(t*, acornM_malloc_(L, (n)*sizeof(t), 0))
#define acornM_newvectorchecked(L,n,t) \
  (acornM_checksize(L,n,sizeof(t)), acornM_newvector(L,n,t))

#define acornM_newobject(L,tag,s)	acornM_malloc_(L, (s), tag)

#define acornM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, acornM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         acornM_limitN(limit,t),e)))

#define acornM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, acornM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define acornM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, acornM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

ACORNI_FUNC l_noret acornM_toobig (acorn_State *L);

/* not to be called directly */
ACORNI_FUNC void *acornM_realloc_ (acorn_State *L, void *block, size_t oldsize,
                                                          size_t size);
ACORNI_FUNC void *acornM_saferealloc_ (acorn_State *L, void *block, size_t oldsize,
                                                              size_t size);
ACORNI_FUNC void acornM_free_ (acorn_State *L, void *block, size_t osize);
ACORNI_FUNC void *acornM_growaux_ (acorn_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
ACORNI_FUNC void *acornM_shrinkvector_ (acorn_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
ACORNI_FUNC void *acornM_malloc_ (acorn_State *L, size_t size, int tag);

#endif

