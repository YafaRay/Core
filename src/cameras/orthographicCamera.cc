/****************************************************************************
 *
 * 			camera.cc: Camera implementation
 *      This is part of the yafray package
 *      Copyright (C) 2002  Alejandro Conty Estévez
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
 *
 */

#include <cameras/orthographicCamera.h>
#include <core_api/environment.h>

__BEGIN_YAFRAY

orthoCam_t::orthoCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
        int _resx, int _resy, PFLOAT aspect, PFLOAT _scale, float const near_clip_distance, float const far_clip_distance)
        :camera_t(pos, look, up, _resx, _resy, aspect, near_clip_distance, far_clip_distance), scale(_scale)
{
	// Initialize camera specific plane coordinates
	setAxis(camX,camY,camZ);
}

void orthoCam_t::setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz)
{
	camX = vx;
	camY = vy;
	camZ = vz;

	vright = camX;
	vup = aspect_ratio * camY;
	vto = camZ;
	pos = position - 0.5 * scale* (vup + vright);
	vup     *= scale/(PFLOAT)resy;
	vright  *= scale/(PFLOAT)resx;
}


ray_t orthoCam_t::shootRay(PFLOAT px, PFLOAT py, float lu, float lv, PFLOAT &wt) const
{
	ray_t ray;
	wt = 1;	// for now always 1, except 0 for probe when outside sphere
	ray.from = pos + vright*px + vup*py;
	ray.dir = vto;

    ray.tmin = ray_plane_intersection(ray, near_plane);
    ray.tmax = ray_plane_intersection(ray, far_plane);

	return ray;
}

point3d_t orthoCam_t::screenproject(const point3d_t &p) const
{
	point3d_t s;
	vector3d_t dir = p - pos;	
	// Project p to pixel plane

	PFLOAT dz = camZ * dir;
	
	vector3d_t proj = dir - dz * camZ;
	
	s.x = 2 * (proj * camX / scale) - 1.0f;
	s.y = - 2 * proj * camY / (aspect_ratio * scale) + 1.0f;
	s.z = 0;

	return s;
}

camera_t* orthoCam_t::factory(paraMap_t &params, renderEnvironment_t &render)
{
	point3d_t from(0,1,0), to(0,0,0), up(0,1,1);
	int resx=320, resy=200;
	double aspect=1.0, scale=1.0;
    float nearClip = 0.0f, farClip = -1.0f;

	params.getParam("from", from);
	params.getParam("to", to);
	params.getParam("up", up);
	params.getParam("resx", resx);
	params.getParam("resy", resy);
	params.getParam("scale", scale);
	params.getParam("aspect_ratio", aspect);
    params.getParam("nearClip", nearClip);
    params.getParam("farClip", farClip);

    orthoCam_t* cam = new orthoCam_t(from, to, up, resx, resy, aspect, scale, nearClip, farClip);

    return cam;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("orthographic",	orthoCam_t::factory);
	}

}

__END_YAFRAY
