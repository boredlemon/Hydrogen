/*
** $Id: venomlib.h $
** Venom standard libraries
** See Copyright Notice in venom.h
*/


#ifndef venomlib_h
#define venomlib_h

#include "venom.h"


/* version suffix for environment variable names */
#define VENOM_VERSUFFIX          "_" VENOM_VERSION_MAJOR "_" VENOM_VERSION_MINOR


VENOMMOD_API int (venomopen_base) (venom_State *L);

#define VENOM_COLIBNAME	"coroutine"
VENOMMOD_API int (venomopen_coroutine) (venom_State *L);

#define VENOM_TABLIBNAME	"table"
VENOMMOD_API int (venomopen_table) (venom_State *L);

#define VENOM_IOLIBNAME	"io"
VENOMMOD_API int (venomopen_io) (venom_State *L);

#define VENOM_OSLIBNAME	"os"
VENOMMOD_API int (venomopen_os) (venom_State *L);

#define VENOM_STRLIBNAME	"string"
VENOMMOD_API int (venomopen_string) (venom_State *L);

#define VENOM_UTF8LIBNAME	"utf8"
VENOMMOD_API int (venomopen_utf8) (venom_State *L);

#define VENOM_MATHLIBNAME	"math"
VENOMMOD_API int (venomopen_math) (venom_State *L);

#define VENOM_DBLIBNAME	"debug"
VENOMMOD_API int (venomopen_debug) (venom_State *L);

#define VENOM_LOADLIBNAME	"package"
VENOMMOD_API int (venomopen_package) (venom_State *L);


/* open all previous libraries */
VENOMLIB_API void (venomL_openlibs) (venom_State *L);


#endif
