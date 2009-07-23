package tora;
import tora.Code;

class Protocol {

	var sock : flash.net.Socket;
	var headers : Array<{ key : String, value : String }>;
	var params : Array<{ key : String, value : String }>;
	var uri : String;
	var host : String;
	var port : Int;
	var lastMessage : Code;
	var dataLength : Int;

	static var CODES : Array<Code> = Lambda.array(Lambda.map(Type.getEnumConstructs(Code),callback(Reflect.field,Code)));

	public function new( url : String ) {
		headers = new Array();
		params = new Array();
		var r = ~/^http:\/\/([^\/:]+)(:[0-9]+)?(.*)$/;
		if( !r.match(url) )
			throw "Invalid url "+url;
		host = r.matched(1);
		var port = r.matched(2);
		uri = r.matched(3);
		if( uri == "" ) uri = "/";
		this.port = if( port == null ) 6667 else Std.parseInt(port.substr(1));
	}

	public function addHeader(key,value) {
		headers.push({ key : key, value : value });
	}

	public function addParameter(key,value) {
		params.push({ key : key, value : value });
	}

	public function connect() {
		sock = new flash.net.Socket();
		sock.addEventListener(flash.events.Event.CONNECT,onConnect);
		sock.addEventListener(flash.events.Event.CLOSE,onClose);
		sock.addEventListener(flash.events.IOErrorEvent.IO_ERROR, onClose);
        sock.addEventListener(flash.events.SecurityErrorEvent.SECURITY_ERROR, onClose);
		sock.addEventListener(flash.events.ProgressEvent.SOCKET_DATA,onSocketData);
		sock.connect(host,port);
	}

	public function close() {
		sock.removeEventListener(flash.events.Event.CLOSE,onClose);
		try sock.close() catch( e : Dynamic ) {};
		sock = null;
	}

	function send( code : Code, data : String ) {
		sock.writeByte(Type.enumIndex(code) + 1);
		var length = data.length;
		sock.writeByte(length & 0xFF);
		sock.writeByte((length >> 8) & 0xFF);
		sock.writeByte(length >> 16);
		sock.writeUTFBytes(data);
	}

	function onConnect(_) {
		if( sock == null ) return;
		send(CHostResolve,host);
		send(CUri,uri);
		for( h in headers ) {
			send(CHeaderKey,h.key);
			send(CHeaderValue,h.value);
		}
		var get = "";
		for( p in params ) {
			if( get != "" ) get += ";";
			get += StringTools.urlEncode(p.key)+"="+StringTools.urlEncode(p.value);
			send(CParamKey,p.key);
			send(CParamValue,p.value);
		}
		send(CGetParams,get);
		send(CExecute,"");
		sock.flush();
	}

	function onSocketData(_) {
		if( sock == null ) return;
		while( true ) {
			if( lastMessage == null ) {
				if( sock.bytesAvailable < 4 ) return;
				var code = sock.readUnsignedByte() - 1;
				lastMessage = CODES[code];
				if( lastMessage == null ) {
					error("Unknown Code #"+code);
					return;
				}
				var d1 = sock.readUnsignedByte();
				var d2 = sock.readUnsignedByte();
				var d3 = sock.readUnsignedByte();
				dataLength = d1 | (d2 << 8) | (d3 << 16);
			}
			if( sock.bytesAvailable < dataLength )
				return;
			var bytes = new flash.utils.ByteArray();
			var data = sock.readBytes(bytes,0,dataLength);
			var msg = lastMessage;
			lastMessage = null;
			switch( msg ) {
			case CHeaderKey, CHeaderValue, CHeaderAddValue:
			case CPrint: onData(bytes.toString());
			case CError:
				error(bytes.toString());
				return;
			case CListen, CExecute:
			default:
				error("Can't handle "+msg);
				return;
			}
		}
	}

	function error( msg : String ) {
		try sock.close() catch( e : Dynamic ) {};
		sock = null;
		onError(msg);
	}

	function onClose( e : flash.events.Event ) {
		try sock.close() catch( e : Dynamic ) {};
		sock = null;
		onDisconnect();
	}

	public dynamic function onError( msg : String ) {
		throw msg;
	}

	public dynamic function onDisconnect() {
	}

	public dynamic function onData( data : String ) {
	}

}