/*
 * Copyright (C)2015-2017 Haxe Foundation
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
#include "neko_elf.h"


static value elf_update_section_header_for_bytecode(value _file, value _interp_size, value _bytecode_size)
{
/* This function is a no-op on non-ELF platforms... */
#ifdef SEPARATE_SECTION_FOR_BYTECODE

	FILE *exe;
        int interp_size, bytecode_size;
	char buf[size_Shdr], *file;
	int bytecode_sec_idx;

	val_check(_file,string);
	val_check(_interp_size,int);
	val_check(_bytecode_size,int);
	file = val_string(_file);
	interp_size = val_int(_interp_size);
	bytecode_size = val_int(_bytecode_size);

	if ( interp_size%4 != 0 ) {
		return val_false;
	}

	/* Open the file to update the elf nekobytecode section
	   header... */
	exe = fopen(file,"r+b");
	if( exe == NULL )
		return val_false;

	/* First read the elf header... */
	if ( val_true != elf_read_header(exe) ) goto failed;

	/* Find the right section header... */
	bytecode_sec_idx = elf_find_bytecode_section(exe);
	if ( -1 == bytecode_sec_idx ) goto failed;

	/* Now that we have the right section header, update it... */
	if ( val_true != elf_read_section(exe,bytecode_sec_idx,buf) ) goto failed;

	elf_set_Shdr(buf,sh_type,SHT_PROGBITS);
	elf_set_Shdr(buf,sh_flags,elf_get_Shdr(buf,sh_flags) & (SHF_MASKOS|SHF_MASKPROC));
	elf_set_Shdr(buf,sh_addr,0);
	elf_set_Shdr(buf,sh_offset,interp_size);
	elf_set_Shdr(buf,sh_size,bytecode_size);
	elf_set_Shdr(buf,sh_addralign,1);
	elf_set_Shdr(buf,sh_entsize,0);

	/* ...and write it back... */
	if ( val_true != elf_write_section(exe,bytecode_sec_idx,buf) ) goto failed;

        elf_free_section_string_table();
	fclose(exe);
	return val_true;

failed:
        elf_free_section_string_table();
	fclose(exe);
	return val_false;
#else
	return val_true;
#endif
}

DEFINE_PRIM(elf_update_section_header_for_bytecode,3);

