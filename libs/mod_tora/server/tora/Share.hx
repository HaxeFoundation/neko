package tora;

class Share<T> {

	var s : Dynamic;

	public function new( name : String, makeData : Void -> T ) {
		init();
		s = share_init(name,makeData);
	}

	public function get( lock : Bool ) : T {
		return share_get(s,lock);
	}

	public function set( data : T ) {
		share_set(s,data);
	}

	public function commit() {
		share_commit(s);
	}

	public function free() {
		share_free(s);
	}

	static function init() {
		if( share_init != null ) return;
		share_init = neko.Lib.load("mod_neko","share_init",2);
		share_get = neko.Lib.load("mod_neko","share_get",2);
		share_set = neko.Lib.load("mod_neko","share_set",2);
		share_commit = neko.Lib.load("mod_neko","share_commit",1);
		share_free = neko.Lib.load("mod_neko","share_free",1);
	}

	static var share_init = null;
	static var share_get;
	static var share_set;
	static var share_commit;
	static var share_free;

}