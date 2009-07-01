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
	var notify : Int;
	var notifyRetry : Int;
	var queue : Int;
	var upTime : Float;
	var jit : Bool;
}