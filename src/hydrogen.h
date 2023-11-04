

#ifndef hydrogen_h
#define hydrogen_h

#include <stdarg.h>
#include <stddef.h>


#include "hydrogenconf.h"


#define HYDROGEN_VERSION_MAJOR	"9"
#define HYDROGEN_VERSION_MINOR	"1"
#define HYDROGEN_VERSION_RELEASE	"1"

#define HYDROGEN_VERSION_NUM			504
#define HYDROGEN_VERSION_RELEASE_NUM		(HYDROGEN_VERSION_NUM * 100 + 4)

#define HYDROGEN_VERSION	"Hydrogen " HYDROGEN_VERSION_MAJOR "." HYDROGEN_VERSION_MINOR
#define HYDROGEN_RELEASE	HYDROGEN_VERSION "." HYDROGEN_VERSION_RELEASE
#define HYDROGEN_COPYRIGHT	HYDROGEN_RELEASE "  Made by Toast"
#define HYDROGEN_AUTHORS	"made by Toast"


/* mark for precompiled code ('<esc>Hydrogen') */
#define HYDROGEN_SIGNATURE	"\x1b"

/* option for multiple returns in 'hydrogen_pcall' and 'hydrogen_call' */
#define HYDROGEN_MULTRET	(-1)


/*
** Pseudo-indices
** (-HYDROGENI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define HYDROGEN_REGISTRYINDEX	(-HYDROGENI_MAXSTACK - 1000)
#define hydrogen_upvalueindex(i)	(HYDROGEN_REGISTRYINDEX - (i))


/* thread status */
#define HYDROGEN_OK		0
#define HYDROGEN_YIELD	1
#define HYDROGEN_ERRRUN	2
#define HYDROGEN_ERRSYNTAX	3
#define HYDROGEN_ERRMEM	4
#define HYDROGEN_ERRERR	5


typedef struct hydrogen_State hydrogen_State;


/*
** basic types
*/
#define HYDROGEN_TNONE		(-1)

#define HYDROGEN_TNIL		0
#define HYDROGEN_TBOOLEAN		1
#define HYDROGEN_TLIGHTUSERDATA	2
#define HYDROGEN_TNUMBER		3
#define HYDROGEN_TSTRING		4
#define HYDROGEN_TTABLE		5
#define HYDROGEN_TFUNCTION		6
#define HYDROGEN_TUSERDATA		7
#define HYDROGEN_TTHREAD		8

#define HYDROGEN_NUMTYPES		9



/* minimum Hydrogen stack available to a C function */
#define HYDROGEN_MINSTACK	20


/* predefined values in the registry */
#define HYDROGEN_RIDX_MAINTHREAD	1
#define HYDROGEN_RIDX_GLOBALS	2
#define HYDROGEN_RIDX_LAST		HYDROGEN_RIDX_GLOBALS


/* type of numbers in Hydrogen */
typedef HYDROGEN_NUMBER hydrogen_Number;


/* type for integer functions */
typedef HYDROGEN_INTEGER hydrogen_Integer;

/* unsigned integer type */
typedef HYDROGEN_UNSIGNED hydrogen_Unsigned;

/* type for continuation-function contexts */
typedef HYDROGEN_KCONTEXT hydrogen_KContext;


/*
** Type for C functions registered with Hydrogen
*/
typedef int (*hydrogen_CFunction) (hydrogen_State *L);

/*
** Type for continuation functions
*/
typedef int (*hydrogen_KFunction) (hydrogen_State *L, int status, hydrogen_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Hydrogen chunks
*/
typedef const char * (*hydrogen_Reader) (hydrogen_State *L, void *ud, size_t *sz);

typedef int (*hydrogen_Writer) (hydrogen_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*hydrogen_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*hydrogen_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(HYDROGEN_USER_H)
#include HYDROGEN_USER_H
#endif


/*
** RCS ident string
*/
extern const char hydrogen_ident[];


/*
** state manipulation
*/
HYDROGEN_API hydrogen_State *(hydrogen_newstate) (hydrogen_Alloc f, void *ud);
HYDROGEN_API void       (hydrogen_close) (hydrogen_State *L);
HYDROGEN_API hydrogen_State *(hydrogen_newthread) (hydrogen_State *L);
HYDROGEN_API int        (hydrogen_resetthread) (hydrogen_State *L);

HYDROGEN_API hydrogen_CFunction (hydrogen_atpanic) (hydrogen_State *L, hydrogen_CFunction panicf);


HYDROGEN_API hydrogen_Number (hydrogen_version) (hydrogen_State *L);


/*
** basic stack manipulation
*/
HYDROGEN_API int   (hydrogen_absindex) (hydrogen_State *L, int idx);
HYDROGEN_API int   (hydrogen_gettop) (hydrogen_State *L);
HYDROGEN_API void  (hydrogen_settop) (hydrogen_State *L, int idx);
HYDROGEN_API void  (hydrogen_pushvalue) (hydrogen_State *L, int idx);
HYDROGEN_API void  (hydrogen_rotate) (hydrogen_State *L, int idx, int n);
HYDROGEN_API void  (hydrogen_copy) (hydrogen_State *L, int fromidx, int toidx);
HYDROGEN_API int   (hydrogen_checkstack) (hydrogen_State *L, int n);

HYDROGEN_API void  (hydrogen_xmove) (hydrogen_State *from, hydrogen_State *to, int n);


/*
** access functions (stack -> C)
*/

HYDROGEN_API int             (hydrogen_isnumber) (hydrogen_State *L, int idx);
HYDROGEN_API int             (hydrogen_isstring) (hydrogen_State *L, int idx);
HYDROGEN_API int             (hydrogen_iscfunction) (hydrogen_State *L, int idx);
HYDROGEN_API int             (hydrogen_isinteger) (hydrogen_State *L, int idx);
HYDROGEN_API int             (hydrogen_isuserdata) (hydrogen_State *L, int idx);
HYDROGEN_API int             (hydrogen_type) (hydrogen_State *L, int idx);
HYDROGEN_API const char     *(hydrogen_typename) (hydrogen_State *L, int tp);

HYDROGEN_API hydrogen_Number      (hydrogen_tonumberx) (hydrogen_State *L, int idx, int *isnum);
HYDROGEN_API hydrogen_Integer     (hydrogen_tointegerx) (hydrogen_State *L, int idx, int *isnum);
HYDROGEN_API int             (hydrogen_toboolean) (hydrogen_State *L, int idx);
HYDROGEN_API const char     *(hydrogen_tolstring) (hydrogen_State *L, int idx, size_t *len);
HYDROGEN_API hydrogen_Unsigned    (hydrogen_rawlen) (hydrogen_State *L, int idx);
HYDROGEN_API hydrogen_CFunction   (hydrogen_tocfunction) (hydrogen_State *L, int idx);
HYDROGEN_API void	       *(hydrogen_touserdata) (hydrogen_State *L, int idx);
HYDROGEN_API hydrogen_State      *(hydrogen_tothread) (hydrogen_State *L, int idx);
HYDROGEN_API const void     *(hydrogen_topointer) (hydrogen_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define HYDROGEN_OPADD	0	/* ORDER TM, ORDER OP */
#define HYDROGEN_OPSUB	1
#define HYDROGEN_OPMUL	2
#define HYDROGEN_OPMOD	3
#define HYDROGEN_OPPOW	4
#define HYDROGEN_OPDIV	5
#define HYDROGEN_OPIDIV	6
#define HYDROGEN_OPBAND	7
#define HYDROGEN_OPBOR	8
#define HYDROGEN_OPBXOR	9
#define HYDROGEN_OPSHL	10
#define HYDROGEN_OPSHR	11
#define HYDROGEN_OPUNM	12
#define HYDROGEN_OPBNOT	13

HYDROGEN_API void  (hydrogen_arith) (hydrogen_State *L, int op);

#define HYDROGEN_OPEQ	0
#define HYDROGEN_OPLT	1
#define HYDROGEN_OPLE	2

HYDROGEN_API int   (hydrogen_rawequal) (hydrogen_State *L, int idx1, int idx2);
HYDROGEN_API int   (hydrogen_compare) (hydrogen_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
HYDROGEN_API void        (hydrogen_pushnil) (hydrogen_State *L);
HYDROGEN_API void        (hydrogen_pushnumber) (hydrogen_State *L, hydrogen_Number n);
HYDROGEN_API void        (hydrogen_pushinteger) (hydrogen_State *L, hydrogen_Integer n);
HYDROGEN_API const char *(hydrogen_pushlstring) (hydrogen_State *L, const char *s, size_t len);
HYDROGEN_API const char *(hydrogen_pushstring) (hydrogen_State *L, const char *s);
HYDROGEN_API const char *(hydrogen_pushvfstring) (hydrogen_State *L, const char *fmt,
                                                      va_list argp);
HYDROGEN_API const char *(hydrogen_pushfstring) (hydrogen_State *L, const char *fmt, ...);
HYDROGEN_API void  (hydrogen_pushcclosure) (hydrogen_State *L, hydrogen_CFunction fn, int n);
HYDROGEN_API void  (hydrogen_pushboolean) (hydrogen_State *L, int b);
HYDROGEN_API void  (hydrogen_pushlightuserdata) (hydrogen_State *L, void *p);
HYDROGEN_API int   (hydrogen_pushthread) (hydrogen_State *L);


/*
** get functions (Hydrogen -> stack)
*/
HYDROGEN_API int (hydrogen_getglobal) (hydrogen_State *L, const char *name);
HYDROGEN_API int (hydrogen_gettable) (hydrogen_State *L, int idx);
HYDROGEN_API int (hydrogen_getfield) (hydrogen_State *L, int idx, const char *k);
HYDROGEN_API int (hydrogen_geti) (hydrogen_State *L, int idx, hydrogen_Integer n);
HYDROGEN_API int (hydrogen_rawget) (hydrogen_State *L, int idx);
HYDROGEN_API int (hydrogen_rawgeti) (hydrogen_State *L, int idx, hydrogen_Integer n);
HYDROGEN_API int (hydrogen_rawgetp) (hydrogen_State *L, int idx, const void *p);

HYDROGEN_API void  (hydrogen_createtable) (hydrogen_State *L, int narr, int nrec);
HYDROGEN_API void *(hydrogen_newuserdatauv) (hydrogen_State *L, size_t sz, int nuvalue);
HYDROGEN_API int   (hydrogen_getmetatable) (hydrogen_State *L, int objindex);
HYDROGEN_API int  (hydrogen_getiuservalue) (hydrogen_State *L, int idx, int n);


/*
** set functions (stack -> Hydrogen)
*/
HYDROGEN_API void  (hydrogen_setglobal) (hydrogen_State *L, const char *name);
HYDROGEN_API void  (hydrogen_settable) (hydrogen_State *L, int idx);
HYDROGEN_API void  (hydrogen_setfield) (hydrogen_State *L, int idx, const char *k);
HYDROGEN_API void  (hydrogen_seti) (hydrogen_State *L, int idx, hydrogen_Integer n);
HYDROGEN_API void  (hydrogen_rawset) (hydrogen_State *L, int idx);
HYDROGEN_API void  (hydrogen_rawseti) (hydrogen_State *L, int idx, hydrogen_Integer n);
HYDROGEN_API void  (hydrogen_rawsetp) (hydrogen_State *L, int idx, const void *p);
HYDROGEN_API int   (hydrogen_setmetatable) (hydrogen_State *L, int objindex);
HYDROGEN_API int   (hydrogen_setiuservalue) (hydrogen_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Hydrogen code)
*/
HYDROGEN_API void  (hydrogen_callk) (hydrogen_State *L, int nargs, int nresults,
                           hydrogen_KContext ctx, hydrogen_KFunction k);
#define hydrogen_call(L,n,r)		hydrogen_callk(L, (n), (r), 0, NULL)

HYDROGEN_API int   (hydrogen_pcallk) (hydrogen_State *L, int nargs, int nresults, int errfunc,
                            hydrogen_KContext ctx, hydrogen_KFunction k);
#define hydrogen_pcall(L,n,r,f)	hydrogen_pcallk(L, (n), (r), (f), 0, NULL)

HYDROGEN_API int   (hydrogen_load) (hydrogen_State *L, hydrogen_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

HYDROGEN_API int (hydrogen_dump) (hydrogen_State *L, hydrogen_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
HYDROGEN_API int  (hydrogen_yieldk)     (hydrogen_State *L, int nresults, hydrogen_KContext ctx,
                               hydrogen_KFunction k);
HYDROGEN_API int  (hydrogen_resume)     (hydrogen_State *L, hydrogen_State *from, int narg,
                               int *nres);
HYDROGEN_API int  (hydrogen_status)     (hydrogen_State *L);
HYDROGEN_API int (hydrogen_isyieldable) (hydrogen_State *L);

#define hydrogen_yield(L,n)		hydrogen_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
HYDROGEN_API void (hydrogen_setwarnf) (hydrogen_State *L, hydrogen_WarnFunction f, void *ud);
HYDROGEN_API void (hydrogen_warning)  (hydrogen_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define HYDROGEN_GCSTOP		0
#define HYDROGEN_GCRESTART		1
#define HYDROGEN_GCCOLLECT		2
#define HYDROGEN_GCCOUNT		3
#define HYDROGEN_GCCOUNTB		4
#define HYDROGEN_GCSTEP		5
#define HYDROGEN_GCSETPAUSE		6
#define HYDROGEN_GCSETSTEPMUL	7
#define HYDROGEN_GCISRUNNING		9
#define HYDROGEN_GCGEN		10
#define HYDROGEN_GCINC		11

HYDROGEN_API int (hydrogen_gc) (hydrogen_State *L, int what, ...);


/*
** miscellaneous functions
*/

HYDROGEN_API int   (hydrogen_error) (hydrogen_State *L);

HYDROGEN_API int   (hydrogen_next) (hydrogen_State *L, int idx);

HYDROGEN_API void  (hydrogen_concat) (hydrogen_State *L, int n);
HYDROGEN_API void  (hydrogen_len)    (hydrogen_State *L, int idx);

HYDROGEN_API size_t   (hydrogen_stringtonumber) (hydrogen_State *L, const char *s);

HYDROGEN_API hydrogen_Alloc (hydrogen_getallocf) (hydrogen_State *L, void **ud);
HYDROGEN_API void      (hydrogen_setallocf) (hydrogen_State *L, hydrogen_Alloc f, void *ud);

HYDROGEN_API void (hydrogen_toclose) (hydrogen_State *L, int idx);
HYDROGEN_API void (hydrogen_closeslot) (hydrogen_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define hydrogen_getextraspace(L)	((void *)((char *)(L) - HYDROGEN_EXTRASPACE))

#define hydrogen_tonumber(L,i)	hydrogen_tonumberx(L,(i),NULL)
#define hydrogen_tointeger(L,i)	hydrogen_tointegerx(L,(i),NULL)

#define hydrogen_pop(L,n)		hydrogen_settop(L, -(n)-1)

#define hydrogen_newtable(L)		hydrogen_createtable(L, 0, 0)

#define hydrogen_register(L,n,f) (hydrogen_pushcfunction(L, (f)), hydrogen_setglobal(L, (n)))

#define hydrogen_pushcfunction(L,f)	hydrogen_pushcclosure(L, (f), 0)

#define hydrogen_isfunction(L,n)	(hydrogen_type(L, (n)) == HYDROGEN_TFUNCTION)
#define hydrogen_istable(L,n)	(hydrogen_type(L, (n)) == HYDROGEN_TTABLE)
#define hydrogen_islightuserdata(L,n)	(hydrogen_type(L, (n)) == HYDROGEN_TLIGHTUSERDATA)
#define hydrogen_isnil(L,n)		(hydrogen_type(L, (n)) == HYDROGEN_TNIL)
#define hydrogen_isboolean(L,n)	(hydrogen_type(L, (n)) == HYDROGEN_TBOOLEAN)
#define hydrogen_isthread(L,n)	(hydrogen_type(L, (n)) == HYDROGEN_TTHREAD)
#define hydrogen_isnone(L,n)		(hydrogen_type(L, (n)) == HYDROGEN_TNONE)
#define hydrogen_isnoneornil(L, n)	(hydrogen_type(L, (n)) <= 0)

#define hydrogen_pushliteral(L, s)	hydrogen_pushstring(L, "" s)

#define hydrogen_pushglobaltable(L)  \
	((void)hydrogen_rawgeti(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_RIDX_GLOBALS))

#define hydrogen_tostring(L,i)	hydrogen_tolstring(L, (i), NULL)


#define hydrogen_insert(L,idx)	hydrogen_rotate(L, (idx), 1)

#define hydrogen_remove(L,idx)	(hydrogen_rotate(L, (idx), -1), hydrogen_pop(L, 1))

#define hydrogen_replace(L,idx)	(hydrogen_copy(L, -1, (idx)), hydrogen_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(HYDROGEN_COMPAT_APIINTCASTS)

#define hydrogen_pushunsigned(L,n)	hydrogen_pushinteger(L, (hydrogen_Integer)(n))
#define hydrogen_tounsignedx(L,i,is)	((hydrogen_Unsigned)hydrogen_tointegerx(L,i,is))
#define hydrogen_tounsigned(L,i)	hydrogen_tounsignedx(L,(i),NULL)

#endif

#define hydrogen_newuserdata(L,s)	hydrogen_newuserdatauv(L,s,1)
#define hydrogen_getuservalue(L,idx)	hydrogen_getiuservalue(L,idx,1)
#define hydrogen_setuservalue(L,idx)	hydrogen_setiuservalue(L,idx,1)

#define HYDROGEN_NUMTAGS		HYDROGEN_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define HYDROGEN_HOOKCALL	0
#define HYDROGEN_HOOKRET	1
#define HYDROGEN_HOOKLINE	2
#define HYDROGEN_HOOKCOUNT	3
#define HYDROGEN_HOOKTAILCALL 4


/*
** Event masks
*/
#define HYDROGEN_MASKCALL	(1 << HYDROGEN_HOOKCALL)
#define HYDROGEN_MASKRET	(1 << HYDROGEN_HOOKRET)
#define HYDROGEN_MASKLINE	(1 << HYDROGEN_HOOKLINE)
#define HYDROGEN_MASKCOUNT	(1 << HYDROGEN_HOOKCOUNT)

typedef struct hydrogen_Debug hydrogen_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*hydrogen_Hook) (hydrogen_State *L, hydrogen_Debug *ar);


HYDROGEN_API int (hydrogen_getstack) (hydrogen_State *L, int level, hydrogen_Debug *ar);
HYDROGEN_API int (hydrogen_getinfo) (hydrogen_State *L, const char *what, hydrogen_Debug *ar);
HYDROGEN_API const char *(hydrogen_getlocal) (hydrogen_State *L, const hydrogen_Debug *ar, int n);
HYDROGEN_API const char *(hydrogen_setlocal) (hydrogen_State *L, const hydrogen_Debug *ar, int n);
HYDROGEN_API const char *(hydrogen_getupvalue) (hydrogen_State *L, int funcindex, int n);
HYDROGEN_API const char *(hydrogen_setupvalue) (hydrogen_State *L, int funcindex, int n);

HYDROGEN_API void *(hydrogen_upvalueid) (hydrogen_State *L, int fidx, int n);
HYDROGEN_API void  (hydrogen_upvaluejoin) (hydrogen_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

HYDROGEN_API void (hydrogen_sethook) (hydrogen_State *L, hydrogen_Hook func, int mask, int count);
HYDROGEN_API hydrogen_Hook (hydrogen_gethook) (hydrogen_State *L);
HYDROGEN_API int (hydrogen_gethookmask) (hydrogen_State *L);
HYDROGEN_API int (hydrogen_gethookcount) (hydrogen_State *L);

HYDROGEN_API int (hydrogen_setcstacklimit) (hydrogen_State *L, unsigned int limit);

struct hydrogen_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Hydrogen', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[HYDROGEN_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};
#endif