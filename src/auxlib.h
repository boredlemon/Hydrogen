/*
** $Id: auxlib.h $
** Auxiliary functions for building Viper libraries
** See Copyright Notice in viper.h
*/


#ifndef auxlib_h
#define auxlib_h


#include <stddef.h>
#include <stdio.h>

#include "viperconf.h"
#include "viper.h"


/* global table */
#define VIPER_GNAME	"_G"


typedef struct viperL_Buffer viperL_Buffer;


/* extra error code for 'viperL_loadfilex' */
#define VIPER_ERRFILE     (VIPER_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define VIPER_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define VIPER_PRELOAD_TABLE	"_PRELOAD"


typedef struct viperL_Reg {
  const char *name;
  viper_CFunction func;
} viperL_Reg;


#define VIPERL_NUMSIZES	(sizeof(viper_Integer)*16 + sizeof(viper_Number))

VIPERLIB_API void (viperL_checkversion_) (viper_State *L, viper_Number ver, size_t sz);
#define viperL_checkversion(L)  \
	  viperL_checkversion_(L, VIPER_VERSION_NUM, VIPERL_NUMSIZES)

VIPERLIB_API int (viperL_getmetafield) (viper_State *L, int obj, const char *e);
VIPERLIB_API int (viperL_callmeta) (viper_State *L, int obj, const char *e);
VIPERLIB_API const char *(viperL_tolstring) (viper_State *L, int idx, size_t *len);
VIPERLIB_API int (viperL_argerror) (viper_State *L, int arg, const char *extramsg);
VIPERLIB_API int (viperL_typeerror) (viper_State *L, int arg, const char *tname);
VIPERLIB_API const char *(viperL_checklstring) (viper_State *L, int arg,
                                                          size_t *l);
VIPERLIB_API const char *(viperL_optlstring) (viper_State *L, int arg,
                                          const char *def, size_t *l);
VIPERLIB_API viper_Number (viperL_checknumber) (viper_State *L, int arg);
VIPERLIB_API viper_Number (viperL_optnumber) (viper_State *L, int arg, viper_Number def);

VIPERLIB_API viper_Integer (viperL_checkinteger) (viper_State *L, int arg);
VIPERLIB_API viper_Integer (viperL_optinteger) (viper_State *L, int arg,
                                          viper_Integer def);

VIPERLIB_API void (viperL_checkstack) (viper_State *L, int sz, const char *msg);
VIPERLIB_API void (viperL_checktype) (viper_State *L, int arg, int t);
VIPERLIB_API void (viperL_checkany) (viper_State *L, int arg);

VIPERLIB_API int   (viperL_newmetatable) (viper_State *L, const char *tname);
VIPERLIB_API void  (viperL_setmetatable) (viper_State *L, const char *tname);
VIPERLIB_API void *(viperL_testudata) (viper_State *L, int ud, const char *tname);
VIPERLIB_API void *(viperL_checkudata) (viper_State *L, int ud, const char *tname);

VIPERLIB_API void (viperL_where) (viper_State *L, int lvl);
VIPERLIB_API int (viperL_error) (viper_State *L, const char *fmt, ...);

VIPERLIB_API int (viperL_checkoption) (viper_State *L, int arg, const char *def,
                                   const char *const lst[]);

VIPERLIB_API int (viperL_fileresult) (viper_State *L, int stat, const char *fname);
VIPERLIB_API int (viperL_execresult) (viper_State *L, int stat);


/* predefined references */
#define VIPER_NOREF       (-2)
#define VIPER_REFNIL      (-1)

VIPERLIB_API int (viperL_ref) (viper_State *L, int t);
VIPERLIB_API void (viperL_unref) (viper_State *L, int t, int ref);

VIPERLIB_API int (viperL_loadfilex) (viper_State *L, const char *filename,
                                               const char *mode);

#define viperL_loadfile(L,f)	viperL_loadfilex(L,f,NULL)

VIPERLIB_API int (viperL_loadbufferx) (viper_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
VIPERLIB_API int (viperL_loadstring) (viper_State *L, const char *s);

VIPERLIB_API viper_State *(viperL_newstate) (void);

VIPERLIB_API viper_Integer (viperL_len) (viper_State *L, int idx);

VIPERLIB_API void (viperL_addgsub) (viperL_Buffer *b, const char *s,
                                     const char *p, const char *r);
VIPERLIB_API const char *(viperL_gsub) (viper_State *L, const char *s,
                                    const char *p, const char *r);

VIPERLIB_API void (viperL_setfuncs) (viper_State *L, const viperL_Reg *l, int nup);

VIPERLIB_API int (viperL_getsubtable) (viper_State *L, int idx, const char *fname);

VIPERLIB_API void (viperL_traceback) (viper_State *L, viper_State *L1,
                                  const char *msg, int level);

VIPERLIB_API void (viperL_requiref) (viper_State *L, const char *modname,
                                 viper_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define viperL_newlibtable(L,l)	\
  viper_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define viperL_newlib(L,l)  \
  (viperL_checkversion(L), viperL_newlibtable(L,l), viperL_setfuncs(L,l,0))

#define viperL_argcheck(L, cond,arg,extramsg)	\
	((void)(viperi_likely(cond) || viperL_argerror(L, (arg), (extramsg))))

#define viperL_argexpected(L,cond,arg,tname)	\
	((void)(viperi_likely(cond) || viperL_typeerror(L, (arg), (tname))))

#define viperL_checkstring(L,n)	(viperL_checklstring(L, (n), NULL))
#define viperL_optstring(L,n,d)	(viperL_optlstring(L, (n), (d), NULL))

#define viperL_typename(L,i)	viper_typename(L, viper_type(L,(i)))

#define viperL_dofile(L, fn) \
	(viperL_loadfile(L, fn) || viper_pcall(L, 0, VIPER_MULTRET, 0))

#define viperL_dostring(L, s) \
	(viperL_loadstring(L, s) || viper_pcall(L, 0, VIPER_MULTRET, 0))

#define viperL_getmetatable(L,n)	(viper_getfield(L, VIPER_REGISTRYINDEX, (n)))

#define viperL_opt(L,f,n,d)	(viper_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define viperL_loadbuffer(L,s,sz,n)	viperL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on viper_Integer values with wrap-around
** semantics, as the Viper core does.
*/
#define viperL_intop(op,v1,v2)  \
	((viper_Integer)((viper_Unsigned)(v1) op (viper_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define viperL_pushfail(L)	viper_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(viper_assert)

#if defined VIPERI_ASSERT
  #include <assert.h>
  #define viper_assert(c)		assert(c)
#else
  #define viper_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct viperL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  viper_State *L;
  union {
    VIPERI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[VIPERL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define viperL_bufflen(bf)	((bf)->n)
#define viperL_buffaddr(bf)	((bf)->b)


#define viperL_addchar(B,c) \
  ((void)((B)->n < (B)->size || viperL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define viperL_addsize(B,s)	((B)->n += (s))

#define viperL_buffsub(B,s)	((B)->n -= (s))

VIPERLIB_API void (viperL_buffinit) (viper_State *L, viperL_Buffer *B);
VIPERLIB_API char *(viperL_prepbuffsize) (viperL_Buffer *B, size_t sz);
VIPERLIB_API void (viperL_addlstring) (viperL_Buffer *B, const char *s, size_t l);
VIPERLIB_API void (viperL_addstring) (viperL_Buffer *B, const char *s);
VIPERLIB_API void (viperL_addvalue) (viperL_Buffer *B);
VIPERLIB_API void (viperL_pushresult) (viperL_Buffer *B);
VIPERLIB_API void (viperL_pushresultsize) (viperL_Buffer *B, size_t sz);
VIPERLIB_API char *(viperL_buffinitsize) (viper_State *L, viperL_Buffer *B, size_t sz);

#define viperL_prepbuffer(B)	viperL_prepbuffsize(B, VIPERL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'VIPER_FILEHANDLE' and
** initial structure 'viperL_Stream' (it may contain other fields
** after that initial structure).
*/

#define VIPER_FILEHANDLE          "FILE*"


typedef struct viperL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  viper_CFunction closef;  /* to close stream (NULL for closed streams) */
} viperL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(viper_writestring)
#define viper_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(viper_writeline)
#define viper_writeline()        (viper_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(viper_writestringerror)
#define viper_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(VIPER_COMPAT_APIINTCASTS)

#define viperL_checkunsigned(L,a)	((viper_Unsigned)viperL_checkinteger(L,a))
#define viperL_optunsigned(L,a,d)	\
	((viper_Unsigned)viperL_optinteger(L,a,(viper_Integer)(d)))

#define viperL_checkint(L,n)	((int)viperL_checkinteger(L, (n)))
#define viperL_optint(L,n,d)	((int)viperL_optinteger(L, (n), (d)))

#define viperL_checklong(L,n)	((long)viperL_checkinteger(L, (n)))
#define viperL_optlong(L,n,d)	((long)viperL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif