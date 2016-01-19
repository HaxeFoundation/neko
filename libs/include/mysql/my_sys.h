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

#ifndef _my_sys_h
#define _my_sys_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifdef HAVE_AIOWAIT
#include <sys/asynch.h>			/* Used by record-cache */
typedef struct my_aio_result {
  aio_result_t result;
  int	       pending;
} my_aio_result;
#endif

#ifndef THREAD
extern int NEAR my_errno;		/* Last error in mysys */
#else
#include <my_pthread.h>
#endif

#ifndef _m_ctype_h
#include <m_ctype.h>                    /* for CHARSET_INFO */
#endif

#include <stdarg.h>  

#define MYSYS_PROGRAM_USES_CURSES()  { error_handler_hook = my_message_curses;	mysys_uses_curses=1; }
#define MYSYS_PROGRAM_DONT_USE_CURSES()  { error_handler_hook = my_message_no_curses; mysys_uses_curses=0;}
#define MY_INIT(name);		{ my_progname= name; my_init(); }

#define MAXMAPS		(4)	/* Number of error message maps */
#define ERRMOD		(1000)	/* Max number of errors in a map */
#define ERRMSGSIZE	(SC_MAXWIDTH)	/* Max length of a error message */
#define NRERRBUFFS	(2)	/* Buffers for parameters */
#define MY_FILE_ERROR	((uint) ~0)

	/* General bitmaps for my_func's */
#define MY_FFNF		1	/* Fatal if file not found */
#define MY_FNABP	2	/* Fatal if not all bytes read/writen */
#define MY_NABP		4	/* Error if not all bytes read/writen */
#define MY_FAE		8	/* Fatal if any error */
#define MY_WME		16	/* Write message on error */
#define MY_WAIT_IF_FULL 32	/* Wait and try again if disk full error */
#define MY_RAID         64      /* Support for RAID (not the "Johnson&Johnson"-s one ;) */
#define MY_DONT_CHECK_FILESIZE 128	/* Option to init_io_cache() */
#define MY_LINK_WARNING 32	/* my_redel() gives warning if links */
#define MY_COPYTIME	64	/* my_redel() copys time */
#define MY_DELETE_OLD	256	/* my_create_with_symlink() */
#define MY_RESOLVE_LINK 128	/* my_realpath(); Only resolve links */
#define MY_HOLD_ORIGINAL_MODES 128  /* my_copy() holds to file modes */
#define MY_REDEL_MAKE_BACKUP 256
#define MY_SEEK_NOT_DONE 32	/* my_lock may have to do a seek */
#define MY_DONT_WAIT	64	/* my_lock() don't wait if can't lock */
#define MY_ZEROFILL	32	/* my_malloc(), fill array with zero */
#define MY_ALLOW_ZERO_PTR 64	/* my_realloc() ; zero ptr -> malloc */
#define MY_FREE_ON_ERROR 128	/* my_realloc() ; Free old ptr on error */
#define MY_HOLD_ON_ERROR 256	/* my_realloc() ; Return old ptr on error */
#define MY_THREADSAFE	128	/* pread/pwrite:  Don't allow interrupts */
#define MY_DONT_OVERWRITE_FILE 1024	/* my_copy; Don't overwrite file */

#define MY_CHECK_ERROR	1	/* Params to my_end; Check open-close */
#define MY_GIVE_INFO	2	/* Give time info about process*/

#define ME_HIGHBYTE	8	/* Shift for colours */
#define ME_NOCUR	1	/* Don't use curses message */
#define ME_OLDWIN	2	/* Use old window */
#define ME_BELL		4	/* Ring bell then printing message */
#define ME_HOLDTANG	8	/* Don't delete last keys */
#define ME_WAITTOT	16	/* Wait for errtime secs of for a action */
#define ME_WAITTANG	32	/* Wait for a user action  */
#define ME_NOREFRESH	64	/* Dont refresh screen */
#define ME_NOINPUT	128	/* Dont use the input libary */
#define ME_COLOUR1	((1 << ME_HIGHBYTE))	/* Possibly error-colours */
#define ME_COLOUR2	((2 << ME_HIGHBYTE))
#define ME_COLOUR3	((3 << ME_HIGHBYTE))

	/* My seek flags */
#define MY_SEEK_SET	0
#define MY_SEEK_CUR	1
#define MY_SEEK_END	2

        /* My charsets_list flags */
#define MY_NO_SETS       0
#define MY_COMPILED_SETS 1      /* show compiled-in sets */
#define MY_CONFIG_SETS   2      /* sets that have a *.conf file */
#define MY_INDEX_SETS    4      /* all sets listed in the Index file */
#define MY_LOADED_SETS    8      /* the sets that are currently loaded */

	/* Some constants */
#define MY_WAIT_FOR_USER_TO_FIX_PANIC	60	/* in seconds */
#define MY_WAIT_GIVE_USER_A_MESSAGE	10	/* Every 10 times of prev */
#define MIN_COMPRESS_LENGTH		50	/* Don't compress small bl. */
#define KEYCACHE_BLOCK_SIZE		1024

	/* root_alloc flags */
#define MY_KEEP_PREALLOC	1

	/* defines when allocating data */

#define my_checkmalloc() (0)
#undef TERMINATE
#define TERMINATE(A) {}
#define QUICK_SAFEMALLOC
#define NORMAL_SAFEMALLOC
extern gptr my_malloc(size_t Size,myf MyFlags);
#define my_malloc_ci(SZ,FLAG) my_malloc( SZ, FLAG )
extern gptr my_realloc(gptr oldpoint, size_t Size,myf MyFlags);
extern void my_no_flags_free(void *ptr);
extern gptr my_memdup(const unsigned char *from, size_t length,myf MyFlags);
extern my_string my_strdup(const char *from,myf MyFlags);
extern my_string my_strndup(const char *from, size_t length, myf MyFlags);
#define my_free(PTR) my_no_flags_free(PTR)
#define CALLER_INFO_PROTO   /* nothing */
#define CALLER_INFO         /* nothing */
#define ORIG_CALLER_INFO    /* nothing */

#ifdef HAVE_ALLOCA
#if defined(_AIX) && !defined(__GNUC__)
#pragma alloca
#endif /* _AIX */
#if defined(__GNUC__) && !defined(HAVE_ALLOCA_H)
#ifndef alloca
#define alloca __builtin_alloca
#endif
#endif /* GNUC */
#define my_alloca(SZ) alloca((size_t) (SZ))
#define my_afree(PTR) {}
#else
#define my_alloca(SZ) my_malloc(SZ,MYF(0))
#define my_afree(PTR) my_free(PTR)
#endif /* HAVE_ALLOCA */

#ifdef MSDOS
#ifdef __ZTC__
void * __CDECL halloc(long count,size_t length);
void   __CDECL hfree(void *ptr);
#endif
#if defined(USE_HALLOC)
#if defined(_VCM_) || defined(M_IC80386)
#undef USE_HALLOC
#endif
#endif
#ifdef USE_HALLOC
#define malloc(a) halloc((long) (a),1)
#define free(a) hfree(a)
#endif
#endif /* MSDOS */

#ifndef errno
#ifdef HAVE_ERRNO_AS_DEFINE
#include <errno.h>			/* errno is a define */
#else
extern int errno;			/* declare errno */
#endif
#endif
extern const char ** NEAR my_errmsg[];
extern char NEAR errbuff[NRERRBUFFS][ERRMSGSIZE];
extern char *home_dir;			/* Home directory for user */
extern char *my_progname;		/* program-name (printed in errors) */
extern char NEAR curr_dir[];		/* Current directory for user */
extern int (*error_handler_hook)(uint my_err, const char *str,myf MyFlags);
extern int (*fatal_error_handler_hook)(uint my_err, const char *str,
				       myf MyFlags);

/* charsets */
extern uint get_charset_number(const char *cs_name);
extern const char *get_charset_name(uint cs_number);
extern CHARSET_INFO *get_charset(uint cs_number, myf flags);
extern my_bool set_default_charset(uint cs, myf flags);
extern CHARSET_INFO *get_charset_by_name(const char *cs_name);
extern CHARSET_INFO *get_charset_by_nr(uint cs_number);
extern my_bool set_default_charset_by_name(const char *cs_name, myf flags);
extern void free_charsets(void);
extern char *list_charsets(myf want_flags); /* my_free() this string... */
extern char *get_charsets_dir(char *buf);


/* statistics */
extern ulong	_my_cache_w_requests,_my_cache_write,_my_cache_r_requests,
		_my_cache_read;
extern ulong	 _my_blocks_used,_my_blocks_changed;
extern ulong	my_file_opened,my_stream_opened, my_tmp_file_created;
extern my_bool	key_cache_inited;

					/* Point to current my_message() */
extern void (*my_sigtstp_cleanup)(void),
					/* Executed before jump to shell */
	    (*my_sigtstp_restart)(void),
	    (*my_abort_hook)(int);
					/* Executed when comming from shell */
extern int NEAR my_umask,		/* Default creation mask  */
	   NEAR my_umask_dir,
	   NEAR my_recived_signals,	/* Signals we have got */
	   NEAR my_safe_to_handle_signal, /* Set when allowed to SIGTSTP */
	   NEAR my_dont_interrupt;	/* call remember_intr when set */
extern my_bool NEAR mysys_uses_curses, my_use_symdir;
extern size_t lCurMemory,lMaxMemory;	/* from safemalloc */

extern ulong	my_default_record_cache_size;
extern my_bool NEAR my_disable_locking,NEAR my_disable_async_io,
               NEAR my_disable_flush_key_blocks, NEAR my_disable_symlinks;
extern char	wild_many,wild_one,wild_prefix;
extern const char *charsets_dir;
extern char *defaults_extra_file;

typedef struct wild_file_pack	/* Struct to hold info when selecting files */
{
  uint		wilds;		/* How many wildcards */
  uint		not_pos;	/* Start of not-theese-files */
  my_string	*wild;		/* Pointer to wildcards */
} WF_PACK;

struct my_rnd_struct {
  unsigned long seed1,seed2,max_value;
  double max_value_dbl;
};

typedef struct st_typelib {	/* Different types saved here */
  uint count;			/* How many types */
  const char *name;			/* Name of typelib */
  const char **type_names;
} TYPELIB;

enum cache_type {READ_CACHE,WRITE_CACHE,READ_FIFO,READ_NET,WRITE_NET};
enum flush_type { FLUSH_KEEP, FLUSH_RELEASE, FLUSH_IGNORE_CHANGED,
		  FLUSH_FORCE_WRITE};

typedef struct st_record_cache	/* Used when cacheing records */
{
  File file;
  int	rc_seek,error,inited;
  uint	rc_length,read_length,reclength;
  my_off_t rc_record_pos,end_of_file;
  unsigned char	*rc_buff,*rc_buff2,*rc_pos,*rc_end,*rc_request_pos;
#ifdef HAVE_AIOWAIT
  int	use_async_io;
  my_aio_result aio_result;
#endif
  enum cache_type type;
} RECORD_CACHE;

enum file_type { UNOPEN = 0, FILE_BY_OPEN, FILE_BY_CREATE,
		   STREAM_BY_FOPEN, STREAM_BY_FDOPEN, FILE_BY_MKSTEMP };

extern struct my_file_info
{
  my_string		name;
  enum file_type	type;
#if defined(THREAD) && !defined(HAVE_PREAD)  
  pthread_mutex_t	mutex;
#endif
} my_file_info[MY_NFILE];


typedef struct st_dynamic_array {
  char *buffer;
  uint elements,max_element;
  uint alloc_increment;
  uint size_of_element;
} DYNAMIC_ARRAY;

typedef struct st_dynamic_string {
  char *str;
  size_t length,max_length,alloc_increment;
} DYNAMIC_STRING;


typedef struct st_io_cache		/* Used when cacheing files */
{
  my_off_t pos_in_file,end_of_file;
  unsigned char	*rc_pos,*rc_end,*buffer,*rc_request_pos;
  int (*read_function)(struct st_io_cache *,unsigned char *,uint);
  char *file_name;			/* if used with 'open_cached_file' */
  char *dir,*prefix;
  File file;
  int	seek_not_done,error;
  uint	buffer_length,read_length;
  myf	myflags;			/* Flags used to my_read/my_write */
  enum cache_type type;
#ifdef HAVE_AIOWAIT
  uint inited;
  my_off_t aio_read_pos;
  my_aio_result aio_result;
#endif
} IO_CACHE;

typedef int (*qsort2_cmp)(const void *, const void *, const void *);

	/* defines for mf_iocache */

	/* Test if buffer is inited */
#define my_b_clear(info) (info)->buffer=0
#define my_b_inited(info) (info)->buffer
#define my_b_EOF INT_MIN

#define my_b_read(info,Buffer,Count) \
  ((info)->rc_pos + (Count) <= (info)->rc_end ?\
   (memcpy(Buffer,(info)->rc_pos,(size_t) (Count)), \
    ((info)->rc_pos+=(Count)),0) :\
   (*(info)->read_function)((info),Buffer,Count))

#define my_b_get(info) \
  ((info)->rc_pos != (info)->rc_end ?\
   ((info)->rc_pos++, (int) (uchar) (info)->rc_pos[-1]) :\
   _my_b_get(info))

#define my_b_write(info,Buffer,Count) \
  ((info)->rc_pos + (Count) <= (info)->rc_end ?\
   (memcpy((info)->rc_pos,Buffer,(size_t) (Count)), \
    ((info)->rc_pos+=(Count)),0) :\
   _my_b_write(info,Buffer,Count))

	/* my_b_write_byte dosn't have any err-check */
#define my_b_write_byte(info,chr) \
  (((info)->rc_pos < (info)->rc_end) ?\
   ((*(info)->rc_pos++)=(chr)) :\
   (_my_b_write(info,0,0) , ((*(info)->rc_pos++)=(chr))))

#define my_b_fill_cache(info) \
  (((info)->rc_end=(info)->rc_pos),(*(info)->read_function)(info,0,0))

#define my_b_tell(info) ((info)->pos_in_file + \
			 ((info)->rc_pos - (info)->rc_request_pos))

#define my_b_bytes_in_cache(info) ((uint) ((info)->rc_end - (info)->rc_pos))

typedef struct st_changeable_var {
  const char *name;			/* Name of variable */
  long *varptr;				/* Pointer to variable */
  long def_value,			/* Default value */
       min_value,			/* Min allowed value */
       max_value,			/* Max allowed value */
       sub_size,			/* Subtract this from given value */
       block_size;			/* Value should be a mult. of this */
} CHANGEABLE_VAR;


/* structs for alloc_root */

#ifndef ST_USED_MEM_DEFINED
#define ST_USED_MEM_DEFINED
typedef struct st_used_mem {   /* struct for once_alloc */
  struct st_used_mem *next;    /* Next block in use */
  size_t left;                 /* memory left in block  */
  size_t size;                 /* Size of block */
} USED_MEM;

typedef struct st_mem_root {
  USED_MEM *free;
  USED_MEM *used;
  USED_MEM *pre_alloc;
  size_t min_malloc;
  size_t block_size;
  unsigned int block_num;
  unsigned int first_block_usage;
  void (*error_handler)(void);
} MEM_ROOT;
#endif

	/* Prototypes for mysys and my_func functions */

extern int my_copy(const char *from,const char *to,myf MyFlags);
extern int my_append(const char *from,const char *to,myf MyFlags);
extern int my_delete(const char *name,myf MyFlags);
extern int my_getwd(my_string buf,uint size,myf MyFlags);
extern int my_setwd(const char *dir,myf MyFlags);
extern int my_lock(File fd,int op,my_off_t start, my_off_t length,myf MyFlags);
extern gptr my_once_alloc(uint Size,myf MyFlags);
extern void my_once_free(void);
extern my_string my_tempnam(const char *dir,const char *pfx,myf MyFlags);
extern File my_open(const char *FileName,int Flags,myf MyFlags);
extern File my_register_filename(File fd, const char *FileName,
				 enum file_type type_of_file,
				 uint error_message_number, myf MyFlags);
extern File my_create(const char *FileName,int CreateFlags,
		      int AccsesFlags, myf MyFlags);
extern int my_close(File Filedes,myf MyFlags);
extern int my_mkdir(const char *dir, int Flags, myf MyFlags);
extern int my_readlink(char *to, const char *filename, myf MyFlags);
extern int my_realpath(char *to, const char *filename, myf MyFlags);
extern File my_create_with_symlink(const char *linkname, const char *filename,
				   int createflags, int access_flags,
				   myf MyFlags);
extern int my_delete_with_symlink(const char *name, myf MyFlags);
extern int my_rename_with_symlink(const char *from,const char *to,myf MyFlags);
extern int my_symlink(const char *content, const char *linkname, myf MyFlags);
extern uint my_read(File Filedes,unsigned char *Buffer,uint Count,myf MyFlags);
extern uint my_pread(File Filedes,unsigned char *Buffer,uint Count,my_off_t offset,
		     myf MyFlags);
extern int my_rename(const char *from,const char *to,myf MyFlags);
extern my_off_t my_seek(File fd,my_off_t pos,int whence,myf MyFlags);
extern my_off_t my_tell(File fd,myf MyFlags);
extern uint my_write(File Filedes,const unsigned char *Buffer,uint Count,
		     myf MyFlags);
extern uint my_pwrite(File Filedes,const unsigned char *Buffer,uint Count,
		      my_off_t offset,myf MyFlags);
extern uint my_fread(FILE *stream,unsigned char *Buffer,uint Count,myf MyFlags);
extern uint my_fwrite(FILE *stream,const unsigned char *Buffer,uint Count,
		      myf MyFlags);
extern my_off_t my_fseek(FILE *stream,my_off_t pos,int whence,myf MyFlags);
extern my_off_t my_ftell(FILE *stream,myf MyFlags);
extern gptr _mymalloc(size_t uSize,const char *sFile,
		      uint uLine, myf MyFlag);
extern gptr _myrealloc(gptr pPtr,size_t uSize,const char *sFile,
		       uint uLine, myf MyFlag);
extern gptr my_multi_malloc _VARARGS((myf MyFlags, ...));
extern void _myfree(gptr pPtr,const char *sFile,uint uLine, myf MyFlag);
extern int _sanity(const char *sFile,unsigned int uLine);
extern gptr _my_memdup(const unsigned char *from, size_t length,
		       const char *sFile, uint uLine,myf MyFlag);
extern my_string _my_strdup(const char *from, const char *sFile, uint uLine,
			    myf MyFlag);
#ifndef TERMINATE
extern void TERMINATE(FILE *file);
#endif
extern void init_glob_errs(void);
extern FILE *my_fopen(const char *FileName,int Flags,myf MyFlags);
extern FILE *my_fdopen(File Filedes,const char *name, int Flags,myf MyFlags);
extern int my_fclose(FILE *fd,myf MyFlags);
extern int my_chsize(File fd,my_off_t newlength,myf MyFlags);
extern int my_error _VARARGS((int nr,myf MyFlags, ...));
extern int my_printf_error _VARARGS((uint my_err, const char *format,
				     myf MyFlags, ...)
				    __attribute__ ((format (printf, 2, 4))));
extern int my_vsnprintf( char *str, size_t n,
                                const char *format, va_list ap );
extern int my_snprintf(char* to, size_t n, const char* fmt, ...);
extern int my_message(uint my_err, const char *str,myf MyFlags);
extern int my_message_no_curses(uint my_err, const char *str,myf MyFlags);
extern int my_message_curses(uint my_err, const char *str,myf MyFlags);
extern void my_init(void);
extern void my_end(int infoflag);
extern int my_redel(const char *from, const char *to, int MyFlags);
extern int my_copystat(const char *from, const char *to, int MyFlags);
extern my_string my_filename(File fd);

#ifndef THREAD
extern void dont_break(void);
extern void allow_break(void);
#else
#define dont_break()
#define allow_break()
#endif

extern void my_remember_signal(int signal_number,sig_handler (*func)(int));
extern void caseup(my_string str,uint length);
extern void casedn(my_string str,uint length);
extern void caseup_str(my_string str);
extern void casedn_str(my_string str);
extern void case_sort(my_string str,uint length);
extern uint dirname_part(my_string to,const char *name);
extern uint dirname_length(const char *name);
#define base_name(A) (A+dirname_length(A))
extern int test_if_hard_path(const char *dir_name);
extern char *convert_dirname(my_string name);
extern void to_unix_path(my_string name);
extern my_string fn_ext(const char *name);
extern my_string fn_same(my_string toname,const char *name,int flag);
extern my_string fn_format(my_string to,const char *name,const char *dsk,
			   const char *form,int flag);
extern size_s strlength(const char *str);
extern void pack_dirname(my_string to,const char *from);
extern uint unpack_dirname(my_string to,const char *from);
extern uint cleanup_dirname(my_string to,const char *from);
extern uint system_filename(my_string to,const char *from);
extern my_string unpack_filename(my_string to,const char *from);
extern my_string intern_filename(my_string to,const char *from);
extern my_string directory_file_name(my_string dst, const char *src);
extern int pack_filename(my_string to, const char *name, size_s max_length);
extern my_string my_path(my_string to,const char *progname,
			 const char *own_pathname_part);
extern my_string my_load_path(my_string to, const char *path,
			      const char *own_path_prefix);
extern int wild_compare(const char *str,const char *wildstr);
extern my_string my_strcasestr(const char *src,const char *suffix);
extern int my_strcasecmp(const char *s,const char *t);
extern int my_strsortcmp(const char *s,const char *t);
extern int my_casecmp(const char *s,const char *t,uint length);
extern int my_sortcmp(const char *s,const char *t,uint length);
extern int my_sortncmp(const char *s,uint s_len, const char *t,uint t_len);
extern WF_PACK *wf_comp(my_string str);
extern int wf_test(struct wild_file_pack *wf_pack,const char *name);
extern void wf_end(struct wild_file_pack *buffer);
extern size_s strip_sp(my_string str);
extern void get_date(my_string to,int timeflag,time_t use_time);
extern void soundex(my_string out_pntr, my_string in_pntr,pbool remove_garbage);
extern int init_record_cache(RECORD_CACHE *info,uint cachesize,File file,
			     uint reclength,enum cache_type type,
			     pbool use_async_io);
extern int read_cache_record(RECORD_CACHE *info,unsigned char *to);
extern int end_record_cache(RECORD_CACHE *info);
extern int write_cache_record(RECORD_CACHE *info,my_off_t filepos,
			      const unsigned char *record,uint length);
extern int flush_write_cache(RECORD_CACHE *info);
extern long my_clock(void);
extern sig_handler sigtstp_handler(int signal_number);
extern void handle_recived_signals(void);
extern int init_key_cache(ulong use_mem,ulong leave_this_much_mem);
extern unsigned char *key_cache_read(File file,my_off_t filepos,unsigned char* buff,uint length,
			    uint block_length,int return_buffer);
extern int key_cache_write(File file,my_off_t filepos,unsigned char* buff,uint length,
			   uint block_length,int force_write);
extern int flush_key_blocks(int file, enum flush_type type);
extern void end_key_cache(void);
extern sig_handler my_set_alarm_variable(int signo);
extern void my_string_ptr_sort(void *base,uint items,size_s size);
extern void radixsort_for_str_ptr(uchar* base[], uint number_of_elements,
				  size_s size_of_element,uchar *buffer[]);
extern qsort_t qsort2(void *base_ptr, size_t total_elems, size_t size,
		      qsort2_cmp cmp, void *cmp_argument);
extern qsort2_cmp get_ptr_compare(uint);
extern int init_io_cache(IO_CACHE *info,File file,uint cachesize,
			 enum cache_type type,my_off_t seek_offset,
			 pbool use_async_io, myf cache_myflags);
extern my_bool reinit_io_cache(IO_CACHE *info,enum cache_type type,
			       my_off_t seek_offset,pbool use_async_io,
			       pbool clear_cache);
extern int _my_b_read(IO_CACHE *info,unsigned char *Buffer,uint Count);
extern int _my_b_net_read(IO_CACHE *info,unsigned char *Buffer,uint Count);
extern int _my_b_get(IO_CACHE *info);
extern int _my_b_async_read(IO_CACHE *info,unsigned char *Buffer,uint Count);
extern int _my_b_write(IO_CACHE *info,const unsigned char *Buffer,uint Count);
extern int my_block_write(IO_CACHE *info, const unsigned char *Buffer,
			  uint Count, my_off_t pos);
extern int flush_io_cache(IO_CACHE *info);
extern int end_io_cache(IO_CACHE *info);
extern uint my_b_fill(IO_CACHE *info);
extern void my_b_seek(IO_CACHE *info,my_off_t pos);
extern uint my_b_gets(IO_CACHE *info, char *to, uint max_length);
extern uint my_b_printf(IO_CACHE *info, const char* fmt, ...);
extern uint my_b_vprintf(IO_CACHE *info, const char* fmt, va_list ap);
extern my_bool open_cached_file(IO_CACHE *cache,const char *dir,
				 const char *prefix, uint cache_size,
				 myf cache_myflags);
extern my_bool real_open_cached_file(IO_CACHE *cache);
extern void close_cached_file(IO_CACHE *cache);
File create_temp_file(char *to, const char *dir, const char *pfx,
		      int mode, myf MyFlags);
#define my_init_dynamic_array(A,B,C,D) init_dynamic_array(A,B,C,D CALLER_INFO)
#define my_init_dynamic_array_ci(A,B,C,D) init_dynamic_array(A,B,C,D ORIG_CALLER_INFO)
extern my_bool init_dynamic_array(DYNAMIC_ARRAY *array,uint element_size,
	  uint init_alloc,uint alloc_increment CALLER_INFO_PROTO);
extern my_bool insert_dynamic(DYNAMIC_ARRAY *array,gptr element);
extern unsigned char *alloc_dynamic(DYNAMIC_ARRAY *array);
extern unsigned char *pop_dynamic(DYNAMIC_ARRAY*);
extern my_bool set_dynamic(DYNAMIC_ARRAY *array,gptr element,uint array_index);
extern void get_dynamic(DYNAMIC_ARRAY *array,gptr element,uint array_index);
extern void delete_dynamic(DYNAMIC_ARRAY *array);
extern void delete_dynamic_element(DYNAMIC_ARRAY *array, uint array_index);
extern void freeze_size(DYNAMIC_ARRAY *array);
#define dynamic_array_ptr(array,array_index) ((array)->buffer+(array_index)*(array)->size_of_element)
#define dynamic_element(array,array_index,type) ((type)((array)->buffer) +(array_index))
#define push_dynamic(A,B) insert_dynamic(A,B)

extern int find_type(my_string x,TYPELIB *typelib,uint full_name);
extern void make_type(my_string to,uint nr,TYPELIB *typelib);
extern const char *get_type(TYPELIB *typelib,uint nr);
extern my_bool init_dynamic_string(DYNAMIC_STRING *str, const char *init_str,
				   size_t init_alloc, size_t alloc_increment);
extern my_bool dynstr_append(DYNAMIC_STRING *str, const char *append);
my_bool dynstr_append_mem(DYNAMIC_STRING *str, const char *append,
			  size_t length);
extern my_bool dynstr_set(DYNAMIC_STRING *str, const char *init_str);
extern my_bool dynstr_realloc(DYNAMIC_STRING *str, size_t additional_size);
extern void dynstr_free(DYNAMIC_STRING *str);
void set_all_changeable_vars(CHANGEABLE_VAR *vars);
my_bool set_changeable_var(my_string str,CHANGEABLE_VAR *vars);
my_bool set_changeable_varval(const char *var, ulong val,
			      CHANGEABLE_VAR *vars);
#ifdef HAVE_MLOCK
extern unsigned char *my_malloc_lock(size_t length,myf flags);
extern void my_free_lock(unsigned char *ptr,myf flags);
#else
#define my_malloc_lock(A,B) my_malloc((A),(B))
#define my_free_lock(A,B) my_free((A),(B))
#endif
#define alloc_root_inited(A) ((A)->min_malloc != 0)
void init_alloc_root(MEM_ROOT *mem_root, size_t block_size, size_t pre_alloc_size);
gptr alloc_root(MEM_ROOT *mem_root, size_t Size);
void free_root(MEM_ROOT *root, myf MyFLAGS);
char *strdup_root(MEM_ROOT *root,const char *str);
char *memdup_root(MEM_ROOT *root,const char *str, size_t len);
void load_defaults(const char *conf_file, const char **groups,
		   int *argc, char ***argv);
void free_defaults(char **argv);
void print_defaults(const char *conf_file, const char **groups);
my_bool my_compress(unsigned char *, size_t *, size_t *);
my_bool my_uncompress(unsigned char *, size_t *, size_t *);
unsigned char *my_compress_alloc(const unsigned char *packet, size_t *len, size_t *complen);
ulong checksum(const unsigned char *mem, uint count);

#if defined(_MSC_VER) && !defined(_WIN32)
extern void sleep(int sec);
#endif
#ifdef _WIN32
extern my_bool have_tcpip;		/* Is set if tcpip is used */
#endif

#ifdef	__cplusplus
}
#endif
#endif /* _my_sys_h */
