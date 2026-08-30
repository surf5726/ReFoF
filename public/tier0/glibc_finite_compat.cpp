#include <features.h>
#include <math.h>

// Old Linux32 SDK archives reference glibc's retired finite-math entry points.
#if defined( __GLIBC__ ) && __GLIBC_PREREQ( 2, 31 )
extern "C"
{
float __acosf_finite( float value )
{
	return acosf( value );
}

double __atan2_finite( double y, double x )
{
	return atan2( y, x );
}

double __exp_finite( double value )
{
	return exp( value );
}

float __expf_finite( float value )
{
	return expf( value );
}

double __log_finite( double value )
{
	return log( value );
}

float __logf_finite( float value )
{
	return logf( value );
}

double __pow_finite( double base, double exponent )
{
	return pow( base, exponent );
}

float __powf_finite( float base, float exponent )
{
	return powf( base, exponent );
}
}
#endif
