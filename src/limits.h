/*
** $Id: limits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in venom.h
*/

#ifndef limits_h
#define limits_h


#include <limits.h>
#include <stddef.h>


#include "venom.h"


/*
** 'lu_mem' and 'l_mem' are unsigned/signed integers big enough to count
** the total memory used by Venom (in bytes). Usually, 'size_t' and
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/
#if defined(VENOMI_MEM)		/* { external definitions? */
typedef VENOMI_UMEM lu_mem;
typedef VENOMI_MEM l_mem;
#elif VENOMI_IS32INT	/* }{ */
typedef size_t lu_mem;
typedef ptrdiff_t l_mem;
#else  /* 16-bit ints */	/* }{ */
typedef unsigned long lu_mem;
typedef long l_mem;
#endif				/* } */


/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char lu_byte;
typedef signed char ls_byte;


/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))

/* maximum size visible for Venom (must be representable in a venom_Integer) */
#define MAX_SIZE	(sizeof(size_t) < sizeof(venom_Integer) ? MAX_SIZET \
                          : (size_t)(VENOM_MAXINTEGER))


#define MAX_LUMEM	((lu_mem)(~(lu_mem)0))

#define MAX_memory	((l_mem)(MAX_LUMEM >> 1))


#define MAX_INT		INT_MAX  /* maximum value of an int */


/*
** floor of the log2 of the maximum signed value for integral type 't'.
** (That is, maximum 'n' such that '2^n' fits in the given signed type.)
*/
#define log2maxs(t)	(sizeof(t) * 8 - 2)


/*
** test whether an unsigned value is a power of 2 (or zero)
*/
#define ispow2(x)	(((x) & ((x) - 1)) == 0)


/* number of chars of a literal string without the ending \0 */
#define LL(x)   (sizeof(x)/sizeof(char) - 1)


/*
** conversion of pointer to unsigned integer:
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define point2uint(p)	((unsigned int)((size_t)(p) & UINT_MAX))



/* types of 'usual argument conversions' for venom_Number and venom_Integer */
typedef VENOMI_UACNUMBER l_uacNumber;
typedef VENOMI_UACINT l_uacInt;


/*
** Internal assertions for in-house debugging
*/
#if defined VENOMI_ASSERT
#undef NDEBUG
#include <assert.h>
#define venom_assert(c)           assert(c)
#endif

#if defined(venom_assert)
#define check_exp(c,e)		(venom_assert(c), (e))
/* to avoid problems with conditions too long */
#define venom_longassert(c)	((c) ? (void)0 : venom_assert(0))
#else
#define venom_assert(c)		((void)0)
#define check_exp(c,e)		(e)
#define venom_longassert(c)	((void)0)
#endif

/*
** assertion for checking API calls
*/
#if !defined(venomi_apicheck)
#define venomi_apicheck(l,e)	((void)l, venom_assert(e))
#endif

#define api_check(l,e,msg)	venomi_apicheck(l,(e) && msg)


/* macro to avoid warnings about unused variables */
#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif


/* type casts (a macro highlights casts in the code) */
#define cast(t, exp)	((t)(exp))

#define cast_void(i)	cast(void, (i))
#define cast_voidp(i)	cast(void *, (i))
#define cast_num(i)	cast(venom_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uint(i)	cast(unsigned int, (i))
#define cast_byte(i)	cast(lu_byte, (i))
#define cast_uchar(i)	cast(unsigned char, (i))
#define cast_char(i)	cast(char, (i))
#define cast_charp(i)	cast(char *, (i))
#define cast_sizet(i)	cast(size_t, (i))


/* cast a signed venom_Integer to venom_Unsigned */
#if !defined(l_castS2U)
#define l_castS2U(i)	((venom_Unsigned)(i))
#endif

/*
** cast a venom_Unsigned to a signed venom_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/
#if !defined(l_castU2S)
#define l_castU2S(i)	((venom_Integer)(i))
#endif


/*
** non-return type
*/
#if !defined(l_noret)

#if defined(__GNUC__)
#define l_noret		void __attribute__((noreturn))
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define l_noret		void __declspec(noreturn)
#else
#define l_noret		void
#endif

#endif


/*
** Inline functions
*/
#if !defined(VENOM_USE_C89)
#define l_inline	inline
#elif defined(__GNUC__)
#define l_inline	__inline__
#else
#define l_inline	/* empty */
#endif

#define l_sinline	static l_inline


/*
** type for virtual-machine instructions;
** must be an unsigned with (at least) 4 bytes (see details in opcodes.h)
*/
#if VENOMI_IS32INT
typedef unsigned int l_uint32;
#else
typedef unsigned long l_uint32;
#endif

typedef l_uint32 Instruction;



/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#if !defined(VENOMI_MAXSHORTLEN)
#define VENOMI_MAXSHORTLEN	40
#endif


/*
** Initial size for the string table (must be power of 2).
** The Venom core alone registers ~50 strings (reserved words +
** metaevent keys + a few others). Libraries would typically add
** a few dozens more.
*/
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE	128
#endif


/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set (M == 1
** makes a direct cache.)
*/
#if !defined(STRCACHE_N)
#define STRCACHE_N		53
#define STRCACHE_M		2
#endif


/* minimum size for string buffer */
#if !defined(VENOM_MINBUFFER)
#define VENOM_MINBUFFER	32
#endif


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(VENOMI_MAXCCALLS)
#define VENOMI_MAXCCALLS		200
#endif


/*
** macros that are executed whenever program enters the Venom core
** ('venom_lock') and leaves the core ('venom_unlock')
*/
#if !defined(venom_lock)
#define venom_lock(L)	((void) 0)
#define venom_unlock(L)	((void) 0)
#endif

/*
** macro executed during Venom functions at points where the
** function can yield.
*/
#if !defined(venomi_threadyield)
#define venomi_threadyield(L)	{venom_unlock(L); venom_lock(L);}
#endif


/*
** these macros allow user-specific actions when a thread is
** created/deleted/resumed/yielded.
*/
#if !defined(venomi_userstateopen)
#define venomi_userstateopen(L)		((void)L)
#endif

#if !defined(venomi_userstateclose)
#define venomi_userstateclose(L)		((void)L)
#endif

#if !defined(venomi_userstatethread)
#define venomi_userstatethread(L,L1)	((void)L)
#endif

#if !defined(venomi_userstatefree)
#define venomi_userstatefree(L,L1)	((void)L)
#endif

#if !defined(venomi_userstateresume)
#define venomi_userstateresume(L,n)	((void)L)
#endif

#if !defined(venomi_userstateyield)
#define venomi_userstateyield(L,n)	((void)L)
#endif



/*
** The venomi_num* macros define the primitive operations over numbers.
*/

/* floor division (defined as 'floor(a/b)') */
#if !defined(venomi_numidiv)
#define venomi_numidiv(L,a,b)     ((void)L, l_floor(venomi_numdiv(L,a,b)))
#endif

/* float division */
#if !defined(venomi_numdiv)
#define venomi_numdiv(L,a,b)      ((a)/(b))
#endif

/*
** modulo: defined as 'a - floor(a/b)*b'; the direct computation
** using this definition has several problems with rounding errors,
** so it is better to use 'fmod'. 'fmod' gives the result of
** 'a - trunc(a/b)*b', and therefore must be corrected when
** 'trunc(a/b) ~= floor(a/b)'. That happens when the division has a
** non-integer negative result: non-integer result is equivalent to
** a non-zero remainder 'm'; negative result is equivalent to 'a' and
** 'b' with different signs, or 'm' and 'b' with different signs
** (as the result 'm' of 'fmod' has the same sign of 'a').
*/
#if !defined(venomi_nummod)
#define venomi_nummod(L,a,b,m)  \
  { (void)L; (m) = l_mathop(fmod)(a,b); \
    if (((m) > 0) ? (b) < 0 : ((m) < 0 && (b) > 0)) (m) += (b); }
#endif

/* exponentiation */
#if !defined(venomi_numpow)
#define venomi_numpow(L,a,b)  \
  ((void)L, (b == 2) ? (a)*(a) : l_mathop(pow)(a,b))
#endif

/* the others are quite standard operations */
#if !defined(venomi_numadd)
#define venomi_numadd(L,a,b)      ((a)+(b))
#define venomi_numsub(L,a,b)      ((a)-(b))
#define venomi_nummul(L,a,b)      ((a)*(b))
#define venomi_numunm(L,a)        (-(a))
#define venomi_numeq(a,b)         ((a)==(b))
#define venomi_numlt(a,b)         ((a)<(b))
#define venomi_numle(a,b)         ((a)<=(b))
#define venomi_numgt(a,b)         ((a)>(b))
#define venomi_numge(a,b)         ((a)>=(b))
#define venomi_numisnan(a)        (!venomi_numeq((a), (a)))
#endif





/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; venomD_reallocstack((L), sz_, 0); pos; }
#endif

#if !defined(HARDMEMTESTS)
#define condchangemem(L,pre,pos)	((void)0)
#else
#define condchangemem(L,pre,pos)  \
	{ if (gcrunning(G(L))) { pre; venomC_fulgarbageCollection(L, 0); pos; } }
#endif

#endif
