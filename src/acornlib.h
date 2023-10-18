/*
** $Id: acornlib.h $
** Acorn standard libraries
** See Copyright Notice in acorn.h
*/


#ifndef acornlib_h
#define acornlib_h

#include "acorn.h"


/* version suffix for environment variable names */
#define ACORN_VERSUFFIX          "_" ACORN_VERSION_MAJOR "_" ACORN_VERSION_MINOR


ACORNMOD_API int (acornopen_base) (acorn_State *L);

#define ACORN_COLIBNAME	"coroutine"
ACORNMOD_API int (acornopen_coroutine) (acorn_State *L);

#define ACORN_TABLIBNAME	"table"
ACORNMOD_API int (acornopen_table) (acorn_State *L);

#define ACORN_IOLIBNAME	"io"
ACORNMOD_API int (acornopen_io) (acorn_State *L);

#define ACORN_OSLIBNAME	"os"
ACORNMOD_API int (acornopen_os) (acorn_State *L);

#define ACORN_STRLIBNAME	"string"
ACORNMOD_API int (acornopen_string) (acorn_State *L);

#define ACORN_UTF8LIBNAME	"utf8"
ACORNMOD_API int (acornopen_utf8) (acorn_State *L);

#define ACORN_MATHLIBNAME	"math"
ACORNMOD_API int (acornopen_math) (acorn_State *L);

#define ACORN_DBLIBNAME	"debug"
ACORNMOD_API int (acornopen_debug) (acorn_State *L);

#define ACORN_LOADLIBNAME	"package"
ACORNMOD_API int (acornopen_package) (acorn_State *L);


/* open all previous libraries */
ACORNLIB_API void (acornL_openlibs) (acorn_State *L);


#endif
