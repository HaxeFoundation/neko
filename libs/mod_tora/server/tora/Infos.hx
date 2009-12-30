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

typedef ThreadInfos = {
	var hits : Int;
	var errors : Int;
	var file : String;
	var url : String;
	var time : Float;
}

typedef FileInfos = {
	var file : String;
	var loads : Int;
	var cacheHits : Int;
	var cacheCount : Int;
	var bytes : Float;
	var time : Float;
}

typedef Infos = {
	var threads : Array<ThreadInfos>;
	var files : Array<FileInfos>;
	var totalHits : Int;
	var recentHits : Int;
	var queue : Int;
	var upTime : Float;
	var jit : Bool;
}