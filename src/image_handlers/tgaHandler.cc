/****************************************************************************
 *
 *      tgaHandler.cc: Truevision TGA format handler
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
 
#include <core_api/environment.h>
#include <core_api/imagehandler.h>
#include <core_api/params.h>

#include "tgaUtils.h"

#include <cstdio>

__BEGIN_YAFRAY

class tgaHandler_t;

typedef colorA_t (tgaHandler_t::*colorProcessor)(void *data);

class tgaHandler_t: public imageHandler_t
{
public:
	tgaHandler_t();
	void initForOutput(int width, int height, bool withAlpha = false, bool withDepth = false);
	void initForInput();
	~tgaHandler_t();
	bool loadFromFile(const std::string &name);
	bool saveToFile(const std::string &name);
	void putPixel(int x, int y, const colorA_t &rgba, float depth = 0.f);
	colorA_t getPixel(int x, int y);
	static imageHandler_t *factory(paraMap_t &params, renderEnvironment_t &render);

private:
	/*! Image data reading template functions */
	template <class ColorType> void readColorMap(FILE *fp, tgaHeader_t &header, colorProcessor cp);
	template <class ColorType> void readRLEImage(FILE *fp, colorProcessor cp);
	template <class ColorType> void readDirectImage(FILE *fp, colorProcessor cp);

	/*! colorProcesors definitions with signature colorA_t (void *)
	to be passed as pointer-to-non-static-member-functions */
	colorA_t processGray8(void *data);
	colorA_t processGray16(void *data);
	colorA_t processColor8(void *data);
	colorA_t processColor15(void *data);
	colorA_t processColor16(void *data);
	colorA_t processColor24(void *data);
	colorA_t processColor32(void *data);
	
	bool precheckFile(tgaHeader_t &header, const std::string &name, bool &isGray, bool &isRLE, bool &hasColorMap, yByte &alphaBitDepth);
	
	rgba2DImage_nw_t *ColorMap;
	size_t totPixels;
	size_t minX, maxX, stepX;
	size_t minY, maxY, stepY;

};

tgaHandler_t::tgaHandler_t()
{
	m_width = 0;
	m_height = 0;
	m_hasAlpha = false;
	m_hasDepth = false;
	
	m_rgba = NULL;
	m_depth = NULL;
	
	handlerName = "TGAHandler";
}

void tgaHandler_t::initForOutput(int width, int height, bool withAlpha, bool withDepth)
{
	m_width = width;
	m_height = height;
	m_hasAlpha = withAlpha;
	m_hasDepth = withDepth;
	
	m_rgba = new rgba2DImage_nw_t(m_width, m_height);
	
	if(m_hasDepth)
	{
		m_depth = new gray2DImage_nw_t(m_width, m_height);
	}
}

tgaHandler_t::~tgaHandler_t()
{
	if(m_rgba) delete m_rgba;
	if(m_depth) delete m_depth;
	m_rgba = NULL;
	m_depth = NULL;

}

bool tgaHandler_t::saveToFile(const std::string &name)
{
	Y_INFO << handlerName << ": Saving " << ((m_hasAlpha) ? "RGBA" : "RGB" ) << " file as \"" << name << "\"..." << yendl;

	std::string imageId = "Image rendered with YafaRay";
	tgaHeader_t header;
	tgaFooter_t footer;

	header.idLength = imageId.size();
	header.imageType = uncTrueColor;
	header.width = m_width;
	header.height = m_height;
	header.bitDepth = ((m_hasAlpha) ? 32 : 24 );
	header.desc = TL | ((m_hasAlpha) ? alpha8 : noAlpha );
	
	FILE* fp;
	
	fp = fopen(name.c_str(), "wb");

	if (fp == NULL)
		return false;
	else 
	{
		fwrite(&header, sizeof(tgaHeader_t), 1, fp);
		fwrite(imageId.c_str(), (size_t)header.idLength, 1, fp);
		
		for (int y = 0; y < m_height; y++) 
		{
			for (int x = 0; x < m_width; x++) 
			{
				(*m_rgba)(x, y).clampRGBA01();
				if(!m_hasAlpha)
				{
					tgaPixelRGB_t rgb;
					rgb = (color_t)(*m_rgba)(x, y);
					fwrite(&rgb, sizeof(tgaPixelRGB_t), 1, fp);
				}
				else
				{
					tgaPixelRGBA_t rgba;
					rgba = (*m_rgba)(x, y);
					fwrite(&rgba, sizeof(tgaPixelRGBA_t), 1, fp);
				}
			}
		}

		fwrite(&footer, sizeof(tgaFooter_t), 1, fp);
		fclose(fp);
	}
	
	if(m_hasDepth)
	{
		std::string depthName = name.substr(0, name.size() - 4) + "_zbuffer.tga";
		
		Y_INFO << handlerName << ": Saving Z-Buffer as \"" << depthName << "\"..." << yendl;
		
		header.idLength = imageId.size();
		header.imageType = uncGray;
		header.width = m_width;
		header.height = m_height;
		header.bitDepth = 8;
		header.desc = TL | noAlpha;
		
		fp = fopen(depthName.c_str(), "wb");

		if (fp == NULL)
			return false;
		else 
		{
			fwrite(&header, sizeof(tgaHeader_t), 1, fp);
			fwrite(imageId.c_str(), (size_t)header.idLength, 1, fp);
			for (int y = 0; y < m_height; y++) 
			{
				for (int x = 0; x < m_width; x++) 
				{
					yByte depth = (yByte)(std::max(0.f, std::min(1.f, (*m_depth)(x, y))) * 255.f);
					fwrite(&depth, sizeof(yByte), 1, fp);
				}
			}

			fwrite(&footer, sizeof(tgaFooter_t), 1, fp);
			fclose(fp);
		}
			
	}
	
	Y_INFO << handlerName << ": Done." << yendl;
	
	return true;
}

void tgaHandler_t::putPixel(int x, int y, const colorA_t &rgba, float depth)
{
	(*m_rgba)(x, y) = rgba;
	if(m_hasDepth) (*m_depth)(x, y) = depth;
}

colorA_t tgaHandler_t::getPixel(int x, int y)
{
	return (*m_rgba)(x, y);
}

template <class ColorType> void tgaHandler_t::readColorMap(FILE *fp, tgaHeader_t &header, colorProcessor cp)
{
	ColorType *color = new ColorType[header.cmNumberOfEntries];
	
	fread(color, sizeof(ColorType), header.cmNumberOfEntries, fp);
	
	for(int x = 0; x < (int)header.cmNumberOfEntries; x++)
	{
		(*ColorMap)(x, 0) = (this->*cp)(&color[x]);
	}
	
	delete [] color;
}

template <class ColorType> void tgaHandler_t::readRLEImage(FILE *fp, colorProcessor cp)
{
	size_t y = minY;
	size_t x = minX;

	while(!feof(fp) && y != maxY)
	{
		yByte packDesc = 0;
		fread(&packDesc, sizeof(yByte), 1, fp);
		
		bool rlePack = (packDesc & rlePackMask);
		int rleRep = (int)(packDesc & rleRepMask) + 1;

		ColorType color;
		
		if(rlePack) fread(&color, sizeof(ColorType), 1, fp);
		
		for(int i = 0; i < rleRep; i++)
		{
			if(!rlePack)  fread(&color, sizeof(ColorType), 1, fp);

			(*m_rgba)(x, y) = (this->*cp)(&color);
					  
			x += stepX;

			if(x == maxX)
			{
				x = minX;
				y += stepY;
			}
		}
	}
}

template <class ColorType> void tgaHandler_t::readDirectImage(FILE *fp, colorProcessor cp)
{
	ColorType *color = new ColorType[totPixels];

	fread(color, sizeof(ColorType), totPixels, fp);
	
	size_t i = 0;
	
	for(size_t y = minY; y != maxY; y += stepY)
	{
		for(size_t x = minX; x != maxX; x += stepX)
		{
			(*m_rgba)(x, y) = (this->*cp)(&color[i]);
			i++;
		}
	}
	
	delete [] color;
}

colorA_t tgaHandler_t::processGray8(void *data)
{
	return colorA_t(*(yByte *)data * inv255);
}

colorA_t tgaHandler_t::processGray16(void *data)
{
	yWord color = *(yWord *)data;
	return colorA_t( color_t((color & grayMask8Bit) * inv255),
				   ((color & alphaGrayMask8Bit) >> 8) * inv255);
}

colorA_t tgaHandler_t::processColor8(void *data)
{
	yByte color = *(yByte *)data;
	return (*ColorMap)(color, 0);
}

colorA_t tgaHandler_t::processColor15(void *data)
{
	yWord color = *(yWord *)data;
	return colorA_t(((color & RedMask  ) >> 11) * inv31,
					((color & GreenMask) >>  6) * inv31,
					((color & BlueMask ) >>  1) * inv31,
					1.f);
}

colorA_t tgaHandler_t::processColor16(void *data)
{
	yWord color = *(yWord *)data;
	return colorA_t(((color & RedMask  ) >> 11) * inv31,
					((color & GreenMask) >>  6) * inv31,
					((color & BlueMask ) >>  1) * inv31,
					(m_hasAlpha) ? (float)(color & AlphaMask) : 1.f);
}

colorA_t tgaHandler_t::processColor24(void *data)
{
	tgaPixelRGB_t *color = (tgaPixelRGB_t *)data;
	return colorA_t(color->R * inv255,
					color->G * inv255,
					color->B * inv255,
					1.f);
}

colorA_t tgaHandler_t::processColor32(void *data)
{
	tgaPixelRGBA_t *color = (tgaPixelRGBA_t *)data;
	return colorA_t(color->R * inv255,
					color->G * inv255,
					color->B * inv255,
					color->A * inv255);
}

bool tgaHandler_t::precheckFile(tgaHeader_t &header, const std::string &name, bool &isGray, bool &isRLE, bool &hasColorMap, yByte &alphaBitDepth)
{
	switch(header.imageType)
	{
		case noData:
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" has no image data!" << yendl;
			return false;
			break;
			
		case uncColorMap:
			if(!header.ColorMapType)
			{
				Y_ERROR << handlerName << ": TGA file \"" << name << "\" has ColorMap type and no color map embedded!" << yendl;
				return false;
			}
			hasColorMap = true;
			break;
			
		case uncGray:
			isGray = true;
			break;
			
		case rleColorMap:
			if(!header.ColorMapType)
			{
				Y_ERROR << handlerName << ": TGA file \"" << name << "\" has ColorMap type and no color map embedded!" << yendl;
				return false;
			}
			hasColorMap = true;
			isRLE = true;
			break;

		case rleGray:
			isGray = true;
			isRLE = true;
			break;
		
		case rleTrueColor:
			isRLE = true;
			break;
		
		case uncTrueColor:
			break;
	}
	
	if(hasColorMap)
	{
		if(header.cmEntryBitDepth != 15 && header.cmEntryBitDepth != 16 && header.cmEntryBitDepth != 24 && header.cmEntryBitDepth != 32)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" has a ColorMap bit depth not supported! (BitDepth:" << (int)header.cmEntryBitDepth << ")" << yendl;
			return false;
		}
	}
	
	if(isGray)
	{
		if (header.bitDepth != 8 && header.bitDepth != 16)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" has an invalid bit depth only 8 bit depth gray images are supported" << yendl;
			return false;
		}
		if(alphaBitDepth != 8 && header.bitDepth == 16)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" an invalid alpha bit depth for 16 bit gray image" << yendl;
			return false;
		}
	}
	else if(hasColorMap)
	{
		if(header.bitDepth > 16)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" has an invalid bit depth only 8 and 16 bit depth indexed images are supported" << yendl;
			return false;
		}
	}
	else
	{
		if(header.bitDepth != 15 && header.bitDepth != 16 && header.bitDepth != 24 && header.bitDepth != 32)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" has an invalid bit depth only 15/16, 24 and 32 bit depth true color images are supported (BitDepth: " << (int)header.bitDepth << ")" << yendl;
			return false;
		}
		if(alphaBitDepth != 1 && header.bitDepth == 16)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" an invalid alpha bit depth for 16 bit color image" << yendl;
			return false;
		}
		if(alphaBitDepth != 8 && header.bitDepth == 32)
		{
			Y_ERROR << handlerName << ": TGA file \"" << name << "\" an invalid alpha bit depth for 32 bit color image" << yendl;
			return false;
		}
	}
	
	return true;
}

bool tgaHandler_t::loadFromFile(const std::string &name)
{
	Y_INFO << handlerName << ": Loading image \"" << name << "\"..." << yendl;

	FILE *fp = fopen(name.c_str(), "rb");
	if(!fp)
	{
		Y_ERROR << handlerName << ": Cannot open file " << name << yendl;
		return false;
	}
	
	tgaHeader_t header;
	
	fread(&header, 1, sizeof(tgaHeader_t), fp);

	// Prereading checks

	yByte alphaBitDepth = (yByte)(header.desc & alphaBitDepthMask);
	
	m_width = header.width;
	m_height = header.height;
	m_hasAlpha = (alphaBitDepth != 0 || header.cmEntryBitDepth == 32);
	m_hasDepth = false;

	bool isRLE = false;
	bool hasColorMap = false;
	bool isGray = false;
	bool fromTop = ((header.desc & TopMask) >> 5);
	bool fromLeft = ((header.desc & LeftMask) >> 4);
	
	if(!precheckFile(header, name, isGray, isRLE, hasColorMap, alphaBitDepth))
	{
		fclose(fp);
		return false;
	}

	// Jump over any image Id
	fseek(fp, header.idLength, SEEK_CUR);
	
	if(m_rgba) delete m_rgba;
	m_rgba = new rgba2DImage_nw_t(m_width, m_height);
	
	ColorMap = NULL;
	
	// Read the colormap if needed
	if(hasColorMap)
	{
		ColorMap = new rgba2DImage_nw_t(header.cmNumberOfEntries, 1);
		
		switch(header.cmEntryBitDepth)
		{
			case 15:
				readColorMap<yWord>(fp, header, &tgaHandler_t::processColor15);
				break;
				
			case 16:
				readColorMap<yWord>(fp, header, &tgaHandler_t::processColor16);
				break;
				
			case 24:
				readColorMap<tgaPixelRGB_t>(fp, header, &tgaHandler_t::processColor24);
				break;
			
			case 32:
				readColorMap<tgaPixelRGBA_t>(fp, header, &tgaHandler_t::processColor32);
				break;
		}
	}
	
	totPixels = m_width * m_height;
	
	// Set the reading order to fit yafaray's image coordinates
	
	minX = 0;
	maxX = m_width;
	stepX = 1;

	minY = 0;
	maxY = m_height;
	stepY = 1;
	
	if(!fromTop)
	{
		minY = m_height - 1;
		maxY = -1;
		stepY = -1;
	}
	
	if(fromLeft)
	{
		minX = m_width - 1;
		maxX = -1;
		stepX = -1;
	}
	
	// Read the image data
	
	if(isRLE) // RLE compressed image data
	{	
		switch(header.bitDepth)
		{
			case 8: // Indexed color using ColorMap LUT or grayscale map
				readRLEImage<yByte>(fp, &tgaHandler_t::processColor8);
				break;
			
			case 15:
				readRLEImage<yWord>(fp, &tgaHandler_t::processColor15);
				break;
				
			case 16:
				if(isGray) readRLEImage<yWord>(fp, &tgaHandler_t::processGray16);
				else readRLEImage<yWord>(fp, &tgaHandler_t::processColor16);
				break;
				
			case 24:
				readRLEImage<tgaPixelRGB_t>(fp, &tgaHandler_t::processColor24);
				break;
				
			case 32:
				readRLEImage<tgaPixelRGBA_t>(fp, &tgaHandler_t::processColor32);
				break;
		}
	}
	else // Direct color data (uncompressed)
	{
		switch(header.bitDepth)
		{
			case 8: // Indexed color using ColorMap LUT or grayscale map
				if(isGray) readDirectImage<yByte>(fp, &tgaHandler_t::processGray8);
				else readDirectImage<yByte>(fp, &tgaHandler_t::processColor8);
				break;
			
			case 15:
				readDirectImage<yWord>(fp, &tgaHandler_t::processColor15);
				break;
				
			case 16:
				if(isGray) readDirectImage<yWord>(fp, &tgaHandler_t::processGray16);
				else readDirectImage<yWord>(fp, &tgaHandler_t::processColor16);
				break;
				
			case 24:
				readDirectImage<tgaPixelRGB_t>(fp, &tgaHandler_t::processColor24);
				break;
				
			case 32:
				readDirectImage<tgaPixelRGBA_t>(fp, &tgaHandler_t::processColor32);
				break;
		}
	}
	
	fclose(fp);
	fp = NULL;
	
	if (ColorMap) delete ColorMap;
	ColorMap = NULL;

	Y_INFO << handlerName << ": Done." << yendl;

	return true;
}

imageHandler_t *tgaHandler_t::factory(paraMap_t &params,renderEnvironment_t &render)
{
	int width = 0;
	int height = 0;
	bool withAlpha = false;
	bool withDepth = false;
	bool forOutput = true;

	params.getParam("width", width);
	params.getParam("height", height);
	params.getParam("alpha_channel", withAlpha);
	params.getParam("z_channel", withDepth);
	params.getParam("for_output", forOutput);
	
	imageHandler_t *ih = new tgaHandler_t();
	
	if(forOutput) ih->initForOutput(width, height, withAlpha, withDepth);
	
	return ih;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerImageHandler("tga", "tga tpic", "TGA [Truevision TARGA]", tgaHandler_t::factory);
	}

}
__END_YAFRAY
