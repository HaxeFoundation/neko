/* ************************************************************************ */
/*																			*/
/*  MYSQL 5.0 Protocol Implementation 										*/
/*  Copyright (c)2008 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */
#ifndef SOCKET_H
#define SOCKET_H

#ifdef _WIN32
#	define WINDOWS
#else
#	define POSIX
#endif

#ifdef WINDOWS
#	include <winsock2.h>
	typedef SOCKET PSOCK;
#else
	typedef int PSOCK;
#	define INVALID_SOCKET (-1)
#endif

typedef unsigned int PHOST;

#define UNRESOLVED_HOST ((PHOST)-1)

typedef enum {
	PS_OK = 0,
	PS_ERROR = -1,
	PS_BLOCK = -2,
} SERR;

void psock_init();
PSOCK psock_create();
void psock_close( PSOCK s );
SERR psock_connect( PSOCK s, PHOST h, int port );
SERR psock_set_timeout( PSOCK s, double timeout );
SERR psock_set_blocking( PSOCK s, int block );
SERR psock_set_fastsend( PSOCK s, int fast );

int psock_send( PSOCK s, const char *buf, int size );
int psock_recv( PSOCK s, char *buf, int size );

PHOST phost_resolve( const char *hostname );

#endif
/* ************************************************************************ */
