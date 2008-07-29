import Client.Code;

typedef ThreadInfos = {
	var id : Int;
	var t : neko.vm.Thread;
}

class Tora {

	var clientQueue : neko.vm.Deque<Client>;

	function new() {
		clientQueue = new neko.vm.Deque();
	}

	function init( nthreads : Int ) {
		neko.Sys.putEnv("MOD_NEKO","1");
		for( i in 0...nthreads ) {
			var inf : ThreadInfos = {
				id : i,
				t : null,
			};
			inf.t = neko.vm.Thread.create(callback(threadLoop,inf));
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

	function threadLoop( t : ThreadInfos ) {
		var api = new ModNekoApi();
		var loader = initLoader(api);
		var redirect = loader.loadPrimitive("std@print_redirect",1);
		redirect(api.print);
		while( true ) {
			var client = clientQueue.pop(true);
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
			clientQueue.add(new Client(client));
		}
	}

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
		var t = new Tora();
		neko.Lib.println("Starting Tora server on "+host+":"+port+" with "+nthreads+" threads");
		t.init(nthreads);
		t.run(host,port);
	}

}