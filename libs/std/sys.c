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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef NEKO_WINDOWS
#	include <windows.h>
#	include <direct.h>
#	include <conio.h>
#else
#	include <unistd.h>
#	include <dirent.h>
#	include <limits.h>
#	include <termios.h>
#	include <sys/time.h>
#	include <sys/times.h>
#endif

#ifdef NEKO_MAC
#	include <sys/syslimits.h>
#	include <limits.h>
#	include <mach-o/dyld.h>
#endif

#ifndef CLK_TCK
#	define CLK_TCK	100
#endif

/**
	<doc>
	<h1>System</h1>
	<p>
	Interactions with the operating system.
	</p>
	</doc>
**/

/**
	get_env : string -> string?
	<doc>Get some environment variable if exists</doc>
**/
static value get_env( value v ) {
	char *s;
	val_check(v,string);
	s = getenv(val_string(v));
	if( s == NULL )
		return val_null;
	return alloc_string(s);
}

/**
	put_env : var:string -> val:string -> void
	<doc>Set some environment variable value</doc>
**/
static value put_env( value e, value v ) {
#ifdef NEKO_WINDOWS
	buffer b;
	val_check(e,string);
	val_check(v,string);
	b = alloc_buffer(NULL);
	val_buffer(b,e);
	buffer_append_sub(b,"=",1);
	val_buffer(b,v);
	if( putenv(val_string(buffer_to_string(b))) != 0 )
		neko_error();
#else
	val_check(e,string);
	val_check(v,string);
	if( setenv(val_string(e),val_string(v),1) != 0 )
		neko_error();
#endif
	return val_true;
}

/**
	sys_sleep : number -> void
	<doc>Sleep a given number of seconds</doc>
**/
static value sys_sleep( value f ) {
	val_check(f,number);
#ifdef NEKO_WINDOWS
	Sleep((DWORD)(val_number(f) * 1000));
#else
	if( (int)val_number(f) > 0 )
		sleep((int)val_number(f));
	usleep( (int)((val_number(f) - (int)val_number(f)) * 1000000) );
#endif
	return val_true;
}

/**
	set_time_locale : string -> bool
	<doc>Set the locale for LC_TIME, returns true on success</doc>
**/
static value set_time_locale( value l ) {
	val_check(l,string);
	return alloc_bool(setlocale(LC_TIME,val_string(l)) != NULL);
}

/**
	get_cwd : void -> string
	<doc>Return current working directory</doc>
**/
static value get_cwd() {
	char buf[256];
	int l;
	if( getcwd(buf,256) == NULL )
		neko_error();
	l = (int)strlen(buf);
	if( buf[l-1] != '/' && buf[l-1] != '\\' ) {
		buf[l] = '/';
		buf[l+1] = 0;
	}
	return alloc_string(buf);
}

/**
	set_cwd : string -> void
	<doc>Set current working directory</doc>
**/
static value set_cwd( value d ) {
	val_check(d,string);
	if( chdir(val_string(d)) )
		neko_error();
	return val_true;
}


/**
	sys_string : void -> string
	<doc>
	Return the local system string. The current value are possible :
	<ul>
	<li>[Windows]</li>
	<li>[Linux]</li>
	<li>[BSD]</li>
	<li>[Mac]</li>
	</ul>
	</doc>
**/
static value sys_string() {
#if defined(NEKO_WINDOWS)
	return alloc_string("Windows");
#elif defined(NEKO_GNUKBSD)
	return alloc_string("GNU/kFreeBSD");
#elif defined(NEKO_LINUX)
	return alloc_string("Linux");
#elif defined(NEKO_BSD)
	return alloc_string("BSD");
#elif defined(NEKO_MAC)
	return alloc_string("Mac");
#else
#error Unknow system string
#endif
}

/**
	sys_is64 : void -> bool
	<doc>
	Returns true if we are on a 64-bit system
	</doc>
**/
static value sys_is64() {
#ifdef NEKO_64BITS
	return val_true;
#else
	return val_false;
#endif
}

/**
	sys_command : string -> int
	<doc>Run the shell command and return exit code</doc>
**/
static value sys_command( value cmd ) {
	val_check(cmd,string);
	if( val_strlen(cmd) == 0 )
		return alloc_int(-1);
	return alloc_int( system(val_string(cmd)) );
}

/**
	sys_exit : int -> void
	<doc>Exit with the given errorcode. Never returns.</doc>
**/
static value sys_exit( value ecode ) {
	val_check(ecode,int);
	exit(val_int(ecode));
	return val_true;
}

/**
	sys_exists : string -> bool
	<doc>Returns true if the file or directory exists.</doc>
**/
static value sys_exists( value path ) {
	struct stat st;
	val_check(path,string);
	return alloc_bool(stat(val_string(path),&st) == 0);
}

/**
	file_exists : string -> bool
	<doc>Deprecated : use sys_exists instead.</doc>
**/
static value file_exists( value path ) {
	return sys_exists(path);
}

/**
	file_delete : string -> void
	<doc>Delete the file. Exception on error.</doc>
**/
static value file_delete( value path ) {
	val_check(path,string);
	if( unlink(val_string(path)) != 0 )
		neko_error();
	return val_true;
}

/**
	sys_rename : from:string -> to:string -> void
	<doc>Rename the file or directory. Exception on error.</doc>
**/
static value sys_rename( value path, value newname ) {
	val_check(path,string);
	val_check(newname,string);
	if( rename(val_string(path),val_string(newname)) != 0 )
		neko_error();
	return val_true;
}

#define STATF(f) alloc_field(o,val_id(#f),alloc_int(s.st_##f))
#define STATF32(f) alloc_field(o,val_id(#f),alloc_int32(s.st_##f))

/**
	sys_stat : string -> {
		gid => int,
		uid => int,
		atime => 'int32,
		mtime => 'int32,
		ctime => 'int32,
		dev => int,
		ino => int,
		nlink => int,
		rdev => int,
		mode => int,
		size => int
	}
	<doc>Run the [stat] command on the given file or directory.</doc>
**/
static value sys_stat( value path ) {
	struct stat s;
	value o;
	val_check(path,string);
	if( stat(val_string(path),&s) != 0 )
		neko_error();
	o = alloc_object(NULL);
	STATF(gid);
	STATF(uid);
	STATF32(atime);
	STATF32(mtime);
	STATF32(ctime);
	STATF(dev);
	STATF(ino);
	STATF(mode);
	STATF(nlink);
	STATF(rdev);
	STATF(size);
	STATF(mode);
	return o;
}

/**
	sys_file_type : string -> string
	<doc>
	Return the type of the file. The current values are possible :
	<ul>
	<li>[file]</li>
	<li>[dir]</li>
	<li>[symlink]</li>
	<li>[sock]</li>
	<li>[char]</li>
	<li>[block]</li>
	<li>[fifo]</li>
	</ul>
	</doc>
**/
static value sys_file_type( value path ) {
	struct stat s;
	val_check(path,string);
	if( stat(val_string(path),&s) != 0 )
		neko_error();
	if( s.st_mode & S_IFREG )
		return alloc_string("file");
	if( s.st_mode & S_IFDIR )
		return alloc_string("dir");
	if( s.st_mode & S_IFCHR )
		return alloc_string("char");
#ifndef NEKO_WINDOWS
	if( s.st_mode & S_IFLNK )
		return alloc_string("symlink");
	if( s.st_mode & S_IFBLK )
		return alloc_string("block");
	if( s.st_mode & S_IFIFO )
		return alloc_string("fifo");
	if( s.st_mode & S_IFSOCK )
		return alloc_string("sock");
#endif
	neko_error();
}

/**
	sys_create_dir : string -> mode:int -> void
	<doc>Create a directory with the specified rights</doc>
**/
static value sys_create_dir( value path, value mode ) {
	val_check(path,string);
	val_check(mode,int);
#ifdef NEKO_WINDOWS
	if( mkdir(val_string(path)) != 0 )
#else
	if( mkdir(val_string(path),val_int(mode)) != 0 )
#endif
		neko_error();
	return val_true;
}

/**
	sys_remove_dir : string -> void
	<doc>Remove a directory. Exception on error</doc>
**/
static value sys_remove_dir( value path ) {
	val_check(path,string);
	if( rmdir(val_string(path)) != 0 )
		neko_error();
	return val_true;
}

/**
	sys_time : void -> float
	<doc>Return an accurate local time stamp in seconds since Jan 1 1970</doc>
**/
static value sys_time() {
#ifdef NEKO_WINDOWS
#define EPOCH_DIFF	(134774*24*60*60.0)
	SYSTEMTIME t;
	FILETIME ft;
    ULARGE_INTEGER ui;
	GetSystemTime(&t);
	if( !SystemTimeToFileTime(&t,&ft) )
		neko_error();
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
	return alloc_float( ((tfloat)ui.QuadPart) / 10000000.0 - EPOCH_DIFF );
#else
	struct timeval tv;
	if( gettimeofday(&tv,NULL) != 0 )
		neko_error();
	return alloc_float( tv.tv_sec + ((tfloat)tv.tv_usec) / 1000000.0 );
#endif
}

/**
	sys_cpu_time : void -> float
	<doc>Return the most accurate CPU time spent since the process started (in seconds)</doc>
**/
static value sys_cpu_time() {
#ifdef NEKO_WINDOWS
	FILETIME unused;
	FILETIME stime;
	FILETIME utime;
	if( !GetProcessTimes(GetCurrentProcess(),&unused,&unused,&stime,&utime) )
		neko_error();
	return alloc_float( ((tfloat)(utime.dwHighDateTime+stime.dwHighDateTime)) * 65.536 * 6.5536 + (((tfloat)utime.dwLowDateTime + (tfloat)stime.dwLowDateTime) / 10000000) );
#else
	struct tms t;
	times(&t);
	return alloc_float( ((tfloat)(t.tms_utime + t.tms_stime)) / CLK_TCK );
#endif
}

/**
	sys_read_dir : string -> string list
	<doc>Return the content of a directory</doc>
**/
static value sys_read_dir( value path ) {
	value h = val_null;
	value cur = NULL, tmp;
#ifdef NEKO_WINDOWS
	WIN32_FIND_DATA d;
	HANDLE handle;
	buffer b;
	int len;
	val_check(path,string);
	len = val_strlen(path);
	b = alloc_buffer(NULL);
	val_buffer(b,path);
	if( len && val_string(path)[len-1] != '/' && val_string(path)[len-1] != '\\' )
		buffer_append(b,"/*.*");
	else
		buffer_append(b,"*.*");
	path = buffer_to_string(b);
	handle = FindFirstFile(val_string(path),&d);
	if( handle == INVALID_HANDLE_VALUE )
		neko_error();
	while( true ) {
		// skip magic dirs
		if( d.cFileName[0] != '.' || (d.cFileName[1] != 0 && (d.cFileName[1] != '.' || d.cFileName[2] != 0)) ) {
			tmp = alloc_array(2);
			val_array_ptr(tmp)[0] = alloc_string(d.cFileName);
			val_array_ptr(tmp)[1] = val_null;
			if( cur )
				val_array_ptr(cur)[1] = tmp;
			else
				h = tmp;
			cur = tmp;
		}
		if( !FindNextFile(handle,&d) )
			break;
	}
	FindClose(handle);
#else
	DIR *d;
	struct dirent *e;
	val_check(path,string);
	d = opendir(val_string(path));
	if( d == NULL )
		neko_error();
	while( true ) {
		e = readdir(d);
		if( e == NULL )
			break;
		// skip magic dirs
		if( e->d_name[0] == '.' && (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0)) )
			continue;
		tmp = alloc_array(2);
		val_array_ptr(tmp)[0] = alloc_string(e->d_name);
		val_array_ptr(tmp)[1] = val_null;
		if( cur )
			val_array_ptr(cur)[1] = tmp;
		else
			h = tmp;
		cur = tmp;
	}
	closedir(d);
#endif
	return h;
}

/**
	file_full_path : string -> string
	<doc>Return an absolute path from a relative one. The file or directory must exists</doc>
**/
static value file_full_path( value path ) {
#ifdef NEKO_WINDOWS
	char buf[MAX_PATH+1];
	val_check(path,string);
	if( GetFullPathName(val_string(path),MAX_PATH+1,buf,NULL) == 0 )
		neko_error();
	return alloc_string(buf);
#else
	char buf[PATH_MAX];
	val_check(path,string);
	if( realpath(val_string(path),buf) == NULL )
		neko_error();
	return alloc_string(buf);
#endif
}

/**
	sys_exe_path : void -> string
	<doc>Return the path of the executable</doc>
**/
static value sys_exe_path() {
#if defined(NEKO_WINDOWS)
	char path[MAX_PATH];
	if( GetModuleFileName(NULL,path,MAX_PATH) == 0 )
		neko_error();
	return alloc_string(path);
#elif defined(NEKO_MAC)
	char path[PATH_MAX+1];
	uint32_t path_len = PATH_MAX;
	if( _NSGetExecutablePath(path, &path_len) )
		neko_error();
	return alloc_string(path);
#else
	const char *p = getenv("_");
	if( p != NULL )
		return alloc_string(p);
	{
		char path[PATH_MAX];
		int length = readlink("/proc/self/exe", path, sizeof(path));
		if( length < 0 )
			neko_error();
	    path[length] = '\0';
		return alloc_string(path);
	}
#endif
}

#ifdef NEKO_MAC
#	define environ (*_NSGetEnviron())
#endif

#ifndef NEKO_WINDOWS
extern char **environ;
#endif

/**
	sys_env : void -> #list
	<doc>Return all the (key,value) pairs in the environment as a chained list</doc>
**/
static value sys_env() {
	value h = val_null;
	value cur = NULL, tmp, key;
	char **e = environ;
	while( *e ) {
		char *x = strchr(*e,'=');
		if( x == NULL ) {
			e++;
			continue;
		}
		tmp = alloc_array(3);
		key = alloc_empty_string((int)(x - *e));
		memcpy(val_string(key),*e,(int)(x - *e));
		val_array_ptr(tmp)[0] = key;
		val_array_ptr(tmp)[1] = alloc_string(x+1);
		val_array_ptr(tmp)[2] = val_null;
		if( cur )
			val_array_ptr(cur)[2] = tmp;
		else
			h = tmp;
		cur = tmp;
		e++;
	}
	return h;
}

/**
	sys_getch : bool -> int
	<doc>Read a character from stdin with or without echo</doc>
**/
static value sys_getch( value b ) {
#	ifdef NEKO_WINDOWS
	val_check(b,bool);
	return alloc_int( val_bool(b)?getche():getch() );
#	else
	// took some time to figure out how to do that
	// without relying on ncurses, which clear the
	// terminal on initscr()
	int c;
	struct termios term, old;
	val_check(b,bool);
	tcgetattr(fileno(stdin), &old);
	term = old;
	cfmakeraw(&term);
	tcsetattr(fileno(stdin), 0, &term);
	c = getchar();
	tcsetattr(fileno(stdin), 0, &old);
	if( val_bool(b) ) fputc(c,stdout);
	return alloc_int(c);
#	endif
}

/**
	sys_get_pid : void -> int
	<doc>Returns the current process identifier</doc>
**/
static value sys_get_pid() {
#	ifdef NEKO_WINDOWS
	return alloc_int(GetCurrentProcessId());
#	else
	return alloc_int(getpid());
#	endif
}

DEFINE_PRIM(get_env,1);
DEFINE_PRIM(put_env,2);
DEFINE_PRIM(set_time_locale,1);
DEFINE_PRIM(get_cwd,0);
DEFINE_PRIM(set_cwd,1);
DEFINE_PRIM(sys_sleep,1);
DEFINE_PRIM(sys_command,1);
DEFINE_PRIM(sys_exit,1);
DEFINE_PRIM(sys_string,0);
DEFINE_PRIM(sys_is64,0);
DEFINE_PRIM(sys_stat,1);
DEFINE_PRIM(sys_time,0);
DEFINE_PRIM(sys_cpu_time,0);
DEFINE_PRIM(sys_env,0);
DEFINE_PRIM(sys_create_dir,2);
DEFINE_PRIM(sys_remove_dir,1);
DEFINE_PRIM(sys_read_dir,1);
DEFINE_PRIM(file_full_path,1);
DEFINE_PRIM(file_exists,1);
DEFINE_PRIM(sys_exists,1);
DEFINE_PRIM(file_delete,1);
DEFINE_PRIM(sys_rename,2);
DEFINE_PRIM(sys_exe_path,0);
DEFINE_PRIM(sys_file_type,1);
DEFINE_PRIM(sys_getch,1);
DEFINE_PRIM(sys_get_pid,0);

/* ************************************************************************ */
