open Mlast
open Mltype
open Mlmatch

type module_context = {
	path : string list;
	types : (string,t) Hashtbl.t;
	constr : (string, t * t) Hashtbl.t;
	mutable expr : texpr option;
	mutable midents : (string,t) PMap.t;
}

type context = {
	mutable idents : (string,t) PMap.t;
	mutable functions : (string * texpr ref * t * (string * t) list * expr * t * pos) list;
	gen : id_gen;
	records : (string,t * t * mutflag) Hashtbl.t;
	tmptypes : (string, t * t list * (string,t) Hashtbl.t) Hashtbl.t;
	current : module_context;
	modules : (string list, module_context) Hashtbl.t;
	cpath : string list;
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

let load_module_ref = ref (fun _ _ -> assert false)

let get_module ctx path p =
	match path with
	| [] -> ctx.current
	| _ -> 
		try
			Hashtbl.find ctx.modules path
		with
			Not_found -> 
				!load_module_ref ctx path p

let get_type ctx path name p =
	let m = get_module ctx path p in
	try
		Hashtbl.find m.types name
	with
		Not_found -> 
			if m.expr = None then error (Module_not_loaded m) p;
			error (Custom ("Unknown type " ^ s_path path name)) p

let get_constr ctx path name p =
	let m = get_module ctx path p in
	try
		let ut , t = Hashtbl.find m.constr name in
		m.path , ut , t
	with
		Not_found -> 
			if m.expr = None then error (Module_not_loaded m) p;		
			error (Custom ("Unknown constructor " ^ s_path path name)) p

let get_ident ctx path name p =
	let m = get_module ctx path p in
	try
		PMap.find name (if path = [] then ctx.idents else m.midents)
	with
		Not_found -> 
			if m.expr = None then error (Module_not_loaded m) p;
			error (Custom ("Unknown variable " ^ s_path path name)) p

let link ctx t1 t2 p =
	if is_recursive t1 t2 then error (Cannot_unify (t1,t2)) p;
	t1.texpr <- TLink t2;
	if t1.tid < 0 then begin
		if t2.tid = -1 then t1.tid <- -1 else t1.tid <- genid ctx.gen;
	end else
		if t2.tid = -1 then t1.tid <- -1

let record_field ctx f p =
	let rt , ft , mut = (try Hashtbl.find ctx.records f with Not_found -> error (Unknown_field f) p) in
	let h = Hashtbl.create 0 in
	duplicate ctx.gen ~h rt, duplicate ctx.gen ~h ft, mut

let unify_stack t1 t2 = function
	| Error (Cannot_unify _ as e , p) -> error (Stack (e , Cannot_unify (t1,t2))) p
	| e -> raise e

let is_alias = function
	| TAbstract 
	| TRecord _
	| TUnion _ -> false
	| TMono
	| TPoly
	| TTuple _
	| TLink _
	| TFun _
	| TNamed _ -> true

let rec unify ctx t1 t2 p =
	if t1 == t2 then
		()
	else match t1.texpr , t2.texpr with
	| TLink t , _ -> unify ctx t t2 p
	| _ , TLink t -> unify ctx t1 t p
	| TMono , t
	| TPoly , t -> link ctx t1 t2 p
	| t , TMono
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
				let t = t_mono() in
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
	| EArrow (ta,tb) ->
		let ta = type_type ~allow ~h ctx ta p in
		let tb = type_type ~allow ~h ctx tb p in
		match ta.texpr with
		| TFun (params,r) -> mk_fun ctx.gen (params @ [r]) tb
		| _ -> mk_fun ctx.gen [ta] tb

let rec type_constant ctx ?(path=[]) c p =
	match c with
	| Int i -> mk (TConst (TInt i)) t_int p
	| Float s -> mk (TConst (TFloat s)) t_float p
	| String s -> mk (TConst (TString s)) t_string p
	| Ident s ->
		let t = get_ident ctx path s p in
		mk (TConst (TIdent s)) (duplicate ctx.gen t) p
	| Constr s ->
		let path , ut , t = get_constr ctx path s p in
		let t = duplicate ctx.gen (match t.texpr with
			| TAbstract -> ut
			| _ -> mk_fun ctx.gen [t] ut) in
		mk (TConst (TModule (path,TConstr s))) t p
	| Module (path,c) ->
		type_constant ctx ~path c p

type addable = NInt | NFloat | NString | NNan

let addable str e =
	match etype true e with
	| TNamed ("int",_,_) -> NInt
	| TNamed ("float",_,_) -> NFloat
	| TNamed ("string",_,_) when str -> NString
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
	| "="
	| "<"
	| "<="
	| ">"
	| ">="
	| "=="
	| "<>"
	| "!=" ->
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
		emk t
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

let register_function ctx name pl e rt p =
	let pl = (match pl with [] -> ["_",Some (EType (None,[],"void"))] | _ -> pl) in
	let expr = ref (mk (TConst TVoid) t_void p) in
	let h = Hashtbl.create 0 in
	let el = List.map (fun (pn,pt) ->
		let t = (match pt with
			| None -> t_mono()
			| Some pt -> type_type ~h ctx pt p
		) in
		pn , t
	) pl in
	let rt = (match rt with 
		| None -> t_mono() 
		| Some rt -> type_type ~h ctx rt p
	) in
	let ft = mk_fun ctx.gen (List.map snd el) rt in
	let name = (match name with None -> "_" | Some n -> n) in
	ctx.functions <- (name,expr,ft,el,e,rt,p) :: ctx.functions;
	if name <> "_" then ctx.idents <- PMap.add name ft ctx.idents;
	mk (TMut expr) ft p

let rec type_functions ctx =
	let l = ctx.functions in
	ctx.functions <- [];
	let l = List.map (fun (name,expr,ft,el,e,rt,p) ->
		let idents = ctx.idents in
		List.iter (fun (p,pt) ->
			if p <> "_" then ctx.idents <- PMap.add p pt ctx.idents;
		) el;
		let e = type_expr ctx e in
		ctx.idents <- idents;
		let ft2 = mk_fun ctx.gen (List.map snd el) e.etype in
		unify ctx ft ft2 p;
		expr := mk (TFunction (name,el,e)) ft2 p;
		ft2
	) (List.rev l) in
	List.iter (polymorphize ctx.gen) l

and type_expr ctx (e,p) =
	match e with
	| EConst c ->
		type_constant ctx c p
	| EBlock [] -> 
		mk (TConst TVoid) t_void p
	| EBlock (e :: l) ->
		let idents = ctx.idents in
		let e = type_block ctx e in
		let el , t = List.fold_left (fun (l,t) e ->
			let e = type_block ctx e in
			e :: l , e.etype
		) ([e] , e.etype) l in
		type_functions ctx;
		ctx.idents <- idents;
		mk (TBlock (List.rev el)) t p
	| ECall ((EConst (Constr "TYPE"),_),[e]) ->
		let e = type_expr ctx e in
		prerr_endline ("type : " ^ s_type e.etype);
		e
	| ECall (e,el) ->
		let e = type_expr ctx e in
		let el = (match el with [] -> [ETupleDecl [],p] | _ -> el) in
		let el = List.map (type_expr ctx) el in
		(match etype false e with
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
					match tlinks false r with
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
		let t = (match etype false e with
		| TRecord fl ->
			(try
				let _ , _ , t = List.find (fun (s2,_,_) -> s = s2) fl in
				t
			with
				Not_found -> error (Have_no_field (e.etype,s)) p)
		| _ ->
			let r , t , _ = record_field ctx s p in
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
	| EFunction (name,pl,e,rt) ->
		let r = register_function ctx name pl e rt p in
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
						let t = t_mono() in
						Hashtbl.add h p t;
						t
					) params in
					let t = {
						tid = genid ctx.gen;
						texpr = TNamed (s_path ctx.current.path tname,tl,t_abstract);
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
					Hashtbl.add ctx.records f (t,ft,m);
					f , m , ft
				) fields in
				mk_record ctx.gen fields
			| EUnion constr ->
				let constr = List.map (fun (c,ft) ->
					let ft = (match ft with
						| None -> t_abstract
						| Some ft -> type_type ~allow:false ~h ctx ft p
					) in
					Hashtbl.add ctx.current.constr c (t,ft);
					c , ft
				) constr in
				mk_union ctx.gen constr
		) in
		t.tid <- if t2.tid = -1 && params = [] then -1 else genid ctx.gen;
		t.texpr <- TNamed (s_path ctx.current.path tname,tl,t2);
		polymorphize ctx.gen t;
		mk (TTypeDecl t) t_void p
	| ERecordDecl fl ->
		let s , _ = (try List.hd fl with _ -> assert false) in
		let r , _ , _ = record_field ctx s p in
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
	| EListDecl el ->
		let t , pt = t_poly ctx.gen "list" in
		let el = List.map (fun e ->
			let e = type_expr ctx e in
			unify ctx e.etype pt (pos e);
			e
		) el in
		mk (TListDecl el) t p
	| EUnop (op,e) ->
		type_unop ctx op (type_expr ctx e) p
	| EMatch (e,cl) ->
		type_match ctx (type_expr ctx e) cl p

and type_block ctx ((e,p) as x)  = 
	match e with
	| EVar (v,t,e) ->
		type_functions ctx;
		let e = type_expr ctx e in
		(match t with
		| None -> ()
		| Some t ->
			let t = type_type ctx t p in
			unify ctx t e.etype (pos e));
		if v <> "_" then ctx.idents <- PMap.add v e.etype ctx.idents;
		mk (TVar (v,e)) t_void p
	| EFunction (name,pl,e,rt) ->
		register_function ctx name pl e rt p
	| _ ->
		type_functions ctx;
		type_expr ctx x

and type_pattern (ctx:context) h ?(h2 = Hashtbl.create 0) set (pat,p,t) =
	let pvar s =
		if SSet.mem s !set then error (Custom "This variable is several time in the pattern") p;
		set := SSet.add s !set;
		try
			Hashtbl.find h s
		with
			Not_found ->
				let t = t_mono() in
				Hashtbl.add h s t;
				t
	in
	let pat , pt = (match pat with
		| PConst c ->
			let c , t = (match c with
				| Int n -> TInt n , t_int
				| Float s -> TFloat s , t_float
				| String s -> TString s , t_string
				| Ident s -> 
					TIdent s , (if s = "_" then t_mono() else pvar s)
				| Constr _ | Module _ ->
					assert false
			) in
			TPConst c , t
		| PTuple pl -> 
			let pl = List.map (type_pattern ctx h ~h2 set) pl in
			TPTuple pl , mk_tup ctx.gen (List.map (fun (_,_,t) -> t) pl)
		| PRecord fl ->
			let s = (try fst (List.hd fl) with _ -> assert false) in
			let r , _ , _ = record_field ctx s p in
			let fl = (match tlinks false r with
				| TRecord rl ->
					List.map (fun (f,pat) ->
						let pat , pp , pt = type_pattern ctx h ~h2 set pat in
						let t = (try 
							let _ , _ , t = List.find (fun (f2,_,_) -> f = f2 ) rl in t 
						with Not_found ->
							error (Have_no_field (r,f)) p
						) in
						unify ctx pt t pp;
						f , (pat , pp, pt)
					) fl 
				| _ ->
					assert false
			) in
			TPRecord fl , r
		| PConstr (path,s,param) ->
			let param = (match param with None -> None | Some param -> Some (type_pattern ctx h ~h2 set param)) in
			let path , ut , t = get_constr ctx path s p in
			let t = (match t.texpr , param with
				| TAbstract , None -> duplicate ctx.gen ut
				| TAbstract , Some _ -> error (Custom "Constructor does not take parameters") p
				| _ , None -> error (Custom "Constructor require parameters") p
				| _ , Some (pat , pp, pt) -> 
					match pt with
					| { texpr = TTuple [t2] } | t2 -> 
						let h = Hashtbl.create 0 in
						let ut = duplicate ctx.gen ~h ut in
						let t = duplicate ctx.gen ~h t in
						unify ctx t t2 pp;
						ut
			) in
			TPConstr (path,s,param) , t
		| PAlias (s,pat) ->
			let pat , pp , pt = type_pattern ctx h ~h2 set pat in
			let t = pvar s in
			unify ctx pt t pp;
			TPAlias (s,(pat,pp,pt)) , t
		| PList pl ->
			let tl , t = t_poly ctx.gen "list" in
			let pl = List.map (fun p ->
				let pat , pp , pt = type_pattern ctx h ~h2 set p in
				unify ctx pt t pp;
				pat , pp , pt
			) pl in
			TPList pl , tl
	) in
	(match t with 
	| None -> ()
	| Some t -> unify ctx pt (type_type ~h:h2 ctx t p) p);
	pat , p , pt
	
and type_match ctx e cl p =
	let ret = t_mono() in
	let cl = List.map (fun (pl,wh,pe) ->
		let first = ref true in
		let h = Hashtbl.create 0 in
		let mainset = ref SSet.empty in
		let pl = List.map (fun pat ->
			let set = ref SSet.empty in
			let pat , p , pt = type_pattern ctx h set pat in
			if !first then begin
				first := false;
				mainset := !set;
			end else begin
				let s1 = SSet.diff !set !mainset in
				let s2 = SSet.diff !mainset !set in
				SSet.iter (fun s -> error (Custom ("Variable " ^ s ^ " must occur in all patterns")) p) (SSet.union s1 s2);
			end;
			unify ctx pt e.etype p;
			pat , p , pt
		) pl in
		let idents = ctx.idents in
		Hashtbl.iter (fun v t ->
			ctx.idents <- PMap.add v t ctx.idents
		) h;
		let wh = (match wh with None -> None | Some e -> Some (type_expr ctx e)) in
		let pe = type_expr ctx pe in
		unify ctx pe.etype ret (pos pe);
		ctx.idents <- idents;
		pl , wh , pe
	) cl in
	let tree = Mlmatch.make cl e.etype p in
	mk (TMatch (e,tree)) ret p

let context cpath = 
	let ctx = {
		idents = PMap.empty;
		gen = generator();
		records = Hashtbl.create 0;
		tmptypes = Hashtbl.create 0;
		modules = Hashtbl.create 0;
		functions = [];
		cpath = cpath;
		current = {
			midents = PMap.empty;
			path = [];
			constr = Hashtbl.create 0;
			types = Hashtbl.create 0;
			expr = None;
		};
	} in
	let poly name =
		let t , _ = t_poly ctx.gen name in
		polymorphize ctx.gen t;
		t
	in
	Hashtbl.add ctx.current.types "void" t_void;
	Hashtbl.add ctx.current.types "int" t_int;
	Hashtbl.add ctx.current.types "float" t_float;
	Hashtbl.add ctx.current.types "bool" t_bool;
	Hashtbl.add ctx.current.types "string" t_string;
	Hashtbl.add ctx.current.types "ref" (poly "ref");
	Hashtbl.add ctx.current.types "array" (poly "array");
	let list , l = t_poly ctx.gen "list" in
	polymorphize ctx.gen list;
	Hashtbl.add ctx.current.types "list" list;
	Hashtbl.add ctx.current.constr "::" (list,mk_tup ctx.gen [l;list]);
	Hashtbl.add ctx.current.constr "true" (t_bool,t_abstract);
	Hashtbl.add ctx.current.constr "false" (t_bool,t_abstract);
	Hashtbl.add ctx.modules [] ctx.current;
	ctx

let modules ctx =
	let h = Hashtbl.create 0 in
	Hashtbl.iter (fun p m -> 
		match m.expr with 
		| None -> ()
		| Some e -> Hashtbl.add h p e
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
	loop ctx.cpath

let load_module ctx m p =
	try
		Hashtbl.find ctx.modules m
	with
		Not_found ->			
			let file , ch = open_file ctx (String.concat "/" m ^ ".nml") p in
			let base = Hashtbl.find ctx.modules [] in
			let ctx = {	ctx with
				idents = PMap.empty;
				records = Hashtbl.create 0;
				tmptypes = Hashtbl.create 0;
				functions = [];
				current = {
					path = m;
					constr = Hashtbl.copy base.constr;
					types = Hashtbl.copy base.types;
					expr = None;
					midents = PMap.empty;
				}
			} in
			Hashtbl.add ctx.modules m ctx.current;
			let ast = Mlparser.parse (Lexing.from_channel ch) file in
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
			ctx.current.midents <- ctx.idents;
			ctx.current

;;
Mlmatch.error_fun := (fun msg p -> error (Custom msg) p);
load_module_ref := load_module