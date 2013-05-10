/****************************************************************************
 *
 * 			camera.h: Camera implementation api
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

#ifndef Y_CAMERA_H
#define Y_CAMERA_H

#include <yafray_config.h>

#include <core_api/ray.h>
#include <utilities/geometry.h>

__BEGIN_YAFRAY

//! Camera base class.
/*!
 Camera base class used by all camera types.
*/
class YAFRAYCORE_EXPORT camera_t
{
	public:
		camera_t() { }
        camera_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up, int _resx, int _resy, float aspect, float const near_clip_distance, float const far_clip_distance) :
            position(pos), resx(_resx), resy(_resy), aspect_ratio(aspect * (PFLOAT)resy / (PFLOAT)resx)
		{
			// Calculate and store camera axis
			camY = up - position;
			camZ = look - position;
			camX = camZ ^ camY;
			camY = camZ ^ camX;
			camX.normalize();
			camY.normalize();
			camZ.normalize();

            near_plane.n = camZ;
            near_plane.p = vector3d_t(position) + camZ * near_clip_distance;

            far_plane.n = camZ;
            far_plane.p = vector3d_t(position) + camZ * far_clip_distance;

            nearClip = near_clip_distance;
            farClip = far_clip_distance;
		}
        virtual ~camera_t() {}
		virtual void setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz) = 0; //!< Set camera axis
		/*! Shoot a new ray from the camera gived image pixel coordinates px,py and lense dof effect */
		virtual ray_t shootRay(PFLOAT px, PFLOAT py, float u, float v, PFLOAT &wt) const = 0; //!< Shoot a new ray from the camera.
		virtual point3d_t screenproject(const point3d_t &p) const = 0; //!< Get projection of point p into camera plane

		virtual int resX() const { return resx; } //!< Get camera X resolution
		virtual int resY() const { return resy; } //!< Get camera Y resolution
		virtual point3d_t getPosition() const { return position; } //!< Get camera position
		virtual void setPosition(const point3d_t &pos) { position = pos; } //!< Set camera position
		virtual void getAxis(vector3d_t &vx, vector3d_t &vy, vector3d_t &vz) const { vx = camX; vy = camY; vz = camZ; } //!< Get camera axis
		/*! Indicate whether the lense need to be sampled (u, v parameters of shootRay), i.e.
			DOF-like effects. When false, no lense samples need to be computed */
		virtual bool sampleLense() const { return false; } //!< Indicate whether the lense need to be sampled
		virtual bool project(const ray_t &wo, PFLOAT lu, PFLOAT lv, PFLOAT &u, PFLOAT &v, float &pdf) const { return false; }
		virtual float getNearClip() const { return nearClip; }
		virtual float getFarClip() const { return farClip; }
	protected:
		point3d_t position;	//!< Camera position

		int resx;		//!< Camera X resolution
		int resy;		//!< Camera Y resolution

		vector3d_t camX;	//!< Camera X axis
		vector3d_t camY;	//!< Camera Y axis
		vector3d_t camZ;	//!< Camera Z axis

		vector3d_t vto;
		vector3d_t vup;
		vector3d_t vright;

		float aspect_ratio;	//<! Aspect ratio of camera (not image in pixel units!)

        Plane near_plane, far_plane;
        float nearClip, farClip;
};

__END_YAFRAY

#endif // Y_CAMERA_H
