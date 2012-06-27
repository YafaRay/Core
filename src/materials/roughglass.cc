/****************************************************************************
 * 		roughglass.cc: a dielectric material with rough surface
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
#include <materials/roughglass.h>
#include <core_api/environment.h>
#include <materials/microfacet.h>
#include <utilities/mcqmc.h>
#include <yafraycore/spectrum.h>
#include <iostream>

__BEGIN_YAFRAY


roughGlassMat_t::roughGlassMat_t(float IOR, color_t filtC, const color_t &srcol, bool fakeS, float alpha, float disp_pow):
		bumpS(0), mirColS(0), filterCol(filtC), specRefCol(srcol), ior(IOR), a2(alpha*alpha), a(alpha), absorb(false),
		disperse(false), fakeShadow(fakeS)
{
	bsdfFlags = BSDF_ALL_GLOSSY;
	if(fakeS) bsdfFlags |= BSDF_FILTER;
	if(disp_pow > 0.0)
	{
		disperse = true;
		CauchyCoefficients(IOR, disp_pow, CauchyA, CauchyB);
		bsdfFlags |= BSDF_DISPERSIVE;
	}
}

void roughGlassMat_t::initBSDF(const renderState_t &state, const surfacePoint_t &sp, BSDF_t &bsdfTypes) const
{
	nodeStack_t stack(state.userdata);
	if(bumpS) evalBump(stack, state, sp, bumpS);
	
	//eval viewindependent nodes
	std::vector<shaderNode_t *>::const_iterator iter, end=allViewindep.end();
	for(iter = allViewindep.begin(); iter!=end; ++iter) (*iter)->eval(stack, state, sp);
	bsdfTypes=bsdfFlags;
}

color_t roughGlassMat_t::sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W) const
{
	nodeStack_t stack(state.userdata);
	vector3d_t refdir, N = FACE_FORWARD(sp.Ng, sp.N, wo);
	bool outside = sp.Ng * wo > 0.f;

	s.pdf = 1.f;

	float alpha2 = a2;
	float cosTheta, tanTheta2;
	
	vector3d_t H(0.f);
	GGX_Sample(H, alpha2, s.s1, s.s2);
	H = H.x*sp.NU + H.y*sp.NV + H.z*N;
	H.normalize();
	
	float cur_ior = (disperse && state.chromatic) ? getIOR(state.wavelength, CauchyA, CauchyB) : ior;
	float glossy;
	float glossy_D = 0.f;
	float glossy_G = 0.f;
	float wiN, woN, wiH, woH;
	float Jacobian = 0.f;
	
	cosTheta = H * N;
	float cosTheta2 = cosTheta * cosTheta;
	tanTheta2 = (1.f - cosTheta2) / (cosTheta2 * 0.99f + 0.01f);

	if(cosTheta > 0.f) glossy_D = GGX_D(alpha2, cosTheta2, tanTheta2);

	woH = wo*H;
	woN = wo*N;

	float Kr, Kt;
	
	color_t ret(0.f);
	
	if(refractMicrofacet(((outside) ? 1.f / cur_ior : cur_ior), wo, wi, H, woH, woN, Kr, Kt) )
	{
		if(Kt > s.s1 && (s.flags & BSDF_TRANSMIT))
		{
			wiN = wi*N;
			wiH = wi*H;
			
			if((wiH*wiN) > 0.f && (woH*woN) > 0.f) glossy_G = GGX_G(alpha2, wiN, woN);

			float IORwi = 1.f;
			float IORwo = 1.f;

			if(outside)	IORwi = ior;
			else IORwo = ior;
			
			float ht = IORwo * woH + IORwi * wiH;
			Jacobian = (IORwi * IORwi) / (ht * ht);

			glossy = std::fabs( (woH * wiH) / (wiN * woN) ) * Kt * glossy_G * glossy_D * Jacobian;

			s.pdf = GGX_Pdf(glossy_D, cosTheta, Jacobian * std::fabs(wiH));
			s.sampledFlags = ((disperse && state.chromatic) ? BSDF_DISPERSIVE : BSDF_GLOSSY) | BSDF_TRANSMIT;

			ret = (glossy * filterCol);
			W = std::fabs(wiN) / (s.pdf * 0.99f + 0.01f);
		}
		else if(s.flags & BSDF_REFLECT)
		{
			reflectMicrofacet(wo, wi, H, woH);
			
			wiN = wi*N;
			wiH = wi*H;

			glossy_G = GGX_G(alpha2, wiN, woN);
			
			Jacobian = 1.f / ((4.f * std::fabs(wiH)) * 0.99f + 0.01f);
			glossy = (Kr * glossy_G * glossy_D) / ((4.f * std::fabs(woN * wiN) ) * 0.99f + 0.01f);
			
			s.pdf = GGX_Pdf(glossy_D, cosTheta, Jacobian);
			s.sampledFlags = BSDF_GLOSSY | BSDF_REFLECT;
			
			ret = (glossy * (mirColS ? mirColS->getColor(stack) : specRefCol));

			W = std::fabs(wiN) / (s.pdf * 0.99f + 0.01f);
		}
	}
	else // TIR
	{
		wi = wo;
		wi.reflect(H);
		s.sampledFlags = BSDF_GLOSSY | BSDF_REFLECT;
		ret = 1.f;
		W = 1.f;
	}

	return ret;
}

color_t roughGlassMat_t::getTransparency(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	float Kr, Kt;
	fresnel(wo, N, ior, Kr, Kt);
	return Kt*filterCol;
}

float roughGlassMat_t::getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	float alpha = std::max(0.f, std::min(1.f, 1.f - getTransparency(state, sp, wo).energy()));
	return alpha;
}

float roughGlassMat_t::getMatIOR() const
{
	return ior;
}

material_t* roughGlassMat_t::factory(paraMap_t &params, std::list< paraMap_t > &paramList, renderEnvironment_t &render)
{
	float IOR=1.4;
	float filt=0.f;
	float alpha = 0.5f;
	float disp_power=0.0;
	color_t filtCol(1.f), absorp(1.f), srCol(1.f);
	const std::string *name=0;
	bool fake_shad = false;
	params.getParam("IOR", IOR);
	params.getParam("filter_color", filtCol);
	params.getParam("transmit_filter", filt);
	params.getParam("mirror_color", srCol);
	params.getParam("alpha", alpha);
	params.getParam("dispersion_power", disp_power);
	params.getParam("fake_shadows", fake_shad);
	
	alpha = std::max(1e-6f, std::min(alpha, 1.f));
	
	roughGlassMat_t *mat = new roughGlassMat_t(IOR, filt*filtCol + color_t(1.f-filt), srCol, fake_shad, alpha, disp_power);
	
	if( params.getParam("absorption", absorp) )
	{
		double dist=1.f;
		if(absorp.R < 1.f || absorp.G < 1.f || absorp.B < 1.f)
		{
			//deprecated method:
			color_t sigma(0.f);
			if(params.getParam("absorption_dist", dist))
			{
				const float maxlog = log(1e38);
				sigma.R = (absorp.R > 1e-38) ? -log(absorp.R) : maxlog;
				sigma.G = (absorp.G > 1e-38) ? -log(absorp.G) : maxlog;
				sigma.B = (absorp.B > 1e-38) ? -log(absorp.B) : maxlog;
				if (dist!=0.f) sigma *= 1.f/dist;
			}
			mat->absorb = true;
			mat->beer_sigma_a = sigma;
			mat->bsdfFlags |= BSDF_VOLUMETRIC;
			// creat volume handler (backwards compatibility)
			if(params.getParam("name", name))
			{
				paraMap_t map;
				map["type"] = std::string("beer");
				map["absorption_col"] = absorp;
				map["absorption_dist"] = parameter_t(dist);
				mat->volI = render.createVolumeH(*name, map);
				mat->bsdfFlags |= BSDF_VOLUMETRIC;
			}
		}
	}

	std::vector<shaderNode_t *> roots;
	std::map<std::string, shaderNode_t *> nodeList;
	
	// Prepare our node list
	nodeList["mirror_color_shader"] = NULL;
	nodeList["bump_shader"] = NULL;
	
	if(mat->loadNodes(paramList, render))
	{
        mat->parseNodes(params, roots, nodeList);
	}
	else Y_ERROR << "RoughGlass: loadNodes() failed!" << yendl;

	mat->mirColS = nodeList["mirror_color_shader"];
	mat->bumpS = nodeList["bump_shader"];

	// solve nodes order
	if(!roots.empty())
	{
		mat->solveNodesOrder(roots);
		std::vector<shaderNode_t *> colorNodes;
		if(mat->mirColS) mat->getNodeList(mat->mirColS, colorNodes);
		mat->filterNodes(colorNodes, mat->allViewdep, VIEW_DEP);
		mat->filterNodes(colorNodes, mat->allViewindep, VIEW_INDEP);
		if(mat->bumpS)
		{
			mat->getNodeList(mat->bumpS, mat->bumpNodes);
		}
	}
	mat->reqMem = mat->reqNodeMem;

	return mat;
}

extern "C"
{
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("rough_glass", roughGlassMat_t::factory);
	}
}

__END_YAFRAY
