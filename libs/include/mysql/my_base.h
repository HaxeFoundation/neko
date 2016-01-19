/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* This file includes constants used with all databases */
/* Author: Michael Widenius */

#ifndef _my_base_h
#define _my_base_h

#ifndef stdin				/* Included first in handler */
#define USES_TYPES			/* my_dir with sys/types is included */
#define CHSIZE_USED
#include <global.h>
#include <my_dir.h>			/* This includes types */
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>
#ifdef MSDOS
#include <share.h>			/* Neaded for sopen() */
#endif
#if !defined(USE_MY_FUNC) && !defined(THREAD)
#include <my_nosys.h>			/* For faster code, after test */
#endif	/* USE_MY_FUNC */
#endif	/* stdin */
#include <my_list.h>

/* The following is bits in the flag parameter to ha_open() */

#define HA_OPEN_ABORT_IF_LOCKED		0	/* default */
#define HA_OPEN_WAIT_IF_LOCKED		1
#define HA_OPEN_IGNORE_IF_LOCKED	2
#define HA_OPEN_TMP_TABLE		4	/* Table is a temp table */
#define HA_OPEN_DELAY_KEY_WRITE		8	/* Don't update index  */
#define HA_OPEN_ABORT_IF_CRASHED	16
#define HA_OPEN_FOR_REPAIR		32	/* open even if crashed */

	/* The following is parameter to ha_rkey() how to use key */

enum ha_rkey_function {
  HA_READ_KEY_EXACT,			/* Find first record else error */
  HA_READ_KEY_OR_NEXT,			/* Record or next record */
  HA_READ_KEY_OR_PREV,			/* Record or previous */
  HA_READ_AFTER_KEY,			/* Find next rec. after key-record */
  HA_READ_BEFORE_KEY,			/* Find next rec. before key-record */
  HA_READ_PREFIX,			/* Key which as same prefix */
  HA_READ_PREFIX_LAST			/* Last key with the same prefix */			
};

	/* The following is parameter to ha_extra() */

enum ha_extra_function {
  HA_EXTRA_NORMAL=0,			/* Optimize for space (def) */
  HA_EXTRA_QUICK=1,			/* Optimize for speed */
  HA_EXTRA_RESET=2,			/* Reset database to after open */
  HA_EXTRA_CACHE=3,			/* Cash record in HA_rrnd() */
  HA_EXTRA_NO_CACHE=4,			/* End cacheing of records (def) */
  HA_EXTRA_NO_READCHECK=5,		/* No readcheck on update */
  HA_EXTRA_READCHECK=6,			/* Use readcheck (def) */
  HA_EXTRA_KEYREAD=7,			/* Read only key to database */
  HA_EXTRA_NO_KEYREAD=8,		/* Normal read of records (def) */
  HA_EXTRA_NO_USER_CHANGE=9,		/* No user is allowed to write */
  HA_EXTRA_KEY_CACHE=10,
  HA_EXTRA_NO_KEY_CACHE=11,
  HA_EXTRA_WAIT_LOCK=12,		/* Wait until file is avalably (def) */
  HA_EXTRA_NO_WAIT_LOCK=13,		/* If file is locked, return quickly */
  HA_EXTRA_WRITE_CACHE=14,		/* Use write cache in ha_write() */
  HA_EXTRA_FLUSH_CACHE=15,		/* flush write_record_cache */
  HA_EXTRA_NO_KEYS=16,			/* Remove all update of keys */
  HA_EXTRA_KEYREAD_CHANGE_POS=17,	/* Keyread, but change pos */
					/* xxxxchk -r must be used */
  HA_EXTRA_REMEMBER_POS=18,		/* Remember pos for next/prev */
  HA_EXTRA_RESTORE_POS=19,
  HA_EXTRA_REINIT_CACHE=20,		/* init cache from current record */
  HA_EXTRA_FORCE_REOPEN=21,		/* Datafile have changed on disk */
  HA_EXTRA_FLUSH,			/* Flush tables to disk */
  HA_EXTRA_NO_ROWS,			/* Don't write rows */
  HA_EXTRA_RESET_STATE,			/* Reset positions */
  HA_EXTRA_IGNORE_DUP_KEY,		/* Dup keys don't rollback everything*/
  HA_EXTRA_NO_IGNORE_DUP_KEY,
  HA_EXTRA_DONT_USE_CURSOR_TO_UPDATE	/* Cursor will not be used for update */
};

	/* The following is parameter to ha_panic() */

enum ha_panic_function {
  HA_PANIC_CLOSE,			/* Close all databases */
  HA_PANIC_WRITE,			/* Unlock and write status */
  HA_PANIC_READ				/* Lock and read keyinfo */
};

	/* The following is parameter to ha_create(); keytypes */

enum ha_base_keytype {
  HA_KEYTYPE_END=0,
  HA_KEYTYPE_TEXT=1,			/* Key is sorted as letters */
  HA_KEYTYPE_BINARY=2,			/* Key is sorted as unsigned chars */
  HA_KEYTYPE_SHORT_INT=3,
  HA_KEYTYPE_LONG_INT=4,
  HA_KEYTYPE_FLOAT=5,
  HA_KEYTYPE_DOUBLE=6,
  HA_KEYTYPE_NUM=7,			/* Not packed num with pre-space */
  HA_KEYTYPE_USHORT_INT=8,
  HA_KEYTYPE_ULONG_INT=9,
  HA_KEYTYPE_LONGLONG=10,
  HA_KEYTYPE_ULONGLONG=11,
  HA_KEYTYPE_INT24=12,
  HA_KEYTYPE_UINT24=13,
  HA_KEYTYPE_INT8=14,
  HA_KEYTYPE_VARTEXT=15,		/* Key is sorted as letters */
  HA_KEYTYPE_VARBINARY=16		/* Key is sorted as unsigned chars */
};

#define HA_MAX_KEYTYPE	31		/* Must be log2-1 */

	/* These flags kan be OR:ed to key-flag */

#define HA_NOSAME		 1	/* Set if not dupplicated records */
#define HA_PACK_KEY		 2	/* Pack string key to previous key */
#define HA_AUTO_KEY		 16
#define HA_BINARY_PACK_KEY	 32	/* Packing of all keys to prev key */
#define HA_FULLTEXT		128     /* SerG: for full-text search */
#define HA_UNIQUE_CHECK		256	/* Check the key for uniqueness */

	/* Automatic bits in key-flag */

#define HA_SPACE_PACK_USED	 4	/* Test for if SPACE_PACK used */
#define HA_VAR_LENGTH_KEY	 8
#define HA_NULL_PART_KEY	 64
#ifndef ISAM_LIBRARY
#define HA_SORT_ALLOWS_SAME      512    /* Intern bit when sorting records */
#else
/* poor old NISAM has 8-bit flags :-( */
#define HA_SORT_ALLOWS_SAME	 128	/* Intern bit when sorting records */
#endif

	/* These flags can be order to key-seg-flag */

#define HA_SPACE_PACK		 1	/* Pack space in key-seg */
#define HA_PART_KEY		 4	/* Used by MySQL for part-key-cols */
#define HA_VAR_LENGTH		 8
#define HA_NULL_PART		 16
#define HA_BLOB_PART		 32
#define HA_SWAP_KEY		 64
#define HA_REVERSE_SORT		 128	/* Sort key in reverse order */

	/* optionbits for database */
#define HA_OPTION_PACK_RECORD		1
#define HA_OPTION_PACK_KEYS		2
#define HA_OPTION_COMPRESS_RECORD	4
#define HA_OPTION_LONG_BLOB_PTR		8 /* new ISAM format */
#define HA_OPTION_TMP_TABLE		16
#define HA_OPTION_CHECKSUM		32
#define HA_OPTION_DELAY_KEY_WRITE	64
#define HA_OPTION_NO_PACK_KEYS		128  /* Reserved for MySQL */
#define HA_OPTION_TEMP_COMPRESS_RECORD	((uint) 16384)	/* set by isamchk */
#define HA_OPTION_READ_ONLY_DATA	((uint) 32768)	/* Set by isamchk */

	/* Bits in flag to create() */

#define HA_DONT_TOUCH_DATA	1	/* Don't empty datafile (isamchk) */
#define HA_PACK_RECORD		2	/* Request packed record format */
#define HA_CREATE_TMP_TABLE	4
#define HA_CREATE_CHECKSUM	8
#define HA_CREATE_DELAY_KEY_WRITE 64

	/* Bits in flag to _status */

#define HA_STATUS_POS		1		/* Return position */
#define HA_STATUS_NO_LOCK 	2		/* Don't use external lock */
#define HA_STATUS_TIME		4		/* Return update time */
#define HA_STATUS_CONST		8		/* Return constants values */
#define HA_STATUS_VARIABLE	16
#define HA_STATUS_ERRKEY	32
#define HA_STATUS_AUTO		64

	/* Errorcodes given by functions */

#define HA_ERR_KEY_NOT_FOUND	120	/* Didn't find key on read or update */
#define HA_ERR_FOUND_DUPP_KEY	121	/* Dupplicate key on write */
#define HA_ERR_RECORD_CHANGED	123	/* Uppdate with is recoverable */
#define HA_ERR_WRONG_INDEX	124	/* Wrong index given to function */
#define HA_ERR_CRASHED		126	/* Indexfile is crashed */
#define HA_ERR_WRONG_IN_RECORD	127	/* Record-file is crashed */
#define HA_ERR_OUT_OF_MEM	128	/* Record-file is crashed */
#define HA_ERR_WRONG_COMMAND	131	/* Command not supported */
#define HA_ERR_OLD_FILE		132	/* old databasfile */
#define HA_ERR_NO_ACTIVE_RECORD 133	/* No record read in update() */
#define HA_ERR_RECORD_DELETED	134	/* Intern error-code */
#define HA_ERR_RECORD_FILE_FULL 135	/* No more room in file */
#define HA_ERR_INDEX_FILE_FULL	136	/* No more room in file */
#define HA_ERR_END_OF_FILE	137	/* end in next/prev/first/last */
#define HA_ERR_UNSUPPORTED	138	/* unsupported extension used */
#define HA_ERR_TO_BIG_ROW	139	/* Too big row */
#define HA_WRONG_CREATE_OPTION	140	/* Wrong create option */
#define HA_ERR_FOUND_DUPP_UNIQUE 141	/* Dupplicate unique on write */
#define HA_ERR_UNKNOWN_CHARSET	 142	/* Can't open charset */
#define HA_ERR_WRONG_TABLE_DEF	 143
#define HA_ERR_CRASHED_ON_REPAIR 144	/* Last (automatic?) repair failed */
#define HA_ERR_CRASHED_ON_USAGE  145	/* Table must be repaired */
#define HA_ERR_LOCK_WAIT_TIMEOUT 146    
#define HA_ERR_LOCK_TABLE_FULL   147
#define HA_ERR_READ_ONLY_TRANSACTION 148 /* Updates not allowed */
#define HA_ERR_LOCK_DEADLOCK	 149
#define HA_ERR_CANNOT_ADD_FOREIGN 150    /* Cannot add a foreign key constr. */
#define HA_ERR_NO_REFERENCED_ROW 151     /* Cannot add a child row */
#define HA_ERR_ROW_IS_REFERENCED 152     /* Cannot delete a parent row */

	/* Other constants */

#define HA_NAMELEN 64			/* Max length of saved filename */

	/* Intern constants in databases */

	/* bits in _search */
#define SEARCH_FIND	1
#define SEARCH_NO_FIND	2
#define SEARCH_SAME	4
#define SEARCH_BIGGER	8
#define SEARCH_SMALLER	16
#define SEARCH_SAVE_BUFF	32
#define SEARCH_UPDATE	64
#define SEARCH_PREFIX	128
#define SEARCH_LAST	256

	/* bits in opt_flag */
#define QUICK_USED	1
#define READ_CACHE_USED	2
#define READ_CHECK_USED 4
#define KEY_READ_USED	8
#define WRITE_CACHE_USED 16
#define OPT_NO_ROWS 	32

	/* bits in update */
#define HA_STATE_CHANGED	1	/* Database has changed */
#define HA_STATE_AKTIV		2	/* Has a current record */
#define HA_STATE_WRITTEN	4	/* Record is written */
#define HA_STATE_DELETED	8
#define HA_STATE_NEXT_FOUND	16	/* Next found record (record before) */
#define HA_STATE_PREV_FOUND	32	/* Prev found record (record after) */
#define HA_STATE_NO_KEY		64	/* Last read didn't find record */
#define HA_STATE_KEY_CHANGED	128
#define HA_STATE_WRITE_AT_END	256	/* set in _ps_find_writepos */
#define HA_STATE_BUFF_SAVED	512	/* If current keybuff is info->buff */
#define HA_STATE_ROW_CHANGED	1024	/* To invalide ROW cache */
#define HA_STATE_EXTEND_BLOCK	2048

enum en_fieldtype {
  FIELD_LAST=-1,FIELD_NORMAL,FIELD_SKIPP_ENDSPACE,FIELD_SKIPP_PRESPACE,
  FIELD_SKIPP_ZERO,FIELD_BLOB,FIELD_CONSTANT,FIELD_INTERVALL,FIELD_ZERO,
  FIELD_VARCHAR,FIELD_CHECK
};

enum data_file_type {
  STATIC_RECORD,DYNAMIC_RECORD,COMPRESSED_RECORD
};

/* For number of records */
#ifdef BIG_TABLES
typedef my_off_t	ha_rows;
#else
typedef ulong		ha_rows;	
#endif

#define HA_POS_ERROR	(~ (ha_rows) 0)
#define HA_OFFSET_ERROR	(~ (my_off_t) 0)

#if SYSTEM_SIZEOF_OFF_T == 4
#define MAX_FILE_SIZE	INT_MAX32
#else
#define MAX_FILE_SIZE	LONGLONG_MAX
#endif

#endif /* _my_base_h */
