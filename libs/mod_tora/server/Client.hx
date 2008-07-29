enum Code {
	CFile;
	CUri;
	CClientIP;
	CGetParams;
	CPostData;
	CHeaderKey;
	CHeaderValue;
	CHeaderAddValue;
	CParamKey;
	CParamValue;
	CHostName;
	CHttpMethod;
	CExecute;
	CError;
	CPrint;
	CLog;
	CFlush;
	CRedirect;
	CReturnCode;
	CQueryMultipart;
	CPartFilename;
	CPartKey;
	CPartData;
	CPartDone;
}

class Client {

	static var CODES = Type.getEnumConstructs(Code);

	// protocol
	public var sock : neko.net.Socket;
	public var data : String;
	public var bytes : Int;

	// variables
	public var file : String;
	public var uri : String;
	public var ip : String;
	public var getParams : String;
	public var postData : String;
	public var headers : Array<{ k : String, v : String }>;
	public var params : Array<{ k : String, v : String }>;
	public var hostName : String;
	public var httpMethod : String;

	var key : String;

	public function new(s) {
		sock = s;
		headers = new Array();
		params = new Array();
	}

	public function readMessageBuffer( buf : haxe.io.Bytes ) : Code {
		var i = sock.input;
		var code = i.readByte();
		if( code == 0 || code > CODES.length )
			throw "Invalid proto code "+code;
		bytes = i.readUInt24();
		i.readFullBytes(buf,0,bytes);
		return Reflect.field(Code,CODES[code-1]);
	}

	public function readMessage() : Code {
		var i = sock.input;
		var code = i.readByte();
		if( code == 0 || code > CODES.length )
			throw "Invalid proto code "+code;
		var len = i.readUInt24();
		data = i.readString(len);
		return Reflect.field(Code,CODES[code-1]);
	}

	public function sendMessage( code : Code, msg : String ) {
		var o = sock.output;
		o.writeByte( Type.enumIndex(code) + 1 );
		o.writeUInt24( msg.length );
		o.writeString( msg );
	}

	public function processMessage() {
		var code = readMessage();
		//trace(Std.string(code)+" ["+data+"]");
		switch( code ) {
		case CFile: file = data;
		case CUri: uri = data;
		case CClientIP: ip = data;
		case CGetParams: getParams = data;
		case CPostData: postData = data;
		case CHeaderKey: key = data;
		case CHeaderValue, CHeaderAddValue: headers.push({ k : key, v : data });
		case CParamKey: key = data;
		case CParamValue: params.push({ k : key, v : data });
		case CHostName: hostName = data;
		case CHttpMethod: httpMethod = data;
		case CExecute: return true;
		case CError: throw data;
		default: throw "Unexpected "+Std.string(code);
		}
		data = null;
		return false;
	}

}
