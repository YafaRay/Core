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

#define M_2PI		6.28318530717958647692
#define M_PI2		9.86960440108935861882
#define M_1_2PI		0.15915494309189533577
#define M_4_PI		1.27323954473516268615
#define M_4_PI2		0.40528473456935108578

#define degToRad(deg) (deg * 0.01745329251994329576922)
#define radToDeg(rad) (rad * 57.29577951308232087684636)

#define POLYEXP(x) (float)(x * (x * (x * (x * (x * 1.8775767e-3f + 8.9893397e-3f) + 5.5826318e-2f) + 2.4015361e-1f) + 6.9315308e-1f) + 9.9999994e-1f)
#define POLYLOG(x) (float)(x * (x * (x * (x * (x * -3.4436006e-2f + 3.1821337e-1f) + -1.2315303f) + 2.5988452) + -3.3241990f) + 3.1157899f)

#define f_HI 129.00000f
#define f_LOW -126.99999f

#define LOG_EXP 0x7F800000
#define LOG_MANT 0x7FFFFF

#define CONST_P 0.225f

union bitTwiddler
{
	int i;
	float f;
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

inline float fLog2(float x)
{
	bitTwiddler one, i, m, e;

	one.f = 1.0f;
	i.f = x;
	e.f = (float)(((i.i & LOG_EXP) >> 23) - 127);
	m.i = ((i.i & LOG_MANT) | one.i);

	return (POLYLOG(m.f) * (m.f - one.f) + e.f);
}

// Two Babylonian Steps method
inline float bab2xSqrt(float x)
{
	bitTwiddler a;

	a.f = x;
	a.i = (1<<29) + (a.i >> 1) - (1<<22); 
	
	// a simplification to add a bit of speed
	a.f = a.f + x/a.f;
	return 0.25f*a.f + x/a.f;
}

inline float iSqrt(float x)
{
    bitTwiddler a;
    float xhalf = 0.5f * x;

    a.f = x;
    a.i = 0x5f3759df - (a.i>>1);

    return a.f*(1.5f - xhalf*a.f*a.f);
}

inline float fPow(float a, float b)
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
	return fLog2(a) * M_LN2;
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

inline float fISqrt(float a)
{
#ifdef FAST_MATH
	return iSqrt(a);
#else
	return 1.f/sqrt(a);
#endif
}

inline float fSqrt(float a)
{
#ifdef FAST_MATH
	return bab2xSqrt(a);
	//return iSqrt(a) * a;
	//return sqrtf(a);
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
	return CONST_P * (x * std::fabs(x) - x) + x;

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

inline float fTan(float x)
{
#ifdef FAST_TRIG
	return fSin(x) / fCos(x);
#else
	return tan(x);
#endif
}

inline float fAsin(float x)
{
	float x2 = x * x;
	return (x + (0.166666667f + (0.075f + (0.0446428571f + (0.0303819444f + 0.022372159f * x2) * x2) * x2) * x2) * x2);
}

inline float fAcos(float x)
{
	return (float)M_PI_2 - fAsin(x);
}

inline float fAtan(float x)
{
	float x2 = x * x;
	return (x - (0.333333333333f + (0.2f - (0.1428571429f + (0.111111111111f - 0.0909090909f * x2) * x2) * x2) * x2) * x2);
}
__END_YAFRAY

#endif
