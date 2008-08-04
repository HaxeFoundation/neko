class StressTest {

	static var SITES = neko.Sys.args();

	static function loop() {
		var r = new neko.Random();
		while( true ) {
			var site = SITES[r.int(SITES.length)];
			try {
				var r = haxe.Http.request("http://"+site);
				if( r.substr(0,8) == "Error : " )
					trace(site+" "+r);
				else
					neko.Lib.print(".");
			} catch( e : Dynamic ) {
				neko.Lib.print("*");
			}
		}
	}

	static function main() {
		var nthreads = 16;
		for( i in 0...nthreads )
			neko.vm.Thread.create(loop);
		while( true )
			neko.Sys.sleep(1);
	}

}