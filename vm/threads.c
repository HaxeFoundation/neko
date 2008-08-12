/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
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
#include "vm.h"

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

#endif

#endif

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

#ifdef NEKO_WINDOWS
#	define THREAD_FUN DWORD WINAPI 
#else
#	define THREAD_FUN void*
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
	pthread_mutex_init(&p.lock,NULL);
	pthread_mutex_lock(&p.lock);
	// force the use of a the GC method to capture created threads
	// this function should be defined in gc/gc.h
	if( GC_pthread_create((pthread_t*)handle,NULL,&ThreadMain,&p) != 0 ) {
		pthread_mutex_destroy(&p.lock);
		return 0;
	}
	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
	return 1;
#	endif
}

#if defined(NEKO_POSIX) && defined(NEKO_THREADS)
#	if GC_VERSION_MAJOR >= 7
	extern void GC_do_blocking( void (*fn)(void *), void *arg );
#	else
	extern void GC_start_blocking();
	extern void GC_end_blocking();
#	define GC_do_blocking(f,arg) { GC_start_blocking(); f(arg); GC_end_blocking(); }
#	endif
#else
#	define GC_do_blocking(f,arg)	f(arg)
#endif

EXTERN void neko_thread_blocking( thread_main_func f, void *p ) {
	GC_do_blocking(f,p);
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
