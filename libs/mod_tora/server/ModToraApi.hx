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

typedef Share = {
	var name : String;
	var data : Dynamic;
	var lock : neko.vm.Mutex;
	var owner : Client;
}

typedef Queue = {
	var name : String;
	var lock : neko.vm.Mutex;
	var clients : List<Client>;
}

class NullClient extends Client {

	public var buffer : StringBuf;

	public function new( file : String, host : String, uri : String ) {
		super(cast { setTimeout : function(_) {}, setFastSend : function(_) {}, close : function() {} },true);
		this.file = file;
		this.hostName = host;
		this.uri = uri;
		ip = "127.0.0.1";
		getParams = "";
		httpMethod = "CALL";
		buffer = new StringBuf();
		execute = true;
	}

	override public function processMessage() {
		return true;
	}

	override public function sendMessage( code : tora.Code, msg ) {
		switch( code ) {
		case CPrint: buffer.add(msg);
		case CError, CExecute: onExecute();
		default:
		}
	}

	public dynamic function onExecute() {
	}

}


class ModToraApi extends ModNekoApi {

	// keep a list of clients in case the module is updated
	public var listening : List<Client>;
	public var lock : neko.vm.Mutex;

	public function new(client) {
		super(client);
		listening = new List();
		lock = new neko.vm.Mutex();
	}

	// tora-specific

	function tora_infos() {
		return Tora.inst.infos();
	}

	function tora_command(cmd,param) {
		return Tora.inst.command(cmd,param);
	}

	function tora_unsafe() {
		return !client.secure;
	}

	function tora_call( url : neko.NativeString, ?delay : Float, ?result : neko.NativeString ) {
		var url = neko.NativeString.toString(url);
		var c = new NullClient(client.file,client.hostName,url);
		if( delay != null ) {
			if( result != null )
				c.onExecute = function() {
					var data = c.buffer.toString();
					if( data != neko.NativeString.toString(result) ) {
						Tora.log("RETRY "+c.hostName+url);
						c.buffer = new StringBuf();
						Tora.inst.delay(0.5,function() Tora.inst.clientQueue.push(c));
					}
				};
			Tora.inst.delay(delay,function() Tora.inst.clientQueue.push(c));
			return null;
		}
		var lock = new neko.vm.Lock();
		var data = null;
		c.onExecute = function() {
			data = c.buffer.toString();
			lock.release();
		};
		Tora.inst.clientQueue.push(c);
		lock.wait();
		return neko.NativeString.ofString(data);
	}

	// shares

	static var shares = new Hash<Share>();
	static var shares_lock = new neko.vm.Mutex();

	function share_init( name : neko.NativeString, ?make : Void -> Dynamic ) : Share {
		var name = neko.NativeString.toString(name);
		var s = shares.get(name);
		if( s == null ) {
			shares_lock.acquire();
			s = shares.get(name);
			if( s == null ) {
				var tmp = new Hash();
				for( s in shares )
					tmp.set(s.name,s);
				s = {
					name : name,
					data : try make() catch( e : Dynamic ) { shares_lock.release(); neko.Lib.rethrow(e); },
					lock : new neko.vm.Mutex(),
					owner : null,
				};
				tmp.set(name,s);
				shares = tmp;
			}
			shares_lock.release();
		}
		return s;
	}

	function share_get( s : Share, lock : Bool ) {
		if( lock && s.owner != client ) {
			s.lock.acquire();
			s.owner = client;
			if( client.lockedShares == null )
				client.lockedShares = new List();
			client.lockedShares.add(s);
		}
		return s.data;
	}

	function share_set( s : Share, data : Dynamic ) {
		s.data = data;
	}

	function share_commit( s : Share ) {
		if( s.owner != client ) throw neko.NativeString.ofString("Can't commit a not locked share");
		s.owner = null;
		s.lock.release();
		client.lockedShares.remove(s);
	}

	function share_free( s : Share ) {
		shares_lock.acquire();
		shares.remove(s.name);
		shares_lock.release();
	}

	// queues

	static var queues = new Hash<Queue>();
	static var queues_lock = new neko.vm.Mutex();

	function queue_init( name : neko.NativeString ) : Queue {
		var name = neko.NativeString.toString(name);
		queues_lock.acquire();
		var q = queues.get(name);
		if( q == null ) {
			q = {
				name : name,
				lock : new neko.vm.Mutex(),
				clients : new List(),
			};
			queues.set(name,q);
		}
		queues_lock.release();
		return q;
	}

	function queue_listen( q : Queue, onNotify, onStop ) {
		if( client.notifyApi != null )
			throw neko.NativeString.ofString("Can't listen on several queues");
		if( this.main == null )
			throw neko.NativeString.ofString("Can't listen on not cached module");
		client.notifyApi = this;
		client.notifyQueue = q;
		client.onNotify = onNotify;
		var me = this;
		client.onStop = onStop;
		// add to queue
		q.lock.acquire();
		q.clients.add(client);
		q.lock.release();
		// add to listeners
		lock.acquire();
		listening.add(client);
		lock.release();
	}

	function queue_notify( q : Queue, message : Dynamic ) {
		q.lock.acquire();
		var old = this.client, oldapi = client.notifyApi;
		client.notifyApi = this;
		for( c in q.clients ) {
			client = c;
			Tora.inst.handleNotify(c,message);
		}
		client = old;
		client.notifyApi = oldapi;
		q.lock.release();
	}

	function queue_count( q : Queue ) {
		return q.clients.length;
	}

	function queue_stop( q : Queue ) {
		// we might be in a closure on another api, so let's fetch our real client
		var client = Tora.inst.getCurrentClient().notifyApi.client;
		if( client.notifyQueue != q )
			throw neko.NativeString.ofString("You can't stop on a queue you're not waiting");
		q.lock.acquire(); // we should already have it, but in case...
		q.clients.remove(client);
		client.onNotify = null;
		client.onStop = null;
		q.lock.release();
		// the api might be different than 'this'
		var api = client.notifyApi;
		api.lock.acquire();
		api.listening.remove(client);
		api.lock.release();
	}

}
