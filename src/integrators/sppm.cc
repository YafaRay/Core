#include <integrators/sppm.h>
#include <yafraycore/scr_halton.h>
#include <sstream>
#include <cmath>
#include <algorithm>

__BEGIN_YAFRAY

const int nMaxGather = 1000; //used to gather all the photon in the radius. seems could get a better way to do that

SPPM::SPPM(unsigned int dPhotons, int _passnum, bool transpShad, int shadowDepth)
{
	type = SURFACE;
	integratorName = "SPPM";
	integratorShortName = "SPPM";
	nPhotons = dPhotons;
	passNum = _passnum;
	totalnPhotons = 0;
	initialFactor = 1.f;

	sDepth = shadowDepth;
	trShad = transpShad;
	bHashgrid = false;

	hal1.setBase(2);
	hal2.setBase(3);
	hal3.setBase(5);
	hal4.setBase(7);

	hal1.setStart(0);
	hal2.setStart(0);
	hal3.setStart(0);
	hal4.setStart(0);
}

SPPM::~SPPM()
{

}

bool SPPM::preprocess()
{
	return true;
}

bool SPPM::render(yafaray::imageFilm_t *image)
{
	std::stringstream passString;
	std::stringstream SettingsSPPM;
	imageFilm = image;

	passString << "Rendering pass 1 of " << std::max(1, passNum) << "...";
	Y_INFO << integratorName << ": " << passString.str() << yendl;
	if(intpb) intpb->setTag(passString.str().c_str());

	gTimer.addEvent("rendert");
	gTimer.start("rendert");
	imageFilm->init(passNum);

	const camera_t* camera = scene->getCamera();

	maxDepth = 0.f;
	minDepth = 1e38f;

	if(scene->doDepth() && scene->normalizedDepth()) precalcDepths();

	initializePPM(); // seems could integrate into the preRender
	renderPass(1, 0, false);
	PM_IRE = false;

	int hpNum = camera->resX() * camera->resY();
	int passInfo = 1;
	for(int i=1; i<passNum; ++i) //progress pass, the offset start from 1 as it is 0 based.
	{
		if(scene->getSignals() & Y_SIG_ABORT) break;
		passInfo = i+1;
		imageFilm->nextPass(false, integratorName);
		nRefined = 0;
		renderPass(1, 1 + (i-1)*1, false); // offset are only related to the passNum, since we alway have only one sample.
		Y_INFO <<  integratorName << ": This pass refined " << nRefined << " of " << hpNum << " pixels." << yendl;
	}
	maxDepth = 0.f;
	gTimer.stop("rendert");
	Y_INFO << integratorName << ": Overall rendertime: "<< gTimer.getTime("rendert") << "s." << yendl;

	// Integrator Settings for "drawRenderSettings()" in imageFilm, SPPM has own render method, so "getSettings()"
	// in integrator.h has no effect and Integrator settings won't be printed to the parameter badge.
	SettingsSPPM << integratorName << " (" << settings << "; Passes rendered: " << passInfo << ")";
	imageFilm->setIntegParams(SettingsSPPM.str());
	return true;
}


bool SPPM::renderTile(renderArea_t &a, int n_samples, int offset, bool adaptive, int threadID)
{
	int x;
	const camera_t* camera = scene->getCamera();
	bool do_depth = scene->doDepth();
	bool normalizedDepth = scene->normalizedDepth();
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

	for(int i=a.Y; i<end_y; ++i)
	{
		for(int j=a.X; j<end_x; ++j)
		{
			if(scene->getSignals() & Y_SIG_ABORT) break;

			rstate.pixelNumber = x*i+j;
			rstate.samplingOffs = fnv_32a_buf(i*fnv_32a_buf(j));//fnv_32a_buf(rstate.pixelNumber);
			float toff = scrHalton(5, pass_offs+rstate.samplingOffs); // **shall be just the pass number...**

			for(int sample=0; sample<n_samples; ++sample) //set n_samples = 1.
			{
				rstate.setDefaults();
				rstate.pixelSample = pass_offs+sample;
				rstate.time = addMod1((PFLOAT)sample*d1, toff); //(0.5+(PFLOAT)sample)*d1;
				// the (1/n, Larcher&Pillichshammer-Seq.) only gives good coverage when total sample count is known
				// hence we use scrambled (Sobol, van-der-Corput) for multipass AA

				dx = RI_vdC(rstate.pixelSample, rstate.samplingOffs);
				dy = RI_S(rstate.pixelSample, rstate.samplingOffs);

				if(sampleLns)
				{
					lens_u = scrHalton(3, rstate.pixelSample+rstate.samplingOffs);
					lens_v = scrHalton(4, rstate.pixelSample+rstate.samplingOffs);
				}
				c_ray = camera->shootRay(j+dx, i+dy, lens_u, lens_v, wt); // wt need to be considered
				if(wt==0.0)
				{
					imageFilm->addSample(colorA_t(0.f), j, i, dx, dy, &a); //maybe not need
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
				diffRay_t c_ray_copy = c_ray;

				//for sppm progressive
				int index = i*camera->resX() + j;
				HitPoint &hp = hitPoints[index];

				GatherInfo gInfo = traceGatherRay(rstate, c_ray, hp);
				gInfo.photonFlux *= scene->volIntegrator->transmittance(rstate, c_ray);

				gInfo.constantRandiance *= scene->volIntegrator->transmittance(rstate, c_ray);
				gInfo.constantRandiance += scene->volIntegrator->integrate(rstate, c_ray); // Now using it to simulate for volIntegrator not using PPM, need more tests
				hp.constantRandiance += gInfo.constantRandiance; // accumalate the constant randiance for later useage.

				// progressive refinement
				const float _alpha = 0.7f; // another common choice is 0.8, seems not changed much.

				// The author's refine formular
				if(gInfo.photonCount > 0)
				{
					float g = std::min((hp.accPhotonCount + _alpha * gInfo.photonCount) / (hp.accPhotonCount + gInfo.photonCount), 1.0f);
					hp.radius2 *= g;
					hp.accPhotonCount += gInfo.photonCount * _alpha;
					hp.accPhotonFlux = (hp.accPhotonFlux + gInfo.photonFlux) * g;
					nRefined++; // record the pixel that has refined.
				}

				//radiance estimate
				colorA_t color = hp.accPhotonFlux / (hp.radius2 * M_PI * totalnPhotons);
				color += gInfo.constantRandiance;
				color.A = gInfo.constantRandiance.A; //the alpha value is hold in the constantRadiance variable

				imageFilm->addSample(wt * color, j, i, dx, dy, &a);

				if(do_depth)
				{
					if(normalizedDepth)
                    {
                        float depth = 0.f;
                        if(c_ray.tmax > 0.f)
                        {
                            depth = 1.f - (c_ray.tmax - minDepth) * maxDepth; // Distance normalization
                        }

                        imageFilm->addDepthSample(0, depth, j, i, dx, dy);
                    }
                    else
                    {
                        imageFilm->addDepthSample(0, c_ray.tmax, j, i, dx, dy);
                    }
				}
            }
		}
	}
	return true;
}

//photon pass, scatter photon
void SPPM::prePass(int samples, int offset, bool adaptive)
{
	std::stringstream set;
	gTimer.addEvent("prePass");
	gTimer.start("prePass");

	Y_INFO << integratorName << ": Starting Photon tracing pass..." << yendl;

	if(trShad)
	{
		set << "ShadowDepth [" << sDepth << "]";
	}
	if(!set.str().empty()) set << "+";
	set << "RayDepth [" << rDepth << "]";

	if(bHashgrid) photonGrid.clear();
	else {diffuseMap.clear(); causticMap.clear();}

	background = scene->getBackground();
	lights = scene->lights;
	std::vector<light_t*> tmplights;

	//background do not emit photons, or it is merged into normal light?
	settings = set.str();

	ray_t ray;
	float lightNumPdf, lightPdf, s1, s2, s3, s4, s5, s6, s7, sL;
	int numDLights = 0;
	float fNumLights = 0.f;
	float *energies = NULL;
	color_t pcol;

	tmplights.clear();

	for(int i=0; i<(int)lights.size(); ++i)
	{
		numDLights++;
		tmplights.push_back(lights[i]);
	}

	fNumLights = (float)numDLights;
	energies = new float[numDLights];

	for (int i=0; i<numDLights; ++i) energies[i] = tmplights[i]->totalEnergy().energy();

	lightPowerD = new pdf1D_t(energies, numDLights);

	Y_INFO << integratorName << ": Light(s) photon color testing for photon map:" << yendl;

	for(int i=0;i<numDLights;++i)
	{
		pcol = tmplights[i]->emitPhoton(.5, .5, .5, .5, ray, lightPdf);
		lightNumPdf = lightPowerD->func[i] * lightPowerD->invIntegral;
		pcol *= fNumLights * lightPdf / lightNumPdf; //remember that lightPdf is the inverse of the pdf, hence *=...
		Y_INFO << integratorName << ": Light [" << i+1 << "] Photon col:" << pcol << " | lnpdf: " << lightNumPdf << yendl;
	}

	delete[] energies;

	//shoot photons
	bool done=false;
	unsigned int curr=0;

	surfacePoint_t sp;
	random_t prng(offset*(4517)+123);
	renderState_t state(&prng);
	unsigned char userdata[USER_DATA_SIZE+7];
	state.userdata = (void *)( &userdata[7] - ( ((size_t)&userdata[7])&7 ) ); // pad userdata to 8 bytes
	state.cam = scene->getCamera();
	progressBar_t *pb;
	int pbStep;
	if(intpb) pb = intpb;
	else pb = new ConsoleProgressBar_t(80);

	if(bHashgrid) Y_INFO << integratorName << ": Building photon hashgrid..." << yendl;
	else Y_INFO << integratorName << ": Building photon map..." << yendl;

	pb->init(128);
	pbStep = std::max(1U, nPhotons/128);
	//pb->setTag("Building photon map...");

	//Pregather  photons
	float invDiffPhotons = 1.f / (float)nPhotons;

	unsigned int ndPhotonStored = 0;
//	unsigned int ncPhotonStored = 0;

	while(!done)
	{
		if(scene->getSignals() & Y_SIG_ABORT) {  pb->done(); if(!intpb) delete pb; return; }
		state.chromatic = true;
		state.wavelength = scrHalton(5, curr);

	   // Tried LD, get bad and strange results for some stategy.
       s1 = hal1.getNext();
	   s2 = hal2.getNext();
       s3 = hal3.getNext();
       s4 = hal4.getNext();

		sL = float(curr) * invDiffPhotons; // Does sL also need more random for each pass?
		int lightNum = lightPowerD->DSample(sL, &lightNumPdf);
		if(lightNum >= numDLights){ Y_ERROR << integratorName << ": lightPDF sample error! "<<sL<<"/"<<lightNum<<"... stopping now.\n"; delete lightPowerD; return; }

		pcol = tmplights[lightNum]->emitPhoton(s1, s2, s3, s4, ray, lightPdf);
		ray.tmin = MIN_RAYDIST;
		ray.tmax = -1.0;
		pcol *= fNumLights*lightPdf/lightNumPdf; //remember that lightPdf is the inverse of th pdf, hence *=...

		if(pcol.isBlack())
		{
			++curr;
			done = (curr >= nPhotons);
			continue;
		}

		int nBounces=0;
		bool causticPhoton = false;
		bool directPhoton = true;
		const material_t *material = NULL;
		BSDF_t bsdfs;

		while( scene->intersect(ray, sp) ) //scatter photons.
		{
			if(isnan(pcol.R) || isnan(pcol.G) || isnan(pcol.B))
			{ Y_WARNING << integratorName << ": NaN  on photon color for light" << lightNum + 1 << ".\n"; continue; }

			color_t transm(1.f);
			color_t vcol(0.f);
			const volumeHandler_t* vol;

			if(material)
			{
				if((bsdfs&BSDF_VOLUMETRIC) && (vol=material->getVolumeHandler(sp.Ng * -ray.dir < 0)))
				{
					if(vol->transmittance(state, ray, vcol)) transm = vcol;
				}
			}

			vector3d_t wi = -ray.dir, wo;
			material = sp.material;
			material->initBSDF(state, sp, bsdfs);

			//deposit photon on diffuse surface, now we only have one map for all, elimate directPhoton for we estimate it directly
			if(!directPhoton && !causticPhoton && (bsdfs & (BSDF_DIFFUSE)))
			{
				photon_t np(wi, sp.P, pcol);// pcol used here

				if(bHashgrid) photonGrid.pushPhoton(np);
				else
				{
					diffuseMap.pushPhoton(np);
					diffuseMap.setNumPaths(curr);
				}
				ndPhotonStored++;
			}
			// add caustic photon
			if(!directPhoton && causticPhoton && (bsdfs & (BSDF_DIFFUSE | BSDF_GLOSSY)))
			{
				photon_t np(wi, sp.P, pcol);// pcol used here

				if(bHashgrid) photonGrid.pushPhoton(np);
				else
				{
					causticMap.pushPhoton(np);
					causticMap.setNumPaths(curr);
				}
				ndPhotonStored++;
			}

			// need to break in the middle otherwise we scatter the photon and then discard it => redundant
			if(nBounces == maxBounces) break;

			// scatter photon
			s5 = ourRandom(); // now should use this to see correctness
			s6 = ourRandom();
			s7 = ourRandom();

			pSample_t sample(s5, s6, s7, BSDF_ALL, pcol, transm);

			bool scattered = material->scatterPhoton(state, sp, wi, wo, sample);
			if(!scattered) break; //photon was absorped.  actually based on russian roulette

			pcol = sample.color;

			causticPhoton = ((sample.sampledFlags & (BSDF_GLOSSY | BSDF_SPECULAR | BSDF_DISPERSIVE)) && directPhoton) ||
							((sample.sampledFlags & (BSDF_GLOSSY | BSDF_SPECULAR | BSDF_FILTER | BSDF_DISPERSIVE)) && causticPhoton);
			directPhoton = (sample.sampledFlags & BSDF_FILTER) && directPhoton;


			ray.from = sp.P;
			ray.dir = wo;
			ray.tmin = MIN_RAYDIST;
			ray.tmax = -1.0;
			++nBounces;

		}
		++curr;
		if(curr % pbStep == 0) pb->update();
		done = (curr >= nPhotons);
	}
	pb->done();
	//pb->setTag("Photon map built.");
	Y_INFO << integratorName << ":Photon map built." << yendl;
	Y_INFO << integratorName << ": Shot " << curr << " photons from " << numDLights << " light(s)" << yendl;
	delete lightPowerD;

	totalnPhotons +=  nPhotons;	// accumulate the total photon number, not using nPath for the case of hashgrid.

	Y_INFO << integratorName << ": Stored photons: "<< diffuseMap.nPhotons() + causticMap.nPhotons() << yendl;

	if(bHashgrid)
	{
		Y_INFO << integratorName << ": Building photons hashgrid:" << yendl;
		photonGrid.updateGrid();
		Y_INFO << integratorName << ": Done." << yendl;
	}
	else
	{
		if(diffuseMap.nPhotons() > 0)
		{
			Y_INFO << integratorName << ": Building diffuse photons kd-tree:" << yendl;
			diffuseMap.updateTree();
			Y_INFO << integratorName << ": Done." << yendl;
		}
		if(causticMap.nPhotons() > 0)
		{
			Y_INFO << integratorName << ": Building caustic photons kd-tree:" << yendl;
			causticMap.updateTree();
			Y_INFO << integratorName << ": Done." << yendl;
		}
		if(diffuseMap.nPhotons() < 50)
		{
			Y_ERROR << integratorName << ": Too few photons, stopping now." << yendl;
			return;
		}
	}

	tmplights.clear();

	if(!intpb) delete pb;

	gTimer.stop("prePass");

	if(bHashgrid)
		Y_INFO << integratorName << ": PhotonGrid building time: " << gTimer.getTime("prePass") << yendl;
	else
		Y_INFO << integratorName << ": PhotonMap building time: " << gTimer.getTime("prePass") << yendl;

	return;
}

//now it's a dummy function
colorA_t SPPM::integrate(renderState_t &state, diffRay_t &ray/*, sampler_t &sam*/) const
{
	return colorA_t(0.f);
}


GatherInfo SPPM::traceGatherRay(yafaray::renderState_t &state, yafaray::diffRay_t &ray, yafaray::HitPoint &hp)
{
	static int _nMax=0;
	static int calls=0;
	++calls;
	color_t col(0.0);
	GatherInfo gInfo;

	CFLOAT alpha;
	surfacePoint_t sp;

	void *o_udat = state.userdata;
	bool oldIncludeLights = state.includeLights;

	if(transpBackground) alpha=0.0;
	else alpha=1.0;

	if(scene->intersect(ray, sp))
	{
		unsigned char userdata[USER_DATA_SIZE+7];
		state.userdata = (void *)( &userdata[7] - ( ((size_t)&userdata[7])&7 ) ); // pad userdata to 8 bytes
		if(state.raylevel == 0)
		{
			state.chromatic = true;
			state.includeLights = true;
		}

		BSDF_t bsdfs;
		vector3d_t N_nobump = sp.N;
		vector3d_t wo = -ray.dir;
		const material_t *material = sp.material;
		material->initBSDF(state, sp, bsdfs);
		gInfo.constantRandiance += material->emit(state, sp, wo); //add only once, but FG seems add twice?
		state.includeLights = false;
		spDifferentials_t spDiff(sp, ray);

		if(bsdfs & BSDF_DIFFUSE)
		{
			gInfo.constantRandiance += estimateAllDirectLight(state, sp, wo);
		}

		// estimate radiance using photon map
		foundPhoton_t *gathered = new foundPhoton_t[nMaxGather];

		//if PM_IRE is on. we should estimate the initial radius using the photonMaps. (PM_IRE is only for the first pass, so not consume much time)
		if(PM_IRE && !hp.radiusSetted) // "waste" two gather here as it has two maps now. This make the logic simple.
		{
			PFLOAT radius_1 = dsRadius * dsRadius;
			PFLOAT radius_2 = radius_1;
			int nGathered_1 = 0, nGathered_2 = 0;

			if(diffuseMap.nPhotons() > 0)
				nGathered_1 = diffuseMap.gather(sp.P, gathered, nSearch, radius_1);
			if(causticMap.nPhotons() > 0)
				nGathered_2 = causticMap.gather(sp.P, gathered, nSearch, radius_2);
			if(nGathered_1 > 0 || nGathered_2 >0) // it none photon gathered, we just skip.
			{
				if(radius_1 < radius_2) // we choose the smaller one to be the initial radius.
					hp.radius2 = radius_1;
				else
					hp.radius2 = radius_2;

				hp.radiusSetted = true;
			}
		}

		int nGathered=0;
		PFLOAT radius2 = hp.radius2;

		if(bHashgrid)
			nGathered = photonGrid.gather(sp.P, gathered, nMaxGather, radius2); // disable now
		else
		{
			if(diffuseMap.nPhotons() > 0) // this is needed to avoid a runtime error.
			{
				nGathered = diffuseMap.gather(sp.P, gathered, nMaxGather, radius2); //we always collected all the photon inside the radius
			}

			if(nGathered > 0)
			{
				if(nGathered > _nMax)
				{
					_nMax = nGathered;
					std::cout << "maximum Photons: "<<_nMax<<", radius2: "<<radius2<<"\n";
					if(_nMax == 10) for(int j=0; j < nGathered; ++j ) std::cout<<"col:"<<gathered[j].photon->color()<<"\n";
				}
				for(int i=0; i<nGathered; ++i)
				{
					////test if the photon is in the ellipsoid
					//vector3d_t scale  = sp.P - gathered[i].photon->pos;
					//vector3d_t temp;
					//temp.x = scale VDOT sp.NU;
					//temp.y = scale VDOT sp.NV;
					//temp.z = scale VDOT sp.N;

					//double inv_radi = 1 / sqrt(radius2);
					//temp.x  *= inv_radi; temp.y *= inv_radi; temp.z *=  1. / (2.f * MIN_RAYDIST);
					//if(temp.lengthSqr() > 1.)continue;

					gInfo.photonCount++;
					vector3d_t pdir = gathered[i].photon->direction();
					color_t surfCol = material->eval(state, sp, wo, pdir, BSDF_DIFFUSE); // seems could speed up using rho, (something pbrt made)
					gInfo.photonFlux += surfCol * gathered[i].photon->color();// * std::fabs(sp.N*pdir); //< wrong!?
					//color_t  flux= surfCol * gathered[i].photon->color();// * std::fabs(sp.N*pdir); //< wrong!?

					////start refine here
					//double ALPHA = 0.7;
					//double g = (hp.accPhotonCount*ALPHA+ALPHA) / (hp.accPhotonCount*ALPHA+1.0);
					//hp.radius2 *= g;
					//hp.accPhotonCount++;
					//hp.accPhotonFlux=((color_t)hp.accPhotonFlux+flux)*g;
				}
			}

			// gather caustics photons
			if(bsdfs & BSDF_DIFFUSE && causticMap.ready())
			{

				radius2 = hp.radius2; //reset radius2 & nGathered
				nGathered = causticMap.gather(sp.P, gathered, nMaxGather, radius2);
				if(nGathered > 0)
				{
					color_t surfCol(0.f);
					for(int i=0; i<nGathered; ++i)
					{
						vector3d_t pdir = gathered[i].photon->direction();
						gInfo.photonCount++;
						surfCol = material->eval(state, sp, wo, pdir, BSDF_ALL); // seems could speed up using rho, (something pbrt made)
						gInfo.photonFlux += surfCol * gathered[i].photon->color();// * std::fabs(sp.N*pdir); //< wrong!?
						//color_t  flux= surfCol * gathered[i].photon->color();// * std::fabs(sp.N*pdir); //< wrong!?

						////start refine here
						//double ALPHA = 0.7;
						//double g = (hp.accPhotonCount*ALPHA+ALPHA) / (hp.accPhotonCount*ALPHA+1.0);
						//hp.radius2 *= g;
						//hp.accPhotonCount++;
						//hp.accPhotonFlux=((color_t)hp.accPhotonFlux+flux)*g;
					}
				}
			}
		}
		delete [] gathered;

		state.raylevel++;
		if(state.raylevel <= rDepth)
		{
			Halton hal2(2);
			Halton hal3(3);
			// dispersive effects with recursive raytracing:
			if( (bsdfs & BSDF_DISPERSIVE) && state.chromatic )
			{
				state.includeLights = false; //debatable...
				int dsam = 8;
				int oldDivision = state.rayDivision;
				int oldOffset = state.rayOffset;
				float old_dc1 = state.dc1, old_dc2 = state.dc2;
				if(state.rayDivision > 1) dsam = std::max(1, dsam/oldDivision);
				state.rayDivision *= dsam;
				int branch = state.rayDivision*oldOffset;
				float d_1 = 1.f/(float)dsam;
				float ss1 = RI_S(state.pixelSample + state.samplingOffs);
				color_t dcol(0.f), vcol(1.f);
				vector3d_t wi;
				const volumeHandler_t *vol;
				diffRay_t refRay;
				float W = 0.f;
				GatherInfo cing, t_cing; //Dispersive is different handled, not same as GLOSSY, at the BSDF_VOLUMETRIC part

				for(int ns=0; ns<dsam; ++ns)
				{
					state.wavelength = (ns + ss1)*d_1;
					state.dc1 = scrHalton(2*state.raylevel+1, branch + state.samplingOffs);
					state.dc2 = scrHalton(2*state.raylevel+2, branch + state.samplingOffs);
					if(oldDivision > 1)  state.wavelength = addMod1(state.wavelength, old_dc1);
					state.rayOffset = branch;
					++branch;
					sample_t s(0.5f, 0.5f, BSDF_REFLECT|BSDF_TRANSMIT|BSDF_DISPERSIVE);
					color_t mcol = material->sample(state, sp, wo, wi, s, W);

					if(s.pdf > 1.0e-6f && (s.sampledFlags & BSDF_DISPERSIVE))
					{
						state.chromatic = false;
						color_t wl_col;
						wl2rgb(state.wavelength, wl_col);
						refRay = diffRay_t(sp.P, wi, MIN_RAYDIST);
						t_cing = traceGatherRay(state, refRay, hp);
						t_cing.photonFlux *= mcol * wl_col * W;
						t_cing.constantRandiance *= mcol * wl_col * W;
						state.chromatic = true;
					}
					cing += t_cing;
				}
				if((bsdfs&BSDF_VOLUMETRIC) && (vol=material->getVolumeHandler(sp.Ng * refRay.dir < 0)))
				{
					vol->transmittance(state, refRay, vcol);
					cing.photonFlux *= vcol;
					cing.constantRandiance *= vcol;
				}

				gInfo.constantRandiance += cing.constantRandiance * d_1;
				gInfo.photonFlux += cing.photonFlux * d_1;
				gInfo.photonCount += cing.photonCount * d_1;

				state.rayDivision = oldDivision;
				state.rayOffset = oldOffset;
				state.dc1 = old_dc1; state.dc2 = old_dc2;
			}

			// glossy reflection with recursive raytracing:  Pure GLOSSY material doesn't hold photons?

			if( bsdfs & BSDF_GLOSSY )
			{
				state.includeLights = false;
				int gsam = 8;
				int oldDivision = state.rayDivision;
				int oldOffset = state.rayOffset;
				float old_dc1 = state.dc1, old_dc2 = state.dc2;
				if(state.rayDivision > 1) gsam = std::max(1, gsam/oldDivision);
				state.rayDivision *= gsam;
				int branch = state.rayDivision*oldOffset;
				unsigned int offs = gsam * state.pixelSample + state.samplingOffs;
				float d_1 = 1.f/(float)gsam;
				color_t vcol(1.f);
				vector3d_t wi;
				const volumeHandler_t *vol;
				diffRay_t refRay;

				GatherInfo ging, t_ging;

				hal2.setStart(offs);
				hal3.setStart(offs);

				for(int ns=0; ns<gsam; ++ns)
				{
					state.dc1 = scrHalton(2*state.raylevel+1, branch + state.samplingOffs);
					state.dc2 = scrHalton(2*state.raylevel+2, branch + state.samplingOffs);
					state.rayOffset = branch;
					++offs;
					++branch;

					float s1 = hal2.getNext();
					float s2 = hal3.getNext();

					float W = 0.f;

					sample_t s(s1, s2, BSDF_ALL_GLOSSY);
					color_t mcol = material->sample(state, sp, wo, wi, s, W);

					if(s.sampledFlags & BSDF_GLOSSY)
					{
						refRay = diffRay_t(sp.P, wi, MIN_RAYDIST);
						if(s.sampledFlags & BSDF_REFLECT) spDiff.reflectedRay(ray, refRay);
						else if(s.sampledFlags & BSDF_TRANSMIT) spDiff.refractedRay(ray, refRay, material->getMatIOR());

						t_ging = traceGatherRay(state, refRay, hp);
						t_ging.photonFlux *=mcol * W;
						t_ging.constantRandiance *= mcol * W;
					}

					if((bsdfs&BSDF_VOLUMETRIC) && (vol=material->getVolumeHandler(sp.Ng * refRay.dir < 0)))
					{
						if(vol->transmittance(state, refRay, vcol))
						{
							t_ging.photonFlux *= vcol;
							t_ging.constantRandiance *= vcol;
						}
					}
					ging += t_ging;
				}

				gInfo.constantRandiance += ging.constantRandiance * d_1;
				gInfo.photonFlux += ging.photonFlux * d_1;
				gInfo.photonCount += ging.photonCount * d_1;

				state.rayDivision = oldDivision;
				state.rayOffset = oldOffset;
				state.dc1 = old_dc1;
				state.dc2 = old_dc2;
			}

			//...perfect specular reflection/refraction with recursive raytracing...
			if(bsdfs & (BSDF_SPECULAR | BSDF_FILTER))
			{
				state.includeLights = true;
				bool reflect=false, refract=false;
				vector3d_t dir[2];
				color_t rcol[2], vcol;
				material->getSpecular(state, sp, wo, reflect, refract, dir, rcol);
				const volumeHandler_t *vol;

				if(reflect)
				{
					diffRay_t refRay(sp.P, dir[0], MIN_RAYDIST);
					spDiff.reflectedRay(ray, refRay); // compute the ray differentaitl
					GatherInfo refg = traceGatherRay(state, refRay, hp);
					if((bsdfs&BSDF_VOLUMETRIC) && (vol=material->getVolumeHandler(sp.Ng * refRay.dir < 0)))
					{
						if(vol->transmittance(state, refRay, vcol))
						{
							refg.constantRandiance *= vcol;
							refg.photonFlux *= vcol;
						}
					}
					gInfo.constantRandiance += refg.constantRandiance * colorA_t(rcol[0]);
					gInfo.photonFlux += refg.photonFlux * colorA_t(rcol[0]);
					gInfo.photonCount += refg.photonCount;
				}
				if(refract)
				{
					diffRay_t refRay(sp.P, dir[1], MIN_RAYDIST);
					spDiff.refractedRay(ray, refRay, material->getMatIOR());
					GatherInfo refg = traceGatherRay(state, refRay, hp);
					if((bsdfs&BSDF_VOLUMETRIC) && (vol=material->getVolumeHandler(sp.Ng * refRay.dir < 0)))
					{
						if(vol->transmittance(state, refRay, vcol))
						{
							refg.constantRandiance *= vcol;
							refg.photonFlux *= vcol;
						}
					}
					gInfo.constantRandiance += refg.constantRandiance * colorA_t(rcol[1]);
					gInfo.photonFlux += refg.photonFlux * colorA_t(rcol[1]);
					gInfo.photonCount += refg.photonCount;
					alpha = refg.constantRandiance.A;
				}
			}
		}
		--state.raylevel;

		if(transpRefractedBackground)
		{
			CFLOAT m_alpha = material->getAlpha(state, sp, wo);
			alpha = m_alpha + (1.f-m_alpha)*alpha;
		}
		else alpha = 1.0;
	}

	else //nothing hit, return background
	{
		if(background)
		{
			gInfo.constantRandiance += (*background)(ray, state, false);
		}
	}

	state.userdata = o_udat;
	state.includeLights = oldIncludeLights;

	gInfo.constantRandiance.A = alpha; // a small trick for just hold the alpha value.

	return gInfo;
}

void SPPM::initializePPM()
{
	const camera_t* camera = scene->getCamera();
	unsigned int resolution = camera->resX() * camera->resY();

	hitPoints.reserve(resolution);
	bound_t bBox = scene->getSceneBound(); // Now using Scene Bound, this could get a bigger initial radius, and need more tests

	// initialize SPPM statistics
	float initialRadius = ((bBox.longX() + bBox.longY() + bBox.longZ()) / 3.f) / ((camera->resX() + camera->resY()) / 2.0f) * 2.f ;
	initialRadius = std::min(initialRadius, 1.f); //Fix the overflow bug
	for(unsigned int i = 0; i < resolution; i++)
	{
		HitPoint hp;
		hp.accPhotonFlux  = colorA_t(0.f);
		hp.accPhotonCount = 0;
		hp.radius2 = (initialRadius * initialFactor) * (initialRadius * initialFactor);
		hp.constantRandiance = colorA_t(0.f);
		hp.radiusSetted = false;	   // the flag used for IRE

		hitPoints.push_back(hp);
	}

	if(bHashgrid) photonGrid.setParm(initialRadius*2.f, nPhotons, bBox);

}

integrator_t* SPPM::factory(paraMap_t &params, renderEnvironment_t &render)
{
	bool transpShad=false;
	bool pmIRE = false;
	int shadowDepth=5; //may used when integrate Direct Light
	int raydepth=5;
	int _passNum = 1000;
	int numPhotons = 500000;
	int bounces = 5;
	float times = 1.f;
	int searchNum = 100;
	float dsRad = 1.0f;
	bool bg_transp = true;
	bool bg_transp_refract = true;

	params.getParam("transpShad", transpShad);
	params.getParam("shadowDepth", shadowDepth);
	params.getParam("raydepth", raydepth);
	params.getParam("photons", numPhotons);
	params.getParam("passNums", _passNum);
	params.getParam("bounces", bounces);
	params.getParam("times", times); // initial radius times

	params.getParam("photonRadius", dsRad);
	params.getParam("searchNum", searchNum);
	params.getParam("pmIRE", pmIRE);

	params.getParam("bg_transp", bg_transp);
	params.getParam("bg_transp_refract", bg_transp_refract);

	SPPM* ite = new SPPM(numPhotons, _passNum,transpShad, shadowDepth);
	ite->rDepth = raydepth;
	ite->maxBounces = bounces;
	ite->initialFactor = times;

	ite->dsRadius = dsRad; // under tests enable now
	ite->nSearch = searchNum;
	ite->PM_IRE = pmIRE;
	// Background settings
	ite->transpBackground = bg_transp;
	ite->transpRefractedBackground = bg_transp_refract;

	return ite;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("SPPM", SPPM::factory);
	}

}
__END_YAFRAY
