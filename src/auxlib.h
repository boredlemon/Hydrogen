/*
** $Id: auxlib.h $
** Auxiliary functions for building Hydrogen libraries
** See Copyright Notice in hydrogen.h
*/


#ifndef auxlib_h
#define auxlib_h


#include <stddef.h>
#include <stdio.h>

#include "hydrogenconf.h"
#include "hydrogen.h"


/* global table */
#define HYDROGEN_GNAME	"_G"


typedef struct hydrogenL_Buffer hydrogenL_Buffer;


/* extra error code for 'hydrogenL_loadfilex' */
#define HYDROGEN_ERRFILE     (HYDROGEN_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define HYDROGEN_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define HYDROGEN_PRELOAD_TABLE	"_PRELOAD"


typedef struct hydrogenL_Reg {
  const char *name;
  hydrogen_CFunction func;
} hydrogenL_Reg;


#define HYDROGENL_NUMSIZES	(sizeof(hydrogen_Integer)*16 + sizeof(hydrogen_Number))

HYDROGENLIB_API void (hydrogenL_checkversion_) (hydrogen_State *L, hydrogen_Number ver, size_t sz);
#define hydrogenL_checkversion(L)  \
	  hydrogenL_checkversion_(L, HYDROGEN_VERSION_NUM, HYDROGENL_NUMSIZES)

HYDROGENLIB_API int (hydrogenL_getmetafield) (hydrogen_State *L, int obj, const char *e);
HYDROGENLIB_API int (hydrogenL_callmeta) (hydrogen_State *L, int obj, const char *e);
HYDROGENLIB_API const char *(hydrogenL_tolstring) (hydrogen_State *L, int idx, size_t *len);
HYDROGENLIB_API int (hydrogenL_argerror) (hydrogen_State *L, int arg, const char *extramsg);
HYDROGENLIB_API int (hydrogenL_typeerror) (hydrogen_State *L, int arg, const char *tname);
HYDROGENLIB_API const char *(hydrogenL_checklstring) (hydrogen_State *L, int arg,
                                                          size_t *l);
HYDROGENLIB_API const char *(hydrogenL_optlstring) (hydrogen_State *L, int arg,
                                          const char *def, size_t *l);
HYDROGENLIB_API hydrogen_Number (hydrogenL_checknumber) (hydrogen_State *L, int arg);
HYDROGENLIB_API hydrogen_Number (hydrogenL_optnumber) (hydrogen_State *L, int arg, hydrogen_Number def);

HYDROGENLIB_API hydrogen_Integer (hydrogenL_checkinteger) (hydrogen_State *L, int arg);
HYDROGENLIB_API hydrogen_Integer (hydrogenL_optinteger) (hydrogen_State *L, int arg,
                                          hydrogen_Integer def);

HYDROGENLIB_API void (hydrogenL_checkstack) (hydrogen_State *L, int sz, const char *msg);
HYDROGENLIB_API void (hydrogenL_checktype) (hydrogen_State *L, int arg, int t);
HYDROGENLIB_API void (hydrogenL_checkany) (hydrogen_State *L, int arg);

HYDROGENLIB_API int   (hydrogenL_newmetatable) (hydrogen_State *L, const char *tname);
HYDROGENLIB_API void  (hydrogenL_setmetatable) (hydrogen_State *L, const char *tname);
HYDROGENLIB_API void *(hydrogenL_testudata) (hydrogen_State *L, int ud, const char *tname);
HYDROGENLIB_API void *(hydrogenL_checkudata) (hydrogen_State *L, int ud, const char *tname);

HYDROGENLIB_API void (hydrogenL_where) (hydrogen_State *L, int lvl);
HYDROGENLIB_API int (hydrogenL_error) (hydrogen_State *L, const char *fmt, ...);

HYDROGENLIB_API int (hydrogenL_checkoption) (hydrogen_State *L, int arg, const char *def,
                                   const char *const lst[]);

HYDROGENLIB_API int (hydrogenL_fileresult) (hydrogen_State *L, int stat, const char *fname);
HYDROGENLIB_API int (hydrogenL_execresult) (hydrogen_State *L, int stat);


/* predefined references */
#define HYDROGEN_NOREF       (-2)
#define HYDROGEN_REFNIL      (-1)

HYDROGENLIB_API int (hydrogenL_ref) (hydrogen_State *L, int t);
HYDROGENLIB_API void (hydrogenL_unref) (hydrogen_State *L, int t, int ref);

HYDROGENLIB_API int (hydrogenL_loadfilex) (hydrogen_State *L, const char *filename,
                                               const char *mode);

#define hydrogenL_loadfile(L,f)	hydrogenL_loadfilex(L,f,NULL)

HYDROGENLIB_API int (hydrogenL_loadbufferx) (hydrogen_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
HYDROGENLIB_API int (hydrogenL_loadstring) (hydrogen_State *L, const char *s);

HYDROGENLIB_API hydrogen_State *(hydrogenL_newstate) (void);

HYDROGENLIB_API hydrogen_Integer (hydrogenL_len) (hydrogen_State *L, int idx);

HYDROGENLIB_API void (hydrogenL_addgsub) (hydrogenL_Buffer *b, const char *s,
                                     const char *p, const char *r);
HYDROGENLIB_API const char *(hydrogenL_gsub) (hydrogen_State *L, const char *s,
                                    const char *p, const char *r);

HYDROGENLIB_API void (hydrogenL_setfuncs) (hydrogen_State *L, const hydrogenL_Reg *l, int nup);

HYDROGENLIB_API int (hydrogenL_getsubtable) (hydrogen_State *L, int idx, const char *fname);

HYDROGENLIB_API void (hydrogenL_traceback) (hydrogen_State *L, hydrogen_State *L1,
                                  const char *msg, int level);

HYDROGENLIB_API void (hydrogenL_requiref) (hydrogen_State *L, const char *modname,
                                 hydrogen_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define hydrogenL_newlibtable(L,l)	\
  hydrogen_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define hydrogenL_newlib(L,l)  \
  (hydrogenL_checkversion(L), hydrogenL_newlibtable(L,l), hydrogenL_setfuncs(L,l,0))

#define hydrogenL_argcheck(L, cond,arg,extramsg)	\
	((void)(hydrogeni_likely(cond) || hydrogenL_argerror(L, (arg), (extramsg))))

#define hydrogenL_argexpected(L,cond,arg,tname)	\
	((void)(hydrogeni_likely(cond) || hydrogenL_typeerror(L, (arg), (tname))))

#define hydrogenL_checkstring(L,n)	(hydrogenL_checklstring(L, (n), NULL))
#define hydrogenL_optstring(L,n,d)	(hydrogenL_optlstring(L, (n), (d), NULL))

#define hydrogenL_typename(L,i)	hydrogen_typename(L, hydrogen_type(L,(i)))

#define hydrogenL_dofile(L, fn) \
	(hydrogenL_loadfile(L, fn) || hydrogen_pcall(L, 0, HYDROGEN_MULTRET, 0))

#define hydrogenL_dostring(L, s) \
	(hydrogenL_loadstring(L, s) || hydrogen_pcall(L, 0, HYDROGEN_MULTRET, 0))

#define hydrogenL_getmetatable(L,n)	(hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, (n)))

#define hydrogenL_opt(L,f,n,d)	(hydrogen_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define hydrogenL_loadbuffer(L,s,sz,n)	hydrogenL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on hydrogen_Integer values with wrap-around
** semantics, as the Hydrogen core does.
*/
#define hydrogenL_intop(op,v1,v2)  \
	((hydrogen_Integer)((hydrogen_Unsigned)(v1) op (hydrogen_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define hydrogenL_pushfail(L)	hydrogen_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(hydrogen_assert)

#if defined HYDROGENI_ASSERT
  #include <assert.h>
  #define hydrogen_assert(c)		assert(c)
#else
  #define hydrogen_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct hydrogenL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  hydrogen_State *L;
  union {
    HYDROGENI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[HYDROGENL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define hydrogenL_bufflen(bf)	((bf)->n)
#define hydrogenL_buffaddr(bf)	((bf)->b)


#define hydrogenL_addchar(B,c) \
  ((void)((B)->n < (B)->size || hydrogenL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define hydrogenL_addsize(B,s)	((B)->n += (s))

#define hydrogenL_buffsub(B,s)	((B)->n -= (s))

HYDROGENLIB_API void (hydrogenL_buffinit) (hydrogen_State *L, hydrogenL_Buffer *B);
HYDROGENLIB_API char *(hydrogenL_prepbuffsize) (hydrogenL_Buffer *B, size_t sz);
HYDROGENLIB_API void (hydrogenL_addlstring) (hydrogenL_Buffer *B, const char *s, size_t l);
HYDROGENLIB_API void (hydrogenL_addstring) (hydrogenL_Buffer *B, const char *s);
HYDROGENLIB_API void (hydrogenL_addvalue) (hydrogenL_Buffer *B);
HYDROGENLIB_API void (hydrogenL_pushresult) (hydrogenL_Buffer *B);
HYDROGENLIB_API void (hydrogenL_pushresultsize) (hydrogenL_Buffer *B, size_t sz);
HYDROGENLIB_API char *(hydrogenL_buffinitsize) (hydrogen_State *L, hydrogenL_Buffer *B, size_t sz);

#define hydrogenL_prepbuffer(B)	hydrogenL_prepbuffsize(B, HYDROGENL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'HYDROGEN_FILEHANDLE' and
** initial structure 'hydrogenL_Stream' (it may contain other fields
** after that initial structure).
*/

#define HYDROGEN_FILEHANDLE          "FILE*"


typedef struct hydrogenL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  hydrogen_CFunction closef;  /* to close stream (NULL for closed streams) */
} hydrogenL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(hydrogen_writestring)
#define hydrogen_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(hydrogen_writeline)
#define hydrogen_writeline()        (hydrogen_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(hydrogen_writestringerror)
#define hydrogen_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(HYDROGEN_COMPAT_APIINTCASTS)

#define hydrogenL_checkunsigned(L,a)	((hydrogen_Unsigned)hydrogenL_checkinteger(L,a))
#define hydrogenL_optunsigned(L,a,d)	\
	((hydrogen_Unsigned)hydrogenL_optinteger(L,a,(hydrogen_Integer)(d)))

#define hydrogenL_checkint(L,n)	((int)hydrogenL_checkinteger(L, (n)))
#define hydrogenL_optint(L,n,d)	((int)hydrogenL_optinteger(L, (n), (d)))

#define hydrogenL_checklong(L,n)	((long)hydrogenL_checkinteger(L, (n)))
#define hydrogenL_optlong(L,n,d)	((long)hydrogenL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif