#ifndef WIN_PATHS_H
#define WIN_PATHS_H

#include <malloc.h>

#define CONVERT_TO_WPATH(str, wpath) \
	WCHAR wpath[MAX_PATH]; \
	{ \
		int result = MultiByteToWideChar(CP_UTF8, 0, val_string(str), val_strlen(str) + 1, wpath, MAX_PATH); \
		if (result == 0) \
			neko_error(); \
	}

#define CONVERT_TO_WSTR(str, wstr) \
	WCHAR* wstr; \
	{ \
		int wsize = MultiByteToWideChar(CP_UTF8, 0, val_string(str), val_strlen(str) + 1, NULL, 0); \
		if (wsize == 0) \
			neko_error(); \
		wstr = alloca(sizeof(WCHAR) * wsize); \
		int result = MultiByteToWideChar(CP_UTF8, 0, val_string(str), val_strlen(str) + 1, wstr, wsize); \
		if (result == 0) \
			neko_error(); \
	}

#endif
