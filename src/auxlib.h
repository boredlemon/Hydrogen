/*
** $Id: auxlib.h $
** Auxiliary functions for building Nebula libraries
** See Copyright Notice in nebula.h
*/


#ifndef auxlib_h
#define auxlib_h


#include <stddef.h>
#include <stdio.h>

#include "nebulaconf.h"
#include "nebula.h"


/* global table */
#define NEBULA_GNAME	"_G"


typedef struct nebulaL_Buffer nebulaL_Buffer;


/* extra error code for 'nebulaL_loadfilex' */
#define NEBULA_ERRFILE     (NEBULA_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define NEBULA_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define NEBULA_PRELOAD_TABLE	"_PRELOAD"


typedef struct nebulaL_Reg {
  const char *name;
  nebula_CFunction func;
} nebulaL_Reg;


#define NEBULAL_NUMSIZES	(sizeof(nebula_Integer)*16 + sizeof(nebula_Number))

NEBULALIB_API void (nebulaL_checkversion_) (nebula_State *L, nebula_Number ver, size_t sz);
#define nebulaL_checkversion(L)  \
	  nebulaL_checkversion_(L, NEBULA_VERSION_NUM, NEBULAL_NUMSIZES)

NEBULALIB_API int (nebulaL_getmetafield) (nebula_State *L, int obj, const char *e);
NEBULALIB_API int (nebulaL_callmeta) (nebula_State *L, int obj, const char *e);
NEBULALIB_API const char *(nebulaL_tolstring) (nebula_State *L, int idx, size_t *len);
NEBULALIB_API int (nebulaL_argerror) (nebula_State *L, int arg, const char *extramsg);
NEBULALIB_API int (nebulaL_typeerror) (nebula_State *L, int arg, const char *tname);
NEBULALIB_API const char *(nebulaL_checklstring) (nebula_State *L, int arg,
                                                          size_t *l);
NEBULALIB_API const char *(nebulaL_optlstring) (nebula_State *L, int arg,
                                          const char *def, size_t *l);
NEBULALIB_API nebula_Number (nebulaL_checknumber) (nebula_State *L, int arg);
NEBULALIB_API nebula_Number (nebulaL_optnumber) (nebula_State *L, int arg, nebula_Number def);

NEBULALIB_API nebula_Integer (nebulaL_checkinteger) (nebula_State *L, int arg);
NEBULALIB_API nebula_Integer (nebulaL_optinteger) (nebula_State *L, int arg,
                                          nebula_Integer def);

NEBULALIB_API void (nebulaL_checkstack) (nebula_State *L, int sz, const char *msg);
NEBULALIB_API void (nebulaL_checktype) (nebula_State *L, int arg, int t);
NEBULALIB_API void (nebulaL_checkany) (nebula_State *L, int arg);

NEBULALIB_API int   (nebulaL_newmetatable) (nebula_State *L, const char *tname);
NEBULALIB_API void  (nebulaL_setmetatable) (nebula_State *L, const char *tname);
NEBULALIB_API void *(nebulaL_testudata) (nebula_State *L, int ud, const char *tname);
NEBULALIB_API void *(nebulaL_checkudata) (nebula_State *L, int ud, const char *tname);

NEBULALIB_API void (nebulaL_where) (nebula_State *L, int lvl);
NEBULALIB_API int (nebulaL_error) (nebula_State *L, const char *fmt, ...);

NEBULALIB_API int (nebulaL_checkoption) (nebula_State *L, int arg, const char *def,
                                   const char *const lst[]);

NEBULALIB_API int (nebulaL_fileresult) (nebula_State *L, int stat, const char *fname);
NEBULALIB_API int (nebulaL_execresult) (nebula_State *L, int stat);


/* predefined references */
#define NEBULA_NOREF       (-2)
#define NEBULA_REFNIL      (-1)

NEBULALIB_API int (nebulaL_ref) (nebula_State *L, int t);
NEBULALIB_API void (nebulaL_unref) (nebula_State *L, int t, int ref);

NEBULALIB_API int (nebulaL_loadfilex) (nebula_State *L, const char *filename,
                                               const char *mode);

#define nebulaL_loadfile(L,f)	nebulaL_loadfilex(L,f,NULL)

NEBULALIB_API int (nebulaL_loadbufferx) (nebula_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
NEBULALIB_API int (nebulaL_loadstring) (nebula_State *L, const char *s);

NEBULALIB_API nebula_State *(nebulaL_newstate) (void);

NEBULALIB_API nebula_Integer (nebulaL_len) (nebula_State *L, int idx);

NEBULALIB_API void (nebulaL_addgsub) (nebulaL_Buffer *b, const char *s,
                                     const char *p, const char *r);
NEBULALIB_API const char *(nebulaL_gsub) (nebula_State *L, const char *s,
                                    const char *p, const char *r);

NEBULALIB_API void (nebulaL_setfuncs) (nebula_State *L, const nebulaL_Reg *l, int nup);

NEBULALIB_API int (nebulaL_getsubtable) (nebula_State *L, int idx, const char *fname);

NEBULALIB_API void (nebulaL_traceback) (nebula_State *L, nebula_State *L1,
                                  const char *msg, int level);

NEBULALIB_API void (nebulaL_requiref) (nebula_State *L, const char *modname,
                                 nebula_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define nebulaL_newlibtable(L,l)	\
  nebula_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define nebulaL_newlib(L,l)  \
  (nebulaL_checkversion(L), nebulaL_newlibtable(L,l), nebulaL_setfuncs(L,l,0))

#define nebulaL_argcheck(L, cond,arg,extramsg)	\
	((void)(nebulai_likely(cond) || nebulaL_argerror(L, (arg), (extramsg))))

#define nebulaL_argexpected(L,cond,arg,tname)	\
	((void)(nebulai_likely(cond) || nebulaL_typeerror(L, (arg), (tname))))

#define nebulaL_checkstring(L,n)	(nebulaL_checklstring(L, (n), NULL))
#define nebulaL_optstring(L,n,d)	(nebulaL_optlstring(L, (n), (d), NULL))

#define nebulaL_typename(L,i)	nebula_typename(L, nebula_type(L,(i)))

#define nebulaL_dofile(L, fn) \
	(nebulaL_loadfile(L, fn) || nebula_pcall(L, 0, NEBULA_MULTRET, 0))

#define nebulaL_dostring(L, s) \
	(nebulaL_loadstring(L, s) || nebula_pcall(L, 0, NEBULA_MULTRET, 0))

#define nebulaL_getmetatable(L,n)	(nebula_getfield(L, NEBULA_REGISTRYINDEX, (n)))

#define nebulaL_opt(L,f,n,d)	(nebula_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define nebulaL_loadbuffer(L,s,sz,n)	nebulaL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on nebula_Integer values with wrap-around
** semantics, as the Nebula core does.
*/
#define nebulaL_intop(op,v1,v2)  \
	((nebula_Integer)((nebula_Unsigned)(v1) op (nebula_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define nebulaL_pushfail(L)	nebula_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(nebula_assert)

#if defined NEBULAI_ASSERT
  #include <assert.h>
  #define nebula_assert(c)		assert(c)
#else
  #define nebula_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct nebulaL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  nebula_State *L;
  union {
    NEBULAI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[NEBULAL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define nebulaL_bufflen(bf)	((bf)->n)
#define nebulaL_buffaddr(bf)	((bf)->b)


#define nebulaL_addchar(B,c) \
  ((void)((B)->n < (B)->size || nebulaL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define nebulaL_addsize(B,s)	((B)->n += (s))

#define nebulaL_buffsub(B,s)	((B)->n -= (s))

NEBULALIB_API void (nebulaL_buffinit) (nebula_State *L, nebulaL_Buffer *B);
NEBULALIB_API char *(nebulaL_prepbuffsize) (nebulaL_Buffer *B, size_t sz);
NEBULALIB_API void (nebulaL_addlstring) (nebulaL_Buffer *B, const char *s, size_t l);
NEBULALIB_API void (nebulaL_addstring) (nebulaL_Buffer *B, const char *s);
NEBULALIB_API void (nebulaL_addvalue) (nebulaL_Buffer *B);
NEBULALIB_API void (nebulaL_pushresult) (nebulaL_Buffer *B);
NEBULALIB_API void (nebulaL_pushresultsize) (nebulaL_Buffer *B, size_t sz);
NEBULALIB_API char *(nebulaL_buffinitsize) (nebula_State *L, nebulaL_Buffer *B, size_t sz);

#define nebulaL_prepbuffer(B)	nebulaL_prepbuffsize(B, NEBULAL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'NEBULA_FILEHANDLE' and
** initial structure 'nebulaL_Stream' (it may contain other fields
** after that initial structure).
*/

#define NEBULA_FILEHANDLE          "FILE*"


typedef struct nebulaL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  nebula_CFunction closef;  /* to close stream (NULL for closed streams) */
} nebulaL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(nebula_writestring)
#define nebula_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(nebula_writeline)
#define nebula_writeline()        (nebula_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(nebula_writestringerror)
#define nebula_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(NEBULA_COMPAT_APIINTCASTS)

#define nebulaL_checkunsigned(L,a)	((nebula_Unsigned)nebulaL_checkinteger(L,a))
#define nebulaL_optunsigned(L,a,d)	\
	((nebula_Unsigned)nebulaL_optinteger(L,a,(nebula_Integer)(d)))

#define nebulaL_checkint(L,n)	((int)nebulaL_checkinteger(L, (n)))
#define nebulaL_optint(L,n,d)	((int)nebulaL_optinteger(L, (n), (d)))

#define nebulaL_checklong(L,n)	((long)nebulaL_checkinteger(L, (n)))
#define nebulaL_optlong(L,n,d)	((long)nebulaL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif