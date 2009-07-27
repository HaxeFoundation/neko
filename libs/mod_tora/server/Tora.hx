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
import tora.Code;
import tora.Infos;

typedef ThreadData = {
	var id : Int;
	var t : neko.vm.Thread;
	var client : Client;
	var time : Float;
	var hits : Int;
	var notify : Int;
	var notifyRetry : Int;
	var errors : Int;
	var stopped : Bool;
}

typedef FileData = {
	var file : String;
	var filetime : Float;
	var loads : Int;
	var cacheHits : Int;
	var notify : Int;
	var bytes : Float;
	var time : Float;
	var lock : neko.vm.Mutex;
	var cache : haxe.FastList<ModToraApi>;
}

class Tora {

	static var STOP : Dynamic = {};
	static var MODIFIED : Dynamic = {};

	var clientQueue : neko.vm.Deque<Client>;
	var notifyQueue : neko.vm.Deque<Client>;
	var threads : Array<ThreadData>;
	var startTime : Float;
	var totalHits : Int;
	var recentHits : Int;
	var files : Hash<FileData>;
	var flock : neko.vm.Mutex;
	var rootLoader : neko.vm.Loader;
	var modulePath : Array<String>;
	var redirect : Dynamic;
	var set_trusted : Dynamic;
	var enable_jit : Bool -> Bool;
	var running : Bool;
	var jit : Bool;
	var hosts : Hash<String>;
	var ports : Array<Int>;

	function new() {
		totalHits = 0;
		recentHits = 0;
		running = true;
		startTime = haxe.Timer.stamp();
		files = new Hash();
		hosts = new Hash();
		ports = new Array();
		flock = new neko.vm.Mutex();
		clientQueue = new neko.vm.Deque();
		notifyQueue = new neko.vm.Deque();
		threads = new Array();
		rootLoader = neko.vm.Loader.local();
		modulePath = rootLoader.getPath();
	}

	function init( nthreads : Int ) {
		neko.Sys.putEnv("MOD_NEKO","1");
		redirect = neko.Lib.load("std","print_redirect",1);
		set_trusted = neko.Lib.load("std","set_trusted",1);
		enable_jit = neko.Lib.load("std","enable_jit",1);
		jit = (enable_jit(null) == true);
		neko.vm.Thread.create(callback(startup,nthreads));
		neko.vm.Thread.create(notifyLoop);
		neko.vm.Thread.create(speedLoop);
	}

	function startup( nthreads : Int ) {
		// don't start all threads immediatly : this prevent allocating
		// too many instances because we have too many concurent requests
		// when a server get restarted
		for( i in 0...nthreads ) {
			var inf : ThreadData = {
				id : i,
				t : null,
				client : null,
				hits : 0,
				notify : 0,
				notifyRetry : 0,
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
	}

	// measuring speed
	function speedLoop() {
		while( true ) {
			var hits = totalHits, time = neko.Sys.time();
			neko.Sys.sleep(1.0);
			recentHits = Std.int((totalHits - hits) / (neko.Sys.time() - time));
		}
	}

	// checking which listening clients are disconnected
	function notifyLoop() {
		var poll = new neko.net.Poll(4096);
		var socks = new Array();
		var changed = false;
		while( true ) {
			// add new clients
			while( true ) {
				var client = notifyQueue.pop(socks.length == 0);
				if( client == null ) break;
				changed = true;
				// if we have a stopped client, then we close the socket
				if( client.onNotify == null ) {
					try client.sock.close() catch( e : Dynamic ) {};
					socks.remove(client.sock);
				} else {
					client.sock.custom = client;
					socks.push(client.sock);
				}
			}
			if( changed ) {
				poll.prepare(socks,new Array());
				changed = false;
			}
			// check if some clients have been disconnected
			poll.events(1.0);
			var i = 0;
			var toremove = null;
			while( true ) {
				var idx = poll.readIndexes[i++];
				if( idx == -1 ) break;
				var client : Client = socks[idx].custom;
				if( toremove == null ) toremove = new List();
				toremove.add(client.sock);
				client.messageQueue.push(STOP);
				notifyClient(client);
			}
			// remove disconnected clients from socket list
			if( toremove != null ) {
				for( s in toremove )
					socks.remove(s);
				changed = true;
			}
		}
	}

	function initLoader( api : ModToraApi ) {
		var me = this;
		var mod_neko = neko.NativeString.ofString("mod_neko@");
		var mem_size = "std@mem_size";
		var self : neko.vm.Loader = null;
		var first_module = jit;
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

	function cleanupApi( api : ModToraApi ) {
		api.deprecated = true;
		for( c in api.listening ) {
			api.client = c;
			c.removeFromQueue();
			try c.sendMessage(CExecute,"") catch( e : Dynamic ) {};
			notifyQueue.add(c);
		}
		api.client = null;
	}

	function handleNotify( t : ThreadData, client : Client ) {
		// get the api that we are using for this client
		var f = files.get(client.file);
		var api = client.notifyApi;
		f.lock.acquire();
		// has the client already been stopped ?
		if( client.onNotify == null ) {
			f.lock.release();
			return;
		}
		var it = f.cache.head, prev = null;
		while( it != null ) {
			if( it.elt == api )
				break;
			prev = it;
			it = it.next;
		}
		// the api must be already in-use somewhere : sleep 50ms and loop
		if( it == null ) {
			f.lock.release();
			neko.Sys.sleep(0.05);
			t.notifyRetry++;
			notifyClient(client);
			return;
		}
		// remove the api from the list
		if( prev == null )
			f.cache.head = it.next;
		else
			prev.next = it.next;
		f.lock.release();
		// execute the notify event
		t.notify++;
		api.client = client;
		redirect(api.print);
		var stopCalled = false;
		try {
			var message = client.messageQueue.pop(true);
			if( message == STOP ) {
				var stop = client.onStop;
				client.removeFromQueue();
				client.sock.close();
				if( stop != null ) stop();
			} else {
				client.onNotify(message);
				// if the client has been stopped, tell the notifyLoop so it can close the socket
				if( client.onNotify == null ) {
					client.sendMessage(CExecute,"");
					notifyQueue.add(client);
				}
			}
		} catch( e : Dynamic ) {
			var data = try Std.string(e) + haxe.Stack.toString(haxe.Stack.exceptionStack()) catch( _ : Dynamic ) "???";
			try {
				client.sendMessage(CError,data);
			} catch( _ : Dynamic ) {
				// the socket might be closed soon
			}
		}
		redirect(null);
		api.client = null;
		// put back in queue
		f.lock.acquire();
		f.notify++;
		if( api.deprecated )
			cleanupApi(api);
		else
			f.cache.add(api);
		f.lock.release();
		// cleanup
		t.client = null;
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
			t.time = haxe.Timer.stamp();
			t.client = client;
			// process notify
			if( client.notifyApi != null ) {
				handleNotify(t,client);
				continue;
			}
			t.hits++;
			// retrieve request
			try {
				client.sock.setTimeout(3);
				while( !client.processMessage() ) {
				}
				if( client.execute && client.file == null )
					throw "Missing module file";
			} catch( e : Dynamic ) {
				if( client.secure ) log("Error while reading request ("+Std.string(e)+")");
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
						notify : 0,
						lock : new neko.vm.Mutex(),
						cache : new haxe.FastList<ModToraApi>(),
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
				for( api in f.cache )
					cleanupApi(api);
				f.cache = new haxe.FastList<ModToraApi>();
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
					api = new ModToraApi(client);
					redirect(api.print);
					initLoader(api).loadModule(client.file);
				} else {
					api.client = client;
					redirect(api.print);
					api.main();
				}
				if( client.notifyApi != null )
					code = CListen;
			} catch( e : Dynamic ) {
				code = CError;
				data = try Std.string(e) + haxe.Stack.toString(haxe.Stack.exceptionStack()) catch( _ : Dynamic ) "??? TORA Error";
			}
			// send result
			try {
				client.sendHeaders(); // if no data has been printed
				client.sock.setFastSend(true);
				client.sendMessage(code,data);
			} catch( e : Dynamic ) {
				if( client.secure ) log("Error while sending answer ("+Std.string(e)+")");
				t.errors++;
				client.onNotify = null;
			}
			// save infos
			f.lock.acquire();
			f.time += haxe.Timer.stamp() - t.time;
			f.bytes += client.dataBytes;
			api.client = null;
			if( api.main != null && f.filetime == time )
				f.cache.add(api);
			else
				cleanupApi(api);
			f.lock.release();
			// cleanup
			redirect(null);
			t.client = null;
			if( client.notifyApi != null ) {
				client.uri = "<notify>";
				notifyQueue.add(client);
			} else
				client.sock.close();
		}
	}

	public inline function notifyClient( client : Client ) {
		clientQueue.add(client);
	}

	function run( host : String, port : Int, secure : Bool ) {
		var s = new neko.net.Socket();
		try {
			s.bind(new neko.net.Host(host),port);
		} catch( e : Dynamic ) {
			throw "Failed to bind socket : invalid host or port is busy";
		}
		s.listen(100);
		ports.push(port);
		try {
			while( running ) {
				var sock = s.accept();
				totalHits++;
				notifyClient(new Client(sock,secure));
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
		var notify = 0, notifyRetry = 0;
		for( t in threads ) {
			var cur = t.client;
			var ti : ThreadInfos = {
				hits : t.hits,
				errors : t.errors,
				file : (cur == null) ? null : (cur.file == null ? "???" : cur.file),
				url : (cur == null) ? null : cur.getURL(),
				time : (haxe.Timer.stamp() - t.time),
			};
			tot += t.hits;
			notify += t.notify;
			notifyRetry += t.notifyRetry;
			tinf.push(ti);
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
			totalHits : totalHits,
			recentHits : recentHits,
			notify : notify,
			notifyRetry : notifyRetry,
			queue : totalHits - tot,
			upTime : haxe.Timer.stamp() - startTime,
			jit : jit,
		};
	}

	function loadConfig( cfg : String ) {
		var vhost = false;
		var root = null, names = null;
		// parse the apache configuration to extract virtual hosts
		for( l in ~/[\r\n]+/g.split(cfg) ) {
			l = StringTools.trim(l);
			var lto = l.toLowerCase();
			if( !vhost ) {
				if( StringTools.startsWith(lto,"<virtualhost") ) {
					vhost = true;
					root = null;
					names = new Array();
				}
			} else if( lto == "</virtualhost>" ) {
				vhost = false;
				if( root != null )
					for( n in names )
						if( !hosts.exists(n) )
							hosts.set(n,root);
			} else {
				var cmd = ~/[ \t]+/g.split(l);
				switch( cmd.shift().toLowerCase() ) {
				case "documentroot": root = cmd.join(" ")+"/index.n";
				case "servername", "serveralias": names = names.concat(cmd);
				}
			}
		}
	}

	public function resolveHost( name : String ) {
		return hosts.get(name);
	}

	var xmlCache : String;
	public function getCrossDomainXML() {
		if( xmlCache != null ) return xmlCache;
		var buf = new StringBuf();
		buf.add("<cross-domain-policy>");
		for( host in hosts.keys() )
			buf.add('<allow-access-from domain="'+host+'" to-ports="'+ports.join(",")+'"/>');
		buf.add("</cross-domain-policy>");
		buf.addChar(0);
		xmlCache = buf.toString();
		return xmlCache;
	}

	public static function log( msg : String ) {
		neko.io.File.stderr().writeString("["+Date.now().toString()+"] "+msg+"\n");
	}

	public static var inst : Tora;

	static function main() {
		var host = "127.0.0.1";
		var port = 6666;
		var args = neko.Sys.args();
		var nthreads = 32;
		var i = 0;
		// skip first argument for haxelib "run"
		if( args[0] != null && StringTools.endsWith(args[0],"/") )
			i++;
		var unsafePorts = new List();
		inst = new Tora();
		while( true ) {
			var kind = args[i++];
			var value = function() { var v = args[i++]; if( v == null ) throw "Missing value for '"+kind+"'"; return v; };
			if( kind == null ) break;
			switch( kind ) {
			case "-h","-host": host = value();
			case "-p","-port": port = Std.parseInt(value());
			case "-t","-threads": nthreads = Std.parseInt(value());
			case "-config": inst.loadConfig(neko.io.File.getContent(value()));
			case "-unsafe": unsafePorts.add(Std.parseInt(value()));
			default: throw "Unknown argument "+kind;
			}
		}
		inst.init(nthreads);
		for( port in unsafePorts ) {
			log("Opening unsafe port on "+host+":"+port);
			neko.vm.Thread.create(callback(inst.run,host,port,false));
		}
		log("Starting Tora server on "+host+":"+port+" with "+nthreads+" threads");
		inst.run(host,port,true);
		inst.stop();
	}

}