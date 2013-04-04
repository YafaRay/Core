/****************************************************************************
 * 			std_primitives.cc: standard geometric primitives
 *      This is part of the yafray package
 *      Copyright (C) 2006  Mathias Wein
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

#include <yafraycore/std_primitives.h>
#include <core_api/object3d.h>
#include <core_api/environment.h>

__BEGIN_YAFRAY

bound_t sphere_t::getBound() const
{
	vector3d_t r(radius * 1.0001);
	return bound_t(center - r, center + r);
}

bool sphere_t::intersect(const ray_t &ray, PFLOAT *t, intersectData_t &data) const
{
	vector3d_t vf = ray.from - center;
	PFLOAT ea = ray.dir*ray.dir;
	PFLOAT eb = 2.0*(vf*ray.dir);
	PFLOAT ec = vf*vf - radius*radius;
	PFLOAT osc = eb*eb-4.0*ea*ec;
	if(osc<0) return false;
	osc=fSqrt(osc);
	PFLOAT sol1=(-eb-osc)/(2.0*ea);
	PFLOAT sol2=(-eb+osc)/(2.0*ea);
	PFLOAT sol=sol1;
	if(sol < ray.tmin)
	{
		sol = sol2;
		if(sol < ray.tmin) return false;
	}
	//if(sol > ray.tmax) return false; //tmax = -1 is not substituted yet...
	*t = sol;
	return true;
}

void sphere_t::getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const
{
	vector3d_t normal = hit - center;
	sp.orcoP = normal;
	normal.normalize();
	sp.material = material;
	sp.N = normal;
	sp.Ng = normal;
	//sp.origin = (void*)this;
	sp.hasOrco = true;
	sp.P = hit;
	createCS(sp.N, sp.NU, sp.NV);
	sp.U = atan2(normal.y, normal.x)*M_1_PI + 1;
	sp.V = 1.f - fAcos(normal.z)*M_1_PI;
	sp.light = 0;
}

object3d_t* sphere_factory(paraMap_t &params, renderEnvironment_t &env)
{
	point3d_t center(0.f, 0.f, 0.f);
	double radius(1.f);
	const material_t *mat;
	const std::string *matname=0;
	params.getParam("center", center);
	params.getParam("radius", radius);
	params.getParam("material", matname);
	if(!matname) return 0;
	mat = env.getMaterial(*matname);
	if(!mat) return 0;
	sphere_t *sphere = new sphere_t(center, radius, mat);
	return new primObject_t(sphere);
}

__END_YAFRAY
