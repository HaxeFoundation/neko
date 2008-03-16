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
#include <neko_vm.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NEKO_WINDOWS
#	include <windows.h>
	typedef HANDLE vlock;
#else
#	include <pthread.h>
#	include <sys/time.h>
	typedef struct _vlock {
		pthread_mutex_t lock;
		pthread_cond_t cond;
		int counter;
	} *vlock;
#endif

typedef struct _tqueue {
	value msg;
	struct _tqueue *next;
} tqueue;

typedef struct {
#	ifdef NEKO_WINDOWS
	DWORD tid;
	CRITICAL_SECTION lock;
	HANDLE wait;
#	else
	pthread_t phandle;
	pthread_mutex_t lock;
	pthread_cond_t wait;
#	endif
	tqueue *first;
	tqueue *last;
	value v;
} vthread;

DECLARE_KIND(k_thread);

#define val_thread(t)	((vthread*)val_data(t))

#ifdef NEKO_WINDOWS
#	define LOCK(l)		EnterCriticalSection(&(l))
#	define UNLOCK(l)	LeaveCriticalSection(&(l))
#	define SIGNAL(l)	ReleaseSemaphore(l,1,NULL)
#else
#	define LOCK(l)		pthread_mutex_lock(&(l))
#	define UNLOCK(l)	pthread_mutex_unlock(&(l))
#	define SIGNAL(l)	pthread_cond_signal(&(l))
#endif

/**
	<doc>
	<h1>Thread</h1>
	<p>
	An API to create and manager system threads and locks.
	</p>
	</doc>
**/

#define val_lock(l)		((vlock)val_data(l))
#define val_tls(l)		((vtls*)val_data(l))

typedef struct {
#	ifdef NEKO_WINDOWS
	DWORD tls;
#	else
	pthread_key_t key;
#	endif
} vtls;

DEFINE_KIND(k_thread);
DEFINE_KIND(k_lock);
DEFINE_KIND(k_tls);

typedef struct {
	value callb;
	value callparam;
	vthread *t;
	void *handle;
} tparams;

static vthread *neko_thread_current() {
	return (vthread*)neko_vm_custom(neko_vm_current(),k_thread);
}

static void free_thread( value v ) {
	vthread *t = val_thread(v);
#ifdef NEKO_WINDOWS
	DeleteCriticalSection(&t->lock);
	CloseHandle(t->wait);
#else
	pthread_mutex_destroy(&t->lock);
	pthread_cond_destroy(&t->wait);
#endif
}

static vthread *alloc_thread() {
	vthread *t = (vthread*)alloc(sizeof(vthread));
	memset(t,0,sizeof(vthread));
#ifdef NEKO_WINDOWS
	t->tid = GetCurrentThreadId();
	t->wait = CreateSemaphore(NULL,0,1,NULL);
	InitializeCriticalSection(&t->lock);
#else
	t->phandle = pthread_self();
	pthread_mutex_init(&t->lock,NULL);
	pthread_cond_init(&t->wait,NULL);
#endif
	t->v = alloc_abstract(k_thread,t);
	val_gc(t->v,free_thread);
	return t;
}

static int thread_init( void *_p ) {
	tparams *p = (tparams*)_p;
	neko_vm *vm;
	p->t = alloc_thread();
	// init the VM and set current thread
	vm = neko_vm_alloc(NULL);
	neko_vm_select(vm);
	neko_vm_set_custom(vm,k_thread,p->t);
	return 0;
}

static int thread_loop( void *_p ) {
	tparams *p = (tparams*)_p;
	value exc = NULL;
	val_callEx(val_null,p->callb,&p->callparam,1,&exc);
	// display exception
	if( exc != NULL ) {
		buffer b = alloc_buffer(NULL);
		fprintf(stderr,"An exception occured in a neko Thread :\n");
		val_buffer(b,exc);
		fprintf(stderr,"%s\n",val_string(buffer_to_string(b)));
	}
	// cleanup
	neko_vm_select(NULL);
	return 0;
}

/**
	thread_create : f:function:1 -> p:any -> 'thread
	<doc>Creates a thread that will be running the function [f(p)]</doc>
**/
static value thread_create( value f, value param ) {
	tparams *p;
	val_check_function(f,1);
	p = (tparams*)alloc(sizeof(tparams));
	p->callb = f;
	p->callparam = param;
	if( !neko_thread_create(thread_init,thread_loop,p,&p->handle) )
		neko_error();
	return p->t->v;
}

/**
	thread_current : void -> 'thread
	<doc>Returns the current thread</doc>
**/
static value thread_current() {
	vthread *t = neko_thread_current();
	// should only occur for main thread !
	if( t == NULL ) {
		t = alloc_thread();
		neko_vm_set_custom(neko_vm_current(),k_thread,t);
	}
	return t->v;
}

/**
	thread_send : 'thread -> msg:any -> void
	<doc>Send a message into the target thread message queue</doc>
**/
static value thread_send( value vt, value msg ) {
	vthread *t;
	tqueue *q;
	val_check_kind(vt,k_thread);
	t = val_thread(vt);
	q = (tqueue*)alloc(sizeof(tqueue));
	q->msg = msg;
	q->next = NULL;
	LOCK(t->lock);
	if( t->last == NULL )
		t->first = q;
	else
		t->last->next = q;
	t->last = q;
	SIGNAL(t->wait);
	UNLOCK(t->lock);
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
	value v = thread_current();
	vthread *t;
	value msg;
	if( v == NULL )
		neko_error();
	t = val_thread(v);
	val_check(block,bool);
	LOCK(t->lock);
	while( t->first == NULL )
		if( val_bool(block) ) {
#			ifdef NEKO_WINDOWS
			UNLOCK(t->lock);
			WaitForSingleObject(t->wait,INFINITE);
			LOCK(t->lock);
#			else
			pthread_cond_wait(&t->wait,&t->lock);
#			endif
		} else {
			UNLOCK(t->lock);
			return val_null;
		}
	msg = t->first->msg;
	t->first = t->first->next;
	if( t->first == NULL )
		t->last = NULL;
	else
		SIGNAL(t->wait);
	UNLOCK(t->lock);
	return msg;
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
	l = CreateSemaphore(NULL,0,(1 << 30),NULL);
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
				struct timeval tv;
				struct timespec t;
				double delta = val_number(timeout);
				int idelta = (int)delta, idelta2;
				delta -= idelta;
				delta *= 1.0e9;
				gettimeofday(&tv,NULL);
				delta += tv.tv_usec * 1000.0;
				idelta2 = (int)(delta / 1e9);
				delta -= idelta2 * 1e9;
				t.tv_sec = tv.tv_sec + idelta + idelta2;
				t.tv_nsec = (long)delta;
				if( pthread_cond_timedwait(&l->cond,&l->lock,&t) != 0 ) {
					pthread_mutex_unlock(&l->lock);
					return val_false;
				}
			} else
				pthread_cond_wait(&l->cond,&l->lock);
		}
		l->counter--;
		if( l->counter > 0 )
			pthread_cond_signal(&l->cond);
		pthread_mutex_unlock(&l->lock);
		return val_true;
	}
#	endif
}

static void free_tls( value v ) {
	vtls *t = val_tls(v);
#	ifdef NEKO_WINDOWS
	TlsFree(t->tls);
#	else
	pthread_key_delete(t->key);
#	endif
	free(t);
}

/**
	tls_create : void -> 'tls
	<doc>
	Creates thread local storage. This is placeholder that can store a value that will
	be different depending on the local thread. You must set the tls value to [null]
	before exiting the thread or the memory will never be collected.
	</doc>
**/
static value tls_create() {
	value v;
	vtls *t = (vtls*)malloc(sizeof(vtls));
#	ifdef NEKO_WINDOWS
	t->tls = TlsAlloc();
	TlsSetValue(t->tls,NULL);
#	else
	pthread_key_create(&t->key,NULL);
#	endif
	v = alloc_abstract(k_tls,t);
	val_gc(v,free_tls);
	return v;
}

/**
	tls_get : 'tls -> any
	<doc>
	Returns the value set by [tls_set] for the local thread.
	</doc>
**/
static value tls_get( value v ) {
	vtls *t;
	value *r;
	val_check_kind(v,k_tls);
	t = val_tls(v);
#	ifdef NEKO_WINDOWS
	r = (value*)TlsGetValue(t->tls);
#	else
	r = (value*)pthread_getspecific(t->key);
#	endif
	if( r == NULL ) return val_null;
	return *r;
}

/**
	tls_set : 'tls -> any -> void
	<doc>
	Set the value of the TLS for the local thread.
	</doc>
**/
static value tls_set( value v, value content ) {
	vtls *t;
	value *r;
	val_check_kind(v,k_tls);
	t = val_tls(v);
#	ifdef NEKO_WINDOWS
	r = (value*)TlsGetValue(t->tls);
#	else
	r = (value*)pthread_getspecific(t->key);
#	endif
	if( r == NULL ) {
		if( val_is_null(content) )
			return val_null;
		r = alloc_root(1);
#		ifdef NEKO_WINDOWS
		TlsSetValue(t->tls,r);
#		else
		pthread_setspecific(t->key,r);
#		endif
	} else if( val_is_null(content) ) {
		free_root(r);
#		ifdef NEKO_WINDOWS
		TlsSetValue(t->tls,NULL);
#		else
		pthread_setspecific(t->key,NULL);
#		endif
		return val_null;
	}
	*r = content;
	return val_null;
}

DEFINE_PRIM(thread_create,2);
DEFINE_PRIM(thread_current,0);
DEFINE_PRIM(thread_send,2);
DEFINE_PRIM(thread_read_message,1);

DEFINE_PRIM(lock_create,0);
DEFINE_PRIM(lock_wait,2);
DEFINE_PRIM(lock_release,1);

DEFINE_PRIM(tls_create,0);
DEFINE_PRIM(tls_set,2);
DEFINE_PRIM(tls_get,1);

