/*
 * Copyright (C)2005-2017 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _NEKO_OPCODES_H
#define _NEKO_OPCODES_H

#ifndef OP
#	define OP(x)	x
#	define OPBEGIN	enum OPCODE {
#	define OPEND	};
#endif
OPBEGIN
	OP(AccNull),
	OP(AccTrue),
	OP(AccFalse),
	OP(AccThis),
	OP(AccInt),
	OP(AccStack),
	OP(AccGlobal),
	OP(AccEnv),
	OP(AccField),
	OP(AccArray),
	OP(AccIndex),
	OP(AccBuiltin),
	OP(SetStack),
	OP(SetGlobal),
	OP(SetEnv),
	OP(SetField),
	OP(SetArray),
	OP(SetIndex),
	OP(SetThis),
	OP(Push),
	OP(Pop),
	OP(Call),
	OP(ObjCall),
	OP(Jump),
	OP(JumpIf),
	OP(JumpIfNot),
	OP(Trap),
	OP(EndTrap),
	OP(Ret),
	OP(MakeEnv),
	OP(MakeArray),
	OP(Bool),
	OP(IsNull),
	OP(IsNotNull),
	OP(Add),
	OP(Sub),
	OP(Mult),
	OP(Div),
	OP(Mod),
	OP(Shl),
	OP(Shr),
	OP(UShr),
	OP(Or),
	OP(And),
	OP(Xor),
	OP(Eq),
	OP(Neq),
	OP(Gt),
	OP(Gte),
	OP(Lt),
	OP(Lte),
	OP(Not),
	OP(TypeOf),
	OP(Compare),
	OP(Hash),
	OP(New),
	OP(JumpTable),
	OP(Apply),
	OP(AccStack0),
	OP(AccStack1),
	OP(AccIndex0),
	OP(AccIndex1),
	OP(PhysCompare),
	OP(TailCall),
	OP(Loop),

	OP(MakeArray2),
	OP(AccInt32),
	OP(Last),
OPEND

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
	1, // JumpTable
	1, // Apply
	0, // AccStack0
	0, // AccStack1
	0, // AccIndex0
	0, // AccIndex1
	0, // PhysCompare
	1, // TailCall
	0, // Loop
	1, // MakeArray2
	1, // AccInt32
};
#endif

#ifdef STACK_TABLE
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
	6, // Trap
	-6, // EndTrap
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
	0, // JumpTable
	-P, // Apply
	0, // AccStack0
	0, // AccStack1
	0, // AccIndex0
	0, // AccIndex1
	-1, // PhysCompare
	0, // TailCall
	0, // Loop
	-P, // MakeArray2
	0, // AccInt32
	0, // Last
};
#endif

#endif
/* ************************************************************************ */
