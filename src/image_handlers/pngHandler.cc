/****************************************************************************
 *
 *      pngHandler.cc: Portable Network Graphics (PNG) format handler
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

#include <png.h>

extern "C"
{
	#include <setjmp.h>
}

#include <cstdio>

#include "pngUtils.h"

__BEGIN_YAFRAY

class pngHandler_t: public imageHandler_t
{
public:
	pngHandler_t();
	~pngHandler_t();
	void initForOutput(int width, int height, bool withAlpha = false, bool withDepth = false);
	bool loadFromFile(const std::string &name);
	bool loadFromMemory(const yByte *data, size_t size);
	bool saveToFile(const std::string &name);
	void putPixel(int x, int y, const colorA_t &rgba, float depth = 0.f);
	colorA_t getPixel(int x, int y);
	static imageHandler_t *factory(paraMap_t &params, renderEnvironment_t &render);
private:
	void readFromStructs(png_structp pngPtr, png_infop infoPtr);
	bool fillReadStructs(yByte *sig, png_structp &pngPtr, png_infop &infoPtr);
	bool fillWriteStructs(FILE* fp, unsigned int colorType, png_structp &pngPtr, png_infop &infoPtr);
};

pngHandler_t::pngHandler_t()
{
	m_width = 0;
	m_height = 0;
	m_hasAlpha = false;
	m_hasDepth = false;

	m_rgba = NULL;
	m_depth = NULL;

	handlerName = "PNGHandler";
}

void pngHandler_t::initForOutput(int width, int height, bool withAlpha, bool withDepth)
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

pngHandler_t::~pngHandler_t()
{
	if(m_rgba) delete m_rgba;
	if(m_depth) delete m_depth;
	m_rgba = NULL;
	m_depth = NULL;

}

void pngHandler_t::putPixel(int x, int y, const colorA_t &rgba, float depth)
{
	(*m_rgba)(x, y) = rgba;
	if(m_hasDepth) (*m_depth)(x, y) = depth;
}

colorA_t pngHandler_t::getPixel(int x, int y)
{
	return (*m_rgba)(x, y);
}

bool pngHandler_t::saveToFile(const std::string &name)
{
	Y_INFO << handlerName << ": Saving RGB" << ( m_hasAlpha ? "A" : "" ) << " file as \"" << name << "\"..." << yendl;

	FILE *fp;
	png_structp pngPtr;
	png_infop infoPtr;
	int channels;
	png_bytep *rowPointers = NULL;

	fp = fopen(name.c_str(), "wb");

	if(!fp)
	{
		Y_ERROR << handlerName << ": Cannot open file " << name << yendl;
		return false;
	}

	if(!fillWriteStructs(fp, (m_hasAlpha) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB, pngPtr, infoPtr))
	{
		fclose(fp);
		return false;
	}

	rowPointers = new png_bytep[m_height];

	channels = 3;

	if(m_hasAlpha) channels++;

	for(int i = 0; i < m_height; i++)
	{
		rowPointers[i] = new yByte[ m_width * channels ];
	}

	for(int y = 0; y < m_height; y++)
	{
		for(int x = 0; x < m_width; x++)
		{
			colorA_t &color = (*m_rgba)(x, y);
			color.clampRGBA01();

			int i = x * channels;

			rowPointers[y][i]   = (yByte)(color.getR() * 255.f);
			rowPointers[y][i+1] = (yByte)(color.getG() * 255.f);
			rowPointers[y][i+2] = (yByte)(color.getB() * 255.f);
			if(m_hasAlpha) rowPointers[y][i+3] = (yByte)(color.getA() * 255.f);
		}
	}

	png_write_image(pngPtr, rowPointers);

	png_write_end(pngPtr, NULL);

	png_destroy_write_struct(&pngPtr, &infoPtr);

	fclose(fp);

	// cleanup:
	for(int i = 0; i < m_height; i++)
	{
		delete [] rowPointers[i];
	}

	delete[] rowPointers;

	if(m_hasDepth)
	{
		std::string zbufname = name.substr(0, name.size() - 4) + "_zbuffer.png";
		Y_INFO << handlerName << ": Saving Z-Buffer as \"" << zbufname << "\"..." << yendl;

		fp = fopen(zbufname.c_str(), "wb");

		if(!fp)
		{
			Y_ERROR << handlerName << ": Cannot open file " << zbufname << yendl;
			return false;
		}

		if(!fillWriteStructs(fp, PNG_COLOR_TYPE_GRAY, pngPtr, infoPtr))
		{
			fclose(fp);
			return false;
		}

		rowPointers = new png_bytep[m_height];

		for(int i = 0; i < m_height; i++)
		{
			rowPointers[i] = new yByte[ m_width ];
		}


		for(int y = 0; y < m_height; y++)
		{
			for(int x = 0; x < m_width; x++)
			{
				float color = std::max(0.f, std::min(1.f, (*m_depth)(x, y)));

				rowPointers[y][x] = (yByte)(color * 255.f);
			}
		}

		png_write_image(pngPtr, rowPointers);

		png_write_end(pngPtr, NULL);

		png_destroy_write_struct(&pngPtr, &infoPtr);

		fclose(fp);

		// cleanup:
		for(int i = 0; i < m_height; i++)
		{
			delete [] rowPointers[i];
		}

		delete[] rowPointers;
	}

	Y_INFO << handlerName << ": Done." << yendl;

	return true;
}

bool pngHandler_t::loadFromFile(const std::string &name)
{
	Y_INFO << handlerName << ": Loading image \"" << name << "\"..." << yendl;

	png_structp pngPtr = NULL;
	png_infop infoPtr = NULL;

	FILE *fp = fopen(name.c_str(), "rb");

	if(!fp)
	{
		Y_ERROR << handlerName << ": Cannot open file " << name << yendl;
		return false;
	}

	yByte signature[8];

    if(fread(signature, 1, 8, fp) != 8)
    {
    	 Y_ERROR << handlerName << ": EOF found or error reading image file while reading PNG signature." << yendl;
    	 return false;
    }

    if(!fillReadStructs(signature, pngPtr, infoPtr))
    {
    	fclose(fp);
    	return false;
    }

	png_init_io(pngPtr, fp);

	png_set_sig_bytes(pngPtr, 8);

	readFromStructs(pngPtr, infoPtr);

	fclose(fp);

	Y_INFO << handlerName << ": Done." << yendl;

	return true;
}
bool pngHandler_t::loadFromMemory(const yByte *data, size_t size)
{
	png_structp pngPtr = NULL;
	png_infop infoPtr = NULL;

	pngDataReader_t *reader = new pngDataReader_t(data, size);

	yByte signature[8];

    if(reader->read(signature, 8) < 8)
    {
    	 Y_ERROR << handlerName << ": EOF found on image data while reading PNG signature." << yendl;
    	 return false;
    }

    if(!fillReadStructs(signature, pngPtr, infoPtr))
    {
    	delete reader;
    	return false;
    }

	png_set_read_fn(pngPtr, (void*)reader, readFromMem);

	png_set_sig_bytes(pngPtr, 8);

	readFromStructs(pngPtr, infoPtr);

	delete reader;

	return true;
}

bool pngHandler_t::fillReadStructs(yByte *sig, png_structp &pngPtr, png_infop &infoPtr)
{
    if(png_sig_cmp(sig, 0, 8))
    {
		Y_ERROR << handlerName << ": Data is not from a PNG image!" << yendl;
    	return false;
    }

	if(!(pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		Y_ERROR << handlerName << ": Allocation of png struct failed!" << yendl;
		return false;
	}

	if(!(infoPtr = png_create_info_struct(pngPtr)))
	{
		png_destroy_read_struct(&pngPtr, NULL, NULL);
		Y_ERROR << handlerName << ": Allocation of png info failed!" << yendl;
		return false;
	}

	if(setjmp(png_jmpbuf(pngPtr)))
	{
		png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
		Y_ERROR << handlerName << ": Long jump triggered error!" << yendl;
		return false;
	}

	return true;
}

bool pngHandler_t::fillWriteStructs(FILE* fp, unsigned int colorType, png_structp &pngPtr, png_infop &infoPtr)
{
	if(!(pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		Y_ERROR << handlerName << ": Allocation of png struct failed!" << yendl;
		return false;
	}

	if(!(infoPtr = png_create_info_struct(pngPtr)))
	{
		png_destroy_read_struct(&pngPtr, NULL, NULL);
		Y_ERROR << handlerName << ": Allocation of png info failed!" << yendl;
		return false;
	}

	if(setjmp(png_jmpbuf(pngPtr)))
	{
		png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
		Y_ERROR << handlerName << ": Long jump triggered error!" << yendl;
		return false;
	}

	png_init_io(pngPtr, fp);

	png_set_IHDR(pngPtr, infoPtr, (png_uint_32)m_width, (png_uint_32)m_height,
				 8, colorType, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(pngPtr, infoPtr);

	return true;
}

void pngHandler_t::readFromStructs(png_structp pngPtr, png_infop infoPtr)
{
	png_uint_32 w, h;

	int bitDepth, colorType;

	m_hasDepth = false;

	png_read_info(pngPtr, infoPtr);

	png_get_IHDR(pngPtr, infoPtr, &w, &h, &bitDepth, &colorType, NULL, NULL, NULL);

	int numChan = png_get_channels(pngPtr, infoPtr);

	switch(colorType)
	{
		case PNG_COLOR_TYPE_RGB:
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			m_hasAlpha = true;
			break;

		case PNG_COLOR_TYPE_PALETTE:
			png_set_palette_to_rgb(pngPtr);
			if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS)) numChan = 4;
			else numChan = 3;
			break;

		case PNG_COLOR_TYPE_GRAY:
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			if (bitDepth < 8)
			{
				png_set_gray_to_rgb(pngPtr);
				bitDepth = 8;
			}
			break;

		default:
			Y_ERROR << handlerName << ": PNG type is not supported!" << yendl;
			longjmp(png_jmpbuf(pngPtr), 1);
	}

	// yes i know w and h are unsigned ints and the value can be much bigger, i'll think if a different memeber var is really needed since...
	// an image of 4,294,967,295 (max value of unsigned int) pixels on one side?
	// even 2,147,483,647 (max signed int positive value) pixels on one side is purpostrous
	// at 1 channel, 8 bits per channel and the other side of 1 pixel wide the resulting image uses 2gb of memory
	m_width = (int)w;
	m_height = (int)h;

	if(m_rgba) delete m_rgba;
	m_rgba = new rgba2DImage_nw_t(m_width, m_height);

	png_bytepp rowPointers = new png_bytep[m_height];

	int bitMult = 1;
	if(bitDepth == 16) bitMult = 2;

	for(int i = 0; i < m_height; i++)
	{
		rowPointers[i] = new yByte[ m_width * numChan * bitMult ];
	}

	png_read_image(pngPtr, rowPointers);

	float divisor = 1.f;
	if(bitDepth == 8) divisor = inv8;
	else if(bitDepth == 16) divisor = inv16;

	for(int x = 0; x < m_width; x++)
	{
		for(int y = 0; y < m_height; y++)
		{
			colorA_t &color = (*m_rgba)(x, y);

			int i = x * numChan * bitMult;
			float c = 0.f;

			if(bitDepth < 16)
			{
				switch(numChan)
				{
					case 4:
						color.set(rowPointers[y][i] * divisor,
								  rowPointers[y][i+1] * divisor,
								  rowPointers[y][i+2] * divisor,
								  rowPointers[y][i+3] * divisor);
						break;
					case 3:
						color.set(rowPointers[y][i] * divisor,
								  rowPointers[y][i+1] * divisor,
								  rowPointers[y][i+2] * divisor,
								  1.f);
						break;
					case 2:
						c = rowPointers[y][i] * divisor;
						color.set(c, c, c, rowPointers[y][i+1] * divisor);
						break;
					case 1:
						c = rowPointers[y][i] * divisor;
						color.set(c, c, c, 1.f);
						break;
				}
			}
			else
			{
				switch(numChan)
				{
					case 4:
						color.set((yWord)((rowPointers[y][i] << 8) | rowPointers[y][i+1]) * divisor,
								  (yWord)((rowPointers[y][i+2] << 8) | rowPointers[y][i+3]) * divisor,
								  (yWord)((rowPointers[y][i+4] << 8) | rowPointers[y][i+5]) * divisor,
								  (yWord)((rowPointers[y][i+6] << 8) | rowPointers[y][i+7]) * divisor);
						break;
					case 3:
						color.set((yWord)((rowPointers[y][i] << 8) | rowPointers[y][i+1]) * divisor,
								  (yWord)((rowPointers[y][i+2] << 8) | rowPointers[y][i+3]) * divisor,
								  (yWord)((rowPointers[y][i+4] << 8) | rowPointers[y][i+5]) * divisor,
								  1.f);
						break;
					case 2:
						c = (yWord)((rowPointers[y][i] << 8) | rowPointers[y][i+1]) * divisor;
						color.set(c, c, c, (yWord)((rowPointers[y][i+2] << 8) | rowPointers[y][i+3]) * divisor);
						break;
					case 1:
						c = (yWord)((rowPointers[y][i] << 8) | rowPointers[y][i+1]) * divisor;
						color.set(c, c, c, 1.f);
						break;
				}
			}
		}
	}

	png_read_end(pngPtr, infoPtr);

	png_destroy_read_struct(&pngPtr, &infoPtr, NULL);

	// cleanup:
	for(int i = 0; i < m_height; i++)
	{
		delete [] rowPointers[i];
	}

	delete[] rowPointers;
}

imageHandler_t *pngHandler_t::factory(paraMap_t &params, renderEnvironment_t &render)
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

	imageHandler_t *ih = new pngHandler_t();

	if(forOutput) ih->initForOutput(width, height, withAlpha, withDepth);

	return ih;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerImageHandler("png", "png", "PNG [Portable Network Graphics]", pngHandler_t::factory);
	}

}

__END_YAFRAY
