/*
** $Id: hydrogenlib.h $
** Hydrogen standard libraries
** See Copyright Notice in hydrogen.h
*/


#ifndef hydrogenlib_h
#define hydrogenlib_h

#include "hydrogen.h"


/* version suffix for environment variable names */
#define HYDROGEN_VERSUFFIX          "_" HYDROGEN_VERSION_MAJOR "_" HYDROGEN_VERSION_MINOR


HYDROGENMOD_API int (hydrogenopen_base) (hydrogen_State *L);

#define HYDROGEN_COLIBNAME	"coroutine"
HYDROGENMOD_API int (hydrogenopen_coroutine) (hydrogen_State *L);

#define HYDROGEN_TABLIBNAME	"table"
HYDROGENMOD_API int (hydrogenopen_table) (hydrogen_State *L);

#define HYDROGEN_IOLIBNAME	"io"
HYDROGENMOD_API int (hydrogenopen_io) (hydrogen_State *L);

#define HYDROGEN_OSLIBNAME	"os"
HYDROGENMOD_API int (hydrogenopen_os) (hydrogen_State *L);

#define HYDROGEN_STRLIBNAME	"string"
HYDROGENMOD_API int (hydrogenopen_string) (hydrogen_State *L);

#define HYDROGEN_UTF8LIBNAME	"utf8"
HYDROGENMOD_API int (hydrogenopen_utf8) (hydrogen_State *L);

#define HYDROGEN_MATHLIBNAME	"math"
HYDROGENMOD_API int (hydrogenopen_math) (hydrogen_State *L);

#define HYDROGEN_DBLIBNAME	"debug"
HYDROGENMOD_API int (hydrogenopen_debug) (hydrogen_State *L);

#define HYDROGEN_LOADLIBNAME	"package"
HYDROGENMOD_API int (hydrogenopen_package) (hydrogen_State *L);


/* open all previous libraries */
HYDROGENLIB_API void (hydrogenL_openlibs) (hydrogen_State *L);


#endif
