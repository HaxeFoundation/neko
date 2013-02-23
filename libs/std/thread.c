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
#include <neko_vm.h>
#undef free_lock
#undef lock_release

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
	tqueue *first;
	tqueue *last;
#	ifdef NEKO_WINDOWS
	CRITICAL_SECTION lock;
	HANDLE wait;
#	else
	pthread_mutex_t lock;
	pthread_cond_t wait;
#	endif
} vdeque;

typedef struct {
#	ifdef NEKO_WINDOWS
	DWORD tid;
#	else
	pthread_t phandle;
#	endif
	value v;
	vdeque q;
	neko_vm *vm;
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

/*
	deque raw API
*/
static void _deque_init( vdeque *q ) {
#	ifdef NEKO_WINDOWS
	q->wait = CreateSemaphore(NULL,0,(1 << 30),NULL);
	InitializeCriticalSection(&q->lock);
#	else
	pthread_mutex_init(&q->lock,NULL);
	pthread_cond_init(&q->wait,NULL);
#	endif
}

static void _deque_destroy( vdeque *q ) {
#ifdef NEKO_WINDOWS
	DeleteCriticalSection(&q->lock);
	CloseHandle(q->wait);
#else
	pthread_mutex_destroy(&q->lock);
	pthread_cond_destroy(&q->wait);
#endif
}

static void _deque_add( vdeque *q, value msg ) {
	tqueue *t;
	t = (tqueue*)alloc(sizeof(tqueue));
	t->msg = msg;
	t->next = NULL;
	LOCK(q->lock);
	if( q->last == NULL )
		q->first = t;
	else
		q->last->next = t;
	q->last = t;
	SIGNAL(q->wait);
	UNLOCK(q->lock);
}

static void _deque_push( vdeque *q, value msg ) {
	tqueue *t;
	t = (tqueue*)alloc(sizeof(tqueue));
	t->msg = msg;
	LOCK(q->lock);
	t->next = q->first;
	q->first = t;
	if( q->last == NULL )
		q->last = t;
	SIGNAL(q->wait);
	UNLOCK(q->lock);
}

static value _deque_pop( vdeque *q, int block ) {
	value msg;
	LOCK(q->lock);
	while( q->first == NULL )
		if( block ) {
#			ifdef NEKO_WINDOWS
			UNLOCK(q->lock);
			WaitForSingleObject(q->wait,INFINITE);
			LOCK(q->lock);
#			else
			pthread_cond_wait(&q->wait,&q->lock);
#			endif
		} else {
			UNLOCK(q->lock);
			return val_null;
		}
	msg = q->first->msg;
	q->first = q->first->next;
	if( q->first == NULL )
		q->last = NULL;
	else
		SIGNAL(q->wait);
	UNLOCK(q->lock);
	return msg;
}


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
#define val_mutex(l)	((mt_lock*)val_data(l))
#define val_deque(l)	((vdeque*)val_data(l))

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
DEFINE_KIND(k_mutex);
DEFINE_KIND(k_deque);

typedef struct {
	value callb;
	value callparam;
	vthread *t;
	void *handle;
	int jit;
} tparams;

static vthread *neko_thread_current() {
	return (vthread*)neko_vm_custom(neko_vm_current(),k_thread);
}

static void free_thread( value v ) {
	vthread *t = val_thread(v);
	_deque_destroy(&t->q);
}

static vthread *alloc_thread( neko_vm *vm ) {
	vthread *t = (vthread*)alloc(sizeof(vthread));
	memset(t,0,sizeof(vthread));
#ifdef NEKO_WINDOWS
	t->tid = GetCurrentThreadId();
#else
	t->phandle = pthread_self();
#endif
	t->v = alloc_abstract(k_thread,t);
	t->vm = vm;
	_deque_init(&t->q);
	val_gc(t->v,free_thread);
	return t;
}

static void thread_init( void *_p ) {
	tparams *p = (tparams*)_p;
	neko_vm *vm;
	// init the VM and set current thread
	vm = neko_vm_alloc(NULL);
	p->t = alloc_thread(vm);
	neko_vm_jit(vm,p->jit);
	neko_vm_select(vm);
	neko_vm_set_custom(vm,k_thread,p->t);
}

static void thread_loop( void *_p ) {
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
	p->t->v = val_null;
	p->t->vm = NULL;
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
	p->jit = neko_vm_jit(neko_vm_current(),-1);	
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
		neko_vm *vm = neko_vm_current();
		t = alloc_thread(vm);
		neko_vm_set_custom(vm,k_thread,t);
	}
	return t->v;
}

/**
	thread_send : 'thread -> msg:any -> void
	<doc>Send a message into the target thread message queue</doc>
**/
static value thread_send( value vt, value msg ) {
	vthread *t;
	val_check_kind(vt,k_thread);
	t = val_thread(vt);
	_deque_add(&t->q,msg);
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
	if( v == NULL )
		neko_error();
	t = val_thread(v);
	val_check(block,bool);
	return _deque_pop( &t->q, val_bool(block) );
}

/**
	thread_stack : 'thread -> array
	<doc>
	Get the thread current call stack. Might crash if the thread currently manipulate the stack, so mostly used to debug infinite loops.
	</doc>
**/
static value thread_stack( value vt ) {
	vthread *t;
	val_check_kind(vt,k_thread);
	t = val_thread(vt);
	if( t->vm == NULL ) neko_error();
	return neko_call_stack(t->vm);
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

static void free_mutex( value v ) {
	neko_free_lock( val_mutex(v) );
}

/**
	mutex_create : void -> 'mutex
	<doc>
	Creates a mutex, which can be used to acquire a temporary lock to access some ressource.
	The main difference with a lock is that a mutex must always be released by the owner thread.
	</doc>
**/
static value mutex_create() {
	mt_lock *m = neko_alloc_lock();
	value v = alloc_abstract(k_mutex,m);
	val_gc(v,free_mutex);
	return v;
}

/**
	mutex_acquire : 'mutex -> void
	<doc>
	The current thread acquire the mutex or wait if not available. The same thread can acquire
	several times the same mutex but must release it as many times it has been acquired.
	</doc>
**/
static value mutex_acquire( value m ) {
	val_check_kind(m,k_mutex);
	neko_lock_acquire( val_mutex(m) );
	return val_null;
}

/**
	mutex_try : 'mutex -> bool
	<doc>
	Try to acquire the mutex, returns true if acquire or false if it's already locked by another
	thread.
	</doc>
**/
static value mutex_try( value m ) {
	val_check_kind(m,k_mutex);
	return alloc_bool( neko_lock_try(val_mutex(m)) );	
}

/**
	mutex_release : 'mutex -> void
	<doc>
	Release a mutex that has been acquired by the current thread. The behavior is undefined if the
	current thread does not own the mutex.
	</doc>
**/
static value mutex_release( value m ) {
	val_check_kind(m,k_mutex);
	neko_lock_release(val_mutex(m));
	return val_null;
}

static void free_deque( value v ) {	
	_deque_destroy(val_deque(v));
}

/**
	deque_create : void -> 'deque
	<doc>create a message queue for multithread access</doc>
**/
static value deque_create() {
	vdeque *q = (vdeque*)alloc(sizeof(vdeque));
	value v = alloc_abstract(k_deque,q);
	val_gc(v,free_deque);
	_deque_init(q);
	return v;
}

/**
	deque_add : 'deque -> any -> void
	<doc>add a message at the end of the queue</doc>
**/
static value deque_add( value v, value i ) {
	val_check_kind(v,k_deque);
	_deque_add(val_deque(v),i);
	return val_null;
}

/**
	deque_push : 'deque -> any -> void
	<doc>add a message at the head of the queue</doc>
**/
static value deque_push( value v, value i ) {
	val_check_kind(v,k_deque);
	_deque_push(val_deque(v),i);
	return val_null;
}

/**
	deque_pop : 'deque -> bool -> any?
	<doc>pop a message from the queue head. Either block until a message is available or return immedialtly with null.</doc>
**/
static value deque_pop( value v, value block ) {
	val_check_kind(v,k_deque);
	val_check(block,bool);
	return _deque_pop(val_deque(v),val_bool(block));
}

DEFINE_PRIM(thread_create,2);
DEFINE_PRIM(thread_current,0);
DEFINE_PRIM(thread_send,2);
DEFINE_PRIM(thread_read_message,1);
DEFINE_PRIM(thread_stack,1);

DEFINE_PRIM(lock_create,0);
DEFINE_PRIM(lock_wait,2);
DEFINE_PRIM(lock_release,1);

DEFINE_PRIM(tls_create,0);
DEFINE_PRIM(tls_set,2);
DEFINE_PRIM(tls_get,1);

DEFINE_PRIM(mutex_create,0);
DEFINE_PRIM(mutex_acquire,1);
DEFINE_PRIM(mutex_try,1);
DEFINE_PRIM(mutex_release,1);

DEFINE_PRIM(deque_create,0);
DEFINE_PRIM(deque_add,2);
DEFINE_PRIM(deque_push,2);
DEFINE_PRIM(deque_pop,2);
