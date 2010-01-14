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

private enum PType {
	PRaw;
	PString;
	PDate;
	PAnon( fields : Array<{ name : String, t : PType }>, obj : {} );
	PArrayRaw;
	PArray( t : PType );
	PBytes;
	PNativeArray( t : PType );
}

class Persist<T> {

	var cl : Class<T>;
	var schema : PType;

	public function new( c : Class<T> ) {
		cl = c;
		var rtti : String = untyped c.__rtti;
		if( rtti == null ) throw "Class "+Type.getClassName(c)+" needs RTTI";
		var t = new haxe.rtti.XmlParser().processElement(Xml.parse(rtti).firstElement());
		switch( t ) {
		case TClassdecl(cc):
			var fields = new Array();
			for( f in cc.fields ) {
				switch( f.type ) {
				case CFunction(_,_): continue;
				default:
				}
				try {
					fields.push({ name : f.name, t : processType(f.type) });
				} catch( err : String ) {
					neko.Lib.rethrow(err+" in "+Type.getClassName(c)+"."+f.name);
				}
			}
			schema = PAnon(fields,emptyObj(fields));
		default: throw "Invalid class";
		}
	}

	function emptyObj( fields : Array<{ name : String, t : PType }> ) {
		var o = {};
		for( f in fields )
			Reflect.setField(o,f.name,null);
		return o;
	}

	function processType( c : haxe.rtti.CType ) {
		return switch( c ) {
			case CUnknown: throw "Unsupported Unknown";
			case CEnum(e,_): throw "Unsupported enum "+e;
			case CDynamic(_): throw "Unsupported Dynamic";
			case CFunction(_,_): throw "Unsupported function";
			case CClass(c,pl):
				switch( c ) {
				case "Array":
					var t = processType(pl.first());
					if( t == PRaw )
						PArrayRaw;
					else
						PArray(t);
				case "String": PString;
				case "Date": PDate;
				case "Int", "Float", "neko.NativeString": PRaw;
				case "haxe.io.Bytes": PBytes;
				case "neko.NativeArray":
					var t = processType(pl.first());
					if( t == PRaw )
						PRaw;
					else
						PNativeArray(t);
				default: throw "Unsupported class "+c;
				}
			case CAnonymous(a):
				var fields = new Array();
				var raw = true;
				for( f in a ) {
					var t = processType(f.t);
					if( t != PRaw ) raw = false;
					fields.push({ name : f.name, t : t });
				}
				if( raw ) PRaw else PAnon(fields,emptyObj(fields));
			case CTypedef(t,pl):
				if( t.substr(0,6) == "mt.db." ) {
					switch( t.substr(6) ) {
					case "SId", "SInt", "SUInt", "SBigId", "SBigInt", "SFloatLow", "SFloat", "SBool", "SEncoded", "SColor": PRaw;
					case "SString","STinyText","SSmallText", "SText", "SSmallBinary", "SBinary", "SLongBinary": PString;
					case "SDate","SDateTime": PDate;
					case "SNull": processType(pl.first());
					default:
						throw "Unsupported type "+t;
					}
				} else switch( t ) {
				default: throw "Unsupported type "+t;
				}
		};
	}

	public function makePersistent( v : T ) : Dynamic {
		return unwrap(v,schema);
	}

	function unwrap( v : Dynamic, t : PType ) : Dynamic {
		return switch( t ) {
			case PRaw: v;
			case PString: if( v == null ) null else v.__s;
			case PDate: if( v == null ) null else v.__t;
			case PArrayRaw: if( v == null ) null else untyped __dollar__array(v.__a,v.length);
			case PArray(t): if( v == null ) null else {
				var src : neko.NativeArray<Dynamic> = v.__a;
				var max = v.length;
				var dst = neko.NativeArray.alloc(max);
				var i = 0;
				while( i < max ) {
					dst[i] = unwrap(src[i],t);
					i++;
				}
				dst;
			}
			case PAnon(fields,obj): if( v == null ) null else {
				var dst : {} = untyped __dollar__new(obj);
				for( f in fields )
					Reflect.setField(dst,f.name,unwrap(Reflect.field(v,f.name),f.t));
				dst;
			}
			case PBytes: if( v == null ) null else v.b;
			case PNativeArray(t): if( v == null ) null else {
				var max = neko.NativeArray.length(v);
				var dst = neko.NativeArray.alloc(max);
				var i = 0;
				while( i < max ) {
					dst[i] = unwrap(v[i],t);
					i++;
				}
				dst;
			}
		};
	}

	public function makeInstance( v : Dynamic ) : T {
		if( v == null )
			return null;
		return switch( schema ) {
		case PAnon(fields,_):
			var o = Type.createEmptyInstance(cl);
			for( f in fields )
				Reflect.setField(o,f.name,wrap(Reflect.field(v,f.name),f.t));
			o;
		default: throw "assert";
		}
	}

	function wrap( v : Dynamic, t : PType ) : Dynamic {
		return switch( t ) {
			case PRaw: v;
			case PString: if( v == null ) null else cast new String(v);
			case PDate: if( v == null ) null else (cast Date).new1(v);
			case PArrayRaw: if( v == null ) null else {
				var a : Dynamic = new Array();
				a.__a = v[0];
				a.length = v[1];
				a;
			}
			case PArray(t): if( v == null ) null else {
				var src : neko.NativeArray<Dynamic> = v;
				var max = neko.NativeArray.length(src);
				var dst = neko.NativeArray.alloc(max);
				var i = 0;
				while( i < max ) {
					dst[i] = wrap(src[i],t);
					i++;
				}
				neko.NativeArray.toArray(dst);
			}
			case PAnon(fields,obj): if( v == null ) null else {
				var dst : {} = untyped __dollar__new(obj);
				for( f in fields )
					Reflect.setField(dst,f.name,wrap(Reflect.field(v,f.name),f.t));
				dst;
			}
			case PBytes: if( v == null ) null else haxe.io.Bytes.ofData(v);
			case PNativeArray(t): if( v == null ) null else {
				var src : neko.NativeArray<Dynamic> = v;
				var max = neko.NativeArray.length(src);
				var dst = neko.NativeArray.alloc(max);
				var i = 0;
				while( i < max ) {
					dst[i] = wrap(src[i],t);
					i++;
				}
				dst;
			}
		};
	}

}