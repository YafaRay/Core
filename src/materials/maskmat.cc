/****************************************************************************
 * 			maskmat.cc: a material that combines 2 materials with a mask
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
 
#include <yafray_config.h>
#include <materials/maskmat.h>
//#include <core_api/material.h>
#include <core_api/texture.h>
#include <yafraycore/nodematerial.h>
#include <core_api/environment.h>
//#include <utilities/sample_utils.h>


__BEGIN_YAFRAY

maskMat_t::maskMat_t(const material_t *m1, const material_t *m2, CFLOAT thresh):
	mat1(m1), mat2(m2), threshold(thresh)
{
	bsdfFlags = mat1->getFlags() | mat2->getFlags();
}

#define PTR_ADD(ptr,sz) ((char*)ptr+(sz))
void maskMat_t::initBSDF(const renderState_t &state, const surfacePoint_t &sp, BSDF_t &bsdfTypes)const
{
	nodeStack_t stack(state.userdata);
	evalNodes(state, sp, allNodes, stack);
	CFLOAT val = mask->getScalar(stack); //mask->getFloat(sp.P);
	bool mv = val > threshold;
	*(bool*)state.userdata = mv;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) mat2->initBSDF(state, sp, bsdfTypes);
	else   mat1->initBSDF(state, sp, bsdfTypes);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
}

color_t maskMat_t::eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const
{
	bool mv = *(bool*)state.userdata;
	color_t col;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) col = mat2->eval(state, sp, wo, wi, bsdfs);
	else   col = mat1->eval(state, sp, wo, wi, bsdfs);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
	return col;
}

color_t maskMat_t::sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const
{
	bool mv = *(bool*)state.userdata;
	color_t col;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) col = mat2->sample(state, sp, wo, wi, s, W);
	else   col = mat1->sample(state, sp, wo, wi, s, W);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
	return col;
}

float maskMat_t::pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const
{
	bool mv = *(bool*)state.userdata;
	float pdf;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) pdf = mat2->pdf(state, sp, wo, wi, bsdfs);
	else   pdf = mat1->pdf(state, sp, wo, wi, bsdfs);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
	return pdf;
}

bool maskMat_t::isTransparent() const
{
	return mat1->isTransparent() || mat2->isTransparent();
}

color_t maskMat_t::getTransparency(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	nodeStack_t stack(state.userdata);
	evalNodes(state, sp, allNodes, stack);
	CFLOAT val = mask->getScalar(stack);
	bool mv = val > 0.5;
	if(mv) return mat2->getTransparency(state, sp, wo);
	else   return mat1->getTransparency(state, sp, wo);
}

void maskMat_t::getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo,
								 bool &reflect, bool &refract, vector3d_t *const dir, color_t *const col)const
{
	bool mv = *(bool*)state.userdata;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) mat2->getSpecular(state, sp, wo, reflect, refract, dir, col);
	else   mat1->getSpecular(state, sp, wo, reflect, refract, dir, col);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
}

color_t maskMat_t::emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	bool mv = *(bool*)state.userdata;
	color_t col;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) col = mat2->emit(state, sp, wo);
	else   col = mat1->emit(state, sp, wo);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
	return col;
}

CFLOAT maskMat_t::getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	bool mv = *(bool*)state.userdata;
	CFLOAT alpha;
	state.userdata = PTR_ADD(state.userdata, sizeof(bool));
	if(mv) alpha = mat2->getAlpha(state, sp, wo);
	else   alpha = mat1->getAlpha(state, sp, wo);
	state.userdata = PTR_ADD(state.userdata, -sizeof(bool));
	return alpha;
}

material_t* maskMat_t::factory(paraMap_t &params, std::list< paraMap_t > &eparams, renderEnvironment_t &env)
{
	const std::string *name = 0;
	const material_t *m1=0, *m2=0;
	double thresh = 0.5;
	
	params.getParam("threshold", thresh);
	if(! params.getParam("material1", name) ) return 0;
	m1 = env.getMaterial(*name);
	if(! params.getParam("material2", name) ) return 0;
	m2 = env.getMaterial(*name);
	//if(! params.getParam("mask", name) ) return 0;
	//mask = env.getTexture(*name);
	
	if(m1==0 || m2==0 ) return 0;
	
	maskMat_t *mat = new maskMat_t(m1, m2, thresh);
	
	std::vector<shaderNode_t *> roots;
	if(mat->loadNodes(eparams, env))
	{
		if(params.getParam("mask", name))
		{
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->mShadersTable.find(*name);
			if(i!=mat->mShadersTable.end()){ mat->mask = i->second; roots.push_back(mat->mask); }
			else
			{
				Y_ERROR << "MaskMat: Mask shader node '" << *name << "' does not exist!" << yendl;
				delete mat;
				return 0;
			}
		}
	}
	else
	{
		Y_ERROR << "MaskMat: loadNodes() failed!" << yendl;
		delete mat;
		return 0;
	}
	mat->solveNodesOrder(roots);
	size_t inputReq = std::max(m1->getReqMem(), m2->getReqMem());
	mat->reqMem = std::max( mat->reqNodeMem, sizeof(bool) + inputReq);
	return mat;
}

__END_YAFRAY
