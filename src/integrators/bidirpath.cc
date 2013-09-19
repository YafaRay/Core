
//#include <mcqmc.h>
#include <core_api/environment.h>
#include <core_api/material.h>
#include <core_api/tiledintegrator.h>
#include <core_api/background.h>
#include <core_api/light.h>
#include <core_api/camera.h>
#include <core_api/imagefilm.h>
#include <integrators/integr_utils.h>
#include <utilities/mcqmc.h>

__BEGIN_YAFRAY

/*  conventions:
    y_0 := point on light source
    z_0 := point on camera lens
    x_0...x_k := path vertices, while x_0...x_s-1 are y_0...y_s-1 and x_k...x_s are z_0...z_t-1
    so x_i <=> z_k-i, for i>=s
    */


#define MAX_PATH_LENGTH 32
#define MIN_PATH_LENGTH 3

#define _BIDIR_DEBUG 0
#define _DO_LIGHTIMAGE 1

/*! class that holds some vertex y_i/z_i (depending on wether it is a light or camera path)
*/
class pathVertex_t
{
public:
	surfacePoint_t sp;  //!< surface point at which the path vertex lies
	BSDF_t flags;       //!< flags of the sampled BSDF component (not all components of the sp!)
	color_t alpha;      //!< cumulative subpath weight; note that y_i/z_i stores alpha_i+1 !
	color_t f_s;        //!< f(x_i-1, x_i, x_i+1), i.e. throughput from last to next path vertex
	vector3d_t wi, wo;  //!< sampled direction for next vertex (if available)
	float ds;           //!< squared distance between x_i-1 and x_i
	float G;            //!< geometric factor G(x_i-1, x_i), required for MIS
	float qi_wo;        //!< russian roulette probability for terminating path
	float qi_wi;        //!< russian roulette probability for terminating path when generating path in opposite direction
	float cos_wi, cos_wo; //!< (absolute) cosine of the incoming (wi) and sampled (wo) path direction
	float pdf_wi, pdf_wo; //!< the pdf for sampling wi from wo and wo from wi respectively
	void *userdata;     //!< user data of the material at sp (required for sampling and evaluating)
};

/*! vertices of a connected path going forward from light to eye;
    conventions: path vertices are named x_0...x_k, with k=s+t-1 again.
    x_0 lies on the light source, x_k on the camera */
struct pathEvalVert_t
{
	bool specular; //!< indicate that the ingoing direction determines the outgoing one (and vice versa)
	union
	{
		float pdf_f;    //!< pdf of sampling forward direction (x_i->x_i+1) given the backward direction
		float pdf_A_k;  //!< in case of lense vertex we have area pdf here, there is no forward path segment
	};
	union
	{
		float pdf_b;    //!< pdf of sampling backward direction (x_i-1->x_i) given forward direction
		float pdf_A_0;  //!< in case of light vertex we have area pdf here, there is no backward path segment
	};
	float G; //!< geometric term G(x_i-1, x_i)
};


void clear_path(std::vector<pathEvalVert_t> &p, int s, int t)
{
#if _BIDIR_DEBUG
	for(int i=0; i<s+t; ++i)
	{
		pathEvalVert_t &v = p[i];
		v.pdf_f = -1.f;
		v.pdf_b = -1.f;
		v.G = -1.f;
		v.specular = false;
	}
#endif
}

void check_path(std::vector<pathEvalVert_t> &p, int s, int t)
{
#if _BIDIR_DEBUG
	for(int i=0; i<s+t; ++i)
	{
		pathEvalVert_t &v = p[i];
		if(v.pdf_f == -1.f) std::cout << "path[" << i << "].pdf_f uninitialized! (s="<<s<<" t="<<t<<")\n";
		if(v.pdf_b == -1.f)  std::cout << "path[" << i << "].pdf_b uninitialized! (s="<<s<<" t="<<t<<")\n";
		if(v.G == -1.f)  std::cout << "path[" << i << "].G uninitialized! (s="<<s<<" t="<<t<<")\n";
	}
#endif
}

/*! holds eye and light path, aswell as data for connection (s,t),
    i.e. connection of light vertex y_s with eye vertex z_t */
class pathData_t
{
public:
	std::vector<pathVertex_t> lightPath, eyePath;
	std::vector<pathEvalVert_t> path;
	//pathCon_t pc;
	// additional information for current path connection:
	vector3d_t w_l_e;       //!< direction of edge from light to eye vertex, i.e. y_s to z_t
	color_t f_y, f_z;       //!< f for light and eye vertex that are connected
	PFLOAT u, v;            //!< current position on image plane
	float d_yz;             //!< distance between y_s to z_t
	const light_t *light;   //!< the light source to which the current path is connected
	//float pdf_Ad_0;       //!< pdf for direct lighting strategy
	float pdf_emit, pdf_illum;  //!< light pdfs required to calculate p1 for direct lighting strategy
	bool singularL;         //!< true if light has zero area (point lights for example)
	int nPaths;             //!< number of paths that have been sampled (for current thread and image)
};

class YAFRAYPLUGIN_EXPORT biDirIntegrator_t: public tiledIntegrator_t
{
public:
	biDirIntegrator_t(bool transpShad=false, int shadowDepth=4);
	virtual ~biDirIntegrator_t();
	virtual bool preprocess();
	virtual void cleanup();
	virtual colorA_t integrate(renderState_t &state, diffRay_t &ray) const;
	static integrator_t* factory(paraMap_t &params, renderEnvironment_t &render);
protected:
	int createPath(renderState_t &state, ray_t &start, std::vector<pathVertex_t> &path, int maxLen) const;
	color_t evalPath(renderState_t &state, int s, int t, pathData_t &pd) const;
	color_t evalLPath(renderState_t &state, int t, pathData_t &pd, ray_t &lRay, const color_t &lcol) const;
	color_t evalPathE(renderState_t &state, int s, pathData_t &pd) const;
	bool connectPaths(renderState_t &state, int s, int t, pathData_t &pd) const;
	bool connectLPath(renderState_t &state, int t, pathData_t &pd, ray_t &lRay, color_t &lcol) const;
	bool connectPathE(renderState_t &state, int s, pathData_t &pd) const;
	//color_t estimateOneDirect(renderState_t &state, const surfacePoint_t &sp, vector3d_t wo, pathCon_t &pc)const;
	CFLOAT pathWeight(renderState_t &state, int s, int t, pathData_t &pd) const;
	CFLOAT pathWeight_0t(renderState_t &state, int t, pathData_t &pd) const;

	background_t *background;
	const camera_t *cam;
	bool trShad;        //!< calculate transparent shadows for transparent objects
	bool use_bg;        //!< configuration; include background for GI
	bool ibl;           //!< configuration; use background light, if available
	bool include_bg;    //!< determined on precrocess;
	int sDepth, rDepth, bounces;
	std::vector<light_t*> lights;
	//mutable std::vector<pathVertex_t> lightPath, eyePath;
	//mutable int nPaths;
	//mutable pathData_t pathData;
	mutable std::vector<pathData_t> threadData;
	pdf1D_t *lightPowerD;
	float fNumLights;
	std::map <const light_t*, CFLOAT> invLightPowerD;
	imageFilm_t *lightImage;
};

biDirIntegrator_t::biDirIntegrator_t(bool transpShad, int shadowDepth): trShad(transpShad), sDepth(shadowDepth),
	lightPowerD(0), lightImage(0)
{
	type = SURFACE;
	integratorName = "BidirectionalPathTracer";
	integratorShortName = "BdPT";
	settings = "";
}

biDirIntegrator_t::~biDirIntegrator_t()
{
	//done in cleanup() now...
	//for(unsigned int i=0; i<pathData.lightPath.size(); ++i) free(pathData.lightPath[i].userdata);
	//for(unsigned int i=0; i<pathData.eyePath.size(); ++i) free(pathData.eyePath[i].userdata);
}

bool biDirIntegrator_t::preprocess()
{
	background = scene->getBackground();
	lights = scene->lights;

	threadData.resize(scene->getNumThreads());
	for(int t=0; t<scene->getNumThreads(); ++t)
	{
		pathData_t &pathData = threadData[t];
		pathData.eyePath.resize(MAX_PATH_LENGTH);
		pathData.lightPath.resize(MAX_PATH_LENGTH);
		pathData.path.resize(MAX_PATH_LENGTH*2 + 1);
		for(int i=0; i<MAX_PATH_LENGTH; ++i) pathData.lightPath[i].userdata = malloc(USER_DATA_SIZE);
		for(int i=0; i<MAX_PATH_LENGTH; ++i) pathData.eyePath[i].userdata = malloc(USER_DATA_SIZE);
		pathData.nPaths = 0;
	}
	// initialize userdata (todo!)
	int numLights = lights.size();
	fNumLights = 1.f / (float) numLights;
	float *energies = new float[numLights];
	for(int i=0; i<numLights; ++i) energies[i] = lights[i]->totalEnergy().energy();
	lightPowerD = new pdf1D_t(energies, numLights);

	for(int i=0; i<numLights; ++i) invLightPowerD[lights[i]] = lightPowerD->func[i] * lightPowerD->invIntegral;

	for(int i=0; i<numLights; ++i) std::cout << energies[i] << " (" << lightPowerD->func[i] << ") ";
	std::cout << "\n== preprocess(): lights: " << numLights << " invIntegral:" << lightPowerD->invIntegral << std::endl;

	delete[] energies;

	cam = scene->getCamera();
	//nPaths = 0;
	lightImage = scene->getImageFilm();// new imageFilm_t(cam->resX(), cam->resY(), 0, 0, *lightOut, 1.5f);
	lightImage->setDensityEstimation(true);
	//lightImage->setInteractive(false);
	//lightImage->init();

	// test...
	/*
	PFLOAT wt, u, v;
	float pdf;
	ray_t wo = cam->shootRay(10.25, 10.25, 0, 0, wt);
	bool proj = cam->project(wo, 0, 0, u, v, pdf);
	std::cout << "camera u=" << u << " v=" << v << " pdf=" << pdf << " (returned " << proj << ")" << std::endl;
	float integral = 0.f;
	for(int i=0; i<10000; ++i)
	{
	    wo.dir = SampleSphere((float)i/10000.f, RI_vdC(i));
	    if( cam->project(wo, 0, 0, u, v, pdf) ) integral += pdf;
	}
	std::cout << "Camera pdf integral: " << integral/10000.f << std::endl;


	//test...
	lSample_t ls;
	float Apdf, dirPdf, cos_wo;
	surfacePoint_t sp;
	ls.s1=.5f, ls.s2=.5f, ls.s3=.5f, ls.s4=.5f;
	ls.sp = &sp;
	color_t pcol = lights[0]->emitSample(wo.dir, ls);
	integral = 0.f;
	for(int i=0; i<10000; ++i)
	{
	    wo.dir = SampleSphere((float)i/10000.f, RI_vdC(i));
	    lights[0]->emitPdf(sp, wo.dir, Apdf, dirPdf, cos_wo);
	    integral += dirPdf;
	}
	std::cout << "Light pdf integral: " << integral/10000.f << std::endl;
	*/

	return true;
}

void biDirIntegrator_t::cleanup()
{
//	std::cout << "cleanup: flushing light image" << std::endl;
	int nPaths=0;
	for(int i=0; i<(int)threadData.size(); ++i)
	{
		pathData_t &pathData = threadData[i];
		nPaths += pathData.nPaths;
		for(int i=0; i<MAX_PATH_LENGTH; ++i) free(pathData.lightPath[i].userdata);
		for(int i=0; i<MAX_PATH_LENGTH; ++i) free(pathData.eyePath[i].userdata);
	}
	lightImage->setNumSamples(nPaths); //dirty hack...
}

/* ============================================================
    integrate
 ============================================================ */
colorA_t biDirIntegrator_t::integrate(renderState_t &state, diffRay_t &ray) const
{
	color_t col(0.f);
	surfacePoint_t sp;
	ray_t testray = ray;

	if(scene->intersect(testray, sp))
	{
		static int dbg=0;
		state.includeLights = true;
		pathData_t &pathData = threadData[state.threadID];
		++pathData.nPaths;
		random_t &prng = *(state.prng);
		pathVertex_t &ve = pathData.eyePath.front();
		pathVertex_t &vl = pathData.lightPath.front();
		int nEye=1, nLight=1;
		// setup ve
		ve.f_s = color_t(1.f); // some random guess...need to read up on importance paths
		ve.alpha = color_t(1.f);
		ve.sp.P = ray.from;
		ve.qi_wo = ve.qi_wi = 1.f; // definitely no russian roulette here...
		// temporary!
		PFLOAT cu, cv;
		float camPdf = 0.0;
		cam->project(ray, 0, 0, cu, cv, camPdf);
		ve.pdf_wo = camPdf;
		ve.f_s = color_t(camPdf);
		// /temporary
		ve.cos_wo = 1.f;
		//ve.pdf_wo = 1.f;
		ve.pdf_wi = 1.f;
		ve.flags = BSDF_DIFFUSE; //place holder! not applicable for e.g. orthogonal camera!

		// create eyePath
		nEye = createPath(state, ray, pathData.eyePath, MAX_PATH_LENGTH);

		// sample light (todo!)
		ray_t lray;
		lray.tmin = MIN_RAYDIST;
		lray.tmax = -1.f;
		float lightNumPdf;
		int lightNum = lightPowerD->DSample(prng(), &lightNumPdf);
		lightNumPdf *= fNumLights;
		lSample_t ls;
		ls.s1=prng(), ls.s2=prng(), ls.s3=prng(), ls.s4=prng();
		ls.sp = &vl.sp;
		color_t pcol = lights[lightNum]->emitSample(lray.dir, ls);
		lray.from = vl.sp.P;
		// test!
		ls.areaPdf *= lightNumPdf;

		if(dbg<10) std::cout << "lightNumPdf=" << lightNumPdf << std::endl;
		++dbg;

		// setup vl
		vl.f_s = color_t(1.f); // veach set this to L_e^(1)(y0->y1), a BSDF like value; not available yet, cancels out anyway when using direct lighting
		vl.alpha = pcol/ls.areaPdf; // as above, this should not contain the "light BSDF"...missing lightNumPdf!
		vl.G = 0.f; //unused actually...
		vl.qi_wo = vl.qi_wi = 1.f; // definitely no russian roulette here...
		vl.cos_wo = (ls.flags & LIGHT_SINGULAR) ? 1.0 : std::fabs(vl.sp.N * lray.dir); //singularities have no surface, hence no normal
		vl.cos_wi = 1.f;
		vl.pdf_wo = ls.dirPdf;
		vl.pdf_wi = ls.areaPdf; //store area PDF here, so we don't need extra members just for camera/eye vertices
		vl.flags = ls.flags; //store light flags in BSDF flags...same purpose though, check if delta function are involved
		pathData.singularL = (ls.flags & LIGHT_SINGULAR);

		// create lightPath
		nLight = createPath(state, lray, pathData.lightPath, MAX_PATH_LENGTH);
		if(nLight>1)
		{
			pathData.pdf_illum = lights[lightNum]->illumPdf(pathData.lightPath[1].sp, vl.sp) * lightNumPdf;
			pathData.pdf_emit = ls.areaPdf * pathData.lightPath[1].ds / vl.cos_wo;
		}

		// do bidir evalulation

#if _DO_LIGHTIMAGE
		// TEST! create a light image (t == 1)
		for(int s=2; s<=nLight; ++s)
		{
			clear_path(pathData.path, s, 1);
			if(!connectPathE(state, s, pathData)) continue;
			check_path(pathData.path, s, 1);
			CFLOAT wt = pathWeight(state, s, 1, pathData);
			if(wt > 0.f)
			{
				color_t li_col = evalPathE(state, s, pathData);
				if(li_col.isBlack()) continue;
				PFLOAT ix, idx, iy, idy;
				idx = std::modf(pathData.u, &ix);
				idy = std::modf(pathData.v, &iy);
				lightImage->addDensitySample(li_col, ix, iy, idx, idy);
			}
		}
#endif

		CFLOAT wt;
		for(int t=2; t<=nEye; ++t)
		{
			//directly hit a light?
			if(pathData.eyePath[t-1].sp.light)
			{
				// pathWeight_0t computes required probabilities, since no connection is required
				clear_path(pathData.path, 0, t);
				// unless someone proves the necessity, directly visible lights (s+t==2) will never be connected via light vertices
				wt = (t==2) ? 1.f : pathWeight_0t(state, t, pathData);
				if(wt > 0.f)
				{
					//eval is done in place here...
					pathVertex_t v = pathData.eyePath[t-1];
					state.userdata = v.userdata;
					color_t emit = v.sp.material->emit(state, v.sp, v.wi);
					col += wt * v.alpha * emit;
				}
			}
			// direct lighting strategy (desperately needs adaption...):
			ray_t dRay;
			color_t dcol;
			clear_path(pathData.path, 1, t);
			bool o_singularL = pathData.singularL;  // will be overwritten from connectLPath...
			float o_pdf_illum = pathData.pdf_illum; // will be overwritten from connectLPath...
			float o_pdf_emit = pathData.pdf_emit;   // will be overwritten from connectLPath...
			if(connectLPath(state, t, pathData, dRay, dcol))
			{
				check_path(pathData.path, 1, t);
				wt = pathWeight(state, 1, t, pathData);
				if(wt > 0.f)
				{
					col += wt * evalLPath(state, t, pathData, dRay, dcol);
				}
			}
			pathData.singularL = o_singularL;
			pathData.pdf_illum = o_pdf_illum;
			pathData.pdf_emit = o_pdf_emit;
			// light paths with one vertices are handled by classic direct light sampling (like regular path tracing)
			// hence we start with s=2 here. currently the sampling probability is the same though, so weights are unaffected
			pathData.light = lights[lightNum];
			for(int s=2; s<=nLight; ++s)
			{
				clear_path(pathData.path, s, t);
				if(!connectPaths(state, s, t, pathData)) continue;
				check_path(pathData.path, s, t);
				wt = pathWeight(state, s, t, pathData);
				if(wt > 0.f)
				{
					col += wt * evalPath(state, s, t, pathData);
				}
			}
		}
	}
	else
	{
		if(background)
		{
			col += (*background)(ray, state, false);
		}
	}
	return col;
}

/* ============================================================
    createPath: create (sub-)path from given starting point
    important: resize path to maxLen *before* calling this function!
 ============================================================ */

int biDirIntegrator_t::createPath(renderState_t &state, ray_t &start, std::vector<pathVertex_t> &path, int maxLen) const
{
	static int dbg=0;
	random_t &prng = *state.prng;
	ray_t ray(start);
	BSDF_t mBSDF;
	// the 0th vertex has already been generated, which is ray.pos obviously
	int nVert = 1;
	while(nVert < maxLen)
	{
		pathVertex_t &v = path[nVert];
		const pathVertex_t &v_prev = path[nVert-1];
		if(!scene->intersect(ray, v.sp)) break;
		const material_t *mat = v.sp.material;
		// compute alpha_i+1 = alpha_i * fs(wi, wo) / P_proj(wo), where P_proj = bsdf_pdf(wo) / cos(wo*N)
		v.alpha = v_prev.alpha * v_prev.f_s * v_prev.cos_wo / (v_prev.pdf_wo * v_prev.qi_wo);
		v.wi = -ray.dir;
		v.cos_wi = std::fabs(ray.dir * v.sp.N);
		v.ds = (v.sp.P - v_prev.sp.P).lengthSqr();
		v.G = v_prev.cos_wo * v.cos_wi / v.ds;
		++nVert;
		state.userdata = v.userdata;
        //if(dbg<10) std::cout << nVert << "  mat: " << (void*) mat << " alpha:" << v.alpha << " p_f_s:" << v_prev.f_s << " qi:"<< v_prev.qi << std::endl;
		mat->initBSDF(state, v.sp, mBSDF);
		// create tentative sample for next path segment
		sample_t s(prng(), prng(), BSDF_ALL, true);
		float W = 0.f;
		v.f_s = mat->sample(state, v.sp, v.wi, ray.dir, s, W);
		if(v.f_s.isBlack()) break;
		v.pdf_wo = s.pdf;
		v.cos_wo = W * s.pdf;
		// use russian roulette on tentative sample to decide on path termination, unless path is too short
		if(nVert > MIN_PATH_LENGTH)
		{
			v.qi_wo = std::min( 0.98f, v.f_s.col2bri()*v.cos_wo / v.pdf_wo );
			if(prng() > v.qi_wo) break; // terminate path with russian roulette
		}
		else v.qi_wo = 1.f;

		if(s.sampledFlags & BSDF_SPECULAR) // specular surfaces need special treatment...
		{
			// other materials don't return pdf_back and col_back yet
			//v.pdf_wi = s.pdf;
			v.pdf_wi = s.pdf_back;
			v.qi_wi = std::min( 0.98f, s.col_back.col2bri()*v.cos_wi / v.pdf_wi );
		}
		else
		{
			v.pdf_wi = mat->pdf(state, v.sp, ray.dir, v.wi, BSDF_ALL); // all BSDFs? think so...
			v.qi_wi = std::min( 0.98f, v.f_s.col2bri()*v.cos_wi / v.pdf_wi );
		}
		if(v.qi_wi < 0) std::cout << "v["<<nVert<<"].qi_wi="<<v.qi_wi<<" ("<<v.f_s.col2bri()<<" "<<v.cos_wi<<" "<<v.pdf_wi<<")\n"
			                          <<"\t"<<v.pdf_wo<<"  flags:"<<s.sampledFlags<<std::endl;

		v.flags = s.sampledFlags;
		v.wo = ray.dir;
		ray.from = v.sp.P;
		ray.tmin = MIN_RAYDIST;
		ray.tmax = -1.f;
	}
	++dbg;
	return nVert;
}

// small utilities to make code easier (well...less impossible) to read...
inline void copyLightSubpath(pathData_t &pd, int s, int t)
{
	for(int i=0; i<s-1; ++i)
	{
		const pathVertex_t &v = pd.lightPath[i];
		pd.path[i].pdf_f = v.pdf_wo/v.cos_wo;
		pd.path[i].pdf_b = v.pdf_wi/v.cos_wi;
		pd.path[i].specular = v.flags & BSDF_SPECULAR;
		pd.path[i].G = v.G;
	}
	pd.path[s-1].G = pd.lightPath[s-1].G;
}

inline void copyEyeSubpath(pathData_t &pd, int s, int t)
{
	for(int i=s+1, j=t-2; j>=0; ++i, --j)
	{
		const pathVertex_t &v = pd.eyePath[j];
		pd.path[i].pdf_f = v.pdf_wi/v.cos_wi;
		pd.path[i].pdf_b = v.pdf_wo/v.cos_wo;
		pd.path[i].specular = v.flags & BSDF_SPECULAR;
		pd.path[i].G = pd.eyePath[j+1].G;
	}
}

/* ============================================================
    connect the two paths in various ways
    if paths cannot be sampled from either side, return false
 ============================================================ */


//===  connect paths with s and t > 1  ===//
inline bool biDirIntegrator_t::connectPaths(renderState_t &state, int s, int t, pathData_t &pd) const
{
	const pathVertex_t &y = pd.lightPath[s-1];
	const pathVertex_t &z = pd.eyePath[t-1];
	pathEvalVert_t &x_l = pd.path[s-1];
	pathEvalVert_t &x_e = pd.path[s];
	// precompute stuff in pc that is specific to the current connection of sub-paths
	vector3d_t vec = z.sp.P - y.sp.P;
	PFLOAT dist2 = vec.normLenSqr();
	PFLOAT cos_y = std::fabs(y.sp.N * vec);
	PFLOAT cos_z = std::fabs(z.sp.N * vec);

	state.userdata = y.userdata;
	x_l.pdf_f = y.sp.material->pdf(state, y.sp, y.wi, vec, BSDF_ALL); // light vert to eye vert
	x_l.pdf_b = y.sp.material->pdf(state, y.sp, vec, y.wi, BSDF_ALL); // light vert to prev. light vert
	if(x_l.pdf_f < 1e-6f) return false;
	x_l.pdf_f /= cos_y;
	x_l.pdf_b /= y.cos_wi;
	pd.f_y = y.sp.material->eval(state, y.sp, y.wi, vec, BSDF_ALL);
	pd.f_y += y.sp.material->emit(state, y.sp, vec);

	state.userdata = z.userdata;
	x_e.pdf_b = z.sp.material->pdf(state, z.sp, z.wi, -vec, BSDF_ALL); // eye vert to light vert
	x_e.pdf_f = z.sp.material->pdf(state, z.sp, -vec, z.wi, BSDF_ALL); // eye vert to prev eye vert
	if(x_e.pdf_b < 1e-6f) return false;
	x_e.pdf_b /= cos_z;
	x_e.pdf_f /= z.cos_wi;
	pd.f_z = z.sp.material->eval(state, z.sp, z.wi, -vec, BSDF_ALL);
	pd.f_z += z.sp.material->emit(state, z.sp, -vec);

	pd.w_l_e = vec;
	pd.d_yz = fSqrt(dist2);
	x_e.G = std::fabs(cos_y * cos_z) / dist2; // or use Ng??
	x_e.specular = false;
	x_l.specular = false;

	//copy values required
	copyLightSubpath(pd, s, t);
	copyEyeSubpath(pd, s, t);

	// calculate qi's...
	if(s>MIN_PATH_LENGTH) x_l.pdf_f *= std::min( 0.98f, pd.f_y.col2bri()/* *cos_y */ / x_l.pdf_f );
	if(s+1>MIN_PATH_LENGTH) x_e.pdf_f *= std::min( 0.98f, pd.f_z.col2bri()/* *y.cos_wi */ / x_e.pdf_f );
	// backward:
	if(t+1>MIN_PATH_LENGTH) x_l.pdf_b *= std::min( 0.98f, pd.f_y.col2bri()/* *y.cos_wi */ / x_l.pdf_b );
	if(t>MIN_PATH_LENGTH) x_e.pdf_b *= std::min( 0.98f, pd.f_z.col2bri()/* *cos_z */ / x_e.pdf_b );

	// multiply probabilities with qi's
	int k = s+t-1;
	// forward:
	for(int i=MIN_PATH_LENGTH, s1=s-1; i<s1; ++i)
		pd.path[i].pdf_f *= pd.lightPath[i].qi_wo;
	for(int i=std::max(MIN_PATH_LENGTH,s+1), st=s+t; i<st; ++i)
		pd.path[i].pdf_f *= pd.eyePath[k-i].qi_wi;

	//backward:
	for(int i=MIN_PATH_LENGTH, t1=t-1; i<t1; ++i)
		pd.path[k-i].pdf_b *= pd.eyePath[i].qi_wo;
	for(int i=std::max(MIN_PATH_LENGTH,t+1), st=s+t; i<st; ++i)
		pd.path[k-i].pdf_b *= pd.lightPath[k-i].qi_wi;
	return true;
}

// connect path with s==1 (eye path with single light vertex)
inline bool biDirIntegrator_t::connectLPath(renderState_t &state, int t, pathData_t &pd, ray_t &lRay, color_t &lcol) const
{
	// create light sample with direct lighting strategy:
	const pathVertex_t &z = pd.eyePath[t-1];
	lRay.from = z.sp.P;
	lRay.tmin = 0.0005;
	int nLightsI = lights.size();
	if(nLightsI == 0) return false;
	float lightNumPdf, cos_wo;
	int lnum = lightPowerD->DSample((*state.prng)(), &lightNumPdf);
	lightNumPdf *= fNumLights;
	if(lnum > nLightsI-1) lnum = nLightsI-1;
	const light_t *light = lights[lnum];
	surfacePoint_t spLight;

	//== use illumSample, no matter what...s1/s2 is only set when required ==
	lSample_t ls;
	if(light->getFlags() == LIGHT_NONE) //only lights with non-specular components need sample values
	{
		ls.s1 = (*state.prng)();
		ls.s2 = (*state.prng)();
	}
	ls.sp = &spLight;
	// generate light sample, abort when none could be created:
	if( !light->illumSample(z.sp, ls, lRay) ) return false;
	lcol = ls.col/(ls.pdf*lightNumPdf); //shouldn't really do that division, better use proper c_st in evalLPath...
	// get probabilities for generating light sample without a given surface point
	vector3d_t vec = -lRay.dir;
	light->emitPdf(spLight, vec, pd.path[0].pdf_A_0, pd.path[0].pdf_f, cos_wo);
	pd.path[0].pdf_A_0 *= lightNumPdf;
	pd.path[0].pdf_f /= cos_wo;
	pd.path[0].specular = ls.flags & LIGHT_DIRACDIR;
	pd.singularL = ls.flags & LIGHT_SINGULAR;
	pd.pdf_illum = ls.pdf * lightNumPdf;
	pd.pdf_emit = pd.path[0].pdf_A_0 * (spLight.P - z.sp.P).lengthSqr() / cos_wo;


	//fill in pc...connecting to light vertex:
	//pathEvalVert_t &x_l = pd.path[0];
	pathEvalVert_t &x_e = pd.path[1];
	PFLOAT cos_z = std::fabs(z.sp.N * vec);
	x_e.G = std::fabs(cos_wo * cos_z) / (lRay.tmax*lRay.tmax); // or use Ng??
	pd.w_l_e = vec;
	pd.d_yz = lRay.tmax;
	state.userdata = z.userdata;
	x_e.pdf_b = z.sp.material->pdf(state, z.sp, z.wi, lRay.dir, BSDF_ALL); //eye to light
	if(x_e.pdf_b < 1e-6f) return false;
	x_e.pdf_f = z.sp.material->pdf(state, z.sp, lRay.dir, z.wi, BSDF_ALL); // eye to prev eye
	x_e.pdf_b /= cos_z;
	x_e.pdf_f /= z.cos_wi;
	x_e.specular = false;
	pd.f_z = z.sp.material->eval(state, z.sp, z.wi, lRay.dir, BSDF_ALL);
	pd.f_z += z.sp.material->emit(state, z.sp, lRay.dir);
	pd.light = light;

	//copy values required
	pd.path[0].G = 0.f;
	copyEyeSubpath(pd, 1, t);

	// calculate qi's...
	// backward:
	//if(t+1>MIN_PATH_LENGTH) x_l.pdf_b *= std::min( 0.98f, pd.f_y.col2bri()*y.cos_wi / x_l.pdf_b ); //unused/meaningless(?)
	if(t>MIN_PATH_LENGTH) x_e.pdf_b *= std::min( 0.98f, pd.f_z.col2bri()/* *cos_z */ / x_e.pdf_b );

	// multiply probabilities with qi's
	int k = t;
	// forward:
	for(int i=std::max(MIN_PATH_LENGTH,2), st=t+1; i<st; ++i)
		pd.path[i].pdf_f *= pd.eyePath[st-i-1].qi_wi;

	//backward:
	for(int i=MIN_PATH_LENGTH, t1=t-1; i<t1; ++i)
		pd.path[k-i].pdf_b *= pd.eyePath[i].qi_wo;
	return true;
}

// connect path with t==1 (s>1)
inline bool biDirIntegrator_t::connectPathE(renderState_t &state, int s, pathData_t &pd) const
{
	const pathVertex_t &y = pd.lightPath[s-1];
	const pathVertex_t &z = pd.eyePath[0];
	pathEvalVert_t &x_l = pd.path[s-1];
	pathEvalVert_t &x_e = pd.path[s];

	vector3d_t vec = z.sp.P - y.sp.P;
	PFLOAT dist2 = vec.normLenSqr();
	PFLOAT cos_y = std::fabs(y.sp.N * vec);

	ray_t wo(z.sp.P, -vec);
	if(! cam->project(wo, 0, 0, pd.u, pd.v, x_e.pdf_b) ) return false;

	x_e.specular = false; // cannot query yet...

	state.userdata = y.userdata;
	x_l.pdf_f = y.sp.material->pdf(state, y.sp, y.wi, vec, BSDF_ALL); // light vert to eye vert
	if(x_l.pdf_f < 1e-6f) return false;
	x_l.pdf_b = y.sp.material->pdf(state, y.sp, vec, y.wi, BSDF_ALL); // light vert to prev. light vert
	x_l.pdf_f /= cos_y;
	x_l.pdf_b /= y.cos_wi;
	pd.f_y = y.sp.material->eval(state, y.sp, y.wi, vec, BSDF_ALL);
	pd.f_y += y.sp.material->emit(state, y.sp, vec);
	x_l.specular = false;

	pd.w_l_e = vec;
	pd.d_yz = fSqrt(dist2);
	x_e.G = cos_y / dist2; // or use Ng??
	x_e.pdf_f = 1.f; // unused...

	copyLightSubpath(pd, s, 1);

	// calculate qi's...
	if(s>MIN_PATH_LENGTH) x_l.pdf_f *= std::min( 0.98f, pd.f_y.col2bri()/* *cos_y */ / x_l.pdf_f );
	//if(s+1>MIN_PATH_LENGTH) x_e.pdf_f *= std::min( 0.98f, pd.f_z.col2bri()*y.cos_wi / x_e.pdf_f );

	// multiply probabilities with qi's
	int k = s;
	// forward:
	for(int i=MIN_PATH_LENGTH, s1=s-1; i<s1; ++i)
		pd.path[i].pdf_f *= pd.lightPath[i].qi_wo;

	//backward:
	for(int i=std::max(MIN_PATH_LENGTH,2), st=s+1; i<st; ++i) //FIXME: why st=s+1 and not just s?
		pd.path[k-i].pdf_b *= pd.lightPath[k-i].qi_wi;
	return true;
}


/* ============================================================
    calculate the path weight with some combination strategy
 ============================================================ */

// compute path densities and weight path
CFLOAT biDirIntegrator_t::pathWeight(renderState_t &state, int s, int t, pathData_t &pd) const
{
	const std::vector<pathEvalVert_t> &path = pd.path;
	float pr[2*MAX_PATH_LENGTH+1], p[2*MAX_PATH_LENGTH+1];
	p[s] = 1.f;
	// "forward" weights (towards eye), ratio pr_i here is p_i+1 / p_i
	int k = s+t-1;
	for(int i=s; i<k; ++i)
	{
		pr[i] = ( path[i-1].pdf_f * path[i].G ) / ( path[i+1].pdf_b * path[i+1].G );
		p[i+1] = p[i] * pr[i];
	}
	// "backward" weights (towards light), ratio pr_i here is p_i / p_i+1
	for(int i=s-1; i>0; --i)
	{
		pr[i] = ( path[i+1].pdf_b * path[i+1].G ) / ( path[i-1].pdf_f * path[i].G );
		p[i] = p[i+1] * pr[i];
	}
	// do p_0/p_1...
	pr[0] =  ( path[1].pdf_b * path[1].G ) / path[0].pdf_A_0;
	p[0] = p[1] * pr[0];
	// p_k+1/p_k is zero currently, hitting the camera lens in general should be very seldom anyway...
	p[k+1] = 0.f;
#if !(_DO_LIGHTIMAGE)
	p[k] = 0.f; // cannot intersect camera yet...
#endif
	// treat specular scatter events.
	// specular x_i makes p_i (join x_i-1 and x_i) and p_i+1 (join x_i and x_i+1) = 0:
	for(int i=0; i<=k; ++i)
	{
		if(path[i].specular)
		{
			p[i] = 0.f;
			p[i+1] = 0.f;
		}
	}
	if(pd.singularL) p[0] = 0.f;
	// correct p1 with direct lighting strategy:
	else p[1] *= pd.pdf_illum / pd.pdf_emit; //test! workaround for incomplete pdf funcs of lights
	// do MIS...maximum heuristic, particularly simple, if there's a more likely sample method, weight is zero, otherwise 1
	float weight = 1.f;

	for(int i=s-1; i>=0; --i) if(p[i] > p[s]) weight=0.f;
	for(int i=s+1; i<=k+1; ++i) if(p[i] > p[s]) weight=0.f;

	return weight;
}

// weight paths that directly hit a light, i.e. s=0; t is at least 2 //
CFLOAT biDirIntegrator_t::pathWeight_0t(renderState_t &state, int t, pathData_t &pd) const
{
	const std::vector<pathEvalVert_t> &path = pd.path;
	const pathVertex_t &vl = pd.eyePath[t-1];
	// since we need no connect, complete some probabilities here:
	float lightNumPdf = invLightPowerD.find(vl.sp.light)->second;
	lightNumPdf *= fNumLights;
	float cos_wo;
	// direct lighting pdf...
	float pdf_illum = vl.sp.light->illumPdf(pd.eyePath[t-2].sp, vl.sp) * lightNumPdf;
	if(pdf_illum < 1e-6f) return 0.f;

	vl.sp.light->emitPdf(vl.sp, vl.wi, pd.path[0].pdf_A_0, pd.path[0].pdf_f, cos_wo);
	pd.path[0].pdf_A_0 *= lightNumPdf;
	float pdf_emit = pd.path[0].pdf_A_0 * vl.ds / cos_wo;
	pd.path[0].G = 0.f; // unused...
	pd.path[0].specular = false;
	copyEyeSubpath(pd, 0, t);
	check_path(pd.path, 0, t);

	// == standard weighting procedure now == //

	float pr, p[2*MAX_PATH_LENGTH+1];

	p[0] = 1;
	p[1] = path[0].pdf_A_0 / ( path[1].pdf_b * path[1].G );

	int k = t-1;
	for(int i=1; i<k; ++i)
	{
		pr = ( path[i-1].pdf_f * path[i].G ) / ( path[i+1].pdf_b * path[i+1].G );
		p[i+1] = p[i] * pr;
	}

	// p_k+1/p_k is zero currently, hitting the camera lens in general should be very seldom anyway...
	p[k+1] = 0.f;
#if !(_DO_LIGHTIMAGE)
	p[k] = 0.f; // cannot intersect camera yet...
#endif
	// treat specular scatter events.
	for(int i=0; i<=k; ++i)
	{
		if(path[i].specular) p[i] = p[i+1] = 0.f;
	}
	// correct p1 with direct lighting strategy:
	p[1] *= pdf_illum / pdf_emit;

	// do MIS...maximum heuristic, particularly simple, if there's a more likely sample method, weight is zero, otherwise 1
	float weight = 1.f;
	for(int i=1; i<=t; ++i) if(p[i] > 1.f) weight=0.f;

	return weight;
}

/* ============================================================
    eval the (unweighted) path created by connecting
    sub-paths at s,t
 ============================================================ */


color_t biDirIntegrator_t::evalPath(renderState_t &state, int s, int t, pathData_t &pd) const
{
	const pathVertex_t &y = pd.lightPath[s-1];
	const pathVertex_t &z = pd.eyePath[t-1];

	color_t c_st = pd.f_y * pd.path[s].G * pd.f_z;
	//unweighted contronution C*:
	color_t C_uw = y.alpha * c_st * z.alpha;
	ray_t conRay(y.sp.P, pd.w_l_e, 0.0005, pd.d_yz);
	if(scene->isShadowed(state, conRay)) return color_t(0.f);
	return C_uw;
}

//===  eval paths with s==1 (direct lighting strategy)  ===//
color_t biDirIntegrator_t::evalLPath(renderState_t &state, int t, pathData_t &pd, ray_t &lRay, const color_t &lcol) const
{
	static int dbg=0;
	if(scene->isShadowed(state, lRay)) return color_t(0.f);
	const pathVertex_t &z = pd.eyePath[t-1];

	color_t C_uw = lcol * pd.f_z * z.alpha * std::fabs(z.sp.N*lRay.dir); // f_y, cos_x0_f and r^2 computed in connectLPath...(light pdf)
	// hence c_st is only cos_x1_b * f_z...like path tracing
	//if(dbg < 10) std::cout << "evalLPath(): f_z:" << pd.f_z << " C_uw:" << C_uw << std::endl;
	++dbg;
	return C_uw;
}

//=== eval path with t==1 (light path directly connected to eve vertex)
//almost same as evalPath, just that there is no material on one end but a camera sensor function (soon...)
color_t biDirIntegrator_t::evalPathE(renderState_t &state, int s, pathData_t &pd) const
{
	const pathVertex_t &y = pd.lightPath[s-1];

	ray_t conRay(y.sp.P, pd.w_l_e, 0.0005, pd.d_yz);
	if(scene->isShadowed(state, conRay)) return color_t(0.f);

	//eval material
	state.userdata = y.userdata;
	//color_t f_y = y.sp.material->eval(state, y.sp, y.wi, pd.w_l_e, BSDF_ALL);
	//TODO:
	color_t C_uw = y.alpha * M_PI * pd.f_y * pd.path[s].G;

	return C_uw;
}

////

/* inline color_t biDirIntegrator_t::estimateOneDirect(renderState_t &state, const surfacePoint_t &sp, vector3d_t wo, pathCon_t &pc)const
{
    color_t lcol(0.0), scol, col(0.0);
    ray_t lightRay;
    bool shadowed;
    const material_t *oneMat = sp.material;
    lightRay.from = sp.P;
    int nLightsI = lights.size();
    if(nLightsI == 0) return color_t(0.f);
    float s1, s2, lightPdf, lightNumPdf;
    int lnum = lightPowerD->DSample((*state.prng)(), &lightNumPdf);
    if(lnum > nLightsI-1) lnum = nLightsI-1;
    const light_t *light = lights[lnum];
    // handle lights with delta distribution, e.g. point and directional lights
    if( light->diracLight() )
    {
        if( light->illuminate(sp, lcol, lightRay) )
        {
            // ...shadowed...
            lightRay.tmin = 0.0005; // < better add some _smart_ self-bias value...this is bad.
            shadowed = (trShad) ? scene->isShadowed(state, lightRay, sDepth, scol) : scene->isShadowed(state, lightRay);
            if(!shadowed)
            {
                if(trShad) lcol *= scol;
                color_t surfCol = oneMat->eval(state, sp, wo, lightRay.dir, BSDF_ALL);
                col = surfCol * lcol * std::fabs(sp.N*lightRay.dir);
            }
        }
    }
    else // area light and suchlike
    {
        color_t ccol(0.0);
        s1 = (*state.prng)();
        s2 = (*state.prng)();

        if( light->illumSample (sp, s1, s2, lcol, lightPdf, lightRay) )
        {
            // ...shadowed...
            lightRay.tmin = 0.0005; // < better add some _smart_ self-bias value...this is bad.
            shadowed = (trShad) ? scene->isShadowed(state, lightRay, sDepth, scol) : scene->isShadowed(state, lightRay);
            if(!shadowed)
            {
                if(trShad) lcol *= scol;
                color_t surfCol = oneMat->eval(state, sp, wo, lightRay.dir, BSDF_ALL);
                //test! limit lightPdf...
                if(lightPdf > 2.f) lightPdf = 2.f;
                col = surfCol * lcol * std::fabs(sp.N*lightRay.dir) * lightPdf;
            }
        }
    }
    return col/lightNumPdf;
} */

integrator_t* biDirIntegrator_t::factory(paraMap_t &params, renderEnvironment_t &render)
{
	biDirIntegrator_t *inte = new biDirIntegrator_t();
	return inte;
}

extern "C"
{
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("bidirectional", biDirIntegrator_t::factory);
	}
}

__END_YAFRAY
