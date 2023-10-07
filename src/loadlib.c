/*
** $Id: loadlib.c $
** Dynamic library loader for Cup
** See Copyright Notice in cup.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define CUP_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


/*
** CUP_IGMARK is a mark to ignore all before it when building the
** cupopen_ function name.
*/
#if !defined (CUP_IGMARK)
#define CUP_IGMARK		"-"
#endif


/*
** CUP_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** CUP_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Cup loader.
*/
#if !defined(CUP_CSUBSEP)
#define CUP_CSUBSEP		CUP_DIRSEP
#endif

#if !defined(CUP_LSUBSEP)
#define CUP_LSUBSEP		CUP_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define CUP_POF		"cupopen_"

/* separator for open functions in C libraries */
#define CUP_OFSEP	"_"


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
static void *lsys_load (cup_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static cup_CFunction lsys_sym (cup_State *L, void *lib, const char *sym);




#if defined(CUP_USE_DLOPEN)	/* { */
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
#define cast_func(p) (__extension__ (cup_CFunction)(p))
#else
#define cast_func(p) ((cup_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (cup_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    cup_pushstring(L, dlerror());
  return lib;
}


static cup_CFunction lsys_sym (cup_State *L, void *lib, const char *sym) {
  cup_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    cup_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(CUP_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(CUP_LLE_FLAGS)
#define CUP_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of CUP_EXEC_DIR with the executable's path.
*/
static void setprogdir (cup_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    cupL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    cupL_gsub(L, cup_tostring(L, -1), CUP_EXEC_DIR, buff);
    cup_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (cup_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    cup_pushstring(L, buffer);
  else
    cup_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (cup_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, CUP_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static cup_CFunction lsys_sym (cup_State *L, void *lib, const char *sym) {
  cup_CFunction f = (cup_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
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


#define DLMSG	"dynamic libraries not enabled; check your Cup installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (cup_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  cup_pushliteral(L, DLMSG);
  return NULL;
}


static cup_CFunction lsys_sym (cup_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  cup_pushliteral(L, DLMSG);
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
** CUP_PATH_VAR and CUP_CPATH_VAR are the names of the environment
** variables that Cup check to set its paths.
*/
#if !defined(CUP_PATH_VAR)
#define CUP_PATH_VAR    "CUP_PATH"
#endif

#if !defined(CUP_CPATH_VAR)
#define CUP_CPATH_VAR   "CUP_CPATH"
#endif



/*
** return registry.CUP_NOENV as a boolean
*/
static int noenv (cup_State *L) {
  int b;
  cup_getfield(L, CUP_REGISTRYINDEX, "CUP_NOENV");
  b = cup_toboolean(L, -1);
  cup_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (cup_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = cup_pushfstring(L, "%s%s", envname, CUP_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    cup_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, CUP_PATH_SEP CUP_PATH_SEP)) == NULL)
    cup_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    cupL_Buffer b;
    cupL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      cupL_addlstring(&b, path, dftmark - path);  /* add it */
      cupL_addchar(&b, *CUP_PATH_SEP);
    }
    cupL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      cupL_addchar(&b, *CUP_PATH_SEP);
      cupL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    cupL_pushresult(&b);
  }
  setprogdir(L);
  cup_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  cup_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (cup_State *L, const char *path) {
  void *plib;
  cup_getfield(L, CUP_REGISTRYINDEX, CLIBS);
  cup_getfield(L, -1, path);
  plib = cup_touserdata(L, -1);  /* plib = CLIBS[path] */
  cup_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (cup_State *L, const char *path, void *plib) {
  cup_getfield(L, CUP_REGISTRYINDEX, CLIBS);
  cup_pushlightuserdata(L, plib);
  cup_pushvalue(L, -1);
  cup_setfield(L, -3, path);  /* CLIBS[path] = plib */
  cup_rawseti(L, -2, cupL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  cup_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (cup_State *L) {
  cup_Integer n = cupL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    cup_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(cup_touserdata(L, -1));
    cup_pop(L, 1);  /* pop handle */
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
static int lookforfunc (cup_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    cup_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    cup_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    cup_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (cup_State *L) {
  const char *path = cupL_checkstring(L, 1);
  const char *init = cupL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    cupL_pushfail(L);
    cup_insert(L, -2);
    cup_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
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
    *name = *CUP_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *CUP_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name Cupes until the end */
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
static void pusherrornotfound (cup_State *L, const char *path) {
  cupL_Buffer b;
  cupL_buffinit(L, &b);
  cupL_addstring(&b, "no file '");
  cupL_addgsub(&b, path, CUP_PATH_SEP, "'\n\tno file '");
  cupL_addstring(&b, "'");
  cupL_pushresult(&b);
}


static const char *searchpath (cup_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  cupL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = cupL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  cupL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  cupL_addgsub(&buff, path, CUP_PATH_MARK, name);
  cupL_addchar(&buff, '\0');
  pathname = cupL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + cupL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return cup_pushstring(L, filename);  /* save and return name */
  }
  cupL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, cup_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (cup_State *L) {
  const char *f = searchpath(L, cupL_checkstring(L, 1),
                                cupL_checkstring(L, 2),
                                cupL_optstring(L, 3, "."),
                                cupL_optstring(L, 4, CUP_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    cupL_pushfail(L);
    cup_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (cup_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  cup_getfield(L, cup_upvalueindex(1), pname);
  path = cup_tostring(L, -1);
  if (l_unlikely(path == NULL))
    cupL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (cup_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    cup_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return cupL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          cup_tostring(L, 1), filename, cup_tostring(L, -1));
}


static int searcher_Cup (cup_State *L) {
  const char *filename;
  const char *name = cupL_checkstring(L, 1);
  filename = findfile(L, name, "path", CUP_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (cupL_loadfile(L, filename) == CUP_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "cupopen_X" and look for it. (For compatibility, if that
** fails, it also tries "cupopen_Y".) If there is no ignore mark,
** look for a function named "cupopen_modname".
*/
static int loadfunc (cup_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = cupL_gsub(L, modname, ".", CUP_OFSEP);
  mark = strchr(modname, *CUP_IGMARK);
  if (mark) {
    int stat;
    openfunc = cup_pushlstring(L, modname, mark - modname);
    openfunc = cup_pushfstring(L, CUP_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else Cup ahead and try old-style name */
  }
  openfunc = cup_pushfstring(L, CUP_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (cup_State *L) {
  const char *name = cupL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", CUP_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (cup_State *L) {
  const char *filename;
  const char *name = cupL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  cup_pushlstring(L, name, p - name);
  filename = findfile(L, cup_tostring(L, -1), "cpath", CUP_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      cup_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  cup_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (cup_State *L) {
  const char *name = cupL_checkstring(L, 1);
  cup_getfield(L, CUP_REGISTRYINDEX, CUP_PRELOAD_TABLE);
  if (cup_getfield(L, -1, name) == CUP_TNIL) {  /* not found? */
    cup_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    cup_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (cup_State *L, const char *name) {
  int i;
  cupL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(cup_getfield(L, cup_upvalueindex(1), "searchers")
                 != CUP_TTABLE))
    cupL_error(L, "'package.searchers' must be a table");
  cupL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    cupL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(cup_rawgeti(L, 3, i) == CUP_TNIL)) {  /* no more searchers? */
      cup_pop(L, 1);  /* remove nil */
      cupL_buffsub(&msg, 2);  /* remove prefix */
      cupL_pushresult(&msg);  /* create error message */
      cupL_error(L, "module '%s' not found:%s", name, cup_tostring(L, -1));
    }
    cup_pushstring(L, name);
    cup_call(L, 1, 2);  /* call it */
    if (cup_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (cup_isstring(L, -2)) {  /* searcher returned error message? */
      cup_pop(L, 1);  /* remove extra return */
      cupL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      cup_pop(L, 2);  /* remove both returns */
      cupL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (cup_State *L) {
  const char *name = cupL_checkstring(L, 1);
  cup_settop(L, 1);  /* LOADED table will be at index 2 */
  cup_getfield(L, CUP_REGISTRYINDEX, CUP_LOADED_TABLE);
  cup_getfield(L, 2, name);  /* LOADED[name] */
  if (cup_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  cup_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  cup_rotate(L, -2, 1);  /* function <-> loader data */
  cup_pushvalue(L, 1);  /* name is 1st argument to module loader */
  cup_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  cup_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!cup_isnil(L, -1))  /* non-nil return? */
    cup_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    cup_pop(L, 1);  /* pop nil */
  if (cup_getfield(L, 2, name) == CUP_TNIL) {   /* module set no value? */
    cup_pushboolean(L, 1);  /* use true as result */
    cup_copy(L, -1, -2);  /* replace loader result */
    cup_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  cup_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const cupL_Reg pk_funcs[] = {
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


static const cupL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (cup_State *L) {
  static const cup_CFunction searchers[] =
    {searcher_preload, searcher_Cup, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  cup_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    cup_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    cup_pushcclosure(L, searchers[i], 1);
    cup_rawseti(L, -2, i+1);
  }
  cup_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (cup_State *L) {
  cupL_getsubtable(L, CUP_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  cup_createtable(L, 0, 1);  /* create metatable for CLIBS */
  cup_pushcfunction(L, gctm);
  cup_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  cup_setmetatable(L, -2);
}


CUPMOD_API int cupopen_package (cup_State *L) {
  createclibstable(L);
  cupL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", CUP_PATH_VAR, CUP_PATH_DEFAULT);
  setpath(L, "cpath", CUP_CPATH_VAR, CUP_CPATH_DEFAULT);
  /* store config information */
  cup_pushliteral(L, CUP_DIRSEP "\n" CUP_PATH_SEP "\n" CUP_PATH_MARK "\n"
                     CUP_EXEC_DIR "\n" CUP_IGMARK "\n");
  cup_setfield(L, -2, "config");
  /* set field 'loaded' */
  cupL_getsubtable(L, CUP_REGISTRYINDEX, CUP_LOADED_TABLE);
  cup_setfield(L, -2, "loaded");
  /* set field 'preload' */
  cupL_getsubtable(L, CUP_REGISTRYINDEX, CUP_PRELOAD_TABLE);
  cup_setfield(L, -2, "preload");
  cup_pushglobaltable(L);
  cup_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  cupL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  cup_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

