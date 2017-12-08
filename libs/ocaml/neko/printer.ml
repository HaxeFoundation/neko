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
 
open Ast

type 'a ctx = {
	ch : 'a IO.output;
	mutable level : int;
	mutable tabs : bool;
}

let create ch = {
	ch = ch;
	level = 0;
	tabs = true;
}

let newline ctx =
	IO.write ctx.ch '\n';
	ctx.tabs <- false

let level ctx b =
	ctx.level <- ctx.level + (if b then 1 else -1);
	newline ctx

let print ctx =
	if not ctx.tabs then begin
		IO.nwrite ctx.ch (String.make (ctx.level * 4) ' ');
		ctx.tabs <- true;
	end;
	IO.printf ctx.ch

let rec print_list ctx sep f = function
	| [] -> ()
	| x :: [] -> f x
	| x :: l -> f x; print ctx "%s" sep; print_list ctx sep f l

let rec print_ast ?(binop=false) ctx (e,p) =
	match e with
	| EConst c ->
		print ctx "%s" (s_constant c)
	| EBlock el ->
		print ctx "{";
		level ctx true;
		List.iter (fun e ->
			print_ast ctx e;
			if ctx.tabs then begin
				print ctx ";";
				newline ctx;
			end
		) el;
		ctx.level <- ctx.level - 1;
		print ctx "}";
		newline ctx;
	| EParenthesis e when not ctx.tabs ->
		print ctx "{ ";
		print_ast ctx e;
		print ctx " }";
	| EParenthesis e ->
		print ctx "( ";
		print_ast ctx e;
		print ctx " )";
	| EField (e,s) ->
		print_ast ctx e;
		print ctx ".%s" s;
	| ECall (e,el) ->
		print_ast ctx e;
		print ctx "(";
		print_list ctx "," (print_ast ctx) el;
		print ctx ")";
	| EArray (e1,e2) ->
		print_ast ctx e1;
		print ctx "[";
		print_ast ctx e2;
		print ctx "]"
	| EVars vl ->
		print ctx "var ";
		print_list ctx ", " (fun (n,v) ->
			print ctx "%s" n;
			match v with
			| None -> ()
			| Some e ->
				print ctx " = ";
				print_ast ctx e
		) vl;
		print ctx ";";
		newline ctx
	| EWhile (cond,e,NormalWhile) ->
		print ctx "while ";
		print_ast ctx cond;
		level_expr ctx e;
	| EWhile (cond,e,DoWhile) ->
		print ctx "do ";
		level_expr ctx e;
		print ctx "while ";
		print_ast ctx cond;
		newline ctx
	| EIf (cond,e,e2) ->
		print ctx "if ";
		print_ast ctx cond;
		level_expr ~closed:(e2=None) ctx e;
		(match e2 with
		| None -> ()
		| Some e -> 
			print ctx "else";
			level_expr ctx e)
	| ETry (e,id,e2) ->
		print ctx "try";
		level_expr ctx e;
		print ctx "catch %s" id;
		level_expr ctx e2;
	| EFunction (params,e) ->
		print ctx "function(";
		print_list ctx "," (print ctx "%s") params;
		print ctx ")";
		level_expr ctx e;
	| EBinop (op,e1,e2) ->
		let tabs = ctx.tabs in
		if binop then (if tabs then print ctx "(" else print ctx "{");
		print_ast ~binop:true ctx e1;
		print ctx " %s " op;
		print_ast ~binop:true ctx e2;
		if binop then (if tabs then print ctx ")" else print ctx "}");		
	| EReturn None ->
		print ctx "return;";
	| EReturn (Some e) ->
		print ctx "return ";
		print_ast ctx e;		
	| EBreak None ->
		print ctx "break;";			
	| EBreak (Some e) ->
		print ctx "break ";
		print_ast ctx e;			
	| EContinue ->
		print ctx "continue"
	| ENext (e1,e2) ->
		print_ast ctx e1;
		print ctx ";";
		newline ctx;
		print_ast ctx e2
	| EObject [] ->
		print ctx "$new(null)"
	| EObject fl ->
		print ctx "{";
		level ctx true;
		let rec loop = function
			| [] -> assert false
			| [f,e] ->
				print ctx "%s => " f;
				print_ast ctx e;
				newline ctx;
			| (f,e) :: l ->
				print ctx "%s => " f;
				print_ast ctx e;
				print ctx ", ";
				newline ctx;
				loop l
		in
		loop fl;
		level ctx false;
		print ctx "}"
	| ELabel s ->
		print ctx "%s:" s

and level_expr ?(closed=false) ctx (e,p) =
	match e with
	| EBlock _ -> 
		if ctx.tabs then print ctx " ";
		print_ast ctx (e,p)
	| ENext _ ->
		if ctx.tabs then print ctx " ";
		print_ast ctx (EBlock [(e,p)],p)
	| EParenthesis e ->
		if ctx.tabs then print ctx " ";
		print ctx "{";
		level ctx true;
		print_ast ctx e;
		level ctx false;
		print ctx "}";
	| _ ->
		level ctx true;
		print_ast ctx (e,p);
		if closed then print ctx ";";
		level ctx false

let print ctx ast =
	match fst ast with
	| EBlock el ->
		List.iter (fun e ->
			print_ast ctx e;
			if ctx.tabs then begin
				print ctx ";";
				newline ctx;
			end
		) el;
	| _ ->
		print_ast ctx ast

let to_string ast =
	let ch = IO.output_string() in
	let ctx = create ch in
	print ctx ast;
	IO.close_out ch