open Mlast
open Mltype

type context = {
	mutable idents : (string,t) PMap.t;
	gen : id_gen;
	records : (string,t * t) Hashtbl.t;
	types : (string,t) Hashtbl.t;
}

type error_msg =
	| Invalid_expression 
	| Cannot_unify of t * t
	| Have_no_field of t * string
	| Stack of error_msg * error_msg
	| Custom of string

exception Error of error_msg * pos

let rec error_msg ?(h=s_context()) = function
	| Invalid_expression -> "Invalid expression"
	| Cannot_unify (t1,t2) -> "Cannot unify " ^ s_type ~h t1 ^ " and " ^ s_type ~h t2
	| Have_no_field (t,f) -> s_type ~h t ^ " have no field " ^ f
	| Stack (m1,m2) -> error_msg ~h m1 ^ "\n  " ^ error_msg ~h m2
	| Custom s -> s

let error m p = raise (Error (m,p))

let link ctx t1 t2 p =
	if is_recursive t1 t2 then error (Cannot_unify (t1,t2)) p;
	t1.texpr <- TLink t2;
	if t1.tid = -1 then begin
		if t2.tid <> -1 then t1.tid <- genid ctx.gen;
	end else
		if t2.tid = -1 then t1.tid <- -1

let record_field ctx f =
	let rt , ft = Hashtbl.find ctx.records f in
	let h = Hashtbl.create 0 in
	duplicate ctx.gen ~h rt, duplicate ctx.gen ~h ft

let unify_stack t1 t2 = function
	| Error (Cannot_unify _ as e , p) -> error (Stack (e , Cannot_unify (t1,t2))) p
	| e -> raise e

let rec unify ctx t1 t2 p =
	if t1 == t2 then
		()
	else match t1.texpr , t2.texpr with
	| TLink t , _ -> unify ctx t t2 p
	| _ , TLink t -> unify ctx t1 t p
	| TMono , t
	| TPoly , t -> link ctx t1 t2 p
	| t , TMono
	| t , TPoly -> link ctx t2 t1	p
	| TTuple tl1 , TTuple tl2 when List.length tl1 = List.length tl2 ->
		(try
			List.iter2 (fun t1 t2 -> unify ctx t1 t2 p) tl1 tl2
		with	
			e -> unify_stack t1 t2 e)
	| TFun (tl1,r1) , TFun (tl2,r2) when List.length tl1 = List.length tl2 -> 
		(try
			List.iter2 (fun t1 t2 -> unify ctx t1 t2 p) tl1 tl2; 
			unify ctx r1 r2 p;
		with	
			e -> unify_stack t1 t2 e)
	| TNamed (n1,p1,_) , TNamed (n2,p2,_) when n1 = n2 ->
		(try
			List.iter2 (fun p1 p2 -> unify ctx p1 p2 p) p1 p2
		with	
			e -> unify_stack t1 t2 e)
	| _ , _ ->
		error (Cannot_unify (t1,t2)) p

let rec type_type ?(h=Hashtbl.create 0) ctx t p =
	match t with
	| ETuple el ->
		mk_tup ctx.gen (List.map (fun t -> type_type ~h ctx t p) el)
	| EPoly s ->
		(try
			Hashtbl.find h s
		with
			Not_found ->
				let t = {
					tid = genid ctx.gen;
					texpr = TPoly
				} in
				Hashtbl.add h s t;
				t)
	| EType (params,name) ->
		let tl = List.map (fun t -> type_type ~h ctx t p) params in
		try
			let t = Hashtbl.find ctx.types name in
			(match t.texpr with
			| TNamed (_,params,t2) ->
				let tl = (if List.length tl = List.length params then
					tl
				else match tl with (* resolve ambiguity *)
					| [{ texpr = TTuple tl }] when List.length tl = List.length params -> tl
					| _ -> error (Custom ("Invalid number of type parameters for " ^ name)) p) in
				let h = Hashtbl.create 0 in
				let t = duplicate ctx.gen ~h t in
				let params = List.map (duplicate ctx.gen ~h) params in
				List.iter2 (fun pa t -> unify ctx pa t p) params tl;
				t
			| _ -> assert false)
		with
			Not_found -> error (Custom ("Unknown type " ^ name)) p

let type_constant ctx c p =
	match c with
	| True -> mk (TConst TTrue) t_bool p 
	| False -> mk (TConst TFalse) t_bool p
	| Int i -> mk (TConst (TInt i)) t_int p
	| Float s -> mk (TConst (TFloat s)) t_float p
	| String s -> mk (TConst (TString s)) t_string p
	| Ident s ->
		(try
			let t = PMap.find s ctx.idents in
			mk (TConst (TIdent s)) (duplicate ctx.gen t) p
		with
			Not_found -> error (Custom ("Unknown variable " ^ s)) p)
	| Constr s ->
		assert false

type addable = NInt | NFloat | NString | NNan

let addable str e =
	match etype e with
	| TNamed ("int",_,_) -> NInt
	| TNamed ("foat",_,_) -> NFloat
	| TNamed ("string",_,_) when str -> NString
	| _ -> NNan

let type_binop ctx op e1 e2 p =
	let emk t = mk (TBinop (op,e1,e2)) t p in
	match op with
	| ":=" ->
		let t , pt = t_poly ctx.gen "ref" in
		unify ctx e2.etype pt (pos e2);
		unify ctx e1.etype t (pos e1);
		emk t_void
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
	| "&"
	| "|"
	| "^" ->
		unify ctx e1.etype t_int (pos e1);
		unify ctx e2.etype t_int (pos e2);
		emk t_int
	| "&&"
	| "||" ->
		unify ctx e1.etype t_bool (pos e1);
		unify ctx e2.etype t_bool (pos e2);
		emk t_bool
	| "="
	| "<"
	| "<="
	| ">"
	| ">="
	| "==" ->
		unify ctx e2.etype e1.etype (pos e2);
		emk t_bool
	| _ ->
		error (Custom ("Invalid operation " ^ op)) p

let rec type_expr ctx (e,p) =
	match e with
	| EConst c -> type_constant ctx c p
	| EBlock [] -> error Invalid_expression p
	| EBlock (e :: l) ->
		let idents = ctx.idents in
		let e = List.fold_left (fun acc e ->
			let e = type_block ctx e in
			mk (TNext (acc,e)) e.etype (punion (pos acc) (pos e))
		) (type_block ctx e) l in
		ctx.idents <- idents;
		e
	| EParenthesis e ->
		let e = type_expr ctx e in
		mk (TParenthesis e) e.etype (pos e)
	| ECall ((EConst (Constr "TYPE"),_),[e]) ->
		let e = type_expr ctx e in
		prerr_endline (s_type e.etype);
		e
	| ECall (e,el) ->
		let e = type_expr ctx e in
		let el = List.map (type_expr ctx) el in
		(match etype e with
		| TFun (args,r) ->
			let rec loop l tl r =
				match l , tl with
				| e :: l , t :: tl ->
					unify ctx e.etype t (pos e);
					loop l tl r
				| [] , [] ->
					mk (TCall (e,el)) r p
				| [] , tl ->
					mk (TCall (e,el)) (mk_fun ctx.gen tl r) p
				| el , [] ->
					match tlinks r with
					| TFun (args,r) -> loop el args r
					| _ -> error (Custom "Too many arguments") p
			in
			loop el args r
		| _ ->
			let r = t_mono() in
			let f = mk_fun ctx.gen (List.map (fun e -> e.etype) el) r in
			unify ctx e.etype f p;
			mk (TCall (e,el)) r p
		);
	| EField (e,s) ->
		let e = type_expr ctx e in
		let t = (match etype e with
		| TRecord fl ->
			(try
				let _ , _ , t = List.find (fun (s2,_,_) -> s = s2) fl in
				t
			with
				Not_found -> error (Have_no_field (e.etype,s)) p)
		| _ ->
			try
				let r , t = record_field ctx s in
				unify ctx e.etype r (pos e);
				t
			with
				Not_found -> error (Custom ("Unknown field " ^ s)) p
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
	| EFunction (pl,e,rt) ->
		let idents = ctx.idents in
		let h = Hashtbl.create 0 in
		let el = List.map (fun (pn,pt) ->
			let t = t_mono() in
			(match pt with
			| None -> ()
			| Some pt ->
				let pt = type_type ~h ctx pt p in
				monomorphize pt;
				unify ctx t pt p);
			ctx.idents <- PMap.add pn t ctx.idents;
			pn , t
		) pl in
		let e = type_expr ctx e in
		(match rt with 
		| None -> () 
		| Some rt -> 
			let rt = type_type ctx rt p in
			unify ctx e.etype rt p);
		List.iter (fun (_,t) -> polymorphize ctx.gen t) el;
		polymorphize ctx.gen e.etype;
		ctx.idents <- idents;
		mk (TFunction (el,e)) (mk_fun ctx.gen (List.map snd el) e.etype) p
	| EBinop (op,e1,e2) ->
		type_binop ctx op (type_expr ctx e1) (type_expr ctx e2) p
	| ETypeAnnot (e,t) ->
		let e = type_expr ctx e in
		let t = type_type ctx t p in
		unify ctx e.etype t (pos e);
		mk e.edecl t p
	| ETupleDecl el ->
		let el = List.map (type_expr ctx) el in
		mk (TTupleDecl el) (mk_tup ctx.gen (List.map (fun e -> e.etype) el)) p

and type_block ctx ((e,p) as x)  = 
	match e with
	| EVar (v,t,e) ->
		let e = type_expr ctx e in
		(match t with
		| None -> ()
		| Some t ->
			let t = type_type ctx t p in
			unify ctx t e.etype (pos e));
		ctx.idents <- PMap.add v e.etype ctx.idents;
		mk (TVar (v,e)) t_void p
	| _ ->
		type_expr ctx x

let context() = 
	let ctx = {
		idents = PMap.empty;
		gen = generator();
		records = Hashtbl.create 0;
		types = Hashtbl.create 0;
	} in
	Hashtbl.add ctx.types "int" t_int;
	Hashtbl.add ctx.types "float" t_float;
	let t = fst (t_poly ctx.gen "ref") in
	polymorphize ctx.gen t;
	Hashtbl.add ctx.types "ref" t;
	ctx

let type_file ctx file =
	let ch = open_in file in
	let ast = Mlparser.parse (Lexing.from_channel ch) file in
	type_expr ctx ast
