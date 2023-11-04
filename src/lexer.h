/*
** $Id: lexer.h $
** Lexical Analyzer
** See Copyright Notice in hydrogen.h
*/

#ifndef lexer_h
#define lexer_h
 
#include <limits.h>

#include "object.h"
#include "zio.h"


/*
** Single-char tokens (terminal symbols) are represented by their own
** numeric code. Other tokens start at the following value.
*/
#define FIRST_RESERVED	(UCHAR_MAX + 1)


#if !defined(HYDROGEN_ENV)
#define HYDROGEN_ENV		"_ENV"
#endif


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_goto, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_IDIV, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE,
  TK_SHL, TK_SHR,
  TK_DBCOLON, TK_EOS,
  TK_FLT, TK_INT, TK_NAME, TK_STRING
};

/* number of reserved words */
#define NUM_RESERVED	(cast_int(TK_WHILE-FIRST_RESERVED + 1))


typedef union {
  hydrogen_Number r;
  hydrogen_Integer i;
  TString *ts;
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;


/* state of the lexer plus state of the parser when shared by all
   functions */
typedef struct LexState {
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token 'consumed' */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;  /* current function (parser) */
  struct hydrogen_State *L;
  ZIO *z;  /* input stream */
  Mbuffer *buff;  /* buffer for tokens */
  Table *h;  /* to avoid collection/reuse strings */
  struct Dyndata *dyd;  /* dynamic structures used by the parser */
  TString *source;  /* current source name */
  TString *envn;  /* environment variable name */
} LexState;


HYDROGENI_FUNC void hydrogenX_init (hydrogen_State *L);
HYDROGENI_FUNC void hydrogenX_setinput (hydrogen_State *L, LexState *ls, ZIO *z,
                              TString *source, int firstchar);
HYDROGENI_FUNC TString *hydrogenX_newstring (LexState *ls, const char *str, size_t l);
HYDROGENI_FUNC void hydrogenX_next (LexState *ls);
HYDROGENI_FUNC int hydrogenX_lookahead (LexState *ls);
HYDROGENI_FUNC l_noret hydrogenX_syntaxerror (LexState *ls, const char *s);
HYDROGENI_FUNC const char *hydrogenX_token2str (LexState *ls, int token);


#endif
