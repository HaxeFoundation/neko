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
#include <string.h>
#ifdef NEKO_WINDOWS
#	include <windows.h>
#	define WM_TMSG			(WM_USER + 1)

typedef HANDLE vlock;

#else
#	include <pthread.h>

typedef struct _tqueue {
	value msg;
	struct _tqueue *next;
} tqueue;

typedef struct _vlock {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int counter;
} *vlock;

#endif

// this is useful to ensure that calls to pthread_create
// are correctly redefined by the GC so it can scan their heap
#define GC_THREADS
#include <gc/gc.h>

#define val_thread(t)	((vthread*)val_data(t))
#define val_lock(l)		((vlock)val_data(l))

DEFINE_KIND(k_thread);
DEFINE_KIND(k_lock);

typedef struct {
#	ifdef NEKO_WINDOWS
	HANDLE thandle;
	HANDLE tlock;
	DWORD tid;
#	else	
	pthread_t phandle;
	tqueue *first;
	tqueue *last;	
	pthread_mutex_t qlock;
	pthread_mutex_t qwait;
#	endif
	value callb;
	value callparam;
} vthread;

#ifndef NEKO_WINDOWS

static pthread_key_t local_thread = (pthread_key_t)-1;

static void free_key( void *r ) {	
	free_root((value*)r);
}

static vthread *get_local_thread() {
	value *r;
	if( local_thread == (pthread_key_t)-1 )
		pthread_key_create(&local_thread,free_key);
	r = (value*)pthread_getspecific(local_thread);
	return (vthread*)(r?*r:NULL);
}

static void set_local_thread( vthread *t ) {
	value *r = (value*)pthread_getspecific(local_thread);
	if( r == NULL ) {
		r = alloc_root(1);
		pthread_setspecific(local_thread,r);
	}
	*r = (value)t;
}

static void init_thread_queue( vthread *t ) {
	pthread_mutex_init(&t->qlock,NULL);
	pthread_mutex_init(&t->qwait,NULL);
	pthread_mutex_lock(&t->qwait);
}

#endif


#ifdef NEKO_WINDOWS
static DWORD WINAPI ThreadMain( void *_t ) {
#else
static void *ThreadMain( void *_t ) {
#endif
	vthread *t = (vthread*)_t;
	value exc = NULL;
	neko_vm *vm;
#	ifdef NEKO_WINDOWS
	// this will create the thread message queue
	PeekMessage(NULL,NULL,0,0,0);
	// now we can give back control to the main thread
	ReleaseSemaphore(t->tlock,1,NULL);
#	else
	set_local_thread(t);
	pthread_mutex_unlock(&t->qlock);
#	endif
	// init and run the VM
	vm = neko_vm_alloc(NULL);
	neko_vm_select(vm);
	val_callEx(val_null,t->callb,&t->callparam,1,&exc);
	// cleanup
	vm = NULL;
#	ifdef NEKO_WINDOWS
	CloseHandle(t->thandle);
#	endif
	return 0;
}

/**
	thread_create : f:function:1 -> p:any -> 'thread
	<doc>Creates a thread that will be running the function [f(p)]</doc>
**/
static value thread_create( value f, value param ) {
	vthread *t;	
	val_check_function(f,1);
	t = (vthread*)alloc(sizeof(vthread));
	memset(t,0,sizeof(vthread));
	t->callb = f;
	t->callparam = param;
#	ifdef NEKO_WINDOWS
	t->tlock = CreateSemaphore(NULL,0,1,NULL);
	if( t->tlock == NULL )
		neko_error();
	t->thandle = CreateThread(NULL,0,ThreadMain,t,0,&t->tid);
	if( t->thandle == NULL ) {
		CloseHandle(t->tlock);
		neko_error();
	}
	WaitForSingleObject(t->tlock,INFINITE);
	CloseHandle(t->tlock);
#	else
	get_local_thread(); // ensure that the key is initialized
	init_thread_queue(t);
	pthread_mutex_lock(&t->qlock);
	if( pthread_create(&t->phandle,NULL,&ThreadMain,t) != 0 )
		neko_error();
	// wait that the thread unlock the data (prevent t from being GC)
	pthread_mutex_lock(&t->qlock);
	pthread_mutex_unlock(&t->qlock);
#	endif
	return alloc_abstract(k_thread,t);
}

/**
	thread_current : void -> 'thread
	<doc>Returns the current thread</doc>
**/
static value thread_current() {
#	ifdef NEKO_WINDOWS
	vthread *t = (vthread*)alloc(sizeof(vthread));
	memset(t,0,sizeof(vthread));
	t->tid = GetCurrentThreadId();
#	else
	vthread *t = get_local_thread();
	if( t == NULL ) {
		t = (vthread*)alloc(sizeof(vthread));
		init_thread_queue(t);
		set_local_thread(t);
		t->phandle = pthread_self();
	}
#	endif
	return alloc_abstract(k_thread,t);
}

/**
	thread_send : 'thread -> msg:any -> void
	<doc>Send a message into the target thread message queue</doc>
**/
static value thread_send( value vt, value msg ) {
	vthread *t;
	val_check_kind(vt,k_thread);
	t = val_thread(vt);
#	ifdef NEKO_WINDOWS
	{
		value *r = alloc_root(1);
		*r = msg;
		if( !PostThreadMessage(t->tid,WM_TMSG,0,(LPARAM)r) )
			neko_error();
	}
#	else
	{
		tqueue *q = (tqueue*)alloc(sizeof(tqueue));
		q->msg = msg;
		q->next = NULL;
		pthread_mutex_lock(&t->qlock);
		if( t->last == NULL )
			t->first = q;
		else
			t->last->next = q;
		t->last = q;
		pthread_mutex_unlock(&t->qwait);
		pthread_mutex_unlock(&t->qlock);
	}
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
#	ifdef NEKO_WINDOWS
	value *r, v = val_null;
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
		break;
	default:
		neko_error();
		break;
	}
	return v;
#	else
	vthread *t = val_thread(thread_current());
	val_check(block,bool);
	while( true ) {
		value msg = NULL;
		if( val_bool(block) )
			pthread_mutex_lock(&t->qwait);
		pthread_mutex_lock(&t->qlock);
		if( t->first != NULL ) {
			msg = t->first->msg;
			t->first = t->first->next;
			if( t->first == NULL )
				t->last = NULL;
			else
				pthread_mutex_unlock(&t->qwait);
		}
		pthread_mutex_unlock(&t->qlock);
		if( msg != NULL )
			return msg;
		if( !val_bool(block) )
			break;
	}
	return val_null;
#	endif	
}

static void free_lock( value l ) {
#	ifdef NEKO_WINDOWS
	CloseHandle( val_lock(l) );
#	else
	pthread_cond_destroy( &val_lock(l)->cond );
	pthread_mutex_destroy( &val_lock(l)->lock );
#	endif
}

/**
	lock_create : void -> 'lock
	<doc>Creates a lock which is initially locked</doc>
**/
static value lock_create() {
	value vl;
	vlock l;
#	ifdef NEKO_WINDOWS
	l = CreateSemaphore(NULL,0,(1 << 10),NULL);
	if( l == NULL )
		neko_error();
#	else
	l = (vlock)alloc_private(sizeof(struct _vlock));
	l->counter = 0;
	if( pthread_mutex_init(&l->lock,NULL) != 0 || pthread_cond_init(&l->cond,NULL) != 0 )
		neko_error();
#	endif
	vl = alloc_abstract(k_lock,l);
	val_gc(vl,free_lock);
	return vl;
}

/**
	lock_release : 'lock -> void
	<doc>
	Release a lock. The thread does not need to own the lock to be
	able to release it. If a lock is released several times, it can be
	acquired as many times
	</doc>
**/
static value lock_release( value lock ) {
	vlock l; 
	val_check_kind(lock,k_lock);
	l = val_lock(lock);
#	ifdef NEKO_WINDOWS
	if( !ReleaseSemaphore(l,1,NULL) )
		neko_error();
#	else
	pthread_mutex_lock(&l->lock);
	l->counter++;
	pthread_cond_signal(&l->cond);
	pthread_mutex_unlock(&l->lock);
#	endif
	return val_true;
}

/**
	lock_wait : 'lock -> timeout:number? -> bool
	<doc>
	Waits for a lock to be released and acquire it.
	If [timeout] (in seconds) is not null and expires then
	the returned value is false
	</doc>	
**/
static value lock_wait( value lock, value timeout ) {
	int has_timeout = !val_is_null(timeout);
	val_check_kind(lock,k_lock);
	if( has_timeout )
		val_check(timeout,number);
#	ifdef NEKO_WINDOWS
	switch( WaitForSingleObject(val_lock(lock),has_timeout?(DWORD)(val_number(timeout) * 1000.0):INFINITE) ) {
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return val_true;
	case WAIT_TIMEOUT:
		return val_false;
	default:
		neko_error();
	}
#	else
	{
		vlock l = val_lock(lock);
		pthread_mutex_lock(&l->lock);
		while( l->counter == 0 ) {
			if( has_timeout ) {
				struct timespec t;				
				double delta = val_number(timeout) * 1000.0;
				int idelta = (int)delta;
				clock_gettime(CLOCK_REALTIME,&t);
				t.tv_sec += idelta;
				t.tv_nsec += (int)((delta - idelta) * 1e9);
				if( pthread_cond_timedwait(&l->cond,&l->lock,&t) == ETIMEDOUT ) {
					pthread_mutex_unlock(&l->lock);
					return val_false;
				}
			} else
				pthread_cond_wait(&l->cond,&l->lock);
		}
		l->counter--;
		pthread_mutex_unlock(&l->lock);
		return val_true;
	}
#	endif	
}

DEFINE_PRIM(thread_create,2);
DEFINE_PRIM(thread_current,0);
DEFINE_PRIM(thread_send,2);
DEFINE_PRIM(thread_read_message,1);

DEFINE_PRIM(lock_create,0);
DEFINE_PRIM(lock_wait,2);
DEFINE_PRIM(lock_release,1);
