
#ifndef Y_PERSPECTIVECAMERA_H
#define Y_PERSPECTIVECAMERA_H

#include <yafray_config.h>

#include <core_api/camera.h>
#include <vector>

__BEGIN_YAFRAY

class paraMap_t;
class renderEnvironment_t;

class perspectiveCam_t: public camera_t
{
    public:
		enum bokehType {BK_DISK1, BK_DISK2, BK_TRI=3, BK_SQR, BK_PENTA, BK_HEXA, BK_RING};
        enum bkhBiasType {BB_NONE, BB_CENTER, BB_EDGE};
        perspectiveCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
                         int _resx, int _resy, PFLOAT aspect=1,
                         PFLOAT df=1, PFLOAT ap=0, PFLOAT dofd=0, bokehType bt=BK_DISK1, bkhBiasType bbt=BB_NONE, PFLOAT bro=0,
                         float const near_clip_distance = 0.0f, float const far_clip_distance = 1e6f);
        virtual ~perspectiveCam_t();
        virtual void setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz);
		virtual ray_t shootRay(PFLOAT px, PFLOAT py, float lu, float lv, PFLOAT &wt) const;
		virtual bool sampleLense() const;
		virtual point3d_t screenproject(const point3d_t &p) const;
		
		virtual bool project(const ray_t &wo, PFLOAT lu, PFLOAT lv, PFLOAT &u, PFLOAT &v, float &pdf) const;
		
		static camera_t* factory(paraMap_t &params, renderEnvironment_t &render);
	protected:
		void biasDist(PFLOAT &r) const;
		void sampleTSD(PFLOAT r1, PFLOAT r2, PFLOAT &u, PFLOAT &v) const;
		void getLensUV(PFLOAT r1, PFLOAT r2, PFLOAT &u, PFLOAT &v) const;
		
		bokehType bkhtype;
		bkhBiasType bkhbias;
		vector3d_t dof_up, dof_rt;
		PFLOAT aperture;
		PFLOAT focal_distance, dof_distance;
		PFLOAT fdist;
		PFLOAT A_pix;
		std::vector<PFLOAT> LS;
};

__END_YAFRAY

#endif // Y_PERSPECTIVECAMERA_H
