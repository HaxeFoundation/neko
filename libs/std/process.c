/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
/*  Copyright (c)2005-2007 Motion-Twin										*/
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

#ifdef NEKO_WINDOWS
#	include <windows.h>
#else
#	include <sys/types.h>
#	include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>

typedef struct {
#ifdef NEKO_WINDOWS
	HANDLE oread;
	HANDLE eread;
	HANDLE iwrite;
	PROCESS_INFORMATION pinf;
#else
	int oread;
	int eread;
	int iwrite;
	int pid;
#endif
} vprocess;

DEFINE_KIND(k_process);

#define val_process(v)	((vprocess*)val_data(v))

static void free_process( value vp ) {
	vprocess *p = val_process(vp);
#	ifdef NEKO_WINDOWS
	CloseHandle(p->eread);
	CloseHandle(p->oread);
	CloseHandle(p->iwrite);
	CloseHandle(p->pinf.hProcess);
	CloseHandle(p->pinf.hThread);
#	else
#	endif
}

static value process_run( value cmd, value vargs ) {
	int i;
	vprocess *p;
	val_check(cmd,string);
	val_check(vargs,array);
#	ifdef NEKO_WINDOWS
	{		 
		SECURITY_ATTRIBUTES sattr;		
		STARTUPINFO sinf;
		// creates commandline
		buffer b = alloc_buffer(NULL);
		value sargs;
		buffer_append_char(b,'"');
		val_buffer(b,cmd);
		buffer_append_char(b,'"');
		for(i=0;i<val_array_size(vargs);i++) {
			value v = val_array_ptr(vargs)[i];
			val_check(v,string);
			buffer_append(b," \"");
			val_buffer(b,v);
			buffer_append_char(b,'"');
		}
		sargs = buffer_to_string(b);
		p = (vprocess*)alloc_private(sizeof(vprocess));
		// startup process
		sattr.nLength = sizeof(sattr);
		sattr.bInheritHandle = TRUE;
		sattr.lpSecurityDescriptor = NULL;
		memset(&sinf,0,sizeof(sinf));
		sinf.cb = sizeof(sinf);
		sinf.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		sinf.wShowWindow = SW_HIDE;
		CreatePipe(&p->oread,&sinf.hStdOutput,&sattr,0);
		CreatePipe(&p->eread,&sinf.hStdError,&sattr,0);
		CreatePipe(&sinf.hStdInput,&p->iwrite,&sattr,0);
		SetHandleInformation(&p->oread,HANDLE_FLAG_INHERIT,0);
		SetHandleInformation(&p->eread,HANDLE_FLAG_INHERIT,0);
		SetHandleInformation(&p->iwrite,HANDLE_FLAG_INHERIT,0);
		if( !CreateProcess(NULL,val_string(sargs),NULL,NULL,TRUE,0,NULL,NULL,&sinf,&p->pinf) )			
			neko_error();
		// close unused pipes
		CloseHandle(sinf.hStdOutput);
		CloseHandle(sinf.hStdError);
		CloseHandle(sinf.hStdInput);
	}
#	else
	char **argv = (char**)alloc_private(sizeof(char*)*val_array_size(vargs));
	for(i=0;i<val_array_size(vargs);i++) {
		value v = val_array_ptr(vargs)[i];
		val_check(v,string);
		argv[i] = val_string(v);
	}
	int input[2], output[2], error[2];
	if( pipe(input) || pipe(output) || pipe(error) )
		neko_error();
	p->pid = fork();
	if( p->pid == -1 )
		neko_error();
	// child
	if( p->pid == 0 ) {
		close(input[1]);
		close(output[0]);
		close(error[0]);
		dup2(input[0],0);
		dup2(output[1],1);
		dup2(error[1],1);
		execv(val_string(cmd),argv);
		fprintf(stderr,"Command not found : %s\n",cmd);
		exit(1);
	}
	// parent
	close(input[0]);
	close(output[1]);
	close(error[1]);
	p->iwrite = input[1];
	p->oread = output[1];
	p->eread = error[1];
#	endif
	{
		value vp = alloc_abstract(k_process,p);
		val_gc(vp,free_process);
		return vp;
	}
}

#define CHECK_ARGS()	\
	vprocess *p; \
	val_check_kind(vp,k_process); \
	val_check(str,string); \
	val_check(pos,int); \
	val_check(len,int); \
	if( val_int(pos) < 0 || val_int(len) < 0 || val_int(pos) + val_int(len) > val_strlen(str) ) \
		neko_error(); \
	p = val_process(vp); \


static value process_stdout_read( value vp, value str, value pos, value len ) {
	CHECK_ARGS();
#	ifdef NEKO_WINDOWS
	{
		DWORD nbytes;
		if( !ReadFile(p->oread,val_string(str)+val_int(pos),val_int(len),&nbytes,NULL) )
			neko_error();
		return alloc_int(nbytes);
	}
#	else
	int nbytes = fread(val_string(str)+val_int(pos),1,val_int(len),p->oread);
	if( nbytes == -1 )
		neko_error();
	return alloc_int(nbytes);
#	endif
}

static value process_stderr_read( value vp, value str, value pos, value len ) {
	CHECK_ARGS();
#	ifdef NEKO_WINDOWS
	{
		DWORD nbytes;
		if( !ReadFile(p->eread,val_string(str)+val_int(pos),val_int(len),&nbytes,NULL) )
			neko_error();
		return alloc_int(nbytes);
	}
#	else
	int nbytes = fread(val_string(str)+val_int(pos),1,val_int(len),p->eread);
	if( nbytes == -1 )
		neko_error();
	return alloc_int(nbytes);
#	endif
}

static value process_stdin_write( value vp, value str, value pos, value len ) {
	CHECK_ARGS();
#	ifdef NEKO_WINDOWS
	{
		DWORD nbytes;
		if( !WriteFile(p->iwrite,val_string(str)+val_int(pos),val_int(len),&nbytes,NULL) )
			neko_error();
		return alloc_int(nbytes);
	}
#	else
	int nbytes = fwrite(val_string(str)+val_int(pos),1,val_int(len),p->iwrite);
	if( nbytes == -1 )
		neko_error();
	return alloc_int(nbytes);
#	endif
}

static value process_stdin_close( value vp ) {
	vprocess *p;
	val_check_kind(vp,k_process);
	p = val_process(vp);
#	ifdef NEKO_WINDOWS
	if( !CloseHandle(p->iwrite) )
		neko_error();
#	else
	if( close(p->iwrite) != 0 )
		neko_error();
#	endif
	return val_null;
}

static value process_exit( value vp ) {
	vprocess *p;
	val_check_kind(vp,k_process);
	p = val_process(vp);
#	ifdef NEKO_WINDOWS
	{
		DWORD rval;
		WaitForSingleObject(p->pinf.hProcess,INFINITE);
		if( !GetExitCodeProcess(p->pinf.hProcess,&rval) )
			neko_error();
		return alloc_int(rval);
	}
#	else
	int rval;
	while( waitpid(p->pid,&rval,0) != p->pid ) {
		if( errno == EINTR )
			continue;
		neko_error();
	}
	return alloc_int(rval);
#	endif
}

DEFINE_PRIM(process_run,2);
DEFINE_PRIM(process_stdout_read,4);
DEFINE_PRIM(process_stderr_read,4);
DEFINE_PRIM(process_stdin_close,1);
DEFINE_PRIM(process_stdin_write,4);
DEFINE_PRIM(process_exit,1);

/* ************************************************************************ */
