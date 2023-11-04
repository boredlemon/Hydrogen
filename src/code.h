/*
** $Id: code.h $
** Code generator for Hydrogen
** See Copyright Notice in hydrogen.h
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


#define hydrogenK_codeABC(fs,o,a,b,c)	hydrogenK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define hydrogenK_setmultret(fs,e)	hydrogenK_setreturns(fs, e, HYDROGEN_MULTRET)

#define hydrogenK_jumpto(fs,t)	hydrogenK_patchlist(fs, hydrogenK_jump(fs), t)

HYDROGENI_FUNC int hydrogenK_code (FuncState *fs, Instruction i);
HYDROGENI_FUNC int hydrogenK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
HYDROGENI_FUNC int hydrogenK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
HYDROGENI_FUNC int hydrogenK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
HYDROGENI_FUNC int hydrogenK_isKint (expdesc *e);
HYDROGENI_FUNC int hydrogenK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
HYDROGENI_FUNC void hydrogenK_fixline (FuncState *fs, int line);
HYDROGENI_FUNC void hydrogenK_nil (FuncState *fs, int from, int n);
HYDROGENI_FUNC void hydrogenK_reserveregs (FuncState *fs, int n);
HYDROGENI_FUNC void hydrogenK_checkstack (FuncState *fs, int n);
HYDROGENI_FUNC void hydrogenK_int (FuncState *fs, int reg, hydrogen_Integer n);
HYDROGENI_FUNC void hydrogenK_dischargevars (FuncState *fs, expdesc *e);
HYDROGENI_FUNC int hydrogenK_exp2anyreg (FuncState *fs, expdesc *e);
HYDROGENI_FUNC void hydrogenK_exp2anyregup (FuncState *fs, expdesc *e);
HYDROGENI_FUNC void hydrogenK_exp2nextreg (FuncState *fs, expdesc *e);
HYDROGENI_FUNC void hydrogenK_exp2val (FuncState *fs, expdesc *e);
HYDROGENI_FUNC int hydrogenK_exp2RK (FuncState *fs, expdesc *e);
HYDROGENI_FUNC void hydrogenK_self (FuncState *fs, expdesc *e, expdesc *key);
HYDROGENI_FUNC void hydrogenK_indexed (FuncState *fs, expdesc *t, expdesc *k);
HYDROGENI_FUNC void hydrogenK_Hydrogeniftrue (FuncState *fs, expdesc *e);
HYDROGENI_FUNC void hydrogenK_Hydrogeniffalse (FuncState *fs, expdesc *e);
HYDROGENI_FUNC void hydrogenK_storevar (FuncState *fs, expdesc *var, expdesc *e);
HYDROGENI_FUNC void hydrogenK_setreturns (FuncState *fs, expdesc *e, int nresults);
HYDROGENI_FUNC void hydrogenK_setoneret (FuncState *fs, expdesc *e);
HYDROGENI_FUNC int hydrogenK_jump (FuncState *fs);
HYDROGENI_FUNC void hydrogenK_ret (FuncState *fs, int first, int nret);
HYDROGENI_FUNC void hydrogenK_patchlist (FuncState *fs, int list, int target);
HYDROGENI_FUNC void hydrogenK_patchtohere (FuncState *fs, int list);
HYDROGENI_FUNC void hydrogenK_concat (FuncState *fs, int *l1, int l2);
HYDROGENI_FUNC int hydrogenK_getlabel (FuncState *fs);
HYDROGENI_FUNC void hydrogenK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
HYDROGENI_FUNC void hydrogenK_infix (FuncState *fs, BinOpr op, expdesc *v);
HYDROGENI_FUNC void hydrogenK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
HYDROGENI_FUNC void hydrogenK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
HYDROGENI_FUNC void hydrogenK_setlist (FuncState *fs, int base, int nelems, int tostore);
HYDROGENI_FUNC void hydrogenK_finish (FuncState *fs);
HYDROGENI_FUNC l_noret hydrogenK_semerror (LexState *ls, const char *msg);


#endif
