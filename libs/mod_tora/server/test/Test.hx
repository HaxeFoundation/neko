class Test {

	static var persist = new Array();

	static function main() {
	#if flash

		var send = 0, recv = 0, ping = 0.;
		var client = Std.random(0x1000000);
		client += Std.random(0x1000000);
		client += Std.random(0x1000000);

		var start = null;
		var t = new haxe.Timer(1000);
		var last = flash.Lib.getTimer(), lastSend = 0, lastRecv = 0;
		t.run = function() {
			var now = flash.Lib.getTimer();
			var s = Std.int((send - lastSend) * 1000.0 / (now - last + 1));
			var r = Std.int((recv - lastRecv) * 1000.0 / (now - last + 1));
			haxe.Log.trace("SEND "+s+"/s RECV "+r+"/s"+" PING "+Std.int(ping),null);
			last = now;
			lastSend = send;
			lastRecv = recv;
		};

		var url = "http://tora";
		var p = new tora.Protocol(url);
		p.addParameter("wait","1");
		p.onDisconnect = function() {
			trace("DISCONNECTED");
			p.connect();
		};
		p.onError = function(msg) {
			trace("ERROR "+msg);
			p.connect();
		};
		p.onData = function(msg) {
			var clid = Std.parseInt(msg.split("#")[1]);
			recv++;
		};
		p.connect();
		persist.push(p);

		start = function() {
			var p = new tora.Protocol(url);
			var k = 0;
			var t0 = flash.Lib.getTimer();
			p.addParameter("client",Std.string(client));
			p.onData = function(d) {
				send++;
				var dt = flash.Lib.getTimer() - t0;
				ping = ping * 0.9 + 0.1 * dt;
			};
			p.onDisconnect = function() { persist.remove(p); start(); };
			p.onError = function(msg) { trace("ERROR "+msg); p.onDisconnect(); };
			p.connect();
			persist.push(p);
		};

		flash.Lib.current.stage.addEventListener(flash.events.MouseEvent.CLICK,function(_) {
			start();
		});

	#else

		if( !neko.Web.isModNeko )
			throw "Can't run from system";
		neko.Web.cacheModule(main);
		neko.Web.setHeader("Content-Type","text/plain");
		var params = neko.Web.getParams();
		var q = new tora.Queue<{ n : Int, cl : Int }>("test");
		if( params.exists("wait") ) {
			neko.Lib.println("WAIT");
			q.listen(function(v) {
				if( v.n == 0 ) q.stop();
				neko.Lib.println(v.n+"#"+v.cl);
			});
		} else {
			var k = Std.random(1000);
			var cl = Std.parseInt(neko.Web.getParams().get("client"));
			neko.Sys.sleep(0.2);
			q.notify({ n : k, cl : cl });
			neko.Lib.println("SEND "+k+" TO "+q.count()+" FROM #"+cl);
		}

	#end
	}

}