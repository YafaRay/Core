
#include <materials/shinydiff.h>
#include <utilities/sample_utils.h>
#include <materials/microfacet.h>

__BEGIN_YAFRAY

shinyDiffuseMat_t::shinyDiffuseMat_t(const color_t &col, const color_t &srcol, float diffuse, float transp, float transl, float sp_refl, float emit):
			isTranspar(false), isTransluc(false), isReflective(false), isDiffuse(false), fresnelEffect(false),
			diffuseS(0), bumpS(0), transpS(0), translS(0), specReflS(0), mirColS(0), color(col), specRefCol(srcol),
			mSpecRefl(sp_refl), mTransp(transp), mTransl(transl), mDiffuse(diffuse), orenNayar(false), nBSDF(0)
{
	emitCol = emit*col;
	emitVal = emit;
	mDiffuse = diffuse;
	bsdfFlags = BSDF_NONE;
	if(emitVal > 0.f) bsdfFlags |= BSDF_EMIT;
}

shinyDiffuseMat_t::~shinyDiffuseMat_t()
{
	// Empty
}

/*! ATTENTION! You *MUST* call this function before using the material, no matter
	if you want to use shaderNodes or not!
*/
void shinyDiffuseMat_t::config(shaderNode_t *diff, shaderNode_t *refl, shaderNode_t *transp, shaderNode_t *transl, shaderNode_t *bump)
{
	diffuseS = diff;
	bumpS = bump;
	transpS = transp;
	translS = transl;
	specReflS = refl;
	nBSDF=0;
	viNodes[0] = viNodes[1] = viNodes[2] = viNodes[3] = false;
	vdNodes[0] = vdNodes[1] = vdNodes[2] = vdNodes[3] = false;
	float acc = 1.f;
	if(mSpecRefl > 0.00001f || specReflS)
	{
		isReflective = true;
		if(specReflS){ if(specReflS->isViewDependant())vdNodes[0] = true; else viNodes[0] = true; }
		else if(!fresnelEffect) acc = 1.f - mSpecRefl;
		bsdfFlags |= BSDF_SPECULAR | BSDF_REFLECT;
		cFlags[nBSDF] = BSDF_SPECULAR | BSDF_REFLECT;
		cIndex[nBSDF] = 0;
		++nBSDF;
	}
	if(mTransp*acc > 0.00001f || transpS)
	{
		isTranspar = true;
		if(transpS){ if(transpS->isViewDependant())vdNodes[1] = true; else viNodes[1] = true; }
		else acc *= 1.f - mTransp;
		bsdfFlags |= BSDF_TRANSMIT | BSDF_FILTER;
		cFlags[nBSDF] = BSDF_TRANSMIT | BSDF_FILTER;
		cIndex[nBSDF] = 1;
		++nBSDF;
	}
	if(mTransl*acc > 0.00001f || translS)
	{
		isTransluc = true;
		if(translS){ if(translS->isViewDependant())vdNodes[2] = true; else viNodes[2] = true; }
		else acc *= 1.f - mTransp;
		bsdfFlags |= BSDF_DIFFUSE | BSDF_TRANSMIT;
		cFlags[nBSDF] = BSDF_DIFFUSE | BSDF_TRANSMIT;
		cIndex[nBSDF] = 2;
		++nBSDF;
	}
	if(mDiffuse*acc > 0.00001f)
	{
		isDiffuse = true;
		if(diffuseS){ if(diffuseS->isViewDependant())vdNodes[3] = true; else viNodes[3] = true; }
		bsdfFlags |= BSDF_DIFFUSE | BSDF_REFLECT;
		cFlags[nBSDF] = BSDF_DIFFUSE | BSDF_REFLECT;
		cIndex[nBSDF] = 3;
		++nBSDF;
	}
	reqMem = reqNodeMem + sizeof(SDDat_t);
}

// component should be initialized with mSpecRefl, mTransp, mTransl, mDiffuse
// since values for which useNode is false do not get touched so it can be applied
// twice, for view-independent (initBSDF) and view-dependent (sample/eval) nodes

int shinyDiffuseMat_t::getComponents(const bool *useNode, nodeStack_t &stack, float *component) const
{
	if(isReflective)
	{
		component[0] = useNode[0] ? specReflS->getScalar(stack) : mSpecRefl;
	}
	if(isTranspar)
	{
		component[1] = useNode[1] ? transpS->getScalar(stack) : mTransp;
	}
	if(isTransluc)
	{
		component[2] = useNode[2] ? translS->getScalar(stack) : mTransl;
	}
	if(isDiffuse)
	{
		component[3] = mDiffuse;
	}
	return 0;
}

inline void shinyDiffuseMat_t::getFresnel(const vector3d_t &wo, const vector3d_t &n, float &Kr) const
{
	Kr = 1.f;
	if(fresnelEffect)
	{
		vector3d_t N;

		if((wo*n) < 0.f)
		{
			N=-n;
		}
		else
		{
			N=n;
		}

		float c = wo*N;
		float g = IOR + c*c - 1.f;
		if(g < 0.f) g = 0.f;
		else g = fSqrt(g);
		float aux = c * (g+c);

		Kr = ( ( 0.5f * (g-c) * (g-c) )/( (g+c)*(g+c) ) ) *
			   ( 1.f + ((aux-1)*(aux-1))/((aux+1)*(aux+1)) );
	}
}

// calculate the absolute value of scattering components from the "normalized"
// fractions which are between 0 (no scattering) and 1 (scatter all remaining light)
// Kr is an optional reflection multiplier (e.g. from Fresnel)
static inline void accumulate(const float *component, float *accum, float Kr)
{
	accum[0] = component[0]*Kr;
	float acc = 1.f - accum[0];
	accum[1] = component[1] * acc;
	acc *= 1.f - component[1];
	accum[2] = component[2] * acc;
	acc *= 1.f - component[2];
	accum[3] = component[3] * acc;
}

void shinyDiffuseMat_t::initBSDF(const renderState_t &state, const surfacePoint_t &sp, BSDF_t &bsdfTypes)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	memset(dat, 0, 8*sizeof(float));
	dat->nodeStack = (char*)state.userdata + sizeof(SDDat_t);
	//create our "stack" to save node results
	nodeStack_t stack(dat->nodeStack);
	
	//bump mapping (extremely experimental)
	if(bumpS)
	{
		evalBump(stack, state, sp, bumpS);
	}
	
	//eval viewindependent nodes
	std::vector<shaderNode_t *>::const_iterator iter, end=allViewindep.end();
	for(iter = allViewindep.begin(); iter!=end; ++iter) (*iter)->eval(stack, state, sp);
	bsdfTypes=bsdfFlags;
	
	getComponents(viNodes, stack, dat->component);
}

void shinyDiffuseMat_t::initOrenNayar(double sigma)
{
	double sigma2 = sigma*sigma;
	A = 1.0 - 0.5*(sigma2 / (sigma2+0.33));
	B = 0.45 * sigma2 / (sigma2 + 0.09);
	orenNayar = true;
}

CFLOAT shinyDiffuseMat_t::OrenNayar(const vector3d_t &wi, const vector3d_t &wo, const vector3d_t &N) const
{
	PFLOAT cos_ti = std::max(-1.f,std::min(1.f,N*wi));
	PFLOAT cos_to = std::max(-1.f,std::min(1.f,N*wo));
	CFLOAT maxcos_f = 0.f;
	
	if(cos_ti < 0.9999f && cos_to < 0.9999f)
	{
		vector3d_t v1 = (wi - N*cos_ti).normalize();
		vector3d_t v2 = (wo - N*cos_to).normalize();
		maxcos_f = std::max(0.f, v1*v2);
	}
	
	CFLOAT sin_alpha, tan_beta;
	
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
	
	return A + B * maxcos_f * sin_alpha * tan_beta;
}


color_t shinyDiffuseMat_t::eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wl, BSDF_t bsdfs)const
{
	PFLOAT cos_Ng_wo = sp.Ng*wo;
	PFLOAT cos_Ng_wl = sp.Ng*wl;
	// face forward:
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	if(!(bsdfs & bsdfFlags & BSDF_DIFFUSE)) return color_t(0.f);
	
	SDDat_t *dat = (SDDat_t *)state.userdata;
	nodeStack_t stack(dat->nodeStack);
	
	float Kr;
	getFresnel(wo, N, Kr);
	float mT = (1.f - Kr*dat->component[0])*(1.f - dat->component[1]);
	
	bool transmit = ( cos_Ng_wo * cos_Ng_wl ) < 0.f;
	
	if(transmit) // light comes from opposite side of surface
	{
		if(isTransluc) return dat->component[2] * mT * (diffuseS ? diffuseS->getColor(stack) : color);
	}
	
	if(N*wl < 0.0) return color_t(0.f);
	float mD = mT*(1.f - dat->component[2]) * dat->component[3];
	if(orenNayar) mD *= OrenNayar(wo, wl, N);
	return mD * (diffuseS ? diffuseS->getColor(stack) : color);
}

color_t shinyDiffuseMat_t::emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	nodeStack_t stack(dat->nodeStack);
	
	return (diffuseS ? diffuseS->getColor(stack) * emitVal : emitCol);
}

color_t shinyDiffuseMat_t::sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const
{
	float accumC[4];
	PFLOAT cos_Ng_wo = sp.Ng*wo, cos_Ng_wi, cos_N;
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	
	SDDat_t *dat = (SDDat_t *)state.userdata;
	nodeStack_t stack(dat->nodeStack);

	float Kr;
	getFresnel(wo, N, Kr);
	accumulate(dat->component, accumC, Kr);

	float sum=0.f, val[4], width[4];
	BSDF_t choice[4];
	int nMatch=0, pick=-1;
	for(int i=0; i<nBSDF; ++i)
	{
		if((s.flags & cFlags[i]) == cFlags[i])
		{
			width[nMatch] = accumC[cIndex[i]];
			sum += width[nMatch];
			choice[nMatch] = cFlags[i];
			val[nMatch] = sum;
			++nMatch;
		}
	}
	if(!nMatch || sum < 0.00001){ s.sampledFlags=BSDF_NONE; s.pdf=0.f; return color_t(1.f); }
	float inv_sum = 1.f/sum;
	for(int i=0; i<nMatch; ++i)
	{
		val[i] *= inv_sum;
		width[i] *= inv_sum;
		if((s.s1 <= val[i]) && (pick<0 ))	pick = i;
	}
	if(pick<0) pick=nMatch-1;
	float s1;
	if(pick>0) s1 = (s.s1 - val[pick-1]) / width[pick];
	else 	   s1 = s.s1 / width[pick];
	
	color_t scolor(0.f);
	switch(choice[pick])
	{
		case (BSDF_SPECULAR | BSDF_REFLECT): // specular reflect
			wi = reflect_dir(N, wo);
			s.pdf = width[pick]; 
			scolor = (mirColS ? mirColS->getColor(stack) : specRefCol) * (accumC[0]);
			if(s.reverse)
			{
				s.pdf_back = s.pdf;
				s.col_back = scolor/std::fabs(sp.N*wo);
			}
			scolor *= 1.f/std::fabs(sp.N*wi);
			break;
		case (BSDF_TRANSMIT | BSDF_FILTER): // "specular" transmit
			wi = -wo;
			scolor = accumC[1] * (filter*(diffuseS ? diffuseS->getColor(stack) : color) + color_t(1.f-filter) );
			cos_N = std::fabs(wi*N);
			if(cos_N < 1e-6) s.pdf = 0.f;
			else s.pdf = width[pick];
			break;
		case (BSDF_DIFFUSE | BSDF_TRANSMIT): // translucency (diffuse transmitt)
			wi = SampleCosHemisphere(-N, sp.NU, sp.NV, s1, s.s2);
			cos_Ng_wi = sp.Ng*wi;
			if(cos_Ng_wo*cos_Ng_wi < 0) scolor = accumC[2] * (diffuseS ? diffuseS->getColor(stack) : color);
			s.pdf = std::fabs(wi*N) * width[pick]; break;
		case (BSDF_DIFFUSE | BSDF_REFLECT): // diffuse reflect
		default:
			wi = SampleCosHemisphere(N, sp.NU, sp.NV, s1, s.s2);
			cos_Ng_wi = sp.Ng*wi;
			if(cos_Ng_wo*cos_Ng_wi > 0) scolor = accumC[3] * (diffuseS ? diffuseS->getColor(stack) : color);
			if(orenNayar) scolor *= OrenNayar(wo, wi, N);
			s.pdf = std::fabs(wi*N) * width[pick]; break;
	}
	s.sampledFlags = choice[pick];
	W = (std::fabs(wi*sp.N))/(s.pdf*0.99f + 0.01f);
	return scolor;
}

float shinyDiffuseMat_t::pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const
{
	if(!(bsdfs & BSDF_DIFFUSE)) return 0.f;
	
	SDDat_t *dat = (SDDat_t *)state.userdata;
	float pdf=0.f;
	float accumC[4];
	PFLOAT cos_Ng_wo = sp.Ng*wo, cos_Ng_wi;
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	float Kr;
	getFresnel(wo, N, Kr);

	accumulate(dat->component, accumC, Kr);
	float sum=0.f, width;
	int nMatch=0;
	for(int i=0; i<nBSDF; ++i)
	{
		if((bsdfs & cFlags[i]))
		{
			width = accumC[cIndex[i]];
			sum += width;
			
			switch(cFlags[i])
			{
				case (BSDF_DIFFUSE | BSDF_TRANSMIT): // translucency (diffuse transmitt)
					cos_Ng_wi = sp.Ng*wi;
					if(cos_Ng_wo*cos_Ng_wi < 0) pdf += std::fabs(wi*N) * width;
					break;
				
				case (BSDF_DIFFUSE | BSDF_REFLECT): // lambertian
					cos_Ng_wi = sp.Ng*wi;
					pdf += std::fabs(wi*N) * width;
					break;
			}
			++nMatch;
		}
	}
	if(!nMatch || sum < 0.00001) return 0.f;
	return pdf / sum;
}



// todo!

void shinyDiffuseMat_t::getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo,
							  bool &reflect, bool &refract, vector3d_t *const dir, color_t *const col)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	nodeStack_t stack(dat->nodeStack);
	bool backface = sp.Ng * wo < 0;
	vector3d_t N = backface ? -sp.N : sp.N;
	vector3d_t Ng = backface ? -sp.Ng : sp.Ng;
	float Kr;
	getFresnel(wo, N, Kr);
	refract = isTranspar;
	if(isTranspar)
	{
		dir[1] = -wo;
		color_t tcol = filter * (diffuseS ? diffuseS->getColor(stack) : color) + color_t(1.f-filter);
		col[1] = (1.f - dat->component[0]*Kr) * dat->component[1] * tcol;
	}
	reflect=isReflective;
	if(isReflective)
	{
		dir[0] = reflect_plane(N, wo);
		PFLOAT cos_wi_Ng = dir[0]*Ng;
		if(cos_wi_Ng < 0.01){ dir[0] += (0.01-cos_wi_Ng)*Ng; dir[0].normalize(); }
		col[0] = (mirColS ? mirColS->getColor(stack) : specRefCol) * (dat->component[0]*Kr);
	}
}

color_t shinyDiffuseMat_t::getTransparency(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	nodeStack_t stack(state.userdata);
	std::vector<shaderNode_t *>::const_iterator iter, end=allSorted.end();
	for(iter = allSorted.begin(); iter!=end; ++iter) (*iter)->eval(stack, state, sp);
	float accum=1.f;
	float Kr;
	vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
	getFresnel(wo, N, Kr);

	if(isReflective)
	{
		accum = 1.f - Kr*(specReflS ? specReflS->getScalar(stack) : mSpecRefl);
	}
	if(isTranspar) //uhm...should actually be true if this function gets called anyway...
	{
		accum *= transpS ? transpS->getScalar(stack) * accum : mTransp * accum;
	}
	color_t tcol = filter * (diffuseS ? diffuseS->getColor(stack) : color) + color_t(1.f-filter);
	return accum * tcol;
}

CFLOAT shinyDiffuseMat_t::getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	if(isTranspar)
	{
		vector3d_t N = FACE_FORWARD(sp.Ng, sp.N, wo);
		float Kr;
		getFresnel(wo, N, Kr);
		CFLOAT refl = (1.f - dat->component[0]*Kr) * dat->component[1];
		return 1.f - refl;
	}
	return 1.f;
}

material_t* shinyDiffuseMat_t::factory(paraMap_t &params, std::list<paraMap_t> &eparams, renderEnvironment_t &render)
{
	shinyDiffuseMat_t *mat;
	color_t col=1.f, srCol=1.f;
	const std::string *name=0;
	float transp=0.f, emit=0.f, transl=0.f;
	float sp_refl=0.f;
	bool fresnEff=false;
	double IOR = 1.33, filt=1.0;
	CFLOAT diffuse=1.f;
	//bool error=false;
	params.getParam("color", col);
	params.getParam("mirror_color", srCol);
	params.getParam("transparency", transp);
	params.getParam("translucency", transl);
	params.getParam("diffuse_reflect", diffuse);
	params.getParam("specular_reflect", sp_refl);
	params.getParam("emit", emit);
	params.getParam("IOR", IOR);
	params.getParam("fresnel_effect", fresnEff);
	params.getParam("transmit_filter", filt);
	// !!remember to put diffuse multiplier in material itself!
	mat = new shinyDiffuseMat_t(col, srCol, diffuse, transp, transl, sp_refl, emit);
	mat->filter = filt;
	
	if(fresnEff)
	{
		mat->IOR = IOR * IOR;
		mat->fresnelEffect = true;
	}
	if(params.getParam("diffuse_brdf", name))
	{
		if(*name == "oren_nayar")
		{
			double sigma=0.1;
			params.getParam("sigma", sigma);
			mat->initOrenNayar(sigma);
		}
	}
	shaderNode_t *diffuseS=0, *bumpS=0, *specReflS=0, *transpS=0, *translS=0;
	std::vector<shaderNode_t *> roots;
	// create shader nodes:
	bool success = mat->loadNodes(eparams, render);
	if(success)
	{
		if(params.getParam("diffuse_shader", name))
		{
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ diffuseS = i->second; roots.push_back(diffuseS); }
			else Y_WARNING << "ShinyDiffuse: Diffuse shader node '" << *name << "' does not exist!" << yendl;
		}
		if(params.getParam("mirror_color_shader", name))
		{
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mat->mirColS = i->second; roots.push_back(mat->mirColS); }
			else Y_WARNING << "ShinyDiffuse: Mirror color shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("bump_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Bump shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ bumpS = i->second; roots.push_back(bumpS); }
			else Y_WARNING << "ShinyDiffuse: bump shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("mirror_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Mirror shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ specReflS = i->second; roots.push_back(specReflS); }
			else Y_WARNING << "ShinyDiffuse: mirror shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("transparency_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Transparency shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ transpS = i->second; roots.push_back(transpS); }
			else Y_WARNING << "ShinyDiffuse: transparency shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("translucency_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Translucency shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ translS = i->second; roots.push_back(translS); }
			else Y_WARNING << "ShinyDiffuse: transparency shader node '"<<*name<<"' does not exist!" << yendl;
		}
	}
	else Y_WARNING << "ShinyDiffuse: Creating nodes failed!" << yendl;

	// solve nodes order
	if(!roots.empty())
	{
		mat->solveNodesOrder(roots);
		Y_INFO << "ShinyDiffuse: Evaluation order:" << yendl;

		std::vector<shaderNode_t *> colorNodes;

		if(diffuseS) mat->getNodeList(diffuseS, colorNodes);
		if(mat->mirColS) mat->getNodeList(mat->mirColS, colorNodes);
		if(specReflS) mat->getNodeList(specReflS, colorNodes);
		if(transpS) mat->getNodeList(transpS, colorNodes);
		if(translS) mat->getNodeList(translS, colorNodes);

		mat->filterNodes(colorNodes, mat->allViewdep, VIEW_DEP);
		mat->filterNodes(colorNodes, mat->allViewindep, VIEW_INDEP);

		if(bumpS)
		{
			mat->getNodeList(bumpS, mat->bumpNodes);
		}
	}
	mat->config(diffuseS, specReflS, transpS, translS, bumpS);

	//===!!!=== test <<< This test should go, is useless, DT
	if(params.getParam("name", name))
	{
		if(name->substr(0, 6) == "MAsss_")
		{
			paraMap_t map;
			map["type"] = std::string("sss");
			map["absorption_col"] = color_t(0.5f, 0.2f, 0.2f);
			map["absorption_dist"] = 0.5f;
			map["scatter_col"] = color_t(0.9f);
			mat->volI = render.createVolumeH(*name, map);
			mat->bsdfFlags |= BSDF_VOLUMETRIC;
		}
	}
	//===!!!=== end of test

	return mat;
}

extern "C"
{
	
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("shinydiffusemat", shinyDiffuseMat_t::factory);
	}

}

__END_YAFRAY
