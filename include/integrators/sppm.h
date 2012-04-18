#ifndef Y_SPPM_H
#define Y_SPPM_H

#include <yafray_config.h>
#include <core_api/environment.h>
#include <core_api/material.h>
#include <core_api/background.h>
#include <core_api/light.h>
#include <core_api/imagefilm.h>
#include <core_api/camera.h>
#include <core_api/mcintegrator.h>
#include <yafraycore/photon.h>
#include <yafraycore/monitor.h>
#include <yafraycore/ccthreads.h>
#include <yafraycore/timer.h>
#include <yafraycore/spectrum.h>
#include <utilities/sample_utils.h>
#include <utilities/mcqmc.h>
#include <yafraycore/scr_halton.h>
#include <yafraycore/hashgrid.h>

__BEGIN_YAFRAY

typedef struct _HitPoint // actually are per-pixel variable, use to record the sppm's shared statistics
{
	float radius2; // square search-radius, shrink during the passes
	unsigned long long accPhotonCount; // record the total photon this pixel gathered
	colorA_t accPhotonFlux; // accumulated flux
	colorA_t constantRandiance; // record the direct light for this pixel

	bool radiusSetted; // used by IRE to direct whether the initial radius is set or not.
}HitPoint;

//used for gather ray to collect photon information
typedef struct _GatherInfo
{
	unsigned long photonCount;  // the number of photons that the gather ray collected
	colorA_t photonFlux;   // the unnormalized flux of photons that the gather ray collected
	colorA_t constantRandiance; // the radiance from when the gather ray hit the lightsource

	_GatherInfo(): photonCount(0), photonFlux(0.f),constantRandiance(0.f){}

	_GatherInfo & operator +=(const _GatherInfo &g)
	{
		photonCount += g.photonCount;
		photonFlux += g.photonFlux;
		constantRandiance += g.constantRandiance;
		return (*this);
	}
}GatherInfo;


class YAFRAYPLUGIN_EXPORT SPPM: public mcIntegrator_t
{
	public:
		SPPM(unsigned int dPhotons, int passnum, bool transpShad, int shadowDepth);
		~SPPM();
		virtual bool render(imageFilm_t *imageFilm);
		/*! render a tile; only required by default implementation of render() */
		virtual bool renderTile(renderArea_t &a, int n_samples, int offset, bool adaptive, int threadID);
		virtual bool preprocess(); //not used for now
		// not used now
		virtual void prePass(int samples, int offset, bool adaptive);
		/*! not used now, use traceGatherRay instead*/
		virtual colorA_t integrate(renderState_t &state, diffRay_t &ray/*, sampler_t &sam*/) const;
		static integrator_t* factory(paraMap_t &params, renderEnvironment_t &render);
		/*! initializing the things that PPM uses such as initial radius */
		void initializePPM();
		/*! based on integrate method to do the gatering trace, need double-check deadly. */
		GatherInfo traceGatherRay(renderState_t &state, diffRay_t &ray, HitPoint &hp);
	protected:
		hashGrid_t  photonGrid; // the hashgrid for holding photons
		photonMap_t diffuseMap,causticMap; // photonmap
		pdf1D_t *lightPowerD;
		unsigned int nPhotons; //photon number to scatter
		float dsRadius; // used to do initial radius estimate
		int nSearch;// now used to do initial radius estimate
		int passNum; // the progressive pass number
		float initialFactor; // used to time the initial radius
		unsigned int totalnPhotons; // amount of total photons that have been emited, used to normalize photon energy
		bool PM_IRE; // flag to  say if using PM for initial radius estimate
		bool bHashgrid; // flag to choose using hashgrid or not.

		Halton hal1, hal2, hal3, hal4, hal7, hal8, hal9, hal10; // halton sequence to do

		std::vector<HitPoint>hitPoints; // per-pixel refine data

		unsigned int nRefined; // Debug info: Refined pixel per pass
};

__END_YAFRAY

#endif // SPPM
