/*
** $Id: Initialize.c $
** Initialization of libraries for hydrogen.c and other clients
** See Copyright Notice in hydrogen.h
*/


#define Initialize_c
#define HYDROGEN_LIB

/*
** If you embed Hydrogen in your program and need to open the standard
** libraries, call hydrogenL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  hydrogenL_getsubtable(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_PRELOAD_TABLE);
**  hydrogen_pushcfunction(L, hydrogenopen_modname);
**  hydrogen_setfield(L, -2, modname);
**  hydrogen_pop(L, 1);  // remove PRELOAD table
*/

#include "prefix.h"


#include <stddef.h>

#include "hydrogen.h"

#include "hydrogenlib.h"
#include "auxlib.h"


/*
** these libs are loaded by hydrogen.c and are readily available to any Hydrogen
** program
*/
static const hydrogenL_Reg loadedlibs[] = {
  {HYDROGEN_GNAME, hydrogenopen_base},
  {HYDROGEN_LOADLIBNAME, hydrogenopen_package},
  {HYDROGEN_COLIBNAME, hydrogenopen_coroutine},
  {HYDROGEN_TABLIBNAME, hydrogenopen_table},
  {HYDROGEN_IOLIBNAME, hydrogenopen_io},
  {HYDROGEN_OSLIBNAME, hydrogenopen_os},
  {HYDROGEN_STRLIBNAME, hydrogenopen_string},
  {HYDROGEN_MATHLIBNAME, hydrogenopen_math},
  {HYDROGEN_UTF8LIBNAME, hydrogenopen_utf8},
  {HYDROGEN_DBLIBNAME, hydrogenopen_debug},
  {NULL, NULL}
};


HYDROGENLIB_API void hydrogenL_openlibs (hydrogen_State *L) {
  const hydrogenL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    hydrogenL_requiref(L, lib->name, lib->func, 1);
    hydrogen_pop(L, 1);  /* remove lib */
  }
}

