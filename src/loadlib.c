/*
** $Id: loadlib.c $
** Dynamic library loader for Acorn
** See Copyright Notice in acorn.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define ACORN_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acorn.h"

#include "lauxlib.h"
#include "acornlib.h"


/*
** ACORN_IGMARK is a mark to ignore all before it when building the
** acornopen_ function name.
*/
#if !defined (ACORN_IGMARK)
#define ACORN_IGMARK		"-"
#endif


/*
** ACORN_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** ACORN_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Acorn loader.
*/
#if !defined(ACORN_CSUBSEP)
#define ACORN_CSUBSEP		ACORN_DIRSEP
#endif

#if !defined(ACORN_LSUBSEP)
#define ACORN_LSUBSEP		ACORN_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define ACORN_POF		"acornopen_"

/* separator for open functions in C libraries */
#define ACORN_OFSEP	"_"


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
static void *lsys_load (acorn_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static acorn_CFunction lsys_sym (acorn_State *L, void *lib, const char *sym);




#if defined(ACORN_USE_DLOPEN)	/* { */
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
#define cast_func(p) (__extension__ (acorn_CFunction)(p))
#else
#define cast_func(p) ((acorn_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (acorn_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    acorn_pushstring(L, dlerror());
  return lib;
}


static acorn_CFunction lsys_sym (acorn_State *L, void *lib, const char *sym) {
  acorn_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    acorn_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(ACORN_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(ACORN_LLE_FLAGS)
#define ACORN_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of ACORN_EXEC_DIR with the executable's path.
*/
static void setprogdir (acorn_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    acornL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    acornL_gsub(L, acorn_tostring(L, -1), ACORN_EXEC_DIR, buff);
    acorn_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (acorn_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    acorn_pushstring(L, buffer);
  else
    acorn_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (acorn_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, ACORN_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static acorn_CFunction lsys_sym (acorn_State *L, void *lib, const char *sym) {
  acorn_CFunction f = (acorn_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
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


#define DLMSG	"dynamic libraries not enabled; check your Acorn installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (acorn_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  acorn_pushliteral(L, DLMSG);
  return NULL;
}


static acorn_CFunction lsys_sym (acorn_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  acorn_pushliteral(L, DLMSG);
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
** ACORN_PATH_VAR and ACORN_CPATH_VAR are the names of the environment
** variables that Acorn check to set its paths.
*/
#if !defined(ACORN_PATH_VAR)
#define ACORN_PATH_VAR    "ACORN_PATH"
#endif

#if !defined(ACORN_CPATH_VAR)
#define ACORN_CPATH_VAR   "ACORN_CPATH"
#endif



/*
** return registry.ACORN_NOENV as a boolean
*/
static int noenv (acorn_State *L) {
  int b;
  acorn_getfield(L, ACORN_REGISTRYINDEX, "ACORN_NOENV");
  b = acorn_toboolean(L, -1);
  acorn_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (acorn_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = acorn_pushfstring(L, "%s%s", envname, ACORN_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    acorn_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, ACORN_PATH_SEP ACORN_PATH_SEP)) == NULL)
    acorn_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    acornL_Buffer b;
    acornL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      acornL_addlstring(&b, path, dftmark - path);  /* add it */
      acornL_addchar(&b, *ACORN_PATH_SEP);
    }
    acornL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      acornL_addchar(&b, *ACORN_PATH_SEP);
      acornL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    acornL_pushresult(&b);
  }
  setprogdir(L);
  acorn_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  acorn_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (acorn_State *L, const char *path) {
  void *plib;
  acorn_getfield(L, ACORN_REGISTRYINDEX, CLIBS);
  acorn_getfield(L, -1, path);
  plib = acorn_touserdata(L, -1);  /* plib = CLIBS[path] */
  acorn_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (acorn_State *L, const char *path, void *plib) {
  acorn_getfield(L, ACORN_REGISTRYINDEX, CLIBS);
  acorn_pushlightuserdata(L, plib);
  acorn_pushvalue(L, -1);
  acorn_setfield(L, -3, path);  /* CLIBS[path] = plib */
  acorn_rawseti(L, -2, acornL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  acorn_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (acorn_State *L) {
  acorn_Integer n = acornL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    acorn_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(acorn_touserdata(L, -1));
    acorn_pop(L, 1);  /* pop handle */
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
static int lookforfunc (acorn_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    acorn_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    acorn_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    acorn_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (acorn_State *L) {
  const char *path = acornL_checkstring(L, 1);
  const char *init = acornL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    acornL_pushfail(L);
    acorn_insert(L, -2);
    acorn_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
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
    *name = *ACORN_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *ACORN_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name Acornes until the end */
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
static void pusherrornotfound (acorn_State *L, const char *path) {
  acornL_Buffer b;
  acornL_buffinit(L, &b);
  acornL_addstring(&b, "no file '");
  acornL_addgsub(&b, path, ACORN_PATH_SEP, "'\n\tno file '");
  acornL_addstring(&b, "'");
  acornL_pushresult(&b);
}


static const char *searchpath (acorn_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  acornL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = acornL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  acornL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  acornL_addgsub(&buff, path, ACORN_PATH_MARK, name);
  acornL_addchar(&buff, '\0');
  pathname = acornL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + acornL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return acorn_pushstring(L, filename);  /* save and return name */
  }
  acornL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, acorn_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (acorn_State *L) {
  const char *f = searchpath(L, acornL_checkstring(L, 1),
                                acornL_checkstring(L, 2),
                                acornL_optstring(L, 3, "."),
                                acornL_optstring(L, 4, ACORN_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    acornL_pushfail(L);
    acorn_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (acorn_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  acorn_getfield(L, acorn_upvalueindex(1), pname);
  path = acorn_tostring(L, -1);
  if (l_unlikely(path == NULL))
    acornL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (acorn_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    acorn_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return acornL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          acorn_tostring(L, 1), filename, acorn_tostring(L, -1));
}


static int searcher_Acorn (acorn_State *L) {
  const char *filename;
  const char *name = acornL_checkstring(L, 1);
  filename = findfile(L, name, "path", ACORN_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (acornL_loadfile(L, filename) == ACORN_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "acornopen_X" and look for it. (For compatibility, if that
** fails, it also tries "acornopen_Y".) If there is no ignore mark,
** look for a function named "acornopen_modname".
*/
static int loadfunc (acorn_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = acornL_gsub(L, modname, ".", ACORN_OFSEP);
  mark = strchr(modname, *ACORN_IGMARK);
  if (mark) {
    int stat;
    openfunc = acorn_pushlstring(L, modname, mark - modname);
    openfunc = acorn_pushfstring(L, ACORN_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else Acorn ahead and try old-style name */
  }
  openfunc = acorn_pushfstring(L, ACORN_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (acorn_State *L) {
  const char *name = acornL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", ACORN_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (acorn_State *L) {
  const char *filename;
  const char *name = acornL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  acorn_pushlstring(L, name, p - name);
  filename = findfile(L, acorn_tostring(L, -1), "cpath", ACORN_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      acorn_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  acorn_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (acorn_State *L) {
  const char *name = acornL_checkstring(L, 1);
  acorn_getfield(L, ACORN_REGISTRYINDEX, ACORN_PRELOAD_TABLE);
  if (acorn_getfield(L, -1, name) == ACORN_TNIL) {  /* not found? */
    acorn_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    acorn_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (acorn_State *L, const char *name) {
  int i;
  acornL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(acorn_getfield(L, acorn_upvalueindex(1), "searchers")
                 != ACORN_TTABLE))
    acornL_error(L, "'package.searchers' must be a table");
  acornL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    acornL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(acorn_rawgeti(L, 3, i) == ACORN_TNIL)) {  /* no more searchers? */
      acorn_pop(L, 1);  /* remove nil */
      acornL_buffsub(&msg, 2);  /* remove prefix */
      acornL_pushresult(&msg);  /* create error message */
      acornL_error(L, "module '%s' not found:%s", name, acorn_tostring(L, -1));
    }
    acorn_pushstring(L, name);
    acorn_call(L, 1, 2);  /* call it */
    if (acorn_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (acorn_isstring(L, -2)) {  /* searcher returned error message? */
      acorn_pop(L, 1);  /* remove extra return */
      acornL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      acorn_pop(L, 2);  /* remove both returns */
      acornL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (acorn_State *L) {
  const char *name = acornL_checkstring(L, 1);
  acorn_settop(L, 1);  /* LOADED table will be at index 2 */
  acorn_getfield(L, ACORN_REGISTRYINDEX, ACORN_LOADED_TABLE);
  acorn_getfield(L, 2, name);  /* LOADED[name] */
  if (acorn_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  acorn_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  acorn_rotate(L, -2, 1);  /* function <-> loader data */
  acorn_pushvalue(L, 1);  /* name is 1st argument to module loader */
  acorn_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  acorn_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!acorn_isnil(L, -1))  /* non-nil return? */
    acorn_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    acorn_pop(L, 1);  /* pop nil */
  if (acorn_getfield(L, 2, name) == ACORN_TNIL) {   /* module set no value? */
    acorn_pushboolean(L, 1);  /* use true as result */
    acorn_copy(L, -1, -2);  /* replace loader result */
    acorn_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  acorn_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const acornL_Reg pk_funcs[] = {
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


static const acornL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (acorn_State *L) {
  static const acorn_CFunction searchers[] =
    {searcher_preload, searcher_Acorn, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  acorn_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    acorn_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    acorn_pushcclosure(L, searchers[i], 1);
    acorn_rawseti(L, -2, i+1);
  }
  acorn_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (acorn_State *L) {
  acornL_getsubtable(L, ACORN_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  acorn_createtable(L, 0, 1);  /* create metatable for CLIBS */
  acorn_pushcfunction(L, gctm);
  acorn_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  acorn_setmetatable(L, -2);
}


ACORNMOD_API int acornopen_package (acorn_State *L) {
  createclibstable(L);
  acornL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", ACORN_PATH_VAR, ACORN_PATH_DEFAULT);
  setpath(L, "cpath", ACORN_CPATH_VAR, ACORN_CPATH_DEFAULT);
  /* store config information */
  acorn_pushliteral(L, ACORN_DIRSEP "\n" ACORN_PATH_SEP "\n" ACORN_PATH_MARK "\n"
                     ACORN_EXEC_DIR "\n" ACORN_IGMARK "\n");
  acorn_setfield(L, -2, "config");
  /* set field 'loaded' */
  acornL_getsubtable(L, ACORN_REGISTRYINDEX, ACORN_LOADED_TABLE);
  acorn_setfield(L, -2, "loaded");
  /* set field 'preload' */
  acornL_getsubtable(L, ACORN_REGISTRYINDEX, ACORN_PRELOAD_TABLE);
  acorn_setfield(L, -2, "preload");
  acorn_pushglobaltable(L);
  acorn_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  acornL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  acorn_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

