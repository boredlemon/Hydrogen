/*
** $Id: code.h $
** Code generator for Nebula
** See Copyright Notice in nebula.h
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


#define nebulaK_codeABC(fs,o,a,b,c)	nebulaK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define nebulaK_setmultret(fs,e)	nebulaK_setreturns(fs, e, NEBULA_MULTRET)

#define nebulaK_jumpto(fs,t)	nebulaK_patchlist(fs, nebulaK_jump(fs), t)

NEBULAI_FUNC int nebulaK_code (FuncState *fs, Instruction i);
NEBULAI_FUNC int nebulaK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
NEBULAI_FUNC int nebulaK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
NEBULAI_FUNC int nebulaK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
NEBULAI_FUNC int nebulaK_isKint (expdesc *e);
NEBULAI_FUNC int nebulaK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
NEBULAI_FUNC void nebulaK_fixline (FuncState *fs, int line);
NEBULAI_FUNC void nebulaK_nil (FuncState *fs, int from, int n);
NEBULAI_FUNC void nebulaK_reserveregs (FuncState *fs, int n);
NEBULAI_FUNC void nebulaK_checkstack (FuncState *fs, int n);
NEBULAI_FUNC void nebulaK_int (FuncState *fs, int reg, nebula_Integer n);
NEBULAI_FUNC void nebulaK_dischargevars (FuncState *fs, expdesc *e);
NEBULAI_FUNC int nebulaK_exp2anyreg (FuncState *fs, expdesc *e);
NEBULAI_FUNC void nebulaK_exp2anyregup (FuncState *fs, expdesc *e);
NEBULAI_FUNC void nebulaK_exp2nextreg (FuncState *fs, expdesc *e);
NEBULAI_FUNC void nebulaK_exp2val (FuncState *fs, expdesc *e);
NEBULAI_FUNC int nebulaK_exp2RK (FuncState *fs, expdesc *e);
NEBULAI_FUNC void nebulaK_self (FuncState *fs, expdesc *e, expdesc *key);
NEBULAI_FUNC void nebulaK_indexed (FuncState *fs, expdesc *t, expdesc *k);
NEBULAI_FUNC void nebulaK_Nebulaiftrue (FuncState *fs, expdesc *e);
NEBULAI_FUNC void nebulaK_Nebulaiffalse (FuncState *fs, expdesc *e);
NEBULAI_FUNC void nebulaK_storevar (FuncState *fs, expdesc *var, expdesc *e);
NEBULAI_FUNC void nebulaK_setreturns (FuncState *fs, expdesc *e, int nresults);
NEBULAI_FUNC void nebulaK_setoneret (FuncState *fs, expdesc *e);
NEBULAI_FUNC int nebulaK_jump (FuncState *fs);
NEBULAI_FUNC void nebulaK_ret (FuncState *fs, int first, int nret);
NEBULAI_FUNC void nebulaK_patchlist (FuncState *fs, int list, int target);
NEBULAI_FUNC void nebulaK_patchtohere (FuncState *fs, int list);
NEBULAI_FUNC void nebulaK_concat (FuncState *fs, int *l1, int l2);
NEBULAI_FUNC int nebulaK_getlabel (FuncState *fs);
NEBULAI_FUNC void nebulaK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
NEBULAI_FUNC void nebulaK_infix (FuncState *fs, BinOpr op, expdesc *v);
NEBULAI_FUNC void nebulaK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
NEBULAI_FUNC void nebulaK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
NEBULAI_FUNC void nebulaK_setlist (FuncState *fs, int base, int nelems, int tostore);
NEBULAI_FUNC void nebulaK_finish (FuncState *fs);
NEBULAI_FUNC l_noret nebulaK_semerror (LexState *ls, const char *msg);


#endif
