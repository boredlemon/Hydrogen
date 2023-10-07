/*
** $Id: lundump.h $
** load precompiled Cup chunks
** See Copyright Notice in cup.h
*/

#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define CUPC_DATA	"\x19\x93\r\n\x1a\n"

#define CUPC_INT	0x5678
#define CUPC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define CUPC_VERSION	(MYINT(CUP_VERSION_MAJOR)*16+MYINT(CUP_VERSION_MINOR))

#define CUPC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
CUPI_FUNC LClosure* cupU_undump (cup_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
CUPI_FUNC int cupU_dump (cup_State* L, const Proto* f, cup_Writer w,
                         void* data, int strip);

#endif
