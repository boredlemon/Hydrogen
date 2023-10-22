/*
** $Id: viper.c $
** Viper stand-alone interpreter
** See Copyright Notice in viper.h
*/

#define viper_c

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include "viper.h"

#include "lauxlib.h"
#include "viperlib.h"


#if !defined(VIPER_PROGNAME)
#define VIPER_PROGNAME		"viper"
#endif

#if !defined(VIPER_INIT_VAR)
#define VIPER_INIT_VAR		"VIPER_INIT"
#endif

#define VIPER_INITVARVERSION	VIPER_INIT_VAR VIPER_VERSUFFIX


static viper_State *globalL = NULL;

static const char *progname = VIPER_PROGNAME;


#if defined(VIPER_USE_POSIX)   /* { */

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
static void lstop (viper_State *L, viper_Debug *ar) {
  (void)ar;  /* unused arg. */
  viper_sethook(L, NULL, 0, 0);  /* reset hook */
  viperL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Viper state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction (int i) {
  int flag = VIPER_MASKCALL | VIPER_MASKRET | VIPER_MASKLINE | VIPER_MASKCOUNT;
  setsignal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  viper_sethook(globalL, lstop, flag, 1);
}


static void print_usage (const char *badoption) {
  viper_writestringerror("%s: ", progname);
  if (badoption[1] == 'e' || badoption[1] == 'l')
    viper_writestringerror("'%s' needs argument\n", badoption);
  else
    viper_writestringerror("unrecognized option '%s'\n", badoption);
  viper_writestringerror(
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
  if (pname) viper_writestringerror("%s: ", pname);
  viper_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Viper or by 'msghandler'.
*/
static int report (viper_State *L, int status) {
  if (status != VIPER_OK) {
    const char *msg = viper_tostring(L, -1);
    l_message(progname, msg);
    viper_pop(L, 1);  /* remove message */
  }
  return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler (viper_State *L) {
  const char *msg = viper_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (viperL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        viper_type(L, -1) == VIPER_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = viper_pushfstring(L, "(error object is a %s value)",
                               viperL_typename(L, 1));
  }
  viperL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'viper_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (viper_State *L, int narg, int nres) {
  int status;
  int base = viper_gettop(L) - narg;  /* function index */
  viper_pushcfunction(L, msghandler);  /* push message handler */
  viper_insert(L, base);  /* put it under function and args */
  globalL = L;  /* to be available to 'laction' */
  setsignal(SIGINT, laction);  /* set C-signal handler */
  status = viper_pcall(L, narg, nres, base);
  setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
  viper_remove(L, base);  /* remove message handler from the stack */
  return status;
}


static void print_version (void) {
  viper_writestring(VIPER_COPYRIGHT, strlen(VIPER_COPYRIGHT));
  viper_writeline();
}


/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') Viper to positive indices;
** other arguments (before the script name) Viper to negative indices.
** If there is no script name, assume interpreter's name as base.
*/
static void createargtable (viper_State *L, char **argv, int argc, int script) {
  int i, narg;
  if (script == argc) script = 0;  /* no script name? */
  narg = argc - (script + 1);  /* number of positive indices */
  viper_createtable(L, narg, script + 1);
  for (i = 0; i < argc; i++) {
    viper_pushstring(L, argv[i]);
    viper_rawseti(L, -2, i - script);
  }
  viper_setglobal(L, "arg");
}


static int dochunk (viper_State *L, int status) {
  if (status == VIPER_OK) status = docall(L, 0, 0);
  return report(L, status);
}


static int dofile (viper_State *L, const char *name) {
  return dochunk(L, viperL_loadfile(L, name));
}


static int dostring (viper_State *L, const char *s, const char *name) {
  return dochunk(L, viperL_loadbuffer(L, s, strlen(s), name));
}


/*
** Receives 'globname[=modname]' and runs 'globname = require(modname)'.
*/
static int dolibrary (viper_State *L, char *globname) {
  int status;
  char *modname = strchr(globname, '=');
  if (modname == NULL)  /* no explicit name? */
    modname = globname;  /* module name is equal to global name */
  else {
    *modname = '\0';  /* global name ends here */
    modname++;  /* module name starts after the '=' */
  }
  viper_getglobal(L, "require");
  viper_pushstring(L, modname);
  status = docall(L, 1, 1);  /* call 'require(modname)' */
  if (status == VIPER_OK)
    viper_setglobal(L, globname);  /* globname = require(modname) */
  return report(L, status);
}


/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs (viper_State *L) {
  int i, n;
  if (viper_getglobal(L, "arg") != VIPER_TTABLE)
    viperL_error(L, "'arg' is not a table");
  n = (int)viperL_len(L, -1);
  viperL_checkstack(L, n + 3, "too many arguments to script");
  for (i = 1; i <= n; i++)
    viper_rawgeti(L, -i, i);
  viper_remove(L, -i);  /* remove table from the stack */
  return n;
}


static int handle_script (viper_State *L, char **argv) {
  int status;
  const char *fname = argv[0];
  if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
    fname = NULL;  /* stdin */
  status = viperL_loadfile(L, fname);
  if (status == VIPER_OK) {
    int n = pushargs(L);  /* push arguments to script */
    status = docall(L, n, VIPER_MULTRET);
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
** needed before running any Viper code (or an error code if it finds
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
** Processes options 'e' and 'l', which involve running Viper code, and
** 'W', which also affects the state.
** Returns 0 if some code raises an error.
*/
static int runargs (viper_State *L, char **argv, int n) {
  int i;
  for (i = 1; i < n; i++) {
    int option = argv[i][1];
    viper_assert(argv[i][0] == '-');  /* already checked */
    switch (option) {
      case 'e':  case 'l': {
        int status;
        char *extra = argv[i] + 2;  /* both options need an argument */
        if (*extra == '\0') extra = argv[++i];
        viper_assert(extra != NULL);
        status = (option == 'e')
                 ? dostring(L, extra, "=(command line)")
                 : dolibrary(L, extra);
        if (status != VIPER_OK) return 0;
        break;
      }
      case 'W':
        viper_warning(L, "@on", 0);  /* warnings on */
        break;
    }
  }
  return 1;
}


static int handle_viperinit (viper_State *L) {
  const char *name = "=" VIPER_INITVARVERSION;
  const char *init = getenv(name + 1);
  if (init == NULL) {
    name = "=" VIPER_INIT_VAR;
    init = getenv(name + 1);  /* try alternative name */
  }
  if (init == NULL) return VIPER_OK;
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

#if !defined(VIPER_PROMPT)
#define VIPER_PROMPT		"🐍 ==> " 
#define VIPER_PROMPT2		"🐍🦕 ==>> "
#endif

#if !defined(VIPER_MAXINPUT)
#define VIPER_MAXINPUT		512
#endif


/*
** viper_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running viper interactively).
*/
#if !defined(viper_stdin_is_tty)	/* { */

#if defined(VIPER_USE_POSIX)	/* { */

#include <unistd.h>
#define viper_stdin_is_tty()	isatty(0)

#elif defined(VIPER_USE_WINDOWS)	/* }{ */

#include <io.h>
#include <windows.h>

#define viper_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define viper_stdin_is_tty()	1  /* assume stdin is a tty */

#endif				/* } */

#endif				/* } */


/*
** viper_readline defines how to show a prompt and then read a line from
** the standard input.
** viper_saveline defines how to "save" a read line in a "history".
** viper_freeline defines how to free a line read by viper_readline.
*/
#if !defined(viper_readline)	/* { */

#if defined(VIPER_USE_READLINE)	/* { */

#include <readline/readline.h>
#include <readline/history.h>
#define viper_initreadline(L)	((void)L, rl_readline_name="viper")
#define viper_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define viper_saveline(L,line)	((void)L, add_history(line))
#define viper_freeline(L,b)	((void)L, free(b))

#else				/* }{ */

#define viper_initreadline(L)  ((void)L)
#define viper_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, VIPER_MAXINPUT, stdin) != NULL)  /* get line */
#define viper_saveline(L,line)	{ (void)L; (void)line; }
#define viper_freeline(L,b)	{ (void)L; (void)b; }

#endif				/* } */

#endif				/* } */


/*
** Return the string to be used as a prompt by the interpreter. Leave
** the string (or nil, if using the default value) on the stack, to keep
** it anchored.
*/
static const char *get_prompt (viper_State *L, int firstline) {
  if (viper_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2") == VIPER_TNIL)
    return (firstline ? VIPER_PROMPT : VIPER_PROMPT2);  /* use the default */
  else {  /* apply 'tostring' over the value */
    const char *p = viperL_tolstring(L, -1, NULL);
    viper_remove(L, -2);  /* remove original value */
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
static int incomplete (viper_State *L, int status) {
  if (status == VIPER_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = viper_tolstring(L, -1, &lmsg);
    if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
      viper_pop(L, 1);
      return 1;
    }
  }
  return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the Viper stack.
*/
static int pushline (viper_State *L, int firstline) {
  char buffer[VIPER_MAXINPUT];
  char *b = buffer;
  size_t l;
  const char *prmt = get_prompt(L, firstline);
  int readstatus = viper_readline(L, b, prmt);
  if (readstatus == 0)
    return 0;  /* no input (prompt will be popped by caller) */
  viper_pop(L, 1);  /* remove prompt */
  l = strlen(b);
  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
    b[--l] = '\0';  /* remove it */
  if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
    viper_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
  else
    viper_pushlstring(L, b, l);
  viper_freeline(L, b);
  return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn (viper_State *L) {
  const char *line = viper_tostring(L, -1);  /* original line */
  const char *retline = viper_pushfstring(L, "return %s;", line);
  int status = viperL_loadbuffer(L, retline, strlen(retline), "=stdin");
  if (status == VIPER_OK) {
    viper_remove(L, -2);  /* remove modified line */
    if (line[0] != '\0')  /* non empty? */
      viper_saveline(L, line);  /* keep history */
  }
  else
    viper_pop(L, 2);  /* pop result from 'viperL_loadbuffer' and modified line */
  return status;
}


/*
** Read multiple lines until a complete Viper statement
*/
static int multiline (viper_State *L) {
  for (;;) {  /* repeat until gets a complete statement */
    size_t len;
    const char *line = viper_tolstring(L, 1, &len);  /* get what it has */
    int status = viperL_loadbuffer(L, line, len, "=stdin");  /* try it */
    if (!incomplete(L, status) || !pushline(L, 0)) {
      viper_saveline(L, line);  /* keep history */
      return status;  /* cannot or should not try to add continuation line */
    }
    viper_pushliteral(L, "\n");  /* add newline... */
    viper_insert(L, -2);  /* ...between the two lines */
    viper_concat(L, 3);  /* join them */
  }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline (viper_State *L) {
  int status;
  viper_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  if ((status = addreturn(L)) != VIPER_OK)  /* 'return ...' did not work? */
    status = multiline(L);  /* try as command, maybe with continuation lines */
  viper_remove(L, 1);  /* remove line from the stack */
  viper_assert(viper_gettop(L) == 1);
  return status;
}


/*
** Prints (calling the Viper 'print' function) any values on the stack
*/
static void l_print (viper_State *L) {
  int n = viper_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    viperL_checkstack(L, VIPER_MINSTACK, "too many results to print");
    viper_getglobal(L, "print");
    viper_insert(L, 1);
    if (viper_pcall(L, n, 0, 0) != VIPER_OK)
      l_message(progname, viper_pushfstring(L, "error calling 'print' (%s)",
                                             viper_tostring(L, -1)));
  }
}


/*
** Do the REPL: repeatedly read (load) a line, evaviperte (call) it, and
** print any results.
*/
static void doREPL (viper_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;  /* no 'progname' on errors in interactive mode */
  viper_initreadline(L);
  while ((status = loadline(L)) != -1) {
    if (status == VIPER_OK)
      status = docall(L, 0, VIPER_MULTRET);
    if (status == VIPER_OK) l_print(L);
    else report(L, status);
  }
  viper_settop(L, 0);  /* clear stack */
  viper_writeline();
  progname = oldprogname;
}

/* }================================================================== */


/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain (viper_State *L) {
  int argc = (int)viper_tointeger(L, 1);
  char **argv = (char **)viper_touserdata(L, 2);
  int script;
  int args = collectargs(argv, &script);
  viperL_checkversion(L);  /* check that interpreter has correct version */
  if (argv[0] && argv[0][0]) progname = argv[0];
  if (args == has_error) {  /* bad arg? */
    print_usage(argv[script]);  /* 'script' has index of bad arg. */
    return 0;
  }
  if (args & has_v)  /* option '-v'? */
    print_version();
  if (args & has_E) {  /* option '-E'? */
    viper_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    viper_setfield(L, VIPER_REGISTRYINDEX, "VIPER_NOENV");
  }
  viperL_openlibs(L);  /* open standard libraries */
  createargtable(L, argv, argc, script);  /* create table 'arg' */
  viper_gc(L, VIPER_GCGEN, 0, 0);  /* GC in generational mode */
  if (!(args & has_E)) {  /* no option '-E'? */
    if (handle_viperinit(L) != VIPER_OK)  /* run VIPER_INIT */
      return 0;  /* error running VIPER_INIT */
  }
  if (!runargs(L, argv, script))  /* execute arguments -e and -l */
    return 0;  /* something failed */
  if (script < argc &&  /* execute main script (if there is one) */
      handle_script(L, argv + script) != VIPER_OK)
    return 0;
  if (args & has_i)  /* -i option? */
    doREPL(L);  /* do read-eval-print loop */
  else if (script == argc && !(args & (has_e | has_v))) {  /* no arguments? */
    if (viper_stdin_is_tty()) {  /* running in interactive mode? */
      print_version();
      doREPL(L);  /* do read-eval-print loop */
    }
    else dofile(L, NULL);  /* executes stdin as a file */
  }
  viper_pushboolean(L, 1);  /* signal no errors */
  return 1;
}


int main (int argc, char **argv) {
  int status, result;
  viper_State *L = viperL_newstate();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  viper_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  viper_pushinteger(L, argc);  /* 1st argument */
  viper_pushlightuserdata(L, argv); /* 2nd argument */
  status = viper_pcall(L, 2, 1, 0);  /* do the call */
  result = viper_toboolean(L, -1);  /* get result */
  report(L, status);
  viper_close(L);
  return (result && status == VIPER_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
