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

class Admin {

	static function w(str) {
		neko.Lib.println(str);
	}

	static function f( v : Float ) {
		return Math.round(v * 10) / 10;
	}

	static function main() {
		var infos : Infos = neko.Lib.load("mod_neko","tora_infos",0)();
		neko.Web.setHeader("Content-Type","text/plain");
		var busy = 0;
		for( t in infos.threads )
			if( t.current != null )
				busy++;
		w("--- Tora Admin ---");
		w("Total hits : "+infos.hits);
		w("Queue size : "+infos.queue);
		w("Threads : "+busy+" / "+infos.threads.length);
		w("");
		w("--- Threads ---");
		var count = 1;
		for( t in infos.threads ) {
			neko.Lib.print((count++)+"\t"+t.hits+" hits"+"\t\t");
			if( t.current == null )
				w("idle since "+f(t.time)+"s");
			else
				w("running "+t.current+" since "+f(t.time)+"s");
		}
	}

}