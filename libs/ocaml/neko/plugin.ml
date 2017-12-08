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

type exn_infos = {
	exn_name : string;
	exn_message : string;
	exn_pos : (string -> int -> string) -> string;
}

type filter = {
	ext_in : string;
	ext_out : string;
	transform : string -> IO.input -> unit IO.output -> unit;
	exceptions : exn -> exn_infos;
}

exception Error of exn_infos

let paths = ref [""]
let plugins = ref []
let verbose = ref false

let register source dest trans exc =
	plugins := {
		ext_in = String.lowercase source;
		ext_out = String.lowercase dest;
		transform = trans;
		exceptions = exc;
	} :: !plugins

let exn_infos name msg pos = 
	{
		exn_name = name;
		exn_pos = pos;
		exn_message = msg;
	}

let open_file ?(bin=false) f =
	let rec loop = function 
		| [] -> None 
		| path :: l ->
			let file = path ^ f in
			try
				let ch = (if bin then open_in_bin else open_in) file in
				Some (file, IO.input_channel ch)
			with
				_ -> loop l
	in
	loop (!paths)
	
let switch_ext file ext =
	try
		Filename.chop_extension file ^ ext
	with
		_ -> file ^ ext

let add_path path =
	let l = String.length path in
	if l > 0 && path.[l-1] != '\\' && path.[l-1] != '/' then
		paths := (path ^ "/") :: !paths
	else
		paths := path :: !paths

let generate_loop file ch fext ext =
	if ext = fext then
		()
	else
	let rec loop fext acc = function
		| [] -> raise Not_found
		| x :: l when x.ext_in = fext && not (List.exists (fun p -> p.ext_in = x.ext_out) acc) ->
			if x.ext_out = ext then
				x :: acc
			else
			(try
				let l1 = loop x.ext_out (x :: acc) (!plugins) in
				(try
					let l2 = loop fext acc l in
					if List.length l2 < List.length l1 then l2 else l1
				with
					Not_found -> l1)
			with
				Not_found -> loop fext acc l)
		| x :: l ->
			loop fext acc l
	in
	let ftarget = switch_ext file ("." ^ ext) in
	let genlist = (try
		List.rev (loop fext [] (!plugins))
	with Not_found -> 
		let fbase = Filename.basename file in
		failwith ("Don't know how to generate " ^ Filename.basename ftarget ^ " from " ^ fbase)
	) in
	let execute x file ch out =
		try
			x.transform file ch out
		with
			e -> raise (Error (x.exceptions e))
	in
	let rec loop file ch = function
		| [] -> assert false
		| [x] -> 
			let out = IO.output_channel (open_out_bin ftarget) in
			execute x file ch out;
			(try IO.close_in ch with IO.Input_closed -> ());
			(try IO.close_out out with IO.Output_closed -> ());
		| x :: l ->
			let chin , out = IO.pipe() in
			execute x file ch out;
			(try IO.close_in ch with IO.Input_closed -> ());
			(try IO.close_out out with IO.Output_closed -> ());
			loop ("<tmp>." ^ x.ext_out) chin l
	in
	loop file ch genlist

let generate file ext =
	let fext = (try
		let p = String.rindex file '.' in
		String.sub file (p + 1) (String.length file - (p + 1))
	with
		Not_found -> file
	) in
	match open_file ~bin:true file with
	| None -> failwith ("File not found : " ^ file)
	| Some (file,ch) -> generate_loop file ch fext ext
