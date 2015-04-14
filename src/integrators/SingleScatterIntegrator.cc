#include <yafray_config.h>
#include <core_api/environment.h>
#include <core_api/material.h>
#include <core_api/integrator.h>
#include <core_api/background.h>
#include <core_api/light.h>
#include <integrators/integr_utils.h>
#include <yafraycore/photon.h>
#include <utilities/mcqmc.h>
#include <yafraycore/scr_halton.h>
#include <vector>
#include <stack>

__BEGIN_YAFRAY

class YAFRAYPLUGIN_EXPORT SingleScatterIntegrator : public volumeIntegrator_t
{
private:
	bool adaptive;
	bool optimize;
	float adaptiveStepSize;
	std::vector<VolumeRegion*> listVR;
	std::vector<light_t*> lights;
	unsigned int VRSize;
	float iVRSize;

public:

	SingleScatterIntegrator(float sSize, bool adapt, bool opt)
	{
		adaptive = adapt;
		stepSize = sSize;
		optimize = opt;
		adaptiveStepSize = sSize * 100.0f;

		Y_INFO << "SingleScatter: stepSize: " << stepSize << " adaptive: " << adaptive << " optimize: " << optimize << yendl;
	}

	virtual bool preprocess()
	{
		Y_INFO << "SingleScatter: Preprocessing..." << yendl;

		for(unsigned int i=0;i<scene->lights.size();++i)
		{
			lights.push_back(scene->lights[i]);
		}

		listVR = scene->getVolumes();
		VRSize = listVR.size();
		iVRSize = 1.f / (float)VRSize;
		
		if (optimize)
		{
			for (unsigned int i = 0; i < VRSize; i++)
			{
				VolumeRegion* vr = listVR.at(i);
				bound_t bb = vr->getBB();

				int xSize = vr->attGridX;
				int ySize = vr->attGridY;
				int zSize = vr->attGridZ;

				float xSizeInv = 1.f/(float)xSize;
				float ySizeInv = 1.f/(float)ySize;
				float zSizeInv = 1.f/(float)zSize;

				Y_INFO << "SingleScatter: volume, attGridMaps with size: " << xSize << " " << ySize << " " << xSize << std::endl;
			
				for(std::vector<light_t *>::const_iterator l=lights.begin(); l!=lights.end(); ++l)
				{
					color_t lcol(0.0);

					float* attenuationGrid = (float*)malloc(xSize * ySize * zSize * sizeof(float));
					vr->attenuationGridMap[(*l)] = attenuationGrid;

					for (int z = 0; z < zSize; ++z)
					{
						for (int y = 0; y < ySize; ++y)
						{
							for (int x = 0; x < xSize; ++x)
							{
								// generate the world position inside the grid
								point3d_t p(bb.longX() * xSizeInv * x + bb.a.x,
											bb.longY() * ySizeInv * y + bb.a.y,
											bb.longZ() * zSizeInv * z + bb.a.z);

								surfacePoint_t sp;
								sp.P = p;
								
								ray_t lightRay;

								lightRay.from = sp.P;

								// handle lights with delta distribution, e.g. point and directional lights
								if( (*l)->diracLight() )
								{
									bool ill = (*l)->illuminate(sp, lcol, lightRay);
									lightRay.tmin = scene->shadow_bias; // < better add some _smart_ self-bias value...this is bad.
									if (lightRay.tmax < 0.f) lightRay.tmax = 1e10; // infinitely distant light

									// transmittance from the point p in the volume to the light (i.e. how much light reaches p)
									color_t lightstepTau(0.f);
									if (ill)
									{
										for (unsigned int j = 0; j < VRSize; j++)
										{
											VolumeRegion* vr2 = listVR.at(j);
											lightstepTau += vr2->tau(lightRay, stepSize, 0.0f);
										}
									}

									float lightTr = fExp(-lightstepTau.energy());
									attenuationGrid[x + y * xSize + ySize * xSize * z] = lightTr;
								}
								else // area light and suchlike
								{
									float lightTr = 0;
									int n = (*l)->nSamples() >> 1; // samples / 2
									if (n < 1) n = 1;
									lSample_t ls;
									for(int i=0; i<n; ++i)
									{
										ls.s1 = 0.5f; //(*state.prng)();
										ls.s2 = 0.5f; //(*state.prng)();

										(*l)->illumSample(sp, ls, lightRay);
										lightRay.tmin = scene->shadow_bias;
										if (lightRay.tmax < 0.f) lightRay.tmax = 1e10; // infinitely distant light

										// transmittance from the point p in the volume to the light (i.e. how much light reaches p)
										color_t lightstepTau(0.f);
										for (unsigned int j = 0; j < VRSize; j++)
										{
											VolumeRegion* vr2 = listVR.at(j);
											lightstepTau += vr2->tau(lightRay, stepSize, 0.0f);
										}
										lightTr += fExp(-lightstepTau.energy());
									}

									attenuationGrid[x + y * xSize + ySize * xSize * z] = lightTr / (float)n;
								}
							}
						}
					}
				}
			}
		}

		return true;
	}
	
	color_t getInScatter(renderState_t& state, ray_t& stepRay, float currentStep) const
	{
		color_t inScatter(0.f);
		surfacePoint_t sp;
		sp.P = stepRay.from;

		ray_t lightRay;
		lightRay.from = sp.P;

		for(std::vector<light_t *>::const_iterator l=lights.begin(); l!=lights.end(); ++l)
		{
			color_t lcol(0.0);

			// handle lights with delta distribution, e.g. point and directional lights
			if( (*l)->diracLight() )
			{
				if( (*l)->illuminate(sp, lcol, lightRay) )
				{
					// ...shadowed...
					lightRay.tmin = scene->shadow_bias; // < better add some _smart_ self-bias value...this is bad.
					if (lightRay.tmax < 0.f) lightRay.tmax = 1e10; // infinitely distant light
					bool shadowed = scene->isShadowed(state, lightRay);
					if (!shadowed)
					{
						float lightTr = 0.0f;
						// replace lightTr with precalculated attenuation
						if (optimize)
						{
							// replaced by
							for (unsigned int i = 0; i < VRSize; i++)
							{
								VolumeRegion* vr = listVR.at(i);
								float t0Tmp = -1, t1Tmp = -1;
								if (vr->intersect(lightRay, t0Tmp, t1Tmp)) lightTr += vr->attenuation(sp.P, (*l)) * iVRSize;
							}
						}
						else
						{
							// replaced by
							color_t lightstepTau(0.f);
							for (unsigned int i = 0; i < VRSize; i++)
							{
								VolumeRegion* vr = listVR.at(i);
								float t0Tmp = -1, t1Tmp = -1;
								if (listVR.at(i)->intersect(lightRay, t0Tmp, t1Tmp))
								{
									lightstepTau += vr->tau(lightRay, currentStep, 0.f) * iVRSize;
								}
							}
							// transmittance from the point p in the volume to the light (i.e. how much light reaches p)
							lightTr = fExp(-lightstepTau.energy());
						}
						lightTr *= iVRSize;

						inScatter += lightTr * lcol;
					}
				}
			}
			else // area light and suchlike
			{
				int n = (*l)->nSamples() >> 2; // samples / 4
				if (n < 1) n = 1;
				float iN = 1.f / (float)n; // inverse of n
				color_t ccol(0.0);
				float lightTr = 0.0f;
				lSample_t ls;

				for(int i=0; i<n; ++i)
				{
					// ...get sample val...
					ls.s1 = (*state.prng)();
					ls.s2 = (*state.prng)();

					if((*l)->illumSample(sp, ls, lightRay))
					{
						// ...shadowed...
						lightRay.tmin = scene->shadow_bias; // < better add some _smart_ self-bias value...this is bad.
						if (lightRay.tmax < 0.f) lightRay.tmax = 1e10; // infinitely distant light
						bool shadowed = scene->isShadowed(state, lightRay);
						if(!shadowed) {
							ccol += ls.col / ls.pdf;

							// replace lightTr with precalculated attenuation
							if (optimize)
							{
								// replaced by
								for (unsigned int i = 0; i < VRSize; i++)
								{
									VolumeRegion* vr = listVR.at(i);
									float t0Tmp = -1, t1Tmp = -1;
									if (vr->intersect(lightRay, t0Tmp, t1Tmp))
									{
										lightTr += vr->attenuation(sp.P, (*l)) * iVRSize;
										break;
									}
								}
							}
							else
							{
								// replaced by
								color_t lightstepTau(0.f);
								for (unsigned int i = 0; i < VRSize; i++)
								{
									VolumeRegion* vr = listVR.at(i);
									float t0Tmp = -1, t1Tmp = -1;
									if (listVR.at(i)->intersect(lightRay, t0Tmp, t1Tmp))
									{
										lightstepTau += vr->tau(lightRay, currentStep * 4.f, 0.0f);
									}
								}
								// transmittance from the point p in the volume to the light (i.e. how much light reaches p)
								lightTr += fExp(-lightstepTau.energy()) * iVRSize;
							}

						}
					}
					lightTr *= iVRSize;
				} // end of area light sample loop

				lightTr *= iN;

				ccol = ccol * iN;
				inScatter += lightTr * ccol;
			} // end of area lights loop
		}

		return inScatter;
	}


	// optical thickness, absorption, attenuation, extinction
	virtual colorA_t transmittance(renderState_t &state, ray_t &ray) const
	{
		colorA_t Tr(1.f);
		//return Tr;
		
		if (VRSize == 0) return Tr;
		
		for (unsigned int i = 0; i < VRSize; i++)
		{
			VolumeRegion* vr = listVR.at(i);
			float t0 = -1, t1 = -1;
			if (vr->intersect(ray, t0, t1))
			{
				float random = (*state.prng)();
				color_t opticalThickness = vr->tau(ray, stepSize, random);
				Tr *= colorA_t(fExp(-opticalThickness.energy()));
			}
		}
		
		return Tr;
	}
	
	// emission and in-scattering
	virtual colorA_t integrate(renderState_t &state, ray_t &ray) const
	{
		float t0 = 1e10f, t1 = -1e10f;

		colorA_t result(0.f);
		//return result;
				
		if (VRSize == 0) return result;
		
		bool hit = (ray.tmax > 0.f);

		// find min t0 and max t1
		for (unsigned int i = 0; i < VRSize; i++)
		{
			float t0Tmp = 0.f, t1Tmp = 0.f;
			VolumeRegion* vr = listVR.at(i);

			if (!vr->intersect(ray, t0Tmp, t1Tmp)) continue;

			if (hit && ray.tmax < t0Tmp) continue;

			if (t0Tmp < 0.f) t0Tmp = 0.f;

			if (hit && ray.tmax < t1Tmp) t1Tmp = ray.tmax;

			if (t1Tmp > t1) t1 = t1Tmp;
			if (t0Tmp < t0) t0 = t0Tmp;
		}

		float dist = t1-t0;
		if (dist < 1e-3f) return result;

		float pos;
		int samples;
		pos = t0 - (*state.prng)() * stepSize; // start position of ray marching
		dist = t1 - pos;
		samples = dist / stepSize + 1;

		std::vector<float> densitySamples;
		std::vector<float> accumDensity;
		int adaptiveResolution = 1;
		
		if (adaptive)
		{
			adaptiveResolution = adaptiveStepSize / stepSize;

			densitySamples.resize(samples);
			accumDensity.resize(samples);

			accumDensity.at(0) = 0.f;
			for (int i = 0; i < samples; ++i)
			{
				point3d_t p = ray.from + (stepSize * i + pos) * ray.dir;

				float density = 0;
				for (unsigned int j = 0; j < VRSize; j++)
				{
					VolumeRegion* vr = listVR.at(j);
					density += vr->sigma_t(p, vector3d_t()).energy();
				}

				densitySamples.at(i) = density;
				if (i > 0)
					accumDensity.at(i) = accumDensity.at(i - 1) + density * stepSize;
			}
		}

		float adaptThresh = .01f;
		bool adaptNow = false;
		float currentStep = stepSize;
		int stepLength = 1;
		int stepToStopAdapt = -1;
		color_t trTmp(1.f); // transmissivity during ray marching

		if (adaptive)
		{
			currentStep = adaptiveStepSize;
			stepLength = adaptiveResolution;
		}

		color_t stepTau(0.f);
		int lookaheadSamples = adaptiveResolution / 10;

		for (int stepSample = 0; stepSample < samples; stepSample += stepLength)
		{
			if (adaptive)
			{
				if (!adaptNow)
				{
					int nextSample = (stepSample + adaptiveResolution > samples - 1) ? samples - 1 : stepSample + adaptiveResolution;
					if (std::fabs(accumDensity.at(stepSample) - accumDensity.at(nextSample)) > adaptThresh)
					{
						adaptNow = true;
						stepLength = 1;
						stepToStopAdapt = stepSample + lookaheadSamples;
						currentStep = stepSize;
					}
				}
			}

			ray_t stepRay(ray.from + (ray.dir * pos), ray.dir, 0, currentStep, 0);

			if (adaptive)
			{
				stepTau = accumDensity.at(stepSample);
			}
			else
			{
				for (unsigned int j = 0; j < VRSize; j++)
				{
					VolumeRegion* vr = listVR.at(j);
					float t0Tmp = -1, t1Tmp = -1;
					if (vr->intersect(stepRay, t0Tmp, t1Tmp))
					{
						stepTau += vr->sigma_t(stepRay.from, stepRay.dir) * currentStep;
					}
				}
			}

			trTmp = fExp(-stepTau.energy());

			if (optimize && trTmp.energy() < 1e-3f)
			{
				float random = (*state.prng)();
				if (random < 0.5f)
				{ 
					break;
				}
				trTmp = trTmp / random;
			}

			float sigma_s = 0.0f;
			for (unsigned int i = 0; i < VRSize; i++)
			{
				VolumeRegion* vr = listVR.at(i);
				float t0Tmp = -1, t1Tmp = -1;
				if (listVR.at(i)->intersect(stepRay, t0Tmp, t1Tmp))
				{
					sigma_s += vr->sigma_s(stepRay.from, stepRay.dir).energy();
				}
			}
			
			// with a sigma_s close to 0, no light can be scattered -> computation can be skipped
			
			if (optimize && sigma_s < 1e-3f)
			{
				float random = (*state.prng)();
				if (random < 0.5f)
				{
					pos += currentStep;
					continue;
				}
				sigma_s = sigma_s / random;
			}

			result += trTmp * getInScatter(state, stepRay, currentStep) * sigma_s * currentStep;

			if (adaptive)
			{
				if (adaptNow && stepSample >= stepToStopAdapt)
				{
					int nextSample = (stepSample + adaptiveResolution > samples - 1) ? samples - 1 : stepSample + adaptiveResolution;
					if (std::fabs(accumDensity.at(stepSample) - accumDensity.at(nextSample)) > adaptThresh)
					{
						// continue moving slowly ahead until the discontinuity is found
						stepToStopAdapt = stepSample + lookaheadSamples;
					}
					else
					{
						adaptNow = false;
						stepLength = adaptiveResolution;
						currentStep = adaptiveStepSize;
					}
				}
			}

			pos += currentStep;
		}
		result.A = 1.0f; // FIXME: get correct alpha value, does it even matter?
		return result;
	}
	
	static integrator_t* factory(paraMap_t &params, renderEnvironment_t &render)
	{
		bool adapt = false;
		bool opt = false;
		float sSize = 1.f;
		params.getParam("stepSize", sSize);
		params.getParam("adaptive", adapt);
		params.getParam("optimize", opt);
		SingleScatterIntegrator* inte = new SingleScatterIntegrator(sSize, adapt, opt);
		return inte;
	}

	float stepSize;

};

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("SingleScatterIntegrator", SingleScatterIntegrator::factory);
	}

}

__END_YAFRAY
