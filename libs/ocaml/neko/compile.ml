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
 
open Ast
open Bytecode

type label = {
	lname : string;
	ltraps : int;
	lstack : int;
	mutable lpos : int option;
	mutable lwait : (unit -> unit) list;
}

type context = {
	mutable ops : opcode DynArray.t;
	mutable locals : (string,int) PMap.t;
	globals : (global,int) Hashtbl.t;
	gobjects : (string list,int) Hashtbl.t;
	mutable env : (string,int) PMap.t;
	mutable nenv : int;
	mutable stack : int;
	mutable loop_limit : int;
	mutable limit : int;
	mutable ntraps : int;
	mutable breaks : ((unit -> unit) * pos) list;
	mutable continues : ((unit -> unit) * pos) list;
	mutable functions : (opcode DynArray.t * int * int) list;
	mutable gtable : global DynArray.t;
	labels : (string,label) Hashtbl.t;
}

type error_msg = 
	| Custom of string

exception Error of error_msg * pos

let error e p = raise (Error (e,p))

let error_msg = function
	| Custom s -> s

let stack_delta = function
	| AccNull
	| AccTrue
	| AccFalse
	| AccThis
	| AccInt _
	| AccStack _
	| AccGlobal _ 
	| AccEnv _
	| AccField _
	| AccBuiltin _
	| AccIndex _
	| JumpIf _
	| JumpIfNot _
	| Jump _ 
	| Ret _
	| SetGlobal _
	| SetStack _
	| SetEnv _
	| SetThis
	| Bool
	| EndTrap
	| IsNull
	| IsNotNull
	| Not
	| Hash
	| TypeOf
	| New
		-> 0
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
		-> -1
	| AccArray -> -1
	| SetField _ | SetIndex _ | Compare -> -1
	| SetArray -> -2
	| Push -> 1
	| Pop x -> -x
	| Call nargs -> -nargs
	| ObjCall nargs -> -(nargs + 1)
	| MakeEnv size | MakeArray size -> -size
	| Trap _ -> trap_stack_delta

let pos ctx =
	DynArray.length ctx.ops

let write ctx op =
	ctx.stack <- ctx.stack + stack_delta op;
	DynArray.add ctx.ops op

let jmp ?cond ctx =
	let p = pos ctx in
	write ctx (Jump 0);
	(fun() ->
		DynArray.set ctx.ops p
			(match cond with
			| None -> Jump (pos ctx - p)
			| Some true -> JumpIf (pos ctx - p)
			| Some false -> JumpIfNot (pos ctx -p))
	)

let goto ctx p =
	write ctx (Jump (p - pos ctx))

let global ctx g =
	try
		Hashtbl.find ctx.globals g
	with
		Not_found ->
			let gid = DynArray.length ctx.gtable in
			Hashtbl.add ctx.globals g gid;
			DynArray.add ctx.gtable g;
			gid

let save_breaks ctx =
	let oldc = ctx.continues in
	let oldb = ctx.breaks in
	let oldl = ctx.loop_limit in
	ctx.loop_limit <- ctx.stack;
	ctx.breaks <- [];
	ctx.continues <- [];
	ctx , oldc, oldb , oldl

let process_continues (ctx,oldc,_,_) =
	List.iter (fun (f,_) -> f()) ctx.continues;
	ctx.continues <- oldc

let process_breaks (ctx,_,oldb,oldl) =
	List.iter (fun (f,_) -> f()) ctx.breaks;
	ctx.loop_limit <- oldl;
	ctx.breaks <- oldb

let check_breaks ctx =
	List.iter (fun (_,p) -> error (Custom "Break outside a loop") p) ctx.breaks;
	List.iter (fun (_,p) -> error (Custom "Continue outside a loop") p) ctx.continues

let rec scan_labels ctx supported e =
	match fst e with
	| EFunction (args,e) -> 
		let nargs = List.length args in
		let ntraps = ctx.ntraps in
		ctx.ntraps <- 0;
		ctx.stack <- ctx.stack + nargs;
		scan_labels ctx supported e;
		ctx.stack <- ctx.stack - nargs;
		ctx.ntraps <- ntraps
	| EBlock _ -> 
		let old = ctx.stack in
		Ast.iter (scan_labels ctx supported) e;
		ctx.stack <- old
	| EVars l ->
		List.iter (fun (_,e) ->
			(match e with
			| None -> ()
			| Some e -> scan_labels ctx supported e);
			ctx.stack <- ctx.stack + 1
		) l
	| ELabel l when not supported ->
		error (Custom "Label is not supported in this part of the program") (snd e);
	| ELabel l when Hashtbl.mem ctx.labels l ->
		error (Custom ("Duplicate label " ^ l)) (snd e)
	| ELabel l ->
		Hashtbl.add ctx.labels l {
			lname = l;
			ltraps = ctx.ntraps;
			lstack = ctx.stack;
			lpos = None;
			lwait = [];
		}
	| ETry (e,_,e2) ->
		ctx.stack <- ctx.stack + trap_stack_delta;
		ctx.ntraps <- ctx.ntraps + 1;
		scan_labels ctx supported e;
		ctx.stack <- ctx.stack - trap_stack_delta;
		ctx.ntraps <- ctx.ntraps - 1;
		ctx.stack <- ctx.stack + 1;
		scan_labels ctx supported e2;
		ctx.stack <- ctx.stack - 1;
	| EBinop ("=",e1,e2) ->
		let rec is_extended (e,_) =
			match e with
			| EParenthesis e -> is_extended e
			| EArray _
			| EField _ ->
				true
			| _ ->
				false
		in
		let ext = is_extended e1 in
		if ext then ctx.stack <- ctx.stack + 1;
		scan_labels ctx supported e2;
		ctx.stack <- ctx.stack + 1;
		scan_labels ctx supported e1;
		ctx.stack <- ctx.stack - (if ext then 2 else 1);
	| ECall ((EConst (Builtin x),_),el) when x <> "array" && x <> "apply" ->
		Ast.iter (scan_labels ctx false) e
	| ECall (_,el) ->
		List.iter (fun e ->
			scan_labels ctx supported e;
			ctx.stack <- ctx.stack + 1;
		) el;
		ctx.stack <- ctx.stack - List.length el
	| EObject fl ->
		ctx.stack <- ctx.stack + 2;
		List.iter (fun (s,e) ->
			scan_labels ctx supported e
		) fl;
		ctx.stack <- ctx.stack - 2;	
	| EConst _
	| EContinue 
	| EBreak _
	| EReturn _ 
	| EIf _
	| EWhile _ 
	| EParenthesis _
	| ENext _ ->
		Ast.iter (scan_labels ctx supported) e
	| EBinop _
	| EArray _
	| EField _ ->
		Ast.iter (scan_labels ctx false) e

let compile_constant ctx c p =
	match c with
	| True -> write ctx AccTrue
	| False -> write ctx AccFalse
	| Null -> write ctx AccNull
	| This -> write ctx AccThis
	| Int n -> write ctx (AccInt n)
	| Float f -> write ctx (AccGlobal (global ctx (GlobalFloat f)))
	| String s -> write ctx (AccGlobal (global ctx (GlobalString s)))
	| Builtin s -> 
		(match s with
		| "tnull" -> write ctx (AccInt 0)
		| "tint" -> write ctx (AccInt 1)
		| "tfloat" -> write ctx (AccInt 2)
		| "tbool" -> write ctx (AccInt 3)
		| "tstring" -> write ctx (AccInt 4)
		| "tobject" -> write ctx (AccInt 5)
		| "tarray" -> write ctx (AccInt 6)
		| "tfunction" -> write ctx (AccInt 7)
		| "tabstract" -> write ctx (AccInt 8)
		| s ->
			write ctx (AccBuiltin s))
	| Ident s ->
		try
			let e = PMap.find s ctx.env in
			write ctx (AccEnv e);
		with Not_found -> try
			let l = PMap.find s ctx.locals in
			if l <= ctx.limit then begin
				let e = ctx.nenv in
				ctx.nenv <- ctx.nenv + 1;
				ctx.env <- PMap.add s e ctx.env;
				write ctx (AccEnv e);
			end else
				write ctx (AccStack (ctx.stack - l));
		with Not_found ->
			let g = global ctx (GlobalVar s) in
			write ctx (AccGlobal g)

let rec compile_binop ctx op e1 e2 p =
	match op with
	| "=" ->		
		(match fst e1 with
		| EConst (Ident s) ->
			compile ctx e2;
			(try
				let e = PMap.find s ctx.env in
				write ctx (SetEnv e);
			with Not_found -> try
				let l = PMap.find s ctx.locals in
				if l <= ctx.limit then begin
					let e = ctx.nenv in
					ctx.nenv <- ctx.nenv + 1;
					ctx.env <- PMap.add s e ctx.env;
					write ctx (SetEnv e);
				end else
					write ctx (SetStack (ctx.stack - l))
			with
				Not_found ->
					let g = global ctx (GlobalVar s) in
					write ctx (SetGlobal g))
		| EField (e,f) ->
			compile ctx e;
			write ctx Push;
			compile ctx e2;
			write ctx (SetField f)
		| EArray (e1,(EConst (Int n),_)) ->
			compile ctx e1;
			write ctx Push;
			compile ctx e2;
			write ctx (SetIndex n)
		| EArray (ea,ei) ->
			compile ctx ei;
			write ctx Push;
			compile ctx ea;
			write ctx Push;
			compile ctx e2;
			write ctx SetArray
		| EConst This ->
			compile ctx e2;
			write ctx SetThis
		| _ ->
			error (Custom "Invalid assign") p)
	| "&&" ->
		compile ctx e1;
		let jnext = jmp ~cond:false ctx in
		compile ctx e2;
		jnext()
	| "||" ->
		compile ctx e1;
		let jnext = jmp ~cond:true ctx in
		compile ctx e2;
		jnext()
	| _ ->
		match op , e1 , e2 with
		| "==" , _ , (EConst Null,_) ->
			compile ctx e1;
			write ctx IsNull
		| "!=" , _ , (EConst Null,_) ->
			compile ctx e1;
			write ctx IsNotNull
		| "==" , (EConst Null,_) , _ ->
			compile ctx e2;
			write ctx IsNull
		| "!=" , (EConst Null,_) , _ ->
			compile ctx e2;
			write ctx IsNotNull
		| _ ->
			compile ctx e1;
			write ctx Push;
			compile ctx e2;
			match op with
			| "+" -> write ctx Add
			| "-" -> write ctx Sub
			| "/" -> write ctx Div
			| "*" -> write ctx Mult
			| "%" -> write ctx Mod
			| "<<" -> write ctx Shl
			| ">>" -> write ctx Shr
			| ">>>" -> write ctx UShr
			| "|" -> write ctx Or
			| "&" -> write ctx And
			| "^" -> write ctx Xor
			| "==" -> write ctx Eq
			| "!=" -> write ctx Neq
			| ">" -> write ctx Gt
			| ">=" -> write ctx Gte
			| "<" -> write ctx Lt
			| "<=" -> write ctx Lte
			| _ -> error (Custom "Unknown operation") p

and compile_function ctx params e =
	let limit = ctx.limit in
	let ops = ctx.ops in
	let breaks = ctx.breaks in
	let continues = ctx.continues in
	let locals = ctx.locals in
	let env = ctx.env in
	let nenv = ctx.nenv in
	let ntraps = ctx.ntraps in
	ctx.ops <- DynArray.create();
	ctx.breaks <- [];
	ctx.continues <- [];
	ctx.env <- PMap.empty;
	ctx.nenv <- 0;
	ctx.ntraps <- 0;
	ctx.limit <- ctx.stack;
	List.iter (fun v ->
		ctx.stack <- ctx.stack + 1;
		ctx.locals <- PMap.add v ctx.stack ctx.locals;
	) params;
	let s = ctx.stack in
	compile ctx e;
	write ctx (Ret (ctx.stack - ctx.limit));
	assert( ctx.stack = s );
	check_breaks ctx;
	ctx.stack <- ctx.limit;
	ctx.limit <- limit;
	ctx.breaks <- breaks;
	ctx.continues <- continues;
	ctx.locals <- locals;
	let gid = DynArray.length ctx.gtable in
	ctx.functions <- (ctx.ops,gid,List.length params) :: ctx.functions;
	DynArray.add ctx.gtable (GlobalFunction (gid,-1));
	ctx.ops <- ops;
	let local_env = ctx.env in
	let local_nenv = ctx.nenv in
	ctx.env <- env;
	ctx.ntraps <- ntraps;
	ctx.nenv <- nenv;
	if local_nenv > 0 then begin
		let a = Array.create local_nenv "" in
		PMap.iter (fun v i -> a.(i) <- v) local_env;
		Array.iter (fun v ->
			compile_constant ctx (Ident v) null_pos;
			write ctx Push;
		) a;
		write ctx (AccGlobal gid);
		write ctx (MakeEnv local_nenv);
	end else
		write ctx (AccGlobal gid);

and compile_builtin ctx b el p =
	match b , el with
	| "istrue" , [e] ->
		compile ctx e;
		write ctx Bool
	| "not" , [e] ->
		compile ctx e;
		write ctx Not
	| "typeof" , [e] ->
		compile ctx e;
		write ctx TypeOf
	| "hash" , [e] ->
		compile ctx e;
		write ctx Hash
	| "compare" , [e1;e2] ->
		compile ctx e1;
		write ctx Push;
		compile ctx e2;
		write ctx Compare
	| "goto" , [ EConst (Ident l) , _ ] ->
		let l = (try Hashtbl.find ctx.labels l with Not_found -> error (Custom ("Unknown label " ^ l)) p) in
		let os = ctx.stack in
		let ntraps = ref ctx.ntraps in
		let etraps = ref [] in
		while !ntraps > l.ltraps do
			write ctx EndTrap;
			ctx.stack <- ctx.stack - trap_stack_delta;
			ntraps := !ntraps - 1;
		done;
		while !ntraps < l.ltraps do
			etraps := (pos ctx) :: !etraps;
			write ctx (Trap 0);
			ntraps := !ntraps + 1;
		done;
		if ctx.stack > l.lstack then write ctx (Pop (ctx.stack - l.lstack));
		while ctx.stack < l.lstack do
			write ctx Push;
		done;		
		ctx.stack <- os;
		(match l.lpos with
		| None -> l.lwait <- jmp ctx :: l.lwait
		| Some p -> write ctx (Jump p));
		if !etraps <> [] then begin
			List.iter (fun p ->
				DynArray.set ctx.ops p (Trap (pos ctx - p));
			) !etraps;
			write ctx Push;
			compile_constant ctx (Builtin "throw") p;
			write ctx (Call 1);
		end;
	| "goto" , _ ->
		error (Custom "Invalid $goto statement") p
	| _ ->
		List.iter (fun e ->
			compile ctx e;
			write ctx Push;
		) el;
		compile_constant ctx (Builtin b) p;
		write ctx (Call (List.length el))

and compile ctx (e,p) =
	match e with
	| EConst c ->
		compile_constant ctx c p
	| EBlock [] ->
		write ctx AccNull
	| EBlock el ->
		let locals = ctx.locals in
		let stack = ctx.stack in
		List.iter (compile ctx) el;
		if stack < ctx.stack then write ctx (Pop (ctx.stack - stack));
		assert( stack = ctx.stack );
		ctx.locals <- locals
	| EParenthesis e ->
		compile ctx e
	| EField (e,f) ->
		compile ctx e;
		write ctx (AccField f)
	| ECall ((EConst (Builtin "array"),_),el) ->
		List.iter (fun e ->
			compile ctx e;
			write ctx Push;
		) el;
		write ctx (MakeArray (List.length el));
	| ECall (_,el) when List.length el > max_call_args ->
		error (Custom "Too many arguments") p
	| ECall ((EField (e,f),_),el) ->
		List.iter (fun e ->
			compile ctx e;
			write ctx Push;
		) el;
		compile ctx e;
		write ctx Push;
		write ctx (AccField f);
		write ctx (ObjCall (List.length el))
	| ECall ((EConst (Builtin b),_),el) ->
		compile_builtin ctx b el p
	| ECall (e,el) ->
		List.iter (fun e ->
			compile ctx e;
			write ctx Push;
		) el;
		compile ctx e;
		write ctx (Call (List.length el))
	| EArray (e1,(EConst (Int n),_)) ->
		compile ctx e1;
		write ctx (AccIndex n)
	| EArray (e1,e2) ->
		compile ctx e1;
		write ctx Push;
		compile ctx e2;
		write ctx AccArray
	| EVars vl ->
		List.iter (fun (v,o) ->
			(match o with
			| None -> write ctx AccNull
			| Some e -> compile ctx e);
			write ctx Push;
			ctx.locals <- PMap.add v ctx.stack ctx.locals;
		) vl
	| EWhile (econd,e,NormalWhile) ->
		let start = pos ctx in
		compile ctx econd;
		let jend = jmp ~cond:false ctx in
		let save = save_breaks ctx in
		compile ctx e;
		process_continues save;
		goto ctx start;
		process_breaks save;
		jend();
	| EWhile (econd,e,DoWhile) ->
		let start = pos ctx in
		let save = save_breaks ctx in
		compile ctx e;
		process_continues save;
		compile ctx econd;
		write ctx (JumpIf (start - pos ctx));
		process_breaks save
	| EIf (e,e1,e2) ->
		let stack = ctx.stack in
		compile ctx e;
		let jelse = jmp ~cond:false ctx in
		compile ctx e1;
		assert( stack = ctx.stack );
		(match e2 with
		| None ->
			jelse()
		| Some e2 ->
			let jend = jmp ctx in
			jelse();
			compile ctx e2;
			assert( stack = ctx.stack );
			jend());
	| ETry (e,v,ecatch) ->
		let start = pos ctx in
		write ctx (Trap 0);
		ctx.ntraps <- ctx.ntraps + 1;
		compile ctx e;
		ctx.ntraps <- ctx.ntraps - 1;
		ctx.stack <- ctx.stack - trap_stack_delta;
		write ctx EndTrap;
		let jend = jmp ctx in
		DynArray.set ctx.ops start (Trap (pos ctx - start));
		write ctx Push;
		let locals = ctx.locals in
		ctx.locals <- PMap.add v ctx.stack ctx.locals;
		compile ctx ecatch;
		write ctx (Pop 1);
		ctx.locals <- locals;
		jend()
	| EBinop ("-",(EConst (Int 0),_),(EConst (Int i),_)) ->
		compile ctx (EConst (Int (-i)),p)
	| EBinop (op,e1,e2) -> 
		compile_binop ctx op e1 e2 p
	| EReturn None ->
		write ctx AccNull;
		for i = 1 to ctx.ntraps do
			write ctx EndTrap;
		done;
		write ctx (Ret (ctx.stack - ctx.limit));
	| EReturn (Some e) ->
		compile ctx e;
		for i = 1 to ctx.ntraps do
			write ctx EndTrap;
		done;
		write ctx (Ret (ctx.stack - ctx.limit - ctx.ntraps * trap_stack_delta));
	| EBreak e ->
		assert (ctx.ntraps = 0);
		(match e with
		| None -> ()
		| Some e -> compile ctx e);
		if ctx.loop_limit <> ctx.stack then DynArray.add ctx.ops (Pop (ctx.stack - ctx.loop_limit));
		ctx.breaks <- (jmp ctx , p) :: ctx.breaks
	| EContinue ->
		assert (ctx.ntraps = 0);
		if ctx.loop_limit <> ctx.stack then DynArray.add ctx.ops (Pop (ctx.stack - ctx.loop_limit));
		ctx.continues <- (jmp ctx , p) :: ctx.continues
	| EFunction (params,e) ->
		compile_function ctx params e
	| ENext (e1,e2) ->
		compile ctx e1;
		compile ctx e2
	| EObject [] ->
		write ctx AccNull;
		write ctx New
	| EObject fl ->
		let fields = List.sort compare (List.map fst fl) in
		let id = (try
			Hashtbl.find ctx.gobjects fields
		with Not_found ->
			let id = global ctx (GlobalVar ("o:" ^ string_of_int (Hashtbl.length ctx.gobjects))) in
			Hashtbl.add ctx.gobjects fields id;
			id
		) in
		write ctx (AccGlobal id);
		write ctx New;
		write ctx Push;
		List.iter (fun (f,e) ->
			write ctx Push;
			compile ctx e;
			write ctx (SetField f);
			write ctx (AccStack 0);
		) fl;
		write ctx (Pop 1)
	| ELabel l ->
		let l = (try Hashtbl.find ctx.labels l with Not_found -> assert false) in
		if ctx.stack <> l.lstack then assert false;
		if ctx.ntraps <> l.ltraps then assert false;
		List.iter (fun f -> f()) l.lwait;
		l.lwait <- [];
		l.lpos <- Some (pos ctx)

let compile file ast =
	let ctx = {
		stack = 0;
		loop_limit = 0;
		limit = -1;
		globals = Hashtbl.create 0;
		gobjects = Hashtbl.create 0;
		gtable = DynArray.create();
		locals = PMap.empty;
		ops = DynArray.create();
		breaks = [];
		continues = [];
		functions = [];
		env = PMap.empty;
		nenv = 0;
		ntraps = 0;
		labels = Hashtbl.create 0;
	} in
	scan_labels ctx true ast;
	compile ctx ast;
	check_breaks ctx;
	if ctx.functions <> [] || Hashtbl.length ctx.gobjects <> 0 then begin
		let ctxops = ctx.ops in
		let ops = DynArray.create() in
		ctx.ops <- ops;
		write ctx (Jump 0);
		List.iter (fun (fops,gid,nargs) ->
			DynArray.set ctx.gtable gid (GlobalFunction (DynArray.length ops,nargs));
			DynArray.append fops ops;
		) (List.rev ctx.functions);
		DynArray.set ops 0 (Jump (DynArray.length ops));
		Hashtbl.iter (fun fl g ->
			write ctx AccNull;
			write ctx New;
			write ctx (SetGlobal g);
			List.iter (fun f ->
				write ctx (AccGlobal g);
				write ctx Push;		
				write ctx (SetField f);
			) fl
		) ctx.gobjects;
		DynArray.append ctxops ops;
	end;
	DynArray.to_array ctx.gtable, DynArray.to_array ctx.ops

