/****************************************************************************
 *
 *      hdrUtils.h: Radiance RGBE format utilities
 *      This is part of the yafray package
 *      Copyright (C) 2010 George Laskowsky Ziguilinsky (Glaskows)
 *						   Rodrigo Placencia Vazquez (DarkTide)
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

#include <core_api/color.h>
#include <utilities/stringUtils.h>

__BEGIN_YAFRAY

#pragma pack(push, 1)

struct rgbeHeader_t
{
	rgbeHeader_t()
	{
		programType = "RADIANCE";
		exposure = 1.0f;
		yFirst = true;
	}

	float exposure; // in an image corresponds to <exposure> watts/steradian/m^2. Defaults to 1.0
	std::string programType; // A string that usually contains "RADIANCE"
	int min[2], max[2], step[2]; // Image boundaries and iteration stepping
	bool yFirst; // Indicates if the image scanlines are saved starting by y axis (Default: true)
};

struct rgbePixel_t
{
	rgbePixel_t &operator = (const color_t &c)
	{
		int e;
		float v = c.maximum();

		if (v < 1e-32)
		{
			R = G = B = E = 0;
		}
		else
		{
			v = frexp(v, &e) * 255.9999 / v;
			R = (yByte) (c.getR() * v);
			G = (yByte) (c.getG() * v);
			B = (yByte) (c.getB() * v);
			E = (yByte) (e + 128);
		}

		return *this;
	}

	rgbePixel_t &operator = (const rgbePixel_t &c)
	{
		R = c.R;
		G = c.G;
		B = c.B;
		E = c.E;

		return *this;
	}

	yByte &operator [] (int i)
	{
		return (&R)[i];
	}

	colorA_t getRGBA() const
	{
		float f;

		if(E)
		{   /*nonzero pixel*/
			f = fLdexp(1.0, E - (int) (128+8));
			return colorA_t(f * R, f * G, f * B, 1.0f);
		}

		return colorA_t(0.f, 0.f, 0.f, 1.0f);
	}

	bool isORLEDesc()
	{
		return ((R == 1) && (G == 1) && (B == 1));
	}

	bool isARLEDesc()
	{
		return ((R == 2) && (G == 2) && ((int)(B << 8 | E) < 0x8000));
	}

	int getORLECount(int rshift)
	{
		return ((int)E << rshift);
	}

	int getARLECount()
	{
		return ((int)(B << 8 | E));
	}

	void setScanlineStart(int w)
	{
		R = 2;
		G = 2;
		B = w >> 8;
		E = w & 0xFF;
	}

	yByte R;
	yByte G;
	yByte B;
	yByte E;
};

#pragma pack(pop)

__END_YAFRAY
