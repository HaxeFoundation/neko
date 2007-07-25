/* ************************************************************************ */
/*																			*/
/*  Neko UI Core Library													*/
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
#include <neko_vm.h>
#include <stdio.h>

#if defined(NEKO_WINDOWS)
#	include <windows.h>
#	define CLASS_NAME "Neko_UI_wnd_class"
#	define WM_SYNC_CALL	(WM_USER + 101)
#elif defined(NEKO_MAC)
#	include <Carbon/Carbon.h>
#	include <pthread.h>
#	define UIEvent		0xFFFFAA00
#	define eCall		0x0
enum { pFunc = 'func' };
#elif defined(NEKO_LINUX)
#	include <gtk/gtk.h>
#	include <glib.h>
#	include <pthread.h>
#	include <locale.h>
#endif

/**
	<doc>
	<h1>UI</h1>
	<p>
	Core native User Interface support. This API uses native WIN32 API on Windows,
	Carbon API on OSX, and GTK2 on Linux.
	</p>
	</doc>
**/

typedef struct {
	int init_done;
#if defined(NEKO_WINDOWS)
	DWORD tid;
	HWND wnd;
#elif defined(NEKO_MAC)
	pthread_t tid;
#elif defined(NEKO_LINUX)
	pthread_t tid;
	pthread_mutex_t lock;
#endif

} ui_data;

static ui_data data = { 0 };

#if defined(NEKO_WINDOWS)

static LRESULT CALLBACK WindowProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam ) {
	switch( msg ) {
	case WM_SYNC_CALL: {
		value *r = (value*)lparam;
		value f = *r;
		free_root(r);
		// There are some GC issues here when having a lot of threads
		// It seems that somehow the function is not called, it might
		// also trigger some crashes.
		val_call0(f);
		return 0;
	}}
	return DefWindowProc(hwnd,msg,wparam,lparam);
}

#elif defined(NEKO_MAC)

static OSStatus nothing() {
	return 0;
}

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

#elif defined(NEKO_LINUX)

static gint nothing( gpointer data ) {
	return TRUE;
}

static gint onSyncCall( gpointer data ) {
	value *r = (value*)data;
	value f = *r;
	free_root(r);
	val_call0(f);
	return 0;
}

#endif

DEFINE_ENTRY_POINT(ui_main);

void ui_main() {
	if( data.init_done )
		return;
	data.init_done = 1;
#	if defined(NEKO_WINDOWS)
	{
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
	}
	data.tid = GetCurrentThreadId();
	data.wnd = CreateWindow(CLASS_NAME,"",0,0,0,0,0,NULL,NULL,NULL,NULL);
#	elif defined(NEKO_MAC)
	MPCreateTask(nothing,NULL,0,0,0,0,0,NULL); // creates a MPTask that will enable Carbon MT
	data.tid = pthread_self();
	EventTypeSpec ets = { UIEvent, eCall };
	InstallEventHandler(GetApplicationEventTarget(),NewEventHandlerUPP(handleEvents),1,&ets,0,0);
#	elif defined(NEKO_LINUX)
	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(NULL,NULL);
	gtk_timeout_add( 100, nothing, NULL ); 	// keep the loop alive
	setlocale(LC_NUMERIC,"POSIX"); // prevent broking atof()
	data.tid = pthread_self();
	pthread_mutex_init(&data.lock,NULL);
#	endif
}

/**
	ui_is_main : void -> bool
	<doc>
	Tells if the current thread is the main loop thread or not. The main loop thread is the one
	in which the first "ui" library primitive has been loaded.
	</doc>
**/
static value ui_is_main() {
#	ifdef NEKO_WINDOWS
	return alloc_bool(data.tid == GetCurrentThreadId());
#	else
	return alloc_bool(pthread_equal(data.tid,pthread_self()));
#	endif
}

/**
	ui_loop : void -> void
	<doc>
	Starts the native UI event loop. This method can only be called from the main thread.
	</doc>
**/
static value ui_loop() {
	if( !val_bool(ui_is_main()) )
		neko_error();
#	if defined(NEKO_WINDOWS)
	{
		MSG msg;
		while( GetMessage(&msg,NULL,0,0) ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if( msg.message == WM_QUIT )
				break;
		}
	}
#	elif defined(NEKO_MAC)
	RunApplicationEventLoop();
#	else
	gtk_main();
#	endif
	return val_null;
}

/**
	ui_stop_loop : void -> void
	<doc>
	Stop the native UI event loop. This method can only be called from the main thread.
	</doc>
**/
static value ui_stop_loop() {
	if( !val_bool(ui_is_main()) )
		neko_error();
#	if defined(NEKO_WINDOWS)
	while( !PostMessage(data.wnd,WM_QUIT,0,0) )
		Sleep(100);
#	elif defined(NEKO_MAC)
	QuitApplicationEventLoop();
#	else
	gtk_main_quit();
#	endif
	return val_null;
}

/**
	ui_sync : callb:(void -> void) -> void
	<doc>
	Queue a method call [callb] to be executed by the main thread while running the UI event
	loop. This can be used to perform UI updates in the UI thread using results processed by
	another thread.
	</doc>
**/
static value ui_sync( value f ) {
	value *r;
	val_check_function(f,0);
	r = alloc_root(1);
	*r = f;
#	if defined(NEKO_WINDOWS)
	while( !PostMessage(data.wnd,WM_SYNC_CALL,0,(LPARAM)r) )
		Sleep(100);
#	elif defined(NEKO_MAC)
	EventRef e;
	CreateEvent(NULL,UIEvent,eCall,GetCurrentEventTime(),kEventAttributeUserEvent,&e);
	SetEventParameter(e,pFunc,typeVoidPtr,sizeof(void*),&r);
	PostEventToQueue(GetMainEventQueue(),e,kEventPriorityStandard);
	ReleaseEvent(e);
#	elif defined(NEKO_LINUX)
	// the lock should not be needed because GTK is MT-safe
	// however the GTK lock mechanism is a LOT slower than
	// using a pthread_mutex
	pthread_mutex_lock(&data.lock);
	gtk_idle_add( onSyncCall, (gpointer)r );
	pthread_mutex_unlock(&data.lock);
#	endif
	return val_null;
}

DEFINE_PRIM(ui_loop,0);
DEFINE_PRIM(ui_stop_loop,0);
DEFINE_PRIM(ui_is_main,0);
DEFINE_PRIM(ui_sync,1);

/* ************************************************************************ */
