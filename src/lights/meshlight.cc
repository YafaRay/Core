/****************************************************************************
 * 			meshlight.cc: a light source using a triangle mesh as shape
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
 
#include <limits>

#include <lights/meshlight.h>
#include <core_api/background.h>
#include <core_api/texture.h>
#include <core_api/environment.h>
#include <utilities/sample_utils.h>
#include <yafraycore/kdtree.h>

__BEGIN_YAFRAY

meshLight_t::meshLight_t(unsigned int msh, const color_t &col, int sampl, bool dbl_s):
	objID(msh), doubleSided(dbl_s), color(col), samples(sampl), tree(0)
{
	mesh = 0;
	//initIS();
}

meshLight_t::~meshLight_t()
{
	delete areaDist;
	areaDist = 0;
	delete[] tris;
	if(tree) delete tree;
}

void meshLight_t::initIS()
{
	nTris = mesh->numPrimitives();
	tris = new const triangle_t*[nTris];
	mesh->getPrimitives(tris);
	float *areas = new float[nTris];
	double totalArea = 0.0;
	for(int i=0; i<nTris; ++i)
	{
		areas[i] = tris[i]->surfaceArea();
		totalArea += areas[i];
	}
	areaDist = new pdf1D_t(areas, nTris);
	area = (float)totalArea;
	invArea = (float)(1.0/totalArea);
	delete[] areas;
	if(tree) delete tree;
	tree = new triKdTree_t(tris, nTris, -1, 1, 0.8, 0.33);
}

void meshLight_t::init(scene_t &scene)
{
	mesh = scene.getMesh(objID);
	if(mesh)
	{
		initIS();
		// tell the mesh that a meshlight is associated with it (not sure if this is the best place though):
		mesh->setLight(this);

		Y_INFO << "MeshLight: triangles:" << nTris << ", double sided:" << doubleSided << ", area:" << area << " color:" << color << yendl;
	}
}

void meshLight_t::sampleSurface(point3d_t &p, vector3d_t &n, float s1, float s2) const
{
	float primPdf;
	int primNum = areaDist->DSample(s1, &primPdf);
	if(primNum >= areaDist->count)
	{
		Y_INFO << "MeshLight: Sampling error!" << yendl;
		return;
	}
	float ss1, delta = areaDist->cdf[primNum+1];
	if(primNum > 0)
	{
		delta -= areaDist->cdf[primNum];
		ss1 = (s1 - areaDist->cdf[primNum]) / delta;
	}
	else ss1 = s1 / delta;
	tris[primNum]->sample(ss1, s2, p, n);
//	++stats[primNum];
}

color_t meshLight_t::totalEnergy() const { return (doubleSided ? 2.f*color*area : color*area); }

bool meshLight_t::illumSample(const surfacePoint_t &sp, lSample_t &s, ray_t &wi) const
{
	vector3d_t n;
	point3d_t p;
	sampleSurface(p, n, s.s1, s.s2);
	
	vector3d_t ldir = p - sp.P;
	//normalize vec and compute inverse square distance
	PFLOAT dist_sqr = ldir.lengthSqr();
	PFLOAT dist = fSqrt(dist_sqr);
	if(dist <= 0.0) return false;
	ldir *= 1.f/dist;
	PFLOAT cos_angle = -(ldir*n);
	//no light if point is behind area light (single sided!)
	if(cos_angle <= 0)
	{
		if(doubleSided) cos_angle = -cos_angle;
		else return false;
	}
	
	// fill direction
	wi.tmax = dist;
	wi.dir = ldir;
	
	s.col = color;
	// pdf = distance^2 / area * cos(norm, ldir);
	float area_mul_cosangle = area * cos_angle;
	//TODO: replace the hardcoded value (1e-8f) by a macro for min/max values: here used, to avoid dividing by zero
	s.pdf = dist_sqr*M_PI / ((area_mul_cosangle == 0.f)?1e-8f:area_mul_cosangle);
	s.flags = flags;
	if(s.sp)
	{
		s.sp->P = p;
		s.sp->N = s.sp->Ng = n;
	}
	return true;
}

color_t meshLight_t::emitPhoton(float s1, float s2, float s3, float s4, ray_t &ray, float &ipdf) const
{
	vector3d_t normal, du, dv;
	ipdf = area;
	sampleSurface(ray.from, normal, s3, s4);
	createCS(normal, du, dv);
	
	if(doubleSided)
	{
		ipdf *= 2.f;
		if(s1>0.5f)	ray.dir = SampleCosHemisphere(-normal, du, dv, (s1-0.5f)*2.f, s2);
		else 		ray.dir = SampleCosHemisphere(normal, du, dv, s1*2.f, s2);
	}
	else ray.dir = SampleCosHemisphere(normal, du, dv, s1, s2);
	return color;
}

color_t meshLight_t::emitSample(vector3d_t &wo, lSample_t &s) const
{
	s.areaPdf = invArea * M_PI;
	sampleSurface(s.sp->P, s.sp->Ng, s.s3, s.s4);
	s.sp->N = s.sp->Ng;
	vector3d_t du, dv;
	createCS(s.sp->Ng, du, dv);
	
	if(doubleSided)
	{
		if(s.s1>0.5f)	wo = SampleCosHemisphere(-s.sp->Ng, du, dv, (s.s1-0.5f)*2.f, s.s2);
		else 		wo = SampleCosHemisphere(s.sp->Ng, du, dv, s.s1*2.f, s.s2);
		s.dirPdf = 0.5f * std::fabs(s.sp->Ng * wo);
	}
	else
	{
		wo = SampleCosHemisphere(s.sp->Ng, du, dv, s.s1, s.s2);
		s.dirPdf = std::fabs(s.sp->Ng * wo);
	}
	s.flags = flags;
	return color;
}

bool meshLight_t::intersect(const ray_t &ray, PFLOAT &t, color_t &col, float &ipdf) const
{
	if(!tree) return false;
	PFLOAT dis;
	intersectData_t bary;
	triangle_t *hitt=0;
	if(ray.tmax<0) dis=std::numeric_limits<PFLOAT>::infinity();
	else dis=ray.tmax;
	// intersect with tree:
	if( ! tree->Intersect(ray, dis, &hitt, t, bary) ){ return false; }
	
	vector3d_t n = hitt->getNormal();
	PFLOAT cos_angle = ray.dir*(-n);
	if(cos_angle <= 0)
	{
		if(doubleSided) cos_angle = std::fabs(cos_angle);
		else return false;
	}
	PFLOAT idist_sqr = 1.f / (t*t);
	ipdf = idist_sqr * area * cos_angle * (1.f/M_PI);
	col = color;
	
	return true;
}

float meshLight_t::illumPdf(const surfacePoint_t &sp, const surfacePoint_t &sp_light) const
{
	vector3d_t wo = sp.P - sp_light.P;
	PFLOAT r2 = wo.normLenSqr();
	float cos_n = wo * sp_light.Ng;
	return cos_n > 0 ? r2 * M_PI / (area * cos_n) : (doubleSided ? r2 * M_PI / (area * -cos_n)  : 0.f);
}

void meshLight_t::emitPdf(const surfacePoint_t &sp, const vector3d_t &wo, float &areaPdf, float &dirPdf, float &cos_wo) const
{
	areaPdf = invArea * M_PI;
	cos_wo = wo*sp.N;
	dirPdf = cos_wo > 0.f ? (doubleSided ? cos_wo * 0.5f : cos_wo ) : ( doubleSided ?  -cos_wo * 0.5f : 0.f);
}


light_t* meshLight_t::factory(paraMap_t &params,renderEnvironment_t &render)
{
	bool doubleS = false;
	color_t color(1.0);
	double power = 1.0;
	int samples = 4;
	int object = 0;

	params.getParam("object", object);
	params.getParam("color", color);
	params.getParam("power", power);
	params.getParam("samples", samples);
	params.getParam("double_sided", doubleS);

	return new meshLight_t(object, color*(CFLOAT)power*M_PI, samples, doubleS);
}
__END_YAFRAY
