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
#ifndef SHA1_H
#define SHA1_H

#define SHA1_SIZE 20

typedef unsigned char SHA1_DIGEST[SHA1_SIZE];

typedef struct {
	unsigned int state[5];
	unsigned int count[2];
	unsigned char buffer[64];
} SHA1_CTX;

void sha1_init( SHA1_CTX *c );
void sha1_update( SHA1_CTX *c, const unsigned char *data, unsigned int len );
void sha1_final( SHA1_CTX *c, SHA1_DIGEST digest );

#endif
/* ************************************************************************ */
