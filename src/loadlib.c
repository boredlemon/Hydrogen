/*
** $Id: loadlib.c $
** Dynamic library loader for Nebula
** See Copyright Notice in nebula.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define NEBULA_LIB

#include "prefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nebula.h"

#include "auxlib.h"
#include "nebulalib.h"


/*
** NEBULA_IGMARK is a mark to ignore all before it when building the
** nebulaopen_ function name.
*/
#if !defined (NEBULA_IGMARK)
#define NEBULA_IGMARK		"-"
#endif


/*
** NEBULA_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** NEBULA_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Nebula loader.
*/
#if !defined(NEBULA_CSUBSEP)
#define NEBULA_CSUBSEP		NEBULA_DIRSEP
#endif

#if !defined(NEBULA_LSUBSEP)
#define NEBULA_LSUBSEP		NEBULA_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define NEBULA_POF		"nebulaopen_"

/* separator for open functions in C libraries */
#define NEBULA_OFSEP	"_"


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
static void *lsys_load (nebula_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static nebula_CFunction lsys_sym (nebula_State *L, void *lib, const char *sym);




#if defined(NEBULA_USE_DLOPEN)	/* { */
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
#define cast_func(p) (__extension__ (nebula_CFunction)(p))
#else
#define cast_func(p) ((nebula_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (nebula_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    nebula_pushstring(L, dlerror());
  return lib;
}


static nebula_CFunction lsys_sym (nebula_State *L, void *lib, const char *sym) {
  nebula_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    nebula_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(NEBULA_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(NEBULA_LLE_FLAGS)
#define NEBULA_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of NEBULA_EXEC_DIR with the executable's path.
*/
static void setprogdir (nebula_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    nebulaL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    nebulaL_gsub(L, nebula_tostring(L, -1), NEBULA_EXEC_DIR, buff);
    nebula_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (nebula_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    nebula_pushstring(L, buffer);
  else
    nebula_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (nebula_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, NEBULA_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static nebula_CFunction lsys_sym (nebula_State *L, void *lib, const char *sym) {
  nebula_CFunction f = (nebula_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
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


#define DLMSG	"dynamic libraries not enabled; check your Nebula installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (nebula_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  nebula_pushliteral(L, DLMSG);
  return NULL;
}


static nebula_CFunction lsys_sym (nebula_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  nebula_pushliteral(L, DLMSG);
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
** NEBULA_PATH_VAR and NEBULA_CPATH_VAR are the names of the environment
** variables that Nebula check to set its paths.
*/
#if !defined(NEBULA_PATH_VAR)
#define NEBULA_PATH_VAR    "NEBULA_PATH"
#endif

#if !defined(NEBULA_CPATH_VAR)
#define NEBULA_CPATH_VAR   "NEBULA_CPATH"
#endif



/*
** return registry.NEBULA_NOENV as a boolean
*/
static int noenv (nebula_State *L) {
  int b;
  nebula_getfield(L, NEBULA_REGISTRYINDEX, "NEBULA_NOENV");
  b = nebula_toboolean(L, -1);
  nebula_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (nebula_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = nebula_pushfstring(L, "%s%s", envname, NEBULA_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    nebula_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, NEBULA_PATH_SEP NEBULA_PATH_SEP)) == NULL)
    nebula_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    nebulaL_Buffer b;
    nebulaL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      nebulaL_addlstring(&b, path, dftmark - path);  /* add it */
      nebulaL_addchar(&b, *NEBULA_PATH_SEP);
    }
    nebulaL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      nebulaL_addchar(&b, *NEBULA_PATH_SEP);
      nebulaL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    nebulaL_pushresult(&b);
  }
  setprogdir(L);
  nebula_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  nebula_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (nebula_State *L, const char *path) {
  void *plib;
  nebula_getfield(L, NEBULA_REGISTRYINDEX, CLIBS);
  nebula_getfield(L, -1, path);
  plib = nebula_touserdata(L, -1);  /* plib = CLIBS[path] */
  nebula_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (nebula_State *L, const char *path, void *plib) {
  nebula_getfield(L, NEBULA_REGISTRYINDEX, CLIBS);
  nebula_pushlightuserdata(L, plib);
  nebula_pushvalue(L, -1);
  nebula_setfield(L, -3, path);  /* CLIBS[path] = plib */
  nebula_rawseti(L, -2, nebulaL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  nebula_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (nebula_State *L) {
  nebula_Integer n = nebulaL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    nebula_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(nebula_touserdata(L, -1));
    nebula_pop(L, 1);  /* pop handle */
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
static int lookforfunc (nebula_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    nebula_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    nebula_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    nebula_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (nebula_State *L) {
  const char *path = nebulaL_checkstring(L, 1);
  const char *init = nebulaL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    nebulaL_pushfail(L);
    nebula_insert(L, -2);
    nebula_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
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
    *name = *NEBULA_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *NEBULA_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name Nebulaes until the end */
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
static void pusherrornotfound (nebula_State *L, const char *path) {
  nebulaL_Buffer b;
  nebulaL_buffinit(L, &b);
  nebulaL_addstring(&b, "no file '");
  nebulaL_addgsub(&b, path, NEBULA_PATH_SEP, "'\n\tno file '");
  nebulaL_addstring(&b, "'");
  nebulaL_pushresult(&b);
}


static const char *searchpath (nebula_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  nebulaL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = nebulaL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  nebulaL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  nebulaL_addgsub(&buff, path, NEBULA_PATH_MARK, name);
  nebulaL_addchar(&buff, '\0');
  pathname = nebulaL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + nebulaL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return nebula_pushstring(L, filename);  /* save and return name */
  }
  nebulaL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, nebula_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (nebula_State *L) {
  const char *f = searchpath(L, nebulaL_checkstring(L, 1),
                                nebulaL_checkstring(L, 2),
                                nebulaL_optstring(L, 3, "."),
                                nebulaL_optstring(L, 4, NEBULA_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    nebulaL_pushfail(L);
    nebula_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (nebula_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  nebula_getfield(L, nebula_upvalueindex(1), pname);
  path = nebula_tostring(L, -1);
  if (l_unlikely(path == NULL))
    nebulaL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (nebula_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    nebula_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return nebulaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          nebula_tostring(L, 1), filename, nebula_tostring(L, -1));
}


static int searcher_Nebula (nebula_State *L) {
  const char *filename;
  const char *name = nebulaL_checkstring(L, 1);
  filename = findfile(L, name, "path", NEBULA_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (nebulaL_loadfile(L, filename) == NEBULA_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "nebulaopen_X" and look for it. (For compatibility, if that
** fails, it also tries "nebulaopen_Y".) If there is no ignore mark,
** look for a function named "nebulaopen_modname".
*/
static int loadfunc (nebula_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = nebulaL_gsub(L, modname, ".", NEBULA_OFSEP);
  mark = strchr(modname, *NEBULA_IGMARK);
  if (mark) {
    int stat;
    openfunc = nebula_pushlstring(L, modname, mark - modname);
    openfunc = nebula_pushfstring(L, NEBULA_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else Nebula ahead and try old-style name */
  }
  openfunc = nebula_pushfstring(L, NEBULA_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (nebula_State *L) {
  const char *name = nebulaL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", NEBULA_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (nebula_State *L) {
  const char *filename;
  const char *name = nebulaL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  nebula_pushlstring(L, name, p - name);
  filename = findfile(L, nebula_tostring(L, -1), "cpath", NEBULA_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      nebula_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  nebula_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (nebula_State *L) {
  const char *name = nebulaL_checkstring(L, 1);
  nebula_getfield(L, NEBULA_REGISTRYINDEX, NEBULA_PRELOAD_TABLE);
  if (nebula_getfield(L, -1, name) == NEBULA_TNIL) {  /* not found? */
    nebula_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    nebula_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (nebula_State *L, const char *name) {
  int i;
  nebulaL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(nebula_getfield(L, nebula_upvalueindex(1), "searchers")
                 != NEBULA_TTABLE))
    nebulaL_error(L, "'package.searchers' must be a table");
  nebulaL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    nebulaL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(nebula_rawgeti(L, 3, i) == NEBULA_TNIL)) {  /* no more searchers? */
      nebula_pop(L, 1);  /* remove nil */
      nebulaL_buffsub(&msg, 2);  /* remove prefix */
      nebulaL_pushresult(&msg);  /* create error message */
      nebulaL_error(L, "module '%s' not found:%s", name, nebula_tostring(L, -1));
    }
    nebula_pushstring(L, name);
    nebula_call(L, 1, 2);  /* call it */
    if (nebula_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (nebula_isstring(L, -2)) {  /* searcher returned error message? */
      nebula_pop(L, 1);  /* remove extra return */
      nebulaL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      nebula_pop(L, 2);  /* remove both returns */
      nebulaL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (nebula_State *L) {
  const char *name = nebulaL_checkstring(L, 1);
  nebula_settop(L, 1);  /* LOADED table will be at index 2 */
  nebula_getfield(L, NEBULA_REGISTRYINDEX, NEBULA_LOADED_TABLE);
  nebula_getfield(L, 2, name);  /* LOADED[name] */
  if (nebula_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  nebula_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  nebula_rotate(L, -2, 1);  /* function <-> loader data */
  nebula_pushvalue(L, 1);  /* name is 1st argument to module loader */
  nebula_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  nebula_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!nebula_isnil(L, -1))  /* non-nil return? */
    nebula_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    nebula_pop(L, 1);  /* pop nil */
  if (nebula_getfield(L, 2, name) == NEBULA_TNIL) {   /* module set no value? */
    nebula_pushboolean(L, 1);  /* use true as result */
    nebula_copy(L, -1, -2);  /* replace loader result */
    nebula_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  nebula_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const nebulaL_Reg pk_funcs[] = {
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


static const nebulaL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (nebula_State *L) {
  static const nebula_CFunction searchers[] =
    {searcher_preload, searcher_Nebula, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  nebula_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    nebula_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    nebula_pushcclosure(L, searchers[i], 1);
    nebula_rawseti(L, -2, i+1);
  }
  nebula_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (nebula_State *L) {
  nebulaL_getsubtable(L, NEBULA_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  nebula_createtable(L, 0, 1);  /* create metatable for CLIBS */
  nebula_pushcfunction(L, gctm);
  nebula_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  nebula_setmetatable(L, -2);
}


NEBULAMOD_API int nebulaopen_package (nebula_State *L) {
  createclibstable(L);
  nebulaL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", NEBULA_PATH_VAR, NEBULA_PATH_DEFAULT);
  setpath(L, "cpath", NEBULA_CPATH_VAR, NEBULA_CPATH_DEFAULT);
  /* store config information */
  nebula_pushliteral(L, NEBULA_DIRSEP "\n" NEBULA_PATH_SEP "\n" NEBULA_PATH_MARK "\n"
                     NEBULA_EXEC_DIR "\n" NEBULA_IGMARK "\n");
  nebula_setfield(L, -2, "config");
  /* set field 'loaded' */
  nebulaL_getsubtable(L, NEBULA_REGISTRYINDEX, NEBULA_LOADED_TABLE);
  nebula_setfield(L, -2, "loaded");
  /* set field 'preload' */
  nebulaL_getsubtable(L, NEBULA_REGISTRYINDEX, NEBULA_PRELOAD_TABLE);
  nebula_setfield(L, -2, "preload");
  nebula_pushglobaltable(L);
  nebula_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  nebulaL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  nebula_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}