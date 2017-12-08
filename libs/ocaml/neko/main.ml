(*
 *  Neko Compiler
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
 
open Printf

type p_style =
	| StyleJava
	| StyleMSVC

let print_style = ref StyleJava

let normalize_path p =
	let l = String.length p in
	if l = 0 then
		"./"
	else match p.[l-1] with 
		| '\\' | '/' -> p
		| _ -> p ^ "/"

let report inf =
	let pos file line = (match !print_style with
		| StyleJava -> sprintf "%s:%d:" file line
		| StyleMSVC -> sprintf "%s(%d):" file line
	) in
	prerr_endline (sprintf "%s : %s %s" (inf.Plugin.exn_pos pos) inf.Plugin.exn_name inf.Plugin.exn_message);
	exit 1

let dump file ch out =
	let data = (try Bytecode.read ch with Bytecode.Invalid_file -> IO.close_in ch; failwith ("Invalid bytecode file " ^ file)) in
	IO.close_in ch;
	Bytecode.dump out data;
	IO.close_out out

let dump_exn = function
	| e -> raise e

let compile file ch out =
	let ast = Parser.parse (Lexing.from_function (fun s p -> try IO.input ch s 0 p with IO.No_more_input -> 0)) file in
	IO.close_in ch;
	let data = Compile.compile file ast in
	Bytecode.write out data;
	IO.close_out out

let compile_exn = function
	| Lexer.Error (m,p) -> Plugin.exn_infos "syntax error" (Lexer.error_msg m) (fun f -> Lexer.get_error_pos f p)
	| Parser.Error (m,p) -> Plugin.exn_infos "parse error" (Parser.error_msg m) (fun f -> Lexer.get_error_pos f p)
	| Compile.Error (m,p) -> Plugin.exn_infos "compile error" (Compile.error_msg m) (fun f -> Lexer.get_error_pos f p)
	| e -> raise e

let main() =
	try	
		let usage = "Neko v0.4 - (c)2005-2017 Haxe Foundation\n Usage : neko.exe [options] <files...>\n Options :" in
		let output = ref "n" in
		let args_spec = [
			("-msvc",Arg.Unit (fun () -> print_style := StyleMSVC),": use MSVC style errors");
			("-p", Arg.String (fun p -> Plugin.add_path p),"<path> : add the file to path");
			("-o", Arg.String (fun ext -> output := String.lowercase ext),"<file> : specify output extension");
			("-v", Arg.Unit (fun () -> Plugin.verbose := true),": verbose mode");
		] in
		Arg.parse args_spec (fun file -> 
			Plugin.generate file !output
		) usage;
	with
		| Plugin.Error inf ->
			report inf
		| Failure msg ->
			prerr_endline msg;
			exit 1

;;
Plugin.register "neko" "n" compile compile_exn;
Plugin.register "n" "dump" dump dump_exn;
at_exit main