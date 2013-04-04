/****************************************************************************
 *
 *      mathOptimizations.h: Math aproximations to speed up things
 *      This is part of the yafray package
 *      Copyright (C) 2009 Rodrigo Placencia Vazquez (DarkTide)
 *		Creation date: 2009-03-26
 *
 *		fPow() based on the polynomials approach form Jose Fonséca's blog entry:
 *		Fast SSE2 pow: tables or polynomials?
 *		http://jrfonseca.blogspot.com/2008/09/fast-sse2-pow-tables-or-polynomials.html
 *
 *		fSin(), fCos() and fTan() based on Fast and Accurate sine/cosine
 *		thread on DevMaster.net forum, posted by Nick
 *		http://www.devmaster.net/forums/showthread.php?t=5784
 *
 ****************************************************************************
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2.1 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library; if not, write to the Free Software
 *      Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef Y_MATHOPTIMIZATIONS_H
#define Y_MATHOPTIMIZATIONS_H

#include <cmath>
#include <algorithm>

// Reference defines, this should be defined by the standard cmath header

//# define M_E		2.7182818284590452354	/* e */
//# define M_LOG2E	1.4426950408889634074	/* log_2 e */
//# define M_LOG10E	0.43429448190325182765	/* log_10 e */
//# define M_LN2		0.69314718055994530942	/* log_e 2 */
//# define M_LN10		2.30258509299404568402	/* log_e 10 */
//# define M_PI		3.14159265358979323846	/* pi */
//# define M_PI_2		1.57079632679489661923	/* pi/2 */
//# define M_PI_4		0.78539816339744830962	/* pi/4 */
//# define M_1_PI		0.31830988618379067154	/* 1/pi */
//# define M_2_PI		0.63661977236758134308	/* 2/pi */
//# define M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
//# define M_SQRT2	1.41421356237309504880	/* sqrt(2) */
//# define M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

__BEGIN_YAFRAY

#define M_2PI		6.28318530717958647692 // PI * 2
#define M_PI2		9.86960440108935861882 // PI ^ 2
#define M_1_2PI		0.15915494309189533577 // 1 / (2 * PI)
#define M_4_PI		1.27323954473516268615 // 4 / PI
#define M_4_PI2		0.40528473456935108578 // 4 / PI ^ 2
#define M_MINUS_PI	-3.14159265358979323846	/* -pi */
#define M_MINUS_PI_2		-1.57079632679489661923	/* -pi/2 */

#define degToRad(deg) (deg * 0.01745329251994329576922)  // deg * PI / 180
#define radToDeg(rad) (rad * 57.29577951308232087684636) // rad * 180 / PI

//FIXME: All the overloaded double definitions have been added to fix the "white dots" problems and they seem to work fine. However several of them should be refined in the future to include constants with the correct precision for the doubles calculations.

#define POLYEXP(x) (float)(x * (x * (x * (x * (x * 1.8775767e-3f + 8.9893397e-3f) + 5.5826318e-2f) + 2.4015361e-1f) + 6.9315308e-1f) + 9.9999994e-1f)
#define POLYLOG(x) (float)(x * (x * (x * (x * (x * -3.4436006e-2f + 3.1821337e-1f) + -1.2315303f) + 2.5988452) + -3.3241990f) + 3.1157899f)
#define POLYEXP_DOUBLE(x) (double)(x * (x * (x * (x * (x * 1.8775767e-3f + 8.9893397e-3f) + 5.5826318e-2f) + 2.4015361e-1f) + 6.9315308e-1f) + 9.9999994e-1f)
#define POLYLOG_DOUBLE(x) (double)(x * (x * (x * (x * (x * -3.4436006e-2f + 3.1821337e-1f) + -1.2315303f) + 2.5988452) + -3.3241990f) + 3.1157899f)

#define f_HI 129.00000f
#define f_LOW -126.99999f
#define f_HI_DOUBLE 129.000000000000000
#define f_LOW_DOUBLE -126.999999999999999

#define LOG_EXP 0x7F800000
#define LOG_MANT 0x7FFFFF
#define LOG_EXP_DOUBLE 0x7F800000
#define LOG_MANT_DOUBLE 0x7FFFFF

#define CONST_P 0.225f

union bitTwiddler
{
	int i;
	float f;
};

union bitTwiddler_double
{
	long i;
	double f;
};

inline float fExp2(float x)
{
	bitTwiddler ipart, fpart;
	bitTwiddler expipart;

	x = std::min(x, f_HI);
	x = std::max(x, f_LOW);

	ipart.i = (int)(x - 0.5f);
	fpart.f = (x - (float)(ipart.i));
	expipart.i = ((ipart.i + 127) << 23);

	return (expipart.f * POLYEXP(fpart.f));
}

inline double fExp2(double x)
{
	bitTwiddler_double ipart, fpart;
	bitTwiddler_double expipart;

	x = std::min(x, f_HI_DOUBLE);
	x = std::max(x, f_LOW_DOUBLE);

	ipart.i = (int)(x - 0.5);
	fpart.f = (x - (double)(ipart.i));
	expipart.i = ((ipart.i + 127) << 23);

	return (expipart.f * POLYEXP_DOUBLE(fpart.f));
}

inline float fLog2(float x)
{
	bitTwiddler one, i, m, e;

	one.f = 1.0f;
	i.f = x;
	e.f = (float)(((i.i & LOG_EXP) >> 23) - 127);
	m.i = ((i.i & LOG_MANT) | one.i);

	return (POLYLOG(m.f) * (m.f - one.f) + e.f);
}

inline double fLog2(double x)
{
	bitTwiddler_double one, i, m, e;

	one.f = 1.0;
	i.f = x;
	e.f = (double)(((i.i & LOG_EXP_DOUBLE) >> 23) - 127);
	m.i = ((i.i & LOG_MANT_DOUBLE) | one.i);

	return (POLYLOG_DOUBLE(m.f) * (m.f - one.f) + e.f);
}

inline float asmSqrt(float n)
{
    float r = n;
#ifdef _MSC_VER
    __asm
    {
		fld r
		fsqrt
		fstp r
    }
#elif __GNUC__
    asm(
		"fld %0;"
		"fsqrt;"
		"fstp %0"
		:"=m" (r)
		:"m" (r)
		);
#else
    r = fsqrt(n);
#endif
    return r;
}

inline double asmSqrt(double n)
{
    double r = n;
#ifdef _MSC_VER
    __asm
    {
		fld r
		fsqrt
		fstp r
    }
#elif __GNUC__
    asm(
		"fld %0;"
		"fsqrt;"
		"fstp %0"
		:"=m" (r)
		:"m" (r)
		);
#else
    r = fsqrt(n);
#endif
    return r;
}

inline float iSqrt(float x)
{
    bitTwiddler a;
    float xhalf = 0.5f * x;

    a.f = x;
    a.i = 0x5f3759df - (a.i>>1);

    return a.f*(1.5f - xhalf*a.f*a.f);
}

inline double iSqrt(double x)
{
    bitTwiddler_double a;
    double xhalf = 0.5 * x;

    a.f = x;
    a.i = 0x5f3759df - (a.i>>1);

    return a.f*(1.5 - xhalf*a.f*a.f);
}

inline float fPow(float a, float b)
{
#ifdef FAST_MATH
	return fExp2(fLog2(a) * b);
#else
	return pow(a,b);
#endif
}

inline double fPow(double a, double b)
{
#ifdef FAST_MATH
	return fExp2(fLog2(a) * b);
#else
	return pow(a,b);
#endif
}

inline float fLog(float a)
{
#ifdef FAST_MATH
	return fLog2(a) * (float) M_LN2;
#else
	return log(a);
#endif
}

inline double fLog(double a)
{
#ifdef FAST_MATH
	return fLog2(a) * (double) M_LN2;
#else
	return log(a);
#endif
}

inline float fExp(float a)
{
#ifdef FAST_MATH
	return fExp2((float)M_LOG2E * a);
#else
	return exp(a);
#endif
}

inline double fExp(double a)
{
#ifdef FAST_MATH
	return fExp2((double)M_LOG2E * a);
#else
	return exp(a);
#endif
}

inline float fISqrt(float a)
{
#ifdef FAST_MATH
	return iSqrt(a);
#else
	return 1.f/sqrt(a);
#endif
}

inline double fISqrt(double a)
{
#ifdef FAST_MATH
	return iSqrt(a);
#else
	return 1.0/sqrt(a);
#endif
}

inline float fSqrt(float a)
{
#ifdef FAST_MATH
	return asmSqrt(a);
#else
	return sqrt(a);
#endif
}

inline double fSqrt(double a)
{
#ifdef FAST_MATH
	return asmSqrt(a);
#else
	return sqrt(a);
#endif
}

inline float fLdexp(float x, int a)
{
#ifdef FAST_MATH
	//return x * fPow(2.0, a);
	return ldexp(x, a);
#else
	return ldexp(x, a);
#endif
}

inline double fLdexp(double x, int a)
{
#ifdef FAST_MATH
	//return x * fPow(2.0, a);
	return ldexp(x, a);
#else
	return ldexp(x, a);
#endif
}

inline float fSin(float x)
{ 
#ifdef FAST_TRIG
	if(x > M_2PI || x < -M_2PI) x -= ((int)(x * (float)M_1_2PI)) * (float)M_2PI; //float modulo x % M_2PI
	if(x < -M_PI)
	{
		x += (float)M_2PI;
	}
	else if(x > M_PI)
	{
		x -= (float)M_2PI;
	}

	x = ((float)M_4_PI * x) - ((float)M_4_PI2 * x * std::fabs(x));
	float result = CONST_P * (x * std::fabs(x) - x) + x;
  //Make sure that the function is in the valid range [-1.0,+1.0]
	if(result <= -1.0) return -1.0f;
	else if(result >= 1.0) return 1.0f;
	else return result;
#else
	return sin(x);
#endif
}

inline double fSin(double x)
{
#ifdef FAST_TRIG
	if(x > M_2PI || x < -M_2PI) x -= ((int)(x * (double)M_1_2PI)) * (double)M_2PI; //double modulo x % M_2PI
	if(x < -M_PI)
	{
		x += (double)M_2PI;
	}
	else if(x > M_PI)
	{
		x -= (double)M_2PI;
	}

	x = ((double)M_4_PI * x) - ((double)M_4_PI2 * x * std::fabs(x));
	double result = CONST_P * (x * std::fabs(x) - x) + x;
	//Make sure that the function is in the valid range [-1.0,+1.0]
	if(result <= -1.0) return -1.0;
	else if(result >= 1.0) return 1.0;
	else return result;
#else
	return sin(x);
#endif
}

inline float fCos(float x)
{
#ifdef FAST_TRIG
	return fSin(x + (float)M_PI_2);
#else
	return cos(x);
#endif
}

inline double fCos(double x)
{
#ifdef FAST_TRIG
	return fSin(x + (double)M_PI_2);
#else
	return cos(x);
#endif
}

inline float fTan(float x)
{
#ifdef FAST_TRIG
  //FIXME: We should consider the case when fCos(x)=0.0 to avoid Inf numbers.
	return fSin(x) / fCos(x); 
#else
	return tan(x);
#endif
}

inline double fTan(double x)
{
#ifdef FAST_TRIG
  //FIXME: We should consider the case when fCos(x)=0.0 to avoid Inf numbers.
	return fSin(x) / fCos(x);
#else
	return tan(x);
#endif
}

inline float fAcos(float x)
{
	//checks if variable gets out of domain [-1.0,+1.0], so you get the range limit instead of NaN
	if(x<=-1.0) return((float)M_PI);
	else if(x>=1.0) return(0.0);
	else return acos(x);
}

inline double fAcos(double x)
{
	//checks if variable gets out of domain [-1.0,+1.0], so you get the range limit instead of NaN
	if(x<=-1.0) return(M_PI);
	else if(x>=1.0) return(0.0);
	else return acos(x);
}

inline float fAsin(float x)
{
	//checks if variable gets out of domain [-1.0,+1.0], so you get the range limit instead of NaN
	if(x<=-1.0) return((float)M_MINUS_PI_2);	
	else if(x>=1.0) return((float)M_PI_2);
	else return asin(x);
}

inline double fAsin(double x)
{
	//checks if variable gets out of domain [-1.0,+1.0], so you get the range limit instead of NaN
	if(x<=-1.0) return(M_MINUS_PI_2);	
	else if(x>=1.0) return(M_PI_2);
	else return asin(x);
}

__END_YAFRAY

#endif
