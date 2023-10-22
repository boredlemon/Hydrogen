/*
** $Id: viperlib.h $
** Viper standard libraries
** See Copyright Notice in viper.h
*/


#ifndef viperlib_h
#define viperlib_h

#include "viper.h"


/* version suffix for environment variable names */
#define VIPER_VERSUFFIX          "_" VIPER_VERSION_MAJOR "_" VIPER_VERSION_MINOR


VIPERMOD_API int (viperopen_base) (viper_State *L);

#define VIPER_COLIBNAME	"coroutine"
VIPERMOD_API int (viperopen_coroutine) (viper_State *L);

#define VIPER_TABLIBNAME	"table"
VIPERMOD_API int (viperopen_table) (viper_State *L);

#define VIPER_IOLIBNAME	"io"
VIPERMOD_API int (viperopen_io) (viper_State *L);

#define VIPER_OSLIBNAME	"os"
VIPERMOD_API int (viperopen_os) (viper_State *L);

#define VIPER_STRLIBNAME	"string"
VIPERMOD_API int (viperopen_string) (viper_State *L);

#define VIPER_UTF8LIBNAME	"utf8"
VIPERMOD_API int (viperopen_utf8) (viper_State *L);

#define VIPER_MATHLIBNAME	"math"
VIPERMOD_API int (viperopen_math) (viper_State *L);

#define VIPER_DBLIBNAME	"debug"
VIPERMOD_API int (viperopen_debug) (viper_State *L);

#define VIPER_LOADLIBNAME	"package"
VIPERMOD_API int (viperopen_package) (viper_State *L);


/* open all previous libraries */
VIPERLIB_API void (viperL_openlibs) (viper_State *L);


#endif
