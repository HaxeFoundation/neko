open Mlast

type error_msg =
	| Unexpected of token
	| Unclosed of string * token
	| Duplicate_default
	| Unknown_macro of string
	| Invalid_macro_parameters of string * int

exception Error of error_msg * pos

let error_msg = function
	| Unexpected t -> "Unexpected " ^ s_token t
	| Unclosed (s,t) -> "Unexpected " ^ s_token t ^ ", maybe unclosed " ^ s
	| Duplicate_default -> "Duplicate default declaration"
	| Unknown_macro m -> "Unknown macro " ^ m
	| Invalid_macro_parameters (m,n) -> "Invalid number of parameters for macro " ^ m ^ " : " ^ string_of_int n ^ " required"

let error m p = raise (Error (m,p))

let priority = function
	| "=" | "+=" | "-=" | "*=" | "/=" | "|=" | "&=" | "^=" -> -3
	| "&&" | "||" -> -2
	| "==" | "!=" | ">" | "<" | "<=" | ">=" -> -1
	| "+" | "-" -> 0
	| "*" | "/" -> 1
	| "|" | "&" | "^" -> 2
	| "<<" | ">>" | "%" | ">>>" -> 3
	| _ -> 4

let rec make_binop op e ((v,p2) as e2) =
	match v with
	| EBinop (_op,_e,_e2) when priority _op <= priority op ->
		let _e = make_binop op e _e in
		EBinop (_op,_e,_e2) , punion (pos _e) (pos _e2)
	| _ ->
		EBinop (op,e,e2) , punion (pos e) (pos e2)

let unclosed s (x,p) = error (Unclosed (s,x)) p

let rec program = parser
	| [< e = expr; p = program >] -> e :: p
	| [< '(Semicolon,_); p = program >] -> p
	| [< '(Eof,_) >] -> []

and expr = parser	
	| [< '(Const c,p); s >] ->
		expr_next (EConst c,p) s
	| [< '(BraceOpen,p1); p = block; s >] ->
		(match s with parser
		| [< '(BraceClose,p2); s >] -> expr_next (EBlock p,punion p1 p2) s
		| [< 'x >] -> unclosed "{" x)
	| [< '(ParentOpen,p1); pl = parameters; s >] ->
		(match s with parser
		| [< '(ParentClose,p2); s >] -> expr_next (ETupleDecl pl,punion p1 p2) s
		| [< 'x >] -> unclosed "(" x)
	| [< '(Keyword Var,p1); '(Const (Ident name),_); t = type_opt; '(Binop "=",_); e = expr; s >] ->
		expr_next (EVar (name,t,e),punion p1 (pos e)) s
	| [< '(Keyword If,p1); cond = expr; e = expr; s >] ->
		(match s with parser
		| [< '(Keyword Else,_); e2 = expr; s >] -> expr_next (EIf (cond,e,Some e2),punion p1 (pos e2)) s
		| [< >] -> expr_next (EIf (cond,e,None),punion p1 (pos e)) s)
	| [< '(Keyword Fun,p1); '(ParentOpen,po); p = parameter_names; s >] ->
		(match s with parser
		| [< '(ParentClose,_); t = type_opt; e = expr; s >] -> expr_next (EFunction (p,e,t),punion p1 (pos e)) s
		| [< 'x >] -> unclosed "(" x)
	| [< '(Keyword Type,p1); pl = type_decl_parameters; '(Const (Ident tname),p2); d , p2 = type_declaration p2; s >] ->
		expr_next (ETypeDecl (pl,tname,d) , punion p1 p2) s

and expr_next e = parser
	| [< '(Binop ":",_); t , p = type_path; s >] ->
		expr_next (ETypeAnnot (e,t),punion (pos e) p) s
	| [< '(Dot,_); '(Const (Ident name),p); s >] ->
		expr_next (EField (e,name),punion (pos e) p) s
	| [< '(ParentOpen,po); pl = parameters; s >] ->
		(match s with parser
		| [< '(ParentClose,p); s >] -> expr_next (ECall (e,pl),punion (pos e) p) s
		| [< 'x >] -> unclosed "(" x)
	| [< '(BracketOpen,po); e2 = expr; s >] ->
		(match s with parser
		| [< '(BracketClose,p); s >] -> expr_next (EArray (e,e2),punion (pos e) p) s
		| [< 'x >] -> unclosed "[" x)
	| [< '(Binop op,_); e2 = expr; s >] ->
		make_binop op e e2
	| [< >] ->
		e

and block = parser
	| [< e = expr; b = block >] -> e :: b
	| [< '(Semicolon,_); b = block >] -> b
	| [< >] -> []

and parameter_names = parser
	| [< '(Const (Ident name),_); t = type_opt; p = parameter_names >] -> (name , t) :: p
	| [< '(Comma,_); p = parameter_names >] -> p
	| [< >] -> []

and type_opt = parser
	| [< '(Binop ":",_); t , _ = type_path; >] -> Some t
	| [< >] -> None

and parameters = parser
	| [< e = expr; p = parameters_next >] -> e :: p
	| [< >] -> []

and parameters_next = parser
	| [< '(Comma,_); p = parameters >] -> p
	| [< >] -> []

and type_path = parser
	| [< '(Const (Ident tname),p); t = type_path_next (EType ([],tname)) p >] -> t
	| [< '(Quote,_); '(Const (Ident a),p); t = type_path_next (EPoly a) p >] -> t
	| [< '(ParentOpen,_); t , p = type_path; l , p = type_path_list_next p; '(ParentClose,_); s >] ->
		type_path_next (ETuple (t :: l)) p s

and type_path_list p = parser
	| [< t , p = type_path; l , p = type_path_list_next p >] -> t :: l , p

and type_path_list_next p = parser
	| [< '(Comma,_); t = type_path_list p >] -> t
	| [< >] -> [] , p

and type_path_next t p = parser
	| [< '(Binop "->",_); t2 , p = type_path >] -> 
		(match t2 with
		| EArrow (ta,tb) -> EArrow (EArrow(t,ta),tb) , p
		| _ -> EArrow (t,t2) , p);
	| [< '(Const (Ident tname),p); t = type_path_next (EType ([t],tname)) p >] -> t
	| [< >] -> t , p

and type_decl_parameters = parser
	| [< '(Quote,_); '(Const (Ident a),_); >] -> [a]
	| [< '(ParentOpen,_); l = type_decl_plist; '(ParentClose,_); >] -> l
	| [< >] -> []

and type_decl_plist = parser
	| [< '(Quote,_); '(Const (Ident a),_); l = type_decl_plist_next >] -> a :: l

and type_decl_plist_next = parser
	| [< '(Comma,_); l = type_decl_plist >] -> l
	| [< >] -> []

and type_declaration p = parser
	| [< '(BraceOpen,_); s >] ->
		(match s with parser
		| [< el , p = record_declaration false >] ->  ERecord el , p
		| [< el , p = union_declaration >] -> EUnion el , p)
	| [< '(Binop "=",_); t , p = type_path >] -> EAlias t , p
	| [< >] -> EAbstract , p

and record_declaration mut = parser
	| [< '(BraceClose,p) >] -> [] , p
	| [< '(Const (Ident "mutable"),_); l = record_declaration true; >] -> l
	| [< '(Semicolon,_); l = record_declaration false >] -> l 
	| [< '(Const (Ident name),_); '(Binop ":",_); t , _ = type_path; l , p = record_declaration false >] -> (name,mut,t) :: l , p

and union_declaration = parser
	| [< '(BraceClose,p) >] -> [] , p
	| [< '(Semicolon,_); l = union_declaration >] -> l 
	| [< '(Const (Constr name),_); t = type_opt; l , p = union_declaration >] -> (name,t) :: l , p

let parse code file =
	let old = Mllexer.save() in
	Mllexer.init file;
	let last = ref (Eof,null_pos) in
	let rec next_token x =
		let t, p = Mllexer.token code in
		match t with
		| Comment s | CommentLine s -> 
			next_token x
		| _ ->
			last := (t , p);
			Some (t , p)
	in
	try
		let l = program (Stream.from next_token) in
		Mllexer.restore old;
		EBlock l, { pmin = 0; pmax = (pos !last).pmax; pfile = file }
	with
		| Stream.Error _
		| Stream.Failure -> 
			Mllexer.restore old;
			error (Unexpected (fst !last)) (pos !last)
		| e ->
			Mllexer.restore old;
			raise e
