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
import Client.Code;
import Infos;

typedef ThreadData = {
	var id : Int;
	var t : neko.vm.Thread;
	var client : Client;
	var time : Float;
	var hits : Int;
}

class Tora {

	var clientQueue : neko.vm.Deque<Client>;
	var threads : Array<ThreadData>;
	var totalHits : Int;

	function new() {
		totalHits = 0;
		clientQueue = new neko.vm.Deque();
		threads = new Array();
	}

	function init( nthreads : Int ) {
		neko.Sys.putEnv("MOD_NEKO","1");
		for( i in 0...nthreads ) {
			var inf : ThreadData = {
				id : i,
				t : null,
				client : null,
				hits : 0,
				time : haxe.Timer.stamp(),
			};
			inf.t = neko.vm.Thread.create(callback(threadLoop,inf));
			threads.push(inf);
		}
	}

	function initLoader( api : ModNekoApi ) {
		var loader : Dynamic = neko.vm.Loader.local().l;
		var mod_neko = neko.NativeString.ofString("mod_neko@");
		var newloader = {
			path : loader.path,
			cache : {},
			loadmodule : function(m,l) return loader.loadmodule(m,l),
			loadprim : function(prim : neko.NativeString,nargs) {
				if( untyped __dollar__sfind(prim,0,mod_neko) == 0 ) {
					var p = Reflect.field(api,neko.NativeString.toString(prim).substr(9));
					if( p == null || untyped __dollar__nargs(p) != nargs )
						throw "Primitive not found "+prim+" "+nargs;
					return untyped __dollar__varargs( function(args) return __dollar__call(p,api,args) );
				}
				return loader.loadprim(prim,nargs);
			},
		};
		return new neko.vm.Loader(cast newloader);
	}

	function resetLoaderCache() {
		untyped __dollar__loader.cache = __dollar__new(null);
	}

	function threadLoop( t : ThreadData ) {
		var api = new ModNekoApi();
		var loader = initLoader(api);
		var redirect = loader.loadPrimitive("std@print_redirect",1);
		redirect(api.print);
		while( true ) {
			var client = clientQueue.pop(true);
			t.time = haxe.Timer.stamp();
			t.client = client;
			t.hits++;
			try {
				client.sock.setTimeout(3);
				while( !client.processMessage() ) {
				}
				api.client = client;
				api.main = null;
				loader.loadModule(client.file);
				api.client = null;
				client.sendMessage(CExecute,"");
			} catch( e : Dynamic ) try {
				api.client = null;
				var error = Std.string(e) + haxe.Stack.toString(haxe.Stack.exceptionStack());
				client.sendMessage(CError,error);
			} catch( e : Dynamic ) {
				trace(e);
			}
			resetLoaderCache();
			client.sock.close();
			t.client = null;
		}
	}

	function run( host : String, port : Int ) {
		var s = new neko.net.Socket();
		try {
			s.bind(new neko.net.Host(host),port);
		} catch( e : Dynamic ) {
			throw "Failed to bind socket : invalid host or port is busy";
		}
		s.listen(100);
		while( true ) {
			var client = s.accept();
			totalHits++;
			clientQueue.add(new Client(client));
		}
	}

	public function infos() : Infos {
		var tinf = new Array();
		var tot = 0;
		for( t in threads ) {
			var cur = t.client;
			var t : ThreadInfos = {
				hits : t.hits,
				current : (cur == null) ? null : cur.file,
				time : (haxe.Timer.stamp() - t.time),
			};
			tot += t.hits;
			tinf.push(t);
		}
		return {
			threads : tinf,
			hits : totalHits,
			queue : totalHits - tot,
		};
	}

	public static var inst : Tora;

	static function main() {
		var args = neko.Sys.args();
		var host = args[0];
		if( host == null ) host = "127.0.0.1";
		var port = args[1];
		if( port == null ) port = "666";
		var nthreads = args[2];
		if( nthreads == null ) nthreads = "32";
		var port = Std.parseInt(port);
		var nthreads = Std.parseInt(nthreads);
		inst = new Tora();
		neko.Lib.println("Starting Tora server on "+host+":"+port+" with "+nthreads+" threads");
		inst.init(nthreads);
		inst.run(host,port);
	}

}