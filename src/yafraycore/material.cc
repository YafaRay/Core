/****************************************************************************
 * 			material.cc: default implementations for materials
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
#include <core_api/scene.h> 
#include <utilities/sample_utils.h>
#include <utilities/mcqmc.h>
#include <yafraycore/scr_halton.h>

__BEGIN_YAFRAY

bool material_t::scatterPhoton(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wi, vector3d_t &wo, pSample_t &s) const
{
	float W = 0.f;
	color_t scol = sample(state, sp, wi, wo, s, W);
	if(s.pdf > 1.0e-6f)
	{
		color_t cnew = s.lcol * s.alpha * scol * W;
		CFLOAT new_max = cnew.maximum();
		CFLOAT old_max = s.lcol.maximum();
		float prob = std::min(1.f, new_max/old_max);
		if(s.s3 <= prob)
		{
			s.color = cnew / prob;
			return true;
		}
	}
	return false;
}

color_t material_t::getReflectivity(const renderState_t &state, const surfacePoint_t &sp, BSDF_t flags)const
{
	if(! (flags & (BSDF_TRANSMIT | BSDF_REFLECT) & bsdfFlags) ) return color_t(0.f);
	float s1, s2, s3, s4, W = 0.f;
	color_t total(0.f), col;
	vector3d_t wi, wo;
	for(int i=0; i<16; ++i)
	{
		s1 = 0.03125 + 0.0625 * (float)i; // (1.f/32.f) + (1.f/16.f)*(float)i;
		s2 = RI_vdC(i);
		s3 = scrHalton(2, i);
		s4 = scrHalton(3, i);
		wo = SampleCosHemisphere(sp.N, sp.NU, sp.NV, s1, s2);
		sample_t s(s3, s4, flags);
		col = sample(state, sp, wo, wi, s, W);
		total += col * W;
	}
	return total * 0.0625; //total / 16.f
}
		

void material_t::applyBump(surfacePoint_t &sp, PFLOAT dfdNU, PFLOAT dfdNV) const
{
	sp.NU += dfdNU * sp.N;
	sp.NV += dfdNV * sp.N;
	sp.N = (sp.NU ^ sp.NV).normalize();
	sp.NU.normalize();
	sp.NV = (sp.N ^ sp.NU).normalize();
}

__END_YAFRAY
