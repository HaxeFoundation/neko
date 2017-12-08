/*
 * Copyright (C)2016-2017 Haxe Foundation
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

/* None of this is needed on non-ELF platforms... */
#ifdef SEPARATE_SECTION_FOR_BYTECODE

#include <string.h>
#include <stdlib.h>


/* Name of neko bytecode section... */
static const char const* BYTECODE_SEC_NAME = ".nekobytecode";

/* Must be big enough to hold Elf32_Ehdr or Elf64_Ehdr... */
int size_Ehdr = sizeof(Elf64_Ehdr);

/* Must be big enough to hold Elf32_Shdr or Elf64_Shdr... */
int size_Shdr = sizeof(Elf64_Shdr);


static int is_32, shoff, shent, shnum, shstr;
static char *strbuf;
static int strsize, stroff;


static value elf_read_exe(FILE *exe, int loc, char *buf, int size)
{
	if ( 0 != fseek(exe,loc,SEEK_SET) ||
             size != fread(buf,1,size,exe) ) {
		fclose(exe);
		return val_false;
	}
	return val_true;
}

static value elf_write_exe(FILE *exe, int loc, char *buf, int size)
{
	if ( 0 != fseek(exe,loc,SEEK_SET) ||
             size != fwrite(buf,1,size,exe) ) {
		fclose(exe);
		return val_false;
	}
	return val_true;
}

value elf_read_header(FILE *exe)
{
	char hdr[size_Ehdr];
	int hdrsize;

	/* First read the elf header to determine 32/64-bit-ness... */
	if ( val_true != elf_read_exe(exe,0,hdr,EI_NIDENT) ) return val_false;
	if ( hdr[EI_CLASS] == ELFCLASS32 || hdr[EI_CLASS] == ELFCLASS64 ) {
		is_32 = hdr[EI_CLASS] == ELFCLASS32;
	} else return val_false;

	/* Read the full elf header now... */
	hdrsize = is_32 ? sizeof(Elf32_Ehdr) : sizeof(Elf64_Ehdr);
	if ( val_true != elf_read_exe(exe,0,hdr,hdrsize) ) return val_false;

        if ( elf_get_Ehdr(hdr,e_type) != ET_EXEC ) return val_false;

        /* Remember the section headers info... */
	shoff = elf_get_Ehdr(hdr,e_shoff);
	shent = elf_get_Ehdr(hdr,e_shentsize);
	shnum = elf_get_Ehdr(hdr,e_shnum);
	shstr = elf_get_Ehdr(hdr,e_shstrndx);

        return val_true;
}

int elf_is_32()
{
	return is_32;
}

value elf_read_section(FILE *exe, int sec, char *buf)
{
	return elf_read_exe(exe,shoff+sec*shent,buf,shent);
}

value elf_write_section(FILE *exe, int sec, char *buf)
{
	return elf_write_exe(exe,shoff+sec*shent,buf,shent);
}

static value elf_read_section_string_table(FILE *exe)
{
	char buf[size_Ehdr];

        if ( NULL != strbuf ) return val_true;

	if ( val_true != elf_read_section(exe,shstr,buf) ) return val_false;
	stroff  = elf_get_Shdr(buf,sh_offset);
	strsize = elf_get_Shdr(buf,sh_size);
	strbuf = (char*) malloc(strsize);
	if ( val_true != elf_read_exe(exe,stroff,strbuf,strsize) ) return val_false;

	return val_true;
}

void elf_free_section_string_table()
{
	if ( NULL != strbuf ) {
		free(strbuf);
                strbuf = NULL;
	}
}

static int elf_find_section_by_name(FILE *exe, const char *name)
{
	char buf[size_Shdr];
	int shcur = 0, shname;

	if ( val_true != elf_read_section_string_table(exe) ) return -1;

	while ( shcur < shnum ) {
		if ( val_true != elf_read_section(exe,shcur,buf) ) return -1;
		shname = elf_get_Shdr(buf,sh_name);
		if ( shname < strsize && !strncmp(&strbuf[shname], name, strlen(name)) ) {
			/* found the .nekobytecode section! */
			return shcur;
		}
		shcur++;
	}
	return -1;
}

int elf_find_bytecode_section(FILE *exe)
{
	return elf_find_section_by_name(exe, BYTECODE_SEC_NAME);
}

value elf_find_embedded_bytecode(const char *file, int *beg, int *end)
{
	FILE *exe;
	char buf[size_Shdr];
	int bytecode_sec_idx;

	/* Open the file to update the elf nekobytecode section
	   header... */
	exe = fopen(file,"rb");
	if( exe == NULL )
		return val_false;

	/* First read the elf header... */
	if ( val_true != elf_read_header(exe) ) goto failed;

	/* Find the right section header... */
	bytecode_sec_idx = elf_find_bytecode_section(exe);
	if ( -1 == bytecode_sec_idx ) goto failed;

	if ( val_true != elf_read_section(exe,bytecode_sec_idx,buf) ) goto failed;

        elf_free_section_string_table();
	fclose(exe);

        if ( NULL != beg ) *beg = elf_get_Shdr(buf,sh_offset);
        if ( NULL != end ) *end = elf_get_Shdr(buf,sh_offset) + elf_get_Shdr(buf,sh_size);
        return val_true;

failed:
        elf_free_section_string_table();
	fclose(exe);
	return val_false;
}

#endif
