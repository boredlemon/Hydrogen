/*
** $Id: code.h $
** Code generator for Viper
** See Copyright Notice in viper.h
*/

#ifndef code_h
#define code_h

#include "lexer.h"
#include "object.h"
#include "opcodes.h"
#include "parser.h"


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


#define viperK_codeABC(fs,o,a,b,c)	viperK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define viperK_setmultret(fs,e)	viperK_setreturns(fs, e, VIPER_MULTRET)

#define viperK_jumpto(fs,t)	viperK_patchlist(fs, viperK_jump(fs), t)

VIPERI_FUNC int viperK_code (FuncState *fs, Instruction i);
VIPERI_FUNC int viperK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
VIPERI_FUNC int viperK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
VIPERI_FUNC int viperK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
VIPERI_FUNC int viperK_isKint (expdesc *e);
VIPERI_FUNC int viperK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
VIPERI_FUNC void viperK_fixline (FuncState *fs, int line);
VIPERI_FUNC void viperK_nil (FuncState *fs, int from, int n);
VIPERI_FUNC void viperK_reserveregs (FuncState *fs, int n);
VIPERI_FUNC void viperK_checkstack (FuncState *fs, int n);
VIPERI_FUNC void viperK_int (FuncState *fs, int reg, viper_Integer n);
VIPERI_FUNC void viperK_dischargevars (FuncState *fs, expdesc *e);
VIPERI_FUNC int viperK_exp2anyreg (FuncState *fs, expdesc *e);
VIPERI_FUNC void viperK_exp2anyregup (FuncState *fs, expdesc *e);
VIPERI_FUNC void viperK_exp2nextreg (FuncState *fs, expdesc *e);
VIPERI_FUNC void viperK_exp2val (FuncState *fs, expdesc *e);
VIPERI_FUNC int viperK_exp2RK (FuncState *fs, expdesc *e);
VIPERI_FUNC void viperK_self (FuncState *fs, expdesc *e, expdesc *key);
VIPERI_FUNC void viperK_indexed (FuncState *fs, expdesc *t, expdesc *k);
VIPERI_FUNC void viperK_Viperiftrue (FuncState *fs, expdesc *e);
VIPERI_FUNC void viperK_Viperiffalse (FuncState *fs, expdesc *e);
VIPERI_FUNC void viperK_storevar (FuncState *fs, expdesc *var, expdesc *e);
VIPERI_FUNC void viperK_setreturns (FuncState *fs, expdesc *e, int nresults);
VIPERI_FUNC void viperK_setoneret (FuncState *fs, expdesc *e);
VIPERI_FUNC int viperK_jump (FuncState *fs);
VIPERI_FUNC void viperK_ret (FuncState *fs, int first, int nret);
VIPERI_FUNC void viperK_patchlist (FuncState *fs, int list, int target);
VIPERI_FUNC void viperK_patchtohere (FuncState *fs, int list);
VIPERI_FUNC void viperK_concat (FuncState *fs, int *l1, int l2);
VIPERI_FUNC int viperK_getlabel (FuncState *fs);
VIPERI_FUNC void viperK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
VIPERI_FUNC void viperK_infix (FuncState *fs, BinOpr op, expdesc *v);
VIPERI_FUNC void viperK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
VIPERI_FUNC void viperK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
VIPERI_FUNC void viperK_setlist (FuncState *fs, int base, int nelems, int tostore);
VIPERI_FUNC void viperK_finish (FuncState *fs);
VIPERI_FUNC l_noret viperK_semerror (LexState *ls, const char *msg);


#endif
