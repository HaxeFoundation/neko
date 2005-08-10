
let nekoml file ch out =
	IO.close_in ch;
	IO.close_out out;
	let ctx = Mltyper.context ["";Filename.dirname file ^ "/"] in
	ignore(Mltyper.load_module ctx [String.capitalize (Filename.chop_extension (Filename.basename file))] Mlast.null_pos);
	Hashtbl.iter (fun m e ->
		let e = Mlneko.generate e in
		let file = String.concat "/" m ^ ".neko" in
		let ch = IO.output_channel (open_out file) in
		let ctx = Printer.create ch in
		Printer.print ctx e;
		IO.close_out ch
	) (Mltyper.modules ctx)

let nekoml_exn = function
	| Mllexer.Error (m,p) -> Plugin.exn_infos "syntax error" (Mllexer.error_msg m) (fun f -> Mllexer.get_error_pos f p)
	| Mlparser.Error (m,p) -> Plugin.exn_infos "parse error" (Mlparser.error_msg m) (fun f -> Mllexer.get_error_pos f p)
	| Mltyper.Error (m,p) -> Plugin.exn_infos "type error" (Mltyper.error_msg m) (fun f -> Mllexer.get_error_pos f p)
	| e -> raise e

;;
Plugin.register "nml" "neko" nekoml nekoml_exn
