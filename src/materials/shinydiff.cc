
#include <materials/shinydiff.h>
#include <utilities/sample_utils.h>
#include <materials/microfacet.h>

__BEGIN_YAFRAY

shinyDiffuseMat_t::shinyDiffuseMat_t(const color_t &col, const color_t &srcol, float diffuse, float transp, float transl, float sp_refl, float emit):
			mIsTransparent(false), mIsTranslucent(false), mIsMirror(false), mIsDiffuse(false), mHasFresnelEffect(false),
			mDiffuseShader(0), mBumpShader(0), mTransparencyShader(0), mTranslucencyShader(0), mMirrorShader(0), mMirrorColorShader(0), mDiffuseColor(col), mMirrorColor(srcol),
			mMirrorStrength(sp_refl), mTransparencyStrength(transp), mTranslucencyStrength(transl), mDiffuseStrength(diffuse), mUseOrenNayar(false), nBSDF(0)
{
	mEmitColor = emit*col;
	mEmitStrength = emit;
	bsdfFlags = BSDF_NONE;
	if(mEmitStrength > 0.f) bsdfFlags |= BSDF_EMIT;
}

shinyDiffuseMat_t::~shinyDiffuseMat_t()
{
	// Empty
}

/*! ATTENTION! You *MUST* call this function before using the material, no matter
	if you want to use shaderNodes or not!
*/
void shinyDiffuseMat_t::config(shaderNode_t *diffuseShader, shaderNode_t *mirrorShader, shaderNode_t *transparencyShader, shaderNode_t *translucencyShader, shaderNode_t *bumpShader)
{
	mDiffuseShader      = diffuseShader;
	mBumpShader         = bumpShader;
	mTransparencyShader = transparencyShader;
	mTranslucencyShader = translucencyShader;
	mMirrorShader       = mirrorShader;

	nBSDF=0;
	viNodes[0] = viNodes[1] = viNodes[2] = viNodes[3] = false;
	vdNodes[0] = vdNodes[1] = vdNodes[2] = vdNodes[3] = false;
	float acc = 1.f;
	if(mMirrorStrength > 0.00001f || mMirrorShader)
	{
		mIsMirror = true;
		if(mMirrorShader){ if(mMirrorShader->isViewDependant())vdNodes[0] = true; else viNodes[0] = true; }
		else if(!mHasFresnelEffect) acc = 1.f - mMirrorStrength;
		bsdfFlags |= BSDF_SPECULAR | BSDF_REFLECT;
		cFlags[nBSDF] = BSDF_SPECULAR | BSDF_REFLECT;
		cIndex[nBSDF] = 0;
		++nBSDF;
	}
	if(mTransparencyStrength*acc > 0.00001f || mTransparencyShader)
	{
		mIsTransparent = true;
		if(mTransparencyShader){ if(mTransparencyShader->isViewDependant())vdNodes[1] = true; else viNodes[1] = true; }
		else acc *= 1.f - mTransparencyStrength;
		bsdfFlags |= BSDF_TRANSMIT | BSDF_FILTER;
		cFlags[nBSDF] = BSDF_TRANSMIT | BSDF_FILTER;
		cIndex[nBSDF] = 1;
		++nBSDF;
	}
	if(mTranslucencyStrength*acc > 0.00001f || mTranslucencyShader)
	{
		mIsTranslucent = true;
		if(mTranslucencyShader){ if(mTranslucencyShader->isViewDependant())vdNodes[2] = true; else viNodes[2] = true; }
		else acc *= 1.f - mTransparencyStrength;
		bsdfFlags |= BSDF_DIFFUSE | BSDF_TRANSMIT;
		cFlags[nBSDF] = BSDF_DIFFUSE | BSDF_TRANSMIT;
		cIndex[nBSDF] = 2;
		++nBSDF;
	}
	if(mDiffuseStrength*acc > 0.00001f)
	{
		mIsDiffuse = true;
		if(mDiffuseShader){ if(mDiffuseShader->isViewDependant())vdNodes[3] = true; else viNodes[3] = true; }
		bsdfFlags |= BSDF_DIFFUSE | BSDF_REFLECT;
		cFlags[nBSDF] = BSDF_DIFFUSE | BSDF_REFLECT;
		cIndex[nBSDF] = 3;
		++nBSDF;
	}
	reqMem = reqNodeMem + sizeof(SDDat_t);
}

// component should be initialized with mMirrorStrength, mTransparencyStrength, mTranslucencyStrength, mDiffuseStrength
// since values for which useNode is false do not get touched so it can be applied
// twice, for view-independent (initBSDF) and view-dependent (sample/eval) nodes

int shinyDiffuseMat_t::getComponents(const bool *useNode, nodeStack_t &stack, float *component) const
{
	if(mIsMirror)
	{
		component[0] = useNode[0] ? mMirrorShader->getScalar(stack) : mMirrorStrength;
	}
	if(mIsTransparent)
	{
		component[1] = useNode[1] ? mTransparencyShader->getScalar(stack) : mTransparencyStrength;
	}
	if(mIsTranslucent)
	{
		component[2] = useNode[2] ? mTranslucencyShader->getScalar(stack) : mTranslucencyStrength;
	}
	if(mIsDiffuse)
	{
		component[3] = mDiffuseStrength;
	}
	return 0;
}

inline void shinyDiffuseMat_t::getFresnel(const vector3d_t &wo, const vector3d_t &n, float &Kr) const
{
	if(mHasFresnelEffect)
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
		float g = mIOR_Squared + c*c - 1.f;
		if(g < 0.f) g = 0.f;
		else g = fSqrt(g);
		float aux = c * (g+c);

		Kr = ( ( 0.5f * (g-c) * (g-c) )/( (g+c)*(g+c) ) ) *
			   ( 1.f + ((aux-1)*(aux-1))/((aux+1)*(aux+1)) );
	}
	else
	{
		Kr = 1.f;
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
	if(mBumpShader)
	{
		evalBump(stack, state, sp, mBumpShader);
	}
	
	//eval viewindependent nodes
	std::vector<shaderNode_t *>::const_iterator iter, end=allViewindep.end();
	for(iter = allViewindep.begin(); iter!=end; ++iter) (*iter)->eval(stack, state, sp);
	bsdfTypes=bsdfFlags;
	
	getComponents(viNodes, stack, dat->component);
}

/** Initialize Oren Nayar reflectance.
 *  Initialize Oren Nayar A and B coefficient.
 *  @param	sigma Roughness of the surface
 */
void shinyDiffuseMat_t::initOrenNayar(double sigma)
{
	double sigma_squared = sigma * sigma;
	mOrenNayar_A = 1.0 - 0.5 * (sigma_squared / (sigma_squared + 0.33));
	mOrenNayar_B = 0.45 * sigma_squared / (sigma_squared + 0.09);
	mUseOrenNayar = true;
}

/** Calculate Oren Nayar reflectance.
 *  Calculate Oren Nayar reflectance for a given reflection.
 *  @param	wi Reflected ray direction
 *  @param	wo Incident ray direction
 *  @param	N  Surface normal
 *  @note	http://en.wikipedia.org/wiki/Oren-Nayar_reflectance_model
 */
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
	
	return mOrenNayar_A + mOrenNayar_B * maxcos_f * sin_alpha * tan_beta;
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
		if(mIsTranslucent) return dat->component[2] * mT * (mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor);
	}
	
	if(N*wl < 0.0) return color_t(0.f);
	float mD = mT*(1.f - dat->component[2]) * dat->component[3];
	if(mUseOrenNayar) mD *= OrenNayar(wo, wl, N);
	return mD * (mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor);
}

color_t shinyDiffuseMat_t::emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	nodeStack_t stack(dat->nodeStack);
	
	return (mDiffuseShader ? mDiffuseShader->getColor(stack) * mEmitStrength : mEmitColor);
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
			scolor = (mMirrorColorShader ? mMirrorColorShader->getColor(stack) : mMirrorColor) * (accumC[0]);
			if(s.reverse)
			{
				s.pdf_back = s.pdf;
				s.col_back = scolor/std::fabs(sp.N*wo);
			}
			scolor *= 1.f/std::fabs(sp.N*wi);
			break;
		case (BSDF_TRANSMIT | BSDF_FILTER): // "specular" transmit
			wi = -wo;
			scolor = accumC[1] * (mTransmitFilterStrength*(mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor) + color_t(1.f-mTransmitFilterStrength) );
			cos_N = std::fabs(wi*N);
			if(cos_N < 1e-6) s.pdf = 0.f;
			else s.pdf = width[pick];
			break;
		case (BSDF_DIFFUSE | BSDF_TRANSMIT): // translucency (diffuse transmitt)
			wi = SampleCosHemisphere(-N, sp.NU, sp.NV, s1, s.s2);
			cos_Ng_wi = sp.Ng*wi;
			if(cos_Ng_wo*cos_Ng_wi < 0) scolor = accumC[2] * (mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor);
			s.pdf = std::fabs(wi*N) * width[pick]; break;
		case (BSDF_DIFFUSE | BSDF_REFLECT): // diffuse reflect
		default:
			wi = SampleCosHemisphere(N, sp.NU, sp.NV, s1, s.s2);
			cos_Ng_wi = sp.Ng*wi;
			if(cos_Ng_wo*cos_Ng_wi > 0) scolor = accumC[3] * (mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor);
			if(mUseOrenNayar) scolor *= OrenNayar(wo, wi, N);
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


/** Perfect specular reflection.
 *  Calculate perfect specular reflection and refraction from the material for
 *  a given surface point \a sp and a given incident ray direction \a wo
 *  @param	state Render state
 *  @param	sp Surface point
 *  @param	wo Incident ray direction
 *  @param	doReflect Boolean value which is true if you have a reflection, false otherwise
 *  @param	doRefract Boolean value which is true if you have a refraction, false otherwise
 *  @param	wi Array of two vectors to record reflected ray direction (wi[0]) and refracted ray direction (wi[1])
 *  @param	col Array of two colors to record reflected ray color (col[0]) and refracted ray color (col[1])
 */
void shinyDiffuseMat_t::getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, bool &doReflect, bool &doRefract, vector3d_t *const wi, color_t *const col)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	nodeStack_t stack(dat->nodeStack);

	const bool backface = wo * sp.Ng < 0.f;
	const vector3d_t N  = backface ? -sp.N  : sp.N;
	const vector3d_t Ng = backface ? -sp.Ng : sp.Ng;

	float Kr;
	getFresnel(wo, N, Kr);

	if(mIsTransparent)
	{
		doRefract = true;
		wi[1] = -wo;
		color_t tcol = mTransmitFilterStrength * (mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor) + color_t(1.f-mTransmitFilterStrength);
		col[1] = (1.f - dat->component[0]*Kr) * dat->component[1] * tcol;
	}
	else
	{
		doRefract = false;
	}

	if(mIsMirror)
	{
		doReflect = true;
		wi[0] = wo;
		wi[0].reflect(N);
		PFLOAT cos_wi_Ng = wi[0]*Ng;
		if(cos_wi_Ng < 0.01)
		{
			wi[0] += (0.01-cos_wi_Ng)*Ng;
			wi[0].normalize();
		}
		col[0] = (mMirrorColorShader ? mMirrorColorShader->getColor(stack) : mMirrorColor) * (dat->component[0]*Kr);
	}
	else
	{
		doReflect = false;
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

	if(mIsMirror)
	{
		accum = 1.f - Kr*(mMirrorShader ? mMirrorShader->getScalar(stack) : mMirrorStrength);
	}
	if(mIsTransparent) //uhm...should actually be true if this function gets called anyway...
	{
		accum *= mTransparencyShader ? mTransparencyShader->getScalar(stack) * accum : mTransparencyStrength * accum;
	}
	color_t tcol = mTransmitFilterStrength * (mDiffuseShader ? mDiffuseShader->getColor(stack) : mDiffuseColor) + color_t(1.f-mTransmitFilterStrength);
	return accum * tcol;
}

CFLOAT shinyDiffuseMat_t::getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const
{
	SDDat_t *dat = (SDDat_t *)state.userdata;
	if(mIsTransparent)
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
	float transparency=0.f, emit=0.f, translucency=0.f;
	float sp_refl=0.f;
	bool fresnEff=false;
	double IOR = 1.33, filt=1.0;
	CFLOAT diffuse=1.f;
	//bool error=false;
	params.getParam("color", col);
	params.getParam("mirror_color", srCol);
	params.getParam("transparency", transparency);
	params.getParam("translucency", translucency);
	params.getParam("diffuse_reflect", diffuse);
	params.getParam("specular_reflect", sp_refl);
	params.getParam("emit", emit);
	params.getParam("IOR", IOR);
	params.getParam("fresnel_effect", fresnEff);
	params.getParam("transmit_filter", filt);
	// !!remember to put diffuse multiplier in material itself!
	mat = new shinyDiffuseMat_t(col, srCol, diffuse, transparency, translucency, sp_refl, emit);
	mat->mTransmitFilterStrength = filt;
	
	if(fresnEff)
	{
		mat->mIOR_Squared = IOR * IOR;
		mat->mHasFresnelEffect = true;
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
	shaderNode_t *mDiffuseShader=0, *mBumpShader=0, *mMirrorShader=0, *mTransparencyShader=0, *mTranslucencyShader=0;
	std::vector<shaderNode_t *> roots;
	// create shader nodes:
	bool success = mat->loadNodes(eparams, render);
	if(success)
	{
		if(params.getParam("diffuse_shader", name))
		{
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mDiffuseShader = i->second; roots.push_back(mDiffuseShader); }
			else Y_WARNING << "ShinyDiffuse: Diffuse shader node '" << *name << "' does not exist!" << yendl;
		}
		if(params.getParam("mirror_color_shader", name))
		{
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mat->mMirrorColorShader = i->second; roots.push_back(mat->mMirrorColorShader); }
			else Y_WARNING << "ShinyDiffuse: Mirror color shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("bump_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Bump shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mBumpShader = i->second; roots.push_back(mBumpShader); }
			else Y_WARNING << "ShinyDiffuse: bump shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("mirror_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Mirror shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mMirrorShader = i->second; roots.push_back(mMirrorShader); }
			else Y_WARNING << "ShinyDiffuse: mirror shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("transparency_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Transparency shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mTransparencyShader = i->second; roots.push_back(mTransparencyShader); }
			else Y_WARNING << "ShinyDiffuse: transparency shader node '"<<*name<<"' does not exist!" << yendl;
		}
		if(params.getParam("translucency_shader", name))
		{
			Y_INFO << "ShinyDiffuse: Translucency shader: " << name << yendl;
			std::map<std::string,shaderNode_t *>::const_iterator i=mat->shader_table.find(*name);
			if(i!=mat->shader_table.end()){ mTranslucencyShader = i->second; roots.push_back(mTranslucencyShader); }
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

		if(mDiffuseShader) mat->getNodeList(mDiffuseShader, colorNodes);
		if(mat->mMirrorColorShader) mat->getNodeList(mat->mMirrorColorShader, colorNodes);
		if(mMirrorShader) mat->getNodeList(mMirrorShader, colorNodes);
		if(mTransparencyShader) mat->getNodeList(mTransparencyShader, colorNodes);
		if(mTranslucencyShader) mat->getNodeList(mTranslucencyShader, colorNodes);

		mat->filterNodes(colorNodes, mat->allViewdep, VIEW_DEP);
		mat->filterNodes(colorNodes, mat->allViewindep, VIEW_INDEP);

		if(mBumpShader)
		{
			mat->getNodeList(mBumpShader, mat->bumpNodes);
		}
	}
	mat->config(mDiffuseShader, mMirrorShader, mTransparencyShader, mTranslucencyShader, mBumpShader);

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
