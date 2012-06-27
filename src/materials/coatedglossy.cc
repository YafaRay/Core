/****************************************************************************
 * 			coatedglossy.cc: a glossy material with specular coating
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
 *      Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
 
#include <yafray_config.h>
#include <yafraycore/nodematerial.h>
#include <core_api/environment.h>
#include <utilities/sample_utils.h>
#include <materials/microfacet.h>


__BEGIN_YAFRAY

/*! Coated Glossy Material.
	A Material with Phong/Anisotropic Phong microfacet base layer and a layer of
	(dielectric) perfectly specular coating. This is to simulate surfaces like
	metallic paint */

#define C_SPECULAR 	0
#define C_GLOSSY  	1
#define C_DIFFUSE 	2

class coatedGlossyMat_t: public nodeMaterial_t
{
	public:
		coatedGlossyMat_t(const color_t &col, const color_t &dcol, const color_t &mirCol, float reflect, float diff, PFLOAT ior, float expo, bool as_diff);
		virtual void initBSDF(const renderState_t &state, const surfacePoint_t &sp, BSDF_t &bsdfTypes)const;
		virtual color_t eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const;
		virtual color_t sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const;
		virtual float pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const;
		virtual void getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo,
								 bool &refl, bool &refr, vector3d_t *const dir, color_t *const col)const;
		static material_t* factory(paraMap_t &, std::list< paraMap_t > &, renderEnvironment_t &);
		
		struct MDat_t
		{
			float mDiffuse, mGlossy, pDiffuse;
			void *stack;
		};

		void initOrenNayar(double sigma);
		
	private:
		float OrenNayar(const vector3d_t &wi, const vector3d_t &wo, const vector3d_t &N) const;

	protected:
		shaderNode_t* diffuseS;
		shaderNode_t* glossyS;
		shaderNode_t* glossyRefS;
		shaderNode_t* bumpS;
		color_t gloss_color, diff_color, mirror_color; //!< color of glossy base
		PFLOAT IOR;
		float exponent, exp_u, exp_v;
		float reflectivity;
		CFLOAT mGlossy, mDiffuse;
		bool as_diffuse, with_diffuse, anisotropic;
		BSDF_t specFlags, glossyFlags;
		BSDF_t cFlags[3];
		int nBSDF;
		bool orenNayar;
		float orenA, orenB;
};

coatedGlossyMat_t::coatedGlossyMat_t(const color_t &col, const color_t &dcol, const color_t &mirCol, float reflect, float diff, PFLOAT ior, float expo, bool as_diff):
	diffuseS(0), glossyS(0), glossyRefS(0), bumpS(0), gloss_color(col), diff_color(dcol), mirror_color(mirCol), IOR(ior), exponent(expo), reflectivity(reflect), mDiffuse(diff),
	as_diffuse(as_diff), with_diffuse(false), anisotropic(false)
{
	cFlags[C_SPECULAR] = (BSDF_SPECULAR | BSDF_REFLECT);
	cFlags[C_GLOSSY] = as_diffuse? (BSDF_DIFFUSE | BSDF_REFLECT) : (BSDF_GLOSSY | BSDF_REFLECT);

	if(diff>0)
	{
		cFlags[C_DIFFUSE] = BSDF_DIFFUSE | BSDF_REFLECT;
		with_diffuse = true;
		nBSDF = 3;
	}
	else
	{
		cFlags[C_DIFFUSE] = BSDF_NONE;
		nBSDF = 2;
	}

	orenNayar = false;

	bsdfFlags = cFlags[C_SPECULAR] | cFlags[C_GLOSSY] | cFlags[C_DIFFUSE];
}

void coatedGlossyMat_t::initBSDF(const renderState_t &state, const surfacePoint_t &sp, BSDF_t &bsdfTypes)const
{
	MDat_t *dat = (MDat_t *)state.userdata;
	dat->stack = (char*)state.userdata + sizeof(MDat_t);
	nodeStack_t stack(dat->stack);
	if(bumpS) evalBump(stack, state, sp, bumpS);
	
	std::vector<shaderNode_t *>::const_iterator iter, end=allViewindep.end();
	for(iter = allViewindep.begin(); iter!=end; ++iter) (*iter)->eval(stack, state, sp);
	bsdfTypes=bsdfFlags;
	dat->mDiffuse = mDiffuse;
	dat->mGlossy = glossyRefS ? glossyRefS->getScalar(stack) : reflectivity;
	dat->pDiffuse = std::min(0.6f , 1.f - (dat->mGlossy/(dat->mGlossy + (1.f-dat->mGlossy)*dat->mDiffuse)) );
}

void coatedGlossyMat_t::initOrenNayar(double sigma)
{
	double sigma2 = sigma*sigma;
	orenA = 1.0 - 0.5*(sigma2 / (sigma2+0.33));
	orenB = 0.45 * sigma2 / (sigma2 + 0.09);
	orenNayar = true;
}

float coatedGlossyMat_t::OrenNayar(const vector3d_t &wi, const vector3d_t &wo, const vector3d_t &N) const
{
	float cos_ti = std::max(-1.f,std::min(1.f,N*wi));
	float cos_to = std::max(-1.f,std::min(1.f,N*wo));
	float maxcos_f = 0.f;
	
	if(cos_ti < 0.9999f && cos_to < 0.9999f)
	{
		vector3d_t v1 = (wi - N*cos_ti).normalize();
		vector3d_t v2 = (wo - N*cos_to).normalize();
		maxcos_f = std::max(0.f, v1*v2);
	}
	
	float sin_alpha, tan_beta;
	
	if(cos_to >= cos_ti)
	{
		sin_alpha = fSqrt(1.f - cos_ti*cos_ti);
		tan_beta = fSqrt(1.f - cos_to*cos_to) / ((cos_to == 0.f)?1e-8f:cos_to); // white (black on windows) dots fix for oren-nayar, could happen with bad normals
	}
	else
	{
		sin_alpha = fSqrt(1.f - cos_to*cos_to);
		tan_beta = fSqrt(1.f - cos_ti*cos_ti) / ((cos_ti == 0.f)?1e-8f:cos_ti); // white (black on windows) dots fix for oren-nayar, could happen with bad normals
	}
	
	return orenA + orenB * maxcos_f * sin_alpha * tan_beta;
}

color_t coatedGlossyMat_t::eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const
{
	MDat_t *dat = (MDat_t *)state.userdata;
	color_t col(0.f);
	bool diffuse_flag = bsdfs & BSDF_DIFFUSE;
	
	if( !diffuse_flag || ((sp.Ng*wi)*(sp.Ng*wo)) < 0.f ) return col;
	
	nodeStack_t stack(dat->stack);
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	float Kr, Kt;
	float wiN = std::fabs(wi * N);
	float woN = std::fabs(wo * N);
	
	fresnel(wo, N, IOR, Kr, Kt);
	
	if( (as_diffuse && diffuse_flag) || (!as_diffuse && (bsdfs & BSDF_GLOSSY)) )
	{
		vector3d_t H = (wo + wi).normalize(); // half-angle
		float cos_wi_H = wi*H;
		float glossy;
		if(anisotropic)
		{
			vector3d_t Hs(H*sp.NU, H*sp.NV, H*N);
			glossy = Kt * AS_Aniso_D(Hs, exp_u, exp_v) * SchlickFresnel(cos_wi_H, dat->mGlossy) / ASDivisor(cos_wi_H, woN, wiN);
		}
		else
		{
			glossy = Kt * Blinn_D(H*N, exponent) * SchlickFresnel(cos_wi_H, dat->mGlossy) / ASDivisor(cos_wi_H, woN, wiN);
		}
		col = (CFLOAT)glossy*(glossyS ? glossyS->getColor(stack) : gloss_color);
	}
	if(with_diffuse && diffuse_flag)
	{
		col += diffuseReflectFresnel(wiN, woN, dat->mGlossy, dat->mDiffuse, (diffuseS ? diffuseS->getColor(stack) : diff_color), Kt) * ((orenNayar)?OrenNayar(wi, wo, N):1.f);
	}
	return col;
}

color_t coatedGlossyMat_t::sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const
{
	MDat_t *dat = (MDat_t *)state.userdata;
	float cos_Ng_wo = sp.Ng*wo;
	float cos_Ng_wi;
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	vector3d_t Hs(0.f);
	s.pdf = 0.f;
	float Kr, Kt;
	float wiN = 0.f , woN = 0.f;
	
	fresnel(wo, N, IOR, Kr, Kt);
	
	// missing! get components
	nodeStack_t stack(dat->stack);
	bool use[3] = {false, false, false};
	float sum = 0.f, accumC[3], val[3], width[3];
	int cIndex[3]; // entry values: 0 := specular part, 1 := glossy part, 2:= diffuse part;
	int rcIndex[3]; // reverse fmapping of cIndex, gives position of spec/glossy/diff in val/width array
	accumC[0] = Kr;
	accumC[1] = Kt*(1.f - dat->pDiffuse);
	accumC[2] = Kt*(dat->pDiffuse);
	
	int nMatch = 0, pick = -1;
	for(int i = 0; i < nBSDF; ++i)
	{
		if((s.flags & cFlags[i]) == cFlags[i])
		{
			use[i] = true;
			width[nMatch] = accumC[i];
			cIndex[nMatch] = i;
			rcIndex[i] = nMatch;
			sum += width[nMatch];
			val[nMatch] = sum;
			++nMatch;
		}
	}
	if(!nMatch || sum < 0.00001){ return color_t(0.f); }
	else if(nMatch==1){ pick=0; width[0]=1.f; }
	else
	{
		float inv_sum = 1.f/sum;
		for(int i=0; i<nMatch; ++i)
		{
			val[i] *= inv_sum;
			width[i] *= inv_sum;
			if((s.s1 <= val[i]) && (pick<0 ))	pick = i;
		}
	}
	if(pick<0) pick=nMatch-1;
	float s1;
	if(pick>0) s1 = (s.s1 - val[pick-1]) / width[pick];
	else 	   s1 = s.s1 / width[pick];
	
	color_t scolor(0.f);
	switch(cIndex[pick])
	{
		case C_SPECULAR: // specular reflect
			wi = reflect_dir(N, wo);
			scolor = mirror_color * Kr;//)/std::fabs(N*wi);
			s.pdf = width[pick];
			if(s.reverse)
			{
				s.pdf_back = s.pdf; // mirror is symmetrical
				s.col_back = mirror_color * Kr;//)/std::fabs(N*wo);
			}
			break;
		case C_GLOSSY: // glossy
			if(anisotropic) AS_Aniso_Sample(Hs, s1, s.s2, exp_u, exp_v);
			else 			Blinn_Sample(Hs, s1, s.s2, exponent);
			break;
		case C_DIFFUSE: // lambertian
		default:
			wi = SampleCosHemisphere(N, sp.NU, sp.NV, s1, s.s2);
			cos_Ng_wi = sp.Ng*wi;
			if(cos_Ng_wo*cos_Ng_wi < 0) return color_t(0.f);
	}

	wiN = std::fabs(wi * N);
	woN = std::fabs(wo * N);
	
	if(cIndex[pick] != C_SPECULAR)
	{
		// evaluate BSDFs and PDFs...
		if(use[C_GLOSSY])
		{
			float glossy;
			float cos_wo_H;
			vector3d_t H(0.f);
			if(cIndex[pick] != C_GLOSSY)
			{
				H = (wi+wo).normalize();
				Hs = vector3d_t(H*sp.NU, H*sp.NV, H*N);
				cos_wo_H = wo*H;
			}
			else
			{
				H = Hs.x*sp.NU + Hs.y*sp.NV + Hs.z*N;
				cos_wo_H = wo*H;
				if ( cos_wo_H < 0.f )
				{
					H.reflect(N);
					cos_wo_H = wo*H;
				}
				// Compute incident direction by reflecting wo about H
				wi = reflect_dir(H, wo);
				cos_Ng_wi = sp.Ng*wi;
				if(cos_Ng_wo*cos_Ng_wi < 0) return color_t(0.f);
			}
			
			wiN = std::fabs(wi * N);
			
			if(anisotropic)
			{
				s.pdf += AS_Aniso_Pdf(Hs, cos_wo_H, exp_u, exp_v) * width[rcIndex[C_GLOSSY]];
				glossy = AS_Aniso_D(Hs, exp_u, exp_v) * SchlickFresnel(cos_wo_H, dat->mGlossy) / ASDivisor(cos_wo_H, woN, wiN);
			}
			else
			{
				float cosHN = H*N;
				s.pdf += Blinn_Pdf(cosHN, cos_wo_H, exponent) * width[rcIndex[C_GLOSSY]];
				glossy = Blinn_D(cosHN, exponent) * SchlickFresnel(cos_wo_H, dat->mGlossy) / ASDivisor(cos_wo_H, woN, wiN);
			}
			scolor = (CFLOAT)glossy*Kt*(glossyS ? glossyS->getColor(stack) : gloss_color);
		}
		
		if(use[C_DIFFUSE])
		{
			scolor += diffuseReflectFresnel(wiN, woN, dat->mGlossy, dat->mDiffuse, (diffuseS ? diffuseS->getColor(stack) : diff_color), Kt) * ((orenNayar)?OrenNayar(wi, wo, N):1.f);
			s.pdf += wiN * width[rcIndex[C_DIFFUSE]];
		}
		W = wiN / (s.pdf * 0.99f + 0.01f);
	}
	else
	{
		W = 1.f;
	}
	
	s.sampledFlags = cFlags[cIndex[pick]];
	
	return scolor;
}

float coatedGlossyMat_t::pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t flags)const
{
	MDat_t *dat = (MDat_t *)state.userdata;
	bool transmit = ((sp.Ng * wo) * (sp.Ng * wi)) < 0.f;
	if(transmit) return 0.f;
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	float pdf = 0.f;
	float Kr, Kt;
	
	fresnel(wo, N, IOR, Kr, Kt);
	
	float accumC[3], sum=0.f, width;
	accumC[0] = Kr;
	accumC[1] = Kt*(1.f - dat->pDiffuse);
	accumC[2] = Kt*(dat->pDiffuse);
	
	int nMatch=0;
	for(int i=0; i<nBSDF; ++i)
	{
		if((flags & cFlags[i]) == cFlags[i])
		{
			width = accumC[i];
			sum += width;
			if(i == C_GLOSSY)
			{
				vector3d_t H = (wi+wo).normalize();
				float cos_wo_H = wo*H;
				float cos_N_H = N*H;
				if(anisotropic)
				{
					vector3d_t Hs(H*sp.NU, H*sp.NV, cos_N_H);
					pdf += AS_Aniso_Pdf(Hs, cos_wo_H, exp_u, exp_v) * width;
				}
				else pdf += Blinn_Pdf(cos_N_H, cos_wo_H, exponent) * width;
			}
			else if(i == C_DIFFUSE)
			{
				pdf += std::fabs(wi*N) * width;
			}
			++nMatch;
		}
	}
	if(!nMatch || sum < 0.00001) return 0.f;
	return pdf / sum;
}

void coatedGlossyMat_t::getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo,
							 bool &refl, bool &refr, vector3d_t *const dir, color_t *const col)const
{
	bool outside = sp.Ng*wo >= 0;
	vector3d_t N, Ng;
	float cos_wo_N = sp.N*wo;
	if(outside)
	{
		N = (cos_wo_N >= 0) ? sp.N : (sp.N - (1.00001*cos_wo_N)*wo).normalize();
		Ng = sp.Ng;
	}
	else
	{
		N = (cos_wo_N <= 0) ? sp.N : (sp.N - (1.00001*cos_wo_N)*wo).normalize();
		Ng = -sp.Ng;
	}
	
	float Kr, Kt;
	fresnel(wo, N, IOR, Kr, Kt);
	
	refr = false;
	
	if(state.raylevel > 5) return;
	
	dir[0] = wo;
	dir[0].reflect(N);
	col[0] = Kr * mirror_color;
	float cos_wi_Ng = dir[0]*Ng;
	if(cos_wi_Ng < 0.01)
	{
		dir[0] += (0.01-cos_wi_Ng)*Ng;
		dir[0].normalize();
	}
	refl = true;
}

material_t* coatedGlossyMat_t::factory(paraMap_t &params, std::list< paraMap_t > &paramList, renderEnvironment_t &render)
{
	color_t col(1.f), dcol(1.f), mirCol(1.f);
	float refl=1.f;
	float diff=0.f;
	float exponent=50.f; //wild guess, do sth better
	double ior=1.4;
	bool as_diff=true;
	bool aniso=false;
	const std::string *name=0;

	params.getParam("color", col);
	params.getParam("diffuse_color", dcol);
	params.getParam("diffuse_reflect", diff);
	params.getParam("glossy_reflect", refl);
	params.getParam("as_diffuse", as_diff);
	params.getParam("exponent", exponent);
	params.getParam("anisotropic", aniso);
	params.getParam("IOR", ior);
	params.getParam("mirror_color", mirCol);
	
	if(ior == 1.f) ior = 1.0000001f;

	coatedGlossyMat_t *mat = new coatedGlossyMat_t(col, dcol, mirCol, refl, diff, ior, exponent, as_diff);
	if(aniso)
	{
		double e_u=50.0, e_v=50.0;
		params.getParam("exp_u", e_u);
		params.getParam("exp_v", e_v);
		mat->anisotropic = true;
		mat->exp_u = e_u;
		mat->exp_v = e_v;
	}
	
	if(params.getParam("diffuse_brdf", name))
	{
		if(*name == "Oren-Nayar")
		{
			double sigma=0.1;
			params.getParam("sigma", sigma);
			mat->initOrenNayar(sigma);
		}
	}
	
	std::vector<shaderNode_t *> roots;
	std::map<std::string, shaderNode_t *> nodeList;
	
	// Prepare our node list
	nodeList["diffuse_shader"] = NULL;
	nodeList["glossy_shader"] = NULL;
	nodeList["glossy_reflect_shader"] = NULL;
	nodeList["bump_shader"] = NULL;
	
	if(mat->loadNodes(paramList, render))
	{
        mat->parseNodes(params, roots, nodeList);
	}
	else Y_ERROR << "CoatedGlossy: loadNodes() failed!" << yendl;

	mat->diffuseS = nodeList["diffuse_shader"];
	mat->glossyS = nodeList["glossy_shader"];
	mat->glossyRefS = nodeList["glossy_reflect_shader"];
	mat->bumpS = nodeList["bump_shader"];

	// solve nodes order
	if(!roots.empty())
	{
		mat->solveNodesOrder(roots);
		std::vector<shaderNode_t *> colorNodes;
		if(mat->diffuseS) mat->getNodeList(mat->diffuseS, colorNodes);
		if(mat->glossyS) mat->getNodeList(mat->glossyS, colorNodes);
		if(mat->glossyRefS) mat->getNodeList(mat->glossyRefS, colorNodes);
		mat->filterNodes(colorNodes, mat->allViewdep, VIEW_DEP);
		mat->filterNodes(colorNodes, mat->allViewindep, VIEW_INDEP);
		if(mat->bumpS) mat->getNodeList(mat->bumpS, mat->bumpNodes);
	}
	mat->reqMem = mat->reqNodeMem + sizeof(MDat_t);
	return mat;
}

extern "C"
{	
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("coated_glossy", coatedGlossyMat_t::factory);
	}
}

__END_YAFRAY
