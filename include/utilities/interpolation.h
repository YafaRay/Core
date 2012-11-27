/****************************************************************************
 * 		interpolation.h: Some interpolation algorithms
 *      This is part of the yafaray package
 *      Copyright (C) 2009  Bert Buchholz
 *		Split into a header and some speedups by Rodrigo Placencia
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
 */

#ifndef INTERPOLATION_H
#define INTERPOLATION_H

__BEGIN_YAFRAY

// Algorithms from: http://local.wasp.uwa.edu.au/~pbourke/miscellaneous/interpolation/

template <typename T>
inline static T lerp(
        T const& y1, T const& y2,
        double alpha)
{
    return y1 * alpha + y2 * (1.0 - alpha);
}

inline static double CosineInterpolate(
		double y1,double y2,
		double mu)
{
	double mu2;

	mu2 = (1 - fCos(mu * M_PI)) * 0.5f;
	return ( y1 * (1 - mu2) + y2 * mu2);
}

template <class T>
inline T CubicInterpolate(
		const T &y0, const T &y1,
		const T &y2, const T &y3,
		double mu)
{
	T a0,a1,a2,a3,mu2;

	mu2 = mu*mu;
	a0 = y3 - y2 - y0 + y1;
	a1 = y0 - y1 - a0;
	a2 = y2 - y0;
	a3 = y1;

	return (a0*mu*mu2+a1*mu2+a2*mu+a3);
}

__END_YAFRAY

#endif //INTERPOLATION_H
