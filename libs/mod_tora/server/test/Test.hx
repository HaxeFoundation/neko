class Test {

	static var persist = new Array();

	static function main() {
	#if flash

		var send = 0, recv = 0;

		var t = new haxe.Timer(1000);
		var last = flash.Lib.getTimer(), lastSend = 0, lastRecv = 0, sendData = "";
		t.run = function() {
			var now = flash.Lib.getTimer();
			var s = Std.int((send - lastSend) * 1000.0 / (now - last + 1));
			var r = Std.int((recv - lastRecv) * 1000.0 / (now - last + 1));
			haxe.Log.trace("SEND "+s+"/s RECV "+r+"/s"+"    ("+StringTools.trim(sendData)+")");
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
		p.onData = function(msg) recv++;
		p.connect();
		persist.push(p);

		var start = null;
		start = function() {
			var p = new tora.Protocol(url);
			var k = 0;
			p.onData = function(d) { send++; sendData = d; };
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
		var q = new tora.Queue("test");
		if( params.exists("wait") ) {
			neko.Lib.println("WAIT");
			q.listen(function(n:Int) {
				if( n == 0 ) q.stop();
				neko.Lib.println(n);
			});
		} else {
			var k = Std.random(1000);
			neko.Lib.println(k+" TO "+q.count());
			q.notify(k);
		}

	#end
	}

}