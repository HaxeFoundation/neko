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
	let epos = Lexer.get_error_pos error_printer p in
	prerr_endline (sprintf "%s : %s %s" epos etype (printer msg));
	exit 1

;;
try	
	let usage = "Neko Interpreter v0.1 - (c)2005 Nicolas Cannasse\n Usage : interp.exe [options] <files...>\n Options :" in
	let files = ref [] in
	let time = Sys.time() in
	let args_spec = [
		("-msvc",Arg.Unit (fun () -> print_style := StyleMSVC),": use MSVC style errors");
	] in
	Arg.parse args_spec (fun file -> files := file :: !files) usage;
	if !files = [] then begin
		Arg.usage args_spec usage
	end else begin
		let ctx = Interp.create() in
		List.iter (fun file ->
			let ch = (try open_in file with _ -> failwith ("File not found " ^ file)) in
			let ast = Parser.parse (Lexing.from_channel ch) file in
			let v = Interp.interp ctx ast in
			()
		) (List.rev !files);
	end;
with	
	| Lexer.Error (m,p) -> report (m,p) "syntax error" Lexer.error_msg
	| Parser.Error (m,p) -> report (m,p) "parse error" Parser.error_msg
	| Interp.Error (m,p) -> report (m,p) "runtime error" Interp.error_msg
	| Failure msg ->
		prerr_endline msg;
		exit 1;
