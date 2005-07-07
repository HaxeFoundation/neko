
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
	| Macro of string

type keyword =
	| Var
	| For
	| While
	| Do
	| If
	| Else
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
	| ETry of expr * string * expr
	| EFunction of string list * expr
	| EBinop of string * expr * expr
	| EReturn of expr option
	| EBreak of expr option
	| EContinue
	| ENext of expr * expr

and expr = expr_decl * pos

let pos = snd

let var_args = -1

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
	| ETry (e,ident,e2) -> ETry (f e, ident, f e2)
	| EFunction (params,e) -> EFunction (params, f e)
	| EBinop (op,e1,e2) -> EBinop (op, f e1, f e2)
	| EReturn (Some e) -> EReturn (Some (f e))
	| EBreak (Some e) -> EBreak (Some (f e))
	| ENext (e1,e2) -> ENext (f e1,f e2)
	| EReturn None
	| EBreak None
	| EContinue	
	| EConst _ as x -> x) , p

let iter f (e,p) =
	match e with
	| EBlock el -> List.iter f el
	| EParenthesis e -> f e
	| EField (e,s) -> f e
	| ECall (e,el) -> f e; List.iter f el
	| EArray (e1,e2) -> f e1; f e2
	| EVars vl -> List.iter (fun (_,e) -> match e with None -> () | Some e -> f e) vl
	| EFor (e1,e2,e3,e4) -> f e1; f e2; f e3; f e4
	| EWhile (e1,e2,_) -> f e1; f e2
	| EIf (e,e1,e2) -> f e; f e1; (match e2 with None -> () | Some e -> f e)
	| ETry (e1,_,e2) -> f e1; f e2
	| EFunction (_,e) -> f e
	| EBinop (_,e1,e2) -> f e1; f e2
	| EReturn (Some e) -> f e
	| EBreak (Some e) -> f e
	| ENext (e1,e2) -> f e1; f e2
	| EReturn None
	| EBreak None
	| EContinue
	| EConst _ -> ()

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
	| Macro s -> "'" ^ s

let s_keyword = function
	| Var -> "var"
	| For -> "for"
	| While -> "while"
	| Do -> "do"
	| If -> "if"
	| Else -> "else"
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

let builtins_list = [
	"print",var_args;
	"time",0;
	"new",1;
	"array",var_args;
	"amake",1;
	"acopy",1;
	"asize",1;
	"aget",2;
	"aset",3;
	"asub",3;
	"smake",1;
	"ssize",1;
	"scopy",1;
	"ssub",3;
	"sget",2;
	"sset",3;
	"sblit",5;
	"throw",1;
	"isfun",2;
	"nargs",1;
	"callopt",3;
	"call",3;
	"div",2;
	"isNaN",1;
	"isInf",1;
	"isTrue",1;
	"objget",2;
	"objset",3;
	"objcall",3;
	"safeget",2;
	"safeset",3;
	"safecall",3;
	"haveField",2;
	"objrem",2;
	"objopt",1;
	"objFields",1;
	"setThis",1;
	"hash",1;
	"field",1;
	"int",1;
	"stof",1;
	"typeof",1;
	"closure",var_args;
	"compare",2;
	"loader",0;
	"exports",0;
]

let builtins_hash = 
	let h = Hashtbl.create 0 in
	List.iter (fun (b,n) -> Hashtbl.add h b n) builtins_list;
	h
