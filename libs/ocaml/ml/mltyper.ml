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
 
open Mlast
open Mltype

type module_context = {
	path : string list;
	types : (string,t) Hashtbl.t;
	constrs : (string, t * t) Hashtbl.t;
	records : (string,t * t * mutflag) Hashtbl.t;
	deps : (string list, module_context) Hashtbl.t;
	mutable expr : texpr option;
	mutable idents : (string,t) PMap.t;
}

type context = {
	gen : id_gen;
	mutable mink : int;
	mutable functions : (bool * string * texpr ref * t * (string * t) list * expr * t * pos) list;
	mutable opens : module_context list;
	mutable curfunction : string;
	tmptypes : (string, t * t list * (string,t) Hashtbl.t) Hashtbl.t;
	current : module_context;
	modules : (string list, module_context) Hashtbl.t;
	classpath : string list;
}

type error_msg =
	| Cannot_unify of t * t
	| Have_no_field of t * string
	| Stack of error_msg * error_msg
	| Unknown_field of string
	| Module_not_loaded of module_context
	| Custom of string

exception Error of error_msg * pos

module SSet = Set.Make(String)

let rec error_msg ?(h=s_context()) = function
	| Cannot_unify (t1,t2) -> "Cannot unify " ^ s_type ~h t1 ^ " and " ^ s_type ~h t2
	| Have_no_field (t,f) -> s_type ~h t ^ " have no field " ^ f
	| Stack (m1,m2) -> error_msg ~h m1 ^ "\n  " ^ error_msg ~h m2
	| Unknown_field s -> "Unknown field " ^ s
	| Module_not_loaded m -> "Module " ^ String.concat "." m.path ^ " require an interface"
	| Custom s -> s

let error m p = raise (Error (m,p))

let verbose = ref false

let load_module_ref = ref (fun _ _ -> assert false)

let add_local ctx v t =
	if v <> "_" then ctx.current.idents <- PMap.add v t ctx.current.idents

let save_locals ctx = 
	ctx.current.idents

let restore_locals ctx l =
	ctx.current.idents <- l

let get_module ctx path p =
	match path with
	| [] -> ctx.current
	| _ ->
		let m = (try
			Hashtbl.find ctx.modules path
		with
			Not_found -> 
				!load_module_ref ctx path p) in		
		if m != ctx.current then begin
			if m.expr = None then error (Module_not_loaded m) p;
			Hashtbl.replace ctx.current.deps m.path m;
		end;
		m

let get_type ctx path name p =
	let rec loop = function
		| [] -> error (Custom ("Unknown type " ^ s_path path name)) p
		| m :: l ->
			try
				Hashtbl.find m.types name
			with
				Not_found -> loop l
	in
	match path with
	| [] ->
		loop (ctx.current :: ctx.opens)
	| _ ->
		loop [get_module ctx path p]

let get_constr ctx path name p =
	let rec loop = function
		| [] -> error (Custom ("Unknown constructor " ^ s_path path name)) p
		| m :: l ->
			try
				let t1, t2 = Hashtbl.find m.constrs name in
				(if m == ctx.current then [] else m.path) , t1, t2
			with
				Not_found -> loop l
	in
	match path with
	| [] ->
		loop (ctx.current :: ctx.opens)
	| _ ->
		loop [get_module ctx path p]

let get_ident ctx path name p =
	let rec loop = function
		| [] -> error (Custom ("Unknown identifier " ^ s_path path name)) p
		| m :: l ->
			try
				(if m == ctx.current then [] else m.path) , PMap.find name m.idents
			with
				Not_found -> loop l
	in
	match path with
	| [] ->
		loop (ctx.current :: ctx.opens)
	| _ ->
		loop [get_module ctx path p]

let get_record ctx f p =
	let rec loop = function
		| [] -> error (Unknown_field f) p
		| m :: l ->
			try
				Hashtbl.find m.records f
			with
				Not_found -> loop l
	in
	let rt , ft , mut = loop (ctx.current :: ctx.opens) in
	let h = Hashtbl.create 0 in
	duplicate ctx.gen ~h rt, duplicate ctx.gen ~h ft, mut

let rec is_tuple t = 
	match t.texpr with
	| TLink t -> is_tuple t
	| TTuple _ -> true
	| TNamed(_,_,t) -> is_tuple t
	| _ -> false

let rec is_recursive t1 t2 = 
	if t1 == t2 then
		true
	else match t2.texpr with
	| TAbstract
	| TMono _
	| TPoly ->
		false
	| TRecord _
	| TUnion _ ->
		assert false
	| TTuple tl -> List.exists (is_recursive t1) tl
	| TLink t -> is_recursive t1 t
	| TFun (tl,t) -> List.exists (is_recursive t1) tl || is_recursive t1 t
	| TNamed (_,p,t) -> List.exists (is_recursive t1) p

let link ctx t1 t2 p =
	if is_recursive t1 t2 then error (Cannot_unify (t1,t2)) p;
	t1.texpr <- TLink t2;
	if t1.tid < 0 then begin
		if t2.tid = -1 then t1.tid <- -1 else t1.tid <- genid ctx.gen;
	end else
		if t2.tid = -1 then t1.tid <- -1

let unify_stack t1 t2 = function
	| Error (Cannot_unify _ as e , p) -> error (Stack (e , Cannot_unify (t1,t2))) p
	| e -> raise e

let is_alias = function
	| TAbstract 
	| TRecord _
	| TUnion _ -> false
	| TMono _
	| TPoly
	| TTuple _
	| TLink _
	| TFun _
	| TNamed _ -> true

let rec propagate k t =
	match t.texpr with
	| TAbstract
	| TPoly -> ()
	| TUnion _
	| TRecord _ -> assert false
	| TMono k2 -> if k < k2 then t.texpr <- TMono k	
	| TTuple tl -> List.iter (propagate k) tl
	| TLink t -> propagate k t
	| TFun (tl,t) -> propagate k t; List.iter (propagate k) tl
	| TNamed (_,tl,_) -> List.iter (propagate k) tl

let rec unify ctx t1 t2 p =
	if t1 == t2 then
		()
	else match t1.texpr , t2.texpr with
	| TLink t , _ -> unify ctx t t2 p
	| _ , TLink t -> unify ctx t1 t p
	| TMono k , t -> link ctx t1 t2 p; propagate k t2
	| t , TMono k -> link ctx t2 t1 p; propagate k t1
	| TPoly , t -> link ctx t1 t2 p
	| t , TPoly -> link ctx t2 t1 p
	| TNamed (n1,p1,_) , TNamed (n2,p2,_) when n1 = n2 ->
		(try
			List.iter2 (fun p1 p2 -> unify ctx p1 p2 p) p1 p2
		with	
			e -> unify_stack t1 t2 e)
	| TNamed (_,_,t1) , _ when is_alias t1.texpr ->
		(try
			unify ctx t1 t2 p
		with
			e -> unify_stack t1 t2 e)
	| _ , TNamed (_,_,t2) when is_alias t2.texpr ->
		(try
			unify ctx t1 t2 p
		with
			e -> unify_stack t1 t2 e)
	| TFun (tl1,r1) , TFun (tl2,r2) when List.length tl1 = List.length tl2 -> 
		(try
			List.iter2 (fun t1 t2 -> unify ctx t1 t2 p) tl1 tl2; 
			unify ctx r1 r2 p;
		with	
			e -> unify_stack t1 t2 e)
	| TTuple tl1 , TTuple tl2 when List.length tl1 = List.length tl2 ->
		(try
			List.iter2 (fun t1 t2 -> unify ctx t1 t2 p) tl1 tl2
		with	
			e -> unify_stack t1 t2 e)
	| _ , _ ->
		error (Cannot_unify (t1,t2)) p

let rec type_type ?(allow=true) ?(h=Hashtbl.create 0) ctx t p =
	match t with
	| ETuple [] ->
		assert false
	| ETuple [t] ->
		type_type ~allow ~h ctx t p
	| ETuple el ->
		mk_tup ctx.gen (List.map (fun t -> type_type ~allow ~h ctx t p) el)
	| EPoly s ->
		(try
			Hashtbl.find h s
		with
			Not_found ->
				if not allow then error (Custom ("Unbound type variable '" ^ s)) p;
				let t = t_mono ctx.gen in
				Hashtbl.add h s t;
				t)
	| EType (param,path,name) ->
		let param = (match param with None -> None | Some t -> Some (type_type ~allow ~h ctx t p)) in
		let t = get_type ctx path name p in
		(match t.texpr with
		| TNamed (_,params,t2) ->
			let tl = (match params, param with
				| [] , None -> []
				| [x] , Some t -> [t]
				| l , Some { texpr = TTuple tl } when List.length tl = List.length l -> tl
				| _ , _ -> error (Custom ("Invalid number of type parameters for " ^ s_path path name)) p
			) in
			let h = Hashtbl.create 0 in
			let t = duplicate ctx.gen ~h t in
			let params = List.map (duplicate ctx.gen ~h) params in
			List.iter2 (fun pa t -> unify ctx pa t p) params tl;
			t
		| _ -> assert false)
	| EArrow _ ->
		let rec loop params t =
			match t with
			| EArrow (ta,tb) -> 
				let ta = type_type ~allow ~h ctx ta p in
				loop (ta :: params) tb
			| _ ->
				let t = type_type ~allow ~h ctx t p in
				mk_fun ctx.gen (List.rev params) t
		in
		loop [] t

let rec type_constant ctx ?(path=[]) c p =
	match c with
	| Int i -> mk (TConst (TInt i)) t_int p
	| Float s -> mk (TConst (TFloat s)) t_float p
	| String s -> mk (TConst (TString s)) t_string p
	| Bool b -> mk (TConst (TBool b)) t_bool p
	| Char c -> mk (TConst (TChar c)) t_char p
	| Ident s ->
		let path , t = get_ident ctx path s p in
		let t = duplicate ctx.gen t in		
		mk (TConst (TModule (path,TIdent s))) t p
	| Constr s ->
		let path , ut , t = get_constr ctx path s p in
		let t = duplicate ctx.gen (match t.texpr with
			| TAbstract -> ut
			| TTuple tl -> mk_fun ctx.gen tl ut
			| _ -> mk_fun ctx.gen [t] ut) in
		mk (TConst (TModule (path,TConstr s))) t p
	| Module (path,c) ->
		type_constant ctx ~path c p

type addable = NInt | NFloat | NString | NNan

let addable str e =
	match etype true e with
	| TNamed (["int"],_,_) -> NInt
	| TNamed (["float"],_,_) -> NFloat
	| TNamed (["string"],_,_) when str -> NString
	| _ -> NNan

let type_binop ctx op e1 e2 p =
	let emk t = mk (TBinop (op,e1,e2)) t p in
	match op with
	| "%"
	| "+"
	| "-"
	| "/"
	| "*" ->
		let str = (op = "+") in
		(match addable str e1, addable str e2 with
		| NInt , NInt -> emk t_int
		| NFloat , NFloat
		| NInt , NFloat
		| NFloat , NInt -> emk t_float
		| NInt , NString
		| NFloat , NString
		| NString , NInt 
		| NString , NFloat
		| NString , NString -> emk t_string
		| NInt , NNan
		| NFloat , NNan 
		| NString , NNan ->
			unify ctx e2.etype e1.etype (pos e2);
			emk e1.etype
		| NNan , NInt
		| NNan , NFloat 
		| NNan , NString ->
			unify ctx e1.etype e2.etype (pos e1);
			emk e2.etype
		| NNan , NNan ->
			unify ctx e1.etype t_int (pos e1);
			unify ctx e2.etype t_int (pos e2);
			emk t_int)
	| ">>"
	| ">>>"
	| "<<"
	| "and"
	| "or"
	| "xor" ->
		unify ctx e1.etype t_int (pos e1);
		unify ctx e2.etype t_int (pos e2);
		emk t_int
	| "&&"
	| "||" ->
		unify ctx e1.etype t_bool (pos e1);
		unify ctx e2.etype t_bool (pos e2);
		emk t_bool
	| "<"
	| "<="
	| ">"
	| ">="
	| "=="
	| "!="
	| "==="
	| "!==" ->
		unify ctx e2.etype e1.etype (pos e2);
		emk t_bool
	| ":=" ->
		(match e1.edecl with
		| TArray _ ->
			unify ctx e2.etype e1.etype (pos e2);
			emk t_void
		| TField (e,f) ->
			(match tlinks false e.etype with
			| TRecord fl ->
				let _ , mut , _ = (try List.find (fun (f2,_,_) -> f2 = f) fl with Not_found -> assert false) in
				if mut = Immutable then error (Custom ("Field " ^ f ^ " is not mutable")) (pos e1);
				unify ctx e2.etype e1.etype (pos e2);
				emk t_void
			| _ -> assert false);
		| _ ->
			let t , pt = t_poly ctx.gen "ref" in
			unify ctx e2.etype pt (pos e2);
			unify ctx e1.etype t (pos e1);
			emk t_void)
	| "::" ->
		let t , pt = t_poly ctx.gen "list" in
		unify ctx e1.etype pt (pos e1);
		unify ctx e2.etype t (pos e2);
		let c = mk (TConst (TConstr "::")) (t_mono ctx.gen) p in
		mk (TCall (c,[e1;e2])) t p
	| _ ->
		error (Custom ("Invalid operation " ^ op)) p

let type_unop ctx op e p =
	let emk t = mk (TUnop (op,e)) t p in
	match op with
	| "&" ->
		let p , pt = t_poly ctx.gen "ref" in
		unify ctx e.etype pt (pos e);
		emk p
	| "*" ->
		let p , pt = t_poly ctx.gen "ref" in
		unify ctx e.etype p (pos e);
		emk pt
	| "!" -> 
		unify ctx e.etype t_bool (pos e);
		emk t_bool 
	| "-" ->
		(match addable false e with
		| NInt -> emk t_int
		| NFloat -> emk t_float
		| _ ->
			unify ctx e.etype t_int (pos e);
			emk t_int)
	| _ ->
		assert false

let rec type_arg ctx h binds p = function
	| ATyped (a,t) -> 
		let n , ta = type_arg ctx h binds p a in
		unify ctx ta (type_type ~h ctx t p) p;
		n , ta
	| ANamed s ->
		s , t_mono ctx.gen
	| ATuple al ->
		let aname = "@t" ^ string_of_int (genid ctx.gen) in
		let nl , tl = List.split (List.map (type_arg ctx h binds p) al) in
		let k = ref 0 in
		List.iter (fun n ->
			if n <> "_" then binds := (aname,!k,n) :: !binds;
			incr k;
		) nl;
		aname , mk_tup ctx.gen tl

let register_function ctx isrec name pl e rt p =
	if ctx.functions = [] then ctx.mink <- !(ctx.gen);
	let pl = (match pl with [] -> [ATyped (ANamed "_",EType (None,[],"void"))] | _ -> pl) in
	let expr = ref (mk (TConst TVoid) t_void p) in
	let h = Hashtbl.create 0 in
	let binds = ref [] in
	let el = List.map (type_arg ctx h binds p) pl in
	let name = (match name with None -> "_" | Some n -> n) in
	let e = (match List.rev !binds with 
		| [] -> e
		| l -> 
			EBlock (List.fold_left (fun acc (v,n,v2) ->
				(EVar ([v2,None], (ETupleGet ((EConst (Ident v),p),n),p)) , p) :: acc
			) [e] l) , p
	) in
	let rt = (match rt with 
		| None -> t_mono ctx.gen
		| Some rt -> type_type ~h ctx rt p
	) in
	let ft = mk_fun ctx.gen (List.map snd el) rt in		
	ctx.functions <- (isrec,name,expr,ft,el,e,rt,p) :: ctx.functions;
	if isrec then add_local ctx name ft;
	mk (TMut expr) (if name = "_" then ft else t_void) p

let type_format ctx s p =
	let types = ref [] in
	let percent = ref false in
	for i = 0 to String.length s - 1 do
		let c = String.get s i in
		if !percent then begin
			percent := false;
			match c with
			| '%' -> 
				()
			| 'x' | 'X' | 'd' ->
				types := t_int :: !types
			| 'f' ->
				types := t_float :: !types
			| 's' ->
				types := t_string :: !types
			| 'b' ->
				types := t_bool :: !types
			| 'c' ->
				types := t_char :: !types
			| '0'..'9' | '.' ->
				percent := true
			| _ ->
				error (Custom "Invalid % sequence") p
		end else
			match c with
			| '%' -> 
				percent := true
			| _ ->
				()
	done;
	if !percent then error (Custom "Invalid % sequence") p;
	match !types with
	| [] -> t_void
	| [x] -> x
	| l -> mk_tup ctx.gen (List.rev l)

let rec type_functions ctx =
	let l = ctx.functions in
	if l <> [] then
	let mink = ctx.mink in
	ctx.functions <- [];
	let l = List.map (fun (isrec,name,expr,ft,el,e,rt,p) ->
		let locals = save_locals ctx in
		let func = ctx.curfunction in
		if name <> "_" then begin
			let fname = s_path ctx.current.path name in
			if !verbose then prerr_endline ("Typing " ^ fname);
			ctx.curfunction <- fname;
		end;
		List.iter (fun (p,pt) ->
			add_local ctx p pt
		) el;		
		let e = type_expr ctx e in
		restore_locals ctx locals;
		ctx.curfunction <- func;
		let ft2 = mk_fun ctx.gen (List.map snd el) e.etype in
		unify ctx ft ft2 p;
		expr := mk (TFunction (isrec,name,el,e)) ft2 p;
		if not isrec then add_local ctx name ft;
		ft2
	) (List.rev l) in
	List.iter (polymorphize ctx.gen mink) l

and type_expr ctx (e,p) =
	match e with
	| EConst c ->
		type_constant ctx c p
	| EBlock [] -> 
		mk (TConst TVoid) t_void p
	| EBlock (e :: l) ->
		let locals = save_locals ctx in
		let e = type_block ctx e in
		let el , t = List.fold_left (fun (l,t) e ->
			unify ctx t t_void (List.hd l).epos;
			let e = type_block ctx e in
			e :: l , e.etype
		) ([e] , e.etype) l in
		type_functions ctx;
		restore_locals ctx locals;
		mk (TBlock (List.rev el)) t p
	| EApply (e,el) ->
		type_expr ctx (ECall (e,el),p)
	| ECall ((EConst (Ident "open"),_),[EConst (Module (m,Constr modname)),p]) ->
		ctx.opens <- get_module ctx (m @ [modname]) p :: ctx.opens;
		mk (TConst TVoid) t_void p
	| ECall ((EConst (Ident "open"),_),[EConst (Constr modname),p]) ->
		ctx.opens <- get_module ctx [modname] p :: ctx.opens;
		mk (TConst TVoid) t_void p
	| ECall ((EConst (Ident "assert"),_) as a,[]) ->
		let line = Mllexer.get_error_line p in
		type_expr ctx (ECall (a,[EConst (String p.pfile),p;EConst (Int line),p]),p)
	| ECall ((EConst (Ident "invalid_arg"),_) as a,[]) ->
		type_expr ctx (ECall (a,[EConst (String ctx.curfunction),p]),p)
	| ECall ((EConst (Constr "TYPE"),_),[e]) ->
		let e = type_expr ctx e in
		prerr_endline ("type : " ^ s_type e.etype);
		mk (TParenthesis e) t_void p
	| ECall (e,el) ->
		let e = type_expr ctx e in
		let el = (match el with [] -> [ETupleDecl [],p] | _ -> el) in
		let el = List.map (type_expr ctx) el in
		(match etype false e with
		| TFun (args,r) ->
			let rec loop acc expr l tl r =
				match l , tl with
				| e :: l , t :: tl ->
					(match tlinks true t with 
					| TNamed (["format"],[param],_) ->
						(match e.edecl with
						| TConst (TString s) ->
							let tfmt = type_format ctx s e.epos in
							unify ctx param tfmt e.epos;
						| _ ->
							(match tlinks true e.etype with
							| TNamed (["format"],[param2],_) ->
								unify ctx param2 param e.epos
							| _ ->
								error (Custom "Constant string required for format") e.epos))
					| _ ->
						unify ctx e.etype t (pos e));
					loop (e :: acc) expr l tl r
				| [] , [] ->
					mk (TCall (expr,List.rev acc)) r p
				| [] , tl ->
					mk (TCall (expr,List.rev acc)) (mk_fun ctx.gen tl r) p
				| el , [] ->
					match tlinks false r with
					| TFun (args,r2) -> loop [] (mk (TCall (expr,List.rev acc)) r p) el args r2
					| _ -> error (Custom "Too many arguments") p
			in
			loop [] e el args r
		| _ ->
			let r = t_mono ctx.gen in
			let f = mk_fun ctx.gen (List.map (fun e -> e.etype) el) r in
			unify ctx e.etype f p;
			mk (TCall (e,el)) r p
		);
	| EField (e,s) ->
		let e = type_expr ctx e in
		let t = (match etype false e with
		| TRecord fl ->
			(try
				let _ , _ , t = List.find (fun (s2,_,_) -> s = s2) fl in
				t
			with
				Not_found -> error (Have_no_field (e.etype,s)) p)
		| _ ->
			let r , t , _ = get_record ctx s p in
			unify ctx e.etype r (pos e);
			t
		) in
		mk (TField (e,s)) t p
	| EArray (e,ei) ->
		let e = type_expr ctx e in
		let ei = type_expr ctx ei in
		unify ctx ei.etype t_int (pos ei);
		let t , pt = t_poly ctx.gen "array" in
		unify ctx e.etype t (pos e);
		mk (TArray (e,ei)) pt p
	| EVar _ ->
		error (Custom "Variable declaration not allowed outside a block") p	
	| EIf (e,e1,None) ->
		let e = type_expr ctx e in
		unify ctx e.etype t_bool (pos e);
		let e1 = type_expr ctx e1 in
		unify ctx e1.etype t_void (pos e1);
		mk (TIf (e,e1,None)) t_void p
	| EIf (e,e1,Some e2) ->
		let e = type_expr ctx e in
		unify ctx e.etype t_bool (pos e);
		let e1 = type_expr ctx e1 in
		let e2 = type_expr ctx e2 in
		unify ctx e2.etype e1.etype (pos e2);
		mk (TIf (e,e1,Some e2)) e1.etype p
	| EWhile (e1,e2) ->
		let e1 = type_expr ctx e1 in
		unify ctx e1.etype t_bool (pos e1);
		let e2 = type_expr ctx e2 in
		unify ctx e2.etype t_void (pos e2);
		mk (TWhile (e1,e2)) t_void p
	| EFunction (isrec,name,pl,e,rt) ->		
		let r = register_function ctx isrec name pl e rt p in
		type_functions ctx;
		r
	| EBinop (op,e1,e2) ->
		type_binop ctx op (type_expr ctx e1) (type_expr ctx e2) p
	| ETypeAnnot (e,t) ->
		let e = type_expr ctx e in
		let t = type_type ctx t p in
		unify ctx e.etype t (pos e);
		mk e.edecl t p
	| ETupleDecl [] ->
		mk (TConst TVoid) t_void p
	| ETupleDecl [e] ->
		let e = type_expr ctx e in
		mk (TParenthesis e) e.etype (pos e)
	| ETupleDecl el ->
		let el = List.map (type_expr ctx) el in
		mk (TTupleDecl el) (mk_tup ctx.gen (List.map (fun e -> e.etype) el)) p
	| ETypeDecl (params,tname,decl) ->
		let fullname = (match ctx.current.path with ["Core"] -> [tname] | p -> p @ [tname]) in
		let t , tl , h =
			try
				let t , tl , h = Hashtbl.find ctx.tmptypes tname in
				if decl <> EAbstract then Hashtbl.remove ctx.tmptypes tname;
				if List.length tl <> List.length params then error (Custom ("Invalid number of parameters for type " ^ tname)) p;
				t , tl , h
			with
				Not_found ->
					if Hashtbl.mem ctx.current.types tname then error (Custom ("Invalid type redefinition of type " ^ tname)) p;
					let h = Hashtbl.create 0 in
					let tl = List.map (fun p ->
						let t = t_mono ctx.gen in
						Hashtbl.add h p t;
						t
					) params in					
					let t = {
						tid = -1;
						texpr = TNamed (fullname,tl,t_abstract);
					} in
					Hashtbl.add ctx.current.types tname t;
					if decl = EAbstract then Hashtbl.add ctx.tmptypes tname (t,tl,h);
					t , tl , h
		in
		let t2 = (match decl with
			| EAbstract -> t_abstract
			| EAlias t -> type_type ~allow:false ~h ctx t p
			| ERecord fields ->
				let fields = List.map (fun (f,m,ft) ->
					let ft = type_type ~allow:false ~h ctx ft p in
					let m = (if m then Mutable else Immutable) in
					Hashtbl.add ctx.current.records f (t,ft,m);
					f , m , ft
				) fields in
				mk_record ctx.gen fields
			| EUnion constr ->
				let constr = List.map (fun (c,ft) ->
					let ft = (match ft with
						| None -> t_abstract
						| Some ft -> type_type ~allow:false ~h ctx ft p
					) in
					Hashtbl.add ctx.current.constrs c (t,ft);
					c , ft
				) constr in
				mk_union ctx.gen constr
		) in
		t.tid <- if t2.tid = -1 && params = [] then -1 else genid ctx.gen;
		t.texpr <- TNamed (fullname,tl,t2);
		polymorphize ctx.gen 0 t;
		mk (TTypeDecl t) t_void p
	| ERecordDecl fl ->
		let s , _ = (try List.hd fl with _ -> assert false) in
		let r , _ , _ = get_record ctx s p in
		let fll = (match tlinks false r with
			| TRecord fl -> fl
			| _ -> assert false
		) in
		let fl2 = ref fll in
		let rec loop f = function
			| [] -> 
				if List.exists (fun (f2,_,_) -> f = f2) fll then
					error (Custom ("Duplicate declaration for field " ^ f)) p
				else
					error (Have_no_field (r,f)) p
			| (f2,_,ft) :: l when f = f2 -> ft , l
			| x :: l -> 
				let t , l = loop f l in
				t , x :: l
		in
		let el = List.map (fun (f,e) ->
			let ft , fl2b = loop f !fl2 in
			fl2 := fl2b;
			let e = type_expr ctx e in
			unify ctx e.etype ft (pos e);
			(f , e)
		) fl in
		List.iter (fun (f,_,_) ->
			error (Custom ("Missing field " ^ f ^ " in record declaration")) p;
		) !fl2;
		mk (TRecordDecl el) r p
	| EErrorDecl (name,t) ->
		let t = (match t with None -> t_abstract | Some t -> type_type ~allow:false ctx t p) in
		Hashtbl.add ctx.current.constrs name (t_error,t);
		mk (TErrorDecl (name,t)) t_void p
	| EUnop (op,e) ->
		type_unop ctx op (type_expr ctx e) p
	| EMatch (e,cl) ->
		let e = type_expr ctx e in
		let is_stream = List.for_all (fun (l,_,_) -> List.for_all (fun (p,_) -> match p with PStream _ -> true | _ -> false) l) cl in
		let partial , m , t = type_match ctx e.etype cl p in
		if not is_stream && partial then error (Custom "This matching is not complete") p;
		mk (TMatch (e,m,is_stream)) t p
	| ETry (e,cl) ->
		let e = type_expr ctx e in
		let _ , m , t = type_match ctx t_error cl p in
		unify ctx t e.etype p;
		mk (TTry (e,m)) t p
	| ETupleGet (e,n) ->
		let e = type_expr ctx e in
		let try_unify et =
			let t = Array.init (n + 1) (fun _ -> t_mono ctx.gen) in
			unify ctx et (mk_tup ctx.gen (Array.to_list t)) p;
			t.(n)
		in
		let rec loop et =
			match et.texpr with
			| TLink et -> loop et
			| TTuple l -> (try List.nth l n with _ -> try_unify et)
			| _ -> try_unify et
		in
		mk (TTupleGet (e,n)) (loop e.etype) p

and type_block ctx ((e,p) as x)  = 
	match e with
	| EVar (vl,e) ->
		type_functions ctx;
		let e = type_expr ctx e in
		let make v t =
			let t = (match t with 
				| None -> t_mono ctx.gen
				| Some t -> type_type ctx t p
			) in
			add_local ctx v t;
			t
		in
		let t = (match vl with
			| [] -> assert false
			| [v,t] -> make v t
			| _ -> 
				mk_tup ctx.gen (List.map (fun (v,t) -> make v t) vl)
		) in
		unify ctx t e.etype (pos e);
		mk (TVar (List.map fst vl,e)) t_void p
	| EFunction (true,name,pl,e,rt) ->
		register_function ctx true name pl e rt p
	| _ ->
		type_functions ctx;
		type_expr ctx x

and type_pattern (ctx:context) h ?(h2 = Hashtbl.create 0) set add (pat,p) =
	let pvar add s =
		if SSet.mem s !set then error (Custom "This variable is several time in the pattern") p;
		set := SSet.add s !set;
		try
			Hashtbl.find h s
		with
			Not_found ->
				let t = t_mono ctx.gen in
				Hashtbl.add h s t;
				if add then add_local ctx s t;
				t
	in
	let pt , pat = (match pat with
		| PConst c ->
			(match c with
			| Int n -> t_int
			| Float s -> t_float
			| String s -> t_string
			| Char c -> t_char
			| Bool b -> t_bool
			| Ident _ | Constr _ | Module _ ->
				assert false) , pat
		| PTuple [p] ->
			let pt , pat = type_pattern ctx h ~h2 set add p in
			pt , fst pat
		| PTuple pl -> 
			let pl , patl = List.split (List.map (type_pattern ctx h ~h2 set add) pl) in
			mk_tup ctx.gen pl , PTuple patl
		| PRecord fl ->
			let s = (try fst (List.hd fl) with _ -> assert false) in
			let r , _ , _ = get_record ctx s p in
			let fl = (match tlinks false r with
			| TRecord rl ->
				List.map (fun (f,pat) ->
					let pt , pat = type_pattern ctx h ~h2 set add pat in
					let t = (try 
						let _ , _ , t = List.find (fun (f2,_,_) -> f = f2 ) rl in t 
					with Not_found ->
						error (Have_no_field (r,f)) p
					) in
					unify ctx pt t (snd pat);
					f , pat
				) fl 
			| _ ->
				assert false
			) in
			r , PRecord fl
		| PIdent s ->
			(if s = "_" then t_mono ctx.gen else pvar add s) , pat
		| PConstr (path,s,param) ->	
			let tparam , param = (match param with
				| None -> None , None 
				| Some ((_,p) as param) -> 
					let t , pat = type_pattern ctx h ~h2 set add param in
					Some (p,t) , Some pat
			) in
			let path , ut , t = get_constr ctx path s p in
			(match t.texpr , tparam with
			| TAbstract , None -> duplicate ctx.gen ut , PConstr (path,s,param)
			| TAbstract , Some _ -> error (Custom "Constructor does not take parameters") p
			| _ , None -> error (Custom "Constructor require parameters") p
			| _ , Some (p,pt) ->
				let h = Hashtbl.create 0 in
				let ut = duplicate ctx.gen ~h ut in
				let t = duplicate ctx.gen ~h t in
				let param , pt = (match param with 
					| Some (PTuple l,p) when not (is_tuple t) -> Some (PTuple [(PTuple l,p)],p) , mk_fun ctx.gen [pt] ut
					| Some (PIdent "_",p) -> param , pt
					| _  -> param , (match pt.texpr with TTuple l -> mk_fun ctx.gen l ut | _ -> mk_fun ctx.gen [pt] ut)
				) in
				let t = (match t.texpr with TTuple l -> mk_fun ctx.gen l ut | _ -> mk_fun ctx.gen [t] ut) in
				unify ctx t pt p;
				ut , PConstr (path,s,param));
		| PAlias (s,pat) ->
			let pt , pat = type_pattern ctx h ~h2 set false pat in
			let t = pvar false s in
			unify ctx pt t (snd pat);
			t , PAlias (s,pat)
		| PTyped (pat,t) ->
			let pt , pat = type_pattern ctx h ~h2 set add pat in
			unify ctx pt (type_type ~h:h2 ctx t p) p;
			pt , PTyped (pat,t)
		| PStream (l,k) ->
			let t , polyt = t_poly ctx.gen "stream" in
			let locals = save_locals ctx in
			let l = List.map (fun s ->
				match s with
				| SPattern pat ->
					let t , p = type_pattern ctx h ~h2 set true pat in
					unify ctx t polyt (snd p);
					SPattern p 
				| SExpr ([v],e) ->
					let e = type_expr ctx e in
					let t = pvar true v in
					unify ctx t e.etype e.epos;
					SMagicExpr((PIdent v,e.epos),Obj.magic e)
				| SExpr (vl,e) ->
					let e = type_expr ctx e in
					let tl = List.map (pvar true) vl in
					unify ctx (mk_tup ctx.gen tl) e.etype e.epos;
					let tup = PTuple (List.map (fun v -> PIdent v, e.epos) vl) in
					SMagicExpr((tup,e.epos),Obj.magic e)
				| SMagicExpr _ ->
					assert false
			) l in
			restore_locals ctx locals;
			t , PStream (l,k)
	) in	
	pt , (pat,p)
	
and type_match ctx t cl p =
	let ret = t_mono ctx.gen in
	let cl = List.map (fun (pl,wh,pe) ->
		let first = ref true in
		let h = Hashtbl.create 0 in
		let mainset = ref SSet.empty in
		let pl = List.map (fun pat ->
			let set = ref SSet.empty in
			let pt , pat = type_pattern ctx h set false pat in
			if !first then begin
				first := false;
				mainset := !set;
			end else begin
				let s1 = SSet.diff !set !mainset in
				let s2 = SSet.diff !mainset !set in
				SSet.iter (fun s -> error (Custom ("Variable " ^ s ^ " must occur in all patterns")) p) (SSet.union s1 s2);
			end;
			unify ctx pt t p;
			pat 
		) pl in
		let locals = save_locals ctx in
		Hashtbl.iter (fun v t -> add_local ctx v t) h;
		let wh = (match wh with 
			| None -> None
			| Some e -> 
				let e = type_expr ctx e in
				unify ctx e.etype t_bool e.epos;
				Some e
		) in
		let pe = type_expr ctx pe in
		unify ctx pe.etype ret (pos pe);
		restore_locals ctx locals;
		pl , wh , pe
	) cl in
	let rec loop cl =
		match cl with
		| TModule(path,TConstr c) :: l ->
			let path , ut , t = get_constr ctx path c null_pos in
			if ut == t_error then
				false
			else
			(match tlinks false ut with
			| TUnion (n,_) ->
				n = List.length cl
			| _ ->
				assert false)
		| TBool b :: l ->
			let e = List.exists (fun c -> c = TBool (not b)) l in
			prerr_endline (if e then "ok" else "notok");
			e
		| TVoid :: _ ->
			true
		| _ :: l ->
			loop cl
		| [] ->
			false
	in
	Mlmatch.fully_matched_ref := loop;
	let partial , m = Mlmatch.make cl p in
	partial , m , ret

let modules ctx =
	let h = Hashtbl.create 0 in
	Hashtbl.iter (fun p m -> 
		match m.expr with 
		| None -> ()
		| Some e -> 		
			let deps = ref (if m.path = ["Core"] || Hashtbl.mem m.deps ["Core"] then [] else [["Core"]]) in
			let idents = ref [] in
			Hashtbl.iter (fun _ m ->
				deps := m.path :: !deps
			) m.deps;
			PMap.iter (fun i t ->
				idents := i :: !idents
			) m.idents;
			Hashtbl.add h p (e,!deps,!idents)
	) ctx.modules;
	h

let open_file ctx file p =
	let rec loop = function
		| [] -> error (Custom ("File not found " ^ file)) p
		| p :: l ->
			try
				let f = p ^ file in
				f , open_in f
			with
				_ -> loop l
	in
	loop ctx.classpath

let load_module ctx m p =
	try
		Hashtbl.find ctx.modules m
	with
		Not_found ->			
			let file , ch = open_file ctx (String.concat "/" m ^ ".nml") p in
			let is_core , core = (try
				false , Hashtbl.find ctx.modules ["Core"]
			with Not_found ->
				true , ctx.current
			) in
			let ctx = {	ctx with
				tmptypes = Hashtbl.create 0;
				functions = [];
				opens = [core];
				current = (if is_core then ctx.current else {
					path = m;
					constrs = Hashtbl.create 0;
					records = Hashtbl.create 0;
					types = Hashtbl.create 0;
					expr = None;
					idents = PMap.empty;
					deps = Hashtbl.create 0;
				})
			} in
			Hashtbl.add ctx.modules m ctx.current;
			let ast = Mlparser.parse (Lexing.from_channel ch) file in
			if !verbose then print_endline ("Parsed " ^ file);
			let e = (match ast with
				| EBlock (e :: l) , p ->
					let e = type_block ctx e in
					let el , t = List.fold_left (fun (l,t) e ->
						let e = type_block ctx e in
						e :: l , e.etype
					) ([e] , e.etype) l in
					type_functions ctx;
					mk (TBlock (List.rev el)) t p
				| _ ->
					type_expr ctx ast
			) in
			ctx.current.expr <- Some e;			
			if !verbose then print_endline ("Typing done with " ^ file);
			ctx.current

let context cpath = 
	let ctx = {
		gen = generator();
		tmptypes = Hashtbl.create 0;
		modules = Hashtbl.create 0;
		functions = [];
		opens = [];
		mink = 0;
		classpath = cpath;
		curfunction = "anonymous";
		current = {
			path = ["Core"];
			expr = None;
			idents = PMap.empty;
			constrs = Hashtbl.create 0;
			types = Hashtbl.create 0;
			deps = Hashtbl.create 0;
			records = Hashtbl.create 0;
		};
	} in
	let add_type args name t = 
		ignore(type_expr ctx (ETypeDecl (args,name,t) , null_pos));
	in
	let add_variable name t = 
		ctx.current.idents <- PMap.add name t ctx.current.idents
	in
	add_type [] "bool" EAbstract;
	add_type ["a"] "list" (EUnion ["[]",None;"::",Some (ETuple [
		EPoly "a";
		EType (Some (EPoly "a"),[],"list");
	])]);
	add_variable "neko" (mk_fun ctx.gen [t_polymorph ctx.gen] (t_polymorph ctx.gen));
	let core = load_module ctx ["Core"] null_pos in
	ctx

;;
Mlmatch.error_ref := (fun msg p -> error (Custom msg) p);
load_module_ref := load_module