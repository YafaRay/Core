#include <cassert>
#include <yafraycore/meshtypes.h>

__BEGIN_YAFRAY

int triBoxClip(const double b_min[3], const double b_max[3], const double triverts[3][3], bound_t &box, void* n_dat);
int triPlaneClip(double pos, int axis, bool lower, bound_t &box, void* o_dat, void* n_dat);

inline void triangle_t::getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const
{
	sp.Ng = getNormal();

	float u = data.b0, v = data.b1, w = data.b2;

	if(mesh->is_smooth)
	{
		vector3d_t va = (na >= 0) ? mesh->getVertexNormal(na) : sp.Ng;
		vector3d_t vb = (nb >= 0) ? mesh->getVertexNormal(nb) : sp.Ng;
		vector3d_t vc = (nc >= 0) ? mesh->getVertexNormal(nc) : sp.Ng;

		sp.N = u*va + v*vb + w*vc;
		sp.N.normalize();
	}
	else sp.N = sp.Ng;

	if(mesh->has_orco)
	{
		// if the object is an instance, the vertex positions are the orcos
		point3d_t const& p0 = mesh->getVertex(pa + 1);
		point3d_t const& p1 = mesh->getVertex(pb + 1);
		point3d_t const& p2 = mesh->getVertex(pc + 1);

		sp.orcoP = u * p0 + v * p1 + w * p2;

		sp.orcoNg = ((p1 - p0) ^ (p2 - p0)).normalize();
		sp.hasOrco = true;
	}
	else
	{
		sp.orcoP = hit;
		sp.hasOrco = false;
		sp.orcoNg = sp.Ng;
	}

	point3d_t const& p0 = mesh->getVertex(pa);
	point3d_t const& p1 = mesh->getVertex(pb);
	point3d_t const& p2 = mesh->getVertex(pc);

	if(mesh->has_uv)
	{
		size_t uvi = selfIndex * 3;
		const uv_t &uv1 = mesh->uv_values[mesh->uv_offsets[uvi]];
		const uv_t &uv2 = mesh->uv_values[mesh->uv_offsets[uvi + 1]];
		const uv_t &uv3 = mesh->uv_values[mesh->uv_offsets[uvi + 2]];

		//eh...u, v and w are actually the barycentric coords, not some UVs...quite annoying, i know...
		sp.U = u * uv1.u + v * uv2.u + w * uv3.u;
		sp.V = u * uv1.v + v * uv2.v + w * uv3.v;

		// calculate dPdU and dPdV
		float du1 = uv1.u - uv3.u;
		float du2 = uv2.u - uv3.u;
		float dv1 = uv1.v - uv3.v;
		float dv2 = uv2.v - uv3.v;
		float invdet, det = du1 * dv2 - dv1 * du2;

		if(std::fabs(det) > 1e-30f)
		{
			invdet = 1.f/det;
			vector3d_t dp1 = p0 - p2;
			vector3d_t dp2 = p1 - p2;
			sp.dPdU = (dv2 * dp1 - dv1 * dp2) * invdet;
			sp.dPdV = (du1 * dp2 - du2 * dp1) * invdet;
		}
		else
		{
			// implicit mapping, p1 = 0/0, p2 = 1/0, p3 = 0/1 => U = u, V = v; (arbitrary choice)
			sp.dPdU = p1 - p0;
			sp.dPdV = p2 - p1;
		}
	}
	else
	{
		// implicit mapping, p1 = 0/0, p2 = 1/0, p3 = 0/1 => U = u, V = v; (arbitrary choice)
		sp.dPdU = p1 - p0;
		sp.dPdV = p2 - p1;
	}

	sp.dPdU.normalize();
	sp.dPdV.normalize();

	sp.object = mesh;
	sp.primNum = selfIndex;
	sp.material = material;
	sp.P = hit;
	createCS(sp.N, sp.NU, sp.NV);
	// transform dPdU and dPdV in shading space
	sp.dSdU.x = sp.NU * sp.dPdU;
	sp.dSdU.y = sp.NV * sp.dPdU;
	sp.dSdU.z = sp.N * sp.dPdU;
	sp.dSdV.x = sp.NU * sp.dPdV;
	sp.dSdV.y = sp.NV * sp.dPdV;
	sp.dSdV.z = sp.N * sp.dPdV;
	sp.light = mesh->light;
}

inline bool triangle_t::clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const
{
	if(axis>=0) // re-clip
	{
		bool lower = axis & ~3;
		int _axis = axis & 3;
		double split = lower ? bound[0][_axis] : bound[1][_axis];
		int res=triPlaneClip(split, _axis, lower, clipped, d_old, d_new);
		// if an error occured due to precision limits...ugly solution i admitt
		if(res>1) goto WHOOPS;
		return (res==0) ? true : false;
	}
	else // initial clip
	{
		WHOOPS:

		double tPoints[3][3];

		point3d_t const& a = mesh->getVertex(pa);
		point3d_t const& b = mesh->getVertex(pb);
		point3d_t const& c = mesh->getVertex(pc);

		for(int i=0; i<3; ++i)
		{
			tPoints[0][i] = a[i];
			tPoints[1][i] = b[i];
			tPoints[2][i] = c[i];
		}
		int res=triBoxClip(bound[0], bound[1], tPoints, clipped, d_new);
		return (res==0) ? true : false;
	}
	return true;
}

inline float triangle_t::surfaceArea() const
{
	point3d_t const& a = mesh->getVertex(pa);
	point3d_t const& b = mesh->getVertex(pb);
	point3d_t const& c = mesh->getVertex(pc);

	vector3d_t edge1, edge2;
	edge1 = b - a;
	edge2 = c - a;
	return 0.5 * (edge1 ^ edge2).length();
}

inline void triangle_t::sample(float s1, float s2, point3d_t &p, vector3d_t &n) const
{
	point3d_t const& a = mesh->getVertex(pa);
	point3d_t const& b = mesh->getVertex(pb);
	point3d_t const& c = mesh->getVertex(pc);

	float su1 = fSqrt(s1);
	float u = 1.f - su1;
	float v = s2 * su1;
	p = u*a + v*b + (1.f-u-v)*c;
	n = getNormal();
}

// triangleInstance_t Methods

inline void triangleInstance_t::getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const
{
	sp.Ng = getNormal();
	int pa = mBase->pa;
	int pb = mBase->pb;
	int pc = mBase->pc;
	int na = mBase->na;
	int nb = mBase->nb;
	int nc = mBase->nc;

	size_t selfIndex = mBase->selfIndex;

	float u = data.b0, v = data.b1, w = data.b2;

	if(mesh->is_smooth || mesh->normals_exported)
	{
		// assume the smoothed normals exist, if the mesh is smoothed; if they don't, fix this
		// assert(na > 0 && nb > 0 && nc > 0);

		vector3d_t va = (na > 0) ? mesh->getVertexNormal(na) : sp.Ng;
		vector3d_t vb = (nb > 0) ? mesh->getVertexNormal(nb) : sp.Ng;
		vector3d_t vc = (nc > 0) ? mesh->getVertexNormal(nc) : sp.Ng;

		sp.N = u*va + v*vb + w*vc;
		sp.N.normalize();
	}
	else sp.N = sp.Ng;

	if(mesh->has_orco)
	{
		// if the object is an instance, the vertex positions are the orcos
		point3d_t const& p0 = mBase->mesh->getVertex(pa + 1);
		point3d_t const& p1 = mBase->mesh->getVertex(pb + 1);
		point3d_t const& p2 = mBase->mesh->getVertex(pc + 1);

		sp.orcoP = u * p0 + v * p1 + w * p2;

		sp.orcoNg = ((p1 - p0) ^ (p2 - p0)).normalize();
		sp.hasOrco = true;
	}
	else
	{
		sp.orcoP = hit;
		sp.hasOrco = false;
		sp.orcoNg = sp.Ng;
	}

	point3d_t const& p0 = mesh->getVertex(pa);
	point3d_t const& p1 = mesh->getVertex(pb);
	point3d_t const& p2 = mesh->getVertex(pc);

	if(mesh->has_uv)
	{
		size_t uvi = selfIndex * 3;
		const uv_t &uv1 = mesh->mBase->uv_values[mesh->mBase->uv_offsets[uvi]];
		const uv_t &uv2 = mesh->mBase->uv_values[mesh->mBase->uv_offsets[uvi + 1]];
		const uv_t &uv3 = mesh->mBase->uv_values[mesh->mBase->uv_offsets[uvi + 2]];

		//eh...u, v and w are actually the barycentric coords, not some UVs...quite annoying, i know...
		sp.U = u * uv1.u + v * uv2.u + w * uv3.u;
		sp.V = u * uv1.v + v * uv2.v + w * uv3.v;

		// calculate dPdU and dPdV
		float du1 = uv1.u - uv3.u;
		float du2 = uv2.u - uv3.u;
		float dv1 = uv1.v - uv3.v;
		float dv2 = uv2.v - uv3.v;
		float invdet, det = du1 * dv2 - dv1 * du2;

		if(std::fabs(det) > 1e-30f)
		{
			invdet = 1.f/det;
			vector3d_t dp1 = p0 - p2;
			vector3d_t dp2 = p1 - p2;
			sp.dPdU = (dv2 * dp1 - dv1 * dp2) * invdet;
			sp.dPdV = (du1 * dp2 - du2 * dp1) * invdet;
		}
		else
		{
			vector3d_t dp1 = p0 - p2;
			vector3d_t dp2 = p1 - p2;
			createCS((dp2 ^ dp1).normalize(), sp.dPdU, sp.dPdV);
		}
	}
	else
	{
		// implicit mapping, p1 = 0/0, p2 = 1/0, p3 = 0/1 => U = u, V = v; (arbitrary choice)
		vector3d_t dp1 = p0 - p2;
		vector3d_t dp2 = p1 - p2;
		sp.U = v + w;
		sp.V = w;
		sp.dPdU = dp2 - dp1;
		sp.dPdV = -dp2;
	}

	sp.dPdU.normalize();
	sp.dPdV.normalize();

	sp.object = mesh;
	sp.primNum = selfIndex;
	sp.material = mBase->material;
	sp.P = hit;
	createCS(sp.N, sp.NU, sp.NV);
	vector3d_t U, V;
	createCS(sp.Ng, U, V);
	// transform dPdU and dPdV in shading space
	sp.dSdU.x = U * sp.dPdU;
	sp.dSdU.y = V * sp.dPdU;
	sp.dSdU.z = sp.Ng * sp.dPdU;
	sp.dSdV.x = U * sp.dPdV;
	sp.dSdV.y = V * sp.dPdV;
	sp.dSdV.z = sp.Ng * sp.dPdV;
	sp.dSdU.normalize();
	sp.dSdV.normalize();
	sp.light = mesh->mBase->light;
}

inline bool triangleInstance_t::clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const
{
	if(axis>=0) // re-clip
	{
		bool lower = axis & ~3;
		int _axis = axis & 3;
		double split = lower ? bound[0][_axis] : bound[1][_axis];
		int res=triPlaneClip(split, _axis, lower, clipped, d_old, d_new);
		// if an error occured due to precision limits...ugly solution i admitt
		if(res>1)
		{
			double tPoints[3][3];

			point3d_t const& a = mesh->getVertex(mBase->pa);
			point3d_t const& b = mesh->getVertex(mBase->pb);
			point3d_t const& c = mesh->getVertex(mBase->pc);

			for(int i=0; i<3; ++i)
			{
				tPoints[0][i] = a[i];
				tPoints[1][i] = b[i];
				tPoints[2][i] = c[i];
			}
			int res=triBoxClip(bound[0], bound[1], tPoints, clipped, d_new);
			return (res==0);
		}
		return (res==0);
	}
	else // initial clip
	{
		double tPoints[3][3];

        point3d_t const& a = mesh->getVertex(mBase->pa);
        point3d_t const& b = mesh->getVertex(mBase->pb);
        point3d_t const& c = mesh->getVertex(mBase->pc);

		for(int i=0; i<3; ++i)
		{
			tPoints[0][i] = a[i];
			tPoints[1][i] = b[i];
			tPoints[2][i] = c[i];
		}
		int res=triBoxClip(bound[0], bound[1], tPoints, clipped, d_new);
		return (res==0);
	}
	return true;
}

inline float triangleInstance_t::surfaceArea() const
{
	point3d_t const& a = mesh->getVertex(mBase->pa);
	point3d_t const& b = mesh->getVertex(mBase->pb);
	point3d_t const& c = mesh->getVertex(mBase->pc);

	vector3d_t edge1, edge2;
	edge1 = b - a;
	edge2 = c - a;
	return 0.5 * (edge1 ^ edge2).length();
}

inline void triangleInstance_t::sample(float s1, float s2, point3d_t &p, vector3d_t &n) const
{
	point3d_t const& a = mesh->getVertex(mBase->pa);
	point3d_t const& b = mesh->getVertex(mBase->pb);
	point3d_t const& c = mesh->getVertex(mBase->pc);

	float su1 = fSqrt(s1);
	float u = 1.f - su1;
	float v = s2 * su1;
	p = u*a + v*b + (1.f-u-v)*c;
	n = getNormal();
}

//==========================================
// vTriangle_t methods, mosty c&p...
//==========================================

bool vTriangle_t::intersect(const ray_t &ray, float *t, intersectData_t &data) const
{
	//Tomas M??ller and Ben Trumbore ray intersection scheme
	const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
	vector3d_t edge1, edge2, tvec, pvec, qvec;
	float det, inv_det, u, v;
	edge1 = b - a;
	edge2 = c - a;
	pvec = ray.dir ^ edge2;
	det = edge1 * pvec;
	if (/*(det>-0.000001) && (det<0.000001)*/ det == 0.0) return false;
	inv_det = 1.0 / det;
	tvec = ray.from - a;
	u = (tvec*pvec) * inv_det;
	if (u < 0.0 || u > 1.0) return false;
	qvec = tvec^edge1;
	v = (ray.dir*qvec) * inv_det;
	if ((v<0.0) || ((u+v)>1.0) ) return false;
	*t = edge2 * qvec * inv_det;
	data.b1 = u;
	data.b2 = v;
	return true;
}

bound_t vTriangle_t::getBound() const
{
	const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
	point3d_t l, h;
	l.x = Y_MIN3(a.x, b.x, c.x);
	l.y = Y_MIN3(a.y, b.y, c.y);
	l.z = Y_MIN3(a.z, b.z, c.z);
	h.x = Y_MAX3(a.x, b.x, c.x);
	h.y = Y_MAX3(a.y, b.y, c.y);
	h.z = Y_MAX3(a.z, b.z, c.z);
	return bound_t(l, h);
}

void vTriangle_t::getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const
{
	sp.Ng = vector3d_t(normal);
	// gives the index in triangle array, according to my latest informations
	// it _should be_ safe to rely on array-like contiguous memory in std::vector<>!
	int tri_index= this - &(mesh->triangles.front());
	// the "u" and "v" in triangle intersection code are actually "v" and "w" when u=>p1, v=>p2, w=>p3
	float u = data.b0, v = data.b1, w = data.b2;

	if(mesh->is_smooth)
	{
		vector3d_t va(na>0? mesh->normals[na] : normal), vb(nb>0? mesh->normals[nb] : normal), vc(nc>0? mesh->normals[nc] : normal);
		sp.N = u*va + v*vb + w*vc;
		sp.N.normalize();
	}
	else sp.N = vector3d_t(normal);

	if(mesh->has_orco)
	{
		sp.orcoP = u*mesh->points[pa+1] + v*mesh->points[pb+1] + w*mesh->points[pc+1];
		sp.orcoNg = ((mesh->points[pb+1]-mesh->points[pa+1])^(mesh->points[pc+1]-mesh->points[pa+1])).normalize();
		sp.hasOrco = true;
	}
	else
	{
		sp.orcoP = hit;
		sp.orcoNg = sp.Ng;
		sp.hasOrco = false;
	}
	if(mesh->has_uv)
	{
		std::vector<int>::const_iterator uvi = mesh->uv_offsets.begin() + 3*tri_index;
		int uvi1 = *uvi, uvi2 = *(uvi+1), uvi3 = *(uvi+2);
		std::vector<uv_t>::const_iterator it = mesh->uv_values.begin();
		//eh...u, v and w are actually the barycentric coords, not some UVs...quite annoying, i know...
		sp.U = u * it[uvi1].u + v * it[uvi2].u + w * it[uvi3].u;
		sp.V = u * it[uvi1].v + v * it[uvi2].v + w * it[uvi3].v;

		// calculate dPdU and dPdV
		float du1 = it[uvi1].u - it[uvi3].u;
		float du2 = it[uvi2].u - it[uvi3].u;
		float dv1 = it[uvi1].v - it[uvi3].v;
		float dv2 = it[uvi2].v - it[uvi3].v;
		float invdet, det = du1 * dv2 - dv1 * du2;

		if(std::fabs(det) > 1e-30f)
		{
			invdet = 1.f/det;
			vector3d_t dp1 = mesh->points[pa] - mesh->points[pc];
			vector3d_t dp2 = mesh->points[pb] - mesh->points[pc];
			sp.dPdU = (dv2 * invdet) * dp1 - (dv1 * invdet) * dp2;
			sp.dPdV = (du1 * invdet) * dp2 - (du2 * invdet) * dp1;
		}
		else
		{
			sp.dPdU = vector3d_t(0.f);
			sp.dPdV = vector3d_t(0.f);
		}
	}
	else
	{
		// implicit mapping, p1 = 0/0, p2 = 1/0, p3 = 0/1 => U = u, V = v; (arbitrary choice)
		sp.U = u;
		sp.V = v;
		sp.dPdU = mesh->points[pb] - mesh->points[pa];
		sp.dPdV = mesh->points[pc] - mesh->points[pa];
	}

	sp.primNum = tri_index;
	sp.material = material;
	sp.P = hit;
	createCS(sp.N, sp.NU, sp.NV);
	// transform dPdU and dPdV in shading space
	sp.dSdU.x = sp.NU * sp.dPdU;
	sp.dSdU.y = sp.NV * sp.dPdU;
	sp.dSdU.z = sp.N  * sp.dPdU;
	sp.dSdV.x = sp.NU * sp.dPdV;
	sp.dSdV.y = sp.NV * sp.dPdV;
	sp.dSdV.z = sp.N  * sp.dPdV;
	sp.light = mesh->light;
}

bool vTriangle_t::intersectsBound(exBound_t &eb) const
{
	double tPoints[3][3];
	const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
	for(int j=0; j<3; ++j)
	{
		tPoints[0][j] = a[j];
		tPoints[1][j] = b[j];
		tPoints[2][j] = c[j];
	}
	// triBoxOverlap() is in src/yafraycore/tribox3_d.cc!
	return triBoxOverlap(eb.center, eb.halfSize, tPoints);
}

bool vTriangle_t::clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const
{
	if(axis>=0) // re-clip
	{
		bool lower = axis & ~3;
		int _axis = axis & 3;
		double split = lower ? bound[0][_axis] : bound[1][_axis];
		int res=triPlaneClip(split, _axis, lower, clipped, d_old, d_new);
		// if an error occured due to precision limits...ugly solution i admitt
		if(res>1) goto WHOOPS;
		return (res==0) ? true : false;
	}
	else // initial clip
	{
//		std::cout << "+";
		WHOOPS:
//		std::cout << "!";
		double tPoints[3][3];
		const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
		for(int i=0; i<3; ++i)
		{
			tPoints[0][i] = a[i];
			tPoints[1][i] = b[i];
			tPoints[2][i] = c[i];
		}
		int res=triBoxClip(bound[0], bound[1], tPoints, clipped, d_new);
		return (res==0) ? true : false;
	}
	return true;
}

float vTriangle_t::surfaceArea() const
{
	const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
	vector3d_t edge1, edge2;
	edge1 = b - a;
	edge2 = c - a;
	return 0.5 * (edge1 ^ edge2).length();
}

void vTriangle_t::sample(float s1, float s2, point3d_t &p, vector3d_t &n) const
{
	const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
	float su1 = fSqrt(s1);
	float u = 1.f - su1;
	float v = s2 * su1;
	p = u*a + v*b + (1.f-u-v)*c;
	n = vector3d_t(normal);
}

void vTriangle_t::recNormal()
{
	const point3d_t &a=mesh->points[pa], &b=mesh->points[pb], &c=mesh->points[pc];
	normal = ((b-a)^(c-a)).normalize();
}

//==========================================
// bsTriangle_t methods
//==========================================

bool bsTriangle_t::intersect(const ray_t &ray, float *t, intersectData_t &data) const
{
	const point3d_t *an=&mesh->points[pa], *bn=&mesh->points[pb], *cn=&mesh->points[pc];
	float tc = 1.f - ray.time;
	float b1 = tc*tc, b2 = 2.f*ray.time*tc, b3 = ray.time*ray.time;
	const point3d_t a = b1*an[0] + b2*an[1] + b3*an[2];
	const point3d_t b = b1*bn[0] + b2*bn[1] + b3*bn[2];
	const point3d_t c = b1*cn[0] + b2*cn[1] + b3*cn[2];

	vector3d_t edge1, edge2, tvec, pvec, qvec;
	float det, inv_det, u, v;
	edge1 = b - a;
	edge2 = c - a;
	pvec = ray.dir ^ edge2;
	det = edge1 * pvec;
	if (/*(det>-0.000001) && (det<0.000001)*/ det == 0.0) return false;
	inv_det = 1.0 / det;
	tvec = ray.from - a;
	u = (tvec*pvec) * inv_det;
	if (u < 0.0 || u > 1.0) return false;
	qvec = tvec^edge1;
	v = (ray.dir*qvec) * inv_det;
	if ((v<0.0) || ((u+v)>1.0) ) return false;
	*t = edge2 * qvec * inv_det;

	data.b1 = u;
	data.b1 = v;
	data.t = ray.time;
	return true;
}

bound_t bsTriangle_t::getBound() const
{
	const point3d_t *an=&mesh->points[pa], *bn=&mesh->points[pb], *cn=&mesh->points[pc];
	point3d_t amin, bmin, cmin, amax, bmax, cmax;
	amin.x = Y_MIN3(an[0].x, an[1].x, an[2].x);
	amin.y = Y_MIN3(an[0].y, an[1].y, an[2].y);
	amin.z = Y_MIN3(an[0].z, an[1].z, an[2].z);
	bmin.x = Y_MIN3(bn[0].x, bn[1].x, bn[2].x);
	bmin.y = Y_MIN3(bn[0].y, bn[1].y, bn[2].y);
	bmin.z = Y_MIN3(bn[0].z, bn[1].z, bn[2].z);
	cmin.x = Y_MIN3(cn[0].x, cn[1].x, cn[2].x);
	cmin.y = Y_MIN3(cn[0].y, cn[1].y, cn[2].y);
	cmin.z = Y_MIN3(cn[0].z, cn[1].z, cn[2].z);
	amax.x = Y_MAX3(an[0].x, an[1].x, an[2].x);
	amax.y = Y_MAX3(an[0].y, an[1].y, an[2].y);
	amax.z = Y_MAX3(an[0].z, an[1].z, an[2].z);
	bmax.x = Y_MAX3(bn[0].x, bn[1].x, bn[2].x);
	bmax.y = Y_MAX3(bn[0].y, bn[1].y, bn[2].y);
	bmax.z = Y_MAX3(bn[0].z, bn[1].z, bn[2].z);
	cmax.x = Y_MAX3(cn[0].x, cn[1].x, cn[2].x);
	cmax.y = Y_MAX3(cn[0].y, cn[1].y, cn[2].y);
	cmax.z = Y_MAX3(cn[0].z, cn[1].z, cn[2].z);
	point3d_t l, h;
	l.x = Y_MIN3(amin.x, bmin.x, cmin.x);
	l.y = Y_MIN3(amin.y, bmin.y, cmin.y);
	l.z = Y_MIN3(amin.z, bmin.z, cmin.z);
	h.x = Y_MAX3(amax.x, bmax.x, cmax.x);
	h.y = Y_MAX3(amax.y, bmax.y, cmax.y);
	h.z = Y_MAX3(amax.z, bmax.z, cmax.z);
	return bound_t(l, h);
}

void bsTriangle_t::getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const
{
	// recalculating the points is not really the nicest solution...
	const point3d_t *an=&mesh->points[pa], *bn=&mesh->points[pb], *cn=&mesh->points[pc];
	float time = data.t;
	float tc = 1.f - time;
	float b1 = tc*tc, b2 = 2.f*time*tc, b3 = time*time;
	const point3d_t a = b1*an[0] + b2*an[1] + b3*an[2];
	const point3d_t b = b1*bn[0] + b2*bn[1] + b3*bn[2];
	const point3d_t c = b1*cn[0] + b2*cn[1] + b3*cn[2];

	sp.Ng = ((b-a)^(c-a)).normalize();
	// the "u" and "v" in triangle intersection code are actually "v" and "w" when u=>p1, v=>p2, w=>p3
	float u = data.b0, v = data.b1, w = data.b2;

	//todo: calculate smoothed normal...
	/* if(mesh->is_smooth)
	{
		vector3d_t va(na>0? mesh->normals[na] : normal), vb(nb>0? mesh->normals[nb] : normal), vc(nc>0? mesh->normals[nc] : normal);
		sp.N = u*va + v*vb + w*vc;
		sp.N.normalize();
	}
	else  */sp.N = sp.Ng;

	if(mesh->has_orco)
	{
		sp.orcoP = u*mesh->points[pa+1] + v*mesh->points[pb+1] + w*mesh->points[pc+1];
		sp.orcoNg = ((mesh->points[pb+1]-mesh->points[pa+1])^(mesh->points[pc+1]-mesh->points[pa+1])).normalize();
		sp.hasOrco = true;
	}
	else
	{
		sp.orcoP = hit;
		sp.orcoNg = sp.Ng;
		sp.hasOrco = false;
	}
	if(mesh->has_uv)
	{
//		static bool test=true;

		// gives the index in triangle array, according to my latest informations
		// it _should be_ safe to rely on array-like contiguous memory in std::vector<>!
		unsigned int tri_index= this - &(mesh->s_triangles.front());
		std::vector<int>::const_iterator uvi = mesh->uv_offsets.begin() + 3*tri_index;
		int uvi1 = *uvi, uvi2 = *(uvi+1), uvi3 = *(uvi+2);
		std::vector<uv_t>::const_iterator it = mesh->uv_values.begin();
		//eh...u, v and w are actually the barycentric coords, not some UVs...quite annoying, i know...
		sp.U = u * it[uvi1].u + v * it[uvi2].u + w * it[uvi3].u;
		sp.V = u * it[uvi1].v + v * it[uvi2].v + w * it[uvi3].v;

		// calculate dPdU and dPdV
		float du1 = it[uvi1].u - it[uvi3].u;
		float du2 = it[uvi2].u - it[uvi3].u;
		float dv1 = it[uvi1].v - it[uvi3].v;
		float dv2 = it[uvi2].v - it[uvi3].v;
		float invdet, det = du1 * dv2 - dv1 * du2;

		if(std::fabs(det) > 1e-30f)
		{
			invdet = 1.f/det;
			vector3d_t dp1 = mesh->points[pa] - mesh->points[pc];
			vector3d_t dp2 = mesh->points[pb] - mesh->points[pc];
			sp.dPdU = (dv2 * invdet) * dp1 - (dv1 * invdet) * dp2;
			sp.dPdV = (du1 * invdet) * dp2 - (du2 * invdet) * dp1;
		}
		else
		{
			sp.dPdU = vector3d_t(0.f);
			sp.dPdV = vector3d_t(0.f);
		}
	}
	else
	{
		// implicit mapping, p1 = 0/0, p2 = 1/0, p3 = 0/1 => U = u, V = v; (arbitrary choice)
		sp.U = u;
		sp.V = v;
		sp.dPdU = mesh->points[pb] - mesh->points[pa];
		sp.dPdV = mesh->points[pc] - mesh->points[pa];
	}
	sp.material = material;
	sp.P = hit;
	createCS(sp.N, sp.NU, sp.NV);
	// transform dPdU and dPdV in shading space
	sp.dSdU.x = sp.NU * sp.dPdU;
	sp.dSdU.y = sp.NV * sp.dPdU;
	sp.dSdU.z = sp.N  * sp.dPdU;
	sp.dSdV.x = sp.NU * sp.dPdV;
	sp.dSdV.y = sp.NV * sp.dPdV;
	sp.dSdV.z = sp.N  * sp.dPdV;
	sp.light = mesh->light;
}


__END_YAFRAY
