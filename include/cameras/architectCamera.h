
#ifndef Y_ARCHITECTCAMERA_H
#define Y_ARCHITECTCAMERA_H

#include <cameras/perspectiveCamera.h>

__BEGIN_YAFRAY

class paraMap_t;
class renderEnvironment_t;

class architectCam_t: public perspectiveCam_t
{
	public:
        architectCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
                       int _resx, int _resy, PFLOAT aspect=1,
                       PFLOAT df=1, PFLOAT ap=0, PFLOAT dofd=0, bokehType bt=BK_DISK1, bkhBiasType bbt=BB_NONE, PFLOAT bro=0,
                       float const near_clip_distance = 0.0f, float const far_clip_distance = 1e6f);
		virtual ~architectCam_t();
		virtual void setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz);
		virtual point3d_t screenproject(const point3d_t &p) const;

		static camera_t* factory(paraMap_t &params, renderEnvironment_t &render);
};

__END_YAFRAY

#endif // Y_ARCHITECTCAMERA_H
