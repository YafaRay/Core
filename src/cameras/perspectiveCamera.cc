/****************************************************************************
 *
 * 			camera.cc: Camera implementation
 *      This is part of the yafray package
 *      Copyright (C) 2002  Alejandro Conty Est??vez
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

#include <cameras/perspectiveCamera.h>
#include <core_api/environment.h>

__BEGIN_YAFRAY

perspectiveCam_t::perspectiveCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
                                   int _resx, int _resy, PFLOAT aspect,
                                   PFLOAT df, PFLOAT ap, PFLOAT dofd, bokehType bt, bkhBiasType bbt, PFLOAT bro,
                                   float const near_clip_distance, float const far_clip_distance) :
    camera_t(pos, look, up,  _resx, _resy, aspect, near_clip_distance, far_clip_distance), bkhtype(bt), bkhbias(bbt), aperture(ap) , focal_distance(df), dof_distance(dofd)
{
	// Initialize camera specific plane coordinates
	setAxis(camX,camY,camZ);

	fdist = (look - pos).length();
	A_pix = aspect_ratio / ( df * df );

	int ns = (int)bkhtype;
	if ((ns>=3) && (ns<=6)) {
		PFLOAT w=degToRad(bro), wi=(M_2PI)/(PFLOAT)ns;
		ns = (ns+2)*2;
		LS.resize(ns);
		for (int i=0;i<ns;i+=2) {
			LS[i] = fCos(w);
			LS[i+1] = fSin(w);
			w += wi;
		}
	}
}

perspectiveCam_t::~perspectiveCam_t() 
{
}

void perspectiveCam_t::setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz)
{
	camX = vx;
	camY = vy;
	camZ = vz;

	dof_rt = aperture * camX; // for dof, premul with aperture
	dof_up = aperture * camY;

	vright = camX;
	vup = aspect_ratio * camY;
	vto = (camZ * focal_distance) - 0.5 * (vup + vright);
	vup /= (PFLOAT)resy;
	vright /= (PFLOAT)resx;
}

void perspectiveCam_t::biasDist(PFLOAT &r) const
{
	switch (bkhbias) {
		case BB_CENTER:
			r = fSqrt(fSqrt(r)*r);
			break;
		case BB_EDGE:
			r = fSqrt((PFLOAT)1.0-r*r);
			break;
		default:
		case BB_NONE:
			r = fSqrt(r);
	}
}

void perspectiveCam_t::sampleTSD(PFLOAT r1, PFLOAT r2, PFLOAT &u, PFLOAT &v) const
{
	PFLOAT fn = (PFLOAT)bkhtype;
	int idx = int(r1*fn);
	r1 = (r1-((PFLOAT)idx)/fn)*fn;
	biasDist(r1);
	PFLOAT b1 = r1 * r2;
	PFLOAT b0 = r1 - b1;
	idx <<= 1;
	u = LS[idx]*b0 + LS[idx+2]*b1;
	v = LS[idx+1]*b0 + LS[idx+3]*b1;
}

void perspectiveCam_t::getLensUV(PFLOAT r1, PFLOAT r2, PFLOAT &u, PFLOAT &v) const
{
	switch (bkhtype) {
		case BK_TRI:
		case BK_SQR:
		case BK_PENTA:
		case BK_HEXA:
			sampleTSD(r1, r2, u, v);
			break;
		case BK_DISK2:
		case BK_RING: {
			PFLOAT w = (PFLOAT)M_2PI*r2;
			if (bkhtype==BK_RING) r1 = fSqrt((PFLOAT)0.707106781 + (PFLOAT)0.292893218);
			else biasDist(r1);
			u = r1*fCos(w);
			v = r1*fSin(w);
			break;
		}
		default:
		case BK_DISK1:
			ShirleyDisk(r1, r2, u, v);
	}
}



ray_t perspectiveCam_t::shootRay(PFLOAT px, PFLOAT py, float lu, float lv, PFLOAT &wt) const
{
	ray_t ray;
	wt = 1;	// for now always 1, except 0 for probe when outside sphere
	
	ray.from = position;
	ray.dir = vright*px + vup*py + vto;
	ray.dir.normalize();

    ray.tmin = ray_plane_intersection(ray, near_plane);
    ray.tmax = ray_plane_intersection(ray, far_plane);

	if (aperture!=0) {
		PFLOAT u, v;
		
		getLensUV(lu, lv, u, v);
		vector3d_t LI = dof_rt * u + dof_up * v;
		ray.from += point3d_t(LI);
		ray.dir = (ray.dir * dof_distance) - LI;
		ray.dir.normalize();
	}
	return ray;
}

point3d_t perspectiveCam_t::screenproject(const point3d_t &p) const
{
	point3d_t s;
	vector3d_t dir = p - position;

	// project p to pixel plane:
	PFLOAT dx = dir * camX;
	PFLOAT dy = dir * camY;
	PFLOAT dz = dir * camZ;
	
	s.x = 2.0f * dx * focal_distance / dz;
	s.y = -2.0f * dy * focal_distance / (dz * aspect_ratio);
	s.z = 0;

	return s;
}

bool perspectiveCam_t::project(const ray_t &wo, PFLOAT lu, PFLOAT lv, PFLOAT &u, PFLOAT &v, float &pdf) const
{
	// project wo to pixel plane:
	PFLOAT dx = camX * wo.dir;
	PFLOAT dy = camY * wo.dir;
	PFLOAT dz = camZ * wo.dir;
	if(dz <= 0) return false;
	
	u = dx * focal_distance / dz;
	if(u < -0.5 || u > 0.5) return false;
	u = (u + 0.5) * (PFLOAT) resx;
	
	v = dy * focal_distance / (dz * aspect_ratio);
	if(v < -0.5 || v > 0.5) return false;
	v = (v + 0.5) * (PFLOAT) resy;
	
	// pdf = 1/A_pix * r^2 / cos(forward, dir), where r^2 is also 1/cos(vto, dir)^2
	PFLOAT cos_wo = dz; //camZ * wo.dir;
	pdf = 8.f * M_PI / (A_pix *  cos_wo * cos_wo * cos_wo );
	return true;
}

bool perspectiveCam_t::sampleLense() const { return aperture!=0; }

camera_t* perspectiveCam_t::factory(paraMap_t &params, renderEnvironment_t &render)
{
	std::string _bkhtype="disk1", _bkhbias="uniform";
	const std::string *bkhtype=&_bkhtype, *bkhbias=&_bkhbias;
	point3d_t from(0,1,0), to(0,0,0), up(0,1,1);
	int resx=320, resy=200;
	float aspect=1, dfocal=1, apt=0, dofd=0, bkhrot=0;
    float nearClip = 0.0f, farClip = -1.0f;
	params.getParam("from", from);
	params.getParam("to", to);
	params.getParam("up", up);
	params.getParam("resx", resx);
	params.getParam("resy", resy);
	params.getParam("focal", dfocal);
	params.getParam("aperture", apt);
	params.getParam("dof_distance", dofd);
	params.getParam("bokeh_type", bkhtype);
	params.getParam("bokeh_bias", bkhbias);
	params.getParam("bokeh_rotation", bkhrot);
	params.getParam("aspect_ratio", aspect);
    params.getParam("nearClip", nearClip);
    params.getParam("farClip", farClip);

	bokehType bt = BK_DISK1;
	if (*bkhtype=="disk2")			bt = BK_DISK2;
	else if (*bkhtype=="triangle")	bt = BK_TRI;
	else if (*bkhtype=="square")	bt = BK_SQR;
	else if (*bkhtype=="pentagon")	bt = BK_PENTA;
	else if (*bkhtype=="hexagon")	bt = BK_HEXA;
	else if (*bkhtype=="ring")		bt = BK_RING;
	// bokeh bias
	bkhBiasType bbt = BB_NONE;
	if (*bkhbias=="center") 		bbt = BB_CENTER;
	else if (*bkhbias=="edge") 		bbt = BB_EDGE;

    perspectiveCam_t* cam = new perspectiveCam_t(from, to, up, resx, resy, aspect, dfocal, apt, dofd, bt, bbt, bkhrot, nearClip, farClip);

    return cam;
}

__END_YAFRAY
