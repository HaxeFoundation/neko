(*
 *  Neko Compiler
 *  Copyright (c)2005-2017 Haxe Foundation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *)

type opcode =
	(* getters *)
	| AccNull
	| AccTrue
	| AccFalse
	| AccThis
	| AccInt of int
	| AccStack of int
	| AccGlobal of int
	| AccEnv of int
	| AccField of string
	| AccArray
	| AccIndex of int
	| AccBuiltin of string
	(* setters *)
	| SetStack of int
	| SetGlobal of int
	| SetEnv of int
	| SetField of string
	| SetArray
	| SetIndex of int
	| SetThis
	(* stack ops *)
	| Push
	| Pop of int
	| Call of int
	| ObjCall of int
	| Jump of int
	| JumpIf of int
	| JumpIfNot of int
	| Trap of int
	| EndTrap
	| Ret of int
	| MakeEnv of int
	| MakeArray of int
	(* value ops *)
	| Bool
	| IsNull
	| IsNotNull
	| Add
	| Sub
	| Mult
	| Div
	| Mod
	| Shl
	| Shr
	| UShr
	| Or
	| And
	| Xor
	| Eq
	| Neq
	| Gt
	| Gte
	| Lt
	| Lte
	| Not
	| TypeOf
	| Compare
	| Hash
	| New

type global =
	| GlobalVar of string
	| GlobalFunction of int * int
	| GlobalString of string
	| GlobalFloat of string

exception Invalid_file

let trap_stack_delta = 5
let max_call_args = 5

let hash_field s =
	let acc = ref 0 in
	for i = 0 to String.length s - 1 do
		acc := 223 * !acc + Char.code (String.unsafe_get s i)
	done;
	acc := !acc land ((1 lsl 31) - 1);
	!acc	

let op_param = function
	| AccInt _
	| AccStack _
	| AccGlobal _
	| AccEnv _
	| AccField _
	| AccBuiltin _
	| SetStack _
	| SetGlobal _
	| SetEnv _
	| SetField _
	| Pop _
	| Call _
	| ObjCall _
	| Jump _
	| JumpIf _
	| JumpIfNot _
	| Trap _
	| MakeEnv _ 
	| MakeArray _
	| Ret _
	| AccIndex _
	| SetIndex _
		-> true
	| AccNull
	| AccTrue
	| AccFalse
	| AccThis
	| AccArray
	| SetArray
	| SetThis
	| Push
	| EndTrap
	| Bool
	| Add
	| Sub
	| Mult
	| Div
	| Mod
	| Shl
	| Shr
	| UShr
	| Or
	| And
	| Xor
	| Eq
	| Neq
	| Gt
	| Gte
	| Lt
	| Lte
	| IsNull
	| IsNotNull
	| Not
	| TypeOf
	| Compare
	| Hash
	| New
		-> false

let code_tables ops =
	let ids = Hashtbl.create 0 in
	Array.iter (function
		| AccField s
		| SetField s
		| AccBuiltin s ->
			let id = hash_field s in
			(try
				let f = Hashtbl.find ids id in
				if f <> s then failwith ("Field hashing conflict " ^ s ^ " and " ^ f);
			with
				Not_found ->	
					Hashtbl.add ids id s)
		| _ -> ()
	) ops;
	let p = ref 0 in
	let pos = Array.create (Array.length ops + 1) 0 in
	Array.iteri (fun i op ->
		Array.unsafe_set pos i !p;
		p := !p + (if op_param op then 2 else 1);
	) ops;
	Array.unsafe_set pos (Array.length ops) !p;
	ids , pos , !p

let write ch (globals,ops) =
	IO.nwrite ch "NEKO";
	let globals = DynArray.of_array globals in
	let ids , pos , csize = code_tables ops in
	IO.write_i32 ch (DynArray.length globals);
	IO.write_i32 ch (Hashtbl.length ids);
	IO.write_i32 ch csize;
	DynArray.iter (function
		| GlobalVar s -> IO.write_byte ch 1; IO.nwrite ch s; IO.write ch '\000';
		| GlobalFunction (p,nargs) -> IO.write_byte ch 2; IO.write_i32 ch (pos.(p) lor (nargs lsl 24))
		| GlobalString s -> IO.write_byte ch 3; IO.write_ui16 ch (String.length s); IO.nwrite ch s
		| GlobalFloat s -> IO.write_byte ch 4; IO.nwrite ch s; IO.write ch '\000'
	) globals;
	Hashtbl.iter (fun _ s ->
		IO.nwrite ch s;
		IO.write ch '\000';
	) ids;
	Array.iteri (fun i op ->
		let pop = ref None in
		let opid = (match op with
			| AccNull -> 0
			| AccTrue -> 1
			| AccFalse -> 2
			| AccThis -> 3
			| AccInt n -> pop := Some n; 4
			| AccStack n -> pop := Some n; 5
			| AccGlobal n -> pop := Some n; 6
			| AccEnv n -> pop := Some n; 7
			| AccField s -> pop := Some (hash_field s); 8
			| AccArray -> 9
			| AccIndex n -> pop := Some n; 10
			| AccBuiltin s -> pop := Some (hash_field s); 11
			| SetStack n -> pop := Some n; 12
			| SetGlobal n -> pop := Some n; 13
			| SetEnv n -> pop := Some n; 14
			| SetField s -> pop := Some (hash_field s); 15
			| SetArray -> 16
			| SetIndex n -> pop := Some n; 17
			| SetThis -> 18
			| Push -> 19
			| Pop n -> pop := Some n; 20
			| Call n -> pop := Some n; 21
			| ObjCall n -> pop := Some n; 22
			| Jump n -> pop := Some (pos.(i+n) - pos.(i)); 23
			| JumpIf n -> pop := Some (pos.(i+n) - pos.(i)); 24
			| JumpIfNot n -> pop := Some (pos.(i+n) - pos.(i)); 25
			| Trap n -> pop := Some (pos.(i+n) - pos.(i)); 26
			| EndTrap -> 27
			| Ret n -> pop := Some n; 28
			| MakeEnv n -> pop := Some n; 29
			| MakeArray n -> pop := Some n; 30
			| Bool -> 31
			| IsNull -> 32
			| IsNotNull -> 33
			| Add -> 34
			| Sub -> 35
			| Mult -> 36
			| Div -> 37
			| Mod -> 38
			| Shl -> 39
			| Shr -> 40
			| UShr -> 41
			| Or -> 42
			| And -> 43
			| Xor -> 44
			| Eq -> 45
			| Neq -> 46
			| Gt -> 47
			| Gte -> 48
			| Lt -> 49
			| Lte -> 50
			| Not -> 51
			| TypeOf -> 52
			| Compare -> 53
			| Hash -> 54
			| New -> 55
		) in
		match !pop with
		| None -> IO.write_byte ch (opid lsl 2)
		| Some n when opid < 32 && (n = 0 || n = 1) -> IO.write_byte ch ((opid lsl 3) lor (n lsl 2) lor 1)
		| Some n when n >= 0 && n <= 0xFF -> IO.write_byte ch ((opid lsl 2) lor 2); IO.write_byte ch n
		| Some n -> IO.write_byte ch ((opid lsl 2) lor 3); IO.write_i32 ch n
	) ops

let read_string ch =
	let b = Buffer.create 5 in
	let rec loop() =
		let c = IO.read ch in
		if c = '\000' then
			Buffer.contents b
		else begin
			Buffer.add_char b c;
			loop()
		end;
	in
	loop()

let read ch =
	try
		let head = IO.nread ch 4 in
		if head <> "NEKO" then raise Invalid_file;
		let nglobals = IO.read_i32 ch in
		let nids = IO.read_i32 ch in
		let csize = IO.read_i32 ch in
		if nglobals < 0 || nglobals > 0xFFFF || nids < 0 || nids > 0xFFFF || csize < 0 || csize > 0xFFFFFF then raise Invalid_file;
		let globals = Array.init nglobals (fun _ ->
			match IO.read_byte ch with
			| 1 -> GlobalVar (read_string ch)
			| 2 -> let v = IO.read_i32 ch in GlobalFunction (v land 0xFFFFFF, v lsr 24)
			| 3 -> let len = IO.read_ui16 ch in GlobalString (IO.nread ch len)
			| 4 -> GlobalFloat (read_string ch)
			| _ -> raise Invalid_file
		) in
		let ids = Hashtbl.create 0 in
		let rec loop n = 
			if n = 0 then
				()
			else
				let s = read_string ch in
				let id = hash_field s in
				try
					let s2 = Hashtbl.find ids id in
					if s <> s2 then raise Invalid_file;
				with
					Not_found ->
						Hashtbl.add ids id s;
						loop (n-1)
		in
		loop nids;
		let pos = Array.create (csize+1) (-1) in
		let cpos = ref 0 in
		let jumps = ref [] in
		let ops = DynArray.create() in
		while !cpos < csize do
			let code = IO.read_byte ch in
			let op , p = (match code land 3 with
				| 0 -> code lsr 2 , 0
				| 1 -> code lsr 3 , ((code lsr 2) land 1)
				| 2 -> code lsr 2 , IO.read_byte ch
				| 3 -> code lsr 2 , IO.read_i32 ch
				| _ -> assert false
			) in
			let op = (match op with
				| 0 -> AccNull
				| 1 -> AccTrue
				| 2 -> AccFalse
				| 3 -> AccThis
				| 4 -> AccInt p
				| 5 -> AccStack p
				| 6 -> AccGlobal p
				| 7 -> AccEnv p
				| 8 -> AccField (try Hashtbl.find ids p with Not_found -> raise Invalid_file)
				| 9 -> AccArray
				| 10 -> AccIndex p
				| 11 -> AccBuiltin (try Hashtbl.find ids p with Not_found -> raise Invalid_file)
				| 12 -> SetStack p
				| 13 -> SetGlobal p
				| 14 -> SetEnv p
				| 15 -> SetField (try Hashtbl.find ids p with Not_found -> raise Invalid_file)
				| 16 -> SetArray
				| 17 -> SetIndex p
				| 18 -> SetThis
				| 19 -> Push
				| 20 -> Pop p
				| 21 -> Call p
				| 22 -> ObjCall p
				| 23 -> jumps := (!cpos , DynArray.length ops) :: !jumps; Jump p
				| 24 -> jumps := (!cpos , DynArray.length ops) :: !jumps; JumpIf p
				| 25 -> jumps := (!cpos , DynArray.length ops) :: !jumps; JumpIfNot p
				| 26 -> jumps := (!cpos , DynArray.length ops) :: !jumps; Trap p
				| 27 -> EndTrap
				| 28 -> Ret p
				| 29 -> MakeEnv p
				| 30 -> MakeArray p
				| 31 -> Bool
				| 32 -> IsNull
				| 33 -> IsNotNull
				| 34 -> Add
				| 35 -> Sub
				| 36 -> Mult
				| 37 -> Div
				| 38 -> Mod
				| 39 -> Shl
				| 40 -> Shr
				| 41 -> UShr
				| 42 -> Or
				| 43 -> And
				| 44 -> Xor
				| 45 -> Eq
				| 46 -> Neq
				| 47 -> Gt
				| 48 -> Gte
				| 49 -> Lt
				| 50 -> Lte
				| 51 -> Not
				| 52 -> TypeOf
				| 53 -> Compare
				| 54 -> Hash
				| 55 -> New
				| _ -> raise Invalid_file
			) in
			pos.(!cpos) <- DynArray.length ops;
			cpos := !cpos + (if op_param op then 2 else 1);
			DynArray.add ops op;
		done;
		if !cpos <> csize then raise Invalid_file;
		pos.(!cpos) <- DynArray.length ops;
		let pos_index i sadr =
			let idx = pos.(sadr) in
			if idx = -1 then raise Invalid_file;
			idx - i
		in
		List.iter (fun (a,i) ->
			DynArray.set ops i (match DynArray.get ops i with
			| Jump p -> Jump (pos_index i (a+p))
			| JumpIf p -> JumpIf (pos_index i (a+p))
			| JumpIfNot p -> JumpIfNot (pos_index i (a+p))
			| Trap p -> Trap (pos_index i (a+p))
			| _ -> assert false)			
		) !jumps;
		Array.iteri (fun i g ->
			match g with
			| GlobalFunction (f,n) -> globals.(i) <- GlobalFunction (pos_index 0 f,n)
			| _ -> ()
		) globals;
		globals , DynArray.to_array ops
	with
		| IO.No_more_input
		| IO.Overflow _ -> raise Invalid_file

let escape str =
	String.escaped str

let dump ch (globals,ops) =
	let ids, pos , csize = code_tables ops in
	IO.printf ch "nglobals : %d\n" (Array.length globals);
	IO.printf ch "nfields : %d\n" (Hashtbl.length ids);
	IO.printf ch "codesize : %d ops , %d total\n" (Array.length ops) csize;
	IO.printf ch "GLOBALS =\n";
	let marks = Array.create csize false in
	Array.iteri (fun i g ->
		IO.printf ch "  global %d : %s\n" i 
			(match g with
			| GlobalVar s -> "var " ^ s
			| GlobalFunction (p,n) -> 
				if p >= 0 && p < csize then marks.(p) <- true;
				"function " ^ string_of_int p ^ " nargs " ^ string_of_int n
			| GlobalString s -> "string \"" ^ escape s ^ "\""
			| GlobalFloat s -> "float " ^ s)
	) globals;
	IO.printf ch "FIELDS =\n";
	Hashtbl.iter (fun h f ->
		IO.printf ch "  %s%s%.8X\n" f (if String.length f >= 24 then " " else String.make (24 - String.length f) ' ') h;
	) ids;
	IO.printf ch "CODE =\n";
	let str s i = s ^ " " ^ string_of_int i in
	let bpos = ref 0 in
	Array.iteri (fun pos op ->
		if marks.(pos) then IO.write ch '\n';
		IO.printf ch "%.6X %6d    %s\n" (!bpos) pos (match op with
			| AccNull -> "AccNull"
			| AccTrue -> "AccTrue"
			| AccFalse -> "AccFalse"
			| AccThis -> "AccThis"
			| AccInt i -> str "AccInt" i
			| AccStack i -> str "AccStack" i
			| AccGlobal i -> str "AccGlobal" i
			| AccEnv i -> str "AccEnv" i
			| AccField s -> "AccField " ^ s
			| AccArray -> "AccArray"
			| AccIndex i -> str "AccIndex" i
			| AccBuiltin s -> "AccBuiltin " ^ s
			| SetStack i -> str "SetStack" i
			| SetGlobal i -> str "SetGlobal" i
			| SetEnv i -> str "SetEnv" i
			| SetField f -> "SetField " ^ f
			| SetArray -> "SetArray"
			| SetIndex i -> str "SetIndex" i
			| SetThis -> "SetThis"
			| Push -> "Push"
			| Pop i -> str "Pop" i
			| Call i -> str "Call" i
			| ObjCall i -> str "ObjCall" i
			| Jump i -> str "Jump" (pos + i)
			| JumpIf i -> str "JumpIf" (pos + i)
			| JumpIfNot i -> str "JumpIfNot" (pos + i)
			| Trap i -> str "Trap" (pos + i)
			| EndTrap -> "EndTrap"
			| Ret i -> str "Ret" i
			| MakeEnv i -> str "MakeEnv" i
			| MakeArray i -> str "MakeArray" i
			| Bool -> "Bool"
			| IsNull -> "IsNull"
			| IsNotNull -> "IsNotNull"
			| Add -> "Add"
			| Sub -> "Sub" 
			| Mult -> "Mult"
			| Div -> "Div"
			| Mod -> "Mod"
			| Shl -> "Shl"
			| Shr -> "Shr"
			| UShr -> "UShr"
			| Or -> "Or"
			| And -> "And"
			| Xor -> "Xor"
			| Eq -> "Eq"
			| Neq -> "Neq"
			| Gt -> "Gt"
			| Gte -> "Gte"
			| Lt -> "Lt"
			| Lte -> "Lte"
			| Not -> "Not"
			| TypeOf -> "TypeOf"
			| Compare -> "Compare"
			| Hash -> "Hash"
			| New -> "New"
		);
		bpos := !bpos + if op_param op then 2 else 1;
	) ops;
	IO.printf ch "END\n"

