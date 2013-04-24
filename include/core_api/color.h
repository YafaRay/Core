/****************************************************************************
 *
 * 			color.h: Color type and operators api
 *      This is part of the yafray package
 *      Copyright (C) 2002  Alejandro Conty Est�vez
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
#ifndef Y_COLOR_H
#define Y_COLOR_H

#include<yafray_config.h>

#include <iostream>
#include <utilities/mathOptimizations.h>

#define COLOR_SIZE 3

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT color_t
{
	friend color_t operator * (const color_t &a, const color_t &b);
	friend color_t operator * (const CFLOAT f, const color_t &b);
	friend color_t operator * (const color_t &b, const CFLOAT f);
	friend color_t operator / (const color_t &b, const CFLOAT f);
	friend color_t operator + (const color_t &a, const color_t &b);
	friend color_t operator - (const color_t &a, const color_t &b);
	friend CFLOAT maxAbsDiff(const color_t &a, const color_t &b);
	friend YAFRAYCORE_EXPORT void operator >> (unsigned char *data, color_t &c);
	friend YAFRAYCORE_EXPORT void operator << (unsigned char *data, const color_t &c);
	friend YAFRAYCORE_EXPORT void operator >> (float *data, color_t &c);
	friend YAFRAYCORE_EXPORT void operator << (float *data, const color_t &c);
	friend YAFRAYCORE_EXPORT std::ostream & operator << (std::ostream & out, const color_t c);
	friend YAFRAYCORE_EXPORT color_t mix(const color_t &a, const color_t &b, CFLOAT point);
	friend YAFRAYCORE_EXPORT color_t convergenceAccell(const color_t &cn_1, const color_t &cn0, const color_t &cn1);
	public:
		color_t() { R=G=B=0; }
		color_t(CFLOAT r, CFLOAT g, CFLOAT b) {R=r;G=g;B=b;};
		color_t(CFLOAT g) { R=G=B=g; }
		color_t(CFLOAT af[3]) { R=af[0];  G=af[1];  B=af[2]; }
		bool isBlack() const { return ((R==0) && (G==0) && (B==0)); }
		bool isNaN() const { return (isnan(R) || isnan(G) || isnan(B)); }
		bool isInf() const { return (isinf(R) || isinf(G) || isinf(B)); }
		~color_t() {}
		void set(CFLOAT r, CFLOAT g, CFLOAT b) { R=r;  G=g;  B=b; }

		color_t & operator +=(const color_t &c);
		color_t & operator -=(const color_t &c);
		color_t & operator *=(const color_t &c);
		color_t & operator *=(CFLOAT f);

		CFLOAT energy() const {return (R+G+B)*0.333333f;};
		// Using ITU/Photometric values Y = 0.2126 R + 0.7152 G + 0.0722 B
		CFLOAT col2bri() const { return (0.2126f*R + 0.7152f*G + 0.0722f*B); }
		CFLOAT abscol2bri() const { return (0.2126f*std::fabs(R) + 0.7152f*std::fabs(G) + 0.0722f*std::fabs(B)); }
		void gammaAdjust(CFLOAT g){ R = fPow(R, g); G = fPow(G, g); B = fPow(B, g); }
		void expgam_Adjust (CFLOAT e, CFLOAT g, bool clamp_rgb);
		CFLOAT getR() const { return R; }
		CFLOAT getG() const { return G; }
		CFLOAT getB() const { return B; }

		// used in blendershader
		void invertRGB()
		{
			if (R!=0.f) R=1.f/R;
			if (G!=0.f) G=1.f/G;
			if (B!=0.f) B=1.f/B;
		}
		void absRGB() { R=std::fabs(R);  G=std::fabs(G);  B=std::fabs(B); }
		void darkenRGB(const color_t &col)
		{
			if (R>col.R) R=col.R;
			if (G>col.G) G=col.G;
			if (B>col.B) B=col.B;
		}
		void lightenRGB(const color_t &col)
		{
			if (R<col.R) R=col.R;
			if (G<col.G) G=col.G;
			if (B<col.B) B=col.B;
		}

		void black() { R=G=B=0; }
		CFLOAT minimum() const { return std::min(R, std::min(G, B)); }
		CFLOAT maximum() const { return std::max(R, std::max(G, B)); }
		CFLOAT absmax() const { return std::max(std::fabs(R), std::max(std::fabs(G), std::fabs(B))); }
		void clampRGB0()
		{
			if (R<0.0) R=0.0;
			if (G<0.0) G=0.0;
			if (B<0.0) B=0.0;
		}

		void clampRGB01()
		{
			if (R<0.0) R=0.0; else if (R>1.0) R=1.0;
			if (G<0.0) G=0.0; else if (G>1.0) G=1.0;
			if (B<0.0) B=0.0; else if (B>1.0) B=1.0;
		}
//	protected:
		CFLOAT R, G, B;
};

class YAFRAYCORE_EXPORT colorA_t : public color_t
{
	friend colorA_t operator * (const colorA_t &a, const colorA_t &b);
	friend colorA_t operator * (const CFLOAT f, const colorA_t &b);
	friend colorA_t operator * (const colorA_t &b, const CFLOAT f);
	friend colorA_t operator / (const colorA_t &b, const CFLOAT f);
	friend colorA_t operator + (const colorA_t &a, const colorA_t &b);
	friend colorA_t operator - (const colorA_t &a, const colorA_t &b);
	friend YAFRAYCORE_EXPORT void operator >> (unsigned char *data, colorA_t &c);
	friend YAFRAYCORE_EXPORT void operator << (unsigned char *data, const colorA_t &c);
	friend YAFRAYCORE_EXPORT void operator >> (float *data, colorA_t &c);
	friend YAFRAYCORE_EXPORT void operator << (float *data, const colorA_t &c);
	friend YAFRAYCORE_EXPORT std::ostream & operator << (std::ostream & out, const colorA_t c);
	friend YAFRAYCORE_EXPORT colorA_t mix(const colorA_t &a, const colorA_t &b, CFLOAT point);
	public:
		colorA_t() { /* A=0; */ }
		colorA_t(const color_t &c):color_t(c), A(1.f) { /* A=0; */ }
		colorA_t(const color_t &c, CFLOAT a):color_t(c), A(a) {}
		colorA_t(CFLOAT r, CFLOAT g, CFLOAT b, CFLOAT a=0):color_t(r,g,b), A(a) {}
		colorA_t(CFLOAT g):color_t(g) { A=g; }
		colorA_t(CFLOAT af[4]):color_t(af) { A=af[3]; }
		~colorA_t() {};
		void set(CFLOAT r, CFLOAT g, CFLOAT b, CFLOAT a=0) { color_t::set(r,g,b);  A=a; }

		colorA_t & operator +=(const colorA_t &c);
		colorA_t & operator -=(const colorA_t &c);
		colorA_t & operator *=(const colorA_t &c);
		colorA_t & operator *=(CFLOAT f);

		void alphaPremultiply() { R*=A; G*=A; B*=A; }
		CFLOAT getA() const { return A; }
		void setAlpha(CFLOAT a) { A=a; }

		void clampRGBA0()
		{
			clampRGB0();
			if (A<0.0) A=0.0;
		}

		void clampRGBA01()
		{
			clampRGB01();
			if (A<0.0) A=0.0; else if (A>1.0) A=1.0;
		}

//	protected:
		CFLOAT A;
};

class YAFRAYCORE_EXPORT rgbe_t
{
	public:
		rgbe_t() {rgbe[3]=0;};
		rgbe_t(const color_t &s);
		operator color_t ()const
		{
			color_t res;
			CFLOAT f;
			if (rgbe[3])
			{   /*nonzero pixel*/
				f = fLdexp(1.0,rgbe[3]-(int)(128+8));
				return color_t(rgbe[0] * f,rgbe[1] * f,rgbe[2] * f);
			}
			else return color_t(0,0,0);
		}
//		unsigned char& operator [] (int i){ return rgbe[i]; }

//	protected:
		unsigned char rgbe[4];
};

inline void color_t::expgam_Adjust(CFLOAT e, CFLOAT g, bool clamp_rgb)
{
	if ((e==0.f) && (g==1.f)) {
		if (clamp_rgb) clampRGB01();
		return;
	}
	if (e!=0.f) {
		// exposure adjust
		clampRGB0();
		R = 1.f - fExp(R*e);
		G = 1.f - fExp(G*e);
		B = 1.f - fExp(B*e);
	}
	if (g!=1.f) {
		// gamma adjust
		clampRGB0();
		R = fPow(R, g);
		G = fPow(G, g);
		B = fPow(B, g);
	}
}

YAFRAYCORE_EXPORT void operator >> (unsigned char *data,color_t &c);
YAFRAYCORE_EXPORT void operator << (unsigned char *data,const color_t &c);
YAFRAYCORE_EXPORT void operator >> (float *data, color_t &c);
YAFRAYCORE_EXPORT void operator << (float *data, const color_t &c);
YAFRAYCORE_EXPORT std::ostream & operator << (std::ostream & out,const color_t c);
YAFRAYCORE_EXPORT color_t mix(const color_t &a,const color_t &b,CFLOAT point);

YAFRAYCORE_EXPORT void operator >> (unsigned char *data,colorA_t &c);
YAFRAYCORE_EXPORT void operator << (unsigned char *data,const colorA_t &c);
YAFRAYCORE_EXPORT void operator >> (float *data, colorA_t &c);
YAFRAYCORE_EXPORT void operator << (float *data, const colorA_t &c);
YAFRAYCORE_EXPORT std::ostream & operator << (std::ostream & out,const colorA_t c);
YAFRAYCORE_EXPORT colorA_t mix(const colorA_t &a,const colorA_t &b,CFLOAT point);


inline color_t operator * (const color_t &a,const color_t &b)
{
	return color_t(a.R*b.R,a.G*b.G,a.B*b.B);
}

inline color_t operator * (const CFLOAT f,const color_t &b)
{
	return color_t(f*b.R,f*b.G,f*b.B);
}

inline color_t operator * (const color_t &b,const CFLOAT f)
{
	return color_t(f*b.R,f*b.G,f*b.B);
}

inline color_t operator / (const color_t &b,CFLOAT f)
{
	return color_t(b.R/f,b.G/f,b.B/f);
}

inline color_t operator + (const color_t &a,const color_t &b)
{
	return color_t(a.R+b.R,a.G+b.G,a.B+b.B);
}

inline color_t operator - (const color_t &a, const color_t &b)
{
	return color_t(a.R-b.R, a.G-b.G, a.B-b.B);
}

/*
inline color_t & color_t::operator *=(const color_t &c)
{
	FLUSH_3DNOW();
	MMX_LOAD64(MM0,c.R);
	MMX_LOAD32(MM1,c.B);
	MMX_LOAD64(MM2,R);
	MMX_LOAD32(MM3,B);
	MMX_MULF(MM0,MM2);
	MMX_MULF(MM1,MM3);
	MMX_STORE64(R,MM0);
	MMX_STORE32(B,MM1);
	FLUSH_3DNOW();
	return *this;
}*/

inline color_t & color_t::operator +=(const color_t &c)
{ R += c.R;  G += c.G;  B += c.B;  return *this; }
inline color_t & color_t::operator *=(const color_t &c)
{ R *= c.R;  G *= c.G;  B *= c.B;  return *this; }
inline color_t & color_t::operator *=(CFLOAT f)
{ R *= f;  G*= f;  B *= f;  return *this; }
inline color_t & color_t::operator -=(const color_t &c)
{ R -= c.R;  G -= c.G;  B -= c.B;  return *this; }

inline colorA_t operator * (const colorA_t &a,const colorA_t &b)
{
	return colorA_t(a.R*b.R, a.G*b.G, a.B*b.B, a.A*b.A);
}

inline colorA_t operator * (const CFLOAT f,const colorA_t &b)
{
	return colorA_t(f*b.R, f*b.G, f*b.B, f*b.A);
}

inline colorA_t operator * (const colorA_t &b,const CFLOAT f)
{
	return colorA_t(f*b.R, f*b.G, f*b.B, f*b.A);
}

inline colorA_t operator / (const colorA_t &b,CFLOAT f)
{
	if (f!=0) f=1.0/f;
	return colorA_t(b.R*f, b.G*f, b.B*f, b.A*f);
}

inline colorA_t operator + (const colorA_t &a,const colorA_t &b)
{
	return colorA_t(a.R+b.R, a.G+b.G, a.B+b.B, a.A+b.A);
}

inline colorA_t operator - (const colorA_t &a, const colorA_t &b)
{
	return colorA_t(a.R-b.R, a.G-b.G, a.B-b.B, a.A-b.A);
}

inline colorA_t & colorA_t::operator +=(const colorA_t &c) { R += c.R;  G += c.G;  B += c.B;  A += c.A;  return *this; }
inline colorA_t & colorA_t::operator *=(const colorA_t &c) { R *= c.R;  G *= c.G;  B *= c.B;  A *= c.A;  return *this; }
inline colorA_t & colorA_t::operator *=(CFLOAT f) { R *= f;  G*= f;  B *= f;  A *= f;  return *this; }
inline colorA_t & colorA_t::operator -=(const colorA_t &c) { R -= c.R;  G -= c.G;  B -= c.B;  A -= c.A;  return *this; }

inline CFLOAT maxAbsDiff(const color_t &a,const color_t &b)
{
	return (a - b).absmax();
}

YAFRAYCORE_EXPORT color_t convergenceAccell(const color_t &cn_1,const color_t &cn0,const color_t &cn1);

__END_YAFRAY

#endif // Y_COLOR_H
