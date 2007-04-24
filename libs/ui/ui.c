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
#define HEADER_IMPORTS
#include <neko.h>
#include "../std/thread.h"

#ifdef NEKO_WINDOWS
#	include <windows.h>
#	define CLASS_NAME "Neko_OS_wnd_class"
#	define WM_SYNC_CALL	(WM_USER + 101)
static LRESULT CALLBACK WindowProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam ) {
	switch( msg ) {
	case WM_SYNC_CALL: {
		value *r = (value*)lparam;
		value f = *r;
		free_root(r);
		val_call0(f);
		return 0;
	}}
	return DefWindowProc(hwnd,msg,wparam,lparam);
}
#else NEKO_MAC
#	include <Carbon/Carbon.h>
#	define xCrossEvent	0xFFFFAA00
#	define eCall		0x0

enum {
	pFunc = 'func',
};

static OSStatus handleEvents( EventHandlerCallRef ref, EventRef e, void *data ) {
	switch( GetEventKind(e) ) {
	case eCall: {
		value *r;
		value f;
		GetEventParameter(e,pFunc,typeVoidPtr,0,sizeof(void*),0,&r);
		f = *r;
		free_root(r);
		val_call0(f);
		break;
	}}	
	return 0;
}
#endif

static void os_thread_init( vthread *t ) {
#	ifdef NEKO_WINDOWS
	t->os_wnd = CreateWindow(CLASS_NAME,"",0,0,0,0,0,NULL,NULL,NULL,NULL);
#	else NEKO_MAC
	EventTypeSpec ets[] = { { xCrossEvent, eCall } };
	t->os_queue = GetCurrentQueue();
	t->os_loop = GetCurrentEventLoop();
	InstallEventHandler(GetEventDispatcherTarget(),NewEventHandlerUPP(handleEvents),sizeof(ets) / sizeof(EventTypeSpec),ets,0,0);
#	else
#	endif	
}

static void os_thread_cleanup( vthread *t ) {	
#	ifdef NEKO_WINDOWS
	DestroyWindow(t->os_wnd);
	t->os_wnd = NULL;
#	endif
}

DEFINE_ENTRY_POINT(os_main);

void os_main() {
#	ifdef NEKO_WINDOWS
	WNDCLASSEX wcl;
	HINSTANCE hinst = GetModuleHandle(NULL);
	memset(&wcl,0,sizeof(wcl));
	wcl.cbSize			= sizeof(WNDCLASSEX);
	wcl.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wcl.lpfnWndProc		= WindowProc;
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hInstance		= hinst;
	wcl.hIcon			= NULL;
	wcl.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcl.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
	wcl.lpszMenuName	= "";
	wcl.lpszClassName	= CLASS_NAME;
	wcl.hIconSm			= 0;
	RegisterClassEx(&wcl);
#	endif
	neko_thread_init_hook = os_thread_init;
	neko_thread_cleanup_hook = os_thread_cleanup;
}

static void os_loop() {
#	ifdef NEKO_WINDOWS
	MSG msg;
	while( GetMessage(&msg,NULL,0,0) ) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if( msg.message == WM_QUIT )
			break;
	}
#	else NEKO_MAC
	RunCurrentEventLoop(kEventDurationForever);
#	endif
}

static value os_loop_stop( value t ) {
	val_check_kind(t,k_thread);
#	ifdef NEKO_WINDOWS
	if( !PostThreadMessage(val_thread(t)->tid,WM_QUIT,0,0) )
		neko_error();
#	else NEKO_MAC
	if( QuitEventLoop((EventLoopRef)t->os_loop) != noErr )
		neko_error();
#	endif
	return val_null;
}

static value os_sync( value t, value f ) {
	value *r;
	val_check_kind(t,k_thread);
	val_check_function(f,0);
	r = alloc_root(1);
	*r = f;
#	ifdef NEKO_WINDOWS
	if( !PostMessage(val_thread(t)->os_wnd,WM_SYNC_CALL,0,(LPARAM)r) ) {
		free_root(r);
		neko_error();
	}
#	else NEKO_MAC
	EventRef e;
	CreateEvent(NULL,xCrossEvent,eCall,GetCurrentEventTime(),kEventAttributeUserEvent,&e);
	SetEventParameter(e,pFunc,typeVoidPtr,sizeof(void*),&r);
	PostEventToQueue((EventQueueRef)val_thread(t)->os_queue,e,kEventPriorityStandard);
	ReleaseEvent(e);
#	endif
	return val_null;
}

DEFINE_PRIM(os_loop,0);
DEFINE_PRIM(os_loop_stop,1);
DEFINE_PRIM(os_sync,2);

/* ************************************************************************ */
