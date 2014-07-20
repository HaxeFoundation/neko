// haxe program to generate unicase.c from UnicodeData.txt
class UCase {
	static function main(){
		var h = sys.io.File.getContent("UnicodeData.txt");
		var low = 0, up = 0, max = 0;
		var a = [], b = [];
		var bits = 6;
		for( l in h.split("\n") ) {
			var l = l.split(";");
			var code = Std.parseInt("0x"+l[0]);
			if( code == 0 ) continue;
			if( l[13] != "" ) {
				var k = a[code >> bits];
				if( k == null )
					a[code>>bits] = k = [for( i in 0...1<<bits ) 0];
				k[code & ((1<<bits) - 1)] = Std.parseInt("0x"+l[13]);
			}
			if( l[14] != "" ) {
				var k = b[code >> bits];
				if( k == null )
					b[code>>bits] = k = [for( i in 0...1<<bits ) 0];
				k[code & ((1<<bits) - 1)] = Std.parseInt("0x"+l[14]);
			}
			if( (l[13] != "" || l[14] != "") && code > max )
				max = code;
		}
		var sz = a.length;
		for( i in 0...a.length )
			if( a[i] != null )
				sz += a[i].length;
		var bsz = b.length;
		for( i in 0...b.length )
			if( b[i] != null )
				bsz += b[i].length;
		Sys.print("#define UL_BITS "+bits+"\n");		
		Sys.print("#define UL_SIZE "+(1<<bits)+"\n");
		Sys.print('static uchar _E[UL_SIZE] = {'+[for( i in 0...1<<bits ) 0].join(',')+'};\n');
		for( i in 0...a.length ) {
			var k = a[i];
			if( k != null )
				Sys.print('static uchar L$i[UL_SIZE] = {${k.join(",")}};\n');
		}
		for( i in 0...b.length ) {
			var k = b[i];
			if( k != null )
				Sys.print('static uchar U$i[UL_SIZE] = {${k.join(",")}};\n');
		}
		Sys.print("#define LMAX "+a.length+"\n");
		Sys.print("#define UMAX "+b.length+"\n");
		Sys.print("static uchar *LOWER[LMAX] = {"+[for( i in 0...a.length ) if( a[i] == null ) "_E" else "L"+i].join(",")+"};\n");
		Sys.print("static uchar *UPPER[UMAX] = {"+[for( i in 0...b.length ) if( b[i] == null ) "_E" else "U"+i].join(",")+"};\n");
	}
}
