/* ************************************************************************ */
/*																			*/
/*  Tora - Neko Application Server											*/
/*  Copyright (c)2008 Motion-Twin											*/
/*																			*/
/* This library is free software; you can redistribute it and/or			*/
/* modify it under the terms of the GNU Lesser General Public				*/
/* License as published by the Free Software Foundation; either				*/
/* version 2.1 of the License, or (at your option) any later version.		*/
/*																			*/
/* This library is distributed in the hope that it will be useful,			*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU		*/
/* Lesser General Public License or the LICENSE file for more details.		*/
/*																			*/
/* ************************************************************************ */
import Infos;

class Admin {

	static function w(str) {
		neko.Lib.println(str);
	}

	static function f( v : Float ) {
		return Math.round(v * 10) / 10;
	}

	static function list( a : Iterable<String> ) {
		w("<ul>");
		for( x in a )
			w("<li>"+x+"</li>");
		w("</ul>");
	}

	static function title( s : String ) {
		w("<h1>"+s+"</h1>");
	}

	static function table<T>( headers : Array<String>, list : Iterable<T>, f : T -> Array<Dynamic> ) {
		w('<table>');
		w("<tr>");
		for( h in headers )
			w("<th>"+h+"</th>");
		w("</tr>");
		for( i in list ) {
			w("<tr>");
			for( x in f(i) )
				w("<td>"+Std.string(x)+"</td>");
			w("</tr>");
		}
		w("</table>");
	}

	static function main() {

		w("<html>");
		w("<head>");
		w("<title>Tora Admin</title>");
		w('<style type="text/css">');
		w("body, td, th { font-size : 8pt; font-family : monospace; }");
		w("h1 { font-size : 20pt; margin-top : 5px; margin-bottom : 5px; }");
		w("table { border-collapse : collapse; border-spacing : 0; margin-left : 30px; }");
		w("ul { margin-top : 5px; margin-bottom : 5px; }");
		w("tr { margin : 0; padding : 0; }");
		w("td, th { margin : 0; padding : 1px 5px 1px 5px; border : 1px solid black; }");
		w(".left { float : left; }");
		w(".right { float : right; margin-right : 30px; }");
		w('</style>');
		w("</head>");
		w("<body>");

		title("Tora Admin");

		var command : String -> String -> Void = neko.Lib.load("mod_neko","tora_command",2);
		var params = neko.Web.getParams();
		var cmd = params.get("command");
		if( cmd != null ) {
			var t = neko.Sys.time();
			command(cmd,params.get("p"));
			w("<p>Command <b>"+cmd+"</b> took "+f(neko.Sys.time() - t)+"s to execute</p>");
		}
		var mem = neko.vm.Gc.stats();
		var infos : Infos = neko.Lib.load("mod_neko","tora_infos",0)();
		var busy = 0;
		var cacheHits = 0;
		for( t in infos.threads )
			if( t.file != null )
				busy++;
		for( f in infos.files )
			cacheHits += f.cacheHits;

		list([
			"Uptime : "+f(infos.upTime)+"s",
			"Threads : "+busy+" / "+infos.threads.length,
			"Queue size : "+infos.queue,
			"Memory : "+Std.int((mem.heap - mem.free)/1024)+" / "+Std.int(mem.heap/1024)+" KB",
			"Total hits : "+infos.hits+" ("+f(infos.hits/infos.upTime)+"/sec)",
			"Cache hits : "+cacheHits+" ("+f(cacheHits*100/infos.hits)+"%)",
		]);

		w('<div class="left">');
		title("Files");

		infos.files.sort(function(f1,f2) return (f2.loads + f2.cacheHits) - (f1.loads + f1.cacheHits));

		table(
			["File","Loads","C.Hits","Inst"],
			infos.files,
			function(f:FileInfos) return [f.file,f.loads,f.cacheHits,f.cacheCount]
		);
		w("</div>");

		w('<div class="right">');
		title("Threads");

		var count = 1;
		table(
			["TID","Hits","E","Status","Time"],
			infos.threads,
			function(t:ThreadInfos) return [count++,t.hits,t.errors,if( t.file == null ) "idle" else t.url,f(t.time)+"s"]
		);
		w("</div>");

		w("</body></html>");

	}

}