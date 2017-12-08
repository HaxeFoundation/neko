/*
 * Copyright (C)2005-2017 Haxe Foundation
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
#ifdef __APPLE__
// prevent later redefinition of bool
#	include <dlfcn.h>
#endif
#include "vm.h"
#include <string.h>

#if !defined(NEKO_THREADS)

#include <stdlib.h>
struct _mt_local {
	void *value;
};

#else

#ifdef NEKO_WINDOWS
// necessary for TryEnterCriticalSection
// which is only available on 2000 PRO and XP
#	define _WIN32_WINNT 0x0400
#	define GC_NOT_DLL
#	define GC_WIN32_THREADS
#endif

#define GC_THREADS
#include <gc/gc.h>

#if GC_VERSION_MAJOR < 7
#	define GC_SUCCESS	0
#	define GC_DUPLICATE	1
#endif

#ifdef NEKO_WINDOWS

struct _mt_lock {
	CRITICAL_SECTION cs;
};

#else

#include <stdlib.h>
#include <pthread.h>

struct _mt_local {
	pthread_key_t key;
};
struct _mt_lock {
	pthread_mutex_t lock;
};

// should be enough to store any GC_stack_base
// implementation
typedef char __stack_base[64];

#endif

#endif // !NEKO_THREADS

typedef struct {
	thread_main_func init;
	thread_main_func main;
	void *param;
#ifdef NEKO_THREADS
#	ifdef NEKO_WINDOWS
	HANDLE lock;
#	else
	pthread_mutex_t lock;
#	endif
#endif
} tparams;

#ifdef NEKO_THREADS

#ifdef NEKO_WINDOWS
#	define THREAD_FUN DWORD WINAPI
#else
#	define THREAD_FUN void *
#endif

typedef int (*rec)( int, void * );

static int clean_c_stack( int n, void *f ) {
	char buf[256];
	memset(buf,n,sizeof(buf));
	if( n == 0 ) return *buf;
	return ((rec)f)(n-1,f) ? 1 : 0; // prevent tail-rec
}

static THREAD_FUN ThreadMain( void *_p ) {
	tparams *lp = (tparams*)_p;
	tparams p = *lp;
	p.init(p.param);
	// we have the 'param' value on this thread C stack
	// so it's safe to give back control to main thread
#	ifdef NEKO_WINDOWS
	ReleaseSemaphore(p.lock,1,NULL);
#	else
	pthread_mutex_unlock(&lp->lock);
#	endif
	clean_c_stack(40,clean_c_stack);
	p.main(p.param);
	return 0;
}

#endif

EXTERN int neko_thread_create( thread_main_func init, thread_main_func main, void *param, void **handle ) {
	tparams p;
	p.init = init;
	p.main = main;
	p.param = param;
#	if !defined(NEKO_THREADS)
	return 0;
#	elif defined(NEKO_WINDOWS)
	{
		HANDLE h;
		p.lock = CreateSemaphore(NULL,0,1,NULL);
		h = GC_CreateThread(NULL,0,ThreadMain,&p,0,(void*)handle);
		if( h == NULL ) {
			CloseHandle(p.lock);
			return 0;
		}
		WaitForSingleObject(p.lock,INFINITE);
		CloseHandle(p.lock);
		CloseHandle(h);
		return 1;
	}
#	else
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	pthread_mutex_init(&p.lock,NULL);
	pthread_mutex_lock(&p.lock);
	// force the use of a the GC method to capture created threads
	// this function should be defined in gc/gc.h
	if( GC_pthread_create((pthread_t*)handle,&attr,&ThreadMain,&p) != 0 ) {
		pthread_attr_destroy(&attr);
		pthread_mutex_destroy(&p.lock);
		return 0;
	}
	pthread_mutex_lock(&p.lock);
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&p.lock);
	return 1;
#	endif
}

#if defined(NEKO_POSIX) && defined(NEKO_THREADS)
#	include <dlfcn.h>
	typedef void (*callb_func)( thread_main_func, void * );
	typedef int (*std_func)();
	typedef int (*gc_stack_ptr)( __stack_base * );

static int do_nothing( __stack_base *sb ) {
	return -1;
}

#endif

EXTERN void neko_thread_blocking( thread_main_func f, void *p ) {
#	if !defined(NEKO_THREADS)
	f(p); // nothing
#	elif defined(NEKO_WINDOWS)
	f(p); // we don't have pthreads issues
#	else
	// we have different APIs depending on the GC version, make sure we load
	// the good one at runtime
	static callb_func do_blocking = NULL;
	static std_func start = NULL, end = NULL;
	if( do_blocking )
		do_blocking(f,p);
	else if( start ) {
		start();
		f(p);
		end();
	} else {
		void *self = dlopen(NULL,0);
		do_blocking = (callb_func)dlsym(self,"GC_do_blocking");
		if( !do_blocking ) {
			start = (std_func)dlsym(self,"GC_start_blocking");
			end = (std_func)dlsym(self,"GC_end_blocking");
			if( !start || !end )
				val_throw(alloc_string("Could not init GC blocking API"));
		}
		neko_thread_blocking(f,p);
	}
#	endif
}

EXTERN bool neko_thread_register( bool t ) {
#	if !defined(NEKO_THREADS)
	return 0;
#	elif defined(NEKO_WINDOWS)
	struct GC_stack_base sb;
	int r;
	if( !t )
		return GC_unregister_my_thread() == GC_SUCCESS;
	if( GC_get_stack_base(&sb) != GC_SUCCESS )
		return 0;
	r = GC_register_my_thread(&sb);
	return( r == GC_SUCCESS || r == GC_DUPLICATE );
#	else
	// since the API is only available on GC 7.0,
	// we will do our best to locate it dynamically
	static gc_stack_ptr get_sb = NULL, my_thread = NULL;
	static std_func unreg_my_thread = NULL;
	if( !t && unreg_my_thread != NULL ) {
		return unreg_my_thread() == GC_SUCCESS;
	} else if( my_thread != NULL ) {
		__stack_base sb;
		int r;
		if( get_sb(&sb) != GC_SUCCESS )
			return 0;
		r = my_thread(&sb);
		return( r == GC_SUCCESS || r == GC_DUPLICATE );
	} else {
		void *self = dlopen(NULL,0);
		my_thread = (gc_stack_ptr)dlsym(self,"GC_register_my_thread");
		get_sb = (gc_stack_ptr)dlsym(self,"GC_get_stack_base");
		unreg_my_thread = (std_func)dlsym(self,"GC_unregister_my_thread");
		if( my_thread == NULL ) my_thread = do_nothing;
		if( get_sb == NULL ) get_sb = do_nothing;
		if( unreg_my_thread == NULL ) unreg_my_thread = (std_func)do_nothing;
		return neko_thread_register(t);
	}
#	endif
}


EXTERN mt_local *alloc_local() {
#	if !defined(NEKO_THREADS)
	mt_local *l = malloc(sizeof(mt_local));
	l->value = NULL;
	return l;
#	elif defined(NEKO_WINDOWS)
	DWORD t = TlsAlloc();
	TlsSetValue(t,NULL);
	return (mt_local*)(int_val)t;
#	else
	mt_local *l = malloc(sizeof(mt_local));
	pthread_key_create(&l->key,NULL);
	return l;
#	endif
}

EXTERN void free_local( mt_local *l ) {
#	if !defined(NEKO_THREADS)
	free(l);
#	elif defined(NEKO_WINDOWS)
	TlsFree((DWORD)(int_val)l);
#	else
	pthread_key_delete(l->key);
	free(l);
#	endif
}

EXTERN void local_set( mt_local *l, void *v ) {
#	if !defined(NEKO_THREADS)
	l->value = v;
#	elif defined(NEKO_WINDOWS)
	TlsSetValue((DWORD)(int_val)l,v);
#	else
	pthread_setspecific(l->key,v);
#	endif
}

EXTERN void *local_get( mt_local *l ) {
	if( l == NULL )
		return NULL;
#	if !defined(NEKO_THREADS)
	return l->value;
#	elif defined(NEKO_WINDOWS)
	return (void*)TlsGetValue((DWORD)(int_val)l);
#	else
	return pthread_getspecific(l->key);
#	endif
}

EXTERN mt_lock *alloc_lock() {
#	if !defined(NEKO_THREADS)
	return (mt_lock*)1;
#	elif defined(NEKO_WINDOWS)
	mt_lock *l = (mt_lock*)malloc(sizeof(mt_lock));
	InitializeCriticalSection(&l->cs);
	return l;
#	else
	mt_lock *l = (mt_lock*)malloc(sizeof(mt_lock));
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&l->lock,&a);
	pthread_mutexattr_destroy(&a);
	return l;
#	endif
}

EXTERN void lock_acquire( mt_lock *l ) {
#	if !defined(NEKO_THREADS)
#	elif defined(NEKO_WINDOWS)
	EnterCriticalSection(&l->cs);
#	else
	pthread_mutex_lock(&l->lock);
#	endif
}

EXTERN int lock_try( mt_lock *l ) {
#if	!defined(NEKO_THREADS)
	return 1;
#	elif defined(NEKO_WINDOWS)
	return TryEnterCriticalSection(&l->cs);
#	else
	return pthread_mutex_trylock(&l->lock) == 0;
#	endif
}

EXTERN void lock_release( mt_lock *l ) {
#	if !defined(NEKO_THREADS)
#	elif defined(NEKO_WINDOWS)
	LeaveCriticalSection(&l->cs);
#	else
	pthread_mutex_unlock(&l->lock);
#	endif
}

EXTERN void free_lock( mt_lock *l ) {
#	if !defined(NEKO_THREADS)
#	elif defined(NEKO_WINDOWS)
	DeleteCriticalSection(&l->cs);
	free(l);
#	else
	pthread_mutex_destroy(&l->lock);
	free(l);
#	endif
}

/* ************************************************************************ */
