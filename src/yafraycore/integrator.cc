/****************************************************************************
 *      integrator.cc: Basic tile based surface integrator
 *      This is part of the yafaray package
 *      Copyright (C) 2006  Mathias Wein (Lynx)
 *		Copyright (C) 2009  Rodrigo Placencia (DarkTide)
 *		Previous code might belong to:
 *		Alejandro Conty (jandro)
 *		Alfredo Greef (eshlo)
 *		Others?
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

#include <yafraycore/timer.h>
#include <yafraycore/scr_halton.h>
#include <yafraycore/spectrum.h>

#include <core_api/tiledintegrator.h>
#include <core_api/imagefilm.h>
#include <core_api/camera.h>
#include <core_api/surface.h>
#include <core_api/material.h>

#include <utilities/mcqmc.h>
#include <utilities/sample_utils.h>

#include <sstream>

__BEGIN_YAFRAY

#ifdef USING_THREADS

class renderWorker_t: public yafthreads::thread_t
{
	public:
		renderWorker_t(tiledIntegrator_t *it, scene_t *s, imageFilm_t *f, threadControl_t *c, int id, int smpls, int offs=0, bool adptv=false):
			integrator(it), scene(s), imageFilm(f), control(c), samples(smpls), offset(offs), threadID(id), adaptive(adptv)
		{
			//Empty
		}
		virtual void body();
	protected:
		tiledIntegrator_t *integrator;
		scene_t *scene;
		imageFilm_t *imageFilm;
		threadControl_t *control;
		int samples, offset;
		int threadID;
		bool adaptive;
};

void renderWorker_t::body()
{
	renderArea_t a;
	while(imageFilm->nextArea(a))
	{
		if(scene->getSignals() & Y_SIG_ABORT) break;
		integrator->preTile(a, samples, offset, adaptive, threadID);
		integrator->renderTile(a, samples, offset, adaptive, threadID);
		control->countCV.lock();
		control->areas.push_back(a);
		control->countCV.signal();
		control->countCV.unlock();
	}
	control->countCV.lock();
	++(control->finishedThreads);
	control->countCV.signal();
	control->countCV.unlock();
}
#endif

void tiledIntegrator_t::preRender()
{
	// Empty
}

void tiledIntegrator_t::prePass(int samples, int offset, bool adaptive)
{
	// Empty
}

void tiledIntegrator_t::preTile(renderArea_t &a, int n_samples, int offset, bool adaptive, int threadID)
{
	// Empty
}

void tiledIntegrator_t::precalcDepths()
{
	const camera_t* camera = scene->getCamera();
	diffRay_t ray;
	// We sample the scene at render resolution to get the precision required for AA
	int w = camera->resX();
	int h = camera->resY();
	float wt = 0.f; // Dummy variable
	surfacePoint_t sp;

	for(int i=0; i<h; ++i)
	{
		for(int j=0; j<w; ++j)
		{
			ray.tmax = -1.f;
			ray = camera->shootRay(i, j, 0.5f, 0.5f, wt);
			scene->intersect(ray, sp);
			if(ray.tmax > maxDepth) maxDepth = ray.tmax;
			if(ray.tmax < minDepth && ray.tmax >= 0.f) minDepth = ray.tmax;
		}
	}
	// we use the inverse multiplicative of the value aquired
	if(maxDepth > 0.f) maxDepth = 1.f / (maxDepth - minDepth);
}

bool tiledIntegrator_t::render(imageFilm_t *image)
{
	std::stringstream passString;
	imageFilm = image;
	scene->getAAParameters(AA_samples, AA_passes, AA_inc_samples, AA_threshold);
	iAA_passes = 1.f / (float) AA_passes;
	Y_INFO << integratorName << ": Rendering " << AA_passes << " passes" << yendl;
	Y_INFO << integratorName << ": Min. " << AA_samples << " samples" << yendl;
	Y_INFO << integratorName << ": "<< AA_inc_samples << " per additional pass" << yendl;
	Y_INFO << integratorName << ": Max. " << AA_samples + std::max(0,AA_passes-1) * AA_inc_samples << " total samples" << yendl;
	passString << "Rendering pass 1 of " << std::max(1, AA_passes) << "...";
	Y_INFO << integratorName << ": " << passString.str() << yendl;
	if(intpb) intpb->setTag(passString.str().c_str());

	gTimer.addEvent("rendert");
	gTimer.start("rendert");
	imageFilm->init(AA_passes);

	maxDepth = 0.f;
	minDepth = 1e38f;

	if(scene->doDepth()) precalcDepths();

	preRender();

	renderPass(AA_samples, 0, false);
	for(int i=1; i<AA_passes; ++i)
	{
		if(scene->getSignals() & Y_SIG_ABORT) break;
		imageFilm->setAAThreshold(AA_threshold);
		imageFilm->nextPass(true, integratorName);
		renderPass(AA_inc_samples, AA_samples + (i-1)*AA_inc_samples, true);
	}
	maxDepth = 0.f;
	gTimer.stop("rendert");
	Y_INFO << integratorName << ": Overall rendertime: " << gTimer.getTime("rendert") << "s" << yendl;

	return true;
}


bool tiledIntegrator_t::renderPass(int samples, int offset, bool adaptive)
{
	prePass(samples, offset, adaptive);

	int nthreads = scene->getNumThreads();

#ifdef USING_THREADS
	if(nthreads>1)
	{
		threadControl_t tc;
		std::vector<renderWorker_t *> workers;
		for(int i=0;i<nthreads;++i) workers.push_back(new renderWorker_t(this, scene, imageFilm, &tc, i, samples, offset, adaptive));
		for(int i=0;i<nthreads;++i)
		{
			workers[i]->run();
		}
		//update finished tiles
		tc.countCV.lock();
		while(tc.finishedThreads < nthreads)
		{
			tc.countCV.wait();
			for(size_t i=0; i<tc.areas.size(); ++i) imageFilm->finishArea(tc.areas[i]);
			tc.areas.clear();
		}
		tc.countCV.unlock();
		//join all threads (although they probably have exited already, but not necessarily):
		for(int i=0;i<nthreads;++i) delete workers[i];
	}
	else
	{
#endif
		renderArea_t a;
		while(imageFilm->nextArea(a))
		{
			if(scene->getSignals() & Y_SIG_ABORT) break;
			preTile(a, samples, offset, adaptive, 0);
			renderTile(a, samples, offset, adaptive, 0);
			imageFilm->finishArea(a);
		}
#ifdef USING_THREADS
	}
#endif
	return true; //hm...quite useless the return value :)
}

bool tiledIntegrator_t::renderTile(renderArea_t &a, int n_samples, int offset, bool adaptive, int threadID)
{
	int x;
	const camera_t* camera = scene->getCamera();
	bool do_depth = scene->doDepth();
	x=camera->resX();
	diffRay_t c_ray;
	ray_t d_ray;
	PFLOAT dx=0.5, dy=0.5, d1=1.0/(PFLOAT)n_samples;
	float lens_u=0.5f, lens_v=0.5f;
	PFLOAT wt, wt_dummy;
	random_t prng(offset*(x*a.Y+a.X)+123);
	renderState_t rstate(&prng);
	rstate.threadID = threadID;
	rstate.cam = camera;
	bool sampleLns = camera->sampleLense();
	int pass_offs=offset, end_x=a.X+a.W, end_y=a.Y+a.H;

	Halton halU(3);
	Halton halV(5);

	for(int i=a.Y; i<end_y; ++i)
	{
		for(int j=a.X; j<end_x; ++j)
		{
			if(scene->getSignals() & Y_SIG_ABORT) break;

			if(adaptive)
			{
				if(!imageFilm->doMoreSamples(j, i)) continue;
			}

			rstate.pixelNumber = x*i+j;
			rstate.samplingOffs = fnv_32a_buf(i*fnv_32a_buf(j));//fnv_32a_buf(rstate.pixelNumber);
			float toff = scrHalton(5, pass_offs+rstate.samplingOffs); // **shall be just the pass number...**

			halU.setStart(pass_offs+rstate.samplingOffs);
			halV.setStart(pass_offs+rstate.samplingOffs);

			for(int sample=0; sample<n_samples; ++sample)
			{
				rstate.setDefaults();
				rstate.pixelSample = pass_offs+sample;
				rstate.time = addMod1((PFLOAT)sample*d1, toff);//(0.5+(PFLOAT)sample)*d1;

				// the (1/n, Larcher&Pillichshammer-Seq.) only gives good coverage when total sample count is known
				// hence we use scrambled (Sobol, van-der-Corput) for multipass AA
				if(AA_passes>1)
				{
					dx = RI_vdC(rstate.pixelSample, rstate.samplingOffs);
					dy = RI_S(rstate.pixelSample, rstate.samplingOffs);
				}
				else if(n_samples > 1)
				{
					dx = (0.5+(PFLOAT)sample)*d1;
					dy = RI_LP(sample+rstate.samplingOffs);
				}
				if(sampleLns)
				{
					lens_u = halU.getNext();
					lens_v = halV.getNext();
				}
				c_ray = camera->shootRay(j+dx, i+dy, lens_u, lens_v, wt);
				if(wt==0.0)
				{
					imageFilm->addSample(colorA_t(0.f), j, i, dx, dy, &a);
					continue;
				}
				//setup ray differentials
				d_ray = camera->shootRay(j+1+dx, i+dy, lens_u, lens_v, wt_dummy);
				c_ray.xfrom = d_ray.from;
				c_ray.xdir = d_ray.dir;
				d_ray = camera->shootRay(j+dx, i+1+dy, lens_u, lens_v, wt_dummy);
				c_ray.yfrom = d_ray.from;
				c_ray.ydir = d_ray.dir;
				c_ray.time = rstate.time;
				c_ray.hasDifferentials = true;
				// col = T * L_o + L_v
				colorA_t col = integrate(rstate, c_ray); // L_o
				col *= scene->volIntegrator->transmittance(rstate, c_ray); // T
				col += scene->volIntegrator->integrate(rstate, c_ray); // L_v
				imageFilm->addSample(wt * col, j, i, dx, dy, &a);

				if(do_depth)
				{
					float depth = 0.f;
					if(c_ray.tmax > 0.f)
					{
						depth = 1.f - (c_ray.tmax - minDepth) * maxDepth; // Distance normalization
					}

					imageFilm->addDepthSample(0, depth, j, i, dx, dy);
				}
			}
		}
	}
	return true;
}

__END_YAFRAY
