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
	Eq,
	Neq,
	Gt,
	Gte,
	Lt,
	Lte,
};

/* ************************************************************************ */
