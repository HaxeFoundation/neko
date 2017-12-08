(*
 *  NekoML Compiler
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
open Mltype

type comparison =
	| Native
	| Structural	

type context = {
	module_name : string;
	mutable counter : int;
	mutable refvars : (string,unit) PMap.t;
}

let verbose = ref false

let gen_label ctx =
	let c = ctx.counter in
	ctx.counter <- ctx.counter + 1;
	"l" ^ string_of_int c

let gen_variable ctx =
	let c = ctx.counter in
	ctx.counter <- ctx.counter + 1;
	"v" ^ string_of_int c

let module_name m =
	"@" ^ String.concat "_" m

let builtin name =
	EConst (Builtin name) , Ast.null_pos

let ident name =
	EConst (Ident name) , Ast.null_pos

let int n =
	EConst (Int n) , Ast.null_pos

let null =
	EConst Null , Ast.null_pos

let core s p =
	EField ((EConst (Ident (module_name ["Core"])),p),s) , p

let pos (p : Mlast.pos) = 
	{
		pmin = p.Mlast.pmin;
		pmax = p.Mlast.pmax;
		pfile = p.Mlast.pfile;
	}

let rec is_fun t =
	match t.texpr with
	| TNamed (_,_,t) | TLink t -> is_fun t
	| TPoly
	| TMono _
	| TFun _ -> true
	| _ -> false

let rec call ret f args p =
	match f with
	| EConst (Builtin _) , _ -> ECall (f,args) , p
	| _ ->
		match args with
		| a :: b :: c :: d :: x :: l -> 
			let app = ECall ((EConst (Builtin "apply"),p),[f;a;b;c;d]) , p in
			call ret app (x :: l) p
		| _ ->
			if is_fun ret then
				ECall ((EConst (Builtin "apply"),p),f :: args) , p
			else
				ECall (f,args) , p

let array args p =
	ECall ((EConst (Builtin "array"),p),args) , p

let block e =
	match e with
	| EBlock _ , _ -> e 
	| _ -> EBlock [e] , snd e

let rec arity t =
	match t.texpr with
	| TAbstract -> 0
	| TTuple tl -> List.length tl
	| TLink t -> arity t
	| _ -> 1

let comparison t =
	match tlinks true t with
	| TNamed (["int"],[],_)
	| TNamed (["char"],[],_)
	| TNamed (["float"],[],_)
	| TNamed (["string"],[],_) -> Native
	| _ -> Structural

let rec gen_constant ctx c p =
	(match c with
	| TVoid -> EConst Null
	| TInt n when n < 0 -> EBinop ("-",int 0, int (-n))
	| TInt n -> EConst (Int n)
	| TFloat s -> EConst (Float s)
	| TChar c -> EConst (Int (int_of_char c))
	| TString s -> EConst (String s)
	| TIdent s ->
		if PMap.mem s ctx.refvars then EArray ((EConst (Ident s),null_pos),int 0) else EConst (Ident s)
	| TBool true -> EConst True
	| TBool false -> EConst False
	| TConstr "[]" | TModule (["Core"],TConstr "[]") -> fst (core "@empty" p)
	| TConstr "::" | TModule (["Core"],TConstr "::") -> fst (core "@cons" p)
	| TConstr s -> EConst (Ident s)
	| TModule ([],c) -> fst (gen_constant ctx c p)
	| TModule (m,c) ->		
		EField ( (EConst (Ident (module_name m)),p) , (match c with TConstr x -> x | TIdent s -> s | _ -> assert false))
	) , p

type match_context = {
	ctx : context;
	h : (match_op , string) Hashtbl.t;
	out : string;
	pos : Ast.pos;
	mutable next : string;
	mutable first : bool;
}

let no_label = "<assert>"

let rec gen_match_rec mctx fail m =
	try
		ident (Hashtbl.find mctx.h m)
	with Not_found ->
	let p = mctx.pos in
	let ctx = mctx.ctx in
	let gen_rec = gen_match_rec mctx in
	match m with
	| MFailure ->
		let label = if mctx.first && mctx.next <> no_label then mctx.next else fail in
		if label = no_label then
			EBlock [] , p
		else
			call t_void (builtin "goto") [ident label] p
	| MHandle (m1,m2) -> 
		let label = gen_label ctx in
		let m1 = gen_rec label m1 in
		let m2 = gen_rec fail m2 in
		EBlock [m1; ELabel label, p; m2] , p
	| MRoot ->
		assert false
	| MExecute (e,b) ->
		if not b then begin
			let ematch = EBinop ("==" , ident "@exc" , core "Stream_matching" p) , p in
			let reraise = EIf ( ematch , gen_rec fail MFailure, Some(ECall (builtin "throw", [ident "@exc"]) , p) ) , p in
			mctx.first <- false;
			match e.edecl with
			| TConst _ -> gen_expr ctx e 
			| _ -> ETry (gen_expr ctx e, "@exc" , reraise ) , p
		end else begin
			mctx.first <- true;
			let out = call t_void (builtin "goto") [ident mctx.out] p in
			EBlock [gen_expr ctx e;out] , p
		end;
	| MConstants (m,[TIdent v,m1]) ->
		let m = gen_rec fail m in
		EBlock [
			EVars [v, Some m] , p;
			gen_rec fail m1
		] , p
	| MConstants (m,cl) ->
		let e = gen_rec fail m in
		let v = gen_variable ctx in
		let exec = List.fold_left (fun acc (c,m) ->
			let test = EBinop ("==", ident v, gen_constant ctx c p) , p in
			let exec = gen_rec fail m in
			EIf (test, exec, Some acc) , p
		) (gen_rec fail MFailure) (List.rev cl) in
		EBlock [
			EVars [v, Some e] , p;
			exec
		] , p
	| MRecordField (m,f) ->
		EField (gen_rec fail m, f) , p
	| MTuple (m,n) ->
		EArray (gen_rec fail m, int n) , p
	| MField (m,n) ->
		EArray (gen_rec fail m, int (n + 2)) , p
	| MNext (m1,m2) ->
		let old = mctx.next in
		let label = gen_label ctx in
		mctx.next <- label;
		let m1 = gen_rec fail m1 in
		mctx.next <- old;
		let m2 = gen_rec fail m2 in
		EBlock [m1; ELabel label, p; m2] , p
	| MSwitch (m,cl) ->
		let e = gen_rec fail m in
		let v = gen_variable ctx in
		let exec = List.fold_left (fun acc (c,m) ->
			let test = EBinop ("==", ident v, gen_constant ctx c p) , p in
			let exec = gen_rec fail m in
			EIf (test, exec, Some acc) , p
		) (gen_rec fail MFailure) (List.rev cl) in
		EBlock [
			EVars [v, Some (EArray (e,int 0),p)] , p;
			exec;
		] , p
	| MBind (v,m1,m2) ->
		let e1 = gen_rec fail m1 in
		Hashtbl.add mctx.h m1 v;
		let e2 = gen_rec fail m2 in
		Hashtbl.remove mctx.h m1;
		EBlock [(EVars [v, Some e1] , p); e2] , p
	| MWhen (e,m) ->
		let e = gen_expr ctx e in
		let m = gen_rec fail m in
		let fail = gen_rec fail MFailure in
		EIf (e,m,Some fail) , p
	| MToken (m,n) ->
		call t_void (core "stream_token" p) [gen_rec fail m; int n] p		
	| MJunk (m,n,m2) ->
		let m = gen_rec fail m in
		mctx.first <- false;
		EBlock [
			call t_void (core "stream_junk" p) [m; int n] p;		
			gen_rec fail m2
		] , p

and gen_matching ctx v m p stream out =
	let mctx = {
		ctx = ctx;
		h = Hashtbl.create 0;
		pos = p;
		out = out;
		first = stream;
		next = no_label;
	} in
	let label = (if stream then gen_label ctx else no_label) in
	Hashtbl.add mctx.h MRoot v;	
	let e = gen_match_rec mctx label m in
	if stream then begin
		let vpos = gen_variable ctx in
		let stream_pos = ECall (core "stream_pos" p, [ident v]) , p in
		let test = EBinop ("==", ident vpos , stream_pos) , p in
		let exc = ECall (builtin "throw",[EIf (test, core "Stream_matching" p , Some (core "Stream_error" p)) , p]) , p in
		EBlock [EVars [vpos , Some stream_pos] , p; e; ELabel label , p; exc] , p
	end else
		e

and gen_match ctx e m stream p =
	let out = gen_label ctx in 
	let v = gen_variable ctx in
	let m = gen_matching ctx v m p stream out in
	let m = ENext ((EVars [v,Some e],p),m) , p in
	EBlock [m; ELabel out , p] , p

and gen_constructor ctx tname c t p =
	let field = ident c in
	let printer = EConst (Ident (tname ^ "__string")) , p in
	let val_type t =
		match arity t with
		| 0 ->
			let make = array [null;printer] p in
			ENext ((EBinop ("=" , field, make) ,p) , (EBinop("=" , (EArray (field,int 0),p) , field) , p)) , p
		| n ->
			let args = Array.to_list (Array.init n (fun n -> "p" ^ string_of_int n)) in
			let build = array (field :: printer :: List.map (fun a -> EConst (Ident a) , p) args) p in
			let func = EFunction (args, (EBlock [EReturn (Some build),p] , p)) , p in
			EBinop ("=" , field , func ) , p
	in
	let export = EBinop ("=", (EField (ident ctx.module_name,c),p) , field) , p in
	ENext (val_type t , export) , p

and gen_type_printer ctx c t =	
	let printer = mk (TConst (TModule (["Core"],TIdent "@print_union"))) t_void Mlast.null_pos in
	let e = mk (TCall (printer,[
		mk (TConst (TString c)) t_string Mlast.null_pos;
		mk (TConst (TIdent "v")) t_void Mlast.null_pos
	])) t_string Mlast.null_pos in
	e

and gen_type ctx name t p =
	match t.texpr with
	| TAbstract
	| TMono _
	| TPoly
	| TRecord _
	| TTuple _
	| TFun _
	| TNamed (_,_,{ texpr = TNamed _ }) ->
		EBlock [] , p
	| TLink t ->
		gen_type ctx name t p
	| TNamed (name,_,t) ->
		let rec loop = function
			| [] -> assert false
			| [x] -> x
			| _ :: l -> loop l
		in
		gen_type ctx (loop name) t p
	| TUnion (_,constrs) ->
		let cmatch = gen_match ctx (ident "v") (MSwitch (MRoot,List.map (fun (c,t) ->
			let e = gen_type_printer ctx c t in
			TConstr c , MExecute (e,true)
		) constrs)) false p in
		let printer = EFunction (["v"], cmatch) , p in
		let regs = List.map (fun (c,t) -> gen_constructor ctx name c t p) constrs in
		EBlock ((EVars [name ^ "__string",Some printer],p) :: regs) , p

and gen_binop ctx op e1 e2 p =
	let compare op =
		let cmp = ECall (core "@compare" p,[gen_expr ctx e1; gen_expr ctx e2]) , p in
		EBinop (op , cmp , int 0) , p
	in
	let make op =
		EBinop (op,gen_expr ctx e1,gen_expr ctx e2) , p
	in
	let builtin op =
		ECall (builtin op,[gen_expr ctx e1; gen_expr ctx e2]) , p
	in
	match op with
	| "and" -> make "&"
	| "or" -> make "|"
	| "xor" -> make "^"
	| "==" | "!=" | ">" | "<" | ">=" | "<=" -> 
		(match comparison e1.etype with
		| Structural -> compare op
		| Native -> make op)
	| "===" -> EBinop ("==", builtin "pcompare" , int 0) , p
	| "!==" -> EBinop ("!=" , builtin "pcompare" , int 0) , p
	| ":=" ->
		(match e1.edecl with
		| TField _ -> make "="
		| TArray (a,i) ->
			ECall (core "@aset" p,[gen_expr ctx a; gen_expr ctx i; gen_expr ctx e2]) , p
		| _ ->
			EBinop ("=",(EArray (gen_expr ctx e1,int 0),pos e1.epos),gen_expr ctx e2) , p)
	| _ -> 
		make op

and gen_expr ctx e =
	let p = pos e.epos in
	match e.edecl with
	| TConst c -> gen_constant ctx c p
	| TBlock el -> EBlock (gen_block ctx el p) , p
	| TParenthesis e -> EParenthesis (gen_expr ctx e) , p
	| TCall ({ edecl = TConst (TIdent "neko") },[{ edecl = TConst (TString s) }])
	| TCall ({ edecl = TConst (TModule ([],TIdent "neko")) },[{ edecl = TConst (TString s) }])
	| TCall ({ edecl = TConst (TModule (["Core"],TIdent "neko")) },[{ edecl = TConst (TString s) }]) ->
		let ch = IO.input_string (String.concat "\"" (ExtString.String.nsplit s "'")) in
		let file = "neko@" ^ p.pfile in
		Parser.parse (Lexing.from_function (fun s p -> try IO.input ch s 0 p with IO.No_more_input -> 0)) file
	| TCall (f,el) -> 
		let f = gen_expr ctx f in
		call e.etype f (List.map (gen_expr ctx) el) p
	| TField (e,s) -> EField (gen_expr ctx e, s) , p
	| TArray (e1,e2) -> 
		let e1 = gen_expr ctx e1 in
		ECall (core "@aget" p,[e1;gen_expr ctx e2]) , p
	| TVar ([v],e) ->
		ctx.refvars <- PMap.remove v ctx.refvars;
		EVars [v , Some (gen_expr ctx e)] , p
	| TVar (vl,e) ->
		let n = ref (-1) in
		EVars (("@tmp" , Some (gen_expr ctx e)) :: List.map (fun v ->
			ctx.refvars <- PMap.remove v ctx.refvars;
			incr n;
			v , Some (EArray (ident "@tmp",int !n),p)
		) vl) , p
	| TIf (e,e1,e2) -> 
		let e = gen_expr ctx e in
		let e1 = gen_expr ctx e1 in
		EIf (e, e1, match e2 with None -> None | Some e2 -> Some (gen_expr ctx e2)) , p
	| TWhile (e1,e2) ->
		let e1 = gen_expr ctx e1 in
		let e2 = gen_expr ctx e2 in
		EWhile (e1, e2, NormalWhile) , p
	| TFunction (_,"_",params,e) -> EFunction (List.map fst params,block (gen_expr ctx e)) , p
	| TFunction (false,name,params,e) -> EVars [name , Some (EFunction (List.map fst params,block (gen_expr ctx e)) , p)] , p
	| TFunction _ -> EBlock [gen_functions ctx [e] p] , p
	| TBinop (op,e1,e2) -> gen_binop ctx op e1 e2 p
	| TTupleDecl tl -> array (List.map (gen_expr ctx) tl) p
	| TTypeDecl t -> gen_type ctx "<assert>" t p
	| TMut e -> gen_expr ctx (!e)
	| TRecordDecl fl -> 
		EObject (("__string", core "@print_record" p) :: List.map (fun (s,e) -> s , gen_expr ctx e) fl) , p
	| TListDecl el ->
		(match el with
		| [] -> array [] p
		| x :: l ->
			let x = gen_expr ctx x in
			array [x; gen_expr ctx { e with edecl = TListDecl l }] p)
	| TUnop (op,e) -> 
		(match op with
		| "-" -> EBinop ("-",int 0,gen_expr ctx e) , p
		| "*" -> EArray (gen_expr ctx e,int 0) , p
		| "!" -> call t_void (builtin "not") [gen_expr ctx e] p
		| "&" -> array [gen_expr ctx e] p
		| _ -> assert false)
	| TMatch (e,m,stream) ->
		gen_match ctx (gen_expr ctx e) m stream p
	| TTupleGet (e,n) ->
		EArray (gen_expr ctx e,int n) , p
	| TErrorDecl (e,t) ->
		let printer = gen_expr ctx (gen_type_printer ctx e t) in
		let printer = EFunction (["v"], (EBlock [printer],p)) , p in
		let printer = EVars [e ^ "__string",Some printer] , p in
		ENext (printer , gen_constructor ctx e e t p) , p
	| TTry (e,m) ->
		let out = gen_label ctx in
		let matching = gen_matching ctx "@exc" m p false out in
		let reraise = call t_void (builtin "throw") [ident "@exc"] p in
		let handle = EBlock [matching;reraise;ELabel out , p] , p in
		ETry (gen_expr ctx e,"@exc",handle) , p

and gen_functions ctx fl p =
	let ell = ref (EVars (List.map (fun e ->
		match e.edecl with
		| TFunction (_,"_",params,e) ->
			"_" , Some (EFunction (List.map fst params,block (gen_expr ctx e)),p)
		| TFunction (_,name,_,_) ->
			ctx.refvars <- PMap.add name () ctx.refvars;
			name , Some (array [null] null_pos)
		| _ -> assert false
	) fl) , null_pos) in
	List.iter (fun e ->
		let p = pos e.epos in
		match e.edecl with
		| TFunction (_,name,params,e) ->
			if name <> "_" then begin
				let e = gen_expr ctx e in
				let e = EFunction (List.map fst params,block e) , p in
				let e = EBinop ("=",(EArray (ident name,int 0),p),e) , p in
				let e = EBlock [e; EBinop ("=",ident name,(EArray (ident name,int 0),p)) , p] , p in
				ell := ENext (!ell, e) , p;
				ctx.refvars <- PMap.remove name ctx.refvars;
			end;
		| _ ->
			assert false
	) fl;
	!ell

and gen_block ctx el p =
	let old = ctx.refvars in
	let ell = ref [] in
	let rec loop fl = function
		| [] -> if fl <> [] then ell := gen_functions ctx (List.rev fl) p :: !ell
		| { edecl = TFunction (true,name,p,f) } as e :: l -> loop (e :: fl) l
		| { edecl = TMut r } :: l -> loop fl (!r :: l)
		| x :: l ->
			if fl <> [] then ell := gen_functions ctx (List.rev fl) p :: !ell;
			ell := gen_expr ctx x :: !ell;
			loop [] l
	in
	loop [] el;	
	ctx.refvars <- old;
	List.rev !ell

let generate e deps idents m =
	let m = module_name m in
	let ctx = {
		module_name = m;
		counter = 0;
		refvars = PMap.empty;
	} in
	if !verbose then print_endline ("Generating " ^ m ^ ".neko");
	let init = EBinop ("=",ident m,builtin "exports"), null_pos in
	let deps = List.map (fun m -> 
		let file = String.concat "/" m in
		let load = ECall ((EField (builtin "loader","loadmodule"),null_pos),[gen_constant ctx (TString file) null_pos;builtin "loader"]) , null_pos in
		EBinop ("=", ident (module_name m), load ) , null_pos
	) deps in
	let exports = List.map (fun i ->
		EBinop ("=", (EField (builtin "exports",i),null_pos) , ident i) , null_pos
	) idents in
	match gen_expr ctx e with
	| EBlock e , p -> EBlock (init :: deps @ e @ exports) , p 
	| e -> EBlock (init :: deps @ e :: exports) , null_pos

