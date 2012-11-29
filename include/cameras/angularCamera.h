
#ifndef Y_ANGULARCAMERA_H
#define Y_ANGULARCAMERA_H

#include <yafray_config.h>

#include <core_api/camera.h>

__BEGIN_YAFRAY

class paraMap_t;
class renderEnvironment_t;


class angularCam_t: public camera_t
{
	public:
		angularCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
                     int _resx, int _resy, PFLOAT aspect, PFLOAT angle, bool circ,
                     float const near_clip_distance = 0.0f, float const far_clip_distance = 1e6f);
        virtual void setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz);
		virtual ray_t shootRay(PFLOAT px, PFLOAT py, float lu, float lv, PFLOAT &wt) const;
		virtual point3d_t screenproject(const point3d_t &p) const;
		
		static camera_t* factory(paraMap_t &params, renderEnvironment_t &render);
	protected:
		PFLOAT aspect,hor_phi, max_r;
		bool circular;
};


__END_YAFRAY

#endif // Y_ANGULARCAMERA_H
