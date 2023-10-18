/*
** $Id: acornconf.h $
** Configuration file for Acorn
** See Copyright Notice in acorn.h
*/


#ifndef acornconf_h
#define acornconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Acorn
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Acorn ABI (by making the changes here, you ensure that all software
** connected to Acorn, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Acorn to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ ACORN_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Acorn to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define ACORN_USE_C89 */


/*
** By default, Acorn on Windows use (some) specific Windows features
*/
#if !defined(ACORN_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define ACORN_USE_WINDOWS  /* enable Acornodies for regular Windows */
#endif


#if defined(ACORN_USE_WINDOWS)
#define ACORN_DL_DLL	/* enable support for DLL */
#define ACORN_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(ACORN_USE_LINUX)
#define ACORN_USE_POSIX
#define ACORN_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(ACORN_USE_MACOSX)
#define ACORN_USE_POSIX
#define ACORN_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


/*
@@ ACORNI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define ACORNI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Acorn must
** use the same configuration.
** ===================================================================
*/

/*
@@ ACORN_INT_TYPE defines the type for Acorn integers.
@@ ACORN_FLOAT_TYPE defines the type for Acorn floats.
** Acorn should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for ACORN_INT_TYPE */
#define ACORN_INT_INT		1
#define ACORN_INT_LONG		2
#define ACORN_INT_LONGLONG	3

/* predefined options for ACORN_FLOAT_TYPE */
#define ACORN_FLOAT_FLOAT		1
#define ACORN_FLOAT_DOUBLE	2
#define ACORN_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Acorn) */
#define ACORN_INT_DEFAULT		ACORN_INT_LONGLONG
#define ACORN_FLOAT_DEFAULT	ACORN_FLOAT_DOUBLE


/*
@@ ACORN_32BITS enables Acorn with 32-bit integers and 32-bit floats.
*/
#define ACORN_32BITS	0


/*
@@ ACORN_C89_NUMBERS ensures that Acorn uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(ACORN_USE_C89) && !defined(ACORN_USE_WINDOWS)
#define ACORN_C89_NUMBERS		1
#else
#define ACORN_C89_NUMBERS		0
#endif


#if ACORN_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if ACORNI_IS32INT  /* use 'int' if big enough */
#define ACORN_INT_TYPE	ACORN_INT_INT
#else  /* otherwise use 'long' */
#define ACORN_INT_TYPE	ACORN_INT_LONG
#endif
#define ACORN_FLOAT_TYPE	ACORN_FLOAT_FLOAT

#elif ACORN_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define ACORN_INT_TYPE	ACORN_INT_LONG
#define ACORN_FLOAT_TYPE	ACORN_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define ACORN_INT_TYPE	ACORN_INT_DEFAULT
#define ACORN_FLOAT_TYPE	ACORN_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** ACORN_PATH_SEP is the character that separates templates in a path.
** ACORN_PATH_MARK is the string that marks the substitution points in a
** template.
** ACORN_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define ACORN_PATH_SEP            ";"
#define ACORN_PATH_MARK           "?"
#define ACORN_EXEC_DIR            "!"


/*
@@ ACORN_PATH_DEFAULT is the default path that Acorn uses to look for
** Acorn libraries.
@@ ACORN_CPATH_DEFAULT is the default path that Acorn uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define ACORN_VDIR	ACORN_VERSION_MAJOR "." ACORN_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define ACORN_LDIR	"!\\acorn\\"
#define ACORN_CDIR	"!\\"
#define ACORN_SHRDIR	"!\\..\\share\\acorn\\" ACORN_VDIR "\\"

#if !defined(ACORN_PATH_DEFAULT)
#define ACORN_PATH_DEFAULT  \
		ACORN_LDIR"?.acorn;"  ACORN_LDIR"?\\init.acorn;" \
		ACORN_CDIR"?.acorn;"  ACORN_CDIR"?\\init.acorn;" \
		ACORN_SHRDIR"?.acorn;" ACORN_SHRDIR"?\\init.acorn;" \
		".\\?.acorn;" ".\\?\\init.acorn"
#endif

#if !defined(ACORN_CPATH_DEFAULT)
#define ACORN_CPATH_DEFAULT \
		ACORN_CDIR"?.dll;" \
		ACORN_CDIR"..\\lib\\acorn\\" ACORN_VDIR "\\?.dll;" \
		ACORN_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define ACORN_ROOT	"/usr/local/"
#define ACORN_LDIR	ACORN_ROOT "share/acorn/" ACORN_VDIR "/"
#define ACORN_CDIR	ACORN_ROOT "lib/acorn/" ACORN_VDIR "/"

#if !defined(ACORN_PATH_DEFAULT)
#define ACORN_PATH_DEFAULT  \
		ACORN_LDIR"?.acorn;"  ACORN_LDIR"?/init.acorn;" \
		ACORN_CDIR"?.acorn;"  ACORN_CDIR"?/init.acorn;" \
		"./?.acorn;" "./?/init.acorn"
#endif

#if !defined(ACORN_CPATH_DEFAULT)
#define ACORN_CPATH_DEFAULT \
		ACORN_CDIR"?.so;" ACORN_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ ACORN_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Acorn automatically uses "\".)
*/
#if !defined(ACORN_DIRSEP)

#if defined(_WIN32)
#define ACORN_DIRSEP	"\\"
#else
#define ACORN_DIRSEP	"/"
#endif

#endif

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ ACORN_API is a mark for all core API functions.
@@ ACORNLIB_API is a mark for all auxiliary library functions.
@@ ACORNMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** ACORN_BUILD_AS_DLL to get it).
*/
#if defined(ACORN_BUILD_AS_DLL)	/* { */

#if defined(ACORN_CORE) || defined(ACORN_LIB)	/* { */
#define ACORN_API __declspec(dllexport)
#else						/* }{ */
#define ACORN_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define ACORN_API		extern

#endif				/* } */


/*
** More often than not the libs Acorn together with the core.
*/
#define ACORNLIB_API	ACORN_API
#define ACORNMOD_API	ACORN_API


/*
@@ ACORNI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ ACORNI_DDEF and ACORNI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (ACORNI_DDEF for
** definitions and ACORNI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Acorn is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define ACORNI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define ACORNI_FUNC	extern
#endif				/* } */

#define ACORNI_DDEC(dec)	ACORNI_FUNC dec
#define ACORNI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ ACORN_COMPAT_5_3 controls other macros for compatibility with Acorn 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(ACORN_COMPAT_5_3)	/* { */

/*
@@ ACORN_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define ACORN_COMPAT_MATHLIB

/*
@@ ACORN_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (acorn_pushunsigned, acorn_tounsigned,
** acornL_checkint, acornL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define ACORN_COMPAT_APIINTCASTS


/*
@@ ACORN_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define ACORN_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define acorn_strlen(L,i)		acorn_rawlen(L, (i))

#define acorn_objlen(L,i)		acorn_rawlen(L, (i))

#define acorn_equal(L,idx1,idx2)		acorn_compare(L,(idx1),(idx2),ACORN_OPEQ)
#define acorn_lessthan(L,idx1,idx2)	acorn_compare(L,(idx1),(idx2),ACORN_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined ACORN_FLOAT_* / ACORN_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ ACORNI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ ACORN_NUMBER_FRMLEN is the length modifier for writing floats.
@@ ACORN_NUMBER_FMT is the format for writing floats.
@@ acorn_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ acorn_str2number converts a decimal numeral to a number.
*/


/* The following definitions are Acornod for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define acorn_number2str(s,sz,n)  \
	l_sprintf((s), sz, ACORN_NUMBER_FMT, (ACORNI_UACNUMBER)(n))

/*
@@ acorn_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a acorn_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define acorn_numbertointeger(n,p) \
  ((n) >= (ACORN_NUMBER)(ACORN_MININTEGER) && \
   (n) < -(ACORN_NUMBER)(ACORN_MININTEGER) && \
      (*(p) = (ACORN_INTEGER)(n), 1))


/* now the variable definitions */

#if ACORN_FLOAT_TYPE == ACORN_FLOAT_FLOAT		/* { single float */

#define ACORN_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define ACORNI_UACNUMBER	double

#define ACORN_NUMBER_FRMLEN	""
#define ACORN_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define acorn_str2number(s,p)	strtof((s), (p))


#elif ACORN_FLOAT_TYPE == ACORN_FLOAT_LONGDOUBLE	/* }{ long double */

#define ACORN_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define ACORNI_UACNUMBER	long double

#define ACORN_NUMBER_FRMLEN	"L"
#define ACORN_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define acorn_str2number(s,p)	strtold((s), (p))

#elif ACORN_FLOAT_TYPE == ACORN_FLOAT_DOUBLE	/* }{ double */

#define ACORN_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define ACORNI_UACNUMBER	double

#define ACORN_NUMBER_FRMLEN	""
#define ACORN_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define acorn_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ ACORN_UNSIGNED is the unsigned version of ACORN_INTEGER.
@@ ACORNI_UACINT is the result of a 'default argument promotion'
@@ over a ACORN_INTEGER.
@@ ACORN_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ ACORN_INTEGER_FMT is the format for writing integers.
@@ ACORN_MAXINTEGER is the maximum value for a ACORN_INTEGER.
@@ ACORN_MININTEGER is the minimum value for a ACORN_INTEGER.
@@ ACORN_MAXUNSIGNED is the maximum value for a ACORN_UNSIGNED.
@@ acorn_integer2str converts an integer to a string.
*/


/* The following definitions are Acornod for most cases here */

#define ACORN_INTEGER_FMT		"%" ACORN_INTEGER_FRMLEN "d"

#define ACORNI_UACINT		ACORN_INTEGER

#define acorn_integer2str(s,sz,n)  \
	l_sprintf((s), sz, ACORN_INTEGER_FMT, (ACORNI_UACINT)(n))

/*
** use ACORNI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define ACORN_UNSIGNED		unsigned ACORNI_UACINT


/* now the variable definitions */

#if ACORN_INT_TYPE == ACORN_INT_INT		/* { int */

#define ACORN_INTEGER		int
#define ACORN_INTEGER_FRMLEN	""

#define ACORN_MAXINTEGER		INT_MAX
#define ACORN_MININTEGER		INT_MIN

#define ACORN_MAXUNSIGNED		UINT_MAX

#elif ACORN_INT_TYPE == ACORN_INT_LONG	/* }{ long */

#define ACORN_INTEGER		long
#define ACORN_INTEGER_FRMLEN	"l"

#define ACORN_MAXINTEGER		LONG_MAX
#define ACORN_MININTEGER		LONG_MIN

#define ACORN_MAXUNSIGNED		ULONG_MAX

#elif ACORN_INT_TYPE == ACORN_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define ACORN_INTEGER		long long
#define ACORN_INTEGER_FRMLEN	"ll"

#define ACORN_MAXINTEGER		LLONG_MAX
#define ACORN_MININTEGER		LLONG_MIN

#define ACORN_MAXUNSIGNED		ULLONG_MAX

#elif defined(ACORN_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define ACORN_INTEGER		__int64
#define ACORN_INTEGER_FRMLEN	"I64"

#define ACORN_MAXINTEGER		_I64_MAX
#define ACORN_MININTEGER		_I64_MIN

#define ACORN_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DACORN_32BITS' \
  or '-DACORN_C89_NUMBERS' (see file 'acornconf.h' for details)"

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
** (All uses in Acorn have only one format item.)
*/
#if !defined(ACORN_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ acorn_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'acorn_strx2number' undefined and Acorn will provide its own
** implementation.
*/
#if !defined(ACORN_USE_C89)
#define acorn_strx2number(s,p)		acorn_str2number(s,p)
#endif


/*
@@ acorn_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define acorn_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ acorn_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'acorn_number2strx' undefined and Acorn will
** provide its own implementation.
*/
#if !defined(ACORN_USE_C89)
#define acorn_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(ACORNI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a Acornod proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(ACORN_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef acorn_str2number
#define l_mathop(op)		(acorn_Number)op  /* no variant */
#define acorn_str2number(s,p)	((acorn_Number)strtod((s), (p)))
#endif


/*
@@ ACORN_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Acorn will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define ACORN_KCONTEXT	ptrdiff_t

#if !defined(ACORN_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef ACORN_KCONTEXT
#define ACORN_KCONTEXT	intptr_t
#endif
#endif


/*
@@ acorn_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(acorn_getlocaledecpoint)
#define acorn_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Acorn API use these macros.
** Define ACORN_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(acorni_likely)

#if defined(__GNUC__) && !defined(ACORN_NOBUILTIN)
#define acorni_likely(x)		(__builtin_expect(((x) != 0), 1))
#define acorni_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define acorni_likely(x)		(x)
#define acorni_unlikely(x)	(x)
#endif

#endif


#if defined(ACORN_CORE) || defined(ACORN_LIB)
/* shorter names for Acorn's own use */
#define l_likely(x)	acorni_likely(x)
#define l_unlikely(x)	acorni_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ ACORN_NOCVTN2S/ACORN_NOCVTS2N control how Acorn performs some
** coercions. Define ACORN_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define ACORN_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define ACORN_NOCVTN2S */
/* #define ACORN_NOCVTS2N */


/*
@@ ACORN_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(ACORN_USE_APICHECK)
#include <assert.h>
#define acorni_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Acorn and when you compile code that links to
** Acorn).
** =====================================================================
*/

/*
@@ ACORNI_MAXSTACK limits the size of the Acorn stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Acorn from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32.)
*/
#if ACORNI_IS32INT
#define ACORNI_MAXSTACK		1000000
#else
#define ACORNI_MAXSTACK		15000
#endif


/*
@@ ACORN_EXTRASPACE defines the size of a raw memory area associated with
** a Acorn state with very fast access.
** CHANGE it if you need a different size.
*/
#define ACORN_EXTRASPACE		(sizeof(void *))


/*
@@ ACORN_IDSIZE gives the maximum size for the description of the source
@@ of a function in debug information.
** CHANGE it if you want a different size.
*/
#define ACORN_IDSIZE	60


/*
@@ ACORNL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
*/
#define ACORNL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(acorn_Number)))


/*
@@ ACORNI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define ACORNI_MAXALIGN  acorn_Number n; double u; void *s; acorn_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

