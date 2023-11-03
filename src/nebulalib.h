/*
** $Id: nebulalib.h $
** Nebula standard libraries
** See Copyright Notice in nebula.h
*/


#ifndef nebulalib_h
#define nebulalib_h

#include "nebula.h"


/* version suffix for environment variable names */
#define NEBULA_VERSUFFIX          "_" NEBULA_VERSION_MAJOR "_" NEBULA_VERSION_MINOR


NEBULAMOD_API int (nebulaopen_base) (nebula_State *L);

#define NEBULA_COLIBNAME	"coroutine"
NEBULAMOD_API int (nebulaopen_coroutine) (nebula_State *L);

#define NEBULA_TABLIBNAME	"table"
NEBULAMOD_API int (nebulaopen_table) (nebula_State *L);

#define NEBULA_IOLIBNAME	"io"
NEBULAMOD_API int (nebulaopen_io) (nebula_State *L);

#define NEBULA_OSLIBNAME	"os"
NEBULAMOD_API int (nebulaopen_os) (nebula_State *L);

#define NEBULA_STRLIBNAME	"string"
NEBULAMOD_API int (nebulaopen_string) (nebula_State *L);

#define NEBULA_UTF8LIBNAME	"utf8"
NEBULAMOD_API int (nebulaopen_utf8) (nebula_State *L);

#define NEBULA_MATHLIBNAME	"math"
NEBULAMOD_API int (nebulaopen_math) (nebula_State *L);

#define NEBULA_DBLIBNAME	"debug"
NEBULAMOD_API int (nebulaopen_debug) (nebula_State *L);

#define NEBULA_LOADLIBNAME	"package"
NEBULAMOD_API int (nebulaopen_package) (nebula_State *L);


/* open all previous libraries */
NEBULALIB_API void (nebulaL_openlibs) (nebula_State *L);


#endif
