/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
/*  Copyright (c)2005 Motion-Twin											*/
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
