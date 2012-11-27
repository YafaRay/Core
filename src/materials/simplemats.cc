/****************************************************************************
 * 			simplemats.cc: a collection of simple materials
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
 
#include <core_api/material.h>
#include <core_api/environment.h>
#include <core_api/scene.h>
#include <materials/maskmat.h>
//#include <yafraycore/spectrum.h>


/*=============================================================
a material intended for visible light sources, i.e. it has no
other properties than emiting light in conformance to uniform
surface light sources (area, sphere, mesh lights...)
=============================================================*/

__BEGIN_YAFRAY

class lightMat_t: public material_t
{
	public:
		lightMat_t(color_t lightC, bool ds=false);
        virtual void initBSDF(const renderState_t &state, surfacePoint_t &sp, unsigned int &bsdfTypes) const { bsdfTypes=bsdfFlags; }
		virtual color_t eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wl, BSDF_t bsdfs) const {return color_t(0.0);}
		virtual color_t sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W) const;
		virtual color_t emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo) const;
		virtual float pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs) const;
		static material_t* factory(paraMap_t &params, std::list< paraMap_t > &eparans, renderEnvironment_t &env);
	protected:
		color_t lightCol;
		bool doubleSided;
};

lightMat_t::lightMat_t(color_t lightC, bool ds): lightCol(lightC), doubleSided(ds)
{
	bsdfFlags = BSDF_EMIT;
}

color_t lightMat_t::sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const
{
	s.pdf = 0.f;
	W = 0.f;
	return color_t(0.f);
}

color_t lightMat_t::emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	if(!state.includeLights) return color_t(0.f);
	if(doubleSided) return lightCol;
	
	PFLOAT angle = wo*sp.N;
	return (angle>0) ? lightCol : color_t(0.f);
}

float lightMat_t::pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs) const
{
	return 0.f;
}
material_t* lightMat_t::factory(paraMap_t &params, std::list< paraMap_t > &eparans, renderEnvironment_t &env)
{
	color_t col(1.0);
	double power = 1.0;
	bool ds = false;
	params.getParam("color", col);
	params.getParam("power", power);
	params.getParam("double_sided", ds);
	return new lightMat_t(col*(CFLOAT)power, ds);
}

extern "C"
{
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("light_mat", lightMat_t::factory);
		render.registerFactory("mask_mat", maskMat_t::factory);
	}
}

__END_YAFRAY
