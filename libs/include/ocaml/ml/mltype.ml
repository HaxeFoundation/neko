
type pos = Mlast.pos

type mutflag =
	| Mutable
	| Immutable

type type_expr = 
	| TAbstract
	| TMono
	| TPoly
	| TRecord of (string * mutflag * t) list
	| TUnion of (string * t) list
	| TTuple of t list
	| TLink of t
	| TFun of t list * t
	| TNamed of string * t list * t

and t = {
	mutable tid : int;
	mutable texpr : type_expr;
}

type tconstant =
	| TTrue
	| TFalse
	| TInt of int
	| TFloat of string
	| TString of string
	| TVar of string

type texpr_decl =
	| TConst of tconstant
	| TNext of texpr * texpr
	| TParenthesis of texpr
	| TCall of texpr * texpr list
	| TField of texpr * string
	| TArray of texpr * texpr
	| TVars of (string * texpr) list
	| TIf of texpr * texpr * texpr option
	| TFunction of string list * texpr
	| TBinop of string * texpr * texpr

and texpr = {
	edecl : texpr_decl;
	etype : t;
	epos : pos;
}

type id_gen = int ref

let pos e = e.epos

let rec tlinks t =
	match t.texpr with
	| TLink t -> tlinks t
	| _ -> t.texpr

let etype e = tlinks e.etype

let genid i = incr i; !i
let generator() = ref 0

let mk e t p = {
	edecl = e;
	etype = t;
	epos = p;
}

let abstract s = {
	tid = -1;
	texpr = TNamed (s,[], { tid = -1; texpr = TAbstract });
}

let t_void = abstract "void"
let t_int = abstract "int"
let t_float = abstract "float"
let t_bool = abstract "bool"
let t_string = abstract "string"
let t_char = abstract "char"

let t_mono() = {
	tid = -1;
	texpr = TMono;
}

let t_poly g name = 
	let param = t_mono() in
	{
		tid = -1;
		texpr = TNamed (name,[param], { tid = -1; texpr = TAbstract });
	} , param

let mk_fun g params ret = {
	tid = if List.exists (fun t -> t.tid <> -1) (ret :: params) then genid g else -1;
	texpr = TFun (params,ret);
}

type print_infos = {
	mutable pi_mcount : int;
	mutable pi_pcount : int;
	mutable pi_ml : (t * int) list;
	mutable pi_ph : (int , int) Hashtbl.t;
}

let s_context() = {
	pi_mcount = 0;
	pi_pcount = 0;
	pi_ml = [];
	pi_ph = Hashtbl.create 0;
}

let poly_id n =
	if n < 26 then
		String.make 1 (char_of_int (int_of_char 'a' + n))
	else
		string_of_int (n - 25)

let s_mutable = function
	| Mutable -> "mutable "
	| Immutable -> ""

let rec s_type ?(h=s_context()) t = 
	match t.texpr with
	| TAbstract -> "<abstract>"
	| TMono -> Printf.sprintf "'_%s" (poly_id (try
			List.assq t h.pi_ml
		with Not_found -> 
			let k = h.pi_mcount in
			h.pi_mcount <- h.pi_mcount + 1;
			h.pi_ml <- (t,k) :: h.pi_ml;
			k))
	| TPoly -> Printf.sprintf "'%s" (poly_id (try
			if t.tid = -1 then assert false;
			Hashtbl.find h.pi_ph t.tid
		with Not_found -> 
			let k = h.pi_pcount in
			h.pi_pcount <- h.pi_pcount + 1;
			Hashtbl.add h.pi_ph t.tid k;
			k))
	| TRecord fl -> Printf.sprintf "{ %s }" (String.concat "; " (List.map (fun (f,m,t) -> s_mutable m ^ f ^ " : " ^ s_type ~h t) fl))
	| TUnion fl -> Printf.sprintf "{ %s }" (String.concat " | " (List.map (fun (f,t) -> f ^ " : " ^ s_type ~h t) fl))
	| TTuple l -> Printf.sprintf "(%s)" (String.concat ", " (List.map (s_type ~h) l))
	| TLink t  -> s_type ~h t
	| TFun (tl,r) -> 
		let l = String.concat " -> " (List.map (s_fun ~h) tl) ^ " -> " in
		l ^ s_type ~h r
	| TNamed (name,params,t) ->
		let s = (match params with
			| [] -> ""
			| [p] -> s_type ~h p ^ " "
			| l -> "(" ^ String.concat " " (List.map (s_type ~h) l) ^ ") ")
		in
		s ^ name

and s_fun ~h t =
	match t.texpr with
	| TLink t -> s_fun ~h t
	| TFun _ -> "(" ^ s_type ~h t ^ ")"
	| _ -> s_type ~h t

let rec is_recursive t1 t2 = 
	if t1 == t2 then
		true
	else match t2.texpr with
	| TAbstract
	| TMono
	| TPoly ->
		false
	| TRecord tl -> List.exists (fun (_,_,t) -> is_recursive t1 t) tl
	| TUnion tl -> List.exists (fun (_,t) -> is_recursive t1 t) tl
	| TTuple tl -> List.exists (is_recursive t1) tl
	| TLink t -> is_recursive t1 t
	| TFun (tl,t) -> List.exists (is_recursive t1) tl || is_recursive t1 t
	| TNamed (_,p,t) -> List.exists (is_recursive t1) p || is_recursive t1 t

let rec duplicate g ?(h=Hashtbl.create 0) t =
	if t.tid = -1 then
		t
	else try
		Hashtbl.find h t.tid
	with Not_found ->
		let t2 = {
			tid = genid g;
			texpr = TAbstract;
		} in
		Hashtbl.add h t.tid t2;
		t2.texpr <- (match t.texpr with
			| TAbstract -> TAbstract
			| TMono -> assert false
			| TPoly -> TPoly
			| TRecord tl -> TRecord (List.map (fun (n,m,t) -> n , m, duplicate g ~h t) tl)
			| TUnion tl -> TUnion (List.map (fun (n,t) -> n , duplicate g ~h t) tl)
			| TTuple tl -> TTuple (List.map (duplicate g ~h) tl)
			| TLink t -> TLink (duplicate g ~h t)
			| TFun (tl,t) -> TFun (List.map (duplicate g ~h) tl, duplicate g ~h t)
			| TNamed (n,p,t) -> TNamed (n,List.map (duplicate g ~h) p,duplicate g ~h t));
		t2

let rec polymorphize g t =
	if t.tid = -1 then t.tid <- genid g;
	match t.texpr with
	| TAbstract -> ()
	| TMono -> t.texpr <- TPoly
	| TPoly -> ()
	| TRecord fl -> List.iter (fun (_,_,t) -> polymorphize g t) fl
	| TUnion fl -> List.iter (fun (_,t) -> polymorphize g t) fl
	| TTuple tl -> List.iter (polymorphize g) tl
	| TLink t -> polymorphize g t
	| TFun (tl,t) -> List.iter (polymorphize g) tl; polymorphize g t
	| TNamed (_,tl,_) -> List.iter (polymorphize g) tl

