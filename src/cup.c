/*
** $Id: cup.c $
** Cup stand-alone interpreter
** See Copyright Notice in cup.h
*/

#define cup_c

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include "cup.h"

#include "lauxlib.h"
#include "cuplib.h"


#if !defined(CUP_PROGNAME)
#define CUP_PROGNAME		"cup"
#endif

#if !defined(CUP_INIT_VAR)
#define CUP_INIT_VAR		"CUP_INIT"
#endif

#define CUP_INITVARVERSION	CUP_INIT_VAR CUP_VERSUFFIX


static cup_State *globalL = NULL;

static const char *progname = CUP_PROGNAME;


#if defined(CUP_USE_POSIX)   /* { */

/*
** Use 'sigaction' when available.
*/
static void setsignal (int sig, void (*handler)(int)) {
  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);  /* do not mask any signal */
  sigaction(sig, &sa, NULL);
}

#else           /* }{ */

#define setsignal            signal

#endif                               /* } */


/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop (cup_State *L, cup_Debug *ar) {
  (void)ar;  /* unused arg. */
  cup_sethook(L, NULL, 0, 0);  /* reset hook */
  cupL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Cup state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction (int i) {
  int flag = CUP_MASKCALL | CUP_MASKRET | CUP_MASKLINE | CUP_MASKCOUNT;
  setsignal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  cup_sethook(globalL, lstop, flag, 1);
}


static void print_usage (const char *badoption) {
  cup_writestringerror("%s: ", progname);
  if (badoption[1] == 'e' || badoption[1] == 'l')
    cup_writestringerror("'%s' needs argument\n", badoption);
  else
    cup_writestringerror("unrecognized option '%s'\n", badoption);
  cup_writestringerror(
  "usage: %s [options] [script [args]]\n"
  "Available options are:\n"
  "  -e stat   execute string 'stat'\n"
  "  -i        enter interactive mode after executing 'script'\n"
  "  -l mod    require library 'mod' into global 'mod'\n"
  "  -l g=mod  require library 'mod' into global 'g'\n"
  "  -v        show version information\n"
  "  -E        ignore environment variables\n"
  "  -W        turn warnings on\n"
  "  --        stop handling options\n"
  "  -         stop handling options and execute stdin\n"
  ,
  progname);
}


/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) cup_writestringerror("%s: ", pname);
  cup_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Cup or by 'msghandler'.
*/
static int report (cup_State *L, int status) {
  if (status != CUP_OK) {
    const char *msg = cup_tostring(L, -1);
    l_message(progname, msg);
    cup_pop(L, 1);  /* remove message */
  }
  return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler (cup_State *L) {
  const char *msg = cup_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (cupL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        cup_type(L, -1) == CUP_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = cup_pushfstring(L, "(error object is a %s value)",
                               cupL_typename(L, 1));
  }
  cupL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'cup_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (cup_State *L, int narg, int nres) {
  int status;
  int base = cup_gettop(L) - narg;  /* function index */
  cup_pushcfunction(L, msghandler);  /* push message handler */
  cup_insert(L, base);  /* put it under function and args */
  globalL = L;  /* to be available to 'laction' */
  setsignal(SIGINT, laction);  /* set C-signal handler */
  status = cup_pcall(L, narg, nres, base);
  setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
  cup_remove(L, base);  /* remove message handler from the stack */
  return status;
}


static void print_version (void) {
  cup_writestring(CUP_COPYRIGHT, strlen(CUP_COPYRIGHT));
  cup_writeline();
}


/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') Cup to positive indices;
** other arguments (before the script name) Cup to negative indices.
** If there is no script name, assume interpreter's name as base.
*/
static void createargtable (cup_State *L, char **argv, int argc, int script) {
  int i, narg;
  if (script == argc) script = 0;  /* no script name? */
  narg = argc - (script + 1);  /* number of positive indices */
  cup_createtable(L, narg, script + 1);
  for (i = 0; i < argc; i++) {
    cup_pushstring(L, argv[i]);
    cup_rawseti(L, -2, i - script);
  }
  cup_setglobal(L, "arg");
}


static int dochunk (cup_State *L, int status) {
  if (status == CUP_OK) status = docall(L, 0, 0);
  return report(L, status);
}


static int dofile (cup_State *L, const char *name) {
  return dochunk(L, cupL_loadfile(L, name));
}


static int dostring (cup_State *L, const char *s, const char *name) {
  return dochunk(L, cupL_loadbuffer(L, s, strlen(s), name));
}


/*
** Receives 'globname[=modname]' and runs 'globname = require(modname)'.
*/
static int dolibrary (cup_State *L, char *globname) {
  int status;
  char *modname = strchr(globname, '=');
  if (modname == NULL)  /* no explicit name? */
    modname = globname;  /* module name is equal to global name */
  else {
    *modname = '\0';  /* global name ends here */
    modname++;  /* module name starts after the '=' */
  }
  cup_getglobal(L, "require");
  cup_pushstring(L, modname);
  status = docall(L, 1, 1);  /* call 'require(modname)' */
  if (status == CUP_OK)
    cup_setglobal(L, globname);  /* globname = require(modname) */
  return report(L, status);
}


/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs (cup_State *L) {
  int i, n;
  if (cup_getglobal(L, "arg") != CUP_TTABLE)
    cupL_error(L, "'arg' is not a table");
  n = (int)cupL_len(L, -1);
  cupL_checkstack(L, n + 3, "too many arguments to script");
  for (i = 1; i <= n; i++)
    cup_rawgeti(L, -i, i);
  cup_remove(L, -i);  /* remove table from the stack */
  return n;
}


static int handle_script (cup_State *L, char **argv) {
  int status;
  const char *fname = argv[0];
  if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
    fname = NULL;  /* stdin */
  status = cupL_loadfile(L, fname);
  if (status == CUP_OK) {
    int n = pushargs(L);  /* push arguments to script */
    status = docall(L, n, CUP_MULTRET);
  }
  return report(L, status);
}


/* bits of various argument indicators in 'args' */
#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */


/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any Cup code (or an error code if it finds
** any invalid argument). 'first' returns the first not-handled argument
** (either the script name or a bad argument in case of error).
*/
static int collectargs (char **argv, int *first) {
  int args = 0;
  int i;
  for (i = 1; argv[i] != NULL; i++) {
    *first = i;
    if (argv[i][0] != '-')  /* not an option? */
        return args;  /* stop handling options */
    switch (argv[i][1]) {  /* else check option */
      case '-':  /* '--' */
        if (argv[i][2] != '\0')  /* extra characters after '--'? */
          return has_error;  /* invalid option */
        *first = i + 1;
        return args;
      case '\0':  /* '-' */
        return args;  /* script "name" is '-' */
      case 'E':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_E;
        break;
      case 'W':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        break;
      case 'i':
        args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
      case 'v':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_v;
        break;
      case 'e':
        args |= has_e;  /* FALLTHROUGH */
      case 'l':  /* both options need an argument */
        if (argv[i][2] == '\0') {  /* no concatenated argument? */
          i++;  /* try next 'argv' */
          if (argv[i] == NULL || argv[i][0] == '-')
            return has_error;  /* no next argument or it is another option */
        }
        break;
      default:  /* invalid option */
        return has_error;
    }
  }
  *first = i;  /* no script name */
  return args;
}


/*
** Processes options 'e' and 'l', which involve running Cup code, and
** 'W', which also affects the state.
** Returns 0 if some code raises an error.
*/
static int runargs (cup_State *L, char **argv, int n) {
  int i;
  for (i = 1; i < n; i++) {
    int option = argv[i][1];
    cup_assert(argv[i][0] == '-');  /* already checked */
    switch (option) {
      case 'e':  case 'l': {
        int status;
        char *extra = argv[i] + 2;  /* both options need an argument */
        if (*extra == '\0') extra = argv[++i];
        cup_assert(extra != NULL);
        status = (option == 'e')
                 ? dostring(L, extra, "=(command line)")
                 : dolibrary(L, extra);
        if (status != CUP_OK) return 0;
        break;
      }
      case 'W':
        cup_warning(L, "@on", 0);  /* warnings on */
        break;
    }
  }
  return 1;
}


static int handle_cupinit (cup_State *L) {
  const char *name = "=" CUP_INITVARVERSION;
  const char *init = getenv(name + 1);
  if (init == NULL) {
    name = "=" CUP_INIT_VAR;
    init = getenv(name + 1);  /* try alternative name */
  }
  if (init == NULL) return CUP_OK;
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, name);
}


/*
** {==================================================================
** Read-Eval-Print Loop (REPL)
** ===================================================================
*/

#if !defined(CUP_PROMPT)
#define CUP_PROMPT		"> "
#define CUP_PROMPT2		">> "
#endif

#if !defined(CUP_MAXINPUT)
#define CUP_MAXINPUT		512
#endif


/*
** cup_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running cup interactively).
*/
#if !defined(cup_stdin_is_tty)	/* { */

#if defined(CUP_USE_POSIX)	/* { */

#include <unistd.h>
#define cup_stdin_is_tty()	isatty(0)

#elif defined(CUP_USE_WINDOWS)	/* }{ */

#include <io.h>
#include <windows.h>

#define cup_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define cup_stdin_is_tty()	1  /* assume stdin is a tty */

#endif				/* } */

#endif				/* } */


/*
** cup_readline defines how to show a prompt and then read a line from
** the standard input.
** cup_saveline defines how to "save" a read line in a "history".
** cup_freeline defines how to free a line read by cup_readline.
*/
#if !defined(cup_readline)	/* { */

#if defined(CUP_USE_READLINE)	/* { */

#include <readline/readline.h>
#include <readline/history.h>
#define cup_initreadline(L)	((void)L, rl_readline_name="cup")
#define cup_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define cup_saveline(L,line)	((void)L, add_history(line))
#define cup_freeline(L,b)	((void)L, free(b))

#else				/* }{ */

#define cup_initreadline(L)  ((void)L)
#define cup_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, CUP_MAXINPUT, stdin) != NULL)  /* get line */
#define cup_saveline(L,line)	{ (void)L; (void)line; }
#define cup_freeline(L,b)	{ (void)L; (void)b; }

#endif				/* } */

#endif				/* } */


/*
** Return the string to be used as a prompt by the interpreter. Leave
** the string (or nil, if using the default value) on the stack, to keep
** it anchored.
*/
static const char *get_prompt (cup_State *L, int firstline) {
  if (cup_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2") == CUP_TNIL)
    return (firstline ? CUP_PROMPT : CUP_PROMPT2);  /* use the default */
  else {  /* apply 'tostring' over the value */
    const char *p = cupL_tolstring(L, -1, NULL);
    cup_remove(L, -2);  /* remove original value */
    return p;
  }
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete (cup_State *L, int status) {
  if (status == CUP_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = cup_tolstring(L, -1, &lmsg);
    if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
      cup_pop(L, 1);
      return 1;
    }
  }
  return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the Cup stack.
*/
static int pushline (cup_State *L, int firstline) {
  char buffer[CUP_MAXINPUT];
  char *b = buffer;
  size_t l;
  const char *prmt = get_prompt(L, firstline);
  int readstatus = cup_readline(L, b, prmt);
  if (readstatus == 0)
    return 0;  /* no input (prompt will be popped by caller) */
  cup_pop(L, 1);  /* remove prompt */
  l = strlen(b);
  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
    b[--l] = '\0';  /* remove it */
  if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
    cup_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
  else
    cup_pushlstring(L, b, l);
  cup_freeline(L, b);
  return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn (cup_State *L) {
  const char *line = cup_tostring(L, -1);  /* original line */
  const char *retline = cup_pushfstring(L, "return %s;", line);
  int status = cupL_loadbuffer(L, retline, strlen(retline), "=stdin");
  if (status == CUP_OK) {
    cup_remove(L, -2);  /* remove modified line */
    if (line[0] != '\0')  /* non empty? */
      cup_saveline(L, line);  /* keep history */
  }
  else
    cup_pop(L, 2);  /* pop result from 'cupL_loadbuffer' and modified line */
  return status;
}


/*
** Read multiple lines until a complete Cup statement
*/
static int multiline (cup_State *L) {
  for (;;) {  /* repeat until gets a complete statement */
    size_t len;
    const char *line = cup_tolstring(L, 1, &len);  /* get what it has */
    int status = cupL_loadbuffer(L, line, len, "=stdin");  /* try it */
    if (!incomplete(L, status) || !pushline(L, 0)) {
      cup_saveline(L, line);  /* keep history */
      return status;  /* cannot or should not try to add continuation line */
    }
    cup_pushliteral(L, "\n");  /* add newline... */
    cup_insert(L, -2);  /* ...between the two lines */
    cup_concat(L, 3);  /* join them */
  }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline (cup_State *L) {
  int status;
  cup_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  if ((status = addreturn(L)) != CUP_OK)  /* 'return ...' did not work? */
    status = multiline(L);  /* try as command, maybe with continuation lines */
  cup_remove(L, 1);  /* remove line from the stack */
  cup_assert(cup_gettop(L) == 1);
  return status;
}


/*
** Prints (calling the Cup 'print' function) any values on the stack
*/
static void l_print (cup_State *L) {
  int n = cup_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    cupL_checkstack(L, CUP_MINSTACK, "too many results to print");
    cup_getglobal(L, "print");
    cup_insert(L, 1);
    if (cup_pcall(L, n, 0, 0) != CUP_OK)
      l_message(progname, cup_pushfstring(L, "error calling 'print' (%s)",
                                             cup_tostring(L, -1)));
  }
}


/*
** Do the REPL: repeatedly read (load) a line, evacupte (call) it, and
** print any results.
*/
static void doREPL (cup_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;  /* no 'progname' on errors in interactive mode */
  cup_initreadline(L);
  while ((status = loadline(L)) != -1) {
    if (status == CUP_OK)
      status = docall(L, 0, CUP_MULTRET);
    if (status == CUP_OK) l_print(L);
    else report(L, status);
  }
  cup_settop(L, 0);  /* clear stack */
  cup_writeline();
  progname = oldprogname;
}

/* }================================================================== */


/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain (cup_State *L) {
  int argc = (int)cup_tointeger(L, 1);
  char **argv = (char **)cup_touserdata(L, 2);
  int script;
  int args = collectargs(argv, &script);
  cupL_checkversion(L);  /* check that interpreter has correct version */
  if (argv[0] && argv[0][0]) progname = argv[0];
  if (args == has_error) {  /* bad arg? */
    print_usage(argv[script]);  /* 'script' has index of bad arg. */
    return 0;
  }
  if (args & has_v)  /* option '-v'? */
    print_version();
  if (args & has_E) {  /* option '-E'? */
    cup_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    cup_setfield(L, CUP_REGISTRYINDEX, "CUP_NOENV");
  }
  cupL_openlibs(L);  /* open standard libraries */
  createargtable(L, argv, argc, script);  /* create table 'arg' */
  cup_gc(L, CUP_GCGEN, 0, 0);  /* GC in generational mode */
  if (!(args & has_E)) {  /* no option '-E'? */
    if (handle_cupinit(L) != CUP_OK)  /* run CUP_INIT */
      return 0;  /* error running CUP_INIT */
  }
  if (!runargs(L, argv, script))  /* execute arguments -e and -l */
    return 0;  /* something failed */
  if (script < argc &&  /* execute main script (if there is one) */
      handle_script(L, argv + script) != CUP_OK)
    return 0;
  if (args & has_i)  /* -i option? */
    doREPL(L);  /* do read-eval-print loop */
  else if (script == argc && !(args & (has_e | has_v))) {  /* no arguments? */
    if (cup_stdin_is_tty()) {  /* running in interactive mode? */
      print_version();
      doREPL(L);  /* do read-eval-print loop */
    }
    else dofile(L, NULL);  /* executes stdin as a file */
  }
  cup_pushboolean(L, 1);  /* signal no errors */
  return 1;
}


int main (int argc, char **argv) {
  int status, result;
  cup_State *L = cupL_newstate();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  cup_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  cup_pushinteger(L, argc);  /* 1st argument */
  cup_pushlightuserdata(L, argv); /* 2nd argument */
  status = cup_pcall(L, 2, 1, 0);  /* do the call */
  result = cup_toboolean(L, -1);  /* get result */
  report(L, status);
  cup_close(L);
  return (result && status == CUP_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

