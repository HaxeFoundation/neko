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
import neko.net.Socket.SocketHandle;
import Client.Code;
import Infos;

typedef ThreadData = {
	var id : Int;
	var t : neko.vm.Thread;
	var client : Client;
	var time : Float;
	var hits : Int;
	var errors : Int;
	var stopped : Bool;
}

typedef FileData = {
	var file : String;
	var filetime : Float;
	var loads : Int;
	var cacheHits : Int;
	var bytes : Float;
	var time : Float;
	var lock : neko.vm.Mutex;
	var cache : haxe.FastList<ModNekoApi>;
}

class Tora {

	var clientQueue : neko.vm.Deque<Client>;
	var threads : Array<ThreadData>;
	var startTime : Float;
	var totalHits : Int;
	var files : Hash<FileData>;
	var flock : neko.vm.Mutex;
	var rootLoader : neko.vm.Loader;
	var modulePath : Array<String>;
	var redirect : Dynamic;
	var set_trusted : Dynamic;
	var enable_jit : Bool -> Bool;
	var set_fast_send : SocketHandle -> Bool -> Void;
	var running : Bool;

	function new() {
		totalHits = 0;
		running = true;
		startTime = haxe.Timer.stamp();
		files = new Hash();
		flock = new neko.vm.Mutex();
		clientQueue = new neko.vm.Deque();
		threads = new Array();
		rootLoader = neko.vm.Loader.local();
		modulePath = rootLoader.getPath();
	}

	function init( nthreads : Int ) {
		neko.Sys.putEnv("MOD_NEKO","1");
		redirect = neko.Lib.load("std","print_redirect",1);
		set_trusted = neko.Lib.load("std","set_trusted",1);
		enable_jit = neko.Lib.load("std","enable_jit",1);
		// always disable jit if not enabled when running tora
		if( enable_jit(null) != true )
			enable_jit = function(_) return false;
		set_fast_send = try neko.Lib.load("std","socket_set_fast_send",2) catch( e : Dynamic ) function(s,f) {};
		neko.vm.Thread.create(callback(startup,nthreads));
	}

	function startup( nthreads : Int ) {
		for( i in 0...nthreads ) {
			var inf : ThreadData = {
				id : i,
				t : null,
				client : null,
				hits : 0,
				errors : 0,
				time : haxe.Timer.stamp(),
				stopped : false,
			};
			inf.t = neko.vm.Thread.create(callback(threadLoop,inf));
			threads.push(inf);
			while( true ) {
				neko.Sys.sleep(0.5);
				if( totalHits > i * 10 )
					break;
			}
		}
		cleanupLoop();
	}

	function cleanupLoop() {
		while( running ) {
			neko.Sys.sleep(15);
			flock.acquire();
			var files = Lambda.array(files);
			if( files.length == 0 ) {
				flock.release();
				continue;
			}
			var f = null;
			for( i in 0...10 ) {
				f = files[Std.random(files.length)];
				if( f.cache.head != null ) break;
			}
			flock.release();
			// remove the last from the list : it's better to recycle the oldest to prevent leaks
			f.lock.acquire();
			var h = f.cache.head;
			var prev = null;
			while( h != null ) {
				prev = h;
				h = h.next;
			}
			if( prev == null )
				f.cache.head = null;
			else
				prev.next = null;
			f.lock.release();
		}
	}

	function initLoader( api : ModNekoApi ) {
		var me = this;
		var mod_neko = neko.NativeString.ofString("mod_neko@");
		var mem_size = "std@mem_size";
		var self : neko.vm.Loader = null;
		var first_module = true;
		var loadPrim = function(prim:String,nargs:Int) {
			if( untyped __dollar__sfind(prim.__s,0,mod_neko) == 0 ) {
				var p = Reflect.field(api,prim.substr(9));
				if( p == null || untyped __dollar__nargs(p) != nargs )
					throw "Primitive not found "+prim+" "+nargs;
				return untyped __dollar__varargs( function(args) return __dollar__call(p,api,args) );
			}
			if( prim == mem_size )
				return function(_) return 0;
			return me.rootLoader.loadPrimitive(prim,nargs);
		};
		var loadModule = function(module:String,l) {
			var idx = module.lastIndexOf(".");
			if( idx >= 0 )
				module = module.substr(0,idx);
			var cache : Dynamic = untyped self.l.cache;
			var mod = Reflect.field(cache,module);
			if( mod == null ) {
				if( first_module )
					me.enable_jit(true);
				mod = neko.vm.Module.readPath(module,me.modulePath,self);
				if( first_module ) {
					me.enable_jit(false);
					first_module = false;
				}
				Reflect.setField(cache,module,mod);
				mod.execute();
			}
			return mod;
		};
		self = neko.vm.Loader.make(loadPrim,loadModule);
		return self;
	}

	function getFileTime( file ) {
		return try neko.FileSystem.stat(file).mtime.getTime() catch( e : Dynamic ) 0.;
	}

	function threadLoop( t : ThreadData ) {
		set_trusted(true);
		while( true ) {
			var client = clientQueue.pop(true);
			if( client == null ) {
				// let other threads pop 'null' as well
				// in case of global restart
				t.stopped = true;
				break;
			}
			t.hits++;
			t.time = haxe.Timer.stamp();
			t.client = client;
			try {
				client.sock.setTimeout(3);
				while( !client.processMessage() ) {
				}
				if( client.execute && client.file == null )
					throw "Missing module file";
			} catch( e : Dynamic ) {
				log("Error while reading request ("+Std.string(e)+")");
				t.errors++;
				client.execute = false;
			}
			// check if we need to do something
			if( !client.execute ) {
				client.sock.close();
				t.client = null;
				continue;
			}
			var f = files.get(client.file);
			var api = null;
			// file entry not found : we need to acquire
			// a global lock before setting the entry
			if( f == null ) {
				flock.acquire();
				f = files.get(client.file);
				if( f == null ) {
					f = {
						file : client.file,
						filetime : 0.,
						loads : 0,
						cacheHits : 0,
						lock : new neko.vm.Mutex(),
						cache : new haxe.FastList<ModNekoApi>(),
						bytes : 0.,
						time : 0.,
					};
					files.set(client.file,f);
				}
				flock.release();
			}
			// check if up-to-date cache is available
			f.lock.acquire();
			var time = getFileTime(client.file);
			if( time != f.filetime ) {
				f.filetime = time;
				f.cache = new haxe.FastList<ModNekoApi>();
			}
			api = f.cache.pop();
			if( api == null )
				f.loads++;
			else
				f.cacheHits++;
			f.lock.release();
			// execute
			var code = CExecute;
			var data = "";
			try {
				if( api == null ) {
					api = new ModNekoApi(client);
					redirect(api.print);
					initLoader(api).loadModule(client.file);
				} else {
					api.client = client;
					redirect(api.print);
					api.main();
				}
			} catch( e : Dynamic ) {
				code = CError;
				data = try Std.string(e) + haxe.Stack.toString(haxe.Stack.exceptionStack()) catch( _ : Dynamic ) "??? TORA Error";
			}
			// send result
			try {
				client.sendHeaders(); // if no data has been printed
				var s : { private var __s : SocketHandle; } = client.sock;
				set_fast_send(s.__s,true);
				client.sendMessage(code,data);
			} catch( e : Dynamic ) {
				log("Error while sending answer ("+Std.string(e)+")");
				t.errors++;
				api.main = null; // in case of cache-bug
			}
			// save infos
			f.lock.acquire();
			f.time += haxe.Timer.stamp() - t.time;
			f.bytes += client.dataBytes;
			if( api.main != null && f.filetime == time ) {
				api.client = null;
				f.cache.add(api);
			}
			f.lock.release();
			// cleanup
			redirect(null);
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
		try {
			while( running ) {
				var client = s.accept();
				totalHits++;
				clientQueue.add(new Client(client));
			}
		} catch( e : Dynamic ) {
			log("accept() failure : maybe too much FD opened ?");
		}
		// close our waiting socket
		s.close();
	}

	function stop() {
		log("Shuting down...");
		// inform all threads that we are stopping
		for( i in 0...threads.length )
			clientQueue.add(null);
		// our own marker
		clientQueue.add(null);
		var count = 0;
		while( true ) {
			var c = clientQueue.pop(false);
			if( c == null )
				break;
			c.sock.close();
			count++;
		}
		log(count + " sockets closed in queue...");
		// wait for threads to stop
		neko.Sys.sleep(5);
		count = 0;
		for( t in threads )
			if( t.stopped )
				count++;
			else
				log("Thread "+t.id+" is locked in "+((t.client == null)?"???":t.client.getURL()));
		log(count + " / " + threads.length + " threads stopped");
	}


	public function command( cmd : String, param : String ) : Void {
		switch( cmd ) {
		case "stop":
			running = false;
		case "gc":
			neko.vm.Gc.run(true);
		case "clean":
			flock.acquire();
			for( f in files.keys() )
				files.remove(f);
			flock.release();
		default:
			throw "No such command '"+cmd+"'";
		}
	}

	public function infos() : Infos {
		var tinf = new Array();
		var tot = 0;
		for( t in threads ) {
			var cur = t.client;
			var t : ThreadInfos = {
				hits : t.hits,
				errors : t.errors,
				file : (cur == null) ? null : (cur.file == null ? "???" : cur.file),
				url : (cur == null) ? null : cur.getURL(),
				time : (haxe.Timer.stamp() - t.time),
			};
			tot += t.hits;
			tinf.push(t);
		}
		var finf = new Array();
		for( f in files ) {
			var f : FileInfos = {
				file : f.file,
				loads : f.loads,
				cacheHits : f.cacheHits,
				cacheCount : Lambda.count(f.cache),
				bytes : f.bytes,
				time : f.time,
			};
			finf.push(f);
		}
		return {
			threads : tinf,
			files : finf,
			hits : totalHits,
			queue : totalHits - tot,
			upTime : haxe.Timer.stamp() - startTime,
		};
	}

	public static function log( msg : String ) {
		neko.io.File.stderr().writeString("["+Date.now().toString()+"] "+msg+"\n");
	}

	public static var inst : Tora;

	static function main() {
		var args = neko.Sys.args();
		var host = args[0];
		if( host == null ) host = "127.0.0.1";
		var port = args[1];
		if( port == null ) port = "6666";
		var nthreads = args[2];
		if( nthreads == null ) nthreads = "32";
		var port = Std.parseInt(port);
		var nthreads = Std.parseInt(nthreads);
		inst = new Tora();
		log("Starting Tora server on "+host+":"+port+" with "+nthreads+" threads");
		inst.init(nthreads);
		inst.run(host,port);
		inst.stop();
	}

}