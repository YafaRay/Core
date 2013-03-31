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

__BEGIN_YAFRAY

float mieScatter(float theta) {
	theta *= 360 / M_2PI;
	if (theta < 1.f)
		return 4.192;
	if (theta < 4.f)
		return (1.f - ((theta - 1.f) / 3.f)) * 4.192 + ((theta - 1.f) / 3.f) * 3.311;
	if (theta < 7.f)
		return (1.f - ((theta - 4.f) / 3.f)) * 3.311 + ((theta - 4.f) / 3.f) * 2.860;
	if (theta < 10.f)
		return (1.f - ((theta - 7.f) / 3.f)) * 2.860 + ((theta - 7.f) / 3.f) * 2.518;
	if (theta < 30.f)
		return (1.f - ((theta - 10.f) / 20.f)) * 2.518 + ((theta - 10.f) / 20.f) * 1.122;
	if (theta < 60.f)
		return (1.f - ((theta - 30.f) / 30.f)) * 1.122 + ((theta - 30.f) / 30.f) * 0.3324;
	if (theta < 80.f)
		return (1.f - ((theta - 60.f) / 20.f)) * 0.3324 + ((theta - 60.f) / 20.f) * 0.1644;

	return (1.f - ((theta - 80.f) / 100.f)) * 0.1644f + ((theta - 80.f) / 100.f) * 0.1;
}

class YAFRAYPLUGIN_EXPORT SkyIntegrator : public volumeIntegrator_t {
	private:
		float stepSize;
		float alpha; // steepness of the exponential density
		float sigma_t; // beta in the paper, more or less the thickness coefficient
		float turbidity;
		background_t *background;
		float b_m, b_r;
		float alpha_r; // rayleigh, molecules
		float alpha_m; // mie, haze
		float scale;
		
	
	public:
	SkyIntegrator(float sSize, float a, float ss, float t) {
		stepSize = sSize;
		alpha = a;
		//sigma_t = ss;
		turbidity = t;
		scale = ss;

		alpha_r = 0.1136f * alpha; // rayleigh, molecules
		alpha_m = 0.8333f * alpha; // mie, haze

		// beta m for rayleigh scattering
		
		float N = 2.545e25f;
		float n = 1.0003f;
		float p_n = 0.035f;
		float l = 500e-9f;
		
		b_r = 8 * M_PI * M_PI * M_PI * (n * n - 1) * (n * n - 1) / (3 * N * l * l * l * l) * (6 + 3 * p_n) / (6 - 7 * p_n); // * sigma_t;

		// beta p for mie scattering
		
		float T = turbidity;
		float c = (0.6544 * T - 0.651) * 1e-16f;
		float v = 4.f;
		float K = 0.67f;

		b_m = 0.434 * c * M_PI * powf(2*M_PI/l, v-2) * K * 0.01; // * sigma_t; // FIXME: bad, 0.01 scaling to make it look better
		
		std::cout << "SkyIntegrator: b_m: " << b_m << " b_r: " << b_r << std::endl;
	}
	
	virtual bool preprocess() {
		background = scene->getBackground();
		return true;
	}

	colorA_t skyTau(const ray_t &ray) const {
		//std::cout << " ray.from: " << ray.from << " ray.dir: " << ray.dir << " ray.tmax: " << ray.tmax << " t0: " << t0 << " t1: " << t1 << std::endl;
		/*
		if (ray.tmax < t0 && ! (ray.tmax < 0)) return color_t(0.f);
		
		if (ray.tmax < t1 && ! (ray.tmax < 0)) t1 = ray.tmax;
		
		if (t0 < 0.f) t0 = 0.f;
		*/

		float dist;
		if (ray.tmax < 0.f) dist = 1000.f;
		else dist = ray.tmax;
		
		if (dist < 0.f)
			dist = 1000.f;

		float s = dist;

		color_t tauVal(0.f);

		float cos_theta = ray.dir.z; //vector3d_t(0.f, 0.f, 1.f) * ray.dir;

		float h0 = ray.from.z;
		
		/*
		float K = - sigma_t / (alpha * cos_theta);

		float H = exp(-alpha * h0);

		float u = exp(-alpha * (h0 + s * cos_theta));

		tauVal = colorA_t(K*(H-u));
		*/

		tauVal = colorA_t(sigma_t * exp(-alpha*h0) * (1.f - exp(-alpha * cos_theta * s)) / (alpha * cos_theta));

		//std::cout << tauVal.energy() << " " << cos_theta << " " << dist << " " << ray.tmax << std::endl;

		return tauVal;
		
		//return colorA_t(exp(-result.getR()), exp(-result.getG()), exp(-result.getB()));
	}
	
	colorA_t skyTau(const ray_t &ray, float beta, float alpha) const {
		float s;
		if (ray.tmax < 0.f) return colorA_t(0.f);

		s = ray.tmax * scale;
		
		color_t tauVal(0.f);

		float cos_theta = ray.dir.z;

		float h0 = ray.from.z * scale;
		
		tauVal = colorA_t(beta * exp(-alpha*h0) * (1.f - exp(-alpha * cos_theta * s)) / (alpha * cos_theta));
		//tauVal = colorA_t(-beta / (alpha * cos_theta) * ( exp(-alpha * (h0 + cos_theta * s)) - exp(-alpha*h0) ));

		return tauVal;
	}
	
	
	// optical thickness, absorption, attenuation, extinction
	virtual colorA_t transmittance(renderState_t &state, ray_t &ray) const {
		//return colorA_t(0.f);
		colorA_t result;

		result = skyTau(ray, b_m, alpha_m);
		result += skyTau(ray, b_r, alpha_r);
		return colorA_t(exp(-result.energy()));
	}
	
	// emission and in-scattering
	virtual colorA_t integrate(renderState_t &state, ray_t &ray) const {
		//return colorA_t(0.f);
		colorA_t I_r(0.f);
		colorA_t I_m(0.f);
		
		float s;
		if (ray.tmax < 0.f) return colorA_t(0.f);
		else s = ray.tmax * scale;
		
		colorA_t S0_r(0.f); // light scattered into the view ray over the complete hemisphere
		colorA_t S0_m(0.f); // light scattered into the view ray over the complete hemisphere
		int V = 3;
		int U = 8;
		for (int v = 0; v < V; v++) {
			float theta = (v * 0.3f + 0.2f) * 0.5f * M_PI;
			for (int u = 0; u < U; u++) {
				float phi = (u /* + (*state.prng)() */) * 2.0f * M_PI / (float)U;
				float z = cos(theta);
				float x = sin(theta) * cos(phi);
				float y = sin(theta) * sin(phi);
				vector3d_t w(x, y, z);
				ray_t bgray(point3d_t(0, 0, 0), w, 0, 1, 0);
				color_t L_s = background->eval(bgray, false);
				float b_r_angular = b_r * 3 / (2 * M_PI * 8) * (1.0f + (w * (-ray.dir)) * (w * (-ray.dir)));
				float K = 0.67f;
				float angle = fAcos(w * (ray.dir));
				float b_m_angular = b_m / (2 * K * M_PI) * mieScatter(angle);
				//std::cout << "w: " << w << " theta: " << theta << " -ray.dir: " << -ray.dir << " angle: " << angle << " mie ang " << b_m_angular << std::endl;
				S0_m = S0_m + colorA_t(L_s) * b_m_angular;
				S0_r = S0_r + colorA_t(L_s) * b_r_angular;
			}
			//std::cout << "w: " << w << " theta: " << theta << " phi: " << phi << " S0: " << S0 << std::endl;
		}

		S0_r = S0_r * (1.f / (float)(U * V)); //* 10.f; // FIXME: bad again, *10 scaling to make it look better
		S0_m = S0_m * (1.f / (float)(U * V));
		
		//std::cout << " S0_m: " << S0_m.energy() << std::endl;

		float cos_theta = ray.dir.z;

		float h0 = ray.from.z * scale;

		float step = stepSize * scale;
		
		float pos = 0.f + (*state.prng)() * step;
		
		while (pos < s) {
			ray_t stepRay(ray.from, ray.dir, 0, pos / scale, 0);

			float u_r = exp(-alpha_r * (h0 + pos * cos_theta));
			float u_m = exp(-alpha_m * (h0 + pos * cos_theta));
			
			colorA_t tau_m = skyTau(stepRay, b_m, alpha_m);
			colorA_t tau_r = skyTau(stepRay, b_r, alpha_r);

			float Tr_r = exp(-tau_r.energy());
			float Tr_m = exp(-tau_m.energy());
			
			//if (Tr < 1e-3) // || u < 1e-3)
			//	break;

			I_r += Tr_r * u_r * step;
			I_m += Tr_m * u_m * step;

			//std::cout << /* tau_r.energy() << " " << Tr_r << " " <<  */ I_m.energy() << " " << Tr_m << " " << pos << " " << cos_theta << std::endl;
			
			pos += step;
		}

		//std::cout << "result: " << S0_m * I_m << " " << I_m.energy() << " " << S0_m.energy() << std::endl;
		return S0_r * I_r + S0_m * I_m;
	}
	
	static integrator_t* factory(paraMap_t &params, renderEnvironment_t &render)
	{
		float sSize = 1.f;
		float a = .5f;
		float ss = .1f;
		float t = 3.f;
		params.getParam("stepSize", sSize);
		params.getParam("sigma_t", ss);
		params.getParam("alpha", a);
		params.getParam("turbidity", t);
		SkyIntegrator* inte = new SkyIntegrator(sSize, a, ss, t);
		return inte;
	}

};

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("SkyIntegrator", SkyIntegrator::factory);
	}

}

__END_YAFRAY
