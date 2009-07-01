package tora;

class Api {

	public static function getInfos() : Infos {
		return neko.Lib.load("mod_neko","tora_infos",0)();
	}

	public static function command( cmd : String, ?param : String ) : Dynamic {
		return neko.Lib.load("mod_neko","tora_command",2)(cmd,param);
	}

}