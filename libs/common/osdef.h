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
