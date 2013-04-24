/****************************************************************************
 * 			sunsky.cc: a light source using the background
 *      This is part of the yafray package
 *      Copyright (C) 2006  Mathias Wein
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
 */

#include<yafray_config.h>

#include <core_api/environment.h>
#include <core_api/background.h>
#include <core_api/params.h>
#include <core_api/scene.h>
#include <core_api/light.h>

__BEGIN_YAFRAY

// sunsky, from 'A Practical Analytic Model For DayLight" by Preetham, Shirley & Smits.
// http://www.cs.utah.edu/vissim/papers/sunsky/
// based on the actual code by Brian Smits
// and a thread on gamedev.net on skycolor algorithms

color_t ComputeAttenuatedSunlight(float theta, int turbidity);

class sunskyBackground_t: public background_t
{
	public:
		sunskyBackground_t(const point3d_t dir, float turb, float a_var, float b_var, float c_var, float d_var, float e_var, float pwr);
		virtual color_t operator() (const ray_t &ray, renderState_t &state, bool filtered=false) const;
		virtual color_t eval(const ray_t &ray, bool filtered=false) const;
		virtual ~sunskyBackground_t();
		static background_t *factory(paraMap_t &,renderEnvironment_t &);
	protected:
		color_t getSkyCol(const ray_t &ray) const;
		vector3d_t sunDir;
		float turbidity;
		double thetaS, phiS;	// sun coords
		double theta2, theta3, T, T2;
		double zenith_Y, zenith_x, zenith_y;
		double perez_Y[5], perez_x[5], perez_y[5];
		double AngleBetween(double thetav, double phiv) const;
		double PerezFunction(const double *lam, double theta, double gamma, double lvz) const;
		float power;
};

sunskyBackground_t::sunskyBackground_t(const point3d_t dir, float turb, float a_var, float b_var, float c_var, float d_var, float e_var, float pwr): power(pwr)
{
	sunDir.set(dir.x, dir.y, dir.z);
	sunDir.normalize();
	thetaS = fAcos(sunDir.z);
	theta2 = thetaS*thetaS;
	theta3 = theta2*thetaS;
	phiS = atan2(sunDir.y, sunDir.x);
	T = turb;
	T2 = turb*turb;
	double chi = (4.0 / 9.0 - T / 120.0) * (M_PI - 2.0 * thetaS);
	zenith_Y = (4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192;
	zenith_Y *= 1000;  // conversion from kcd/m^2 to cd/m^2

	zenith_x =	( 0.00165*theta3 - 0.00375*theta2 + 0.00209*thetaS)* T2 +
			(-0.02903*theta3 + 0.06377*theta2 - 0.03202*thetaS + 0.00394) * T +
			( 0.11693*theta3 - 0.21196*theta2 + 0.06052*thetaS + 0.25886);

	zenith_y =	( 0.00275*theta3 - 0.00610*theta2 + 0.00317*thetaS)* T2 +
			(-0.04214*theta3 + 0.08970*theta2 - 0.04153*thetaS + 0.00516) * T +
			( 0.15346*theta3 - 0.26756*theta2 + 0.06670*thetaS + 0.26688);

	perez_Y[0] = ( 0.17872 * T - 1.46303) * a_var;
	perez_Y[1] = (-0.35540 * T + 0.42749) * b_var;
	perez_Y[2] = (-0.02266 * T + 5.32505) * c_var;
	perez_Y[3] = ( 0.12064 * T - 2.57705) * d_var;
	perez_Y[4] = (-0.06696 * T + 0.37027) * e_var;

	perez_x[0] = (-0.01925 * T - 0.25922) * a_var;
	perez_x[1] = (-0.06651 * T + 0.00081) * b_var;
	perez_x[2] = (-0.00041 * T + 0.21247) * c_var;
	perez_x[3] = (-0.06409 * T - 0.89887) * d_var;
	perez_x[4] = (-0.00325 * T + 0.04517) * e_var;

	perez_y[0] = (-0.01669 * T - 0.26078) * a_var;
	perez_y[1] = (-0.09495 * T + 0.00921) * b_var;
	perez_y[2] = (-0.00792 * T + 0.21023) * c_var;
	perez_y[3] = (-0.04405 * T - 1.65369) * d_var;
	perez_y[4] = (-0.01092 * T + 0.05291) * e_var;
}

sunskyBackground_t::~sunskyBackground_t()
{
	// Empty
}

double sunskyBackground_t::PerezFunction(const double *lam, double theta, double gamma, double lvz) const
{
  double e1=0, e2=0, e3=0, e4=0;
  if (lam[1]<=230.)
    e1 = fExp(lam[1]);
  else
    e1 = 7.7220185e99;
  if ((e2 = lam[3]*thetaS)<=230.)
    e2 = fExp(e2);
  else
    e2 = 7.7220185e99;
  if ((e3 = lam[1]/cos(theta))<=230.)
    e3 = fExp(e3);
  else
    e3 = 7.7220185e99;
  if ((e4 = lam[3]*gamma)<=230.)
    e4 = fExp(e4);
  else
    e4 = 7.7220185e99;
  double den = (1 + lam[0]*e1) * (1 + lam[2]*e2 + lam[4]*fCos(thetaS)*fCos(thetaS));
  double num = (1 + lam[0]*e3) * (1 + lam[2]*e4 + lam[4]*fCos(gamma)*fCos(gamma));
  return (lvz * num / den);
}


double sunskyBackground_t::AngleBetween(double thetav, double phiv) const
{
  double cospsi = fSin(thetav) * fSin(thetaS) * fCos(phiS-phiv) + fCos(thetav) * fCos(thetaS);
  if (cospsi > 1)  return 0;
  if (cospsi < -1) return M_PI;
  return fAcos(cospsi);
}

inline color_t sunskyBackground_t::getSkyCol(const ray_t &ray) const
{
	vector3d_t Iw = ray.dir;
	Iw.normalize();

	double theta, phi, hfade=1, nfade=1;

	color_t skycolor(0.0);

	theta = fAcos(Iw.z);
	if (theta>(0.5*M_PI)) {
		// this stretches horizon color below horizon, must be possible to do something better...
		// to compensate, simple fade to black
		hfade = 1.0-(theta*M_1_PI-0.5)*2.0;
		hfade = hfade*hfade*(3.0-2.0*hfade);
		theta = 0.5*M_PI;
	}
	// compensation for nighttime exaggerated blue
	// also simple fade out towards zenith
	if (thetaS>(0.5*M_PI)) {
		if (theta<=0.5*M_PI) {
			nfade = 1.0-(0.5-theta*M_1_PI)*2.0;
			nfade *= 1.0-(thetaS*M_1_PI-0.5)*2.0;
			nfade = nfade*nfade*(3.0-2.0*nfade);
		}
	}

	if ((Iw.y==0.0) && (Iw.x==0.0))
		phi = M_PI*0.5;
	else
		phi = atan2(Iw.y, Iw.x);

	double gamma = AngleBetween(theta, phi);
	// Compute xyY values
	double x = PerezFunction(perez_x, theta, gamma, zenith_x);
	double y = PerezFunction(perez_y, theta, gamma, zenith_y);
	// Luminance scale 1.0/15000.0
	double Y = 6.666666667e-5 * nfade * hfade * PerezFunction(perez_Y, theta, gamma, zenith_Y);

	if(y == 0.f) return skycolor;

	// conversion to RGB, from gamedev.net thread on skycolor computation
	double X = (x / y) * Y;
	double Z = ((1.0 - x - y) / y) * Y;

	skycolor.set((3.240479 * X - 1.537150 * Y - 0.498535 * Z),
				 (-0.969256 * X + 1.875992 * Y + 0.041556 * Z),
				 ( 0.055648 * X - 0.204043 * Y + 1.057311 * Z));
	skycolor.clampRGB01();
	return skycolor;
}

color_t sunskyBackground_t::operator() (const ray_t &ray, renderState_t &state, bool filtered) const
{
	return power * getSkyCol(ray);
}

color_t sunskyBackground_t::eval(const ray_t &ray, bool filtered) const
{
	return power * getSkyCol(ray);
}

background_t *sunskyBackground_t::factory(paraMap_t &params,renderEnvironment_t &render)
{
	point3d_t dir(1,1,1);	// same as sunlight, position interpreted as direction
	float turb = 4.0;	// turbidity of atmosphere
	bool add_sun = false;	// automatically add real sunlight
	bool bgl = false;
	int bgl_samples = 8;
	double power = 1.0;
	float pw = 1.0;	// sunlight power
	float av, bv, cv, dv, ev;
	av = bv = cv = dv = ev = 1.0;	// color variation parameters, default is normal

	params.getParam("from", dir);
	params.getParam("turbidity", turb);
	params.getParam("power", power);

	// new color variation parameters
	params.getParam("a_var", av);
	params.getParam("b_var", bv);
	params.getParam("c_var", cv);
	params.getParam("d_var", dv);
	params.getParam("e_var", ev);

	// create sunlight with correct color and position?
	params.getParam("add_sun", add_sun);
	params.getParam("sun_power", pw);

	params.getParam("background_light", bgl);
	params.getParam("light_samples", bgl_samples);

	background_t *new_sunsky = new sunskyBackground_t(dir, turb, av, bv, cv, dv, ev, power);

	if(bgl)
	{
		paraMap_t bgp;
		bgp["type"] = std::string("bglight");
		bgp["samples"] = bgl_samples;

		light_t *bglight = render.createLight("sunsky_bgLight", bgp);

		bglight->setBackground(new_sunsky);

		if(bglight) render.getScene()->addLight(bglight);
	}

	if (add_sun)
	{
		color_t suncol = ComputeAttenuatedSunlight(fAcos(std::fabs(dir.z)), turb);//(*new_sunsky)(vector3d_t(dir.x, dir.y, dir.z));
		double angle = 0.27;
		double cosAngle = cos(degToRad(angle));
		float invpdf = (2.f * M_PI * (1.f - cosAngle));
		suncol *= invpdf * power;

		Y_INFO << "Sunsky: sun color = " << suncol << yendl;

		paraMap_t p;
		p["type"] = std::string("sunlight");
		p["direction"] = point3d_t(dir[0], dir[1], dir[2]);
		p["color"] = suncol;
		p["angle"] = parameter_t(angle);
		p["power"] = parameter_t(pw);

		light_t *light = render.createLight("sunsky_SUN", p);

		if(light) render.getScene()->addLight(light);
	}

	return new_sunsky;
}

extern "C"
{

YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
{
	render.registerFactory("sunsky",sunskyBackground_t::factory);
}

}
__END_YAFRAY
