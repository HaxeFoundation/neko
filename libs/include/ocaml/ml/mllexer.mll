{
open Mlast
open Lexing

type error_msg =
	| Invalid_character of char
	| Unterminated_string
	| Unclosed_comment

exception Error of error_msg * pos

let error_msg = function
	| Invalid_character c when int_of_char c > 32 && int_of_char c < 128 -> Printf.sprintf "Invalid character '%c'" c
	| Invalid_character c -> Printf.sprintf "Invalid character 0x%.2X" (int_of_char c)
	| Unterminated_string -> "Unterminated string"
	| Unclosed_comment -> "Unclosed comment"

let cur_file = ref ""
let all_lines = Hashtbl.create 0
let lines = ref []
let buf = Buffer.create 100

let error e pos = 
	raise (Error (e,{ pmin = pos; pmax = pos; pfile = !cur_file }))

let keywords =
	let h = Hashtbl.create 3 in
	List.iter (fun k -> Hashtbl.add h (s_keyword k) k)
	[Var;If;Else;Fun;Try;Catch]
	; h

let init file =
	cur_file := file;
	lines := []

let save_lines() =
	Hashtbl.replace all_lines !cur_file !lines

let save() =
	save_lines();
	!cur_file

let restore file =
	save_lines();
	cur_file := file;
	lines := Hashtbl.find all_lines file 

let newline lexbuf =
	lines := (lexeme_end lexbuf) :: !lines

let find_line p lines =
	let rec loop n delta = function
		| [] -> n , p - delta
		| lp :: l when lp > p -> n , p - delta
		| lp :: l -> loop (n+1) lp l
	in
	loop 1 0 lines

let get_error_line p =
	let lines = List.rev (try Hashtbl.find all_lines p.pfile with Not_found -> []) in
	let l, _ = find_line p.pmin lines in
	l

let get_error_pos printer p =
	if p.pmin = -1 then
		"(unknown)"
	else
		let lines = List.rev (try Hashtbl.find all_lines p.pfile with Not_found -> []) in
		let l1, p1 = find_line p.pmin lines in
		let l2, p2 = find_line p.pmax lines in
		if l1 = l2 then begin
			let s = (if p1 = p2 then Printf.sprintf " %d" p1 else Printf.sprintf "s %d-%d" p1 p2) in
			Printf.sprintf "%s character%s" (printer p.pfile l1) s
		end else
			Printf.sprintf "%s lines %d-%d" (printer p.pfile l1) l1 l2

let reset() = Buffer.reset buf
let contents() = Buffer.contents buf
let store lexbuf = Buffer.add_string buf (lexeme lexbuf)
let add c = Buffer.add_string buf c

let mk_tok t pmin pmax =
	t , { pfile = !cur_file; pmin = pmin; pmax = pmax }

let mk lexbuf t = 
	mk_tok t (lexeme_start lexbuf) (lexeme_end lexbuf)

let mk_ident lexbuf =
	let s = lexeme lexbuf in
	mk lexbuf (try Keyword (Hashtbl.find keywords s) with Not_found -> Const (Ident s))

}

let ident = ['a'-'z' '_'] ['a'-'z' 'A'-'Z' '0'-'9' '_']*
let modident = ['A'-'Z'] ['a'-'z' 'A'-'Z' '0'-'9' '_']*
let binop = ['!' '=' '*' '/' '<' '>' '&' '|' '^' '%' '+' ':' '-']
let number = ['0'-'9']

rule token = parse
	| eof { mk lexbuf Eof }
	| ';' { mk lexbuf Semicolon }
	| '.' { mk lexbuf Dot }
	| ',' { mk lexbuf Comma }
	| '{' { mk lexbuf BraceOpen }
	| '}' { mk lexbuf BraceClose }
	| '(' { mk lexbuf ParentOpen }
	| ')' { mk lexbuf ParentClose }
	| '[' { mk lexbuf BracketOpen }
	| ']' { mk lexbuf BracketClose }
	| "=>" { mk lexbuf Arrow }
	| [' ' '\r' '\t']+ { token lexbuf } 
	| '\n' { newline lexbuf; token lexbuf }
	| "0x" ['0'-'9' 'a'-'f' 'A'-'F']+	
	| number+ { mk lexbuf (Const (Int (int_of_string (lexeme lexbuf)))) }
	| number+ '.' number*
	| '.' number+ { mk lexbuf (Const (Float (lexeme lexbuf))) }
	| "true" { mk lexbuf (Const True) } 
	| "false" { mk lexbuf (Const False) }
	| ident { mk_ident lexbuf }
	| modident { mk lexbuf (Const (Constr (lexeme lexbuf))) } 
	| '"' {
			reset();
			let pmin = lexeme_start lexbuf in
			let pmax = (try string lexbuf with Exit -> error Unterminated_string pmin) in
			mk_tok (Const (String (contents()))) pmin pmax;
		}
	| "/*" {
			reset();
			let pmin = lexeme_start lexbuf in
			let pmax = (try comment lexbuf with Exit -> error Unclosed_comment pmin) in
			mk_tok (Comment (contents())) pmin pmax;
		}	
	| "//" [^'\n']*  {
			let s = lexeme lexbuf in
			let n = (if s.[String.length s - 1] = '\r' then 3 else 2) in
			mk lexbuf (CommentLine (String.sub s 2 ((String.length s)-n)))
		}
	| binop binop? | ">>>" { mk lexbuf (Binop (lexeme lexbuf)) }
	| _ {
			error (Invalid_character (lexeme_char lexbuf 0)) (lexeme_start lexbuf)
		}

and comment = parse
	| eof { raise Exit }
	| '\r' { comment lexbuf }
	| '\n' { newline lexbuf; store lexbuf; comment lexbuf }
	| "*/" { lexeme_end lexbuf }
	| '*' { store lexbuf; comment lexbuf }
	| [^'*' '\n' '\r']+ { store lexbuf; comment lexbuf }

and string = parse
	| eof { raise Exit }
	| '\r' { string lexbuf }
	| '\n' { newline lexbuf; store lexbuf; string lexbuf }
	| "\\\"" { store lexbuf; string lexbuf }
	| "\\\\" { store lexbuf; string lexbuf }
	| "\\n" { add "\n"; string lexbuf }
	| "\\t" { add "\t"; string lexbuf }
	| "\\r" { add "\r"; string lexbuf }
	| '\\' { store lexbuf; string lexbuf }
	| '"' { lexeme_end lexbuf }
	| [^'"' '\\' '\r' '\n']+ { store lexbuf; string lexbuf }
 