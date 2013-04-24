/****************************************************************************
 * 			directlight.cc: an integrator for direct lighting only
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
 *      Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <yafray_config.h>
#include <core_api/environment.h>
#include <core_api/material.h>
#include <core_api/tiledintegrator.h>
#include <core_api/background.h>
#include <core_api/light.h>
#include <integrators/integr_utils.h>
#include <yafraycore/photon.h>
#include <utilities/mcqmc.h>
#include <yafraycore/scr_halton.h>

__BEGIN_YAFRAY

enum SurfaceProperties {N = 1, dPdU = 2, dPdV = 3, NU = 4, NV = 5, dSdU = 6, dSdV = 7};

class YAFRAYPLUGIN_EXPORT DebugIntegrator : public tiledIntegrator_t
{
	public:
		DebugIntegrator(SurfaceProperties dt);
		virtual bool preprocess();
		virtual colorA_t integrate(renderState_t &state, diffRay_t &ray/*, sampler_t &sam*/) const;
		static integrator_t* factory(paraMap_t &params, renderEnvironment_t &render);
	protected:
		std::vector<light_t*> lights;
		SurfaceProperties debugType;
		bool showPN;
};

DebugIntegrator::DebugIntegrator(SurfaceProperties dt)
{
	type = SURFACE;
	debugType = dt;
	integratorName = "DebugIntegrator";
	integratorShortName = "DBG";
	switch(dt)
	{
		case N:
		settings = "N";
		break;
		case dPdU:
		settings = "dPdU";
		break;
		case dPdV:
		settings = "dPdV";
		break;
		case NU:
		settings = "NU";
		break;
		case NV:
		settings = "NV";
		break;
		case dSdU:
		settings = "dSdU";
		break;
		case dSdV:
		settings = "dSdV";
		break;
	}
}

bool DebugIntegrator::preprocess()
{
	return true;
}

colorA_t DebugIntegrator::integrate(renderState_t &state, diffRay_t &ray/*, sampler_t &sam*/) const
{
	color_t col(0.0);
	surfacePoint_t sp;
	void *o_udat = state.userdata;
	bool oldIncludeLights = state.includeLights;
	//shoot ray into scene
	if(scene->intersect(ray, sp))
	{
		if (showPN){
			// Normals perturbed by materials
			unsigned char userdata[USER_DATA_SIZE+7];
			userdata[0] = 0;
			state.userdata = (void *)( &userdata[7] - ( ((size_t)&userdata[7])&7 ) ); // pad userdata to 8 bytes

			BSDF_t bsdfs;
			const material_t *material = sp.material;
			material->initBSDF(state, sp, bsdfs);
		}
		if (debugType == N)
			col = color_t((sp.N.x + 1.f) * .5f, (sp.N.y + 1.f) * .5f, (sp.N.z + 1.f) * .5f);
		else if (debugType == dPdU)
			col = color_t((sp.dPdU.x + 1.f) * .5f, (sp.dPdU.y + 1.f) * .5f, (sp.dPdU.z + 1.f) * .5f);
		else if (debugType == dPdV)
			col = color_t((sp.dPdV.x + 1.f) * .5f, (sp.dPdV.y + 1.f) * .5f, (sp.dPdV.z + 1.f) * .5f);
		else if (debugType == NU)
			col = color_t((sp.NU.x + 1.f) * .5f, (sp.NU.y + 1.f) * .5f, (sp.NU.z + 1.f) * .5f);
		else if (debugType == NV)
			col = color_t((sp.NV.x + 1.f) * .5f, (sp.NV.y + 1.f) * .5f, (sp.NV.z + 1.f) * .5f);
		else if (debugType == dSdU)
			col = color_t((sp.dSdU.x + 1.f) * .5f, (sp.dSdU.y + 1.f) * .5f, (sp.dSdU.z + 1.f) * .5f);
		else if (debugType == dSdV)
			col = color_t((sp.dSdV.x + 1.f) * .5f, (sp.dSdV.y + 1.f) * .5f, (sp.dSdV.z + 1.f) * .5f);

	}
	state.userdata = o_udat;
	state.includeLights = oldIncludeLights;
	return colorA_t(col, 1.f);
}

integrator_t* DebugIntegrator::factory(paraMap_t &params, renderEnvironment_t &render)
{
	int dt = 1;
	bool pn = false;
	params.getParam("debugType", dt);
	params.getParam("showPN", pn);
	std::cout << "debugType " << dt << std::endl;
	DebugIntegrator *inte = new DebugIntegrator((SurfaceProperties)dt);
	inte->showPN = pn;

	return inte;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("DebugIntegrator",DebugIntegrator::factory);
	}

}

__END_YAFRAY
