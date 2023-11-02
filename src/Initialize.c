/*
** $Id: Initialize.c $
** Initialization of libraries for venom.c and other clients
** See Copyright Notice in venom.h
*/


#define Initialize_c
#define VENOM_LIB

/*
** If you embed Venom in your program and need to open the standard
** libraries, call venomL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  venomL_getsubtable(L, VENOM_REGISTRYINDEX, VENOM_PRELOAD_TABLE);
**  venom_pushcfunction(L, venomopen_modname);
**  venom_setfield(L, -2, modname);
**  venom_pop(L, 1);  // remove PRELOAD table
*/

#include "prefix.h"


#include <stddef.h>

#include "venom.h"

#include "venomlib.h"
#include "auxlib.h"


/*
** these libs are loaded by venom.c and are readily available to any Venom
** program
*/
static const venomL_Reg loadedlibs[] = {
  {VENOM_GNAME, venomopen_base},
  {VENOM_LOADLIBNAME, venomopen_package},
  {VENOM_COLIBNAME, venomopen_coroutine},
  {VENOM_TABLIBNAME, venomopen_table},
  {VENOM_IOLIBNAME, venomopen_io},
  {VENOM_OSLIBNAME, venomopen_os},
  {VENOM_STRLIBNAME, venomopen_string},
  {VENOM_MATHLIBNAME, venomopen_math},
  {VENOM_UTF8LIBNAME, venomopen_utf8},
  {VENOM_DBLIBNAME, venomopen_debug},
  {NULL, NULL}
};


VENOMLIB_API void venomL_openlibs (venom_State *L) {
  const venomL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    venomL_requiref(L, lib->name, lib->func, 1);
    venom_pop(L, 1);  /* remove lib */
  }
}

