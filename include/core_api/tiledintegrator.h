
#ifndef Y_TILEDINTEGRATOR_H
#define Y_TILEDINTEGRATOR_H

#include <core_api/integrator.h>
#include <core_api/imagesplitter.h>
#include <core_api/material.h>

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT tiledIntegrator_t: public surfaceIntegrator_t
{
	public:
		/*! Rendering prepasses to precalc suff in case needed */
		virtual void preRender(); //!< Called before the render starts and after the minDepth and maxDepth are calculated
		virtual void prePass(int samples, int offset, bool adaptive); //!< Called before the proper rendering of all the tiles starts
		virtual void preTile(renderArea_t &a, int n_samples, int offset, bool adaptive, int threadID); //!< Called brfore each tile is rendered
		
		/*! do whatever is required to render the image; default implementation renders image in passes
		dividing each pass into tiles for multithreading. */
		virtual bool render(imageFilm_t *imageFilm);
		/*! render a pass; only required by the default implementation of render() */
		virtual bool renderPass(int samples, int offset, bool adaptive);
		/*! render a tile; only required by default implementation of render() */
		virtual bool renderTile(renderArea_t &a, int n_samples, int offset, bool adaptive, int threadID);
		
//		virtual void recursiveRaytrace(renderState_t &state, diffRay_t &ray, int rDepth, BSDF_t bsdfs, surfacePoint_t &sp, vector3d_t &wo, color_t &col, float &alpha) const;
		virtual void precalcDepths();
	
	protected:
		int AA_samples, AA_passes, AA_inc_samples;
		float iAA_passes; //!< Inverse of AA_passes used for depth map
		float AA_threshold;
		int AA_resampled_floor; //!< minimum amount of resampled pixels below which we will automatically decrease the AA_threshold value for the next pass
		imageFilm_t *imageFilm;
		float maxDepth; //!< Inverse of max depth from camera within the scene boundaries
		float minDepth; //!< Distance between camera and the closest object on the scene
		bool diffRaysEnabled;	//!< Differential rays enabled/disabled - for future motion blur / interference features

};

#ifdef USING_THREADS

struct threadControl_t
{
	threadControl_t() : finishedThreads(0) {}
	yafthreads::conditionVar_t countCV; //!< condition variable to signal main thread
	std::vector<renderArea_t> areas; //!< area to be output to e.g. blender, if any
	volatile int finishedThreads; //!< number of finished threads, lock countCV when increasing/reading!
};

#endif

__END_YAFRAY

#endif // Y_TILEDINTEGRATOR_H
