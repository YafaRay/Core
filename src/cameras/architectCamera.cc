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

#include <cameras/architectCamera.h>
#include <core_api/environment.h>

__BEGIN_YAFRAY

architectCam_t::architectCam_t(const point3d_t &pos, const point3d_t &look, const point3d_t &up,
		int _resx, int _resy, PFLOAT aspect,
        PFLOAT df, PFLOAT ap, PFLOAT dofd, bokehType bt, bkhBiasType bbt, PFLOAT bro, float const near_clip_distance, float const far_clip_distance)
        :perspectiveCam_t(pos, look, up, _resx, _resy, aspect, df, ap, dofd, bt, bbt, bro, near_clip_distance, far_clip_distance)
{
	// Initialize camera specific plane coordinates
	setAxis(camX,camY,camZ);

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

architectCam_t::~architectCam_t() 
{
}

void architectCam_t::setAxis(const vector3d_t &vx, const vector3d_t &vy, const vector3d_t &vz)
{
	camX = vx;
	camY = vy;
	camZ = vz;
	
	dof_rt = camX * aperture; // for dof, premul with aperture
	dof_up = camY * aperture;

	vright = camX;
	vup = aspect_ratio * vector3d_t(0, 0, -1);
	vto = (camZ * focal_distance) - 0.5 * (vup + vright);
	vup /= (PFLOAT)resy;
	vright /= (PFLOAT)resx;
}

point3d_t architectCam_t::screenproject(const point3d_t &p) const
{
	// FIXME
	point3d_t s;
	vector3d_t dir = p - position;
	
	// project p to pixel plane:
	vector3d_t camy = vector3d_t(0,0,1);
	vector3d_t camz = camy ^ camX;
	vector3d_t camx = camz ^ camy;
	
	PFLOAT dx = dir * camx;
	PFLOAT dy = dir * camY;
	PFLOAT dz = dir * camz;
	
	s.y = 2 * dy * focal_distance / (dz * aspect_ratio);
	// Needs focal_distance correction
	PFLOAT fod = (focal_distance) * camy * camY / (camx * camX);
	s.x = 2 * dx * fod / dz;
	s.z = 0;
	
	return s;
}

camera_t* architectCam_t::factory(paraMap_t &params, renderEnvironment_t &render)
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
    architectCam_t* cam = new architectCam_t(from, to, up, resx, resy, aspect, dfocal, apt, dofd, bt, bbt, bkhrot, nearClip, farClip);

    return cam;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("perspective",   perspectiveCam_t::factory);
		render.registerFactory("architect",	architectCam_t::factory);
	}

}

__END_YAFRAY
