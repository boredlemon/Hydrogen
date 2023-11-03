/*
** $Id: undump.h $
** load precompiled Nebula chunks
** See Copyright Notice in nebula.h
*/

#ifndef undump_h
#define undump_h

#include "limits.h"
#include "object.h"
#include "zio.h"


/* data to catch conversion errors */
#define NEBULAC_DATA	"\x19\x93\r\n\x1a\n"

#define NEBULAC_INT	0x5678
#define NEBULAC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define NEBULAC_VERSION	(MYINT(NEBULA_VERSION_MAJOR)*16+MYINT(NEBULA_VERSION_MINOR))

#define NEBULAC_FORMAT	0	/* this is the official format */

/* load one chunk; from undump.c */
NEBULAI_FUNC LClosure* nebulaU_undump (nebula_State* L, ZIO* Z, const char* name);

/* dump one chunk; from dump.c */
NEBULAI_FUNC int nebulaU_dump (nebula_State* L, const Proto* f, nebula_Writer w,
                         void* data, int strip);

#endif
