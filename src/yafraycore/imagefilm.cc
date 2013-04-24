/****************************************************************************
 *
 *      imagefilm.cc: image data handling class
 *      This is part of the yafray package
 *		See AUTHORS for more information
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

#include <core_api/imagefilm.h>
#include <core_api/imagehandler.h>
#include <yafraycore/monitor.h>
#include <yafraycore/timer.h>
#include <utilities/math_utils.h>
#include <resources/yafLogoTiny.h>

#include <yaf_revision.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <iomanip>

#if HAVE_FREETYPE
#include <resources/guifont.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

__BEGIN_YAFRAY

#define FILTER_TABLE_SIZE 16
#define MAX_FILTER_SIZE 8

//! Simple alpha blending
#define alphaBlend(b_bg_col, b_fg_col, b_alpha) (( b_bg_col * (1.f - b_alpha) ) + ( b_fg_col * b_alpha ))

typedef float filterFunc(float dx, float dy);

float Box(float dx, float dy){ return 1.f; }

/*!
Mitchell-Netravali constants
with B = 1/3 and C = 1/3 as suggested by the authors
mnX1 = constants for 1 <= |x| < 2
mna1 = (-B - 6 * C)/6
mnb1 = (6 * B + 30 * C)/6
mnc1 = (-12 * B - 48 * C)/6
mnd1 = (8 * B + 24 * C)/6

mnX2 = constants for 1 > |x|
mna2 = (12 - 9 * B - 6 * C)/6
mnb2 = (-18 + 12 * B + 6 * C)/6
mnc2 = (6 - 2 * B)/6

#define mna1 -0.38888889
#define mnb1  2.0
#define mnc1 -3.33333333
#define mnd1  1.77777778

#define mna2  1.16666666
#define mnb2 -2.0
#define mnc2  0.88888889
*/
#define gaussExp 0.00247875

float Mitchell(float dx, float dy)
{
	float x = 2.f * fSqrt(dx*dx + dy*dy);

	if(x >= 2.f) return (0.f);

	if(x >= 1.f) // from mitchell-netravali paper 1 <= |x| < 2
	{
		return (float)( x * ( x * ( x * -0.38888889f + 2.0f) - 3.33333333f) + 1.77777778f );
	}

	return (float)( x * x * ( 1.16666666f * x - 2.0f ) + 0.88888889f );
}

float Gauss(float dx, float dy)
{
	float r2 = dx*dx + dy*dy;
	return std::max(0.f, float(fExp(-6 * r2) - gaussExp));
}

//Lanczos sinc window size 2
float Lanczos2(float dx, float dy)
{
	float x = fSqrt(dx*dx + dy*dy);

	if(x == 0.f) return 1.f;

	if(-2 < x && x < 2)
	{
		float a = M_PI * x;
		float b = M_PI_2 * x;
		return ((fSin(a) * fSin(b)) / (a*b));
	}

	return 0.f;
}

imageFilm_t::imageFilm_t (int width, int height, int xstart, int ystart, colorOutput_t &out, float filterSize, filterType filt,
						  renderEnvironment_t *e, bool showSamMask, int tSize, imageSpliter_t::tilesOrderType tOrder, bool pmA, bool drawParams):
	flags(0), w(width), h(height), cx0(xstart), cy0(ystart), gamma(1.0), filterw(filterSize*0.5), output(&out),
	clamp(false), split(true), interactive(true), abort(false), correctGamma(false), splitter(0), pbar(0),
	env(e), showMask(showSamMask), tileSize(tSize), tilesOrder(tOrder), premultAlpha(pmA), drawParams(drawParams)
{
	cx1 = xstart + width;
	cy1 = ystart + height;
	filterTable = new float[FILTER_TABLE_SIZE * FILTER_TABLE_SIZE];

	image = new rgba2DImage_t(width, height);
	densityImage = NULL;
	estimateDensity = false;
	depthMap = NULL;
	dpimage = NULL;

	// fill filter table:
	float *fTp = filterTable;
	float scale = 1.f/(float)FILTER_TABLE_SIZE;

	filterFunc *ffunc=0;
	switch(filt)
	{
		case MITCHELL: ffunc = Mitchell; filterw *= 2.6f; break;
		case LANCZOS: ffunc = Lanczos2; break;
		case GAUSS: ffunc = Gauss; filterw *= 2.f; break;
		case BOX:
		default:	ffunc = Box;
	}

	filterw = std::min(std::max(0.501f, filterw), 0.5f * MAX_FILTER_SIZE); // filter needs to cover at least the area of one pixel and no more than MAX_FILTER_SIZE/2

	for(int y=0; y < FILTER_TABLE_SIZE; ++y)
	{
		for(int x=0; x < FILTER_TABLE_SIZE; ++x)
		{
			*fTp = ffunc((x+.5f)*scale, (y+.5f)*scale);
			++fTp;
		}
	}

	tableScale = 0.9999 * FILTER_TABLE_SIZE/filterw;
	area_cnt = 0;

	pbar = new ConsoleProgressBar_t(80);
}

imageFilm_t::~imageFilm_t ()
{
	delete image;
	if(depthMap) delete depthMap;
	if(densityImage) delete densityImage;
	delete[] filterTable;
	if(splitter) delete splitter;
	if(dpimage) delete dpimage;
	if(pbar) delete pbar; //remove when pbar no longer created by imageFilm_t!!
}

void imageFilm_t::init(int numPasses)
{
	// Clear color buffer
	image->clear();

	// Clear density image
	if(estimateDensity)
	{
		if(!densityImage) densityImage = new rgb2DImage_nw_t(w, h);
		else densityImage->clear();
	}

	// Setup the bucket splitter
	if(split)
	{
		next_area = 0;
		splitter = new imageSpliter_t(w, h, cx0, cy0, tileSize, tilesOrder);
		area_cnt = splitter->size();
	}
	else area_cnt = 1;

	if(pbar) pbar->init(area_cnt);

	abort = false;
	completed_cnt = 0;
	nPass = 1;
	nPasses = numPasses;
}

void imageFilm_t::initDepthMap()
{
	if(!depthMap) depthMap = new gray2DImage_t(w, h);
	else depthMap->clear();
}
void imageFilm_t::nextPass(bool adaptive_AA, std::string integratorName)
{
	int n_resample=0;

	splitterMutex.lock();
	next_area = 0;
	splitterMutex.unlock();
	nPass++;
	std::stringstream passString;

	if(flags) flags->clear();
	else flags = new tiledBitArray2D_t<3>(w, h, true);

	if(adaptive_AA && AA_thesh > 0.f)
	{
		for(int y=0; y<h-1; ++y)
		{
			for(int x = 0; x < w-1; ++x)
			{
				bool needAA = false;
				float c = (*image)(x, y).normalized().abscol2bri();
				if(std::fabs(c - (*image)(x+1, y).normalized().col2bri()) >= AA_thesh)
				{
					needAA=true; flags->setBit(x+1, y);
				}
				if(std::fabs(c - (*image)(x, y+1).normalized().col2bri()) >= AA_thesh)
				{
					needAA=true; flags->setBit(x, y+1);
				}
				if(std::fabs(c - (*image)(x+1, y+1).normalized().col2bri()) >= AA_thesh)
				{
					needAA=true; flags->setBit(x+1, y+1);
				}
				if(x > 0 && std::fabs(c - (*image)(x-1, y+1).normalized().col2bri()) >= AA_thesh)
				{
					needAA=true; flags->setBit(x-1, y+1);
				}
				if(needAA)
				{
					flags->setBit(x, y);

					if(interactive && showMask)
					{
						color_t pix = (*image)(x, y).normalized();
						color_t pixcol(0.f);

						if(pix.R < pix.G && pix.R < pix.B)
							pixcol.set(0.7f, c, c);
						else
							pixcol.set(c, 0.7f, c);

						output->putPixel(x, y, (const float *)&pixcol, false);
					}

					++n_resample;
				}
			}
		}
	}
	else
	{
		n_resample = h*w;
	}

	if(interactive) output->flush();

	passString << "Rendering pass " << nPass << " of " << nPasses << ", resampling " << n_resample << " pixels.";

	Y_INFO << integratorName << ": " << passString.str() << yendl;

	if(pbar)
	{
		pbar->init(area_cnt);
		pbar->setTag(passString.str().c_str());
	}
	completed_cnt = 0;
}

bool imageFilm_t::nextArea(renderArea_t &a)
{
	if(abort) return false;

	int ifilterw = (int) ceil(filterw);

	if(split)
	{
		int n;
		splitterMutex.lock();
		n = next_area++;
		splitterMutex.unlock();

		if(	splitter->getArea(n, a) )
		{
			a.sx0 = a.X + ifilterw;
			a.sx1 = a.X + a.W - ifilterw;
			a.sy0 = a.Y + ifilterw;
			a.sy1 = a.Y + a.H - ifilterw;

			if(interactive)
			{
				outMutex.lock();
				int end_x = a.X+a.W, end_y = a.Y+a.H;
				output->highliteArea(a.X, a.Y, end_x, end_y);
				outMutex.unlock();
			}

			return true;
		}
	}
	else
	{
		if(area_cnt) return false;
		a.X = cx0;
		a.Y = cy0;
		a.W = w;
		a.H = h;
		a.sx0 = a.X + ifilterw;
		a.sx1 = a.X + a.W - ifilterw;
		a.sy0 = a.Y + ifilterw;
		a.sy1 = a.Y + a.H - ifilterw;
		++area_cnt;
		return true;
	}
	return false;
}

void imageFilm_t::finishArea(renderArea_t &a)
{
	outMutex.lock();

	int end_x = a.X+a.W-cx0, end_y = a.Y+a.H-cy0;
	colorA_t col;

	for(int j=a.Y-cy0; j<end_y; ++j)
	{
		for(int i=a.X-cx0; i<end_x; ++i)
		{
			col = (*image)(i, j).normalized();
			col.clampRGB0();

			if(correctGamma) col.gammaAdjust(gamma);

			if(premultAlpha) col.alphaPremultiply();

			if(depthMap)
			{
				if( !output->putPixel(i, j, (const float*)&col, true, true, (*depthMap)(i, j).normalized()) ) abort=true;
			}
			else
			{
				if( !output->putPixel(i, j, (const float *)&col) ) abort=true;
			}

		}
	}

	if(interactive) output->flushArea(a.X, a.Y, end_x+cx0, end_y+cy0);

	if(pbar)
	{
		if(++completed_cnt == area_cnt) pbar->done();
		else pbar->update(1);
	}

	outMutex.unlock();
}

void imageFilm_t::flush(int flags, colorOutput_t *out)
{
	outMutex.lock();

	Y_INFO << "imageFilm: Flushing buffer..." << yendl;

	colorOutput_t *colout = out ? out : output;

	if (drawParams) drawRenderSettings();

#ifndef HAVE_FREETYPE
	Y_WARNING << "imageFilm: Compiled without FreeType support." << yendl;
	Y_WARNING << "imageFilm: Text on the parameters badge won't be available." << yendl;
#endif

	float multi = 0.f;
	colorA_t col;
	int k = 0;

	if(estimateDensity) multi = (float) (w * h) / (float) numSamples;

	for(int j = 0; j < h; j++)
	{
		for(int i = 0; i < w; i++)
		{
			if(flags & IF_IMAGE) col = (*image)(i, j).normalized();
			else col = colorA_t(0.f);

			if(estimateDensity && (flags & IF_DENSITYIMAGE)) col += (*densityImage)(i, j) * multi;

			col.clampRGB0();

			if(correctGamma) col.gammaAdjust(gamma);

			if(drawParams && h - j <= dpHeight && dpimage)
			{
				colorA_t &dpcol = (*dpimage)(i, k);
				col = colorA_t( alphaBlend(col, dpcol, dpcol.getA()), std::max(col.getA(), dpcol.getA()) );
			}

			if(premultAlpha) col.alphaPremultiply();

			if(depthMap)
			{
				colout->putPixel(i, j, (const float*)&col, true, true, (*depthMap)(i, j).normalized());
			}
			else
			{
				colout->putPixel(i, j, (const float*)&col);
			}
		}

		if(drawParams && h - j <= dpHeight) k++;
	}

	colout->flush();

	outMutex.unlock();

	Y_INFO << "imageFilm: Done." << yendl;
}

bool imageFilm_t::doMoreSamples(int x, int y) const
{
	return (AA_thesh>0.f) ? flags->getBit(x-cx0, y-cy0) : true;
}

/* CAUTION! Implemantation of this function needs to be thread safe for samples that
	contribute to pixels outside the area a AND pixels that might get
	contributions from outside area a! (yes, really!) */
void imageFilm_t::addSample(const colorA_t &c, int x, int y, float dx, float dy, const renderArea_t *a)
{
	colorA_t col = c;

	if(clamp) col.clampRGB01();

	int dx0, dx1, dy0, dy1, x0, x1, y0, y1;

	// get filter extent and make sure we don't leave image area:

	dx0 = std::max(cx0-x,   Round2Int( (double)dx - filterw));
	dx1 = std::min(cx1-x-1, Round2Int( (double)dx + filterw - 1.0));
	dy0 = std::max(cy0-y,   Round2Int( (double)dy - filterw));
	dy1 = std::min(cy1-y-1, Round2Int( (double)dy + filterw - 1.0));

	// get indizes in filter table
	double x_offs = dx - 0.5;

	int xIndex[MAX_FILTER_SIZE+1], yIndex[MAX_FILTER_SIZE+1];

	for (int i=dx0, n=0; i <= dx1; ++i, ++n)
	{
		double d = std::fabs( (double(i) - x_offs) * tableScale);
		xIndex[n] = Floor2Int(d);
	}

	double y_offs = dy - 0.5;

	for (int i=dy0, n=0; i <= dy1; ++i, ++n)
	{
		double d = std::fabs( (double(i) - y_offs) * tableScale);
		yIndex[n] = Floor2Int(d);
	}

	x0 = x+dx0; x1 = x+dx1;
	y0 = y+dy0; y1 = y+dy1;

	imageMutex.lock();

	for (int j = y0; j <= y1; ++j)
	{
		for (int i = x0; i <= x1; ++i)
		{
			// get filter value at pixel (x,y)
			int offset = yIndex[j-y0]*FILTER_TABLE_SIZE + xIndex[i-x0];
			float filterWt = filterTable[offset];
			// update pixel values with filtered sample contribution
			pixel_t &pixel = (*image)(i - cx0, j - cy0);

            if(premultAlpha) col.alphaPremultiply();

            pixel.col += (col * filterWt);

			pixel.weight += filterWt;
		}
	}

	imageMutex.unlock();
}

void imageFilm_t::addDepthSample(int chan, float val, int x, int y, float dx, float dy)
{
	int dx0, dx1, dy0, dy1, x0, x1, y0, y1;

	// get filter extent and make sure we don't leave image area:

	dx0 = std::max(cx0-x,   Round2Int( (double)dx - filterw));
	dx1 = std::min(cx1-x-1, Round2Int( (double)dx + filterw - 1.0));
	dy0 = std::max(cy0-y,   Round2Int( (double)dy - filterw));
	dy1 = std::min(cy1-y-1, Round2Int( (double)dy + filterw - 1.0));

	// get indizes in filter table
	double x_offs = dx - 0.5;

	int xIndex[MAX_FILTER_SIZE+1], yIndex[MAX_FILTER_SIZE+1];

	for (int i=dx0, n=0; i <= dx1; ++i, ++n)
	{
		double d = std::fabs( (double(i) - x_offs) * tableScale);
		xIndex[n] = Floor2Int(d);
	}

	double y_offs = dy - 0.5;

	for (int i=dy0, n=0; i <= dy1; ++i, ++n)
	{
		double d = std::fabs( (double(i) - y_offs) * tableScale);
		yIndex[n] = Floor2Int(d);
	}

	x0 = x+dx0; x1 = x+dx1;
	y0 = y+dy0; y1 = y+dy1;

	depthMapMutex.lock();

	for (int j = y0; j <= y1; ++j)
	{
		for (int i = x0; i <= x1; ++i)
		{
			// get filter value at pixel (x,y)
			int offset = yIndex[j-y0]*FILTER_TABLE_SIZE + xIndex[i-x0];
			float filterWt = filterTable[offset];
			// update pixel values with filtered sample contribution
			pixelGray_t &pixel = (*depthMap)(i - cx0, j - cy0);

			pixel.val += (val * filterWt);
			pixel.weight += filterWt;
		}
	}

	depthMapMutex.unlock();
}

void imageFilm_t::addDensitySample(const color_t &c, int x, int y, float dx, float dy, const renderArea_t *a)
{
	if(!estimateDensity) return;

	int dx0, dx1, dy0, dy1, x0, x1, y0, y1;

	// get filter extent and make sure we don't leave image area:

	dx0 = std::max(cx0-x,   Round2Int( (double)dx - filterw));
	dx1 = std::min(cx1-x-1, Round2Int( (double)dx + filterw - 1.0));
	dy0 = std::max(cy0-y,   Round2Int( (double)dy - filterw));
	dy1 = std::min(cy1-y-1, Round2Int( (double)dy + filterw - 1.0));


	int xIndex[MAX_FILTER_SIZE+1], yIndex[MAX_FILTER_SIZE+1];

	double x_offs = dx - 0.5;
	for (int i=dx0, n=0; i <= dx1; ++i, ++n)
	{
		double d = std::fabs( (double(i) - x_offs) * tableScale);
		xIndex[n] = Floor2Int(d);
	}

	double y_offs = dy - 0.5;
	for (int i=dy0, n=0; i <= dy1; ++i, ++n)
	{
		float d = fabsf( (double(i) - y_offs) * tableScale);
		yIndex[n] = Floor2Int(d);
	}

	x0 = x+dx0; x1 = x+dx1;
	y0 = y+dy0; y1 = y+dy1;

	densityImageMutex.lock();

	for (int j = y0; j <= y1; ++j)
	{
		for (int i = x0; i <= x1; ++i)
		{
			int offset = yIndex[j-y0]*FILTER_TABLE_SIZE + xIndex[i-x0];

			color_t &pixel = (*densityImage)(i - cx0, j - cy0);
			pixel += c * filterTable[offset];
		}
	}

	++numSamples;

	densityImageMutex.unlock();
}

void imageFilm_t::setDensityEstimation(bool enable)
{
	if(enable)
	{
		if(!densityImage) densityImage = new rgb2DImage_nw_t(w, h);
		else densityImage->clear();
	}
	else
	{
		if(densityImage) delete densityImage;
	}
	estimateDensity = enable;
}

void imageFilm_t::setGamma(float gammaVal, bool enable)
{
	correctGamma = enable;
	if(gammaVal > 0) gamma = 1.f/gammaVal; //gamma correction means applying gamma curve with 1/gamma
}

void imageFilm_t::setProgressBar(progressBar_t *pb)
{
	if(pbar) delete pbar;
	pbar = pb;
}

void imageFilm_t::setAAParams(const std::string &aa_params)
{
	aaSettings = aa_params;
}

void imageFilm_t::setIntegParams(const std::string &integ_params)
{
	integratorSettings = integ_params;
}

void imageFilm_t::setCustomString(const std::string &custom)
{
	customString = custom;
}

#if HAVE_FREETYPE

void imageFilm_t::drawFontBitmap( FT_Bitmap* bitmap, int x, int y)
{
	int i, j, p, q;
	int x_max = x + bitmap->width;
	int y_max = y + bitmap->rows;
	color_t textColor(1.f);
	int tmpBuf;
	float alpha;

	for ( i = x, p = 0; i < x_max; i++, p++ )
	{
		for ( j = y, q = 0; j < y_max; j++, q++ )
		{
			if ( i >= w || j >= h ) continue;

			tmpBuf = bitmap->buffer[q * bitmap->width + p];

			if (tmpBuf > 0)
			{
				colorA_t &col = (*dpimage)(i, j);
				alpha = (float) tmpBuf / 255.0;
				col = colorA_t(alphaBlend((color_t)col, textColor, alpha), col.getA());
			}
		}
	}
}

#endif

void imageFilm_t::drawRenderSettings()
{
	if(dpimage) return;

	dpHeight = 30;

	dpimage = new rgba2DImage_nw_t(w, dpHeight);
#ifdef HAVE_FREETYPE
	FT_Library library;
	FT_Face face;

	FT_GlyphSlot slot;
	FT_Vector pen; // untransformed origin

#ifdef RELEASE
	std::string version = std::string(VERSION);
#else
	std::string version = std::string(YAF_SVN_REV);
#endif

	std::stringstream ss;

	ss << "YafaRay (" << version << ")";

	ss << std::setprecision(2);
	double times = gTimer.getTime("rendert");
	int timem, timeh;
	gTimer.splitTime(times, &times, &timem, &timeh);
	ss << " | Render time:";
	if (timeh > 0) ss << " " << timeh << "h";
	if (timem > 0) ss << " " << timem << "m";
	ss << " " << times << "s";
	ss << " | " << aaSettings;
	ss << "\nLighting: " << integratorSettings;

	if(!customString.empty())
	{
		ss << " | " << customString;
	}

	std::string text = ss.str();

	// set font size at default dpi
	float fontsize = 9.5f;

	// initialize library
	if (FT_Init_FreeType( &library ))
	{
		Y_ERROR << "imageFilm: FreeType lib couldn't be initialized!" << yendl;
		return;
	}

	// create face object
	if (FT_New_Memory_Face( library, (const FT_Byte*)guifont, guifont_size, 0, &face ))
	{
		Y_ERROR << "imageFilm: FreeType couldn't load the font!" << yendl;
		return;
	}

	// set character size
	if (FT_Set_Char_Size( face, (FT_F26Dot6)(fontsize * 64.0), 0, 0, 0 ))
	{
		Y_ERROR << "imageFilm: FreeType couldn't set the character size!" << yendl;
		return;
	}

	slot = face->glyph;

	int textOffsetY = 18;
	int textInterlineOffset = 13;
#endif
	// offsets
	int textOffsetX = 4;
	int logoWidth = 0;

	// Draw logo image
	paraMap_t ihParams;
	ihParams["type"] = std::string("png");
	ihParams["for_output"] = false;

	imageHandler_t *logo = env->createImageHandler("logoLoader", ihParams, false);

	if(logo && logo->loadFromMemory(yafLogoTiny, yafLogoTiny_size))
	{
		int lx, ly;
		int imWidth = std::min(logo->getWidth(), w);
		int imHeight = std::min(logo->getHeight(), dpHeight);
		logoWidth = logo->getWidth();
		textOffsetX += logoWidth;

		for ( lx = 0; lx < imWidth; lx++ )
			for ( ly = 0; ly < imHeight; ly++ )
				(*dpimage)(lx, ly) = logo->getPixel(lx, ly);

		delete logo;
	}

	// Draw the dark bar at the bottom
	float bgAlpha = 0.4f;
	color_t bgColor(0.f);

	for ( int x = logoWidth; x < w; x++ )
	{
		for ( int y = 0; y < dpHeight; y++ )
		{
			(*dpimage)(x, y) = colorA_t(bgColor, bgAlpha);
		}
	}
#ifdef HAVE_FREETYPE
	// The pen position in 26.6 cartesian space coordinates
	pen.x = textOffsetX * 64;
	pen.y = textOffsetY * 64;

	// Draw the text
	for ( size_t n = 0; n < text.size(); n++ )
	{
		// Set Coordinates for the carrige return
		if (text[n] == '\n') {
			pen.x = textOffsetX * 64;
			pen.y -= textInterlineOffset * 64;
			continue;
		}

		// Set transformation
		FT_Set_Transform( face, 0, &pen );

		// Load glyph image into the slot (erase previous one)
		if (FT_Load_Char( face, text[n], FT_LOAD_DEFAULT ))
		{
			Y_ERROR << "imageFilm: FreeType Couldn't load the glyph image for: '" << text[n] << "'!" << yendl;
			continue;
		}

		// Render the glyph into the slot
		FT_Render_Glyph( slot, FT_RENDER_MODE_NORMAL );

		// Now, draw to our target surface (convert position)
		drawFontBitmap( &slot->bitmap, slot->bitmap_left, dpHeight - slot->bitmap_top);

		// increment pen position
		pen.x += slot->advance.x;
		pen.y += slot->advance.y;
	}

	// Cleanup
	FT_Done_Face    ( face );
	FT_Done_FreeType( library );
#endif
	Y_INFO << "imageFilm: Rendering parameters badge created." << yendl;
}

__END_YAFRAY
