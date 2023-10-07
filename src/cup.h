

#ifndef cup_h
#define cup_h

#include <stdarg.h>
#include <stddef.h>


#include "cupconf.h"


#define CUP_VERSION_MAJOR	"0"
#define CUP_VERSION_MINOR	"1"
#define CUP_VERSION_RELEASE	"0"

#define CUP_VERSION_NUM			504
#define CUP_VERSION_RELEASE_NUM		(CUP_VERSION_NUM * 100 + 4)

#define CUP_VERSION	"Cup " CUP_VERSION_MAJOR "." CUP_VERSION_MINOR
#define CUP_RELEASE	CUP_VERSION "." CUP_VERSION_RELEASE
#define CUP_COPYRIGHT	CUP_RELEASE "  made by Coffee or Dolphin#6086"
#define CUP_AUTHORS	"made by Coffee/Dolphin from discord"


/* mark for precompiled code ('<esc>Cup') */
#define CUP_SIGNATURE	"\x1b"

/* option for multiple returns in 'cup_pcall' and 'cup_call' */
#define CUP_MULTRET	(-1)


/*
** Pseudo-indices
** (-CUPI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define CUP_REGISTRYINDEX	(-CUPI_MAXSTACK - 1000)
#define cup_upvalueindex(i)	(CUP_REGISTRYINDEX - (i))


/* thread status */
#define CUP_OK		0
#define CUP_YIELD	1
#define CUP_ERRRUN	2
#define CUP_ERRSYNTAX	3
#define CUP_ERRMEM	4
#define CUP_ERRERR	5


typedef struct cup_State cup_State;


/*
** basic types
*/
#define CUP_TNONE		(-1)

#define CUP_TNIL		0
#define CUP_TBOOLEAN		1
#define CUP_TLIGHTUSERDATA	2
#define CUP_TNUMBER		3
#define CUP_TSTRING		4
#define CUP_TTABLE		5
#define CUP_TFUNCTION		6
#define CUP_TUSERDATA		7
#define CUP_TTHREAD		8

#define CUP_NUMTYPES		9



/* minimum Cup stack available to a C function */
#define CUP_MINSTACK	20


/* predefined values in the registry */
#define CUP_RIDX_MAINTHREAD	1
#define CUP_RIDX_GLOBALS	2
#define CUP_RIDX_LAST		CUP_RIDX_GLOBALS


/* type of numbers in Cup */
typedef CUP_NUMBER cup_Number;


/* type for integer functions */
typedef CUP_INTEGER cup_Integer;

/* unsigned integer type */
typedef CUP_UNSIGNED cup_Unsigned;

/* type for continuation-function contexts */
typedef CUP_KCONTEXT cup_KContext;


/*
** Type for C functions registered with Cup
*/
typedef int (*cup_CFunction) (cup_State *L);

/*
** Type for continuation functions
*/
typedef int (*cup_KFunction) (cup_State *L, int status, cup_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Cup chunks
*/
typedef const char * (*cup_Reader) (cup_State *L, void *ud, size_t *sz);

typedef int (*cup_Writer) (cup_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*cup_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*cup_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(CUP_USER_H)
#include CUP_USER_H
#endif


/*
** RCS ident string
*/
extern const char cup_ident[];


/*
** state manipulation
*/
CUP_API cup_State *(cup_newstate) (cup_Alloc f, void *ud);
CUP_API void       (cup_close) (cup_State *L);
CUP_API cup_State *(cup_newthread) (cup_State *L);
CUP_API int        (cup_resetthread) (cup_State *L);

CUP_API cup_CFunction (cup_atpanic) (cup_State *L, cup_CFunction panicf);


CUP_API cup_Number (cup_version) (cup_State *L);


/*
** basic stack manipulation
*/
CUP_API int   (cup_absindex) (cup_State *L, int idx);
CUP_API int   (cup_gettop) (cup_State *L);
CUP_API void  (cup_settop) (cup_State *L, int idx);
CUP_API void  (cup_pushvalue) (cup_State *L, int idx);
CUP_API void  (cup_rotate) (cup_State *L, int idx, int n);
CUP_API void  (cup_copy) (cup_State *L, int fromidx, int toidx);
CUP_API int   (cup_checkstack) (cup_State *L, int n);

CUP_API void  (cup_xmove) (cup_State *from, cup_State *to, int n);


/*
** access functions (stack -> C)
*/

CUP_API int             (cup_isnumber) (cup_State *L, int idx);
CUP_API int             (cup_isstring) (cup_State *L, int idx);
CUP_API int             (cup_iscfunction) (cup_State *L, int idx);
CUP_API int             (cup_isinteger) (cup_State *L, int idx);
CUP_API int             (cup_isuserdata) (cup_State *L, int idx);
CUP_API int             (cup_type) (cup_State *L, int idx);
CUP_API const char     *(cup_typename) (cup_State *L, int tp);

CUP_API cup_Number      (cup_tonumberx) (cup_State *L, int idx, int *isnum);
CUP_API cup_Integer     (cup_tointegerx) (cup_State *L, int idx, int *isnum);
CUP_API int             (cup_toboolean) (cup_State *L, int idx);
CUP_API const char     *(cup_tolstring) (cup_State *L, int idx, size_t *len);
CUP_API cup_Unsigned    (cup_rawlen) (cup_State *L, int idx);
CUP_API cup_CFunction   (cup_tocfunction) (cup_State *L, int idx);
CUP_API void	       *(cup_touserdata) (cup_State *L, int idx);
CUP_API cup_State      *(cup_tothread) (cup_State *L, int idx);
CUP_API const void     *(cup_topointer) (cup_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define CUP_OPADD	0	/* ORDER TM, ORDER OP */
#define CUP_OPSUB	1
#define CUP_OPMUL	2
#define CUP_OPMOD	3
#define CUP_OPPOW	4
#define CUP_OPDIV	5
#define CUP_OPIDIV	6
#define CUP_OPBAND	7
#define CUP_OPBOR	8
#define CUP_OPBXOR	9
#define CUP_OPSHL	10
#define CUP_OPSHR	11
#define CUP_OPUNM	12
#define CUP_OPBNOT	13

CUP_API void  (cup_arith) (cup_State *L, int op);

#define CUP_OPEQ	0
#define CUP_OPLT	1
#define CUP_OPLE	2

CUP_API int   (cup_rawequal) (cup_State *L, int idx1, int idx2);
CUP_API int   (cup_compare) (cup_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
CUP_API void        (cup_pushnil) (cup_State *L);
CUP_API void        (cup_pushnumber) (cup_State *L, cup_Number n);
CUP_API void        (cup_pushinteger) (cup_State *L, cup_Integer n);
CUP_API const char *(cup_pushlstring) (cup_State *L, const char *s, size_t len);
CUP_API const char *(cup_pushstring) (cup_State *L, const char *s);
CUP_API const char *(cup_pushvfstring) (cup_State *L, const char *fmt,
                                                      va_list argp);
CUP_API const char *(cup_pushfstring) (cup_State *L, const char *fmt, ...);
CUP_API void  (cup_pushcclosure) (cup_State *L, cup_CFunction fn, int n);
CUP_API void  (cup_pushboolean) (cup_State *L, int b);
CUP_API void  (cup_pushlightuserdata) (cup_State *L, void *p);
CUP_API int   (cup_pushthread) (cup_State *L);


/*
** get functions (Cup -> stack)
*/
CUP_API int (cup_getglobal) (cup_State *L, const char *name);
CUP_API int (cup_gettable) (cup_State *L, int idx);
CUP_API int (cup_getfield) (cup_State *L, int idx, const char *k);
CUP_API int (cup_geti) (cup_State *L, int idx, cup_Integer n);
CUP_API int (cup_rawget) (cup_State *L, int idx);
CUP_API int (cup_rawgeti) (cup_State *L, int idx, cup_Integer n);
CUP_API int (cup_rawgetp) (cup_State *L, int idx, const void *p);

CUP_API void  (cup_createtable) (cup_State *L, int narr, int nrec);
CUP_API void *(cup_newuserdatauv) (cup_State *L, size_t sz, int nuvalue);
CUP_API int   (cup_getmetatable) (cup_State *L, int objindex);
CUP_API int  (cup_getiuservalue) (cup_State *L, int idx, int n);


/*
** set functions (stack -> Cup)
*/
CUP_API void  (cup_setglobal) (cup_State *L, const char *name);
CUP_API void  (cup_settable) (cup_State *L, int idx);
CUP_API void  (cup_setfield) (cup_State *L, int idx, const char *k);
CUP_API void  (cup_seti) (cup_State *L, int idx, cup_Integer n);
CUP_API void  (cup_rawset) (cup_State *L, int idx);
CUP_API void  (cup_rawseti) (cup_State *L, int idx, cup_Integer n);
CUP_API void  (cup_rawsetp) (cup_State *L, int idx, const void *p);
CUP_API int   (cup_setmetatable) (cup_State *L, int objindex);
CUP_API int   (cup_setiuservalue) (cup_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Cup code)
*/
CUP_API void  (cup_callk) (cup_State *L, int nargs, int nresults,
                           cup_KContext ctx, cup_KFunction k);
#define cup_call(L,n,r)		cup_callk(L, (n), (r), 0, NULL)

CUP_API int   (cup_pcallk) (cup_State *L, int nargs, int nresults, int errfunc,
                            cup_KContext ctx, cup_KFunction k);
#define cup_pcall(L,n,r,f)	cup_pcallk(L, (n), (r), (f), 0, NULL)

CUP_API int   (cup_load) (cup_State *L, cup_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

CUP_API int (cup_dump) (cup_State *L, cup_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
CUP_API int  (cup_yieldk)     (cup_State *L, int nresults, cup_KContext ctx,
                               cup_KFunction k);
CUP_API int  (cup_resume)     (cup_State *L, cup_State *from, int narg,
                               int *nres);
CUP_API int  (cup_status)     (cup_State *L);
CUP_API int (cup_isyieldable) (cup_State *L);

#define cup_yield(L,n)		cup_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
CUP_API void (cup_setwarnf) (cup_State *L, cup_WarnFunction f, void *ud);
CUP_API void (cup_warning)  (cup_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define CUP_GCSTOP		0
#define CUP_GCRESTART		1
#define CUP_GCCOLLECT		2
#define CUP_GCCOUNT		3
#define CUP_GCCOUNTB		4
#define CUP_GCSTEP		5
#define CUP_GCSETPAUSE		6
#define CUP_GCSETSTEPMUL	7
#define CUP_GCISRUNNING		9
#define CUP_GCGEN		10
#define CUP_GCINC		11

CUP_API int (cup_gc) (cup_State *L, int what, ...);


/*
** miscellaneous functions
*/

CUP_API int   (cup_error) (cup_State *L);

CUP_API int   (cup_next) (cup_State *L, int idx);

CUP_API void  (cup_concat) (cup_State *L, int n);
CUP_API void  (cup_len)    (cup_State *L, int idx);

CUP_API size_t   (cup_stringtonumber) (cup_State *L, const char *s);

CUP_API cup_Alloc (cup_getallocf) (cup_State *L, void **ud);
CUP_API void      (cup_setallocf) (cup_State *L, cup_Alloc f, void *ud);

CUP_API void (cup_toclose) (cup_State *L, int idx);
CUP_API void (cup_closeslot) (cup_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define cup_getextraspace(L)	((void *)((char *)(L) - CUP_EXTRASPACE))

#define cup_tonumber(L,i)	cup_tonumberx(L,(i),NULL)
#define cup_tointeger(L,i)	cup_tointegerx(L,(i),NULL)

#define cup_pop(L,n)		cup_settop(L, -(n)-1)

#define cup_newtable(L)		cup_createtable(L, 0, 0)

#define cup_register(L,n,f) (cup_pushcfunction(L, (f)), cup_setglobal(L, (n)))

#define cup_pushcfunction(L,f)	cup_pushcclosure(L, (f), 0)

#define cup_isfunction(L,n)	(cup_type(L, (n)) == CUP_TFUNCTION)
#define cup_istable(L,n)	(cup_type(L, (n)) == CUP_TTABLE)
#define cup_islightuserdata(L,n)	(cup_type(L, (n)) == CUP_TLIGHTUSERDATA)
#define cup_isnil(L,n)		(cup_type(L, (n)) == CUP_TNIL)
#define cup_isboolean(L,n)	(cup_type(L, (n)) == CUP_TBOOLEAN)
#define cup_isthread(L,n)	(cup_type(L, (n)) == CUP_TTHREAD)
#define cup_isnone(L,n)		(cup_type(L, (n)) == CUP_TNONE)
#define cup_isnoneornil(L, n)	(cup_type(L, (n)) <= 0)

#define cup_pushliteral(L, s)	cup_pushstring(L, "" s)

#define cup_pushglobaltable(L)  \
	((void)cup_rawgeti(L, CUP_REGISTRYINDEX, CUP_RIDX_GLOBALS))

#define cup_tostring(L,i)	cup_tolstring(L, (i), NULL)


#define cup_insert(L,idx)	cup_rotate(L, (idx), 1)

#define cup_remove(L,idx)	(cup_rotate(L, (idx), -1), cup_pop(L, 1))

#define cup_replace(L,idx)	(cup_copy(L, -1, (idx)), cup_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(CUP_COMPAT_APIINTCASTS)

#define cup_pushunsigned(L,n)	cup_pushinteger(L, (cup_Integer)(n))
#define cup_tounsignedx(L,i,is)	((cup_Unsigned)cup_tointegerx(L,i,is))
#define cup_tounsigned(L,i)	cup_tounsignedx(L,(i),NULL)

#endif

#define cup_newuserdata(L,s)	cup_newuserdatauv(L,s,1)
#define cup_getuservalue(L,idx)	cup_getiuservalue(L,idx,1)
#define cup_setuservalue(L,idx)	cup_setiuservalue(L,idx,1)

#define CUP_NUMTAGS		CUP_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define CUP_HOOKCALL	0
#define CUP_HOOKRET	1
#define CUP_HOOKLINE	2
#define CUP_HOOKCOUNT	3
#define CUP_HOOKTAILCALL 4


/*
** Event masks
*/
#define CUP_MASKCALL	(1 << CUP_HOOKCALL)
#define CUP_MASKRET	(1 << CUP_HOOKRET)
#define CUP_MASKLINE	(1 << CUP_HOOKLINE)
#define CUP_MASKCOUNT	(1 << CUP_HOOKCOUNT)

typedef struct cup_Debug cup_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*cup_Hook) (cup_State *L, cup_Debug *ar);


CUP_API int (cup_getstack) (cup_State *L, int level, cup_Debug *ar);
CUP_API int (cup_getinfo) (cup_State *L, const char *what, cup_Debug *ar);
CUP_API const char *(cup_getlocal) (cup_State *L, const cup_Debug *ar, int n);
CUP_API const char *(cup_setlocal) (cup_State *L, const cup_Debug *ar, int n);
CUP_API const char *(cup_getupvalue) (cup_State *L, int funcindex, int n);
CUP_API const char *(cup_setupvalue) (cup_State *L, int funcindex, int n);

CUP_API void *(cup_upvalueid) (cup_State *L, int fidx, int n);
CUP_API void  (cup_upvaluejoin) (cup_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

CUP_API void (cup_sethook) (cup_State *L, cup_Hook func, int mask, int count);
CUP_API cup_Hook (cup_gethook) (cup_State *L);
CUP_API int (cup_gethookmask) (cup_State *L);
CUP_API int (cup_gethookcount) (cup_State *L);

CUP_API int (cup_setcstacklimit) (cup_State *L, unsigned int limit);

struct cup_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Cup', 'C', 'main', 'tail' */
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
  char short_src[CUP_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};
#endif
