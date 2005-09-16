(*
 *  NekoML Compiler
 *  Copyright (c)2005 Nicolas Cannasse
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *)
 
open Ast
open Mltype

type context = {
	module_name : string;
	mutable counter : int;
	mutable refvars : (string,unit) PMap.t;
}

let gen_label ctx =
	let c = ctx.counter in
	ctx.counter <- ctx.counter + 1;
	"l" ^ string_of_int c

let gen_variable ctx =
	let c = ctx.counter in
	ctx.counter <- ctx.counter + 1;
	"v" ^ string_of_int c

let builtin name =
	EConst (Builtin name) , Ast.null_pos

let ident name =
	EConst (Ident name) , Ast.null_pos

let int n =
	EConst (Int n) , Ast.null_pos

let null =
	EConst Null , Ast.null_pos

let pos (p : Mlast.pos) = 
	{
		pmin = p.Mlast.pmin;
		pmax = p.Mlast.pmax;
		pfile = p.Mlast.pfile;
	}

let block e =
	match e with
	| EBlock _ , _ -> e 
	| _ -> EBlock [e] , snd e

let rec gen_constant ctx c =
	match c with
	| TVoid -> EConst Null
	| TInt n -> EConst (Int n)
	| TFloat s -> EConst (Float s)
	| TChar c -> EConst (String (String.make 1 c))
	| TString s -> EConst (String s)
	| TIdent s -> 		
		if PMap.mem s ctx.refvars then EArray ((EConst (Ident s),null_pos),int 0) else EConst (Ident s)
	| TConstr "true" -> EConst True
	| TConstr "false" -> EConst False
	| TConstr s -> EConst (Ident s)
	| TModule (path,c) ->
		let rec loop = function
			| [] -> assert false
			| [x] -> EConst (Ident x)
			| x :: l -> EField ((loop l,Ast.null_pos) , x)
		in
		loop ((match c with TConstr x -> x | TIdent s -> s | _ -> assert false) :: List.rev path)

let rec gen_matching ctx h fail m p =
	try
		ident (Hashtbl.find h m)
	with Not_found ->
	match m with
	| MFailure ->
		ECall (builtin "goto",[ident fail]) , p
	| MHandle (m1,m2) -> 
		let label = gen_label ctx in
		EBlock [gen_matching ctx h label m1 p; ELabel label, p; gen_matching ctx h fail m2 p] , p
	| MExecute e ->
		gen_expr ctx e
	| MConstants (m,cl) ->
		let e = gen_matching ctx h fail m p in
		let v = gen_variable ctx in
		let exec = List.fold_right (fun (c,m) acc ->
			let test = EBinop ("==", ident v , (gen_constant ctx c,p)) , p in
			let exec = gen_matching ctx h fail m p in
			Some (EIf (test, exec, acc) , p)
		) cl None in
		(match exec with
		| None -> assert false
		| Some exec ->
			EBlock [
				EVars [v, Some e] , p;
				exec
			] , p)
	| MField (m,n) ->
		EArray (gen_matching ctx h fail m p, int (n + 1)) , p
	| MSwitch (m,[TVoid,m1]) ->
		gen_matching ctx h fail m1 p
	| MSwitch (m,cl) ->
		let e = gen_matching ctx h fail m p in
		let v = gen_variable ctx in
		let exec = List.fold_right (fun (c,m) acc ->
			let test = EBinop ("==", ident v , (gen_constant ctx c,p)) , p in
			let exec = gen_matching ctx h fail m p in
			Some (EIf (test, exec, acc) , p)
		) cl None in
		(match exec with
		| None -> assert false
		| Some exec ->
			EBlock [
				EVars [v, Some (EArray (e,int 0),p)] , p;
				exec;
			] , p)
	| MBind (v,m1,m2) ->
		let e1 = gen_matching ctx h fail m1 p in
		Hashtbl.add h m1 v;
		let e2 = gen_matching ctx h fail m2 p in
		Hashtbl.remove h m1;
		EBlock [(EVars [v, Some e1] , p); e2] , p

and gen_type ctx t p =
	match t.texpr with
	| TAbstract
	| TMono
	| TPoly
	| TRecord _
	| TTuple _
	| TFun _ ->
		EBlock [] , p
	| TLink t
	| TNamed (_,_,t) ->
		gen_type ctx t p
	| TUnion (_,constrs) ->
		EBlock (List.map (fun (c,t) ->
			let field = EField (ident ctx.module_name,c) , p in
			let val_fun n =
				let args = Array.to_list (Array.init n (fun n -> "p" ^ string_of_int n)) in
				let build = ECall (builtin "array",field :: List.map (fun a -> EConst (Ident a) , p) args) , p in
				let func = EFunction (args, (EBlock [EReturn (Some build),p] , p)) , p in
				EBinop ("=" , field , func ) , p
			in
			let rec val_type t =
				match t.texpr with
				| TAbstract ->
					let make = ECall (builtin "array",[null]) , p in
					ENext ((EBinop ("=" , field, make) ,p) , (EBinop("=" , (EArray (field,int 0),p) , field) , p)) , p
				| TTuple tl -> val_fun (List.length tl)
				| TLink t -> val_type t
				| _ -> val_fun 1
			in
			val_type t
		) constrs) , p

and gen_expr ctx e =
	let p = pos e.epos in
	match e.edecl with
	| TConst c -> gen_constant ctx c , p
	| TBlock el -> EBlock (gen_block ctx el p) , p
	| TParenthesis e -> EParenthesis (gen_expr ctx e) , p
	| TCall (e,el) -> ECall (gen_expr ctx e, List.map (gen_expr ctx) el) , p
	| TField (e,s) -> EField (gen_expr ctx e, s) , p
	| TArray (e1,e2) -> EArray (gen_expr ctx e1,gen_expr ctx e2) , p
	| TVar (s,e) ->
		ctx.refvars <- PMap.remove s ctx.refvars;
		EVars [s , Some (gen_expr ctx e)] , p
	| TIf (e,e1,e2) -> EIf (gen_expr ctx e, gen_expr ctx e1, match e2 with None -> None | Some e2 -> Some (gen_expr ctx e2)) , p
	| TFunction ("_",params,e) -> EFunction (List.map fst params,block (gen_expr ctx e)) , p
	| TFunction _ -> EBlock [gen_functions ctx [e] p] , p
	| TBinop (op,e1,e2) -> EBinop (op,gen_expr ctx e1,gen_expr ctx e2) , p
	| TTupleDecl tl -> ECall (builtin "array",List.map (gen_expr ctx) tl) , p
	| TTypeDecl t -> gen_type ctx t p
	| TMut e -> gen_expr ctx (!e)
	| TRecordDecl fl -> 
		EBlock (
			(EVars ["@o" , Some (ECall (builtin "object",[]),p) ] , p) ::
			List.map (fun (s,e) ->
				EBinop ("=",(EField (ident "@o",s),p),gen_expr ctx e) , pos e.epos
			) fl
			@ [ident "@o"]
		) , p
	| TListDecl el ->
		(match el with
		| [] -> ECall (builtin "array",[]) , p
		| x :: l ->
			ECall (builtin "array",[gen_expr ctx x; gen_expr ctx { e with edecl = TListDecl l }]) , p)
	| TUnop (op,e) -> 
		(match op with
		| "-" -> EBinop ("-",int 0,gen_expr ctx e) , p
		| "*" -> EArray (gen_expr ctx e,int 0) , p
		| "!" -> ECall (builtin "not",[gen_expr ctx e]) , p
		| "&" -> ECall (builtin "array",[gen_expr ctx e]) , p
		| _ -> assert false)
	| TMatch m ->
		gen_matching ctx (Hashtbl.create 0) "<assert>" m p
	| TTupleGet (e,n) ->
		EArray (gen_expr ctx e,int (n+1)) , p

and gen_functions ctx fl p =
	let ell = ref (EVars (List.map (fun e ->
		match e.edecl with
		| TFunction ("_",params,e) ->
			"_" , Some (EFunction (List.map fst params,block (gen_expr ctx e)),p)
		| TFunction (name,_,_) ->
			ctx.refvars <- PMap.add name () ctx.refvars;
			name , Some (ECall (builtin "array",[null]),null_pos)
		| _ -> assert false
	) fl) , null_pos) in
	List.iter (fun e ->
		let p = pos e.epos in
		match e.edecl with
		| TFunction (name,params,e) ->
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
		| { edecl = TFunction (name,p,f) } as e :: l -> loop (e :: fl) l
		| { edecl = TMut r } :: l -> loop fl (!r :: l)
		| x :: l ->
			if fl <> [] then ell := gen_functions ctx (List.rev fl) p :: !ell;
			ell := gen_expr ctx x :: !ell;
			loop [] l
	in
	loop [] el;	
	ctx.refvars <- old;
	List.rev !ell

let generate e m =
	let m = String.concat "_" m in
	let ctx = {
		module_name = m;
		counter = 0;
		refvars = PMap.empty;
	} in
	let init = EBinop ("=",ident m,(ECall (builtin "new",[null]),Ast.null_pos)) , Ast.null_pos in
	ENext (init,gen_expr ctx e) , Ast.null_pos
