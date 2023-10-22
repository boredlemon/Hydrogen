/*
** $Id: viperconf.h $
** Configuration file for Viper
** See Copyright Notice in viper.h
*/


#ifndef viperconf_h
#define viperconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Viper
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Viper ABI (by making the changes here, you ensure that all software
** connected to Viper, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Viper to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ VIPER_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Viper to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define VIPER_USE_C89 */


/*
** By default, Viper on Windows use (some) specific Windows features
*/
#if !defined(VIPER_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define VIPER_USE_WINDOWS  /* enable Viperodies for regular Windows */
#endif


#if defined(VIPER_USE_WINDOWS)
#define VIPER_DL_DLL	/* enable support for DLL */
#define VIPER_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(VIPER_USE_LINUX)
#define VIPER_USE_POSIX
#define VIPER_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(VIPER_USE_MACOSX)
#define VIPER_USE_POSIX
#define VIPER_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


/*
@@ VIPERI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define VIPERI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Viper must
** use the same configuration.
** ===================================================================
*/

/*
@@ VIPER_INT_TYPE defines the type for Viper integers.
@@ VIPER_FLOAT_TYPE defines the type for Viper floats.
** Viper should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for VIPER_INT_TYPE */
#define VIPER_INT_INT		1
#define VIPER_INT_LONG		2
#define VIPER_INT_LONGLONG	3

/* predefined options for VIPER_FLOAT_TYPE */
#define VIPER_FLOAT_FLOAT		1
#define VIPER_FLOAT_DOUBLE	2
#define VIPER_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Viper) */
#define VIPER_INT_DEFAULT		VIPER_INT_LONGLONG
#define VIPER_FLOAT_DEFAULT	VIPER_FLOAT_DOUBLE


/*
@@ VIPER_32BITS enables Viper with 32-bit integers and 32-bit floats.
*/
#define VIPER_32BITS	0


/*
@@ VIPER_C89_NUMBERS ensures that Viper uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(VIPER_USE_C89) && !defined(VIPER_USE_WINDOWS)
#define VIPER_C89_NUMBERS		1
#else
#define VIPER_C89_NUMBERS		0
#endif


#if VIPER_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if VIPERI_IS32INT  /* use 'int' if big enough */
#define VIPER_INT_TYPE	VIPER_INT_INT
#else  /* otherwise use 'long' */
#define VIPER_INT_TYPE	VIPER_INT_LONG
#endif
#define VIPER_FLOAT_TYPE	VIPER_FLOAT_FLOAT

#elif VIPER_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define VIPER_INT_TYPE	VIPER_INT_LONG
#define VIPER_FLOAT_TYPE	VIPER_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define VIPER_INT_TYPE	VIPER_INT_DEFAULT
#define VIPER_FLOAT_TYPE	VIPER_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** VIPER_PATH_SEP is the character that separates templates in a path.
** VIPER_PATH_MARK is the string that marks the substitution points in a
** template.
** VIPER_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define VIPER_PATH_SEP            ";"
#define VIPER_PATH_MARK           "?"
#define VIPER_EXEC_DIR            "!"


/*
@@ VIPER_PATH_DEFAULT is the default path that Viper uses to look for
** Viper libraries.
@@ VIPER_CPATH_DEFAULT is the default path that Viper uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define VIPER_VDIR	VIPER_VERSION_MAJOR "." VIPER_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define VIPER_LDIR	"!\\viper\\"
#define VIPER_CDIR	"!\\"
#define VIPER_SHRDIR	"!\\..\\share\\viper\\" VIPER_VDIR "\\"

#if !defined(VIPER_PATH_DEFAULT)
#define VIPER_PATH_DEFAULT  \
		VIPER_LDIR"?.viper;"  VIPER_LDIR"?\\init.viper;" \
		VIPER_CDIR"?.viper;"  VIPER_CDIR"?\\init.viper;" \
		VIPER_SHRDIR"?.viper;" VIPER_SHRDIR"?\\init.viper;" \
		".\\?.viper;" ".\\?\\init.viper"
#endif

#if !defined(VIPER_CPATH_DEFAULT)
#define VIPER_CPATH_DEFAULT \
		VIPER_CDIR"?.dll;" \
		VIPER_CDIR"..\\lib\\viper\\" VIPER_VDIR "\\?.dll;" \
		VIPER_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define VIPER_ROOT	"/usr/local/"
#define VIPER_LDIR	VIPER_ROOT "share/viper/" VIPER_VDIR "/"
#define VIPER_CDIR	VIPER_ROOT "lib/viper/" VIPER_VDIR "/"

#if !defined(VIPER_PATH_DEFAULT)
#define VIPER_PATH_DEFAULT  \
		VIPER_LDIR"?.viper;"  VIPER_LDIR"?/init.viper;" \
		VIPER_CDIR"?.viper;"  VIPER_CDIR"?/init.viper;" \
		"./?.viper;" "./?/init.viper"
#endif

#if !defined(VIPER_CPATH_DEFAULT)
#define VIPER_CPATH_DEFAULT \
		VIPER_CDIR"?.so;" VIPER_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ VIPER_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Viper automatically uses "\".)
*/
#if !defined(VIPER_DIRSEP)

#if defined(_WIN32)
#define VIPER_DIRSEP	"\\"
#else
#define VIPER_DIRSEP	"/"
#endif

#endif

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ VIPER_API is a mark for all core API functions.
@@ VIPERLIB_API is a mark for all auxiliary library functions.
@@ VIPERMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** VIPER_BUILD_AS_DLL to get it).
*/
#if defined(VIPER_BUILD_AS_DLL)	/* { */

#if defined(VIPER_CORE) || defined(VIPER_LIB)	/* { */
#define VIPER_API __declspec(dllexport)
#else						/* }{ */
#define VIPER_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define VIPER_API		extern

#endif				/* } */


/*
** More often than not the libs Viper together with the core.
*/
#define VIPERLIB_API	VIPER_API
#define VIPERMOD_API	VIPER_API


/*
@@ VIPERI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ VIPERI_DDEF and VIPERI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (VIPERI_DDEF for
** definitions and VIPERI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Viper is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define VIPERI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define VIPERI_FUNC	extern
#endif				/* } */

#define VIPERI_DDEC(dec)	VIPERI_FUNC dec
#define VIPERI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ VIPER_COMPAT_5_3 controls other macros for compatibility with Viper 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(VIPER_COMPAT_5_3)	/* { */

/*
@@ VIPER_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define VIPER_COMPAT_MATHLIB

/*
@@ VIPER_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (viper_pushunsigned, viper_tounsigned,
** viperL_checkint, viperL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define VIPER_COMPAT_APIINTCASTS


/*
@@ VIPER_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define VIPER_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define viper_strlen(L,i)		viper_rawlen(L, (i))

#define viper_objlen(L,i)		viper_rawlen(L, (i))

#define viper_equal(L,idx1,idx2)		viper_compare(L,(idx1),(idx2),VIPER_OPEQ)
#define viper_lessthan(L,idx1,idx2)	viper_compare(L,(idx1),(idx2),VIPER_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined VIPER_FLOAT_* / VIPER_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ VIPERI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ VIPER_NUMBER_FRMLEN is the length modifier for writing floats.
@@ VIPER_NUMBER_FMT is the format for writing floats.
@@ viper_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ viper_str2number converts a decimal numeral to a number.
*/


/* The following definitions are Viperod for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define viper_number2str(s,sz,n)  \
	l_sprintf((s), sz, VIPER_NUMBER_FMT, (VIPERI_UACNUMBER)(n))

/*
@@ viper_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a viper_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define viper_numbertointeger(n,p) \
  ((n) >= (VIPER_NUMBER)(VIPER_MININTEGER) && \
   (n) < -(VIPER_NUMBER)(VIPER_MININTEGER) && \
      (*(p) = (VIPER_INTEGER)(n), 1))


/* now the variable definitions */

#if VIPER_FLOAT_TYPE == VIPER_FLOAT_FLOAT		/* { single float */

#define VIPER_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define VIPERI_UACNUMBER	double

#define VIPER_NUMBER_FRMLEN	""
#define VIPER_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define viper_str2number(s,p)	strtof((s), (p))


#elif VIPER_FLOAT_TYPE == VIPER_FLOAT_LONGDOUBLE	/* }{ long double */

#define VIPER_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define VIPERI_UACNUMBER	long double

#define VIPER_NUMBER_FRMLEN	"L"
#define VIPER_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define viper_str2number(s,p)	strtold((s), (p))

#elif VIPER_FLOAT_TYPE == VIPER_FLOAT_DOUBLE	/* }{ double */

#define VIPER_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define VIPERI_UACNUMBER	double

#define VIPER_NUMBER_FRMLEN	""
#define VIPER_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define viper_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ VIPER_UNSIGNED is the unsigned version of VIPER_INTEGER.
@@ VIPERI_UACINT is the result of a 'default argument promotion'
@@ over a VIPER_INTEGER.
@@ VIPER_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ VIPER_INTEGER_FMT is the format for writing integers.
@@ VIPER_MAXINTEGER is the maximum value for a VIPER_INTEGER.
@@ VIPER_MININTEGER is the minimum value for a VIPER_INTEGER.
@@ VIPER_MAXUNSIGNED is the maximum value for a VIPER_UNSIGNED.
@@ viper_integer2str converts an integer to a string.
*/


/* The following definitions are Viperod for most cases here */

#define VIPER_INTEGER_FMT		"%" VIPER_INTEGER_FRMLEN "d"

#define VIPERI_UACINT		VIPER_INTEGER

#define viper_integer2str(s,sz,n)  \
	l_sprintf((s), sz, VIPER_INTEGER_FMT, (VIPERI_UACINT)(n))

/*
** use VIPERI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define VIPER_UNSIGNED		unsigned VIPERI_UACINT


/* now the variable definitions */

#if VIPER_INT_TYPE == VIPER_INT_INT		/* { int */

#define VIPER_INTEGER		int
#define VIPER_INTEGER_FRMLEN	""

#define VIPER_MAXINTEGER		INT_MAX
#define VIPER_MININTEGER		INT_MIN

#define VIPER_MAXUNSIGNED		UINT_MAX

#elif VIPER_INT_TYPE == VIPER_INT_LONG	/* }{ long */

#define VIPER_INTEGER		long
#define VIPER_INTEGER_FRMLEN	"l"

#define VIPER_MAXINTEGER		LONG_MAX
#define VIPER_MININTEGER		LONG_MIN

#define VIPER_MAXUNSIGNED		ULONG_MAX

#elif VIPER_INT_TYPE == VIPER_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define VIPER_INTEGER		long long
#define VIPER_INTEGER_FRMLEN	"ll"

#define VIPER_MAXINTEGER		LLONG_MAX
#define VIPER_MININTEGER		LLONG_MIN

#define VIPER_MAXUNSIGNED		ULLONG_MAX

#elif defined(VIPER_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define VIPER_INTEGER		__int64
#define VIPER_INTEGER_FRMLEN	"I64"

#define VIPER_MAXINTEGER		_I64_MAX
#define VIPER_MININTEGER		_I64_MIN

#define VIPER_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DVIPER_32BITS' \
  or '-DVIPER_C89_NUMBERS' (see file 'viperconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Viper have only one format item.)
*/
#if !defined(VIPER_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ viper_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'viper_strx2number' undefined and Viper will provide its own
** implementation.
*/
#if !defined(VIPER_USE_C89)
#define viper_strx2number(s,p)		viper_str2number(s,p)
#endif


/*
@@ viper_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define viper_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ viper_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'viper_number2strx' undefined and Viper will
** provide its own implementation.
*/
#if !defined(VIPER_USE_C89)
#define viper_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(VIPERI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a Viperod proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(VIPER_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef viper_str2number
#define l_mathop(op)		(viper_Number)op  /* no variant */
#define viper_str2number(s,p)	((viper_Number)strtod((s), (p)))
#endif


/*
@@ VIPER_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Viper will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define VIPER_KCONTEXT	ptrdiff_t

#if !defined(VIPER_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef VIPER_KCONTEXT
#define VIPER_KCONTEXT	intptr_t
#endif
#endif


/*
@@ viper_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(viper_getlocaledecpoint)
#define viper_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Viper API use these macros.
** Define VIPER_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(viperi_likely)

#if defined(__GNUC__) && !defined(VIPER_NOBUILTIN)
#define viperi_likely(x)		(__builtin_expect(((x) != 0), 1))
#define viperi_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define viperi_likely(x)		(x)
#define viperi_unlikely(x)	(x)
#endif

#endif


#if defined(VIPER_CORE) || defined(VIPER_LIB)
/* shorter names for Viper's own use */
#define l_likely(x)	viperi_likely(x)
#define l_unlikely(x)	viperi_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ VIPER_NOCVTN2S/VIPER_NOCVTS2N control how Viper performs some
** coercions. Define VIPER_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define VIPER_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define VIPER_NOCVTN2S */
/* #define VIPER_NOCVTS2N */


/*
@@ VIPER_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(VIPER_USE_APICHECK)
#include <assert.h>
#define viperi_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Viper and when you compile code that links to
** Viper).
** =====================================================================
*/

/*
@@ VIPERI_MAXSTACK limits the size of the Viper stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Viper from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32.)
*/
#if VIPERI_IS32INT
#define VIPERI_MAXSTACK		1000000
#else
#define VIPERI_MAXSTACK		15000
#endif


/*
@@ VIPER_EXTRASPACE defines the size of a raw memory area associated with
** a Viper state with very fast access.
** CHANGE it if you need a different size.
*/
#define VIPER_EXTRASPACE		(sizeof(void *))


/*
@@ VIPER_IDSIZE gives the maximum size for the description of the source
@@ of a function in debug information.
** CHANGE it if you want a different size.
*/
#define VIPER_IDSIZE	60


/*
@@ VIPERL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
*/
#define VIPERL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(viper_Number)))


/*
@@ VIPERI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define VIPERI_MAXALIGN  viper_Number n; double u; void *s; viper_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

