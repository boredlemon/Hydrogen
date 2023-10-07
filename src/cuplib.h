/*
** $Id: cuplib.h $
** Cup standard libraries
** See Copyright Notice in cup.h
*/


#ifndef cuplib_h
#define cuplib_h

#include "cup.h"


/* version suffix for environment variable names */
#define CUP_VERSUFFIX          "_" CUP_VERSION_MAJOR "_" CUP_VERSION_MINOR


CUPMOD_API int (cupopen_base) (cup_State *L);

#define CUP_COLIBNAME	"coroutine"
CUPMOD_API int (cupopen_coroutine) (cup_State *L);

#define CUP_TABLIBNAME	"table"
CUPMOD_API int (cupopen_table) (cup_State *L);

#define CUP_IOLIBNAME	"io"
CUPMOD_API int (cupopen_io) (cup_State *L);

#define CUP_OSLIBNAME	"os"
CUPMOD_API int (cupopen_os) (cup_State *L);

#define CUP_STRLIBNAME	"string"
CUPMOD_API int (cupopen_string) (cup_State *L);

#define CUP_UTF8LIBNAME	"utf8"
CUPMOD_API int (cupopen_utf8) (cup_State *L);

#define CUP_MATHLIBNAME	"math"
CUPMOD_API int (cupopen_math) (cup_State *L);

#define CUP_DBLIBNAME	"debug"
CUPMOD_API int (cupopen_debug) (cup_State *L);

#define CUP_LOADLIBNAME	"package"
CUPMOD_API int (cupopen_package) (cup_State *L);


/* open all previous libraries */
CUPLIB_API void (cupL_openlibs) (cup_State *L);


#endif
