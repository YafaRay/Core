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

#include <cameras/angularCamera.h>
#include <core_api/environment.h>

__BEGIN_YAFRAY

angularCam_t::angularCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
                           int _resx, int _resy, PFLOAT asp, PFLOAT angle, bool circ,
                           float const near_clip_distance, float const far_clip_distance) :
    camera_t(pos, look, up, _resx, _resy, asp, near_clip_distance, far_clip_distance), hor_phi(angle*M_PI/180.f), circular(circ)
{
	// Initialize camera specific plane coordinates
	setAxis(camX,camY,camZ);

	max_r = 1;
}

void angularCam_t::setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz)
{
	camX = vx;
	camY = vy;
	camZ = vz;

	vright = camX;
	vup = camY;
	vto = camZ;
}

ray_t angularCam_t::shootRay(PFLOAT px, PFLOAT py, float lu, float lv, PFLOAT &wt) const
{
	ray_t ray;
	wt = 1;	// for now always 1, or 0 when circular and outside angle
	ray.from = position;
	PFLOAT u = 1.f - 2.f * (px/(PFLOAT)resx);
	PFLOAT v = 2.f * (py/(PFLOAT)resy) - 1.f;
	v *= aspect_ratio;
	PFLOAT radius = fSqrt(u*u + v*v);
	if (circular && radius>max_r) { wt=0; return ray; }
	PFLOAT theta=0;
	if (!((u==0) && (v==0))) theta = atan2(v,u);
	PFLOAT phi = radius * hor_phi;
	//PFLOAT sp = sin(phi);
	ray.dir = fSin(phi)*(fCos(theta)*vright + fSin(theta)*vup ) + fCos(phi)*vto;

    ray.tmin = ray_plane_intersection(ray, near_plane);
    ray.tmax = ray_plane_intersection(ray, far_plane);

	return ray;
}

camera_t* angularCam_t::factory(paraMap_t &params, renderEnvironment_t &render)
{
	point3d_t from(0,1,0), to(0,0,0), up(0,1,1);
	int resx=320, resy=200;
	double aspect=1.0, angle=90, max_angle=90;
	bool circular = true, mirrored = false;
    float nearClip = 0.0f, farClip = -1.0f;

	params.getParam("from", from);
	params.getParam("to", to);
	params.getParam("up", up);
	params.getParam("resx", resx);
	params.getParam("resy", resy);
	params.getParam("aspect_ratio", aspect);
	params.getParam("angle", angle);
	max_angle = angle;
	params.getParam("max_angle", max_angle);
	params.getParam("circular", circular);
	params.getParam("mirrored", mirrored);
    params.getParam("nearClip", nearClip);
    params.getParam("farClip", farClip);
	
    angularCam_t *cam = new angularCam_t(from, to, up, resx, resy, aspect, angle, circular, nearClip, farClip);
	if(mirrored) cam->vright *= -1.0;
	cam->max_r = max_angle/angle;
	
	return cam;
}

point3d_t angularCam_t::screenproject(const point3d_t &p) const
{
	//FIXME
	point3d_t s;
	vector3d_t dir = vector3d_t(p) - vector3d_t(position);
	dir.normalize();
	
	// project p to pixel plane:	
	PFLOAT dx = camX * dir;
	PFLOAT dy = camY * dir;
	PFLOAT dz = camZ * dir;

	s.x = -dx / (4.0 * M_PI * dz);
	s.y = -dy / (4.0 * M_PI * dz);
	s.z = 0;
	
	return s;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("angular",	angularCam_t::factory);
	}

}

__END_YAFRAY
