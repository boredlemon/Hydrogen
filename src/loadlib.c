/*
** $Id: loadlib.c $
** Dynamic library loader for Venom
** See Copyright Notice in venom.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define VENOM_LIB

#include "prefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "venom.h"

#include "auxlib.h"
#include "venomlib.h"


/*
** VENOM_IGMARK is a mark to ignore all before it when building the
** venomopen_ function name.
*/
#if !defined (VENOM_IGMARK)
#define VENOM_IGMARK		"-"
#endif


/*
** VENOM_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** VENOM_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Venom loader.
*/
#if !defined(VENOM_CSUBSEP)
#define VENOM_CSUBSEP		VENOM_DIRSEP
#endif

#if !defined(VENOM_LSUBSEP)
#define VENOM_LSUBSEP		VENOM_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define VENOM_POF		"venomopen_"

/* separator for open functions in C libraries */
#define VENOM_OFSEP	"_"


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
static void *lsys_load (venom_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static venom_CFunction lsys_sym (venom_State *L, void *lib, const char *sym);




#if defined(VENOM_USE_DLOPEN)	/* { */
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
#define cast_func(p) (__extension__ (venom_CFunction)(p))
#else
#define cast_func(p) ((venom_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (venom_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    venom_pushstring(L, dlerror());
  return lib;
}


static venom_CFunction lsys_sym (venom_State *L, void *lib, const char *sym) {
  venom_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    venom_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(VENOM_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(VENOM_LLE_FLAGS)
#define VENOM_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of VENOM_EXEC_DIR with the executable's path.
*/
static void setprogdir (venom_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    venomL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    venomL_gsub(L, venom_tostring(L, -1), VENOM_EXEC_DIR, buff);
    venom_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (venom_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    venom_pushstring(L, buffer);
  else
    venom_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (venom_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, VENOM_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static venom_CFunction lsys_sym (venom_State *L, void *lib, const char *sym) {
  venom_CFunction f = (venom_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
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


#define DLMSG	"dynamic libraries not enabled; check your Venom installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (venom_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  venom_pushliteral(L, DLMSG);
  return NULL;
}


static venom_CFunction lsys_sym (venom_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  venom_pushliteral(L, DLMSG);
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
** VENOM_PATH_VAR and VENOM_CPATH_VAR are the names of the environment
** variables that Venom check to set its paths.
*/
#if !defined(VENOM_PATH_VAR)
#define VENOM_PATH_VAR    "VENOM_PATH"
#endif

#if !defined(VENOM_CPATH_VAR)
#define VENOM_CPATH_VAR   "VENOM_CPATH"
#endif



/*
** return registry.VENOM_NOENV as a boolean
*/
static int noenv (venom_State *L) {
  int b;
  venom_getfield(L, VENOM_REGISTRYINDEX, "VENOM_NOENV");
  b = venom_toboolean(L, -1);
  venom_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (venom_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = venom_pushfstring(L, "%s%s", envname, VENOM_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    venom_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, VENOM_PATH_SEP VENOM_PATH_SEP)) == NULL)
    venom_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    venomL_Buffer b;
    venomL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      venomL_addlstring(&b, path, dftmark - path);  /* add it */
      venomL_addchar(&b, *VENOM_PATH_SEP);
    }
    venomL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      venomL_addchar(&b, *VENOM_PATH_SEP);
      venomL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    venomL_pushresult(&b);
  }
  setprogdir(L);
  venom_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  venom_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (venom_State *L, const char *path) {
  void *plib;
  venom_getfield(L, VENOM_REGISTRYINDEX, CLIBS);
  venom_getfield(L, -1, path);
  plib = venom_touserdata(L, -1);  /* plib = CLIBS[path] */
  venom_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (venom_State *L, const char *path, void *plib) {
  venom_getfield(L, VENOM_REGISTRYINDEX, CLIBS);
  venom_pushlightuserdata(L, plib);
  venom_pushvalue(L, -1);
  venom_setfield(L, -3, path);  /* CLIBS[path] = plib */
  venom_rawseti(L, -2, venomL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  venom_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (venom_State *L) {
  venom_Integer n = venomL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    venom_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(venom_touserdata(L, -1));
    venom_pop(L, 1);  /* pop handle */
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
static int lookforfunc (venom_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    venom_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    venom_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    venom_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (venom_State *L) {
  const char *path = venomL_checkstring(L, 1);
  const char *init = venomL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    venomL_pushfail(L);
    venom_insert(L, -2);
    venom_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
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
    *name = *VENOM_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *VENOM_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name Venomes until the end */
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
static void pusherrornotfound (venom_State *L, const char *path) {
  venomL_Buffer b;
  venomL_buffinit(L, &b);
  venomL_addstring(&b, "no file '");
  venomL_addgsub(&b, path, VENOM_PATH_SEP, "'\n\tno file '");
  venomL_addstring(&b, "'");
  venomL_pushresult(&b);
}


static const char *searchpath (venom_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  venomL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = venomL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  venomL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  venomL_addgsub(&buff, path, VENOM_PATH_MARK, name);
  venomL_addchar(&buff, '\0');
  pathname = venomL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + venomL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return venom_pushstring(L, filename);  /* save and return name */
  }
  venomL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, venom_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (venom_State *L) {
  const char *f = searchpath(L, venomL_checkstring(L, 1),
                                venomL_checkstring(L, 2),
                                venomL_optstring(L, 3, "."),
                                venomL_optstring(L, 4, VENOM_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    venomL_pushfail(L);
    venom_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (venom_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  venom_getfield(L, venom_upvalueindex(1), pname);
  path = venom_tostring(L, -1);
  if (l_unlikely(path == NULL))
    venomL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (venom_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    venom_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return venomL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          venom_tostring(L, 1), filename, venom_tostring(L, -1));
}


static int searcher_Venom (venom_State *L) {
  const char *filename;
  const char *name = venomL_checkstring(L, 1);
  filename = findfile(L, name, "path", VENOM_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (venomL_loadfile(L, filename) == VENOM_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "venomopen_X" and look for it. (For compatibility, if that
** fails, it also tries "venomopen_Y".) If there is no ignore mark,
** look for a function named "venomopen_modname".
*/
static int loadfunc (venom_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = venomL_gsub(L, modname, ".", VENOM_OFSEP);
  mark = strchr(modname, *VENOM_IGMARK);
  if (mark) {
    int stat;
    openfunc = venom_pushlstring(L, modname, mark - modname);
    openfunc = venom_pushfstring(L, VENOM_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else Venom ahead and try old-style name */
  }
  openfunc = venom_pushfstring(L, VENOM_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (venom_State *L) {
  const char *name = venomL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", VENOM_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (venom_State *L) {
  const char *filename;
  const char *name = venomL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  venom_pushlstring(L, name, p - name);
  filename = findfile(L, venom_tostring(L, -1), "cpath", VENOM_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      venom_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  venom_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (venom_State *L) {
  const char *name = venomL_checkstring(L, 1);
  venom_getfield(L, VENOM_REGISTRYINDEX, VENOM_PRELOAD_TABLE);
  if (venom_getfield(L, -1, name) == VENOM_TNIL) {  /* not found? */
    venom_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    venom_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (venom_State *L, const char *name) {
  int i;
  venomL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(venom_getfield(L, venom_upvalueindex(1), "searchers")
                 != VENOM_TTABLE))
    venomL_error(L, "'package.searchers' must be a table");
  venomL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    venomL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(venom_rawgeti(L, 3, i) == VENOM_TNIL)) {  /* no more searchers? */
      venom_pop(L, 1);  /* remove nil */
      venomL_buffsub(&msg, 2);  /* remove prefix */
      venomL_pushresult(&msg);  /* create error message */
      venomL_error(L, "module '%s' not found:%s", name, venom_tostring(L, -1));
    }
    venom_pushstring(L, name);
    venom_call(L, 1, 2);  /* call it */
    if (venom_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (venom_isstring(L, -2)) {  /* searcher returned error message? */
      venom_pop(L, 1);  /* remove extra return */
      venomL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      venom_pop(L, 2);  /* remove both returns */
      venomL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (venom_State *L) {
  const char *name = venomL_checkstring(L, 1);
  venom_settop(L, 1);  /* LOADED table will be at index 2 */
  venom_getfield(L, VENOM_REGISTRYINDEX, VENOM_LOADED_TABLE);
  venom_getfield(L, 2, name);  /* LOADED[name] */
  if (venom_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  venom_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  venom_rotate(L, -2, 1);  /* function <-> loader data */
  venom_pushvalue(L, 1);  /* name is 1st argument to module loader */
  venom_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  venom_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!venom_isnil(L, -1))  /* non-nil return? */
    venom_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    venom_pop(L, 1);  /* pop nil */
  if (venom_getfield(L, 2, name) == VENOM_TNIL) {   /* module set no value? */
    venom_pushboolean(L, 1);  /* use true as result */
    venom_copy(L, -1, -2);  /* replace loader result */
    venom_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  venom_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const venomL_Reg pk_funcs[] = {
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


static const venomL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (venom_State *L) {
  static const venom_CFunction searchers[] =
    {searcher_preload, searcher_Venom, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  venom_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    venom_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    venom_pushcclosure(L, searchers[i], 1);
    venom_rawseti(L, -2, i+1);
  }
  venom_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (venom_State *L) {
  venomL_getsubtable(L, VENOM_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  venom_createtable(L, 0, 1);  /* create metatable for CLIBS */
  venom_pushcfunction(L, gctm);
  venom_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  venom_setmetatable(L, -2);
}


VENOMMOD_API int venomopen_package (venom_State *L) {
  createclibstable(L);
  venomL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", VENOM_PATH_VAR, VENOM_PATH_DEFAULT);
  setpath(L, "cpath", VENOM_CPATH_VAR, VENOM_CPATH_DEFAULT);
  /* store config information */
  venom_pushliteral(L, VENOM_DIRSEP "\n" VENOM_PATH_SEP "\n" VENOM_PATH_MARK "\n"
                     VENOM_EXEC_DIR "\n" VENOM_IGMARK "\n");
  venom_setfield(L, -2, "config");
  /* set field 'loaded' */
  venomL_getsubtable(L, VENOM_REGISTRYINDEX, VENOM_LOADED_TABLE);
  venom_setfield(L, -2, "loaded");
  /* set field 'preload' */
  venomL_getsubtable(L, VENOM_REGISTRYINDEX, VENOM_PRELOAD_TABLE);
  venom_setfield(L, -2, "preload");
  venom_pushglobaltable(L);
  venom_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  venomL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  venom_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}