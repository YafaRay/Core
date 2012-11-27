
#ifndef Y_MATERIAL_H
#define Y_MATERIAL_H

#include <yafray_config.h>

//#include "scene.h"
#include "ray.h"
#include "surface.h"
#include "utilities/sample_utils.h"


__BEGIN_YAFRAY

#define FACE_FORWARD(Ng,N,I) ((((Ng)*(I))<0) ? (-N) : (N))
/*
class YAFRAYCORE_EXPORT BSDF_t
{
	public:
	BSDF_t(){};
	virtual ~BSDF_t(){};
	BSDF_t(const surfacePoint_t &sp, void* userdata);
	virtual color_t sample(const vector3d_t &wo, vector3d_t &wi, float s1, float s2, void* userdata) = 0;
	virtual color_t eval(const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wl, void* userdata) = 0;
};
*/

class surfacePoint_t;
class renderState_t;
class volumeHandler_t;

enum bsdfFlags_t
{
	BSDF_NONE		= 0x0000,
	BSDF_SPECULAR	= 0x0001,
	BSDF_GLOSSY		= 0x0002,
	BSDF_DIFFUSE	= 0x0004,
	BSDF_DISPERSIVE	= 0x0008,
	BSDF_REFLECT	= 0x0010,
	BSDF_TRANSMIT	= 0x0020,
	BSDF_FILTER		= 0x0040,
	BSDF_EMIT		= 0x0080,
	BSDF_VOLUMETRIC = 0x0100,
	BSDF_ALL_SPECULAR = BSDF_SPECULAR | BSDF_REFLECT | BSDF_TRANSMIT,
	BSDF_ALL_GLOSSY = BSDF_GLOSSY | BSDF_REFLECT | BSDF_TRANSMIT,
	BSDF_ALL = BSDF_SPECULAR | BSDF_GLOSSY | BSDF_DIFFUSE | BSDF_DISPERSIVE | BSDF_REFLECT | BSDF_TRANSMIT | BSDF_FILTER
};

typedef unsigned int BSDF_t;

struct sample_t
{
	sample_t(float s_1, float s_2, BSDF_t sflags=BSDF_ALL, bool revrs=false):
			s1(s_1), s2(s_2), pdf(0.f), flags(sflags), sampledFlags(BSDF_NONE), reverse(revrs){}
	float s1, s2;
	float pdf;
	BSDF_t flags, sampledFlags;
	bool reverse; //!< if true, the sample method shall return the probability/color for swapped incoming/outgoing dir
	float pdf_back;
	color_t col_back;
};

struct pSample_t: public sample_t // << whats with the public?? structs inherit publicly by default *DarkTide
{
	pSample_t(float s_1, float s_2, float s_3, BSDF_t sflags, const color_t &l_col, const color_t& transm=color_t(1.f)):
		sample_t(s_1, s_2, sflags), s3(s_3), lcol(l_col), alpha(transm){}
	float s3;
	const color_t lcol; //!< the photon color from last scattering
	const color_t alpha; //!< the filter color between last scattering and this hit (not pre-applied to lcol!)
	color_t color; //!< the new color after scattering, i.e. what will be lcol for next scatter.
};

class YAFRAYCORE_EXPORT material_t
{
	public:
		material_t(): bsdfFlags(BSDF_NONE), reqMem(0), volI(0), volO(0) {}
		virtual ~material_t() {}
		
		/*! Initialize the BSDF of a material. You must call this with the current surface point
			first before any other methods (except isTransparent/getTransparency)! The renderstate
			holds a pointer to preallocated userdata to save data that only depends on the current sp,
			like texture lookups etc.
			\param bsdfTypes returns flags for all bsdf components the material has
		 */
        virtual void initBSDF(const renderState_t &state, surfacePoint_t &sp, BSDF_t &bsdfTypes)const = 0;
		
		/*! evaluate the BSDF for the given components.
				@param types the types of BSDFs to be evaluated (e.g. diffuse only, or diffuse and glossy) */
		virtual color_t eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wl, BSDF_t types)const = 0;
		
		/*! take a sample from the BSDF, given a 2-dimensional sample value and the BSDF types to be sampled from
			\param s s1, s2 and flags members give necessary information for creating the sample, pdf and sampledFlags need to be returned
			\param W returns the weight for importance sampling
		*/
		virtual color_t sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const = 0;// {return color_t(0.f);}
		
		/*! return the pdf for sampling the BSDF with wi and wo
		*/
		virtual float pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const {return 0.f;}
		

		/*! indicate whether light can (partially) pass the material without getting refracted,
			e.g. a curtain or even very thin foils approximated as single non-refractive layer.
			used to trace transparent shadows. Note that in this case, initBSDF was NOT called before!
		*/
		virtual bool isTransparent() const { return false; }
		
		/*!	used for computing transparent shadows.	Default implementation returns black (i.e. solid shadow).
			This is only used for shadow calculations and may only be called when isTransparent returned true.	*/
		virtual color_t getTransparency(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const { return color_t(0.0); }
		/*! evaluate the specular components for given direction. Somewhat a specialization of sample(),
			because neither sample values nor pdf values are necessary for this.
			Typical use: recursive raytracing of integrators.
			\param dir dir[0] returns reflected direction, dir[1] refracted direction
			\param col col[0] returns reflected spectrum, dir[1] refracted spectrum */
		virtual void getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo,
								 bool &reflect, bool &refract, vector3d_t *const dir, color_t *const col)const
		{ reflect=false; refract=false; }
		
		/*! get the overall reflectivity of the material (used to compute radiance map for example) */
		virtual color_t getReflectivity(const renderState_t &state, const surfacePoint_t &sp, BSDF_t flags)const;
		
		/*!	allow light emitting materials, for realizing correctly visible area lights.
			default implementation returns black obviously.	*/
		virtual color_t emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const { return color_t(0.0); }
		
		/*! get the volumetric handler for space at specified side of the surface
			\param inside true means space opposite of surface normal, which is considered "inside" */
		const volumeHandler_t* getVolumeHandler(bool inside)const { return inside ? volI : volO; }
		
		/*! special function, get the alpha-value of a material, used to calculate the alpha-channel */
		virtual CFLOAT getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const { return 1.f; }
		
		/*! specialized function for photon mapping. Default function uses the sample function, which will do fine for
			most materials unless there's a less expensive way or smarter scattering approach */
		virtual bool scatterPhoton(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wi, vector3d_t &wo, pSample_t &s) const;
		
		BSDF_t getFlags() const { return bsdfFlags; }
		/*! Materials may have to do surface point specific (pre-)calculation that need extra storage.
			returns the required amount of "userdata" memory for all the functions that require a render state */
		size_t getReqMem() const { return reqMem; }
		
		/*! Get materials IOR (for refracted photons) */
		
		virtual float getMatIOR() const { return 1.5f; }
		
	protected:
		/* small function to apply bump mapping to a surface point 
			you need to determine the partial derivatives for NU and NV first, e.g. from a shader node */
        void applyBump(surfacePoint_t &sp, PFLOAT dfdNU, PFLOAT dfdNV) const;
		
		BSDF_t bsdfFlags;
		size_t reqMem; //!< the amount of "temporary" memory required to compute/store surface point specific data
		volumeHandler_t* volI; //!< volumetric handler for space inside material (opposed to surface normal)
		volumeHandler_t* volO; //!< volumetric handler for space outside ofmaterial (where surface normal points to)
};

	

__END_YAFRAY

#endif // Y_MATERIAL_H
