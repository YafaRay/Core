#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <yafray_constants.h>

#include <core_api/vector3d.h>
#include <core_api/ray.h>

__BEGIN_YAFRAY

struct Plane
{
    vector3d_t p;
    vector3d_t n;
};

inline float ray_plane_intersection(ray_t const& ray, Plane const& plane)
{
    return plane.n * (plane.p - vector3d_t(ray.from)) / (ray.dir * plane.n);
}

__END_YAFRAY

#endif // GEOMETRY_H
