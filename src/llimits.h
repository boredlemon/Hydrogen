/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in viper.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>
#include <stddef.h>


#include "viper.h"


/*
** 'lu_mem' and 'l_mem' are unsigned/signed integers big enough to count
** the total memory used by Viper (in bytes). Usually, 'size_t' and
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/
#if defined(VIPERI_MEM)		/* { external definitions? */
typedef VIPERI_UMEM lu_mem;
typedef VIPERI_MEM l_mem;
#elif VIPERI_IS32INT	/* }{ */
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

/* maximum size visible for Viper (must be representable in a viper_Integer) */
#define MAX_SIZE	(sizeof(size_t) < sizeof(viper_Integer) ? MAX_SIZET \
                          : (size_t)(VIPER_MAXINTEGER))


#define MAX_LUMEM	((lu_mem)(~(lu_mem)0))

#define MAX_LMEM	((l_mem)(MAX_LUMEM >> 1))


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



/* types of 'usual argument conversions' for viper_Number and viper_Integer */
typedef VIPERI_UACNUMBER l_uacNumber;
typedef VIPERI_UACINT l_uacInt;


/*
** Internal assertions for in-house debugging
*/
#if defined VIPERI_ASSERT
#undef NDEBUG
#include <assert.h>
#define viper_assert(c)           assert(c)
#endif

#if defined(viper_assert)
#define check_exp(c,e)		(viper_assert(c), (e))
/* to avoid problems with conditions too long */
#define viper_longassert(c)	((c) ? (void)0 : viper_assert(0))
#else
#define viper_assert(c)		((void)0)
#define check_exp(c,e)		(e)
#define viper_longassert(c)	((void)0)
#endif

/*
** assertion for checking API calls
*/
#if !defined(viperi_apicheck)
#define viperi_apicheck(l,e)	((void)l, viper_assert(e))
#endif

#define api_check(l,e,msg)	viperi_apicheck(l,(e) && msg)


/* macro to avoid warnings about unused variables */
#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif


/* type casts (a macro highlights casts in the code) */
#define cast(t, exp)	((t)(exp))

#define cast_void(i)	cast(void, (i))
#define cast_voidp(i)	cast(void *, (i))
#define cast_num(i)	cast(viper_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uint(i)	cast(unsigned int, (i))
#define cast_byte(i)	cast(lu_byte, (i))
#define cast_uchar(i)	cast(unsigned char, (i))
#define cast_char(i)	cast(char, (i))
#define cast_charp(i)	cast(char *, (i))
#define cast_sizet(i)	cast(size_t, (i))


/* cast a signed viper_Integer to viper_Unsigned */
#if !defined(l_castS2U)
#define l_castS2U(i)	((viper_Unsigned)(i))
#endif

/*
** cast a viper_Unsigned to a signed viper_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/
#if !defined(l_castU2S)
#define l_castU2S(i)	((viper_Integer)(i))
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
#if !defined(VIPER_USE_C89)
#define l_inline	inline
#elif defined(__GNUC__)
#define l_inline	__inline__
#else
#define l_inline	/* empty */
#endif

#define l_sinline	static l_inline


/*
** type for virtual-machine instructions;
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
#if VIPERI_IS32INT
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
#if !defined(VIPERI_MAXSHORTLEN)
#define VIPERI_MAXSHORTLEN	40
#endif


/*
** Initial size for the string table (must be power of 2).
** The Viper core alone registers ~50 strings (reserved words +
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
#if !defined(VIPER_MINBUFFER)
#define VIPER_MINBUFFER	32
#endif


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(VIPERI_MAXCCALLS)
#define VIPERI_MAXCCALLS		200
#endif


/*
** macros that are executed whenever program enters the Viper core
** ('viper_lock') and leaves the core ('viper_unlock')
*/
#if !defined(viper_lock)
#define viper_lock(L)	((void) 0)
#define viper_unlock(L)	((void) 0)
#endif

/*
** macro executed during Viper functions at points where the
** function can yield.
*/
#if !defined(viperi_threadyield)
#define viperi_threadyield(L)	{viper_unlock(L); viper_lock(L);}
#endif


/*
** these macros allow user-specific actions when a thread is
** created/deleted/resumed/yielded.
*/
#if !defined(viperi_userstateopen)
#define viperi_userstateopen(L)		((void)L)
#endif

#if !defined(viperi_userstateclose)
#define viperi_userstateclose(L)		((void)L)
#endif

#if !defined(viperi_userstatethread)
#define viperi_userstatethread(L,L1)	((void)L)
#endif

#if !defined(viperi_userstatefree)
#define viperi_userstatefree(L,L1)	((void)L)
#endif

#if !defined(viperi_userstateresume)
#define viperi_userstateresume(L,n)	((void)L)
#endif

#if !defined(viperi_userstateyield)
#define viperi_userstateyield(L,n)	((void)L)
#endif



/*
** The viperi_num* macros define the primitive operations over numbers.
*/

/* floor division (defined as 'floor(a/b)') */
#if !defined(viperi_numidiv)
#define viperi_numidiv(L,a,b)     ((void)L, l_floor(viperi_numdiv(L,a,b)))
#endif

/* float division */
#if !defined(viperi_numdiv)
#define viperi_numdiv(L,a,b)      ((a)/(b))
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
#if !defined(viperi_nummod)
#define viperi_nummod(L,a,b,m)  \
  { (void)L; (m) = l_mathop(fmod)(a,b); \
    if (((m) > 0) ? (b) < 0 : ((m) < 0 && (b) > 0)) (m) += (b); }
#endif

/* exponentiation */
#if !defined(viperi_numpow)
#define viperi_numpow(L,a,b)  \
  ((void)L, (b == 2) ? (a)*(a) : l_mathop(pow)(a,b))
#endif

/* the others are quite standard operations */
#if !defined(viperi_numadd)
#define viperi_numadd(L,a,b)      ((a)+(b))
#define viperi_numsub(L,a,b)      ((a)-(b))
#define viperi_nummul(L,a,b)      ((a)*(b))
#define viperi_numunm(L,a)        (-(a))
#define viperi_numeq(a,b)         ((a)==(b))
#define viperi_numlt(a,b)         ((a)<(b))
#define viperi_numle(a,b)         ((a)<=(b))
#define viperi_numgt(a,b)         ((a)>(b))
#define viperi_numge(a,b)         ((a)>=(b))
#define viperi_numisnan(a)        (!viperi_numeq((a), (a)))
#endif





/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; viperD_reallocstack((L), sz_, 0); pos; }
#endif

#if !defined(HARDMEMTESTS)
#define condchangemem(L,pre,pos)	((void)0)
#else
#define condchangemem(L,pre,pos)  \
	{ if (gcrunning(G(L))) { pre; viperC_fullgc(L, 0); pos; } }
#endif

#endif
