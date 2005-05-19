/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#pragma once

enum OPCODE {
	AccNull,
	AccTrue,
	AccFalse,
	AccThis,
	AccInt,
	AccStack,
	AccGlobal,
	AccEnv,
	AccField,
	AccArray,
	AccBuiltin,
	SetStack,
	SetGlobal,
	SetEnv,
	SetField,
	SetArray,
	Push,
	Pop,
	Call,
	ObjCall,
	Jump,
	JumpIf,
	JumpIfNot,
	Trap,
	EndTrap,
	Ret,
	MakeEnv,
	Bool,
	Add,
	Sub,
	Mult,
	Div,
	Mod,
	Shl,
	Shr,
	UShr,
	Or,
	And,
	Xor,
	Eq,
	Neq,
	Gt,
	Gte,
	Lt,
	Lte,

	Last,

	FastOps = 128,
	AccStackFast,
	SetStackFast,
};

#ifdef PARAMETER_TABLE
static int parameter_table[] = {
	0, // AccNull
	0, // AccTrue
	0, // AccFalse
	0, // AccThis
	1, // AccInt
	1, // AccStack
	1, // AccGlobal
	1, // AccEnv
	1, // AccField
	0, // AccArray
	1, // AccBuiltin
	1, // SetStack
	1, // SetGlobal
	1, // SetEnv
	1, // SetField
	0, // SetArray
	0, // Push
	1, // Pop
	1, // Call
	1, // ObjCall
	1, // Jump
	1, // JumpIf
	1, // JumpIfNot
	1, // Trap
	0, // EndTrap
	1, // Ret
	1, // MakeEnv
	0, // Bool
	0, // Add
	0, // Sub
	0, // Mult
	0, // Div
	0, // Mod
	0, // Shl
	0, // Shr
	0, // UShr
	0, // Or
	0, // And
	0, // Xor
	0, // Eq
	0, // Neq
	0, // Gt
	0, // Gte
	0, // Lt
	0, // Lte
};
#endif

/* ************************************************************************ */
