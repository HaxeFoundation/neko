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

#ifdef NEKO_WINDOWS
#	include <windows.h>
#else
#	include <sys/types.h>
#	include <unistd.h>
#	include <errno.h>
#	ifndef NEKO_MAC
#		include <wait.h>
#	endif
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

/**
	<doc>
	<h1>Process</h1>
	<p>
	An API for starting and communication with sub processes.
	</p>
	</doc>
**/
#ifndef NEKO_WINDOWS
static int do_close( int fd ) {
	POSIX_LABEL(close_again);
	if( close(fd) != 0 ) {
		HANDLE_EINTR(close_again);
		return 1;
	}
	return 0;
}
#endif

static void free_process( value vp ) {
	vprocess *p = val_process(vp);
#	ifdef NEKO_WINDOWS
	CloseHandle(p->eread);
	CloseHandle(p->oread);
	CloseHandle(p->iwrite);
	CloseHandle(p->pinf.hProcess);
	CloseHandle(p->pinf.hThread);
#	else
	do_close(p->eread);
	do_close(p->oread);
	do_close(p->iwrite);
#	endif
}

/**
	process_run : cmd:string -> args:string array -> 'process
	<doc>
	Start a process using a command and the specified arguments.
	</doc>
**/
static value process_run( value cmd, value vargs ) {
	int i;
	vprocess *p;
	val_check(cmd,string);
	val_check(vargs,array);
#	ifdef NEKO_WINDOWS
	{		 
		SECURITY_ATTRIBUTES sattr;		
		STARTUPINFO sinf;
		HANDLE proc = GetCurrentProcess();
		HANDLE oread,eread,iwrite;
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
		CreatePipe(&oread,&sinf.hStdOutput,&sattr,0);
		CreatePipe(&eread,&sinf.hStdError,&sattr,0);
		CreatePipe(&sinf.hStdInput,&iwrite,&sattr,0);
		DuplicateHandle(proc,oread,proc,&p->oread,0,FALSE,DUPLICATE_SAME_ACCESS);
		DuplicateHandle(proc,eread,proc,&p->eread,0,FALSE,DUPLICATE_SAME_ACCESS);
		DuplicateHandle(proc,iwrite,proc,&p->iwrite,0,FALSE,DUPLICATE_SAME_ACCESS);
		CloseHandle(oread);
		CloseHandle(eread);
		CloseHandle(iwrite);
		if( !CreateProcess(NULL,val_string(sargs),NULL,NULL,TRUE,0,NULL,NULL,&sinf,&p->pinf) )			
			neko_error();
		// close unused pipes
		CloseHandle(sinf.hStdOutput);
		CloseHandle(sinf.hStdError);
		CloseHandle(sinf.hStdInput);
	}
#	else
	char **argv = (char**)alloc_private(sizeof(char*)*(val_array_size(vargs)+2));
	argv[0] = val_string(cmd);
	for(i=0;i<val_array_size(vargs);i++) {
		value v = val_array_ptr(vargs)[i];
		val_check(v,string);
		argv[i+1] = val_string(v);
	}
	argv[i+1] = NULL;
	int input[2], output[2], error[2];
	if( pipe(input) || pipe(output) || pipe(error) )
		neko_error();
	p = (vprocess*)alloc_private(sizeof(vprocess));
	p->pid = fork();
	if( p->pid == -1 ) {
		do_close(input[0]);
		do_close(input[1]);
		do_close(output[0]);
		do_close(output[1]);
		do_close(error[0]);
		do_close(error[1]);
		neko_error();
	}
	// child
	if( p->pid == 0 ) {
		close(input[1]);
		close(output[0]);
		close(error[0]);
		dup2(input[0],0);
		dup2(output[1],1);
		dup2(error[1],2);
		execvp(val_string(cmd),argv);
		fprintf(stderr,"Command not found : %s\n",val_string(cmd));
		exit(1);
	}
	// parent
	do_close(input[0]);
	do_close(output[1]);
	do_close(error[1]);
	p->iwrite = input[1];
	p->oread = output[0];
	p->eread = error[0];
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


/**
	process_stdout_read : 'process -> buf:string -> pos:int -> len:int -> int
	<doc>
	Read up to [len] bytes in [buf] starting at [pos] from the process stdout.
	Returns the number of bytes readed this way. Raise an exception if this
	process stdout is closed and no more data is available for reading.
	</doc>
**/
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
	int nbytes;
	POSIX_LABEL(stdout_read_again);
	nbytes = read(p->oread,val_string(str)+val_int(pos),val_int(len));
	if( nbytes < 0 ) {
		HANDLE_EINTR(stdout_read_again);
		neko_error();
	}
	if( nbytes == 0 )
		neko_error();
	return alloc_int(nbytes);
#	endif
}

/**
	process_stderr_read : 'process -> buf:string -> pos:int -> len:int -> int
	<doc>
	Read up to [len] bytes in [buf] starting at [pos] from the process stderr.
	Returns the number of bytes readed this way. Raise an exception if this
	process stderr is closed and no more data is available for reading.
	</doc>
**/
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
	int nbytes;
	POSIX_LABEL(stderr_read_again);
	nbytes = read(p->eread,val_string(str)+val_int(pos),val_int(len));
	if( nbytes < 0 ) {
		HANDLE_EINTR(stderr_read_again);
		neko_error();
	}
	if( nbytes == 0 )
		neko_error();
	return alloc_int(nbytes);
#	endif
}

/**
	process_stdin_write : 'process -> buf:string -> pos:int -> len:int -> int
	<doc>
	Write up to [len] bytes from [buf] starting at [pos] to the process stdin.
	Returns the number of bytes writen this way. Raise an exception if this
	process stdin is closed.
	</doc>
**/
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
	int nbytes;
	POSIX_LABEL(stdin_write_again);
	nbytes = write(p->iwrite,val_string(str)+val_int(pos),val_int(len));
	if( nbytes == -1 ) {
		HANDLE_EINTR(stdin_write_again);
		neko_error();
	}
	return alloc_int(nbytes);
#	endif
}

/**
	process_stdin_close : 'process -> void
	<doc>
	Close the process standard input.
	</doc>
**/
static value process_stdin_close( value vp ) {
	vprocess *p;
	val_check_kind(vp,k_process);
	p = val_process(vp);
#	ifdef NEKO_WINDOWS
	if( !CloseHandle(p->iwrite) )
		neko_error();
#	else
	if( do_close(p->iwrite) )
		neko_error();
	p->iwrite = -1;
#	endif
	return val_null;
}

/**
	process_exit : 'process -> int
	<doc>
	Wait until the process terminate, then returns its exit code.
	</doc>
**/
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
	if( !WIFEXITED(rval) )
		neko_error();
	return alloc_int(WEXITSTATUS(rval));
#	endif
}

/**
	process_pid : 'process -> int
	<doc>
	Returns the process id.
	</doc>
**/
static value process_pid( value vp ) {
	vprocess *p;
	val_check_kind(vp,k_process);
	p = val_process(vp);
#	ifdef NEKO_WINDOWS
	return alloc_int(p->pinf.dwProcessId);
#	else
	return alloc_int(p->pid);
#	endif
}

/**
	process_close : 'process -> void
	<doc>
	Close the process I/O.
	</doc>
**/
static value process_close( value vp ) {	
	val_check_kind(vp,k_process);
	free_process(vp);
	val_kind(vp) = NULL;
	val_gc(vp,NULL);
	return val_null;
}

/**
	process_kill : 'process -> void
	<doc>
	Terminates a running process.
	</doc>
**/
static value process_kill( value vp ) {
	val_check_kind(vp,k_process);
#	ifdef NEKO_WINDOWS
	TerminateProcess(val_process(vp)->pinf.hProcess,-1);
#	else
	kill(val_process(vp)->pid,9);
#	endif
	return val_null;
}

DEFINE_PRIM(process_run,2);
DEFINE_PRIM(process_stdout_read,4);
DEFINE_PRIM(process_stderr_read,4);
DEFINE_PRIM(process_stdin_close,1);
DEFINE_PRIM(process_stdin_write,4);
DEFINE_PRIM(process_exit,1);
DEFINE_PRIM(process_pid,1);
DEFINE_PRIM(process_close,1);
DEFINE_PRIM(process_kill,1);

/* ************************************************************************ */
