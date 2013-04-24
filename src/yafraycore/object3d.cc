#include <yafraycore/meshtypes.h>
#include <cstdlib>

__BEGIN_YAFRAY

triangleObject_t::triangleObject_t(int ntris, bool hasUV, bool hasOrco):
    has_orco(hasOrco), has_uv(hasUV), is_smooth(false), normals_exported(false)
{
	triangles.reserve(ntris);
	if(hasUV)
	{
		uv_offsets.reserve(ntris);
	}

	if(hasOrco)
	{
		points.reserve(2 * 3 * ntris);
	}
	else
	{
		points.reserve(3 * ntris);
	}
}

int triangleObject_t::getPrimitives(const triangle_t **prims)
{
	for(unsigned int i=0; i < triangles.size(); ++i)
	{
		prims[i] = &(triangles[i]);
	}
	return triangles.size();
}

triangle_t* triangleObject_t::addTriangle(const triangle_t &t)
{
	triangles.push_back(t);
	triangles.back().selfIndex = triangles.size() - 1;
	return &(triangles.back());
}

void triangleObject_t::finish()
{
	for(std::vector<triangle_t>::iterator i=triangles.begin(); i!= triangles.end(); ++i)
	{
		i->recNormal();
	}
}

// triangleObjectInstance_t Methods

triangleObjectInstance_t::triangleObjectInstance_t(triangleObject_t *base, matrix4x4_t obj2World)
{
	objToWorld = obj2World;
	mBase = base;
	has_orco = mBase->has_orco;
	has_uv = mBase->has_uv;
	is_smooth = mBase->is_smooth;
	normals_exported = mBase->normals_exported;
	visible = true;
	is_base_mesh = false;

	triangles.reserve(mBase->triangles.size());

	for(size_t i = 0; i < mBase->triangles.size(); i++)
	{
		triangles.push_back(triangleInstance_t(&mBase->triangles[i], this));
	}
}

int triangleObjectInstance_t::getPrimitives(const triangle_t **prims)
{
	for(size_t i = 0; i < triangles.size(); i++)
	{
		prims[i] = &triangles[i];
	}
	return triangles.size();
}

void triangleObjectInstance_t::finish()
{
	// Empty
}

/*===================
	meshObject_t methods
=====================================*/

meshObject_t::meshObject_t(int ntris, bool hasUV, bool hasOrco):
	has_orco(hasOrco), has_uv(hasUV), has_vcol(false), is_smooth(false), light(0)
{
	//triangles.reserve(ntris);
	if(hasUV)
	{
		uv_offsets.reserve(ntris);
	}
}

int meshObject_t::getPrimitives(const primitive_t **prims) const
{
	int n=0;
	for(unsigned int i=0; i < triangles.size(); ++i, ++n)
	{
		prims[n] = &(triangles[i]);
	}
	for(unsigned int i=0; i < s_triangles.size(); ++i, ++n)
	{
		prims[n] = &(s_triangles[i]);
	}
	return n;
}

primitive_t* meshObject_t::addTriangle(const vTriangle_t &t)
{
	triangles.push_back(t);
	return &(triangles.back());
}

primitive_t* meshObject_t::addBsTriangle(const bsTriangle_t &t)
{
	s_triangles.push_back(t);
	return &(triangles.back());
}

void meshObject_t::finish()
{
	for(std::vector<vTriangle_t>::iterator i=triangles.begin(); i!= triangles.end(); ++i)
	{
		i->recNormal();
	}
}

__END_YAFRAY
