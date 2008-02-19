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
#include "context.h"
#include "neko.h"

#ifndef NEKO_NO_THREADS

#ifdef NEKO_WINDOWS
#include <windows.h>
// disable warnings for type conversions
#pragma warning(disable : 4311)
#pragma warning(disable : 4312)

_context *context_new() {
	DWORD t = TlsAlloc();
	TlsSetValue(t,NULL);
	return (_context*)t;
}

void context_delete( _context *ctx ) {
	TlsFree((DWORD)ctx);
}

void context_set( _context *ctx, void *c ) {
	TlsSetValue((DWORD)ctx,c);
}

void *context_get( _context *ctx ) {
	if( ctx == NULL )
		return NULL;
	return (void*)TlsGetValue((DWORD)ctx);
}

_clock *context_lock_new() {
	return (_clock*)CreateMutex(NULL,FALSE,NULL);
}

void context_lock( _clock *l ) {
	WaitForSingleObject((HANDLE)l,INFINITE);
}

void context_release( _clock *l ) {
	ReleaseMutex((HANDLE)l);
}

void context_lock_delete( _clock *l ) {
	CloseHandle((HANDLE)l);
}

#else
/* ************************************************************************ */
#include <stdlib.h>
#include <pthread.h>

struct _context {
	pthread_key_t key;
};

_context *context_new() {
	_context *ctx = malloc(sizeof(_context));
	pthread_key_create( &ctx->key, NULL );
	return ctx;
}

void context_delete( _context *ctx ) {
	pthread_key_delete( ctx->key );
	free(ctx);
}

void context_set( _context *ctx, void *data ) {
	pthread_setspecific( ctx->key, data );
}

void *context_get( _context *ctx ) {
	if( ctx == NULL )
		return NULL;
	return pthread_getspecific( ctx->key );
}

struct _clock {
	pthread_mutex_t lock;
};

_clock *context_lock_new() {
	_clock *l = malloc(sizeof(_clock));
	pthread_mutex_init(&l->lock,NULL);
	pthread_mutex_unlock(&l->lock);
	return l;
}

void context_lock( _clock *l ) {
	pthread_mutex_lock(&l->lock);
}

void context_release( _clock *l ) {
	pthread_mutex_unlock(&l->lock);
}

void context_lock_delete( _clock *l ) {
	pthread_mutex_destroy(&l->lock);
}

#endif

#endif // NEKO_NO_THREADS

/* ************************************************************************ */
