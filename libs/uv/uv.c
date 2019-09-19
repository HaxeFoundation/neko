#include <uv.h>
#include <neko.h>
#include <stdlib.h>

#if (UV_VERSION_MAJOR <= 0)
#	error "libuv1-dev required, uv version 0.x found"
#endif

// ------------- TYPES ----------------------------------------------

/**
	Type kinds, used for tagging abstracts.
**/

// Handle types

DEFINE_KIND(k_uv_loop_t);
DEFINE_KIND(k_uv_handle_t);
DEFINE_KIND(k_uv_dir_t);
DEFINE_KIND(k_uv_stream_t);
DEFINE_KIND(k_uv_tcp_t);
DEFINE_KIND(k_uv_udp_t);
DEFINE_KIND(k_uv_pipe_t);
DEFINE_KIND(k_uv_tty_t);
DEFINE_KIND(k_uv_poll_t);
DEFINE_KIND(k_uv_timer_t);
DEFINE_KIND(k_uv_prepare_t);
DEFINE_KIND(k_uv_check_t);
DEFINE_KIND(k_uv_idle_t);
DEFINE_KIND(k_uv_async_t);
DEFINE_KIND(k_uv_process_t);
DEFINE_KIND(k_uv_fs_event_t);
DEFINE_KIND(k_uv_fs_poll_t);
DEFINE_KIND(k_uv_signal_t);

// Request types

DEFINE_KIND(k_uv_req_t);
DEFINE_KIND(k_uv_getaddrinfo_t);
DEFINE_KIND(k_uv_getnameinfo_t);
DEFINE_KIND(k_uv_shutdown_t);
DEFINE_KIND(k_uv_write_t);
DEFINE_KIND(k_uv_connect_t);
DEFINE_KIND(k_uv_udp_send_t);
DEFINE_KIND(k_uv_fs_t);
DEFINE_KIND(k_uv_work_t);

// ------------- UTILITY MACROS -------------------------------------

/**
	The `data` field of handles and requests is used to store Haxe callbacks.
	These callbacks are called from the various `handle_...` functions, after
	pre-processing libuv results as necessary. At runtime, a callback is simply
	a `value`. To ensure it is not garbage-collected, we add the data pointer of
	the handle or request to Neko's global GC roots, then remove it after the
	callback is called.

	Handle-specific macros are defined further, in the HANDLE DATA section.
**/

// access the data of a request
#define UV_REQ_DATA(r) (*((value *)(((uv_req_t *)(r))->data)))
#define UV_REQ_DATA_A(r) (((uv_req_t *)(r))->data)

// allocate a request, add its callback to GC roots
#define UV_ALLOC_REQ(name, type, cb) \
	UV_ALLOC_CHECK(name, type); \
	{ \
		value *_data = alloc_root(1); \
		*_data = cb; \
		UV_REQ_DATA_A(UV_UNWRAP(name, type)) = (void *)_data; \
	};

// free a request, remove its callback from GC roots
#define UV_FREE_REQ(name) \
	free_root(UV_REQ_DATA_A(name)); \
	free(name);

// malloc a single value of the given type
#define UV_ALLOC(t) ((t *)malloc(sizeof(t)))

// unwrap an abstract block (see UV_ALLOC_CHECK notes below)
#define UV_UNWRAP(v, t) ((t *)val_data(v))

#define Connect_val(v) UV_UNWRAP(v, uv_connect_t)
#define Fs_val(v) UV_UNWRAP(v, uv_fs_t)
#define FsEvent_val(v) UV_UNWRAP(v, uv_fs_event_t)
#define GetAddrInfo_val(v) UV_UNWRAP(v, uv_getaddrinfo_t)
#define Handle_val(v) UV_UNWRAP(v, uv_handle_t)
#define Loop_val(v) UV_UNWRAP(v, uv_loop_t)
#define Pipe_val(v) UV_UNWRAP(v, uv_pipe_t)
#define Process_val(v) UV_UNWRAP(v, uv_process_t)
#define Shutdown_val(v) UV_UNWRAP(v, uv_shutdown_t)
#define Stream_val(v) UV_UNWRAP(v, uv_stream_t)
#define Tcp_val(v) UV_UNWRAP(v, uv_tcp_t)
#define Timer_val(v) UV_UNWRAP(v, uv_timer_t)
#define Udp_val(v) UV_UNWRAP(v, uv_udp_t)
#define UdpSend_val(v) UV_UNWRAP(v, uv_udp_send_t)
#define Write_val(v) UV_UNWRAP(v, uv_write_t)
	
// (no-op) typecast to juggle value and uv_file (which is an unboxed integer)
#define alloc_file(f) (alloc_int(f))
#define val_file(v) ((uv_file)val_int(v))

/**
	Neko primitives cannot be directly called if they take 6 or more arguments.
	The following macros declare wrappers which take the arguments as an array.
**/

#define BC_WRAP6(name) \
	static value name ## _dyn(value args) { \
		value *argv = val_array_ptr(args); \
		return name(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]); \
	} \
	DEFINE_PRIM(name ## _dyn, 1);
#define BC_WRAP7(name) \
	static value name ## _dyn(value args) { \
		value *argv = val_array_ptr(args); \
		return name(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]); \
	} \
	DEFINE_PRIM(name ## _dyn, 1);
#define BC_WRAP8(name) \
	static value name ## _dyn(value args) { \
		value *argv = val_array_ptr(args); \
		return name(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]); \
	} \
	DEFINE_PRIM(name ## _dyn, 1);
#define BC_WRAP9(name) \
	static value name ## _dyn(value args) { \
		value *argv = val_array_ptr(args); \
		return name(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]); \
	} \
	DEFINE_PRIM(name ## _dyn, 1);
#define BC_WRAP10(name) \
	static value name ## _dyn(value args) { \
		value *argv = val_array_ptr(args); \
		return name(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9]); \
	} \
	DEFINE_PRIM(name ## _dyn, 1);

// ------------- HAXE CONSTRUCTORS ----------------------------------

/**
	To make it easier to construct values expected by the Haxe standard library,
	the library provides constructors for various types. These are called from
	the methods in this file and either returned or thrown back into Haxe code.
**/

static value construct_error;
static value construct_fs_stat;
static value construct_fs_dirent;
static value construct_addrinfo_ipv4;
static value construct_addrinfo_ipv6;
static value construct_addrport;
static value construct_pipe_accept_socket;
static value construct_pipe_accept_pipe;

static value glue_register(
	value c_error,
	value c_fs_stat,
	value c_fs_dirent,
	value c_addrinfo_ipv4,
	value c_addrinfo_ipv6,
	value c_addrport,
	value c_pipe_accept_socket,
	value c_pipe_accept_pipe
) {
	construct_error = c_error;
	construct_fs_stat = c_fs_stat;
	construct_fs_dirent = c_fs_dirent;
	construct_addrinfo_ipv4 = c_addrinfo_ipv4;
	construct_addrinfo_ipv6 = c_addrinfo_ipv6;
	construct_addrport = c_addrport;
	construct_pipe_accept_socket = c_pipe_accept_socket;
	construct_pipe_accept_pipe = c_pipe_accept_pipe;
	return val_null;
}
BC_WRAP8(glue_register);

// ------------- ERROR HANDLING -------------------------------------

/**
	UV_ERROR throws an error with the given error code.

	UV_ALLOC_CHECK tries to allocate a variable of the given type with the given
	name and throws an error if this fails. UV_ALLOC_CHECK_C is the same, but
	allows specifying custom clean-up code before the error result is returned.
	Allocation returns a value that is a pointer-pointer to the malloc'ed native
	value.

	UV_ERROR_CHECK checks for a libuv error in the given int expression (indicated
	by a negative value), and in case of an error, throws an error. Once again,
	UV_ERROR_CHECK_C is the same, but allows specifying custom clean-up code.
**/

#define UV_ERROR(errno) val_throw(val_call1(construct_error, alloc_int(errno)))

#define UV_ALLOC_CHECK_C(var, type, cleanup) \
	type *_native = UV_ALLOC(type); \
	if (_native == NULL) { \
		cleanup; \
		UV_ERROR(0); \
	} \
	value var = alloc_abstract(k_ ## type, _native);

#define UV_ALLOC_CHECK(var, type) UV_ALLOC_CHECK_C(var, type, )

#define UV_ERROR_CHECK_C(expr, cleanup) do { \
		int __tmp_result = expr; \
		if (__tmp_result < 0) { \
			cleanup; \
			UV_ERROR(__tmp_result); \
		} \
	} while (0)

#define UV_ERROR_CHECK(expr) UV_ERROR_CHECK_C(expr, )

// ------------- LOOP -----------------------------------------------

static value w_loop_init(void) {
	UV_ALLOC_CHECK(loop, uv_loop_t);
	UV_ERROR_CHECK_C(uv_loop_init(Loop_val(loop)), free(Loop_val(loop)));
	return loop;
}
DEFINE_PRIM(w_loop_init, 0);

static value w_loop_close(value loop) {
	UV_ERROR_CHECK(uv_loop_close(Loop_val(loop)));
	free(Loop_val(loop));
	return val_null;
}
DEFINE_PRIM(w_loop_close, 1);

static value w_run(value loop, value mode) {
	return alloc_bool(uv_run(Loop_val(loop), val_int(mode)) != 0);
}
DEFINE_PRIM(w_run, 2);

static value w_stop(value loop) {
	uv_stop(Loop_val(loop));
	return val_null;
}
DEFINE_PRIM(w_stop, 1);

static value w_loop_alive(value loop) {
	return alloc_bool(uv_loop_alive(Loop_val(loop)) != 0);
}
DEFINE_PRIM(w_loop_alive, 1);

// ------------- FILESYSTEM -----------------------------------------

/**
	FS handlers all have the same structure.

	The async version (no suffix) calls the callback with either the result in
	the second argument, or an error in the first argument.

	The sync version (`_sync` suffix) returns the result directly.
**/

#define UV_FS_HANDLER(name, setup) \
	static void name(uv_fs_t *req) { \
		value cb = UV_REQ_DATA(req); \
		if (req->result < 0) \
			val_call2(cb, val_call1(construct_error, alloc_int(req->result)), val_null); \
		else { \
			value value2; \
			do setup while (0); \
			val_call2(cb, val_null, value2); \
		} \
		uv_fs_req_cleanup(req); \
		UV_FREE_REQ(req); \
	} \
	static value name ## _sync(value req_w) { \
		uv_fs_t *req = Fs_val(req_w); \
		value value2; \
		do setup while (0); \
		return value2; \
	}

typedef struct vlist {
	value v;
	struct vlist *next;
} vlist;

UV_FS_HANDLER(handle_fs_cb, value2 = val_null;);
UV_FS_HANDLER(handle_fs_cb_bytes, value2 = alloc_string((const char *)req->ptr););
UV_FS_HANDLER(handle_fs_cb_path, value2 = alloc_string((const char *)req->path););
UV_FS_HANDLER(handle_fs_cb_int, value2 = alloc_int(req->result););
UV_FS_HANDLER(handle_fs_cb_file, value2 = alloc_int(req->result););
UV_FS_HANDLER(handle_fs_cb_stat, {
		value args[12] = {};
		args[0] = alloc_int(req->statbuf.st_dev);
		args[1] = alloc_int(req->statbuf.st_mode);
		args[2] = alloc_int(req->statbuf.st_nlink);
		args[3] = alloc_int(req->statbuf.st_uid);
		args[4] = alloc_int(req->statbuf.st_gid);
		args[5] = alloc_int(req->statbuf.st_rdev);
		args[6] = alloc_int(req->statbuf.st_ino);
		args[7] = alloc_int(req->statbuf.st_size);
		args[8] = alloc_int(req->statbuf.st_blksize);
		args[9] = alloc_int(req->statbuf.st_blocks);
		args[10] = alloc_int(req->statbuf.st_flags);
		args[11] = alloc_int(req->statbuf.st_gen);
		value2 = val_callN(construct_fs_stat, args, 12);
	});
UV_FS_HANDLER(handle_fs_cb_scandir, {
		uv_dirent_t ent;
		vlist *last = NULL;
		int count = 0;
		while (uv_fs_scandir_next(req, &ent) != UV_EOF) {
			count++;
			vlist *node = (vlist *)malloc(sizeof(vlist));
			node->v = val_call2(construct_fs_dirent, alloc_string(ent.name), alloc_int(ent.type));
			node->next = last;
			last = node;
		}
		value2 = alloc_array(count);
		for (int i = 0; i < count; i++) {
			val_array_ptr(value2)[count - i - 1] = last->v;
			vlist *next = last->next;
			free(last);
			last = next;
		}
	});

/**
	Most FS functions from libuv can be wrapped with FS_WRAP (or one of the
	FS_WRAP# variants defined below) - create a request, register a callback for
	it, register the callback with the GC, perform request. Then, either in the
	handler function (synchronous or asynchronous), the result is checked and
	given to the Haxe callback if successful, with the appropriate value
	conversions done, as defined in the various UV_FS_HANDLERs above.
**/

#define FS_WRAP(name, sign, precall, call, argcount, argcount2, handler) \
	static value w_fs_ ## name(value loop, sign, value cb) { \
		UV_ALLOC_REQ(req, uv_fs_t, cb); \
		precall \
		UV_ERROR_CHECK_C(uv_fs_ ## name(Loop_val(loop), Fs_val(req), call, handler), UV_FREE_REQ(Fs_val(req))); \
		return val_null; \
	} \
	DEFINE_PRIM(w_fs_ ## name, argcount); \
	static value w_fs_ ## name ## _sync(value loop, sign) { \
		UV_ALLOC_CHECK(req, uv_fs_t); \
		precall \
		UV_ERROR_CHECK_C(uv_fs_ ## name(Loop_val(loop), Fs_val(req), call, NULL), free(Fs_val(req))); \
		return handler ## _sync(req); \
	} \
	DEFINE_PRIM(w_fs_ ## name ## _sync, argcount2);

#define COMMA ,
#define FS_WRAP1(name, arg1conv, handler) \
	FS_WRAP(name, value _arg1, , arg1conv(_arg1), 3, 2, handler);
#define FS_WRAP2(name, arg1conv, arg2conv, handler) \
	FS_WRAP(name, value _arg1 COMMA value _arg2, , arg1conv(_arg1) COMMA arg2conv(_arg2), 4, 3, handler);
#define FS_WRAP3(name, arg1conv, arg2conv, arg3conv, handler) \
	FS_WRAP(name, value _arg1 COMMA value _arg2 COMMA value _arg3, , arg1conv(_arg1) COMMA arg2conv(_arg2) COMMA arg3conv(_arg3), 5, 4, handler);
#define FS_WRAP4(name, arg1conv, arg2conv, arg3conv, arg4conv, handler) \
	FS_WRAP(name, value _arg1 COMMA value _arg2 COMMA value _arg3 COMMA value _arg4, , arg1conv(_arg1) COMMA arg2conv(_arg2) COMMA arg3conv(_arg3) COMMA arg4conv(_arg4), 6, 5, handler); \
	BC_WRAP6(w_fs_ ## name);

FS_WRAP1(close, val_file, handle_fs_cb);
FS_WRAP3(open, val_string, val_int, val_int, handle_fs_cb_file);
FS_WRAP1(unlink, val_string, handle_fs_cb);
FS_WRAP2(mkdir, val_string, val_int, handle_fs_cb);
FS_WRAP1(mkdtemp, val_string, handle_fs_cb_path);
FS_WRAP1(rmdir, val_string, handle_fs_cb);
FS_WRAP2(scandir, val_string, val_int, handle_fs_cb_scandir);
FS_WRAP1(stat, val_string, handle_fs_cb_stat);
FS_WRAP1(fstat, val_file, handle_fs_cb_stat);
FS_WRAP1(lstat, val_string, handle_fs_cb_stat);
FS_WRAP2(rename, val_string, val_string, handle_fs_cb);
FS_WRAP1(fsync, val_file, handle_fs_cb);
FS_WRAP1(fdatasync, val_file, handle_fs_cb);
FS_WRAP2(ftruncate, val_file, val_int, handle_fs_cb);
FS_WRAP4(sendfile, val_file, val_file, val_int, val_int, handle_fs_cb);
FS_WRAP2(access, val_string, val_int, handle_fs_cb);
FS_WRAP2(chmod, val_string, val_int, handle_fs_cb);
FS_WRAP2(fchmod, val_file, val_int, handle_fs_cb);
FS_WRAP3(utime, val_string, val_float, val_float, handle_fs_cb);
FS_WRAP3(futime, val_file, val_float, val_float, handle_fs_cb);
FS_WRAP2(link, val_string, val_string, handle_fs_cb);
FS_WRAP3(symlink, val_string, val_string, val_int, handle_fs_cb);
FS_WRAP1(readlink, val_string, handle_fs_cb_bytes);
FS_WRAP1(realpath, val_string, handle_fs_cb_bytes);
FS_WRAP3(chown, val_string, (uv_uid_t)val_int, (uv_gid_t)val_int, handle_fs_cb);
FS_WRAP3(fchown, val_file, (uv_uid_t)val_int, (uv_gid_t)val_int, handle_fs_cb);

/**
	`fs_read` and `fs_write` require a tiny bit of setup just before the libuv
	request is actually started; namely, a buffer structure needs to be set up,
	which is simply a wrapper of a pointer to the Haxe bytes value.

	libuv actually supports multiple buffers in both calls, but this is not
	mirrored in the Haxe API, so only a single-buffer call is used.
**/

FS_WRAP(read,
	value file COMMA value buffer COMMA value offset COMMA value length COMMA value position,
	uv_buf_t buf = uv_buf_init(val_string(buffer) + val_int(offset), val_int(length));,
	val_file(file) COMMA &buf COMMA 1 COMMA val_int(position),
	7, 6,
	handle_fs_cb_int);
BC_WRAP7(w_fs_read);
BC_WRAP6(w_fs_read_sync);

FS_WRAP(write,
	value file COMMA value buffer COMMA value offset COMMA value length COMMA value position,
	uv_buf_t buf = uv_buf_init(val_string(buffer) + val_int(offset), val_int(length));,
	val_file(file) COMMA &buf COMMA 1 COMMA val_int(position),
	7, 6,
	handle_fs_cb_int);
BC_WRAP7(w_fs_write);
BC_WRAP6(w_fs_write_sync);

// ------------- HANDLE DATA ----------------------------------------

/**
	There is a single `void *data` field on requests and handles. For requests,
	we use this to directly store the `value` for the callback function. For
	handles, however, it is sometimes necessary to register multiple different
	callbacks, hence a separate allocated struct is needed to hold them all.
	All of the fields of the struct are registered with the garbage collector
	immediately upon creation, although initially some of the callback fields are
	set to unit values.
**/

#define UV_HANDLE_DATA(h) ((((uv_handle_t *)(h))->data))
#define UV_HANDLE_DATA_SUB(h, t) (((uv_w_handle_t *)UV_HANDLE_DATA(h))->u.t)

typedef struct {
	value cb_close;
	union {
		struct {
			value cb1;
			value cb2;
		} all;
		struct {
			value cb_fs_event;
			value unused1;
		} fs_event;
		struct {
			value cb_read;
			value cb_connection;
		} stream;
		struct {
			value cb_read;
			value cb_connection;
		} tcp;
		struct {
			value cb_read;
			value unused1;
		} udp;
		struct {
			value cb_timer;
			value unused1;
		} timer;
		struct {
			value cb_exit;
			value unused1;
		} process;
		struct {
			value unused1;
			value unused2;
		} pipe;
	} u;
} uv_w_handle_t;

static uv_w_handle_t *alloc_data(void) {
	return (uv_w_handle_t *)alloc_root(3);
}

static void unalloc_data(uv_w_handle_t *data) {
	free_root((void *)data);
}

static void handle_close_cb(uv_handle_t *handle) {
	value cb = ((uv_w_handle_t *)UV_HANDLE_DATA(handle))->cb_close;
	unalloc_data(UV_HANDLE_DATA(handle));
	free(handle);
	val_call1(cb, val_null);
}

static value w_close(value handle, value cb) {
	((uv_w_handle_t *)UV_HANDLE_DATA(Handle_val(handle)))->cb_close = cb;
	uv_close(Handle_val(handle), handle_close_cb);
	return val_null;
}
DEFINE_PRIM(w_close, 2);

static value w_ref(value handle) {
	uv_ref(Handle_val(handle));
	return val_null;
}
DEFINE_PRIM(w_ref, 1);

static value w_unref(value handle) {
	uv_unref(Handle_val(handle));
	return val_null;
}
DEFINE_PRIM(w_unref, 1);

// ------------- FILESYSTEM EVENTS ----------------------------------

static void handle_fs_event_cb(uv_fs_event_t *handle, const char *filename, int events, int status) {
	value cb = UV_HANDLE_DATA_SUB(handle, fs_event).cb_fs_event;
	if (status < 0)
		val_call3(cb, val_call1(construct_error, alloc_int(status)), val_null, val_null);
	else
		val_call3(cb, val_null, alloc_string(filename), alloc_int(events));
}

static value w_fs_event_start(value loop, value path, value recursive, value cb) {
	UV_ALLOC_CHECK(handle, uv_fs_event_t);
	UV_ERROR_CHECK_C(uv_fs_event_init(Loop_val(loop), FsEvent_val(handle)), free(FsEvent_val(handle)));
	UV_HANDLE_DATA(FsEvent_val(handle)) = alloc_data();
	if (UV_HANDLE_DATA(FsEvent_val(handle)) == NULL)
		UV_ERROR(0);
	UV_HANDLE_DATA_SUB(FsEvent_val(handle), fs_event).cb_fs_event = cb;
	UV_ERROR_CHECK_C(
		uv_fs_event_start(FsEvent_val(handle), handle_fs_event_cb, val_string(path), val_bool(recursive) ? UV_FS_EVENT_RECURSIVE : 0),
		{ unalloc_data(UV_HANDLE_DATA(FsEvent_val(handle))); free(FsEvent_val(handle)); }
		);
	return handle;
}
DEFINE_PRIM(w_fs_event_start, 4);

static value w_fs_event_stop(value handle, value cb) {
	UV_ERROR_CHECK_C(
		uv_fs_event_stop(FsEvent_val(handle)),
		{ unalloc_data(UV_HANDLE_DATA(FsEvent_val(handle))); free(FsEvent_val(handle)); }
		);
	((uv_w_handle_t *)UV_HANDLE_DATA(FsEvent_val(handle)))->cb_close = cb;
	uv_close(Handle_val(handle), handle_close_cb);
	return val_null;
}
DEFINE_PRIM(w_fs_event_stop, 2);

// ------------- STREAM ---------------------------------------------

static void handle_stream_cb(uv_req_t *req, int status) {
	value cb = UV_REQ_DATA(req);
	if (status < 0)
		val_call1(cb, val_call1(construct_error, alloc_int(status)));
	else
		val_call1(cb, val_null);
	UV_FREE_REQ(req);
}

static void handle_stream_cb_connection(uv_stream_t *stream, int status) {
	value cb = UV_HANDLE_DATA_SUB(stream, stream).cb_connection;
	if (status < 0)
		val_call1(cb, val_call1(construct_error, alloc_int(status)));
	else
		val_call1(cb, val_null);
}

static void handle_stream_cb_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = malloc(suggested_size);
	buf->len = suggested_size;
}

static void handle_stream_cb_read(uv_stream_t *stream, long int nread, const uv_buf_t *buf) {
	value cb = UV_HANDLE_DATA_SUB(stream, stream).cb_read;
	if (nread < 0)
		val_call3(cb, val_call1(construct_error, alloc_int(nread)), val_null, alloc_int(0));
	else {
		val_call3(cb, val_null, copy_string(buf->base, nread), alloc_int(nread));
		free(buf->base); // TODO
	}
}

static value w_shutdown(value stream, value cb) {
	UV_ALLOC_REQ(req, uv_shutdown_t, cb);
	UV_ERROR_CHECK_C(uv_shutdown(Shutdown_val(req), Stream_val(stream), (void (*)(uv_shutdown_t *, int))handle_stream_cb), UV_FREE_REQ(Shutdown_val(req)));
	return val_null;
}
DEFINE_PRIM(w_shutdown, 2);

static value w_listen(value stream, value backlog, value cb) {
	UV_HANDLE_DATA_SUB(Stream_val(stream), stream).cb_connection = cb;
	UV_ERROR_CHECK(uv_listen(Stream_val(stream), val_int(backlog), handle_stream_cb_connection));
	return val_null;
}
DEFINE_PRIM(w_listen, 3);

static value w_write(value stream, value data, value length, value cb) {
	UV_ALLOC_REQ(req, uv_write_t, cb);
	uv_buf_t buf = uv_buf_init(val_string(data), val_int(length));
	UV_ERROR_CHECK_C(uv_write(Write_val(req), Stream_val(stream), &buf, 1, (void (*)(uv_write_t *, int))handle_stream_cb), UV_FREE_REQ(Write_val(req)));
	return val_null;
}
DEFINE_PRIM(w_write, 4);

static value w_read_start(value stream, value cb) {
	UV_HANDLE_DATA_SUB(Stream_val(stream), stream).cb_read = cb;
	UV_ERROR_CHECK(uv_read_start(Stream_val(stream), handle_stream_cb_alloc, handle_stream_cb_read));
	return val_null;
}
DEFINE_PRIM(w_read_start, 2);

static value w_read_stop(value stream) {
	UV_ERROR_CHECK(uv_read_stop(Stream_val(stream)));
	return val_null;
}
DEFINE_PRIM(w_read_stop, 1);

// ------------- NETWORK MACROS -------------------------------------

#define UV_SOCKADDR_IPV4(var, host, port) \
	struct sockaddr_in var; \
	var.sin_family = AF_INET; \
	var.sin_port = htons((unsigned short)port); \
	var.sin_addr.s_addr = htonl(host);
#define UV_SOCKADDR_IPV6(var, host, port) \
	struct sockaddr_in6 var; \
	memset(&var, 0, sizeof(var)); \
	var.sin6_family = AF_INET6; \
	var.sin6_port = htons((unsigned short)port); \
	memcpy(var.sin6_addr.s6_addr, host, 16);

// ------------- TCP ------------------------------------------------

static value w_tcp_init(value loop) {
	UV_ALLOC_CHECK(handle, uv_tcp_t);
	UV_ERROR_CHECK_C(uv_tcp_init(Loop_val(loop), Tcp_val(handle)), free(Tcp_val(handle)));
	UV_HANDLE_DATA(Tcp_val(handle)) = alloc_data();
	if (UV_HANDLE_DATA(Tcp_val(handle)) == NULL)
		UV_ERROR(0);
	return handle;
}
DEFINE_PRIM(w_tcp_init, 1);

static value w_tcp_nodelay(value handle, value enable) {
	UV_ERROR_CHECK(uv_tcp_nodelay(Tcp_val(handle), val_bool(enable)));
	return val_null;
}
DEFINE_PRIM(w_tcp_nodelay, 2);

static value w_tcp_keepalive(value handle, value enable, value delay) {
	UV_ERROR_CHECK(uv_tcp_keepalive(Tcp_val(handle), val_bool(enable), val_int(delay)));
	return val_null;
}
DEFINE_PRIM(w_tcp_keepalive, 3);

static value w_tcp_accept(value loop, value server) {
	UV_ALLOC_CHECK(client, uv_tcp_t);
	UV_ERROR_CHECK_C(uv_tcp_init(Loop_val(loop), Tcp_val(client)), free(Tcp_val(client)));
	UV_HANDLE_DATA(Tcp_val(client)) = alloc_data();
	if (UV_HANDLE_DATA(Tcp_val(client)) == NULL)
		UV_ERROR(0);
	UV_ERROR_CHECK_C(uv_accept(Stream_val(server), Stream_val(client)), free(Tcp_val(client)));
	return client;
}
DEFINE_PRIM(w_tcp_accept, 2);

static value w_tcp_bind_ipv4(value handle, value host, value port) {
	UV_SOCKADDR_IPV4(addr, val_int(host), val_int(port));
	UV_ERROR_CHECK(uv_tcp_bind(Tcp_val(handle), (const struct sockaddr *)&addr, 0));
	return val_null;
}
DEFINE_PRIM(w_tcp_bind_ipv4, 3);

static value w_tcp_bind_ipv6(value handle, value host, value port, value ipv6only) {
	UV_SOCKADDR_IPV6(addr, val_string(host), val_int(port));
	UV_ERROR_CHECK(uv_tcp_bind(Tcp_val(handle), (const struct sockaddr *)&addr, val_bool(ipv6only) ? UV_TCP_IPV6ONLY : 0));
	return val_null;
}
DEFINE_PRIM(w_tcp_bind_ipv6, 4);

static value w_tcp_connect_ipv4(value handle, value host, value port, value cb) {
	UV_SOCKADDR_IPV4(addr, val_int(host), val_int(port));
	UV_ALLOC_REQ(req, uv_connect_t, cb);
	UV_ERROR_CHECK_C(uv_tcp_connect(Connect_val(req), Tcp_val(handle), (const struct sockaddr *)&addr, (void (*)(uv_connect_t *, int))handle_stream_cb), UV_FREE_REQ(Connect_val(req)));
	return val_null;
}
DEFINE_PRIM(w_tcp_connect_ipv4, 4);

static value w_tcp_connect_ipv6(value handle, value host, value port, value cb) {
	UV_SOCKADDR_IPV6(addr, val_string(host), val_int(port));
	UV_ALLOC_REQ(req, uv_connect_t, cb);
	UV_ERROR_CHECK_C(uv_tcp_connect(Connect_val(req), Tcp_val(handle), (const struct sockaddr *)&addr, (void (*)(uv_connect_t *, int))handle_stream_cb), UV_FREE_REQ(Connect_val(req)));
	return val_null;
}
DEFINE_PRIM(w_tcp_connect_ipv6, 4);

static value w_getname(struct sockaddr_storage *addr) {
	if (addr->ss_family == AF_INET) {
		value w_addr = val_call1(construct_addrinfo_ipv4, alloc_int32(ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr)));
		return val_call2(construct_addrport, w_addr, alloc_int(ntohs(((struct sockaddr_in *)addr)->sin_port)));
	} else if (addr->ss_family == AF_INET6) {
		value w_addr = val_call1(construct_addrinfo_ipv6, copy_string((char *)((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, 16));
		return val_call2(construct_addrport, w_addr, alloc_int(ntohs(((struct sockaddr_in6 *)addr)->sin6_port)));
	}
	UV_ERROR(0);
	return val_null;
}

static value w_tcp_getsockname(value handle) {
	struct sockaddr_storage storage;
	int dummy = sizeof(struct sockaddr_storage);
	UV_ERROR_CHECK(uv_tcp_getsockname(Tcp_val(handle), (struct sockaddr *)&storage, &dummy));
	return w_getname(&storage);
}
DEFINE_PRIM(w_tcp_getsockname, 1);

static value w_tcp_getpeername(value handle) {
	struct sockaddr_storage storage;
	int dummy = sizeof(struct sockaddr_storage);
	UV_ERROR_CHECK(uv_tcp_getpeername(Tcp_val(handle), (struct sockaddr *)&storage, &dummy));
	return w_getname(&storage);
}
DEFINE_PRIM(w_tcp_getpeername, 1);

// ------------- UDP ------------------------------------------------

static void handle_udp_cb_recv(uv_udp_t *handle, long int nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned int flags) {
	value cb = UV_HANDLE_DATA_SUB(handle, udp).cb_read;
	value args[5] = {val_null, val_null, val_null, val_null, val_null};
	if (nread < 0)
		args[0] = val_call1(construct_error, alloc_int(nread));
	else {
		args[1] = copy_string(buf->base, nread);
		args[2] = alloc_int(nread);
		if (addr != NULL) {
			if (addr->sa_family == AF_INET) {
				args[3] = val_call1(construct_addrinfo_ipv4, alloc_int(ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr)));
				args[4] = alloc_int(ntohs(((struct sockaddr_in *)addr)->sin_port));
			} else if (addr->sa_family == AF_INET6) {
				args[3] = val_call1(construct_addrinfo_ipv6, copy_string((char *)((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, 16));
				args[4] = alloc_int(ntohs(((struct sockaddr_in6 *)addr)->sin6_port));
			}
		}
	}
	val_callN(cb, args, 5);
}

static value w_udp_init(value loop) {
	UV_ALLOC_CHECK(handle, uv_udp_t);
	UV_ERROR_CHECK_C(uv_udp_init(Loop_val(loop), Udp_val(handle)), free(Udp_val(handle)));
	UV_HANDLE_DATA(Udp_val(handle)) = alloc_data();
	if (UV_HANDLE_DATA(Udp_val(handle)) == NULL)
		UV_ERROR(0);
	return handle;
}
DEFINE_PRIM(w_udp_init, 1);

static value w_udp_bind_ipv4(value handle, value host, value port) {
	UV_SOCKADDR_IPV4(addr, val_int32(host), val_int(port));
	UV_ERROR_CHECK(uv_udp_bind(Udp_val(handle), (const struct sockaddr *)&addr, 0));
	return val_null;
}
DEFINE_PRIM(w_udp_bind_ipv4, 3);

static value w_udp_bind_ipv6(value handle, value host, value port, value ipv6only) {
	UV_SOCKADDR_IPV6(addr, val_string(host), val_int(port));
	UV_ERROR_CHECK(uv_udp_bind(Udp_val(handle), (const struct sockaddr *)&addr, val_bool(ipv6only) ? UV_UDP_IPV6ONLY : 0));
	return val_null;
}
DEFINE_PRIM(w_udp_bind_ipv6, 4);

static value w_udp_send_ipv4(value handle, value msg, value offset, value length, value host, value port, value cb) {
	UV_SOCKADDR_IPV4(addr, val_int(host), val_int(port));
	UV_ALLOC_REQ(req, uv_udp_send_t, cb);
	uv_buf_t buf = uv_buf_init(val_string(msg) + val_int(offset), val_int(length));
	UV_ERROR_CHECK_C(uv_udp_send(UdpSend_val(req), Udp_val(handle), &buf, 1, (const struct sockaddr *)&addr, (void (*)(uv_udp_send_t *, int))handle_stream_cb), UV_FREE_REQ(UdpSend_val(req)));
	return val_null;
}
BC_WRAP7(w_udp_send_ipv4);

static value w_udp_send_ipv6(value handle, value msg, value offset, value length, value host, value port, value cb) {
	UV_SOCKADDR_IPV6(addr, val_string(host), val_int(port));
	UV_ALLOC_REQ(req, uv_udp_send_t, cb);
	uv_buf_t buf = uv_buf_init(val_string(msg) + val_int(offset), val_int(length));
	UV_ERROR_CHECK_C(uv_udp_send(UdpSend_val(req), Udp_val(handle), &buf, 1, (const struct sockaddr *)&addr, (void (*)(uv_udp_send_t *, int))handle_stream_cb), UV_FREE_REQ(UdpSend_val(req)));
	return val_null;
}
BC_WRAP7(w_udp_send_ipv6);

static value w_udp_recv_start(value handle, value cb) {
	UV_HANDLE_DATA_SUB(Udp_val(handle), udp).cb_read = cb;
	UV_ERROR_CHECK(uv_udp_recv_start(Udp_val(handle), handle_stream_cb_alloc, handle_udp_cb_recv));
	return val_null;
}
DEFINE_PRIM(w_udp_recv_start, 2);

static value w_udp_recv_stop(value handle) {
	UV_ERROR_CHECK(uv_udp_recv_stop(Udp_val(handle)));
	UV_HANDLE_DATA_SUB(Udp_val(handle), udp).cb_read = val_null;
	return val_null;
}
DEFINE_PRIM(w_udp_recv_stop, 1);

static value w_udp_set_membership(value handle, value address, value intfc, value join) {
	UV_ERROR_CHECK(uv_udp_set_membership(Udp_val(handle), val_string(address), val_is_null(intfc) ? NULL : val_string(intfc), val_bool(join) ? UV_JOIN_GROUP : UV_LEAVE_GROUP));
	return val_null;
}
DEFINE_PRIM(w_udp_set_membership, 4);

static value w_udp_close(value handle, value cb) {
	return w_close(handle, cb);
}
DEFINE_PRIM(w_udp_close, 2);

static value w_udp_getsockname(value handle) {
	struct sockaddr_storage storage;
	int dummy = sizeof(struct sockaddr_storage);
	UV_ERROR_CHECK(uv_udp_getsockname(Udp_val(handle), (struct sockaddr *)&storage, &dummy));
	return w_getname(&storage);
}
DEFINE_PRIM(w_udp_getsockname, 1);

static value w_udp_set_broadcast(value handle, value flag) {
	UV_ERROR_CHECK(uv_udp_set_broadcast(Udp_val(handle), val_bool(flag)));
	return val_null;
}
DEFINE_PRIM(w_udp_set_broadcast, 2);

static value w_udp_set_multicast_interface(value handle, value intfc) {
	UV_ERROR_CHECK(uv_udp_set_multicast_interface(Udp_val(handle), val_string(intfc)));
	return val_null;
}
DEFINE_PRIM(w_udp_set_multicast_interface, 2);

static value w_udp_set_multicast_loopback(value handle, value flag) {
	UV_ERROR_CHECK(uv_udp_set_multicast_loop(Udp_val(handle), val_bool(flag) ? 1 : 0));
	return val_null;
}
DEFINE_PRIM(w_udp_set_multicast_loopback, 2);

static value w_udp_set_multicast_ttl(value handle, value ttl) {
	UV_ERROR_CHECK(uv_udp_set_multicast_ttl(Udp_val(handle), val_int(ttl)));
	return val_null;
}
DEFINE_PRIM(w_udp_set_multicast_ttl, 2);

static value w_udp_set_ttl(value handle, value ttl) {
	UV_ERROR_CHECK(uv_udp_set_ttl(Udp_val(handle), val_int(ttl)));
	return val_null;
}
DEFINE_PRIM(w_udp_set_ttl, 2);

static value w_udp_get_recv_buffer_size(value handle) {
	int size_u = 0;
	int res = uv_recv_buffer_size(Handle_val(handle), &size_u);
	return alloc_int(res);
}
DEFINE_PRIM(w_udp_get_recv_buffer_size, 1);

static value w_udp_get_send_buffer_size(value handle) {
	int size_u = 0;
	int res = uv_send_buffer_size(Handle_val(handle), &size_u);
	return alloc_int(res);
}
DEFINE_PRIM(w_udp_get_send_buffer_size, 1);

static value w_udp_set_recv_buffer_size(value handle, value size) {
	int size_u = val_int(size);
	int res = uv_recv_buffer_size(Handle_val(handle), &size_u);
	return alloc_int(res);
}
DEFINE_PRIM(w_udp_set_recv_buffer_size, 2);

static value w_udp_set_send_buffer_size(value handle, value size) {
	int size_u = val_int(size);
	int res = uv_send_buffer_size(Handle_val(handle), &size_u);
	return alloc_int(res);
}
DEFINE_PRIM(w_udp_set_send_buffer_size, 2);

// ------------- DNS ------------------------------------------------

static void handle_dns_gai_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
	value cb = UV_REQ_DATA(req);
	if (status < 0)
		val_call2(cb, val_call1(construct_error, alloc_int(status)), val_null);
	else {
		int count = 0;
		struct addrinfo *cur;
		for (cur = res; cur != NULL; cur = cur->ai_next) {
			if (cur->ai_family == AF_INET || cur->ai_family == AF_INET6)
				count++;
		}
		value arr = alloc_array(count);
		cur = res;
		for (int i = 0; i < count; i++) {
			if (cur->ai_family == AF_INET)
				val_array_ptr(arr)[i] = val_call1(construct_addrinfo_ipv4, alloc_int32(ntohl(((struct sockaddr_in *)cur->ai_addr)->sin_addr.s_addr)));
			else if (cur->ai_family == AF_INET6)
				val_array_ptr(arr)[i] = val_call1(construct_addrinfo_ipv6, copy_string((char *)((struct sockaddr_in6 *)cur->ai_addr)->sin6_addr.s6_addr, 1));
			cur = cur->ai_next;
		}
		uv_freeaddrinfo(res);
		val_call2(cb, val_null, arr);
	}
	UV_FREE_REQ(req);
}

static value w_dns_getaddrinfo(value loop, value node, value flag_addrconfig, value flag_v4mapped, value hint_family, value cb) {
	UV_ALLOC_REQ(req, uv_getaddrinfo_t, cb);
	int hint_flags_u = 0;
	if (val_bool(flag_addrconfig))
		hint_flags_u |= AI_ADDRCONFIG;
	if (val_bool(flag_v4mapped))
		hint_flags_u |= AI_V4MAPPED;
	int hint_family_u = AF_UNSPEC;
	if (val_int(hint_family) == 4)
		hint_family_u = AF_INET;
	else if (val_int(hint_family) == 6)
		hint_family_u = AF_INET6;
	struct addrinfo hints = {
		.ai_flags = hint_flags_u,
		.ai_family = hint_family_u,
		.ai_socktype = 0,
		.ai_protocol = 0,
		.ai_addrlen = 0,
		.ai_addr = NULL,
		.ai_canonname = NULL,
		.ai_next = NULL
	};
	UV_ERROR_CHECK_C(uv_getaddrinfo(Loop_val(loop), GetAddrInfo_val(req), handle_dns_gai_cb, val_string(node), NULL, &hints), UV_FREE_REQ(GetAddrInfo_val(req)));
	return val_null;
}
BC_WRAP6(w_dns_getaddrinfo);

// ------------- TIMERS ---------------------------------------------

static void handle_timer_cb(uv_timer_t *handle) {
	value cb = UV_HANDLE_DATA_SUB(handle, timer).cb_timer;
	val_call0(cb);
}

static value w_timer_start(value loop, value timeout, value cb) {
	UV_ALLOC_CHECK(handle, uv_timer_t);
	UV_ERROR_CHECK_C(uv_timer_init(Loop_val(loop), Timer_val(handle)), free(Timer_val(handle)));
	UV_HANDLE_DATA(Timer_val(handle)) = alloc_data();
	UV_HANDLE_DATA_SUB(Timer_val(handle), timer).cb_timer = cb;
	if (UV_HANDLE_DATA(Timer_val(handle)) == NULL)
		UV_ERROR(0);
	UV_ERROR_CHECK_C(
		uv_timer_start(Timer_val(handle), handle_timer_cb, val_int(timeout), val_int(timeout)),
		{ unalloc_data(UV_HANDLE_DATA(Timer_val(handle))); free(Timer_val(handle)); }
		);
	return handle;
}
DEFINE_PRIM(w_timer_start, 3);

static value w_timer_stop(value handle, value cb) {
	UV_ERROR_CHECK_C(
		uv_timer_stop(Timer_val(handle)),
		{ unalloc_data(UV_HANDLE_DATA(Timer_val(handle))); free(Timer_val(handle)); }
		);
	((uv_w_handle_t *)UV_HANDLE_DATA(Timer_val(handle)))->cb_close = cb;
	uv_close(Handle_val(handle), handle_close_cb);
	return val_null;
}
DEFINE_PRIM(w_timer_stop, 2);

// ------------- PROCESS --------------------------------------------

static void handle_process_cb(uv_process_t *handle, int64_t exit_status, int term_signal) {
	value cb = UV_HANDLE_DATA_SUB(handle, process).cb_exit;
	// FIXME: int64 -> int conversion
	val_call2(cb, alloc_int(exit_status), alloc_int(term_signal));
}

static value w_spawn(value loop, value cb, value file, value args, value env, value cwd, value flags, value stdio, value uid, value gid) {
	UV_ALLOC_CHECK(handle, uv_process_t);
	UV_HANDLE_DATA(Process_val(handle)) = alloc_data();
	if (UV_HANDLE_DATA(Process_val(handle)) == NULL)
		UV_ERROR(0);
	UV_HANDLE_DATA_SUB(Process_val(handle), process).cb_exit = cb;
	char **args_u = malloc(sizeof(char *) * (val_array_size(args) + 1));
	for (int i = 0; i < val_array_size(args); i++)
		args_u[i] = strdup(val_string(val_array_ptr(args)[i]));
	args_u[val_array_size(args)] = NULL;
	char **env_u = malloc(sizeof(char *) * (val_array_size(env) + 1));
	for (int i = 0; i < val_array_size(env); i++)
		env_u[i] = strdup(val_string(val_array_ptr(env)[i]));
	env_u[val_array_size(env)] = NULL;
	uv_stdio_container_t *stdio_u = malloc(sizeof(uv_stdio_container_t) * val_array_size(stdio));
	for (int i = 0; i < val_array_size(stdio); i++) {
		value stdio_entry = val_array_ptr(stdio)[i];
		switch (val_int(val_field(stdio_entry, val_id("index")))) {
			case 0: // Ignore
				stdio_u[i].flags = UV_IGNORE;
				break;
			case 1: // Inherit
				stdio_u[i].flags = UV_INHERIT_FD;
				stdio_u[i].data.fd = i;
				break;
			case 2: { // Pipe
				value *args = val_array_ptr(val_field(stdio_entry, val_id("args")));
				stdio_u[i].flags = UV_CREATE_PIPE;
				if (val_bool(args[0]))
					stdio_u[i].flags |= UV_READABLE_PIPE;
				if (val_bool(args[1]))
					stdio_u[i].flags |= UV_WRITABLE_PIPE;
				stdio_u[i].data.stream = Stream_val(args[2]);
			} break;
			default: { // 3, Ipc
				value *args = val_array_ptr(val_field(stdio_entry, val_id("args")));
				stdio_u[i].flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
				stdio_u[i].data.stream = Stream_val(args[0]);
			} break;
		}
	}
	uv_process_options_t options = {
		.exit_cb = handle_process_cb,
		.file = val_string(file),
		.args = args_u,
		.env = env_u,
		.cwd = val_string(cwd),
		.flags = val_int(flags),
		.stdio_count = val_array_size(stdio),
		.stdio = stdio_u,
		.uid = val_int(uid),
		.gid = val_int(gid)
	};
	UV_ERROR_CHECK_C(
		uv_spawn(Loop_val(loop), Process_val(handle), &options),
		{ free(args_u); free(env_u); free(stdio_u); unalloc_data(UV_HANDLE_DATA(Process_val(handle))); free(Process_val(handle)); }
		);
	free(args_u);
	free(env_u);
	free(stdio_u);
	return handle;
}
BC_WRAP10(w_spawn);

static value w_process_kill(value handle, value signum) {
	UV_ERROR_CHECK(uv_process_kill(Process_val(handle), val_int(signum)));
	return val_null;
}
DEFINE_PRIM(w_process_kill, 2);

static value w_process_get_pid(value handle) {
	return alloc_int(Process_val(handle)->pid);
}
DEFINE_PRIM(w_process_get_pid, 1);

// ------------- PIPES ----------------------------------------------

static value w_pipe_init(value loop, value ipc) {
	UV_ALLOC_CHECK(handle, uv_pipe_t);
	UV_ERROR_CHECK_C(uv_pipe_init(Loop_val(loop), Pipe_val(handle), val_bool(ipc)), free(Pipe_val(handle)));
	UV_HANDLE_DATA(Pipe_val(handle)) = alloc_data();
	if (UV_HANDLE_DATA(Pipe_val(handle)) == NULL)
		UV_ERROR(0);
	return handle;
}
DEFINE_PRIM(w_pipe_init, 2);

static value w_pipe_open(value pipe, value fd) {
	UV_ERROR_CHECK(uv_pipe_open(Pipe_val(pipe), val_int(fd)));
	return val_null;
}
DEFINE_PRIM(w_pipe_open, 2);

static value w_pipe_accept(value loop, value server) {
	UV_ALLOC_CHECK(client, uv_pipe_t);
	UV_ERROR_CHECK_C(uv_pipe_init(Loop_val(loop), Pipe_val(client), 0), free(Pipe_val(client)));
	UV_HANDLE_DATA(Pipe_val(client)) = alloc_data();
	if (UV_HANDLE_DATA(Pipe_val(client)) == NULL)
		UV_ERROR(0);
	UV_ERROR_CHECK_C(uv_accept(Stream_val(server), Stream_val(client)), free(Pipe_val(client)));
	return client;
}
DEFINE_PRIM(w_pipe_accept, 2);

static value w_pipe_bind_ipc(value handle, value path) {
	UV_ERROR_CHECK(uv_pipe_bind(Pipe_val(handle), val_string(path)));
	return val_null;
}
DEFINE_PRIM(w_pipe_bind_ipc, 2);

static value w_pipe_connect_ipc(value handle, value path, value cb) {
	UV_ALLOC_REQ(req, uv_connect_t, cb);
	uv_pipe_connect(Connect_val(req), Pipe_val(handle), val_string(path), (void (*)(uv_connect_t *, int))handle_stream_cb);
	return val_null;
}
DEFINE_PRIM(w_pipe_connect_ipc, 3);

static value w_pipe_pending_count(value handle) {
	return alloc_int(uv_pipe_pending_count(Pipe_val(handle)));
}
DEFINE_PRIM(w_pipe_pending_count, 1);

static value w_pipe_accept_pending(value loop, value handle) {
	switch (uv_pipe_pending_type(Pipe_val(handle))) {
		case UV_NAMED_PIPE: {
			UV_ALLOC_CHECK(client, uv_pipe_t);
			UV_ERROR_CHECK_C(uv_pipe_init(Loop_val(loop), Pipe_val(client), 0), free(Pipe_val(client)));
			UV_HANDLE_DATA(Pipe_val(client)) = alloc_data();
			if (UV_HANDLE_DATA(Pipe_val(client)) == NULL)
				UV_ERROR(0);
			UV_ERROR_CHECK_C(uv_accept(Stream_val(handle), Stream_val(client)), free(Pipe_val(client)));
			return val_call1(construct_pipe_accept_pipe, client);
		} break;
		case UV_TCP: {
			UV_ALLOC_CHECK(client, uv_tcp_t);
			UV_ERROR_CHECK_C(uv_tcp_init(Loop_val(loop), Tcp_val(client)), free(Tcp_val(client)));
			UV_HANDLE_DATA(Tcp_val(client)) = alloc_data();
			if (UV_HANDLE_DATA(Tcp_val(client)) == NULL)
				UV_ERROR(0);
			UV_ERROR_CHECK_C(uv_accept(Stream_val(handle), Stream_val(client)), free(Tcp_val(client)));
			return val_call1(construct_pipe_accept_socket, client);
		} break;
		default:
			UV_ERROR(0);
			return val_null;
			break;
	}
}
DEFINE_PRIM(w_pipe_accept_pending, 2);

static value w_pipe_getsockname(value handle) {
	char path[256];
	size_t path_size = 255;
	UV_ERROR_CHECK(uv_pipe_getsockname(Pipe_val(handle), path, &path_size));
	path[path_size] = 0;
	return copy_string(path, path_size);
}
DEFINE_PRIM(w_pipe_getsockname, 1);

static value w_pipe_getpeername(value handle) {
	char path[256];
	size_t path_size = 255;
	UV_ERROR_CHECK(uv_pipe_getpeername(Pipe_val(handle), path, &path_size));
	path[path_size] = 0;
	return copy_string(path, path_size);
}
DEFINE_PRIM(w_pipe_getpeername, 1);

static value w_pipe_write_handle(value handle, value data, value send_handle, value cb) {
	UV_ALLOC_REQ(req, uv_write_t, cb);
	uv_buf_t buf = uv_buf_init(val_string(data), val_strlen(data));
	UV_ERROR_CHECK_C(uv_write2(Write_val(req), Stream_val(handle), &buf, 1, Stream_val(send_handle), (void (*)(uv_write_t *, int))handle_stream_cb), UV_FREE_REQ(Write_val(req)));
	return val_null;
}
DEFINE_PRIM(w_pipe_write_handle, 4);

// ------------- CASTS ----------------------------------------------

static value w_stream_handle(value handle) {
	return handle;
}
DEFINE_PRIM(w_stream_handle, 1);

static value w_fs_event_handle(value handle) {
	return handle;
}
DEFINE_PRIM(w_fs_event_handle, 1);

static value w_timer_handle(value handle) {
	return handle;
}
DEFINE_PRIM(w_timer_handle, 1);

static value w_process_handle(value handle) {
	return handle;
}
DEFINE_PRIM(w_process_handle, 1);

static value w_tcp_stream(value handle) {
	return handle;
}
DEFINE_PRIM(w_tcp_stream, 1);

static value w_udp_stream(value handle) {
	return handle;
}
DEFINE_PRIM(w_udp_stream, 1);

static value w_pipe_stream(value handle) {
	return handle;
}
DEFINE_PRIM(w_pipe_stream, 1);
