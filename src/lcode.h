/*
** $Id: lcode.h $
** Code generator for Cup
** See Copyright Notice in cup.h
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


#define cupK_codeABC(fs,o,a,b,c)	cupK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define cupK_setmultret(fs,e)	cupK_setreturns(fs, e, CUP_MULTRET)

#define cupK_jumpto(fs,t)	cupK_patchlist(fs, cupK_jump(fs), t)

CUPI_FUNC int cupK_code (FuncState *fs, Instruction i);
CUPI_FUNC int cupK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
CUPI_FUNC int cupK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
CUPI_FUNC int cupK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
CUPI_FUNC int cupK_isKint (expdesc *e);
CUPI_FUNC int cupK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
CUPI_FUNC void cupK_fixline (FuncState *fs, int line);
CUPI_FUNC void cupK_nil (FuncState *fs, int from, int n);
CUPI_FUNC void cupK_reserveregs (FuncState *fs, int n);
CUPI_FUNC void cupK_checkstack (FuncState *fs, int n);
CUPI_FUNC void cupK_int (FuncState *fs, int reg, cup_Integer n);
CUPI_FUNC void cupK_dischargevars (FuncState *fs, expdesc *e);
CUPI_FUNC int cupK_exp2anyreg (FuncState *fs, expdesc *e);
CUPI_FUNC void cupK_exp2anyregup (FuncState *fs, expdesc *e);
CUPI_FUNC void cupK_exp2nextreg (FuncState *fs, expdesc *e);
CUPI_FUNC void cupK_exp2val (FuncState *fs, expdesc *e);
CUPI_FUNC int cupK_exp2RK (FuncState *fs, expdesc *e);
CUPI_FUNC void cupK_self (FuncState *fs, expdesc *e, expdesc *key);
CUPI_FUNC void cupK_indexed (FuncState *fs, expdesc *t, expdesc *k);
CUPI_FUNC void cupK_Cupiftrue (FuncState *fs, expdesc *e);
CUPI_FUNC void cupK_Cupiffalse (FuncState *fs, expdesc *e);
CUPI_FUNC void cupK_storevar (FuncState *fs, expdesc *var, expdesc *e);
CUPI_FUNC void cupK_setreturns (FuncState *fs, expdesc *e, int nresults);
CUPI_FUNC void cupK_setoneret (FuncState *fs, expdesc *e);
CUPI_FUNC int cupK_jump (FuncState *fs);
CUPI_FUNC void cupK_ret (FuncState *fs, int first, int nret);
CUPI_FUNC void cupK_patchlist (FuncState *fs, int list, int target);
CUPI_FUNC void cupK_patchtohere (FuncState *fs, int list);
CUPI_FUNC void cupK_concat (FuncState *fs, int *l1, int l2);
CUPI_FUNC int cupK_getlabel (FuncState *fs);
CUPI_FUNC void cupK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
CUPI_FUNC void cupK_infix (FuncState *fs, BinOpr op, expdesc *v);
CUPI_FUNC void cupK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
CUPI_FUNC void cupK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
CUPI_FUNC void cupK_setlist (FuncState *fs, int base, int nelems, int tostore);
CUPI_FUNC void cupK_finish (FuncState *fs);
CUPI_FUNC l_noret cupK_semerror (LexState *ls, const char *msg);


#endif
