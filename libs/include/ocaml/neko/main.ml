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

let report (msg,p) etype printer =
	let error_printer file line =
		match !print_style with
		| StyleJava -> sprintf "%s:%d:" file line
		| StyleMSVC -> sprintf "%s(%d):" file line
	in
	let epos = PhpLexer.get_error_pos error_printer p in
	prerr_endline (sprintf "%s : %s %s" epos etype (printer msg));
	exit 1

let rec unique = function
	| [] -> []
	| x :: l when List.exists (( = ) x) l -> unique l
	| x :: l -> x :: (unique l)

;;
try	
	let usage = "Neko Interpreter v0.1 - (c)2005 Nicolas Cannasse\n Usage : interp.exe [options] <files...>\n Options :" in
	let files = ref [] in
	let time = Sys.time() in
	let paths = ref [""] in
	let args_spec = [
		("-msvc",Arg.Unit (fun () -> print_style := StyleMSVC),": use MSVC style errors");
	] in
	Arg.parse args_spec (fun file ->
		paths := normalize_path (Filename.dirname file) :: !paths;
		files := file :: !files
	) usage;
	if !files = [] then begin
		Arg.usage args_spec usage
	end else begin
		let ctx = Interp.create (unique (List.rev !paths)) in
		List.iter (fun file ->
			let mname = Filename.chop_extension (Filename.basename file) in
			try
				ignore(Interp.execute ctx mname Ast.null_pos);				
			with
				Interp.Error (Interp.Module_not_found m,_) when m = mname -> 
					failwith ("File not found " ^ file)
		) (List.rev !files);
	end;
with	
(*	| Lexer.Error (m,p) -> report (m,p) "syntax error" Lexer.error_msg
	| Parser.Error (m,p) -> report (m,p) "parse error" Parser.error_msg
*)	| PhpLexer.Error (m,p) -> report (m,p) "syntax error" PhpLexer.error_msg
	| PhpParser.Error (m,p) -> report (m,p) "parse error" PhpParser.error_msg
	| Interp.Error (m,p) -> report (m,p) "runtime error" Interp.error_msg
	| Failure msg ->
		prerr_endline msg;
		exit 1;
