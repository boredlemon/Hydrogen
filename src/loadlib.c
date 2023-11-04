/*
** $Id: loadlib.c $
** Dynamic library loader for Hydrogen
** See Copyright Notice in hydrogen.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define HYDROGEN_LIB

#include "prefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydrogen.h"

#include "auxlib.h"
#include "hydrogenlib.h"


/*
** HYDROGEN_IGMARK is a mark to ignore all before it when building the
** hydrogenopen_ function name.
*/
#if !defined (HYDROGEN_IGMARK)
#define HYDROGEN_IGMARK		"-"
#endif


/*
** HYDROGEN_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** HYDROGEN_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Hydrogen loader.
*/
#if !defined(HYDROGEN_CSUBSEP)
#define HYDROGEN_CSUBSEP		HYDROGEN_DIRSEP
#endif

#if !defined(HYDROGEN_LSUBSEP)
#define HYDROGEN_LSUBSEP		HYDROGEN_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define HYDROGEN_POF		"hydrogenopen_"

/* separator for open functions in C libraries */
#define HYDROGEN_OFSEP	"_"


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
static void *lsys_load (hydrogen_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static hydrogen_CFunction lsys_sym (hydrogen_State *L, void *lib, const char *sym);




#if defined(HYDROGEN_USE_DLOPEN)	/* { */
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
#define cast_func(p) (__extension__ (hydrogen_CFunction)(p))
#else
#define cast_func(p) ((hydrogen_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (hydrogen_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    hydrogen_pushstring(L, dlerror());
  return lib;
}


static hydrogen_CFunction lsys_sym (hydrogen_State *L, void *lib, const char *sym) {
  hydrogen_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    hydrogen_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(HYDROGEN_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(HYDROGEN_LLE_FLAGS)
#define HYDROGEN_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of HYDROGEN_EXEC_DIR with the executable's path.
*/
static void setprogdir (hydrogen_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    hydrogenL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    hydrogenL_gsub(L, hydrogen_tostring(L, -1), HYDROGEN_EXEC_DIR, buff);
    hydrogen_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (hydrogen_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    hydrogen_pushstring(L, buffer);
  else
    hydrogen_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (hydrogen_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, HYDROGEN_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static hydrogen_CFunction lsys_sym (hydrogen_State *L, void *lib, const char *sym) {
  hydrogen_CFunction f = (hydrogen_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
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


#define DLMSG	"dynamic libraries not enabled; check your Hydrogen installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (hydrogen_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  hydrogen_pushliteral(L, DLMSG);
  return NULL;
}


static hydrogen_CFunction lsys_sym (hydrogen_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  hydrogen_pushliteral(L, DLMSG);
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
** HYDROGEN_PATH_VAR and HYDROGEN_CPATH_VAR are the names of the environment
** variables that Hydrogen check to set its paths.
*/
#if !defined(HYDROGEN_PATH_VAR)
#define HYDROGEN_PATH_VAR    "HYDROGEN_PATH"
#endif

#if !defined(HYDROGEN_CPATH_VAR)
#define HYDROGEN_CPATH_VAR   "HYDROGEN_CPATH"
#endif



/*
** return registry.HYDROGEN_NOENV as a boolean
*/
static int noenv (hydrogen_State *L) {
  int b;
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, "HYDROGEN_NOENV");
  b = hydrogen_toboolean(L, -1);
  hydrogen_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (hydrogen_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = hydrogen_pushfstring(L, "%s%s", envname, HYDROGEN_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    hydrogen_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, HYDROGEN_PATH_SEP HYDROGEN_PATH_SEP)) == NULL)
    hydrogen_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    hydrogenL_Buffer b;
    hydrogenL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      hydrogenL_addlstring(&b, path, dftmark - path);  /* add it */
      hydrogenL_addchar(&b, *HYDROGEN_PATH_SEP);
    }
    hydrogenL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      hydrogenL_addchar(&b, *HYDROGEN_PATH_SEP);
      hydrogenL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    hydrogenL_pushresult(&b);
  }
  setprogdir(L);
  hydrogen_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  hydrogen_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (hydrogen_State *L, const char *path) {
  void *plib;
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, CLIBS);
  hydrogen_getfield(L, -1, path);
  plib = hydrogen_touserdata(L, -1);  /* plib = CLIBS[path] */
  hydrogen_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (hydrogen_State *L, const char *path, void *plib) {
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, CLIBS);
  hydrogen_pushlightuserdata(L, plib);
  hydrogen_pushvalue(L, -1);
  hydrogen_setfield(L, -3, path);  /* CLIBS[path] = plib */
  hydrogen_rawseti(L, -2, hydrogenL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  hydrogen_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (hydrogen_State *L) {
  hydrogen_Integer n = hydrogenL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    hydrogen_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(hydrogen_touserdata(L, -1));
    hydrogen_pop(L, 1);  /* pop handle */
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
static int lookforfunc (hydrogen_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    hydrogen_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    hydrogen_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    hydrogen_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (hydrogen_State *L) {
  const char *path = hydrogenL_checkstring(L, 1);
  const char *init = hydrogenL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    hydrogenL_pushfail(L);
    hydrogen_insert(L, -2);
    hydrogen_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
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
    *name = *HYDROGEN_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *HYDROGEN_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name Hydrogenes until the end */
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
static void pusherrornotfound (hydrogen_State *L, const char *path) {
  hydrogenL_Buffer b;
  hydrogenL_buffinit(L, &b);
  hydrogenL_addstring(&b, "no file '");
  hydrogenL_addgsub(&b, path, HYDROGEN_PATH_SEP, "'\n\tno file '");
  hydrogenL_addstring(&b, "'");
  hydrogenL_pushresult(&b);
}


static const char *searchpath (hydrogen_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  hydrogenL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = hydrogenL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  hydrogenL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  hydrogenL_addgsub(&buff, path, HYDROGEN_PATH_MARK, name);
  hydrogenL_addchar(&buff, '\0');
  pathname = hydrogenL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + hydrogenL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return hydrogen_pushstring(L, filename);  /* save and return name */
  }
  hydrogenL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, hydrogen_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (hydrogen_State *L) {
  const char *f = searchpath(L, hydrogenL_checkstring(L, 1),
                                hydrogenL_checkstring(L, 2),
                                hydrogenL_optstring(L, 3, "."),
                                hydrogenL_optstring(L, 4, HYDROGEN_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    hydrogenL_pushfail(L);
    hydrogen_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (hydrogen_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  hydrogen_getfield(L, hydrogen_upvalueindex(1), pname);
  path = hydrogen_tostring(L, -1);
  if (l_unlikely(path == NULL))
    hydrogenL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (hydrogen_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    hydrogen_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return hydrogenL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          hydrogen_tostring(L, 1), filename, hydrogen_tostring(L, -1));
}


static int searcher_Hydrogen (hydrogen_State *L) {
  const char *filename;
  const char *name = hydrogenL_checkstring(L, 1);
  filename = findfile(L, name, "path", HYDROGEN_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (hydrogenL_loadfile(L, filename) == HYDROGEN_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "hydrogenopen_X" and look for it. (For compatibility, if that
** fails, it also tries "hydrogenopen_Y".) If there is no ignore mark,
** look for a function named "hydrogenopen_modname".
*/
static int loadfunc (hydrogen_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = hydrogenL_gsub(L, modname, ".", HYDROGEN_OFSEP);
  mark = strchr(modname, *HYDROGEN_IGMARK);
  if (mark) {
    int stat;
    openfunc = hydrogen_pushlstring(L, modname, mark - modname);
    openfunc = hydrogen_pushfstring(L, HYDROGEN_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else Hydrogen ahead and try old-style name */
  }
  openfunc = hydrogen_pushfstring(L, HYDROGEN_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (hydrogen_State *L) {
  const char *name = hydrogenL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", HYDROGEN_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (hydrogen_State *L) {
  const char *filename;
  const char *name = hydrogenL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  hydrogen_pushlstring(L, name, p - name);
  filename = findfile(L, hydrogen_tostring(L, -1), "cpath", HYDROGEN_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      hydrogen_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  hydrogen_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (hydrogen_State *L) {
  const char *name = hydrogenL_checkstring(L, 1);
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_PRELOAD_TABLE);
  if (hydrogen_getfield(L, -1, name) == HYDROGEN_TNIL) {  /* not found? */
    hydrogen_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    hydrogen_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (hydrogen_State *L, const char *name) {
  int i;
  hydrogenL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(hydrogen_getfield(L, hydrogen_upvalueindex(1), "searchers")
                 != HYDROGEN_TTABLE))
    hydrogenL_error(L, "'package.searchers' must be a table");
  hydrogenL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    hydrogenL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(hydrogen_rawgeti(L, 3, i) == HYDROGEN_TNIL)) {  /* no more searchers? */
      hydrogen_pop(L, 1);  /* remove nil */
      hydrogenL_buffsub(&msg, 2);  /* remove prefix */
      hydrogenL_pushresult(&msg);  /* create error message */
      hydrogenL_error(L, "module '%s' not found:%s", name, hydrogen_tostring(L, -1));
    }
    hydrogen_pushstring(L, name);
    hydrogen_call(L, 1, 2);  /* call it */
    if (hydrogen_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (hydrogen_isstring(L, -2)) {  /* searcher returned error message? */
      hydrogen_pop(L, 1);  /* remove extra return */
      hydrogenL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      hydrogen_pop(L, 2);  /* remove both returns */
      hydrogenL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (hydrogen_State *L) {
  const char *name = hydrogenL_checkstring(L, 1);
  hydrogen_settop(L, 1);  /* LOADED table will be at index 2 */
  hydrogen_getfield(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_LOADED_TABLE);
  hydrogen_getfield(L, 2, name);  /* LOADED[name] */
  if (hydrogen_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  hydrogen_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  hydrogen_rotate(L, -2, 1);  /* function <-> loader data */
  hydrogen_pushvalue(L, 1);  /* name is 1st argument to module loader */
  hydrogen_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  hydrogen_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!hydrogen_isnil(L, -1))  /* non-nil return? */
    hydrogen_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    hydrogen_pop(L, 1);  /* pop nil */
  if (hydrogen_getfield(L, 2, name) == HYDROGEN_TNIL) {   /* module set no value? */
    hydrogen_pushboolean(L, 1);  /* use true as result */
    hydrogen_copy(L, -1, -2);  /* replace loader result */
    hydrogen_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  hydrogen_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const hydrogenL_Reg pk_funcs[] = {
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


static const hydrogenL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (hydrogen_State *L) {
  static const hydrogen_CFunction searchers[] =
    {searcher_preload, searcher_Hydrogen, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  hydrogen_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    hydrogen_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    hydrogen_pushcclosure(L, searchers[i], 1);
    hydrogen_rawseti(L, -2, i+1);
  }
  hydrogen_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (hydrogen_State *L) {
  hydrogenL_getsubtable(L, HYDROGEN_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  hydrogen_createtable(L, 0, 1);  /* create metatable for CLIBS */
  hydrogen_pushcfunction(L, gctm);
  hydrogen_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  hydrogen_setmetatable(L, -2);
}


HYDROGENMOD_API int hydrogenopen_package (hydrogen_State *L) {
  createclibstable(L);
  hydrogenL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", HYDROGEN_PATH_VAR, HYDROGEN_PATH_DEFAULT);
  setpath(L, "cpath", HYDROGEN_CPATH_VAR, HYDROGEN_CPATH_DEFAULT);
  /* store config information */
  hydrogen_pushliteral(L, HYDROGEN_DIRSEP "\n" HYDROGEN_PATH_SEP "\n" HYDROGEN_PATH_MARK "\n"
                     HYDROGEN_EXEC_DIR "\n" HYDROGEN_IGMARK "\n");
  hydrogen_setfield(L, -2, "config");
  /* set field 'loaded' */
  hydrogenL_getsubtable(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_LOADED_TABLE);
  hydrogen_setfield(L, -2, "loaded");
  /* set field 'preload' */
  hydrogenL_getsubtable(L, HYDROGEN_REGISTRYINDEX, HYDROGEN_PRELOAD_TABLE);
  hydrogen_setfield(L, -2, "preload");
  hydrogen_pushglobaltable(L);
  hydrogen_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  hydrogenL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  hydrogen_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}