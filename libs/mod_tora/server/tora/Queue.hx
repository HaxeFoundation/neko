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
package tora;

class Queue<T> {

	var q : Dynamic;
	public var name(default,null) : String;

	public function new( name : String ) {
		init();
		this.name = name;
		q = queue_init(untyped name.__s);
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