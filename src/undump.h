/*
** $Id: undump.h $
** load precompiled Venom chunks
** See Copyright Notice in venom.h
*/

#ifndef undump_h
#define undump_h

#include "limits.h"
#include "object.h"
#include "zio.h"


/* data to catch conversion errors */
#define VENOMC_DATA	"\x19\x93\r\n\x1a\n"

#define VENOMC_INT	0x5678
#define VENOMC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define VENOMC_VERSION	(MYINT(VENOM_VERSION_MAJOR)*16+MYINT(VENOM_VERSION_MINOR))

#define VENOMC_FORMAT	0	/* this is the official format */

/* load one chunk; from undump.c */
VENOMI_FUNC LClosure* venomU_undump (venom_State* L, ZIO* Z, const char* name);

/* dump one chunk; from dump.c */
VENOMI_FUNC int venomU_dump (venom_State* L, const Proto* f, venom_Writer w,
                         void* data, int strip);

#endif
