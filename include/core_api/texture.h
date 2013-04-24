#ifndef Y_TEXTURE_H
#define Y_TEXTURE_H

#include <yafray_config.h>
#include "surface.h"

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT texture_t
{
	public :
		/* indicate wether the the texture is discrete (e.g. image map) or continuous */
		virtual bool discrete() const { return false; }
		/* indicate wether the the texture is 3-dimensional. If not, or p.z (and
		   z for discrete textures) are unused on getColor and getFloat calls */
		virtual bool isThreeD() const { return true; }
		virtual bool isNormalmap() const { return false; }
		virtual colorA_t getColor(const point3d_t &p) const = 0;
		virtual colorA_t getColor(int x, int y, int z) const { return colorA_t(0.f); }
		virtual colorA_t getNoGammaColor(const point3d_t &p) const { return getColor(p); }
		virtual colorA_t getNoGammaColor(int x, int y, int z) const { return getColor(x, y, z); }
		virtual CFLOAT getFloat(const point3d_t &p) const { return getNoGammaColor(p).col2bri(); }
		virtual CFLOAT getFloat(int x, int y, int z) const { return getNoGammaColor(x, y, z).col2bri(); }
		/* gives the number of values in each dimension for discrete textures */
		virtual void resolution(int &x, int &y, int &z) const { x=0, y=0, z=0; }
		virtual void getInterpolationStep(float &step) const { step = 0.f; };
		virtual ~texture_t() {}
};

inline void angmap(const point3d_t &p, PFLOAT &u, PFLOAT &v)
{
	PFLOAT r = p.x*p.x + p.z*p.z;
	u = v = 0.f;
	if (r > 0.f)
	{
		float phiRatio = M_1_PI * fAcos(p.y);//[0,1] range
		r = phiRatio / fSqrt(r);
		u = p.x * r;// costheta * r * phiRatio
		v = p.z * r;// sintheta * r * phiRatio
	}
}

// slightly modified Blender's own function,
// works better than previous function which needed extra tweaks
inline void tubemap(const point3d_t &p, PFLOAT &u, PFLOAT &v)
{
	u = 0;
	v = 1 - (p.z + 1)*0.5;
	PFLOAT d = p.x*p.x + p.y*p.y;
	if (d>0) {
		d = 1/fSqrt(d);
		u = 0.5*(1 - (atan2(p.x*d, p.y*d) *M_1_PI));
	}
}

// maps a direction to a 2d 0..1 interval
inline void spheremap(const point3d_t &p, PFLOAT &u, PFLOAT &v)
{
	float sqrtRPhi = p.x*p.x + p.y*p.y;
	float sqrtRTheta = sqrtRPhi + p.z*p.z;
	float phiRatio;

	u = 0.f;
	v = 0.f;

	if(sqrtRPhi > 0.f)
	{
		if(p.y < 0.f) phiRatio = (M_2PI - fAcos(p.x / fSqrt(sqrtRPhi))) * M_1_2PI;
		else		  phiRatio = fAcos(p.x / fSqrt(sqrtRPhi)) * M_1_2PI;
		u = 1.f - phiRatio;
	}

	v = 1.f - (fAcos(p.z / fSqrt(sqrtRTheta)) * M_1_PI);
}

// maps u,v coords in the 0..1 interval to a direction
inline void invSpheremap(float u, float v, vector3d_t &p)
{
	float theta = v * M_PI;
	float phi = -(u * M_2PI);
	float costheta = fCos(theta), sintheta = fSin(theta);
	float cosphi = fCos(phi), sinphi = fSin(phi);
	p.x = sintheta * cosphi;
	p.y = sintheta * sinphi;
	p.z = -costheta;
}

__END_YAFRAY

#endif // Y_TEXTURE_H
