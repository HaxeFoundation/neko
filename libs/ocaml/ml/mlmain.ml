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

let nekoml file ch out =
	IO.close_in ch;
	Mltyper.verbose := !Plugin.verbose;
	Mlneko.verbose := !Plugin.verbose;
	let path = ExtString.String.nsplit (Filename.dirname file) "/" in
	let modname = path @ [String.capitalize (Filename.chop_extension (Filename.basename file))] in
	let ctx = Mltyper.context (!Plugin.paths) in
	ignore(Mltyper.load_module ctx modname Mlast.null_pos);
	Hashtbl.iter (fun m (e,deps,idents) ->
		let e = Mlneko.generate e deps idents m in
		let file = String.concat "/" m ^ ".neko" in		
		let ch = (if m = modname then out else IO.output_channel (open_out file)) in
		let ctx = Printer.create ch in
		Printer.print ctx e;
		IO.close_out ch
	) (Mltyper.modules ctx)

let nekoml_exn = function
	| Mllexer.Error (m,p) -> Plugin.exn_infos "syntax error" (Mllexer.error_msg m) (fun f -> Mllexer.get_error_pos f p)
	| Mlparser.Error (m,p) -> Plugin.exn_infos "parse error" (Mlparser.error_msg m) (fun f -> Mllexer.get_error_pos f p)
	| Mltyper.Error (m,p) -> Plugin.exn_infos "type error" (Mltyper.error_msg m) (fun f -> Mllexer.get_error_pos f p)
	| Lexer.Error (m,p) -> Plugin.exn_infos "syntax error" (Lexer.error_msg m) (fun f -> Lexer.get_error_pos f p)
	| Parser.Error (m,p) -> Plugin.exn_infos "parse error" (Parser.error_msg m) (fun f -> Lexer.get_error_pos f p)
	| e -> raise e

;;
Plugin.register "nml" "neko" nekoml nekoml_exn
