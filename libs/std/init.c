#include <neko.h>

field id_h;
field id_m;
field id_s;
field id_y;
field id_d;
field id_mod;
field id_loadmodule;

DEFINE_ENTRY_POINT(std_main);

void std_main() {
	id_h = val_id("h");
	id_m = val_id("m");
	id_s = val_id("s");
	id_y = val_id("y");
	id_d = val_id("d");
	id_loadmodule = val_id("loadmodule");
	id_mod = val_id("@m");
}
