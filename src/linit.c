/*
** $Id: linit.c $
** Initialization of libraries for acorn.c and other clients
** See Copyright Notice in acorn.h
*/


#define linit_c
#define ACORN_LIB

/*
** If you embed Acorn in your program and need to open the standard
** libraries, call acornL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  acornL_getsubtable(L, ACORN_REGISTRYINDEX, ACORN_PRELOAD_TABLE);
**  acorn_pushcfunction(L, acornopen_modname);
**  acorn_setfield(L, -2, modname);
**  acorn_pop(L, 1);  // remove PRELOAD table
*/

#include "lprefix.h"


#include <stddef.h>

#include "acorn.h"

#include "acornlib.h"
#include "lauxlib.h"


/*
** these libs are loaded by acorn.c and are readily available to any Acorn
** program
*/
static const acornL_Reg loadedlibs[] = {
  {ACORN_GNAME, acornopen_base},
  {ACORN_LOADLIBNAME, acornopen_package},
  {ACORN_COLIBNAME, acornopen_coroutine},
  {ACORN_TABLIBNAME, acornopen_table},
  {ACORN_IOLIBNAME, acornopen_io},
  {ACORN_OSLIBNAME, acornopen_os},
  {ACORN_STRLIBNAME, acornopen_string},
  {ACORN_MATHLIBNAME, acornopen_math},
  {ACORN_UTF8LIBNAME, acornopen_utf8},
  {ACORN_DBLIBNAME, acornopen_debug},
  {NULL, NULL}
};


ACORNLIB_API void acornL_openlibs (acorn_State *L) {
  const acornL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    acornL_requiref(L, lib->name, lib->func, 1);
    acorn_pop(L, 1);  /* remove lib */
  }
}

