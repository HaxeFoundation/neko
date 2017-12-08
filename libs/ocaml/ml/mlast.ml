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

type pos = {
	pmin : int;
	pmax : int;
	pfile : string;
}

type constant =
	| Int of int
	| Char of char
	| Bool of bool
	| Float of string
	| String of string
	| Ident of string
	| Constr of string
	| Module of string list * constant

type keyword =
	| Var
	| If
	| Else
	| Function
	| Try
	| Catch
	| Type
	| Match
	| Then
	| When
	| While
	| Exception

type token =
	| Eof
	| Semicolon
	| Dot
	| Comma
	| Quote
	| BraceOpen
	| BraceClose
	| ParentOpen of bool
	| ParentClose
	| BracketOpen
	| BracketClose
	| Arrow
	| Vertical
	| StreamOpen
	| StreamClose
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

type arg =
	| ATyped of arg * type_path
	| ANamed of string
	| ATuple of arg list

type pattern_decl =
	| PIdent of string
	| PConst of constant
	| PTuple of pattern list
	| PRecord of (string * pattern) list
	| PConstr of string list * string * pattern option
	| PAlias of string * pattern
	| PTyped of pattern * type_path
	| PStream of stream_item list * int

and pattern = pattern_decl * pos

and stream_item =
	| SPattern of pattern
	| SExpr of string list * expr
	| SMagicExpr of pattern * int

and expr_decl =
	| EConst of constant
	| EBlock of expr list
	| EField of expr * string
	| ECall of expr * expr list
	| EArray of expr * expr	
	| EVar of (string * type_path option) list * expr 
	| EIf of expr * expr * expr option
	| EFunction of bool * string option * arg list * expr * type_path option
	| EBinop of string * expr * expr
	| EUnop of string * expr
	| ETypeAnnot of expr * type_path
	| ETupleDecl of expr list
	| ETypeDecl of string list * string * type_decl
	| EErrorDecl of string * type_path option
	| ERecordDecl of (string * expr) list
	| EMatch of expr * (pattern list * expr option * expr) list
	| ETry of expr * (pattern list * expr option * expr) list
	| ETupleGet of expr * int
	| EApply of expr * expr list
	| EWhile of expr * expr

and expr = expr_decl * pos

let pos = snd

let null_pos = { pmin = -1; pmax = -1; pfile = "<null pos>" }

let punion p p2 =
	{
		pfile = p.pfile;
		pmin = min p.pmin p2.pmin;
		pmax = max p.pmax p2.pmax;
	}

let escape_char = function
	| '\n' -> "\\n"
	| '\t' -> "\\t"
	| '\r' -> "\\r"
	| '\\' -> "\\\\"
	| c ->
		if c >= '\032' && c <= '\126' then
			String.make 1 c
		else
			Printf.sprintf "\\%.3d" (int_of_char c)

let escape s =
	let b = Buffer.create (String.length s) in
	for i = 0 to (String.length s) - 1 do
		Buffer.add_string b (escape_char s.[i])
	done;
	Buffer.contents b

let rec s_constant = function
	| Int i -> string_of_int i
	| Float s -> s
	| Bool b -> if b then "true" else "false"
	| Char c -> "'" ^ escape_char c ^ "\""
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
	| Function -> "function"
	| Try -> "try"
	| Catch -> "catch"
	| Type -> "type"
	| Match -> "match"
	| Then -> "then"
	| When -> "when"
	| While -> "while"
	| Exception -> "exception"

let s_token = function
	| Eof -> "<eof>"
	| Semicolon -> ";"
	| Dot -> "."
	| Comma -> ","
	| Quote -> "'"
	| BraceOpen -> "{"
	| BraceClose -> "}"
	| ParentOpen _ -> "("
	| ParentClose -> ")"
	| BracketOpen -> "["
	| BracketClose -> "]"
	| StreamOpen -> "[<"
	| StreamClose -> ">]"
	| Arrow -> "->"
	| Vertical -> "|"
	| Const c -> s_constant c
	| Keyword k -> s_keyword k
	| Binop s -> s
	| Comment s -> "/*" ^ s ^ "*/"
	| CommentLine s -> "//" ^ s
