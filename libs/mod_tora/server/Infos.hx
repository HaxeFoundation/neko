
typedef ThreadInfos = {
	var hits : Int;
	var file : String;
	var url : String;
	var time : Float;
}

typedef CacheInfos = {
	var file : String;
	var hits : Int;
	var count : Int;
}

typedef Infos = {
	var threads : Array<ThreadInfos>;
	var cache : Array<CacheInfos>;
	var hits : Int;
	var queue : Int;
	var memoryUsed : Int;
	var memoryTotal : Int;
}