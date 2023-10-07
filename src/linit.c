/*
** $Id: linit.c $
** Initialization of libraries for cup.c and other clients
** See Copyright Notice in cup.h
*/


#define linit_c
#define CUP_LIB

/*
** If you embed Cup in your program and need to open the standard
** libraries, call cupL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  cupL_getsubtable(L, CUP_REGISTRYINDEX, CUP_PRELOAD_TABLE);
**  cup_pushcfunction(L, cupopen_modname);
**  cup_setfield(L, -2, modname);
**  cup_pop(L, 1);  // remove PRELOAD table
*/

#include "lprefix.h"


#include <stddef.h>

#include "cup.h"

#include "cuplib.h"
#include "lauxlib.h"


/*
** these libs are loaded by cup.c and are readily available to any Cup
** program
*/
static const cupL_Reg loadedlibs[] = {
  {CUP_GNAME, cupopen_base},
  {CUP_LOADLIBNAME, cupopen_package},
  {CUP_COLIBNAME, cupopen_coroutine},
  {CUP_TABLIBNAME, cupopen_table},
  {CUP_IOLIBNAME, cupopen_io},
  {CUP_OSLIBNAME, cupopen_os},
  {CUP_STRLIBNAME, cupopen_string},
  {CUP_MATHLIBNAME, cupopen_math},
  {CUP_UTF8LIBNAME, cupopen_utf8},
  {CUP_DBLIBNAME, cupopen_debug},
  {NULL, NULL}
};


CUPLIB_API void cupL_openlibs (cup_State *L) {
  const cupL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    cupL_requiref(L, lib->name, lib->func, 1);
    cup_pop(L, 1);  /* remove lib */
  }
}

