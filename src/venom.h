

#ifndef venom_h
#define venom_h

#include <stdarg.h>
#include <stddef.h>


#include "venomconf.h"


#define VENOM_VERSION_MAJOR	"9"
#define VENOM_VERSION_MINOR	"1"
#define VENOM_VERSION_RELEASE	"1"

#define VENOM_VERSION_NUM			504
#define VENOM_VERSION_RELEASE_NUM		(VENOM_VERSION_NUM * 100 + 4)

#define VENOM_VERSION	"Venom " VENOM_VERSION_MAJOR "." VENOM_VERSION_MINOR
#define VENOM_RELEASE	VENOM_VERSION "." VENOM_VERSION_RELEASE
#define VENOM_COPYRIGHT	VENOM_RELEASE "  Made by Toast"
#define VENOM_AUTHORS	"made by Toast"


/* mark for precompiled code ('<esc>Venom') */
#define VENOM_SIGNATURE	"\x1b"

/* option for multiple returns in 'venom_pcall' and 'venom_call' */
#define VENOM_MULTRET	(-1)


/*
** Pseudo-indices
** (-VENOMI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define VENOM_REGISTRYINDEX	(-VENOMI_MAXSTACK - 1000)
#define venom_upvalueindex(i)	(VENOM_REGISTRYINDEX - (i))


/* thread status */
#define VENOM_OK		0
#define VENOM_YIELD	1
#define VENOM_ERRRUN	2
#define VENOM_ERRSYNTAX	3
#define VENOM_ERRMEM	4
#define VENOM_ERRERR	5


typedef struct venom_State venom_State;


/*
** basic types
*/
#define VENOM_TNONE		(-1)

#define VENOM_TNIL		0
#define VENOM_TBOOLEAN		1
#define VENOM_TLIGHTUSERDATA	2
#define VENOM_TNUMBER		3
#define VENOM_TSTRING		4
#define VENOM_TTABLE		5
#define VENOM_TFUNCTION		6
#define VENOM_TUSERDATA		7
#define VENOM_TTHREAD		8

#define VENOM_NUMTYPES		9



/* minimum Venom stack available to a C function */
#define VENOM_MINSTACK	20


/* predefined values in the registry */
#define VENOM_RIDX_MAINTHREAD	1
#define VENOM_RIDX_GLOBALS	2
#define VENOM_RIDX_LAST		VENOM_RIDX_GLOBALS


/* type of numbers in Venom */
typedef VENOM_NUMBER venom_Number;


/* type for integer functions */
typedef VENOM_INTEGER venom_Integer;

/* unsigned integer type */
typedef VENOM_UNSIGNED venom_Unsigned;

/* type for continuation-function contexts */
typedef VENOM_KCONTEXT venom_KContext;


/*
** Type for C functions registered with Venom
*/
typedef int (*venom_CFunction) (venom_State *L);

/*
** Type for continuation functions
*/
typedef int (*venom_KFunction) (venom_State *L, int status, venom_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Venom chunks
*/
typedef const char * (*venom_Reader) (venom_State *L, void *ud, size_t *sz);

typedef int (*venom_Writer) (venom_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*venom_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*venom_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(VENOM_USER_H)
#include VENOM_USER_H
#endif


/*
** RCS ident string
*/
extern const char venom_ident[];


/*
** state manipulation
*/
VENOM_API venom_State *(venom_newstate) (venom_Alloc f, void *ud);
VENOM_API void       (venom_close) (venom_State *L);
VENOM_API venom_State *(venom_newthread) (venom_State *L);
VENOM_API int        (venom_resetthread) (venom_State *L);

VENOM_API venom_CFunction (venom_atpanic) (venom_State *L, venom_CFunction panicf);


VENOM_API venom_Number (venom_version) (venom_State *L);


/*
** basic stack manipulation
*/
VENOM_API int   (venom_absindex) (venom_State *L, int idx);
VENOM_API int   (venom_gettop) (venom_State *L);
VENOM_API void  (venom_settop) (venom_State *L, int idx);
VENOM_API void  (venom_pushvalue) (venom_State *L, int idx);
VENOM_API void  (venom_rotate) (venom_State *L, int idx, int n);
VENOM_API void  (venom_copy) (venom_State *L, int fromidx, int toidx);
VENOM_API int   (venom_checkstack) (venom_State *L, int n);

VENOM_API void  (venom_xmove) (venom_State *from, venom_State *to, int n);


/*
** access functions (stack -> C)
*/

VENOM_API int             (venom_isnumber) (venom_State *L, int idx);
VENOM_API int             (venom_isstring) (venom_State *L, int idx);
VENOM_API int             (venom_iscfunction) (venom_State *L, int idx);
VENOM_API int             (venom_isinteger) (venom_State *L, int idx);
VENOM_API int             (venom_isuserdata) (venom_State *L, int idx);
VENOM_API int             (venom_type) (venom_State *L, int idx);
VENOM_API const char     *(venom_typename) (venom_State *L, int tp);

VENOM_API venom_Number      (venom_tonumberx) (venom_State *L, int idx, int *isnum);
VENOM_API venom_Integer     (venom_tointegerx) (venom_State *L, int idx, int *isnum);
VENOM_API int             (venom_toboolean) (venom_State *L, int idx);
VENOM_API const char     *(venom_tolstring) (venom_State *L, int idx, size_t *len);
VENOM_API venom_Unsigned    (venom_rawlen) (venom_State *L, int idx);
VENOM_API venom_CFunction   (venom_tocfunction) (venom_State *L, int idx);
VENOM_API void	       *(venom_touserdata) (venom_State *L, int idx);
VENOM_API venom_State      *(venom_tothread) (venom_State *L, int idx);
VENOM_API const void     *(venom_topointer) (venom_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define VENOM_OPADD	0	/* ORDER TM, ORDER OP */
#define VENOM_OPSUB	1
#define VENOM_OPMUL	2
#define VENOM_OPMOD	3
#define VENOM_OPPOW	4
#define VENOM_OPDIV	5
#define VENOM_OPIDIV	6
#define VENOM_OPBAND	7
#define VENOM_OPBOR	8
#define VENOM_OPBXOR	9
#define VENOM_OPSHL	10
#define VENOM_OPSHR	11
#define VENOM_OPUNM	12
#define VENOM_OPBNOT	13

VENOM_API void  (venom_arith) (venom_State *L, int op);

#define VENOM_OPEQ	0
#define VENOM_OPLT	1
#define VENOM_OPLE	2

VENOM_API int   (venom_rawequal) (venom_State *L, int idx1, int idx2);
VENOM_API int   (venom_compare) (venom_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
VENOM_API void        (venom_pushnil) (venom_State *L);
VENOM_API void        (venom_pushnumber) (venom_State *L, venom_Number n);
VENOM_API void        (venom_pushinteger) (venom_State *L, venom_Integer n);
VENOM_API const char *(venom_pushlstring) (venom_State *L, const char *s, size_t len);
VENOM_API const char *(venom_pushstring) (venom_State *L, const char *s);
VENOM_API const char *(venom_pushvfstring) (venom_State *L, const char *fmt,
                                                      va_list argp);
VENOM_API const char *(venom_pushfstring) (venom_State *L, const char *fmt, ...);
VENOM_API void  (venom_pushcclosure) (venom_State *L, venom_CFunction fn, int n);
VENOM_API void  (venom_pushboolean) (venom_State *L, int b);
VENOM_API void  (venom_pushlightuserdata) (venom_State *L, void *p);
VENOM_API int   (venom_pushthread) (venom_State *L);


/*
** get functions (Venom -> stack)
*/
VENOM_API int (venom_getglobal) (venom_State *L, const char *name);
VENOM_API int (venom_gettable) (venom_State *L, int idx);
VENOM_API int (venom_getfield) (venom_State *L, int idx, const char *k);
VENOM_API int (venom_geti) (venom_State *L, int idx, venom_Integer n);
VENOM_API int (venom_rawget) (venom_State *L, int idx);
VENOM_API int (venom_rawgeti) (venom_State *L, int idx, venom_Integer n);
VENOM_API int (venom_rawgetp) (venom_State *L, int idx, const void *p);

VENOM_API void  (venom_createtable) (venom_State *L, int narr, int nrec);
VENOM_API void *(venom_newuserdatauv) (venom_State *L, size_t sz, int nuvalue);
VENOM_API int   (venom_getmetatable) (venom_State *L, int objindex);
VENOM_API int  (venom_getiuservalue) (venom_State *L, int idx, int n);


/*
** set functions (stack -> Venom)
*/
VENOM_API void  (venom_setglobal) (venom_State *L, const char *name);
VENOM_API void  (venom_settable) (venom_State *L, int idx);
VENOM_API void  (venom_setfield) (venom_State *L, int idx, const char *k);
VENOM_API void  (venom_seti) (venom_State *L, int idx, venom_Integer n);
VENOM_API void  (venom_rawset) (venom_State *L, int idx);
VENOM_API void  (venom_rawseti) (venom_State *L, int idx, venom_Integer n);
VENOM_API void  (venom_rawsetp) (venom_State *L, int idx, const void *p);
VENOM_API int   (venom_setmetatable) (venom_State *L, int objindex);
VENOM_API int   (venom_setiuservalue) (venom_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Venom code)
*/
VENOM_API void  (venom_callk) (venom_State *L, int nargs, int nresults,
                           venom_KContext ctx, venom_KFunction k);
#define venom_call(L,n,r)		venom_callk(L, (n), (r), 0, NULL)

VENOM_API int   (venom_pcallk) (venom_State *L, int nargs, int nresults, int errfunc,
                            venom_KContext ctx, venom_KFunction k);
#define venom_pcall(L,n,r,f)	venom_pcallk(L, (n), (r), (f), 0, NULL)

VENOM_API int   (venom_load) (venom_State *L, venom_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

VENOM_API int (venom_dump) (venom_State *L, venom_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
VENOM_API int  (venom_yieldk)     (venom_State *L, int nresults, venom_KContext ctx,
                               venom_KFunction k);
VENOM_API int  (venom_resume)     (venom_State *L, venom_State *from, int narg,
                               int *nres);
VENOM_API int  (venom_status)     (venom_State *L);
VENOM_API int (venom_isyieldable) (venom_State *L);

#define venom_yield(L,n)		venom_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
VENOM_API void (venom_setwarnf) (venom_State *L, venom_WarnFunction f, void *ud);
VENOM_API void (venom_warning)  (venom_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define VENOM_GCSTOP		0
#define VENOM_GCRESTART		1
#define VENOM_GCCOLLECT		2
#define VENOM_GCCOUNT		3
#define VENOM_GCCOUNTB		4
#define VENOM_GCSTEP		5
#define VENOM_GCSETPAUSE		6
#define VENOM_GCSETSTEPMUL	7
#define VENOM_GCISRUNNING		9
#define VENOM_GCGEN		10
#define VENOM_GCINC		11

VENOM_API int (venom_gc) (venom_State *L, int what, ...);


/*
** miscellaneous functions
*/

VENOM_API int   (venom_error) (venom_State *L);

VENOM_API int   (venom_next) (venom_State *L, int idx);

VENOM_API void  (venom_concat) (venom_State *L, int n);
VENOM_API void  (venom_len)    (venom_State *L, int idx);

VENOM_API size_t   (venom_stringtonumber) (venom_State *L, const char *s);

VENOM_API venom_Alloc (venom_getallocf) (venom_State *L, void **ud);
VENOM_API void      (venom_setallocf) (venom_State *L, venom_Alloc f, void *ud);

VENOM_API void (venom_toclose) (venom_State *L, int idx);
VENOM_API void (venom_closeslot) (venom_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define venom_getextraspace(L)	((void *)((char *)(L) - VENOM_EXTRASPACE))

#define venom_tonumber(L,i)	venom_tonumberx(L,(i),NULL)
#define venom_tointeger(L,i)	venom_tointegerx(L,(i),NULL)

#define venom_pop(L,n)		venom_settop(L, -(n)-1)

#define venom_newtable(L)		venom_createtable(L, 0, 0)

#define venom_register(L,n,f) (venom_pushcfunction(L, (f)), venom_setglobal(L, (n)))

#define venom_pushcfunction(L,f)	venom_pushcclosure(L, (f), 0)

#define venom_isfunction(L,n)	(venom_type(L, (n)) == VENOM_TFUNCTION)
#define venom_istable(L,n)	(venom_type(L, (n)) == VENOM_TTABLE)
#define venom_islightuserdata(L,n)	(venom_type(L, (n)) == VENOM_TLIGHTUSERDATA)
#define venom_isnil(L,n)		(venom_type(L, (n)) == VENOM_TNIL)
#define venom_isboolean(L,n)	(venom_type(L, (n)) == VENOM_TBOOLEAN)
#define venom_isthread(L,n)	(venom_type(L, (n)) == VENOM_TTHREAD)
#define venom_isnone(L,n)		(venom_type(L, (n)) == VENOM_TNONE)
#define venom_isnoneornil(L, n)	(venom_type(L, (n)) <= 0)

#define venom_pushliteral(L, s)	venom_pushstring(L, "" s)

#define venom_pushglobaltable(L)  \
	((void)venom_rawgeti(L, VENOM_REGISTRYINDEX, VENOM_RIDX_GLOBALS))

#define venom_tostring(L,i)	venom_tolstring(L, (i), NULL)


#define venom_insert(L,idx)	venom_rotate(L, (idx), 1)

#define venom_remove(L,idx)	(venom_rotate(L, (idx), -1), venom_pop(L, 1))

#define venom_replace(L,idx)	(venom_copy(L, -1, (idx)), venom_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(VENOM_COMPAT_APIINTCASTS)

#define venom_pushunsigned(L,n)	venom_pushinteger(L, (venom_Integer)(n))
#define venom_tounsignedx(L,i,is)	((venom_Unsigned)venom_tointegerx(L,i,is))
#define venom_tounsigned(L,i)	venom_tounsignedx(L,(i),NULL)

#endif

#define venom_newuserdata(L,s)	venom_newuserdatauv(L,s,1)
#define venom_getuservalue(L,idx)	venom_getiuservalue(L,idx,1)
#define venom_setuservalue(L,idx)	venom_setiuservalue(L,idx,1)

#define VENOM_NUMTAGS		VENOM_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define VENOM_HOOKCALL	0
#define VENOM_HOOKRET	1
#define VENOM_HOOKLINE	2
#define VENOM_HOOKCOUNT	3
#define VENOM_HOOKTAILCALL 4


/*
** Event masks
*/
#define VENOM_MASKCALL	(1 << VENOM_HOOKCALL)
#define VENOM_MASKRET	(1 << VENOM_HOOKRET)
#define VENOM_MASKLINE	(1 << VENOM_HOOKLINE)
#define VENOM_MASKCOUNT	(1 << VENOM_HOOKCOUNT)

typedef struct venom_Debug venom_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*venom_Hook) (venom_State *L, venom_Debug *ar);


VENOM_API int (venom_getstack) (venom_State *L, int level, venom_Debug *ar);
VENOM_API int (venom_getinfo) (venom_State *L, const char *what, venom_Debug *ar);
VENOM_API const char *(venom_getlocal) (venom_State *L, const venom_Debug *ar, int n);
VENOM_API const char *(venom_setlocal) (venom_State *L, const venom_Debug *ar, int n);
VENOM_API const char *(venom_getupvalue) (venom_State *L, int funcindex, int n);
VENOM_API const char *(venom_setupvalue) (venom_State *L, int funcindex, int n);

VENOM_API void *(venom_upvalueid) (venom_State *L, int fidx, int n);
VENOM_API void  (venom_upvaluejoin) (venom_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

VENOM_API void (venom_sethook) (venom_State *L, venom_Hook func, int mask, int count);
VENOM_API venom_Hook (venom_gethook) (venom_State *L);
VENOM_API int (venom_gethookmask) (venom_State *L);
VENOM_API int (venom_gethookcount) (venom_State *L);

VENOM_API int (venom_setcstacklimit) (venom_State *L, unsigned int limit);

struct venom_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Venom', 'C', 'main', 'tail' */
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
  char short_src[VENOM_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};
#endif