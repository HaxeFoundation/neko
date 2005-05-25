open Mlast

type error_msg =
	| Unexpected of token
	| Unclosed of string
	| Duplicate_default
	| Unknown_macro of string
	| Invalid_macro_parameters of string * int

exception Error of error_msg * pos

let error_msg = function
	| Unexpected t -> "Unexpected "^(s_token t)
	| Unclosed s -> "Unclosed " ^ s
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
		| [< _ >] -> error (Unclosed "{") p1)
	| [< '(ParentOpen,p1); e = expr; s >] ->
		(match s with parser
		| [< '(ParentClose,p2); s >] -> expr_next (EParenthesis e,punion p1 p2) s
		| [< _ >] -> error (Unclosed "(") p1)
	| [< '(Keyword Var,p1); '(Const (Ident name),_); t = type_opt; '(Binop "=",_); e = expr; s >] ->
		expr_next (EVar (name,t,e),punion p1 (pos e)) s
	| [< '(Keyword If,p1); cond = expr; e = expr; s >] ->
		(match s with parser
		| [< '(Keyword Else,_); e2 = expr; s >] -> expr_next (EIf (cond,e,Some e2),punion p1 (pos e2)) s
		| [< >] -> expr_next (EIf (cond,e,None),punion p1 (pos e)) s)
	| [< '(Keyword Fun,p1); '(ParentOpen,po); p = parameter_names; s >] ->
		(match s with parser
		| [< '(ParentClose,_); t = type_opt; e = expr; s >] -> expr_next (EFunction (p,e,t),punion p1 (pos e)) s
		| [< _ >] -> error (Unclosed "(") po)

and expr_next e = parser
	| [< '(Binop ":",_); t , p = type_path; s >] ->
		expr_next (ETypeAnnot (e,t),punion (pos e) p) s
	| [< '(Dot,_); '(Const (Ident name),p); s >] ->
		expr_next (EField (e,name),punion (pos e) p) s
	| [< '(ParentOpen,po); pl = parameters; s >] ->
		(match s with parser
		| [< '(ParentClose,p); s >] -> expr_next (ECall (e,pl),punion (pos e) p) s
		| [< _ >] -> error (Unclosed "(") po)
	| [< '(BracketOpen,po); e2 = expr; s >] ->
		(match s with parser
		| [< '(BracketClose,p); s >] -> expr_next (EArray (e,e2),punion (pos e) p) s
		| [< _ >] -> error (Unclosed "[") po)
	| [< '(Binop op,_); e2 = expr; s >] ->
		make_binop op e e2
	| [< '(Comma,_); e2 = expr; s >] ->
		(match fst e2 with 
		| ETupleDecl el -> ETupleDecl (e::el) , punion (pos e) (pos e2)
		| _ -> ETupleDecl [e;e2] , punion (pos e) (pos e2))
	| [< >] -> e

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
	| [< e = expr; p = parameters >] -> e :: p
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
	| [< '(Const (Ident tname),p); t = type_path_next (EType ([t],tname)) p >] -> t
	| [< >] -> t , p

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
