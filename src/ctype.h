/*
** $Id: ctype.h $
** 'ctype' functions for Nebula
** See Copyright Notice in nebula.h
*/

#ifndef ctype_h
#define ctype_h

#include "nebula.h"


/*
** WARNING: the functions defined here do not necessarily correspond
** to the similar functions in the standard C ctype.h. They are
** optimized for the specific needs of Nebula.
*/

#if !defined(NEBULA_USE_CTYPE)

#if 'A' == 65 && '0' == 48
/* ASCII case: can use its own tables; faster and fixed */
#define NEBULA_USE_CTYPE	0
#else
/* must use standard C ctype */
#define NEBULA_USE_CTYPE	1
#endif

#endif


#if !NEBULA_USE_CTYPE	/* { */

#include <limits.h>

#include "limits.h"


#define ALPHABIT	0
#define DIGITBIT	1
#define PRINTBIT	2
#define SPACEBIT	3
#define XDIGITBIT	4


#define MASK(B)		(1 << (B))


/*
** add 1 to char to allow index -1 (EOZ)
*/
#define testprop(c,p)	(nebulai_ctype_[(c)+1] & (p))

/*
** 'lalpha' (Nebula alphabetic) and 'lalnum' (Nebula alphanumeric) both include '_'
*/
#define lislalpha(c)	testprop(c, MASK(ALPHABIT))
#define lislalnum(c)	testprop(c, (MASK(ALPHABIT) | MASK(DIGITBIT)))
#define lisdigit(c)	testprop(c, MASK(DIGITBIT))
#define lisspace(c)	testprop(c, MASK(SPACEBIT))
#define lisprint(c)	testprop(c, MASK(PRINTBIT))
#define lisxdigit(c)	testprop(c, MASK(XDIGITBIT))


/*
** In ASCII, this 'ltolower' is correct for alphabetic characters and
** for '.'. That is enough for Nebula needs. ('check_exp' ensures that
** the character either is an upper-case letter or is unchanged by
** the transformation, which holds for lower-case letters and '.'.)
*/
#define ltolower(c)  \
  check_exp(('A' <= (c) && (c) <= 'Z') || (c) == ((c) | ('A' ^ 'a')),  \
            (c) | ('A' ^ 'a'))


/* one entry for each character and for -1 (EOZ) */
NEBULAI_DDEC(const lu_byte nebulai_ctype_[UCHAR_MAX + 2];)


#else			/* }{ */

/*
** use standard C ctypes
*/

#include <ctype.h>


#define lislalpha(c)	(isalpha(c) || (c) == '_')
#define lislalnum(c)	(isalnum(c) || (c) == '_')
#define lisdigit(c)	(isdigit(c))
#define lisspace(c)	(isspace(c))
#define lisprint(c)	(isprint(c))
#define lisxdigit(c)	(isxdigit(c))

#define ltolower(c)	(tolower(c))

#endif			/* } */

#endif

