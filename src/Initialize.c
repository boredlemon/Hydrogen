/*
** $Id: Initialize.c $
** Initialization of libraries for nebula.c and other clients
** See Copyright Notice in nebula.h
*/


#define Initialize_c
#define NEBULA_LIB

/*
** If you embed Nebula in your program and need to open the standard
** libraries, call nebulaL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  nebulaL_getsubtable(L, NEBULA_REGISTRYINDEX, NEBULA_PRELOAD_TABLE);
**  nebula_pushcfunction(L, nebulaopen_modname);
**  nebula_setfield(L, -2, modname);
**  nebula_pop(L, 1);  // remove PRELOAD table
*/

#include "prefix.h"


#include <stddef.h>

#include "nebula.h"

#include "nebulalib.h"
#include "auxlib.h"


/*
** these libs are loaded by nebula.c and are readily available to any Nebula
** program
*/
static const nebulaL_Reg loadedlibs[] = {
  {NEBULA_GNAME, nebulaopen_base},
  {NEBULA_LOADLIBNAME, nebulaopen_package},
  {NEBULA_COLIBNAME, nebulaopen_coroutine},
  {NEBULA_TABLIBNAME, nebulaopen_table},
  {NEBULA_IOLIBNAME, nebulaopen_io},
  {NEBULA_OSLIBNAME, nebulaopen_os},
  {NEBULA_STRLIBNAME, nebulaopen_string},
  {NEBULA_MATHLIBNAME, nebulaopen_math},
  {NEBULA_UTF8LIBNAME, nebulaopen_utf8},
  {NEBULA_DBLIBNAME, nebulaopen_debug},
  {NULL, NULL}
};


NEBULALIB_API void nebulaL_openlibs (nebula_State *L) {
  const nebulaL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    nebulaL_requiref(L, lib->name, lib->func, 1);
    nebula_pop(L, 1);  /* remove lib */
  }
}

