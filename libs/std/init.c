/*
 * Copyright (C)2005-2012 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <neko.h>

field id_h;
field id_m;
field id_s;
field id_y;
field id_d;
field id_module;
field id_loadmodule;
field id_loadprim;
field id_done;
field id_comment;
field id_xml;
field id_pcdata;
field id_cdata;
field id_doctype;
field id_serialize;
field id_unserialize;

DEFINE_ENTRY_POINT(std_main);
extern vkind k_file;
extern vkind k_socket;
extern vkind k_buffer;
extern vkind k_thread;

void std_main() {
	id_h = val_id("h");
	id_m = val_id("m");
	id_s = val_id("s");
	id_y = val_id("y");
	id_d = val_id("d");
	id_loadmodule = val_id("loadmodule");
	id_loadprim = val_id("loadprim");
	id_module = val_id("__module");
	id_done = val_id("done");
	id_comment = val_id("comment");
	id_xml = val_id("xml");
	id_pcdata = val_id("pcdata");
	id_cdata = val_id("cdata");
	id_doctype = val_id("doctype");
	id_serialize = val_id("__serialize");
	id_unserialize = val_id("__unserialize");	
	kind_share(&k_file,"file");
	kind_share(&k_socket,"socket");
	kind_share(&k_buffer,"buffer");
	kind_share(&k_thread,"thread");
}

/* ************************************************************************ */
