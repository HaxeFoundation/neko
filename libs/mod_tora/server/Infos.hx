
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
}

typedef Infos = {
	var threads : Array<ThreadInfos>;
	var files : Array<FileInfos>;
	var hits : Int;
	var queue : Int;
	var upTime : Float;
}