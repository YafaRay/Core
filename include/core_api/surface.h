/****************************************************************************
 *
 * 			surface.h: Surface sampling representation and api
 *      This is part of the yafray package
 *      Copyright (C) 2002 Alejandro Conty Estevez
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
#ifndef Y_SURFACE_H
#define Y_SURFACE_H
#include "vector3d.h"
#include "color.h"

#include<yafray_config.h>

__BEGIN_YAFRAY
class material_t;
class light_t;
class object3d_t;
class diffRay_t;

struct intersectData_t
{
	float b0;
	float b1;
	float b2;
	float t;
	
	intersectData_t() : b0(0.f), b1(0.f), b2(0.f), t(0.f)
	{
		// Empty
	}
	
	inline void calcB0()
	{
		b0 = 1.0 - b1 - b2;
	}
	
	inline intersectData_t & operator = (intersectData_t &in)
	{
		b0 = in.b0;
		b1 = in.b1;
		b2 = in.b2;
		t = in.t;
		
		return *this;
	}
};

/*! This holds a sampled surface point's data
	When a ray intersects an object, a surfacePoint_t is computed.
	It contains data about normal, position, assigned material and other
	things.
 */
struct YAFRAYCORE_EXPORT surfacePoint_t
{
	//int object; //!< the object owner of the point.
	const material_t *material; //!< the surface material
	const light_t *light; //!< light source if surface point is on a light
	const object3d_t *object; //!< object the prim belongs to
//	point2d_t screenpos; // only used with 'win' texture coord. mode
	void *origin;
	
	// Geometry related
    vector3d_t N; //!< the shading normal.
	vector3d_t Ng; //!< the geometric normal.
	vector3d_t orcoNg; //!< the untransformed geometric normal.
	point3d_t P; //!< the (world) position.
	point3d_t orcoP;
//	color_t vertex_col;
	bool hasUV; 
	bool hasOrco;
	bool available;
	int primNum;

	GFLOAT U; //!< the u texture coord.
	GFLOAT V; //!< the v texture coord.
    vector3d_t  NU; //!< second vector building orthogonal shading space with N
    vector3d_t  NV; //!< third vector building orthogonal shading space with N
	vector3d_t dPdU; //!< u-axis in world space
	vector3d_t dPdV; //!< v-axis in world space
	vector3d_t dSdU; //!< u-axis in shading space (NU, NV, N)
	vector3d_t dSdV; //!< v-axis in shading space (NU, NV, N)
	//GFLOAT dudNU;
	//GFLOAT dudNV;
	//GFLOAT dvdNU;
	//GFLOAT dvdNV;
};

surfacePoint_t blend_surface_points(surfacePoint_t const& sp_0, surfacePoint_t const& sp_1, float const alpha);

/*! computes and stores the additional data for surface intersections for
	differential rays */
class YAFRAYCORE_EXPORT spDifferentials_t
{
	public:
		spDifferentials_t(const surfacePoint_t &spoint, const diffRay_t &ray);
		//! compute differentials for a scattered ray
		void reflectedRay(const diffRay_t &in, diffRay_t &out) const;
		//! compute differentials for a refracted ray
		void refractedRay(const diffRay_t &in, diffRay_t &out, PFLOAT IOR) const;
		PFLOAT projectedPixelArea();
		vector3d_t dPdx;
		vector3d_t dPdy;
		const surfacePoint_t &sp;
};

__END_YAFRAY

#endif
