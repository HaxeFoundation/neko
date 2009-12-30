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

class Share<T> {

	var s : Dynamic;
	var p : Persist<T>;
	public var name(default,null) : String;

	public function new( name : String, ?makeData : Void -> T, ?persist : Class<T> ) {
		init();
		if( makeData == null ) makeData = function() return null;
		if( persist != null ) {
			p = untyped persist.__persist;
			if( p == null ) {
				p = new Persist(persist);
				untyped persist.__persist = p;
			}
		}
		this.name = name;
		s = share_init(untyped name.__s,makeData);
	}

	public function get( lock : Bool ) : T {
		var v = share_get(s,lock);
		if( p != null ) v = p.makeInstance(v);
		return v;
	}

	public function set( data : T ) {
		if( p != null ) data = p.makePersistant(data);
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