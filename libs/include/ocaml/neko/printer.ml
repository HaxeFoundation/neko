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

let rec print_ast ctx (e,p) =
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
		newline ctx
	| EFor (init,cond,incr,e) ->
		print ctx "for(";
		print_ast ctx init;
		print ctx " ";
		print_ast ctx cond;
		print ctx " ";
		print_ast ctx incr;
		print ctx ")";
		level_expr ctx e;
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
		level_expr ctx e;
		(match e2 with
		| None -> ()
		| Some e -> 
			print ctx "else";
			level_expr ctx e)
	| ESwitch (v,cases,def) ->
		print ctx "switch ";
		print_ast ctx v;
		print ctx " {";
		newline ctx;
		List.iter (fun (v,e) ->
			print_ast ctx v;
			print ctx " =>";
			level_expr ctx e;
		) cases;
		(match def with
		| None -> ()
		| Some e ->
			print ctx "default =>";
			level_expr ctx e);
		print ctx "}";
		newline ctx;
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
		print_ast ctx e1;
		print ctx " %s " op;
		print_ast ctx e2
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

and level_expr ctx (e,p) =
	match e with
	| EBlock _ -> 
		if ctx.tabs then print ctx " ";
		print_ast ctx (e,p)
	| _ ->
		level ctx true;
		print_ast ctx (e,p);		
		level ctx false

let to_string ast =
	let ch = IO.output_string() in
	let ctx = create ch in
	(match fst ast with
	| EBlock el ->
		List.iter (fun e ->
			print_ast ctx e;
			if ctx.tabs then begin
				print ctx ";";
				newline ctx;
			end
		) el;
	| _ ->
		print_ast ctx ast);
	IO.close_out ch