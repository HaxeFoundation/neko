#include <neko.h>

field id_h;
field id_m;
field id_s;
field id_y;
field id_d;

DEFINE_ENTRY_POINT(main);

void main() {
	id_h = val_id("h");
	id_m = val_id("m");
	id_s = val_id("s");
	id_y = val_id("y");
	id_d = val_id("d");
}
