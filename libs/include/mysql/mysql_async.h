/* Copyright (C) 2012 MariaDB Services and Kristian Nielsen
                 2015 MariaDB Corporation

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Common definitions for MariaDB non-blocking client library. */

#ifndef MYSQL_ASYNC_H
#define MYSQL_ASYNC_H

extern int my_connect_async(struct mysql_async_context *b, my_socket fd,
                            const struct sockaddr *name, uint namelen,
                            int vio_timeout);
extern ssize_t my_recv_async(struct mysql_async_context *b, int fd,
                             unsigned char *buf, size_t size, int timeout);
extern ssize_t my_send_async(struct mysql_async_context *b, int fd,
                             const unsigned char *buf, size_t size,
                             int timeout);
extern my_bool my_io_wait_async(struct mysql_async_context *b,
                                enum enum_vio_io_event event, int timeout);
#ifdef HAVE_OPENSSL
extern int my_ssl_read_async(struct mysql_async_context *b, SSL *ssl,
                             void *buf, int size);
extern int my_ssl_write_async(struct mysql_async_context *b, SSL *ssl,
                              const void *buf, int size);
#endif

#endif  /* MYSQL_ASYNC_H */
