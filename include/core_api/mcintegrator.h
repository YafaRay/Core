/****************************************************************************
 *		mcintegrator.h: A basic abstract integrator for MC sampling
 *		This is part of the yafray package
 *		Copyright (C) 2010  Rodrigo Placencia (DarkTide)
 *
 *		This library is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU Lesser General Public
 *		License as published by the Free Software Foundation; either
 *		version 2.1 of the License, or (at your option) any later version.
 *
 *		This library is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *		Lesser General Public License for more details.
 *
 *		You should have received a copy of the GNU Lesser General Public
 *		License along with this library; if not, write to the Free Software
 *		Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef Y_MCINTEGRATOR_H
#define Y_MCINTEGRATOR_H

#include <core_api/tiledintegrator.h>
#include <yafraycore/photon.h>

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT mcIntegrator_t: public tiledIntegrator_t
{
	protected:
		/*! Estimates direct light from all sources in a mc fashion and completing MIS (Multiple Importance Sampling) for a given surface point */
		virtual color_t estimateAllDirectLight(renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo) const;
		/*! Like previous but for only one random light source for a given surface point */
		virtual color_t estimateOneDirectLight(renderState_t &state, const surfacePoint_t &sp, vector3d_t wo, int n) const;
		/*! Does the actual light estimation on a specific light for the given surface point */
		virtual color_t doLightEstimation(renderState_t &state, light_t *light, const surfacePoint_t &sp, const vector3d_t &wo, const unsigned int &loffs) const;
		/*! Does recursive mc raytracing with MIS (Multiple Importance Sampling) for a given surface point */
		virtual void recursiveRaytrace(renderState_t &state, diffRay_t &ray, BSDF_t bsdfs, surfacePoint_t &sp, vector3d_t &wo, color_t &col, float &alpha) const;
		/*! Creates and prepares the caustic photon map */
		virtual bool createCausticMap();
		/*! Estimates caustic photons for a given surface point */
		virtual color_t estimateCausticPhotons(renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo) const;
		/*! Samples ambient occlusion for a given surface point */
		virtual color_t sampleAmbientOcclusion(renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo) const;
		
		int rDepth; //! Ray depth
		bool trShad; //! Use transparent shadows
		int sDepth; //! Shadow depth for transparent shadows

		bool usePhotonCaustics; //! Use photon caustics
		unsigned int nCausPhotons; //! Number of caustic photons (to be shoot but it should be the target
		int nCausSearch; //! Amount of caustic photons to be gathered in estimation
		float causRadius; //! Caustic search radius for estimation
		int causDepth; //! Caustic photons max path depth
		photonMap_t causticMap; //! Container for the caustic photon map
		pdf1D_t *lightPowerD;
		
		bool useAmbientOcclusion; //! Use ambient occlusion
		int aoSamples; //! Ambient occlusion samples
		float aoDist; //! Ambient occlusion distance
		color_t aoCol; //! Ambient occlusion color
		
		
		background_t *background; //! Background shader
		int nPaths; //! Number of samples for mc raytracing
		int maxBounces; //! Max. path depth for mc raytracing
		std::vector<light_t*> lights; //! An array containing all the scene lights
		bool transpBackground; //! Render background as transparent
		bool transpRefractedBackground; //! Render refractions of background as transparent
};

__END_YAFRAY

#endif
