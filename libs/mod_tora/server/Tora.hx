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

typedef CacheData = {
	var file : String;
	var filetime : Float;
	var hits : Int;
	var lock : neko.vm.Mutex;
	var datas : haxe.FastList<ModNekoApi>;
}

class Tora {

	var clientQueue : neko.vm.Deque<Client>;
	var threads : Array<ThreadData>;
	var totalHits : Int;
	var moduleCache : Hash<CacheData>;
	var cacheLock : neko.vm.Mutex;
	var rootLoader : neko.vm.Loader;
	var modulePath : Array<String>;

	function new() {
		totalHits = 0;
		moduleCache = new Hash();
		cacheLock = new neko.vm.Mutex();
		clientQueue = new neko.vm.Deque();
		threads = new Array();
		rootLoader = neko.vm.Loader.local();
		modulePath = rootLoader.getPath();
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
		neko.vm.Thread.create(cleanupLoop);
	}

	function cleanupLoop() {
		while( true ) {
			neko.Sys.sleep(60);
			cacheLock.acquire();
			var caches = Lambda.array(moduleCache);
			var cache = caches[Std.random(caches.length)];
			cacheLock.release();
			if( cache == null ) continue;
			cache.lock.acquire();
			cache.datas.pop();
			cache.lock.release();
		}
	}

	function initLoader( api : ModNekoApi ) {
		var me = this;
		var mod_neko = neko.NativeString.ofString("mod_neko@");
		var self : neko.vm.Loader = null;
		var loadPrim = function(prim:String,nargs:Int) {
			if( untyped __dollar__sfind(prim.__s,0,mod_neko) == 0 ) {
				var p = Reflect.field(api,prim.substr(9));
				if( p == null || untyped __dollar__nargs(p) != nargs )
					throw "Primitive not found "+prim+" "+nargs;
				return untyped __dollar__varargs( function(args) return __dollar__call(p,api,args) );
			}
			return me.rootLoader.loadPrimitive(prim,nargs);
		};
		var loadModule = function(module,l) {
			var cache : Dynamic = untyped self.l.cache;
			var mod = Reflect.field(cache,module);
			if( mod == null ) {
				mod = neko.vm.Module.readPath(module,me.modulePath,self);
				Reflect.setField(cache,module,mod);
				mod.execute();
			}
			return mod;
		};
		self = neko.vm.Loader.make(loadPrim,loadModule);
		return self;
	}

	function getFileTime( file ) {
		return neko.FileSystem.stat(file).mtime.getTime();
	}

	function threadLoop( t : ThreadData ) {
		var redirect = neko.vm.Loader.local().loadPrimitive("std@print_redirect",1);
		while( true ) {
			var client = clientQueue.pop(true);
			t.hits++;
			t.time = haxe.Timer.stamp();
			t.client = client;
			try {
				client.sock.setTimeout(3);
				while( !client.processMessage() ) {
				}
				var cache = moduleCache.get(client.file);
				var api = null;
				if( cache != null ) {
					cache.lock.acquire();
					var time = getFileTime(client.file);
					if( time != cache.filetime ) {
						cache.filetime = time;
						cache.datas = new haxe.FastList<ModNekoApi>();
					} else
						cache.hits++;
					api = cache.datas.pop();
					cache.lock.release();
				}
				if( api == null ) {
					api = new ModNekoApi(client);
					redirect(api.print);
					initLoader(api).loadModule(client.file);
				} else {
					api.client = client;
					redirect(api.print);
					api.main();
				}
				redirect(null);
				if( api.main != null ) {
					if( cache == null ) {
						cacheLock.acquire();
						cache = moduleCache.get(client.file);
						if( cache == null ) {
							cache = {
								file : client.file,
								filetime : getFileTime(client.file),
								hits : 0,
								lock : new neko.vm.Mutex(),
								datas : new haxe.FastList<ModNekoApi>(),
							};
							moduleCache.set(client.file,cache);
						}
						cacheLock.release();
					}
					api.client = null;
					cache.lock.acquire();
					cache.datas.add(api);
					cache.lock.release();
				}
				client.sendMessage(CExecute,"");
			} catch( e : Dynamic ) try {
				redirect(null);
				var error = try Std.string(e) + haxe.Stack.toString(haxe.Stack.exceptionStack()) catch( _ : Dynamic ) "??? TORA Error";
				try client.sendMessage(CError,error) catch( e : Dynamic ) {}
			}
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
		neko.vm.Gc.run(true);
		var tinf = new Array();
		var tot = 0;
		for( t in threads ) {
			var cur = t.client;
			var t : ThreadInfos = {
				hits : t.hits,
				file : (cur == null) ? null : cur.file,
				url : (cur == null) ? null : cur.hostName+cur.uri,
				time : (haxe.Timer.stamp() - t.time),
			};
			tot += t.hits;
			tinf.push(t);
		}
		var cinf = new Array();
		for( c in moduleCache ) {
			var c : CacheInfos = {
				file : c.file,
				hits : c.hits,
				count : Lambda.count(c.datas),
			};
			cinf.push(c);
		}
		var mem = neko.vm.Gc.stats();
		return {
			threads : tinf,
			cache : cinf,
			hits : totalHits,
			queue : totalHits - tot,
			memoryUsed : mem.heap - mem.free,
			memoryTotal : mem.heap,
		};
	}

	public static function log( v : Dynamic ) {
		var msg = try Std.string(v) catch( e : Dynamic ) "???";
		neko.io.File.stderr().writeString(msg+"\n");
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