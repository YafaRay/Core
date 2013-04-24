/****************************************************************************
 *
 * 		tgaUtils.h: Truevision TGA format utilities
 *      This is part of the yafray package
 *      Copyright (C) 2010 Rodrigo Placencia Vazquez
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

__BEGIN_YAFRAY

enum tgaImageDataType
{
	noData = 0,
	uncColorMap,
	uncTrueColor,
	uncGray,
	rleColorMap = 9,
	rleTrueColor,
	rleGray
};

// TGA image origin corner descriptions
// B = bottom, T =top, L = left, R = right
#define BL 					0x00
#define BR 					0x10
#define TL 					0x20
#define TR 					0x30

#define noAlpha 			0x00
#define alpha8  			0x08

// Mask defines
//15/16 bit color masking for BGRA color order on tga files
// B    |G    |R    |A
// 11111|11111|11111|1
#define BlueMask  			0xF800 // 11111|00000|00000|0
#define GreenMask 			0x07C0 // 00000|11111|00000|0
#define RedMask   			0x003E // 00000|00000|11111|0
#define AlphaMask 			0x0001 // 00000|00000|00000|1

// 8Bit gray + 8Bit alpha in 16Bit packets
#define alphaGrayMask8Bit	0xFF00 // 11111111|00000000
#define grayMask8Bit		0x00FF // 00000000|11111111

// Image description bit masks
#define alphaBitDepthMask 	0x0F   // 00|00|1111
#define TopMask 			0x20   // 00|10|0000
#define LeftMask			0x10   // 00|01|0000

#define rlePackMask			0x80   // 1|0000000
#define rleRepMask			0x7F   // 0|1111111

#define inv31  0.03225806451612903226 // 1 / 31
#define inv255 0.00392156862745098039 // 1 / 255

const char *tgaSignature = "TRUEVISION-XFILE.\0";

#pragma pack(push, 1)

struct tgaHeader_t
{
	tgaHeader_t()
	{
		idLength = 0;
		ColorMapType = 0;
		imageType = 0;
		cmFirstEntryIndex = 0;
		cmNumberOfEntries = 0;
		cmEntryBitDepth = 0;
		xOrigin = 0;
		yOrigin = 0;
		width = 0;
		height = 0;
		bitDepth = 0;
		desc = 0;
	}
// General image info

	yByte idLength;
	yByte ColorMapType; // 0 or 1 (off or on)
	yByte imageType; // one of tgaImageDataTypes

// ColorMap desc

	yWord cmFirstEntryIndex; // Used to offset the start of the ColorMap, ie. start at entry 127 out of 256 entries
	yWord cmNumberOfEntries;
	yByte cmEntryBitDepth; // 15, 16, 24 or 32

// Image descriptor

	yWord xOrigin; // used for Truevision TARGA display devices (anybody still has one?)
	yWord yOrigin; // used for Truevision TARGA display devices
	yWord width; // 0-65535
	yWord height; // 0-65535
	yByte bitDepth; // 8, 15, 16, 24 or 32
	yByte desc; // order of data from most significant bit:
	// |--|--|----| <- 8 bits total
	//  RR BL AlBD
	// RR = 00 <- Reserved
	// BL describe the order of the image data
	// B = 0 or 1 (0 - Bottom, 1 - Top)
	// L = 0 or 1 (0 - Left, 1 - Right)
	// ie. to describe image data ordered from the top left corner use BL = 10
	// AlBD is the bitdepth of the alpha channel, if 0 no alpha channel is defined, valid range 0-8
};

struct tgaFooter_t
{
	tgaFooter_t()
	{
		extOffset = 0;
		devAreaOffset = 0;
		for(int i = 0; i < 18; i++)
			signature[i] = tgaSignature[i];
	}
	int extOffset;
	int devAreaOffset;
	char signature[18];
};

struct tgaPixelRGB_t
{
	yByte B;
	yByte G;
	yByte R;
	tgaPixelRGB_t & operator = (const color_t &c)
	{
		R = (yByte)(c.getR() * 255.f);
		G = (yByte)(c.getG() * 255.f);
		B = (yByte)(c.getB() * 255.f);
		return *this;
	}
};

struct tgaPixelRGBA_t
{
	yByte B;
	yByte G;
	yByte R;
	yByte A;
	tgaPixelRGBA_t & operator = (const colorA_t &c)
	{
		R = (yByte)(c.getR() * 255.f);
		G = (yByte)(c.getG() * 255.f);
		B = (yByte)(c.getB() * 255.f);
		A = (yByte)(c.getA() * 255.f);
		return *this;
	}
};

#pragma pack(pop)

__END_YAFRAY
