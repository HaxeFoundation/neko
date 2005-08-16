#include <neko.h>
#include <math.h>

#ifdef _WIN32
	long _ftol( double f );
	long _ftol2( double f) { return _ftol(f); };
#endif

#define MATH_PRIM(f) \
	value math_##f( value n ) { \
		val_check(n,number); \
		return alloc_float( f( val_number(n) ) ); \
	} \
	DEFINE_PRIM(math_##f,1)

static value math_atan2( value a, value b ) {
	val_check(a,number);
	val_check(b,number);
	return alloc_float( atan2(val_number(a),val_number(b)) );
}

static value math_pow( value a, value b ) {
	if( val_is_int(a) && val_is_int(b) )
		return alloc_int( (int)pow(val_int(a),val_int(b)) );
	val_check(a,number);
	val_check(b,number);
	return alloc_float( pow(val_number(a),val_number(b)) );
}

static value math_abs( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return alloc_int( abs(val_int(n)) );
	case VAL_FLOAT:
		return alloc_float( fabs(val_float(n)) ); 
	default:
		type_error();
	}
}

static value math_ceil( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return n;
	case VAL_FLOAT:
		return alloc_int( (int)ceil(val_float(n)) );
	default:
		type_error();
	}
}

static value math_floor( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return n;
	case VAL_FLOAT:
		return alloc_int( (int)floor(val_float(n)) );
	default:
		type_error();
	}
}

static value math_round( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return n;
	case VAL_FLOAT:
		{
			int ival = (int)val_float(n);
			if( val_float(n) > ival+0.5 )
				return alloc_int(ival+1);			
			return alloc_int(ival);
		}
		break;
	default:
		type_error();
	}
}

#define PI 3.1415926535897932384626433832795

static value math_pi() {
	return alloc_float(PI);
}

MATH_PRIM(sqrt);
MATH_PRIM(atan);
MATH_PRIM(cos);
MATH_PRIM(sin);
MATH_PRIM(tan);
MATH_PRIM(log);
MATH_PRIM(exp);
MATH_PRIM(acos);
MATH_PRIM(asin);
DEFINE_PRIM(math_pi,0);
DEFINE_PRIM(math_atan2,2);
DEFINE_PRIM(math_pow,2);
DEFINE_PRIM(math_abs,1);
DEFINE_PRIM(math_ceil,1);
DEFINE_PRIM(math_floor,1);
DEFINE_PRIM(math_round,1);
