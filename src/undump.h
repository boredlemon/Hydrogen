/*
** $Id: undump.h $
** load precompiled Hydrogen chunks
** See Copyright Notice in hydrogen.h
*/

#ifndef undump_h
#define undump_h

#include "limits.h"
#include "object.h"
#include "zio.h"


/* data to catch conversion errors */
#define HYDROGENC_DATA	"\x19\x93\r\n\x1a\n"

#define HYDROGENC_INT	0x5678
#define HYDROGENC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define HYDROGENC_VERSION	(MYINT(HYDROGEN_VERSION_MAJOR)*16+MYINT(HYDROGEN_VERSION_MINOR))

#define HYDROGENC_FORMAT	0	/* this is the official format */

/* load one chunk; from undump.c */
HYDROGENI_FUNC LClosure* hydrogenU_undump (hydrogen_State* L, ZIO* Z, const char* name);

/* dump one chunk; from dump.c */
HYDROGENI_FUNC int hydrogenU_dump (hydrogen_State* L, const Proto* f, hydrogen_Writer w,
                         void* data, int strip);

#endif
