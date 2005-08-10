
type pos = {
	pmin : int;
	pmax : int;
	pfile : string;
}

type constant =
	| Int of int
	| Float of string
	| String of string
	| Ident of string
	| Constr of string
	| Module of string list * constant

type keyword =
	| Var
	| If
	| Else
	| Fun
	| Try
	| Catch
	| Type
	| Match

type token =
	| Eof
	| Semicolon
	| Dot
	| Comma
	| Quote
	| BraceOpen
	| BraceClose
	| ParentOpen
	| ParentClose
	| BracketOpen
	| BracketClose
	| Arrow
	| Vertical
	| Const of constant
	| Keyword of keyword
	| Binop of string
	| Comment of string
	| CommentLine of string

type type_path =
	| EType of type_path option * string list * string
	| EPoly of string
	| ETuple of type_path list
	| EArrow of type_path * type_path

type type_decl =
	| EAbstract
	| EAlias of type_path
	| ERecord of (string * bool * type_path) list
	| EUnion of (string * type_path option) list

type pattern_decl =
	| PConst of constant
	| PTuple of pattern list
	| PRecord of (string * pattern) list
	| PConstr of string list * string * pattern option
	| PAlias of string * pattern
	| PList of pattern list

and pattern = pattern_decl * pos * type_path option

type expr_decl =
	| EConst of constant
	| EBlock of expr list
	| EField of expr * string
	| ECall of expr * expr list
	| EArray of expr * expr	
	| EVar of string * type_path option * expr 
	| EIf of expr * expr * expr option
	| EFunction of string option * (string * type_path option) list * expr * type_path option
	| EBinop of string * expr * expr
	| EUnop of string * expr
	| ETypeAnnot of expr * type_path
	| ETupleDecl of expr list
	| ETypeDecl of string list * string * type_decl
	| ERecordDecl of (string * expr) list
	| EListDecl of expr list
	| EMatch of expr * (pattern list * expr option * expr) list

and expr = expr_decl * pos

let pos = snd

let null_pos = { pmin = -1; pmax = -1; pfile = "<null pos>" }

let punion p p2 =
	{
		pfile = p.pfile;
		pmin = min p.pmin p2.pmin;
		pmax = max p.pmax p2.pmax;
	}

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

let rec s_constant = function
	| Int i -> string_of_int i
	| Float s -> s
	| String s -> "\"" ^ escape s ^ "\""
	| Ident s -> s
	| Constr s -> s
	| Module (l,c) -> String.concat "." l ^ "." ^ s_constant c 

let s_path path n = 
	match path with
	| [] -> n
	| _ -> String.concat "." path ^ "." ^ n

let s_keyword = function
	| Var -> "var"
	| If -> "if"
	| Else -> "else"
	| Fun -> "fun"
	| Try -> "try"
	| Catch -> "catch"
	| Type -> "type"
	| Match -> "match"

let s_token = function
	| Eof -> "<eof>"
	| Semicolon -> ";"
	| Dot -> "."
	| Comma -> ","
	| Quote -> "'"
	| BraceOpen -> "{"
	| BraceClose -> "}"
	| ParentOpen -> "("
	| ParentClose -> ")"
	| BracketOpen -> "["
	| BracketClose -> "]"
	| Arrow -> "->"
	| Vertical -> "|"
	| Const c -> s_constant c
	| Keyword k -> s_keyword k
	| Binop s -> s
	| Comment s -> "/*" ^ s ^ "*/"
	| CommentLine s -> "//" ^ s
