/****************************************************************************
 *
 * 			vector3d.h: Vector 3d and point representation and manipulation api 
 *      This is part of the yafray package
 *      Copyright (C) 2002 Alejandro Conty Estevez
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
#ifndef Y_VECTOR3D_H
#define Y_VECTOR3D_H

#include <yafray_config.h>

#include <utilities/mathOptimizations.h>
#include <iostream>

// ensure isnan is available. I *hope* it works with OSX w. gcc 4.x too
#ifdef _MSC_VER
#include <float.h>
#define isnan _isnan
#else
using std::isnan; // from cmath
#endif

__BEGIN_YAFRAY

// useful trick found in trimesh2 lib by Szymon Rusinkiewicz
// makes code for vector dot- cross-products much more readable
#define VDOT *
#define VCROSS ^

#ifndef M_PI    //in most cases pi is defined as M_PI in cmath ohterwise we define it
#define M_PI 3.1415926535897932384626433832795
#endif

class normal_t;
class point3d_t;

class YAFRAYCORE_EXPORT vector3d_t
{
	public:
		vector3d_t() { }
		vector3d_t(float v): x(v), y(v), z(v) {  }
		vector3d_t(float ix, float iy, float iz=0): x(ix), y(iy), z(iz) { }
		vector3d_t(const vector3d_t &s): x(s.x), y(s.y), z(s.z) { }
		explicit vector3d_t(const normal_t &n);
		explicit vector3d_t(const point3d_t &p);
		
		void set(float ix, float iy, float iz=0) { x=ix;  y=iy;  z=iz; }
		vector3d_t& normalize();
		// normalizes and returns length
		float normLen()
		{
			float vl = x*x + y*y + z*z;
			if (vl!=0.0) {
				vl = fSqrt(vl);
				const float d = 1.0/vl;
				x*=d; y*=d; z*=d;
			}
			return vl;
		}
		// normalizes and returns length squared
		float normLenSqr()
		{
			float vl = x*x + y*y + z*z;
			if (vl!=0.0) {
				const float d = 1.0/fSqrt(vl);
				x*=d; y*=d; z*=d;
			}
			return vl;
		}
		float length() const;
		float lengthSqr() const{ return x*x+y*y+z*z; }
		bool null()const { return ((x==0) && (y==0) && (z==0)); }
		vector3d_t& operator = (const vector3d_t &s) { x=s.x;  y=s.y;  z=s.z;  return *this;}
		vector3d_t& operator +=(const vector3d_t &s) { x+=s.x;  y+=s.y;  z+=s.z;  return *this;}
		vector3d_t& operator -=(const vector3d_t &s) { x-=s.x;  y-=s.y;  z-=s.z;  return *this;}
		vector3d_t& operator /=(float s) { x/=s;  y/=s;  z/=s;  return *this;}
		vector3d_t& operator *=(float s) { x*=s;  y*=s;  z*=s;  return *this;}
		float operator[] (int i) const{ return (&x)[i]; } //Lynx
		void abs() { x=std::fabs(x);  y=std::fabs(y);  z=std::fabs(z); }
		~vector3d_t() {};
		float x,y,z;
};

class YAFRAYCORE_EXPORT normal_t
{
	public:
		normal_t(){};
		normal_t(GFLOAT nx, GFLOAT ny, GFLOAT nz): x(nx), y(ny), z(nz){}
		explicit normal_t(const vector3d_t &v): x(v.x), y(v.y), z(v.z) { }
		normal_t& operator = (const vector3d_t &v){ x=v.x, y=v.y, z=v.z; return *this; }
		GFLOAT x, y, z;
};

class YAFRAYCORE_EXPORT point3d_t
{
	public:
		point3d_t() {}
		point3d_t(float ix, float iy, float iz=0): x(ix),  y(iy),  z(iz) { }
		point3d_t(const point3d_t &s): x(s.x),  y(s.y),  z(s.z) { }
		point3d_t(const vector3d_t &v): x(v.x),  y(v.y),  z(v.z) { }
		void set(float ix, float iy, float iz=0) { x=ix;  y=iy;  z=iz; }
		float length() const;
		point3d_t& operator= (const point3d_t &s) { x=s.x;  y=s.y;  z=s.z;  return *this; }
		point3d_t& operator *=(float s) { x*=s;  y*=s;  z*=s;  return *this;}
		point3d_t& operator +=(float s) { x+=s;  y+=s;  z+=s;  return *this;}
		point3d_t& operator +=(const point3d_t &s) { x+=s.x;  y+=s.y;  z+=s.z;  return *this;}
		point3d_t& operator -=(const point3d_t &s) { x-=s.x;  y-=s.y;  z-=s.z;  return *this;}
		float operator[] (int i) const{ return (&x)[i]; } //Lynx
		float &operator[](int i) { return (&x)[i]; } //Lynx
		~point3d_t() {};
		float x,y,z;
};


inline vector3d_t::vector3d_t(const normal_t &n): x(n.x), y(n.y), z(n.z) { }
inline vector3d_t::vector3d_t(const point3d_t &p): x(p.x), y(p.y), z(p.z) { }

#define FAST_ANGLE(a,b)  ( (a).x*(b).y - (a).y*(b).x )
#define FAST_SANGLE(a,b) ( (((a).x*(b).y - (a).y*(b).x) >= 0) ? 0 : 1 )
#define SIN(a,b) fSqrt(1-((a)*(b))*((a)*(b)))

YAFRAYCORE_EXPORT std::ostream & operator << (std::ostream &out,const vector3d_t &v);
YAFRAYCORE_EXPORT std::ostream & operator << (std::ostream &out,const point3d_t &p);

inline float operator * ( const vector3d_t &a,const vector3d_t &b)
{
	return (a.x*b.x+a.y*b.y+a.z*b.z);
}

inline vector3d_t operator * ( float f,const vector3d_t &b)
{
	return vector3d_t(f*b.x,f*b.y,f*b.z);
}

inline vector3d_t operator * (const vector3d_t &b,float f)
{
	return vector3d_t(f*b.x,f*b.y,f*b.z);
}

inline point3d_t operator * (float f,const point3d_t &b)
{
	return point3d_t(f*b.x,f*b.y,f*b.z);
}

inline vector3d_t operator / (const vector3d_t &b,float f)
{
	return vector3d_t(b.x/f,b.y/f,b.z/f);
}

inline point3d_t operator / (const point3d_t &b,float f)
{
	return point3d_t(b.x/f,b.y/f,b.z/f);
}

inline point3d_t operator * (const point3d_t &b,float f)
{
	return point3d_t(b.x*f,b.y*f,b.z*f);
}

inline vector3d_t operator / (float f,const vector3d_t &b)
{
	return vector3d_t(b.x/f,b.y/f,b.z/f);
}

inline vector3d_t operator ^ ( const vector3d_t &a,const vector3d_t &b)
{
	return vector3d_t(a.y*b.z-a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

inline vector3d_t  operator - ( const vector3d_t &a,const vector3d_t &b)
{
	return vector3d_t(a.x-b.x,a.y-b.y,a.z-b.z);
}

inline vector3d_t  operator - ( const point3d_t &a,const point3d_t &b)
{
	return vector3d_t(a.x-b.x,a.y-b.y,a.z-b.z);
}

inline point3d_t  operator - ( const point3d_t &a,const vector3d_t &b)
{
	return point3d_t(a.x-b.x,a.y-b.y,a.z-b.z);
}

inline vector3d_t  operator - ( const vector3d_t &b)
{
	return vector3d_t(-b.x,-b.y,-b.z);
}

inline vector3d_t  operator + ( const vector3d_t &a,const vector3d_t &b)
{
	return vector3d_t(a.x+b.x,a.y+b.y,a.z+b.z);
}

inline point3d_t  operator + ( const point3d_t &a,const point3d_t &b)
{
	return point3d_t(a.x+b.x,a.y+b.y,a.z+b.z);
}

inline point3d_t  operator + ( const point3d_t &a,const vector3d_t &b)
{
	return point3d_t(a.x+b.x,a.y+b.y,a.z+b.z);
}

inline bool  operator == ( const point3d_t &a,const point3d_t &b)
{
	return ((a.x==b.x) && (a.y==b.y) && (a.z==b.z));
}

YAFRAYCORE_EXPORT bool  operator == ( const vector3d_t &a,const vector3d_t &b);
YAFRAYCORE_EXPORT bool  operator != ( const vector3d_t &a,const vector3d_t &b);

inline point3d_t mult(const point3d_t &a, const vector3d_t &b)
{
	return point3d_t(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline float vector3d_t::length()const
{
	return fSqrt(x*x+y*y+z*z);
}

inline float point3d_t::length()const
{
	return fSqrt(x*x+y*y+z*z);
}

inline vector3d_t& vector3d_t::normalize()
{
	float len = x*x + y*y + z*z;
	if (len > 0.f)
	{
		len = 1.0/fSqrt(len);
		x *= len;
		y *= len;
		z *= len;
	}
	return *this;
}

inline vector3d_t reflect_dir(const vector3d_t &n,const vector3d_t &v)
{
	const float vn = v*n;
	if (vn<0) return -v;
	return 2*vn*n - v;
}


inline vector3d_t toVector(const point3d_t &p)
{
	return vector3d_t(p.x,p.y,p.z);
}

YAFRAYCORE_EXPORT bool refract(const vector3d_t &n,const vector3d_t &wi, vector3d_t &wo, float IOR);
YAFRAYCORE_EXPORT bool refract_test(const vector3d_t &n,const vector3d_t &wi, float IOR);
YAFRAYCORE_EXPORT bool inv_refract_test(vector3d_t &n,const vector3d_t &wi, const vector3d_t &wo, float IOR);
YAFRAYCORE_EXPORT void fresnel(const vector3d_t & I, const vector3d_t & n, float IOR, float &Kr, float &Kt);
YAFRAYCORE_EXPORT void fast_fresnel(const vector3d_t & I, const vector3d_t & n, float IORF, float &Kr, float &Kt);

inline void createCS(const vector3d_t &N, vector3d_t &u, vector3d_t &v)
{
	if ((N.x==0) && (N.y==0))
	{
		if (N.z<0)
			u.set(-1, 0, 0);
		else
			u.set(1, 0, 0);
		v.set(0, 1, 0);
	}
	else
	{
		// Note: The root cannot become zero if
		// N.x==0 && N.y==0.
		const float d = 1.0/fSqrt(N.y*N.y + N.x*N.x);
		u.set(N.y*d, -N.x*d, 0);
		v = N^u;
	}
}

YAFRAYCORE_EXPORT void ShirleyDisk(float r1, float r2, float &u, float &v);

extern YAFRAYCORE_EXPORT int myseed;

inline int ourRandomI()
{
	const int a = 0x000041A7;
	const int m = 0x7FFFFFFF;
	const int q = 0x0001F31D; // m/a
	const int r = 0x00000B14; // m%a;
	myseed = a * (myseed % q) - r * (myseed/q);
	if (myseed < 0)
		myseed += m;
	return myseed;
}

inline float ourRandom()
{
	const int a = 0x000041A7;
	const int m = 0x7FFFFFFF;
	const int q = 0x0001F31D; // m/a
	const int r = 0x00000B14; // m%a;
	myseed = a * (myseed % q) - r * (myseed/q);
	if (myseed < 0)
		myseed += m;
	return (float)myseed/(float)m;
}

inline float ourRandom(int &seed)
{
	const int a = 0x000041A7;
	const int m = 0x7FFFFFFF;
	const int q = 0x0001F31D; // m/a
	const int r = 0x00000B14; // m%a;
	seed = a * (seed % q) - r * (seed/q);
	if (myseed < 0)
		myseed += m;
	return (float)seed/(float)m;
}

inline vector3d_t RandomSpherical()
{
	float r;
	vector3d_t v(0.0, 0.0, ourRandom());
	if ((r = 1.0 - v.z*v.z)>0.0) {
		float a = M_2PI * ourRandom();
		r = fSqrt(r);
		v.x = r * fCos(a);  v.y = r * fSin(a);
	}
	else v.z = 1.0;
	return v;
}

inline GFLOAT dot( const normal_t &a,const normal_t &b)
{
	return (a.x*b.x+a.y*b.y+a.z*b.z);
}

YAFRAYCORE_EXPORT vector3d_t randomVectorCone(const vector3d_t &D, const vector3d_t &U, const vector3d_t &V,
						float cosang, float z1, float z2);
YAFRAYCORE_EXPORT vector3d_t randomVectorCone(const vector3d_t &dir, float cosangle, float r1, float r2);
YAFRAYCORE_EXPORT vector3d_t discreteVectorCone(const vector3d_t &dir, float cangle, int sample, int square);

__END_YAFRAY

#endif // Y_VECTOR3D_H
