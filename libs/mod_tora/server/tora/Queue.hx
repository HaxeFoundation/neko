package tora;

class Queue<T> {

	var q : Dynamic;

	public function new( name : String ) {
		init();
		q = queue_init(name);
	}

	public function listen( onNotify : T -> Void, ?onStop : Void -> Void ) {
		queue_listen(q,onNotify,onStop);
	}

	public function notify( message : T ) {
		queue_notify(q,message);
	}

	public function count() : Int {
		return queue_count(q);
	}

	public function stop() : Void {
		queue_stop(q);
	}

	static function init() {
		if( queue_init != null ) return;
		queue_init = neko.Lib.load("mod_neko","queue_init",1);
		queue_listen = neko.Lib.load("mod_neko","queue_listen",3);
		queue_notify = neko.Lib.load("mod_neko","queue_notify",2);
		queue_count = neko.Lib.load("mod_neko","queue_count",1);
		queue_stop = neko.Lib.load("mod_neko","queue_stop",1);
	}

	static var queue_init = null;
	static var queue_listen;
	static var queue_notify;
	static var queue_count;
	static var queue_stop;

}