/*
** $Id: lundump.h $
** load precompiled Acorn chunks
** See Copyright Notice in acorn.h
*/

#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define ACORNC_DATA	"\x19\x93\r\n\x1a\n"

#define ACORNC_INT	0x5678
#define ACORNC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define ACORNC_VERSION	(MYINT(ACORN_VERSION_MAJOR)*16+MYINT(ACORN_VERSION_MINOR))

#define ACORNC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
ACORNI_FUNC LClosure* acornU_undump (acorn_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
ACORNI_FUNC int acornU_dump (acorn_State* L, const Proto* f, acorn_Writer w,
                         void* data, int strip);

#endif
