/*
** $Id: Initialize.c $
** Initialization of libraries for viper.c and other clients
** See Copyright Notice in viper.h
*/


#define Initialize_c
#define VIPER_LIB

/*
** If you embed Viper in your program and need to open the standard
** libraries, call viperL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  viperL_getsubtable(L, VIPER_REGISTRYINDEX, VIPER_PRELOAD_TABLE);
**  viper_pushcfunction(L, viperopen_modname);
**  viper_setfield(L, -2, modname);
**  viper_pop(L, 1);  // remove PRELOAD table
*/

#include "prefix.h"


#include <stddef.h>

#include "viper.h"

#include "viperlib.h"
#include "auxlib.h"


/*
** these libs are loaded by viper.c and are readily available to any Viper
** program
*/
static const viperL_Reg loadedlibs[] = {
  {VIPER_GNAME, viperopen_base},
  {VIPER_LOADLIBNAME, viperopen_package},
  {VIPER_COLIBNAME, viperopen_coroutine},
  {VIPER_TABLIBNAME, viperopen_table},
  {VIPER_IOLIBNAME, viperopen_io},
  {VIPER_OSLIBNAME, viperopen_os},
  {VIPER_STRLIBNAME, viperopen_string},
  {VIPER_MATHLIBNAME, viperopen_math},
  {VIPER_UTF8LIBNAME, viperopen_utf8},
  {VIPER_DBLIBNAME, viperopen_debug},
  {NULL, NULL}
};


VIPERLIB_API void viperL_openlibs (viper_State *L) {
  const viperL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    viperL_requiref(L, lib->name, lib->func, 1);
    viper_pop(L, 1);  /* remove lib */
  }
}

