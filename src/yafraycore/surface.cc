#include <core_api/surface.h>
#include <core_api/ray.h>
#include <utilities/interpolation.h>

__BEGIN_YAFRAY

spDifferentials_t::spDifferentials_t(const surfacePoint_t &spoint, const diffRay_t &ray): sp(spoint)
{
	if (ray.hasDifferentials)
	{
		// Estimate screen-space change in \pt and $(u,v)$
		// Compute auxiliary intersection points with plane
		PFLOAT d = -(sp.N * vector3d_t(sp.P));
		vector3d_t rxv(ray.xfrom);
		PFLOAT tx = -((sp.N * rxv) + d) / (sp.N * ray.xdir);
		point3d_t px = ray.xfrom + tx * ray.xdir;
		vector3d_t ryv(ray.yfrom);
		PFLOAT ty = -((sp.N * ryv) + d) / (sp.N * ray.ydir);
		point3d_t py = ray.yfrom + ty * ray.ydir;
		dPdx = px - sp.P;
		dPdy = py - sp.P;
	}
	else
	{
		//dudx = dvdx = 0.;
		//dudy = dvdy = 0.;
		dPdx = dPdy = vector3d_t(0,0,0);
	}
}

void spDifferentials_t::reflectedRay(const diffRay_t &in, diffRay_t &out) const
{
	if(!in.hasDifferentials)
	{
		out.hasDifferentials = false;
		return;
	}
	// Compute ray differential _rd_ for specular reflection
	out.hasDifferentials = true;
	out.xfrom = sp.P + dPdx;
	out.yfrom = sp.P + dPdy;
	// Compute differential reflected directions
//	Normal dndx = bsdf->dgShading.dndu * bsdf->dgShading.dudx +
//				  bsdf->dgShading.dndv * bsdf->dgShading.dvdx;
//	Normal dndy = bsdf->dgShading.dndu * bsdf->dgShading.dudy +
//				  bsdf->dgShading.dndv * bsdf->dgShading.dvdy;
	vector3d_t dwodx = in.dir - in.xdir, dwody = in.dir - in.ydir;
	PFLOAT dDNdx = (dwodx * sp.N); // + (out.dir * dndx);
	PFLOAT dDNdy = (dwody * sp.N); // + (out.dir * dndy);
	out.xdir = out.dir - dwodx + 2 * (/* (out.dir * sp.N) * dndx + */ dDNdx * sp.N);
	out.ydir = out.dir - dwody + 2 * (/* (out.dir * sp.N) * dndy + */ dDNdy * sp.N);
}

void spDifferentials_t::refractedRay(const diffRay_t &in, diffRay_t &out, PFLOAT IOR) const
{
	//RayDifferential rd(p, wi);
	out.hasDifferentials = true;
	out.xfrom = sp.P + dPdx;
	out.yfrom = sp.P + dPdy;
	//if (Dot(wo, n) < 0) eta = 1.f / eta;
	
	//Normal dndx = bsdf->dgShading.dndu * bsdf->dgShading.dudx + bsdf->dgShading.dndv * bsdf->dgShading.dvdx;
	//Normal dndy = bsdf->dgShading.dndu * bsdf->dgShading.dudy + bsdf->dgShading.dndv * bsdf->dgShading.dvdy;
	
	vector3d_t dwodx = in.dir - in.xdir, dwody = in.dir - in.ydir;
	PFLOAT dDNdx = (dwodx * sp.N); // + Dot(wo, dndx);
	PFLOAT dDNdy = (dwody * sp.N); // + Dot(wo, dndy);
	
//	PFLOAT mu = IOR * (in.dir * sp.N) - (out.dir * sp.N);
	PFLOAT dmudx = (IOR - (IOR*IOR*(in.dir * sp.N))/(out.dir * sp.N)) * dDNdx;
	PFLOAT dmudy = (IOR - (IOR*IOR*(in.dir * sp.N))/(out.dir * sp.N)) * dDNdy;
	
	out.xdir = out.dir + IOR * dwodx - (/* mu * dndx + */ dmudx * sp.N);
	out.ydir = out.dir + IOR * dwody - (/* mu * dndy + */ dmudy * sp.N);
}

PFLOAT spDifferentials_t::projectedPixelArea()
{
	return (dPdx ^ dPdy).length();
}

surfacePoint_t blend_surface_points(surfacePoint_t const& sp_0, surfacePoint_t const& sp_1, float const alpha)
{
    surfacePoint_t sp_result(sp_0);

    sp_result.N = lerp(sp_0.N, sp_1.N, alpha);

    sp_result.NU = lerp(sp_0.NU, sp_1.NU, alpha);
    sp_result.NV = lerp(sp_0.NV, sp_1.NV, alpha);
    sp_result.dPdU = lerp(sp_0.dPdU, sp_1.dPdU, alpha);
    sp_result.dPdV = lerp(sp_0.dPdV, sp_1.dPdV, alpha);
    sp_result.dSdU = lerp(sp_0.dSdU, sp_1.dSdU, alpha);
    sp_result.dSdV = lerp(sp_0.dSdV, sp_1.dSdV, alpha);

    return sp_result;
}

__END_YAFRAY
