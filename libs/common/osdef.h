/* ************************************************************************ */
/*																			*/
/*  COMMON C LIBRARY				 										*/
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
#ifndef OS_H
#define OS_H

#if defined(_WIN32)
#	define OS_WINDOWS
#endif

#if defined(__APPLE__) || defined(__MACH__) || defined(macintosh)
#	define OS_MAC
#endif

#if defined(linux) || defined(__linux__)
#	define OS_LINUX
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#	define OS_BSD
#endif

#if defined(NEKO_LINUX) || defined(NEKO_MAC) || defined(NEKO_BSD) || defined(NEKO_GNUKBSD)
#	define OS_POSIX
#endif

#if defined(OS_WINDOWS)
#	define LITTLE_ENDIAN 1
#	define BIG_ENDIAN 2
#	define BYTE_ORDER LITTLE_ENDIAN
#elif defined(OS_MAC) || defined(OS_BSD)
#	include <machine/endian.h>
#else
#	include <endian.h>
#endif

#ifndef BYTE_ORDER
#	warning BYTE_ORDER unknown, assuming BIG_ENDIAN
#	define BYTE_ORDER BIG_ENDIAN
#endif

#if BYTE_ORDER == BIG_ENDIAN
#	define IS_BIG_ENDIAN
#else
#	define IS_LITTLE_ENDIAN
#endif

#ifndef true
#	define true 1
#	define false 0
	typedef int bool;
#endif

#endif
/* ************************************************************************ */
