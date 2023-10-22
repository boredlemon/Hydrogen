/*
** $Id: lundump.h $
** load precompiled Viper chunks
** See Copyright Notice in viper.h
*/

#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define VIPERC_DATA	"\x19\x93\r\n\x1a\n"

#define VIPERC_INT	0x5678
#define VIPERC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define VIPERC_VERSION	(MYINT(VIPER_VERSION_MAJOR)*16+MYINT(VIPER_VERSION_MINOR))

#define VIPERC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
VIPERI_FUNC LClosure* viperU_undump (viper_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
VIPERI_FUNC int viperU_dump (viper_State* L, const Proto* f, viper_Writer w,
                         void* data, int strip);

#endif
