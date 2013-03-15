#ifndef Y_VOLUMETRIC_H
#define Y_VOLUMETRIC_H

#include <map>

#include "ray.h"
#include "color.h"
#include <core_api/bound.h>

__BEGIN_YAFRAY

class renderState_t;
class pSample_t;
class light_t;


class volumeHandler_t
{
	public:
		virtual bool transmittance(const renderState_t &state, const ray_t &ray, color_t &col) const=0;
		virtual bool scatter(const renderState_t &state, const ray_t &ray, ray_t &sRay, pSample_t &s) const=0;
		virtual ~volumeHandler_t(){}
};

class YAFRAYCORE_EXPORT VolumeRegion {
	public:
	VolumeRegion() {}
	VolumeRegion(color_t sa, color_t ss, color_t le, float gg, point3d_t pmin, point3d_t pmax, int attgridScale) {
		bBox = bound_t(pmin, pmax);
		s_a = sa;
		s_s = ss;
		l_e = le;
		g = gg;
		haveS_a = (s_a.energy() > 1e-4f);
		haveS_s = (s_s.energy() > 1e-4f);
		haveL_e = (l_e.energy() > 1e-4f);
		attGridX = 8 * attgridScale;
		attGridY = 8 * attgridScale;
		attGridZ = 8 * attgridScale;
	}

	virtual ~VolumeRegion(){}

	virtual color_t sigma_a(const point3d_t &p, const vector3d_t &v) = 0;
	virtual color_t sigma_s(const point3d_t &p, const vector3d_t &v) = 0;
	virtual color_t emission(const point3d_t &p, const vector3d_t &v) = 0;
	virtual color_t sigma_t(const point3d_t &p, const vector3d_t &v) {
		return sigma_a(p, v) + sigma_s(p, v);
	}

	float attenuation(const point3d_t p, light_t *l);

	// w_l: dir *from* the light, w_s: direction, into which should be scattered
	virtual float p(const vector3d_t &w_l, const vector3d_t &w_s) {
		float k = 1.55f * g - .55f * g * g * g;
		float kcostheta = k * (w_l * w_s);
		return 1.f / (4.f * M_PI) * (1.f - k * k) / ((1.f - kcostheta) * (1.f - kcostheta));
	}

	virtual color_t tau(const ray_t &ray, float step, float offset) = 0;

	bool intersect(const ray_t &ray, float& t0, float& t1) {
		return bBox.cross(ray, t0, t1, 10000.f);
	}

	bound_t getBB() { return bBox; }

	std::map<light_t *, float*> attenuationGridMap;
	int attGridX, attGridY, attGridZ; // FIXME: un-hardcode

	protected:
	bound_t bBox;
	color_t s_a, s_s, l_e;
	bool haveS_a, haveS_s, haveL_e;
	float g;
	// TODO: add transform for BB
};

class YAFRAYCORE_EXPORT DensityVolume : public VolumeRegion {
	public:
	DensityVolume() {}
	DensityVolume(color_t sa, color_t ss, color_t le, float gg, point3d_t pmin, point3d_t pmax, int attgridScale) :
		VolumeRegion(sa, ss, le, gg, pmin, pmax, attgridScale) {}

	virtual ~DensityVolume(){}

	virtual float Density(point3d_t p) = 0;

	virtual color_t tau(const ray_t &ray, float stepSize, float offset);

	color_t sigma_a(const point3d_t &p, const vector3d_t &v) {
		if (!haveS_a) return color_t(0.f);
		if (bBox.includes(p)) {
			return s_a * Density(p);
		}
		else
			return color_t(0.f);

	}

	color_t sigma_s(const point3d_t &p, const vector3d_t &v) {
		if (!haveS_s) return color_t(0.f);
		if (bBox.includes(p)) {
			return s_s * Density(p);
		}
		else
			return color_t(0.f);
	}

	color_t emission(const point3d_t &p, const vector3d_t &v) {
		if (!haveL_e) return color_t(0.f);
		if (bBox.includes(p)) {
			return l_e * Density(p);
		}
		else
			return color_t(0.f);
	}

};



__END_YAFRAY

#endif // Y_VOLUMETRIC_H
