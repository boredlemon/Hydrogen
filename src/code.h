/*
** $Id: code.h $
** Code generator for Venom
** See Copyright Notice in venom.h
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


#define venomK_codeABC(fs,o,a,b,c)	venomK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define venomK_setmultret(fs,e)	venomK_setreturns(fs, e, VENOM_MULTRET)

#define venomK_jumpto(fs,t)	venomK_patchlist(fs, venomK_jump(fs), t)

VENOMI_FUNC int venomK_code (FuncState *fs, Instruction i);
VENOMI_FUNC int venomK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
VENOMI_FUNC int venomK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
VENOMI_FUNC int venomK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
VENOMI_FUNC int venomK_isKint (expdesc *e);
VENOMI_FUNC int venomK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
VENOMI_FUNC void venomK_fixline (FuncState *fs, int line);
VENOMI_FUNC void venomK_nil (FuncState *fs, int from, int n);
VENOMI_FUNC void venomK_reserveregs (FuncState *fs, int n);
VENOMI_FUNC void venomK_checkstack (FuncState *fs, int n);
VENOMI_FUNC void venomK_int (FuncState *fs, int reg, venom_Integer n);
VENOMI_FUNC void venomK_dischargevars (FuncState *fs, expdesc *e);
VENOMI_FUNC int venomK_exp2anyreg (FuncState *fs, expdesc *e);
VENOMI_FUNC void venomK_exp2anyregup (FuncState *fs, expdesc *e);
VENOMI_FUNC void venomK_exp2nextreg (FuncState *fs, expdesc *e);
VENOMI_FUNC void venomK_exp2val (FuncState *fs, expdesc *e);
VENOMI_FUNC int venomK_exp2RK (FuncState *fs, expdesc *e);
VENOMI_FUNC void venomK_self (FuncState *fs, expdesc *e, expdesc *key);
VENOMI_FUNC void venomK_indexed (FuncState *fs, expdesc *t, expdesc *k);
VENOMI_FUNC void venomK_Venomiftrue (FuncState *fs, expdesc *e);
VENOMI_FUNC void venomK_Venomiffalse (FuncState *fs, expdesc *e);
VENOMI_FUNC void venomK_storevar (FuncState *fs, expdesc *var, expdesc *e);
VENOMI_FUNC void venomK_setreturns (FuncState *fs, expdesc *e, int nresults);
VENOMI_FUNC void venomK_setoneret (FuncState *fs, expdesc *e);
VENOMI_FUNC int venomK_jump (FuncState *fs);
VENOMI_FUNC void venomK_ret (FuncState *fs, int first, int nret);
VENOMI_FUNC void venomK_patchlist (FuncState *fs, int list, int target);
VENOMI_FUNC void venomK_patchtohere (FuncState *fs, int list);
VENOMI_FUNC void venomK_concat (FuncState *fs, int *l1, int l2);
VENOMI_FUNC int venomK_getlabel (FuncState *fs);
VENOMI_FUNC void venomK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
VENOMI_FUNC void venomK_infix (FuncState *fs, BinOpr op, expdesc *v);
VENOMI_FUNC void venomK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
VENOMI_FUNC void venomK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
VENOMI_FUNC void venomK_setlist (FuncState *fs, int base, int nelems, int tostore);
VENOMI_FUNC void venomK_finish (FuncState *fs);
VENOMI_FUNC l_noret venomK_semerror (LexState *ls, const char *msg);


#endif
