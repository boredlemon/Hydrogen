/*
** $Id: loadlib.c $
** Dynamic library loader for Viper
** See Copyright Notice in viper.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define VIPER_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viper.h"

#include "lauxlib.h"
#include "viperlib.h"


/*
** VIPER_IGMARK is a mark to ignore all before it when building the
** viperopen_ function name.
*/
#if !defined (VIPER_IGMARK)
#define VIPER_IGMARK		"-"
#endif


/*
** VIPER_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** VIPER_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Viper loader.
*/
#if !defined(VIPER_CSUBSEP)
#define VIPER_CSUBSEP		VIPER_DIRSEP
#endif

#if !defined(VIPER_LSUBSEP)
#define VIPER_LSUBSEP		VIPER_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define VIPER_POF		"viperopen_"

/* separator for open functions in C libraries */
#define VIPER_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/*
** Special type equivalent to '(void*)' for functions in gcc
** (to suppress warnings when converting function pointers)
*/
typedef void (*voidf)(void);


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (viper_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static viper_CFunction lsys_sym (viper_State *L, void *lib, const char *sym);




#if defined(VIPER_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (viper_CFunction)(p))
#else
#define cast_func(p) ((viper_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (viper_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    viper_pushstring(L, dlerror());
  return lib;
}


static viper_CFunction lsys_sym (viper_State *L, void *lib, const char *sym) {
  viper_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    viper_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(VIPER_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(VIPER_LLE_FLAGS)
#define VIPER_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of VIPER_EXEC_DIR with the executable's path.
*/
static void setprogdir (viper_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    viperL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    viperL_gsub(L, viper_tostring(L, -1), VIPER_EXEC_DIR, buff);
    viper_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (viper_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    viper_pushstring(L, buffer);
  else
    viper_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (viper_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, VIPER_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static viper_CFunction lsys_sym (viper_State *L, void *lib, const char *sym) {
  viper_CFunction f = (viper_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Viper installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (viper_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  viper_pushliteral(L, DLMSG);
  return NULL;
}


static viper_CFunction lsys_sym (viper_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  viper_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** VIPER_PATH_VAR and VIPER_CPATH_VAR are the names of the environment
** variables that Viper check to set its paths.
*/
#if !defined(VIPER_PATH_VAR)
#define VIPER_PATH_VAR    "VIPER_PATH"
#endif

#if !defined(VIPER_CPATH_VAR)
#define VIPER_CPATH_VAR   "VIPER_CPATH"
#endif



/*
** return registry.VIPER_NOENV as a boolean
*/
static int noenv (viper_State *L) {
  int b;
  viper_getfield(L, VIPER_REGISTRYINDEX, "VIPER_NOENV");
  b = viper_toboolean(L, -1);
  viper_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (viper_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = viper_pushfstring(L, "%s%s", envname, VIPER_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    viper_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, VIPER_PATH_SEP VIPER_PATH_SEP)) == NULL)
    viper_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    viperL_Buffer b;
    viperL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      viperL_addlstring(&b, path, dftmark - path);  /* add it */
      viperL_addchar(&b, *VIPER_PATH_SEP);
    }
    viperL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      viperL_addchar(&b, *VIPER_PATH_SEP);
      viperL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    viperL_pushresult(&b);
  }
  setprogdir(L);
  viper_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  viper_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (viper_State *L, const char *path) {
  void *plib;
  viper_getfield(L, VIPER_REGISTRYINDEX, CLIBS);
  viper_getfield(L, -1, path);
  plib = viper_touserdata(L, -1);  /* plib = CLIBS[path] */
  viper_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (viper_State *L, const char *path, void *plib) {
  viper_getfield(L, VIPER_REGISTRYINDEX, CLIBS);
  viper_pushlightuserdata(L, plib);
  viper_pushvalue(L, -1);
  viper_setfield(L, -3, path);  /* CLIBS[path] = plib */
  viper_rawseti(L, -2, viperL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  viper_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (viper_State *L) {
  viper_Integer n = viperL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    viper_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(viper_touserdata(L, -1));
    viper_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (viper_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    viper_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    viper_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    viper_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (viper_State *L) {
  const char *path = viperL_checkstring(L, 1);
  const char *init = viperL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    viperL_pushfail(L);
    viper_insert(L, -2);
    viper_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *VIPER_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *VIPER_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name Viperes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (viper_State *L, const char *path) {
  viperL_Buffer b;
  viperL_buffinit(L, &b);
  viperL_addstring(&b, "no file '");
  viperL_addgsub(&b, path, VIPER_PATH_SEP, "'\n\tno file '");
  viperL_addstring(&b, "'");
  viperL_pushresult(&b);
}


static const char *searchpath (viper_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  viperL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = viperL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  viperL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  viperL_addgsub(&buff, path, VIPER_PATH_MARK, name);
  viperL_addchar(&buff, '\0');
  pathname = viperL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + viperL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return viper_pushstring(L, filename);  /* save and return name */
  }
  viperL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, viper_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (viper_State *L) {
  const char *f = searchpath(L, viperL_checkstring(L, 1),
                                viperL_checkstring(L, 2),
                                viperL_optstring(L, 3, "."),
                                viperL_optstring(L, 4, VIPER_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    viperL_pushfail(L);
    viper_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (viper_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  viper_getfield(L, viper_upvalueindex(1), pname);
  path = viper_tostring(L, -1);
  if (l_unlikely(path == NULL))
    viperL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (viper_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    viper_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return viperL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          viper_tostring(L, 1), filename, viper_tostring(L, -1));
}


static int searcher_Viper (viper_State *L) {
  const char *filename;
  const char *name = viperL_checkstring(L, 1);
  filename = findfile(L, name, "path", VIPER_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (viperL_loadfile(L, filename) == VIPER_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "viperopen_X" and look for it. (For compatibility, if that
** fails, it also tries "viperopen_Y".) If there is no ignore mark,
** look for a function named "viperopen_modname".
*/
static int loadfunc (viper_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = viperL_gsub(L, modname, ".", VIPER_OFSEP);
  mark = strchr(modname, *VIPER_IGMARK);
  if (mark) {
    int stat;
    openfunc = viper_pushlstring(L, modname, mark - modname);
    openfunc = viper_pushfstring(L, VIPER_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else Viper ahead and try old-style name */
  }
  openfunc = viper_pushfstring(L, VIPER_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (viper_State *L) {
  const char *name = viperL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", VIPER_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (viper_State *L) {
  const char *filename;
  const char *name = viperL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  viper_pushlstring(L, name, p - name);
  filename = findfile(L, viper_tostring(L, -1), "cpath", VIPER_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      viper_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  viper_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (viper_State *L) {
  const char *name = viperL_checkstring(L, 1);
  viper_getfield(L, VIPER_REGISTRYINDEX, VIPER_PRELOAD_TABLE);
  if (viper_getfield(L, -1, name) == VIPER_TNIL) {  /* not found? */
    viper_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    viper_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (viper_State *L, const char *name) {
  int i;
  viperL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(viper_getfield(L, viper_upvalueindex(1), "searchers")
                 != VIPER_TTABLE))
    viperL_error(L, "'package.searchers' must be a table");
  viperL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    viperL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(viper_rawgeti(L, 3, i) == VIPER_TNIL)) {  /* no more searchers? */
      viper_pop(L, 1);  /* remove nil */
      viperL_buffsub(&msg, 2);  /* remove prefix */
      viperL_pushresult(&msg);  /* create error message */
      viperL_error(L, "module '%s' not found:%s", name, viper_tostring(L, -1));
    }
    viper_pushstring(L, name);
    viper_call(L, 1, 2);  /* call it */
    if (viper_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (viper_isstring(L, -2)) {  /* searcher returned error message? */
      viper_pop(L, 1);  /* remove extra return */
      viperL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      viper_pop(L, 2);  /* remove both returns */
      viperL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (viper_State *L) {
  const char *name = viperL_checkstring(L, 1);
  viper_settop(L, 1);  /* LOADED table will be at index 2 */
  viper_getfield(L, VIPER_REGISTRYINDEX, VIPER_LOADED_TABLE);
  viper_getfield(L, 2, name);  /* LOADED[name] */
  if (viper_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  viper_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  viper_rotate(L, -2, 1);  /* function <-> loader data */
  viper_pushvalue(L, 1);  /* name is 1st argument to module loader */
  viper_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  viper_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!viper_isnil(L, -1))  /* non-nil return? */
    viper_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    viper_pop(L, 1);  /* pop nil */
  if (viper_getfield(L, 2, name) == VIPER_TNIL) {   /* module set no value? */
    viper_pushboolean(L, 1);  /* use true as result */
    viper_copy(L, -1, -2);  /* replace loader result */
    viper_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  viper_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const viperL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const viperL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (viper_State *L) {
  static const viper_CFunction searchers[] =
    {searcher_preload, searcher_Viper, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  viper_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    viper_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    viper_pushcclosure(L, searchers[i], 1);
    viper_rawseti(L, -2, i+1);
  }
  viper_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (viper_State *L) {
  viperL_getsubtable(L, VIPER_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  viper_createtable(L, 0, 1);  /* create metatable for CLIBS */
  viper_pushcfunction(L, gctm);
  viper_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  viper_setmetatable(L, -2);
}


VIPERMOD_API int viperopen_package (viper_State *L) {
  createclibstable(L);
  viperL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", VIPER_PATH_VAR, VIPER_PATH_DEFAULT);
  setpath(L, "cpath", VIPER_CPATH_VAR, VIPER_CPATH_DEFAULT);
  /* store config information */
  viper_pushliteral(L, VIPER_DIRSEP "\n" VIPER_PATH_SEP "\n" VIPER_PATH_MARK "\n"
                     VIPER_EXEC_DIR "\n" VIPER_IGMARK "\n");
  viper_setfield(L, -2, "config");
  /* set field 'loaded' */
  viperL_getsubtable(L, VIPER_REGISTRYINDEX, VIPER_LOADED_TABLE);
  viper_setfield(L, -2, "loaded");
  /* set field 'preload' */
  viperL_getsubtable(L, VIPER_REGISTRYINDEX, VIPER_PRELOAD_TABLE);
  viper_setfield(L, -2, "preload");
  viper_pushglobaltable(L);
  viper_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  viperL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  viper_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

