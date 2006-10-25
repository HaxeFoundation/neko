/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
/*  Copyright (c)2005-2006 Motion-Twin										*/
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
#include <neko_vm.h>
#ifdef NEKO_WINDOWS
#	include <windows.h>
#	define val_thread(t)	((DWORD)(int_val)val_data(t))
#	define val_lock(l)		((HANDLE)val_data(l))
#	define WM_TMSG			(WM_USER + 1)
#endif

DEFINE_KIND(k_thread);
DEFINE_KIND(k_lock);

typedef struct {
#	ifdef NEKO_WINDOWS
	HANDLE thandle;
	HANDLE tlock;
#	else
#	endif
	value callb;
	value callparam;
} vthread;

#ifdef NEKO_WINDOWS
static DWORD WINAPI ThreadMain( void *_t ) {
	vthread *t = (vthread*)_t;
	value exc = NULL;
	neko_vm *vm;
	// this will create the thread message queue
	PeekMessage(NULL,NULL,0,0,0);
	// now we can give back control to the main thread
	ReleaseSemaphore(t->tlock,1,NULL);
	// init and run the VM
	vm = neko_vm_alloc(NULL);
	neko_vm_select(vm);
	val_callEx(val_null,t->callb,&t->callparam,1,&exc);
	// cleanup
	vm = NULL;
	CloseHandle(t->thandle);
	return 0;
}
#endif

/**
	thread_create : f:function:1 -> p:any -> 'thread
	<doc>Creates a thread that will be running the function [f(p)]</doc>
**/
static value thread_create( value f, value param ) {
	vthread *t;
	void *tid;
	val_check_function(f,1);
	t = (vthread*)alloc(sizeof(vthread));
	memset(t,0,sizeof(vthread));
	t->callb = f;
	t->callparam = param;
#	ifdef NEKO_WINDOWS
	t->tlock = CreateSemaphore(NULL,0,1,NULL);
	if( t->tlock == NULL )
		neko_error();
	{
		DWORD id;
		t->thandle = CreateThread(NULL,0,ThreadMain,t,0,&id);
		tid = (void*)(int_val)id;
	}
	if( t->thandle == NULL ) {
		CloseHandle(t->tlock);
		neko_error();
	}
	WaitForSingleObject(t->tlock,INFINITE);
	CloseHandle(t->tlock);
#	else
	neko_error();
#	endif
	return alloc_abstract(k_thread,tid);
}

/**
	thread_current : void -> 'thread
	<doc>Returns the current thread</doc>
**/
static value thread_current() {
	void *tid;
#	ifdef NEKO_WINDOWS
	tid = (void*)(int_val)GetCurrentThreadId();
#	else
	neko_error();
#	endif
	return alloc_abstract(k_thread,tid);
}

/**
	thread_send : 'thread -> msg:any -> void
	<doc>Send a message into the target thread message queue</doc>
**/
static value thread_send( value t, value msg ) {
	value *r;
	val_check_kind(t,k_thread);
	r = alloc_root(1);
	*r = msg;
#	ifdef NEKO_WINDOWS
	if( !PostThreadMessage(val_thread(t),WM_TMSG,0,(LPARAM)r) )
		neko_error();
#	else
	neko_error();
#	endif
	return val_null;
}

/**
	thread_read_message : block:bool -> any
	<doc>
	Reads a message from the message queue. If [block] is true, the
	function only returns when a message is available. If [block] is
	false and no message is available in the queue, the function will
	return immediatly [null].
	</doc>
**/
static value thread_read_message( value block ) {
	value *r, v;
#	ifdef NEKO_WINDOWS
	MSG msg;
	val_check(block,bool);
	if( !val_bool(block) ) {
		if( !PeekMessage(&msg,NULL,0,0,PM_REMOVE) )
			return val_null;
	} else if( !GetMessage(&msg,NULL,0,0) )
		neko_error();	
	switch( msg.message ) {
	case WM_TMSG:
		r = (value*)msg.lParam;
		v = *r;
		free_root(r);
		return v;
	default:
		neko_error();
		break;
	}
#	else
	val_check(block,bool);
	neko_error();
#	endif
	return val_null;
}

static void free_lock( value l ) {
#	ifdef NEKO_WINDOWS
	CloseHandle( val_lock(l) );
#	else
#	endif
}

/**
	lock_create : void -> 'lock
	<doc>Creates a lock, initialy owned by the current thread</doc>
**/
static value lock_create() {
	value l;
	void *data;
#	ifdef NEKO_WINDOWS
	data = CreateSemaphore(NULL,0,(1 << 10),NULL);
	if( data == NULL )
		neko_error();
#	else
	neko_error();
#	endif
	l = alloc_abstract(k_lock,data);
	val_gc(l,free_lock);
	return l;
}

/**
	lock_release : 'lock -> void
	<doc>Release a lock. The thread does not need to own the lock to be
	able to release it. If a lock is released several times, it can be
	acquired as many times.</doc>
**/
static value lock_release( value lock ) {
	val_check_kind(lock,k_lock);
#	ifdef NEKO_WINDOWS
	if( !ReleaseSemaphore(val_lock(lock),1,NULL) )
		neko_error();
#	else
	neko_error();
#	endif
	return val_true;
}

/**
	lock_wait : 'lock -> timeout:number? -> bool
	<doc>
	Waits for a lock to be released and acquire it.
	If [timeout] (in seconds) is not null and expires then
	the returned value is false.
	</doc>	
**/
static value lock_wait( value lock, value timeout ) {
	int has_timeout = !val_is_null(timeout);
	val_check_kind(lock,k_lock);
	if( !has_timeout )
		val_check(timeout,number);
#	ifdef NEKO_WINDOWS
	switch( WaitForSingleObject(val_lock(lock),has_timeout?(DWORD)(val_number(timeout) * 1000):INFINITE) ) {
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return val_true;
	case WAIT_TIMEOUT:
		return val_false;
	default:
		neko_error();
	}
#	endif
	return val_null;
}

DEFINE_PRIM(thread_create,2);
DEFINE_PRIM(thread_current,0);
DEFINE_PRIM(thread_send,2);
DEFINE_PRIM(thread_read_message,1);

DEFINE_PRIM(lock_create,0);
DEFINE_PRIM(lock_wait,2);
DEFINE_PRIM(lock_release,1);
