/*
** $Id: hydrogenconf.h $
** Configuration file for Hydrogen
** See Copyright Notice in hydrogen.h
*/


#ifndef hydrogenconf_h
#define hydrogenconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Hydrogen
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Hydrogen ABI (by making the changes here, you ensure that all software
** connected to Hydrogen, such as C libraries, will be compiled with the same
** configuration); or because they are sedom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Hydrogen to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ HYDROGEN_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Hydrogen to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define HYDROGEN_USE_C89 */


/*
** By default, Hydrogen on Windows use (some) specific Windows features
*/
#if !defined(HYDROGEN_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define HYDROGEN_USE_WINDOWS  /* enable Hydrogenodies for regular Windows */
#endif


#if defined(HYDROGEN_USE_WINDOWS)
#define HYDROGEN_DL_DLL	/* enable support for DLL */
#define HYDROGEN_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(HYDROGEN_USE_LINUX)
#define HYDROGEN_USE_POSIX
#define HYDROGEN_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(HYDROGEN_USE_MACOSX)
#define HYDROGEN_USE_POSIX
#define HYDROGEN_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


/*
@@ HYDROGENI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define HYDROGENI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Hydrogen must
** use the same configuration.
** ===================================================================
*/

/*
@@ HYDROGEN_INT_TYPE defines the type for Hydrogen integers.
@@ HYDROGEN_FLOAT_TYPE defines the type for Hydrogen floats.
** Hydrogen should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for HYDROGEN_INT_TYPE */
#define HYDROGEN_INT_INT		1
#define HYDROGEN_INT_LONG		2
#define HYDROGEN_INT_LONGLONG	3

/* predefined options for HYDROGEN_FLOAT_TYPE */
#define HYDROGEN_FLOAT_FLOAT		1
#define HYDROGEN_FLOAT_DOUBLE	2
#define HYDROGEN_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Hydrogen) */
#define HYDROGEN_INT_DEFAULT		HYDROGEN_INT_LONGLONG
#define HYDROGEN_FLOAT_DEFAULT	HYDROGEN_FLOAT_DOUBLE


/*
@@ HYDROGEN_32BITS enables Hydrogen with 32-bit integers and 32-bit floats.
*/
#define HYDROGEN_32BITS	0


/*
@@ HYDROGEN_C89_NUMBERS ensures that Hydrogen uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(HYDROGEN_USE_C89) && !defined(HYDROGEN_USE_WINDOWS)
#define HYDROGEN_C89_NUMBERS		1
#else
#define HYDROGEN_C89_NUMBERS		0
#endif


#if HYDROGEN_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if HYDROGENI_IS32INT  /* use 'int' if big enough */
#define HYDROGEN_INT_TYPE	HYDROGEN_INT_INT
#else  /* otherwise use 'long' */
#define HYDROGEN_INT_TYPE	HYDROGEN_INT_LONG
#endif
#define HYDROGEN_FLOAT_TYPE	HYDROGEN_FLOAT_FLOAT

#elif HYDROGEN_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define HYDROGEN_INT_TYPE	HYDROGEN_INT_LONG
#define HYDROGEN_FLOAT_TYPE	HYDROGEN_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define HYDROGEN_INT_TYPE	HYDROGEN_INT_DEFAULT
#define HYDROGEN_FLOAT_TYPE	HYDROGEN_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** HYDROGEN_PATH_SEP is the character that separates templates in a path.
** HYDROGEN_PATH_MARK is the string that marks the substitution points in a
** template.
** HYDROGEN_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define HYDROGEN_PATH_SEP            ";"
#define HYDROGEN_PATH_MARK           "?"
#define HYDROGEN_EXEC_DIR            "!"


/*
@@ HYDROGEN_PATH_DEFAULT is the default path that Hydrogen uses to look for
** Hydrogen libraries.
@@ HYDROGEN_CPATH_DEFAULT is the default path that Hydrogen uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define HYDROGEN_VDIR	HYDROGEN_VERSION_MAJOR "." HYDROGEN_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define HYDROGEN_LDIR	"!\\hydrogen\\"
#define HYDROGEN_CDIR	"!\\"
#define HYDROGEN_SHRDIR	"!\\..\\share\\hydrogen\\" HYDROGEN_VDIR "\\"

#if !defined(HYDROGEN_PATH_DEFAULT)
#define HYDROGEN_PATH_DEFAULT  \
		HYDROGEN_LDIR"?.hydrogen;"  HYDROGEN_LDIR"?\\init.hydrogen;" \
		HYDROGEN_CDIR"?.hydrogen;"  HYDROGEN_CDIR"?\\init.hydrogen;" \
		HYDROGEN_SHRDIR"?.hydrogen;" HYDROGEN_SHRDIR"?\\init.hydrogen;" \
		".\\?.hydrogen;" ".\\?\\init.hydrogen"
#endif

#if !defined(HYDROGEN_CPATH_DEFAULT)
#define HYDROGEN_CPATH_DEFAULT \
		HYDROGEN_CDIR"?.dll;" \
		HYDROGEN_CDIR"..\\lib\\hydrogen\\" HYDROGEN_VDIR "\\?.dll;" \
		HYDROGEN_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define HYDROGEN_ROOT	"/usr/local/"
#define HYDROGEN_LDIR	HYDROGEN_ROOT "share/hydrogen/" HYDROGEN_VDIR "/"
#define HYDROGEN_CDIR	HYDROGEN_ROOT "lib/hydrogen/" HYDROGEN_VDIR "/"

#if !defined(HYDROGEN_PATH_DEFAULT)
#define HYDROGEN_PATH_DEFAULT  \
		HYDROGEN_LDIR"?.hydrogen;"  HYDROGEN_LDIR"?/init.hydrogen;" \
		HYDROGEN_CDIR"?.hydrogen;"  HYDROGEN_CDIR"?/init.hydrogen;" \
		"./?.hydrogen;" "./?/init.hydrogen"
#endif

#if !defined(HYDROGEN_CPATH_DEFAULT)
#define HYDROGEN_CPATH_DEFAULT \
		HYDROGEN_CDIR"?.so;" HYDROGEN_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ HYDROGEN_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Hydrogen automatically uses "\".)
*/
#if !defined(HYDROGEN_DIRSEP)

#if defined(_WIN32)
#define HYDROGEN_DIRSEP	"\\"
#else
#define HYDROGEN_DIRSEP	"/"
#endif

#endif

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ HYDROGEN_API is a mark for all core API functions.
@@ HYDROGENLIB_API is a mark for all auxiliary library functions.
@@ HYDROGENMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** HYDROGEN_BUILD_AS_DLL to get it).
*/
#if defined(HYDROGEN_BUILD_AS_DLL)	/* { */

#if defined(HYDROGEN_CORE) || defined(HYDROGEN_LIB)	/* { */
#define HYDROGEN_API __declspec(dlexerport)
#else						/* }{ */
#define HYDROGEN_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define HYDROGEN_API		extern

#endif				/* } */


/*
** More often than not the libs Hydrogen together with the core.
*/
#define HYDROGENLIB_API	HYDROGEN_API
#define HYDROGENMOD_API	HYDROGEN_API


/*
@@ HYDROGENI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ HYDROGENI_DDEF and HYDROGENI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (HYDROGENI_DDEF for
** definitions and HYDROGENI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Hydrogen is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define HYDROGENI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define HYDROGENI_FUNC	extern
#endif				/* } */

#define HYDROGENI_DDEC(dec)	HYDROGENI_FUNC dec
#define HYDROGENI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ HYDROGEN_COMPAT_5_3 controls other macros for compatibility with Hydrogen 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(HYDROGEN_COMPAT_5_3)	/* { */

/*
@@ HYDROGEN_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define HYDROGEN_COMPAT_MATHLIB

/*
@@ HYDROGEN_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (hydrogen_pushunsigned, hydrogen_tounsigned,
** hydrogenL_checkint, hydrogenL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define HYDROGEN_COMPAT_APIINTCASTS


/*
@@ HYDROGEN_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define HYDROGEN_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define hydrogen_strlen(L,i)		hydrogen_rawlen(L, (i))

#define hydrogen_objlen(L,i)		hydrogen_rawlen(L, (i))

#define hydrogen_equal(L,idx1,idx2)		hydrogen_compare(L,(idx1),(idx2),HYDROGEN_OPEQ)
#define hydrogen_lessthan(L,idx1,idx2)	hydrogen_compare(L,(idx1),(idx2),HYDROGEN_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined HYDROGEN_FLOAT_* / HYDROGEN_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ HYDROGENI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ HYDROGEN_NUMBER_FRMLEN is the length modifier for writing floats.
@@ HYDROGEN_NUMBER_FMT is the format for writing floats.
@@ hydrogen_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ hydrogen_str2number converts a decimal numeral to a number.
*/


/* The following definitions are Hydrogenod for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define hydrogen_number2str(s,sz,n)  \
	l_sprintf((s), sz, HYDROGEN_NUMBER_FMT, (HYDROGENI_UACNUMBER)(n))

/*
@@ hydrogen_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a hydrogen_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define hydrogen_numbertointeger(n,p) \
  ((n) >= (HYDROGEN_NUMBER)(HYDROGEN_MININTEGER) && \
   (n) < -(HYDROGEN_NUMBER)(HYDROGEN_MININTEGER) && \
      (*(p) = (HYDROGEN_INTEGER)(n), 1))


/* now the variable definitions */

#if HYDROGEN_FLOAT_TYPE == HYDROGEN_FLOAT_FLOAT		/* { single float */

#define HYDROGEN_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define HYDROGENI_UACNUMBER	double

#define HYDROGEN_NUMBER_FRMLEN	""
#define HYDROGEN_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define hydrogen_str2number(s,p)	strtof((s), (p))


#elif HYDROGEN_FLOAT_TYPE == HYDROGEN_FLOAT_LONGDOUBLE	/* }{ long double */

#define HYDROGEN_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define HYDROGENI_UACNUMBER	long double

#define HYDROGEN_NUMBER_FRMLEN	"L"
#define HYDROGEN_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define hydrogen_str2number(s,p)	strtold((s), (p))

#elif HYDROGEN_FLOAT_TYPE == HYDROGEN_FLOAT_DOUBLE	/* }{ double */

#define HYDROGEN_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define HYDROGENI_UACNUMBER	double

#define HYDROGEN_NUMBER_FRMLEN	""
#define HYDROGEN_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define hydrogen_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ HYDROGEN_UNSIGNED is the unsigned version of HYDROGEN_INTEGER.
@@ HYDROGENI_UACINT is the result of a 'default argument promotion'
@@ over a HYDROGEN_INTEGER.
@@ HYDROGEN_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ HYDROGEN_INTEGER_FMT is the format for writing integers.
@@ HYDROGEN_MAXINTEGER is the maximum value for a HYDROGEN_INTEGER.
@@ HYDROGEN_MININTEGER is the minimum value for a HYDROGEN_INTEGER.
@@ HYDROGEN_MAXUNSIGNED is the maximum value for a HYDROGEN_UNSIGNED.
@@ hydrogen_integer2str converts an integer to a string.
*/


/* The following definitions are Hydrogenod for most cases here */

#define HYDROGEN_INTEGER_FMT		"%" HYDROGEN_INTEGER_FRMLEN "d"

#define HYDROGENI_UACINT		HYDROGEN_INTEGER

#define hydrogen_integer2str(s,sz,n)  \
	l_sprintf((s), sz, HYDROGEN_INTEGER_FMT, (HYDROGENI_UACINT)(n))

/*
** use HYDROGENI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define HYDROGEN_UNSIGNED		unsigned HYDROGENI_UACINT


/* now the variable definitions */

#if HYDROGEN_INT_TYPE == HYDROGEN_INT_INT		/* { int */

#define HYDROGEN_INTEGER		int
#define HYDROGEN_INTEGER_FRMLEN	""

#define HYDROGEN_MAXINTEGER		INT_MAX
#define HYDROGEN_MININTEGER		INT_MIN

#define HYDROGEN_MAXUNSIGNED		UINT_MAX

#elif HYDROGEN_INT_TYPE == HYDROGEN_INT_LONG	/* }{ long */

#define HYDROGEN_INTEGER		long
#define HYDROGEN_INTEGER_FRMLEN	"l"

#define HYDROGEN_MAXINTEGER		LONG_MAX
#define HYDROGEN_MININTEGER		LONG_MIN

#define HYDROGEN_MAXUNSIGNED		ULONG_MAX

#elif HYDROGEN_INT_TYPE == HYDROGEN_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define HYDROGEN_INTEGER		long long
#define HYDROGEN_INTEGER_FRMLEN	"ll"

#define HYDROGEN_MAXINTEGER		LLONG_MAX
#define HYDROGEN_MININTEGER		LLONG_MIN

#define HYDROGEN_MAXUNSIGNED		ULLONG_MAX

#elif defined(HYDROGEN_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define HYDROGEN_INTEGER		__int64
#define HYDROGEN_INTEGER_FRMLEN	"I64"

#define HYDROGEN_MAXINTEGER		_I64_MAX
#define HYDROGEN_MININTEGER		_I64_MIN

#define HYDROGEN_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DHYDROGEN_32BITS' \
  or '-DHYDROGEN_C89_NUMBERS' (see file 'hydrogenconf.h' for details)"

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
** (All uses in Hydrogen have only one format item.)
*/
#if !defined(HYDROGEN_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ hydrogen_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'hydrogen_strx2number' undefined and Hydrogen will provide its own
** implementation.
*/
#if !defined(HYDROGEN_USE_C89)
#define hydrogen_strx2number(s,p)		hydrogen_str2number(s,p)
#endif


/*
@@ hydrogen_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define hydrogen_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ hydrogen_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'hydrogen_number2strx' undefined and Hydrogen will
** provide its own implementation.
*/
#if !defined(HYDROGEN_USE_C89)
#define hydrogen_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(HYDROGENI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a Hydrogenod proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(HYDROGEN_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef hydrogen_str2number
#define l_mathop(op)		(hydrogen_Number)op  /* no variant */
#define hydrogen_str2number(s,p)	((hydrogen_Number)strtod((s), (p)))
#endif


/*
@@ HYDROGEN_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Hydrogen will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define HYDROGEN_KCONTEXT	ptrdiff_t

#if !defined(HYDROGEN_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef HYDROGEN_KCONTEXT
#define HYDROGEN_KCONTEXT	intptr_t
#endif
#endif


/*
@@ hydrogen_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(hydrogen_getlocaledecpoint)
#define hydrogen_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Hydrogen API use these macros.
** Define HYDROGEN_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(hydrogeni_likely)

#if defined(__GNUC__) && !defined(HYDROGEN_NOBUILTIN)
#define hydrogeni_likely(x)		(__builtin_expect(((x) != 0), 1))
#define hydrogeni_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define hydrogeni_likely(x)		(x)
#define hydrogeni_unlikely(x)	(x)
#endif

#endif


#if defined(HYDROGEN_CORE) || defined(HYDROGEN_LIB)
/* shorter names for Hydrogen's own use */
#define l_likely(x)	hydrogeni_likely(x)
#define l_unlikely(x)	hydrogeni_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ HYDROGEN_NOCVTN2S/HYDROGEN_NOCVTS2N control how Hydrogen performs some
** coercions. Define HYDROGEN_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define HYDROGEN_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define HYDROGEN_NOCVTN2S */
/* #define HYDROGEN_NOCVTS2N */


/*
@@ HYDROGEN_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(HYDROGEN_USE_APICHECK)
#include <assert.h>
#define hydrogeni_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Hydrogen and when you compile code that links to
** Hydrogen).
** =====================================================================
*/

/*
@@ HYDROGENI_MAXSTACK limits the size of the Hydrogen stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Hydrogen from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32.)
*/
#if HYDROGENI_IS32INT
#define HYDROGENI_MAXSTACK		1000000
#else
#define HYDROGENI_MAXSTACK		15000
#endif


/*
@@ HYDROGEN_EXTRASPACE defines the size of a raw memory area associated with
** a Hydrogen state with very fast access.
** CHANGE it if you need a different size.
*/
#define HYDROGEN_EXTRASPACE		(sizeof(void *))


/*
@@ HYDROGEN_IDSIZE gives the maximum size for the description of the source
@@ of a function in debug information.
** CHANGE it if you want a different size.
*/
#define HYDROGEN_IDSIZE	60


/*
@@ HYDROGENL_BUFFERSIZE is the buffer size used by the auxlib buffer system.
*/
#define HYDROGENL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(hydrogen_Number)))


/*
@@ HYDROGENI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define HYDROGENI_MAXALIGN  hydrogen_Number n; double u; void *s; hydrogen_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

