open Ast
open Bytecode

type context = {
	mutable ops : opcode DynArray.t;
	mutable locals : (string,int) PMap.t;
	globals : (global,int) Hashtbl.t;
	mutable env : (string,int) PMap.t;
	mutable nenv : int;
	mutable stack : int;
	mutable loop_limit : int;
	mutable limit : int;
	mutable breaks : ((unit -> unit) * pos) list;
	mutable continues : ((unit -> unit) * pos) list;
	mutable functions : (opcode DynArray.t * int * int) list;
	mutable gtable : global DynArray.t;
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
	| JumpIf _
	| JumpIfNot _
	| Jump _ 
	| Ret _
	| SetGlobal _
	| SetStack _
	| SetEnv _
	| SetThis
	| Bool
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
	| SetField _ -> -1
	| SetArray -> -2
	| Push -> 1
	| Pop x -> -x
	| Call nargs -> -nargs
	| ObjCall nargs -> -(nargs + 1)
	| MakeEnv size | MakeArray size -> -size
	| Trap _ -> trap_stack_delta
	| EndTrap -> -trap_stack_delta

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

let compile_constant ctx c p =
	match c with
	| True -> write ctx AccTrue
	| False -> write ctx AccFalse
	| Null -> write ctx AccNull
	| This -> write ctx AccThis
	| Int n -> write ctx (AccInt n)
	| Float f -> write ctx (AccGlobal (global ctx (GlobalFloat f)))
	| String s -> write ctx (AccGlobal (global ctx (GlobalString s)))
	| Builtin s -> write ctx (AccBuiltin s)
	| Module s -> assert false
	| Macro _ -> error (Custom "Ast was not macro-expanded") p
	| Ident s ->
		try
			let e = PMap.find s ctx.env in
			write ctx (AccEnv e);
		with Not_found -> try
			let l = PMap.find s ctx.locals in
			if l < ctx.limit then begin
				let e = ctx.nenv in
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
		compile ctx e2;
		(match fst e1 with
		| EConst (Ident s) ->
			(try
				let e = PMap.find s ctx.env in
				write ctx (SetEnv e);
			with Not_found -> try
				let l = PMap.find s ctx.locals in
				if l < ctx.limit then begin
					let e = ctx.nenv in
					ctx.env <- PMap.add s e ctx.env;
					write ctx (SetEnv e);
				end else
					write ctx (SetStack (ctx.stack - l))
			with
				Not_found ->
					let g = global ctx (GlobalVar s) in
					write ctx (SetGlobal g))
		| EField (e,f) ->
			write ctx Push;
			compile ctx e;
			write ctx (SetField f)
		| EArray (e1,e2) ->
			write ctx Push;
			compile ctx e2;
			write ctx Push;
			compile ctx e1;
			write ctx SetArray
		| EConst This ->
			write ctx SetThis
		| _ ->
			error (Custom "Invalid assign") p)
	| "&&" ->
		compile ctx e1;
		write ctx Bool;
		let jnext = jmp ~cond:false ctx in
		compile ctx e2;
		write ctx Bool;
		jnext()
	| "||" ->
		compile ctx e1;
		write ctx Bool;
		let jnext = jmp ~cond:true ctx in
		compile ctx e2;
		write ctx Bool;
		jnext()
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
	ctx.ops <- DynArray.create();
	ctx.breaks <- [];
	ctx.continues <- [];
	ctx.env <- PMap.empty;
	ctx.nenv <- 0;
	ctx.limit <- ctx.stack;
	ctx.stack <- ctx.stack + 1;
	List.iter (fun v ->
		ctx.stack <- ctx.stack + 1;
		ctx.locals <- PMap.add v ctx.stack ctx.locals;
	) params;
	let s = ctx.stack in
	compile ctx e;
	write ctx (Ret (ctx.stack - ctx.limit - 1));
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
	if ctx.nenv > 0 then begin
		let a = Array.create ctx.nenv "" in
		PMap.iter (fun v i -> a.(i) <- v) ctx.env;
		Array.iter (fun v ->
			let l = (try PMap.find v ctx.locals with Not_found -> assert false) in
			write ctx (AccStack (pos ctx - l));
			write ctx Push;
		) a;
		write ctx (AccGlobal gid);
		write ctx (MakeEnv ctx.nenv);
	end else
		write ctx (AccGlobal gid);
	ctx.env <- env;
	ctx.nenv <- nenv

and compile_builtin ctx b el p =
	match b with
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
	| EFor (einit,econd,eincr,e) ->
		compile ctx einit;
		let start = pos ctx in
		compile ctx econd;
		let jend = jmp ~cond:false ctx in
		let save = save_breaks ctx in
		compile ctx e;
		process_continues save;
		compile ctx eincr;
		goto ctx start;
		process_breaks save;
		jend();
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
		compile ctx e;
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
	| EBinop (op,e1,e2) -> 
		compile_binop ctx op e1 e2 p
	| EReturn None ->
		write ctx AccNull;
		write ctx (Ret (ctx.stack - ctx.limit - 1));
	| EReturn (Some e) ->
		compile ctx e;
		write ctx (Ret (ctx.stack - ctx.limit - 1));
	| EBreak e ->
		(match e with
		| None -> ()
		| Some e -> compile ctx e);
		if ctx.loop_limit <> ctx.stack then DynArray.add ctx.ops (Pop (ctx.stack - ctx.loop_limit));
		ctx.breaks <- (jmp ctx , p) :: ctx.breaks
	| EContinue ->
		if ctx.loop_limit <> ctx.stack then DynArray.add ctx.ops (Pop (ctx.stack - ctx.loop_limit));
		ctx.continues <- (jmp ctx , p) :: ctx.continues
	| EFunction (params,e) ->
		compile_function ctx params e
	| ENext (e1,e2) ->
		compile ctx e1;
		compile ctx e2

let compile file ast =
	let ctx = {
		stack = 0;
		loop_limit = 0;
		limit = -1;
		globals = Hashtbl.create 0;
		gtable = DynArray.create();
		locals = PMap.empty;
		ops = DynArray.create();
		breaks = [];
		continues = [];
		functions = [];
		env = PMap.empty;
		nenv = 0;
	} in
	compile ctx ast;
	check_breaks ctx;
	if ctx.functions <> [] then begin
		let ops = DynArray.create() in
		DynArray.add ops (Jump 0);
		List.iter (fun (fops,gid,nargs) ->
			DynArray.set ctx.gtable gid (GlobalFunction (DynArray.length ops,nargs));
			DynArray.append fops ops;
		) (List.rev ctx.functions);
		DynArray.set ops 0 (Jump (DynArray.length ops));
		DynArray.append ctx.ops ops;
		ctx.ops <- ops;
	end;
	DynArray.to_array ctx.gtable, DynArray.to_array ctx.ops

