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
	HWND os_wnd;
	DWORD tid;
	CRITICAL_SECTION lock;
	HANDLE wait;
#	else
	pthread_t phandle;
	pthread_mutex_t lock;
	pthread_cond_t wait;
#	endif
#	ifdef NEKO_MAC
	void *os_queue;
	void *os_loop;
#	endif
	tqueue *first;
	tqueue *last;
	value v;	
} vthread;

typedef void (*init_func)( vthread *t );
typedef void (*cleanup_func)( vthread *t );

H_EXTERN init_func neko_thread_init_hook;
H_EXTERN cleanup_func neko_thread_cleanup_hook;
H_EXTERN vthread *neko_thread_current();

DECLARE_KIND(k_thread);

#define val_thread(t)	((vthread*)val_data(t))

/* ************************************************************************ */
