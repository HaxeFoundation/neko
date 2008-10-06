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

	static function fmt( v : Float ) {
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


	static var TABID = 0;
	static var TABS = new Array();

	static function tab( s : String, f ) {
		var id = TABID++;
		w('<a class="tab" href="#" onclick="return toggle(\'tab_'+id+'\')">'+s+'</a>');
		TABS.push(function() {
			w('<div name="tab" class="tab" id="tab_'+id+'" style="display : none">');
			f();
			w('</div>');
		});
	}

	static function displayTabs() {
		for( t in TABS )
			t();
		w('<script type="text/javascript">');
		w('
		var ck = document.cookie.split("tab=")[1];
		if( ck == null ) ck = "tab_0"; else ck = ck.split(";")[0];
		toggle(ck);
		');
		w('</script>');
	}

	static function main() {

		w("<html>");
		w("<head>");
		w("<title>Tora Admin</title>");
		w('<style type="text/css">');
		w("body, td, th { font-size : 8pt; font-family : monospace; }");
		w("h1, a.tab { font-size : 20pt; margin-top : 5px; margin-bottom : 5px; }");
		w("a.tab { color : black; font-weight : bold; text-decoration : none; margin-right : 30px; }");
		w("table { border-collapse : collapse; border-spacing : 0; margin-left : 30px; }");
		w("ul { margin-top : 5px; margin-bottom : 5px; }");
		w("tr { margin : 0; padding : 0; }");
		w("td, th { margin : 0; padding : 1px 5px 1px 5px; border : 1px solid black; }");
		w(".left { float : left; }");
		w(".right { float : right; margin-right : 30px; }");
		w('</style>');
		w('<script type="text/javascript">');
		w('
		function toggle(id) {
			var elts = document.getElementsByName("tab");
			var i = 0;
			while( i < elts.length )
				elts[i++].style.display = "none";
			document.getElementById(id).style.display = "";
			document.cookie = "tab="+id;
			return false;
		}
		');
		w('</script>');
		w("</head>");
		w("<body>");

		title("Tora Admin");

		var command : String -> String -> Void = neko.Lib.load("mod_neko","tora_command",2);
		var params = neko.Web.getParams();
		var cmd = params.get("command");
		if( cmd != null ) {
			var t = neko.Sys.time();
			command(cmd,params.get("p"));
			w("<p>Command <b>"+cmd+"</b> took "+fmt(neko.Sys.time() - t)+"s to execute</p>");
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
			"Uptime : "+fmt(infos.upTime)+"s",
			"Threads : "+busy+" / "+infos.threads.length,
			"Queue size : "+infos.queue,
			"Memory : "+Std.int((mem.heap - mem.free)/1024)+" / "+Std.int(mem.heap/1024)+" KB",
			"Total hits : "+infos.hits+" ("+fmt(infos.hits/infos.upTime)+"/sec)",
			"Cache hits : "+cacheHits+" ("+fmt(cacheHits*100.0/infos.hits)+"%)",
		]);

		tab("Files",function() {
			infos.files.sort(function(f1,f2) return (f2.loads + f2.cacheHits) - (f1.loads + f1.cacheHits));
			table(
				["File","Loads","Cache Hits","Instances","KB/hit","ms/hit"],
				infos.files,
				function(f:FileInfos) {
					var tot = f.loads + f.cacheHits;
					return [f.file,f.loads,f.cacheHits,f.cacheCount,fmt(f.bytes/(1024.0 * tot)),fmt(f.time*1000/tot)];
				}
			);
		});

		tab("Threads",function() {
			var count = 1;
			table(
				["TID","Hits","E","Status","Time"],
				infos.threads,
				function(t:ThreadInfos) return [count++,t.hits,t.errors,if( t.file == null ) "idle" else t.url,fmt(t.time)+"s"]
			);
		});

		displayTabs();

		w('</body>');
		w('</html>');
	}

}