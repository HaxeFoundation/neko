/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */
#ifndef _NEKO_OPCODES_H
#define _NEKO_OPCODES_H

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
	AccIndex,
	AccBuiltin,
	SetStack,
	SetGlobal,
	SetEnv,
	SetField,
	SetArray,
	SetIndex,
	SetThis,
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
	MakeArray,
	Bool,
	IsNull,
	IsNotNull,
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
	Not,
	TypeOf,
	Compare,
	Hash,
	New,

	Last,
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
	1, // AccIndex
	1, // AccBuiltin
	1, // SetStack
	1, // SetGlobal
	1, // SetEnv
	1, // SetField
	0, // SetArray
	1, // SetIndex
	0, // SetThis
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
	1, // MakeArray
	0, // Bool
	0, // IsNull
	0, // IsNotNull
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
	0, // Not
	0, // TypeOf
	0, // Compare
	0, // Hash
	0, // New
};

#define P	0xFF

static int stack_table[] = {
	0, // AccNull
	0, // AccTrue
	0, // AccFalse
	0, // AccThis
	0, // AccInt
	0, // AccStack
	0, // AccGlobal
	0, // AccEnv
	0, // AccField
	-1, // AccArray
	0, // AccIndex
	0, // AccBuiltin
	0, // SetStack
	0, // SetGlobal
	0, // SetEnv
	-1, // SetField
	-2, // SetArray
	-1, // SetIndex
	0, // SetThis
	1, // Push
	-P, // Pop
	-P, // Call
	-P, // ObjCall
	0, // Jump
	0, // JumpIf
	0, // JumpIfNot
	5, // Trap
	-5, // EndTrap
	0, // Ret
	-P, // MakeEnv
	-P, // MakeArray
	0, // Bool
	0, // IsNull
	0, // IsNotNull
	-1, // Add
	-1, // Sub
	-1, // Mult
	-1, // Div
	-1, // Mod
	-1, // Shl
	-1, // Shr
	-1, // UShr
	-1, // Or
	-1, // And
	-1, // Xor
	-1, // Eq
	-1, // Neq
	-1, // Gt
	-1, // Gte
	-1, // Lt
	-1, // Lte
	0, // Not
	0, // TypeOf
	-1, // Compare
	0, // Hash
	0, // New

	0, // Last
};
#endif

#endif
/* ************************************************************************ */
