#include <neko.h>
#include <time.h>
#include <string.h>
#ifdef __linux__
#include <sys/time.h>
#endif

DEFINE_KIND(k_random);

#define val_rnd(o)		((rnd*)val_data(o))

#define NSEEDS	25
#define MAX		7

typedef struct _rnd rnd;

struct _rnd {
	unsigned long seeds[NSEEDS];
	unsigned long cur;
};

static unsigned long mag01[2]={ 
	0x0, 0x8ebfd028 // magic, don't change
};

static const unsigned long init_seeds[] = {
	0x95f24dab, 0x0b685215, 0xe76ccae7, 0xaf3ec239, 0x715fad23,
	0x24a590ad, 0x69e4b5ef, 0xbf456141, 0x96bc1b7b, 0xa7bdf825,
	0xc1de75b7, 0x8858a9c9, 0x2da87693, 0xb657f9dd, 0xffdc8a9f,
	0x8121da71, 0x8b823ecb, 0x885d05f5, 0x4e20cd47, 0x5a9ad5d9,
	0x512c0c03, 0xea857ccd, 0x4cc1d30f, 0x8891a8a1, 0xa6b7aadb
};

#ifdef _WIN32
struct timeval {
    long tv_sec;
    long tv_usec;
};

static int gettimeofday( struct timeval *t, struct timezone *tz ) {
	t->tv_sec = time(NULL);
	t->tv_usec = 0;
	return 0;
}
#endif

static int rnd_size() {
	return sizeof(rnd);
}

static void rnd_set_seed( rnd *r, int s ) {
	int i;
	r->cur = 0;
	memcpy(r->seeds,init_seeds,sizeof(init_seeds));
	for(i=0;i<NSEEDS;i++)
		r->seeds[i] ^= s;
}

static rnd *rnd_init( void *data ) {
	rnd *r = (rnd*)data;
	struct timeval t;
	gettimeofday(&t,NULL);
	rnd_set_seed(r,t.tv_sec * 1000000 + t.tv_usec);
	return r;
}

static unsigned int rnd_int( rnd *r ) {
	unsigned int y;
    if( r->cur == NSEEDS ) {
		int kk;
		for(kk=0;kk<NSEEDS-MAX;kk++)
			r->seeds[kk] = r->seeds[kk+MAX] ^ (r->seeds[kk] >> 1) ^ mag01[r->seeds[kk] % 2];		
		for(;kk<NSEEDS;kk++)
			r->seeds[kk] = r->seeds[kk+(MAX-NSEEDS)] ^ (r->seeds[kk] >> 1) ^ mag01[r->seeds[kk] % 2];      
		r->cur = 0;
	}
    y = r->seeds[r->cur++];
    y ^= (y << 7) & 0x2b5b2500;
    y ^= (y << 15) & 0xdb8b0000;
    y ^= (y >> 16);
	return y;
}

static double rnd_float( rnd *r ) {
	double big = 4294967296.0;	
	return ((rnd_int(r) / big + rnd_int(r)) / big + rnd_int(r)) / big;
}

static value random_new() {
	return alloc_abstract( k_random, rnd_init(alloc_private(rnd_size())) );
}

static value random_set_seed( value o, value v ) {
	val_check_kind(o,k_random);
	if( !val_is_int(v) )
		return val_null;
	rnd_set_seed(val_rnd(o),val_int(v));
	return val_true;
}

static value random_int( value o, value max ) {
	val_check_kind(o,k_random);
	if( !val_is_int(max) )
		return val_null;
	if( val_int(max) <= 0 )
		return alloc_int(0);
	return alloc_int( (rnd_int(val_rnd(o)) & 0x3FFFFFFF) % val_int(max) );
}

static value random_float( value o ) {
	val_check_kind(o,k_random);
	return alloc_float( rnd_float(val_rnd(o)) );
}

DEFINE_PRIM(random_new,0);
DEFINE_PRIM(random_set_seed,2);
DEFINE_PRIM(random_int,2);
DEFINE_PRIM(random_float,1);
