
type pos = {
	pmin : int;
	pmax : int;
	pfile : string;
}

type constant =
	| True
	| False
	| Null
	| This
	| Int of int
	| Float of string
	| String of string
	| Builtin of string
	| Ident of string
	| Module of string

type keyword =
	| Var
	| For
	| While
	| Do
	| If
	| Else
	| Switch
	| Function
	| Return
	| Break
	| Continue
	| Default
	| Try
	| Catch

type token =
	| Eof
	| Semicolon
	| Dot
	| Comma
	| Arrow
	| BraceOpen
	| BraceClose
	| ParentOpen
	| ParentClose
	| BracketOpen
	| BracketClose
	| Const of constant
	| Keyword of keyword
	| Binop of string
	| Comment of string
	| CommentLine of string

type while_flag =
	| NormalWhile
	| DoWhile

type expr_decl =
	| EConst of constant
	| EBlock of expr list
	| EParenthesis of expr
	| EField of expr * string
	| ECall of expr * expr list
	| EArray of expr * expr	
	| EVars of (string * expr option) list
	| EFor of expr * expr * expr * expr
	| EWhile of expr * expr * while_flag
	| EIf of expr * expr * expr option
	| ESwitch of expr * (expr * expr) list * expr option
	| ETry of expr * string * expr
	| EFunction of string list * expr
	| EBinop of string * expr * expr
	| EReturn of expr option
	| EBreak of expr option
	| EContinue

and expr = expr_decl * pos

let pos = snd

let null_pos = { pmin = -1; pmax = -1; pfile = "<null pos>" }

let punion p p2 =
	{
		pfile = p.pfile;
		pmin = min p.pmin p2.pmin;
		pmax = max p.pmax p2.pmax;
	}

let mk_call v args p = ECall (v,args) , p

let map f (e,p) =
	(match e with
	| EBlock el -> EBlock (List.map f el)
	| EParenthesis e -> EParenthesis (f e)
	| EField (e,s) -> EField (f e, s)
	| ECall (e,el) -> ECall (f e, List.map f el)
	| EArray (e1,e2) -> EArray (f e1, f e2)
	| EVars vl -> EVars (List.map (fun (v,e) -> v , match e with None -> None | Some e -> Some (f e)) vl)
	| EFor (e1,e2,e3,e4) -> EFor (f e1, f e2, f e3, f e4)
	| EWhile (e1,e2,flag) -> EWhile (f e1, f e2, flag)
	| EIf (e,e1,e2) -> EIf (f e, f e1, match e2 with None -> None | Some e -> Some (f e))
	| ESwitch (e,cases,def) -> ESwitch (f e, List.map (fun (e1,e2) -> f e1 , f e2) cases , match def with None -> None | Some e -> Some (f e))
	| ETry (e,ident,e2) -> ETry (f e, ident, f e2)
	| EFunction (params,e) -> EFunction (params, f e)
	| EBinop (op,e1,e2) -> EBinop (op, f e1, f e2)
	| EReturn (Some e) -> EReturn (Some (f e))
	| EBreak (Some e) -> EBreak (Some (f e))
	| EReturn None
	| EBreak None
	| EContinue	
	| EConst _ as x -> x) , p

let escape s =
	let b = Buffer.create (String.length s) in
	for i = 0 to (String.length s) - 1 do
		match s.[i] with
		| '\n' -> Buffer.add_string b "\\n"
		| '\t' -> Buffer.add_string b "\\t"
		| '\r' -> Buffer.add_string b "\\r"
		| c -> Buffer.add_char b c
	done;
	Buffer.contents b

let s_constant = function
	| True -> "true"
	| False -> "false"
	| Null -> "null"
	| This -> "this"
	| Int i -> string_of_int i
	| Float s -> s
	| String s -> "\"" ^ escape s ^ "\""
	| Builtin s -> "$" ^ s
	| Ident s -> s
	| Module s -> "#" ^ s

let s_keyword = function
	| Var -> "var"
	| For -> "for"
	| While -> "while"
	| Do -> "do"
	| If -> "if"
	| Else -> "else"
	| Switch -> "switch"
	| Function -> "function"
	| Return -> "return"
	| Break -> "break"
	| Continue -> "continue"
	| Default -> "default"
	| Try -> "try"
	| Catch -> "catch"

let s_token = function
	| Eof -> "<eof>"
	| Semicolon -> ";"
	| Dot -> "."
	| Comma -> ","
	| Arrow -> "=>"
	| BraceOpen -> "{"
	| BraceClose -> "}"
	| ParentOpen -> "("
	| ParentClose -> ")"
	| BracketOpen -> "["
	| BracketClose -> "]"
	| Const c -> s_constant c
	| Keyword k -> s_keyword k
	| Binop s -> s
	| Comment s -> "/*" ^ s ^ "*/"
	| CommentLine s -> "//" ^ s
 
	