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

class Api {

	public static function getInfos() : Infos {
		return neko.Lib.load("mod_neko","tora_infos",0)();
	}

	public static function command( cmd : String, ?param : String ) : Dynamic {
		return neko.Lib.load("mod_neko","tora_command",2)(cmd,param);
	}

	public static function unsafeRequest() : Bool {
		return neko.Lib.load("mod_neko","tora_unsafe",0)();
	}

	public static function call( uri : String, ?delay : Float, ?needResult : String ) : String {
		var res = needResult == null ? null : neko.NativeString.ofString(needResult);
		var r = tora_call(neko.NativeString.ofString(uri),delay,res);
		return (r == null) ? null : neko.NativeString.toString(r);
	}

	static var tora_call = neko.Lib.loadLazy("mod_neko","tora_call",3);

}