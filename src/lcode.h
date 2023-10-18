/*
** $Id: lcode.h $
** Code generator for Acorn
** See Copyright Notice in acorn.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define acornK_codeABC(fs,o,a,b,c)	acornK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define acornK_setmultret(fs,e)	acornK_setreturns(fs, e, ACORN_MULTRET)

#define acornK_jumpto(fs,t)	acornK_patchlist(fs, acornK_jump(fs), t)

ACORNI_FUNC int acornK_code (FuncState *fs, Instruction i);
ACORNI_FUNC int acornK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
ACORNI_FUNC int acornK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
ACORNI_FUNC int acornK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
ACORNI_FUNC int acornK_isKint (expdesc *e);
ACORNI_FUNC int acornK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
ACORNI_FUNC void acornK_fixline (FuncState *fs, int line);
ACORNI_FUNC void acornK_nil (FuncState *fs, int from, int n);
ACORNI_FUNC void acornK_reserveregs (FuncState *fs, int n);
ACORNI_FUNC void acornK_checkstack (FuncState *fs, int n);
ACORNI_FUNC void acornK_int (FuncState *fs, int reg, acorn_Integer n);
ACORNI_FUNC void acornK_dischargevars (FuncState *fs, expdesc *e);
ACORNI_FUNC int acornK_exp2anyreg (FuncState *fs, expdesc *e);
ACORNI_FUNC void acornK_exp2anyregup (FuncState *fs, expdesc *e);
ACORNI_FUNC void acornK_exp2nextreg (FuncState *fs, expdesc *e);
ACORNI_FUNC void acornK_exp2val (FuncState *fs, expdesc *e);
ACORNI_FUNC int acornK_exp2RK (FuncState *fs, expdesc *e);
ACORNI_FUNC void acornK_self (FuncState *fs, expdesc *e, expdesc *key);
ACORNI_FUNC void acornK_indexed (FuncState *fs, expdesc *t, expdesc *k);
ACORNI_FUNC void acornK_Acorniftrue (FuncState *fs, expdesc *e);
ACORNI_FUNC void acornK_Acorniffalse (FuncState *fs, expdesc *e);
ACORNI_FUNC void acornK_storevar (FuncState *fs, expdesc *var, expdesc *e);
ACORNI_FUNC void acornK_setreturns (FuncState *fs, expdesc *e, int nresults);
ACORNI_FUNC void acornK_setoneret (FuncState *fs, expdesc *e);
ACORNI_FUNC int acornK_jump (FuncState *fs);
ACORNI_FUNC void acornK_ret (FuncState *fs, int first, int nret);
ACORNI_FUNC void acornK_patchlist (FuncState *fs, int list, int target);
ACORNI_FUNC void acornK_patchtohere (FuncState *fs, int list);
ACORNI_FUNC void acornK_concat (FuncState *fs, int *l1, int l2);
ACORNI_FUNC int acornK_getlabel (FuncState *fs);
ACORNI_FUNC void acornK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
ACORNI_FUNC void acornK_infix (FuncState *fs, BinOpr op, expdesc *v);
ACORNI_FUNC void acornK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
ACORNI_FUNC void acornK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
ACORNI_FUNC void acornK_setlist (FuncState *fs, int base, int nelems, int tostore);
ACORNI_FUNC void acornK_finish (FuncState *fs);
ACORNI_FUNC l_noret acornK_semerror (LexState *ls, const char *msg);


#endif
