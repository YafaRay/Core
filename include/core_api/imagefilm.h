/****************************************************************************
 *
 *      imagefilm.h: image data handling class
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

#ifndef Y_IMAGEFILM_H
#define Y_IMAGEFILM_H

#include <yafray_config.h>

#include "color.h"
#include <yafraycore/ccthreads.h>
#include <core_api/output.h>
#include <core_api/imagesplitter.h>
#include <core_api/environment.h>
#include <utilities/image_buffers.h>
#include <utilities/tiled_array.h>

#if HAVE_FREETYPE
struct FT_Bitmap_;
#endif

__BEGIN_YAFRAY

/*!	This class recieves all rendered image samples.
	You can see it as an enhanced render buffer;
	Holds RGBA, Depth and Density (for actual bidirectional pathtracing implementation) buffers.
*/

class progressBar_t;

// Image types define
#define IF_IMAGE 1
#define IF_DENSITYIMAGE 2
#define IF_ALL (IF_IMAGE | IF_DENSITYIMAGE)

class YAFRAYCORE_EXPORT imageFilm_t
{
	public:
		enum filterType
		{
			BOX,
			MITCHELL,
			GAUSS,
			LANCZOS
		};

		/*! imageFilm_t Constructor */
		imageFilm_t(int width, int height, int xstart, int ystart, colorOutput_t &out, float filterSize=1.0, filterType filt=BOX,
		renderEnvironment_t *e = NULL, bool showSamMask = false, int tSize = 32,
		imageSpliter_t::tilesOrderType tOrder=imageSpliter_t::LINEAR, bool pmA = false, bool drawParams = false);
		/*! imageFilm_t Destructor */
		~imageFilm_t();
		/*! Initialize imageFilm for new rendering, i.e. set pixels black etc */
		void init(int numPasses = 0);
		/*! Allocates memory for the z-buffer rendering */
		void initDepthMap();
		/*! Prepare for next pass, i.e. reset area_cnt, check if pixels need resample...
			\param adaptive_AA if true, flag pixels to be resampled
			\param threshold color threshold for adaptive antialiasing */
		void nextPass(bool adaptive_AA, std::string integratorName);
		/*! Return the next area to be rendered
			CAUTION! This method MUST be threadsafe!
			\return false if no area is left to be handed out, true otherwise */
		bool nextArea(renderArea_t &a);
		/*! Indicate that all pixels inside the area have been sampled for this pass */
		void finishArea(renderArea_t &a);
		/*! Output all pixels to the color output */
		void flush(int flags = IF_ALL, colorOutput_t *out = 0);
		/*! query if sample (x,y) was flagged to need more samples.
			IMPORTANT! You may only call this after you have called nextPass(true, ...), otherwise
			no such flags have been created !! */
		bool doMoreSamples(int x, int y) const;
		/*!	Add image sample; dx and dy describe the position in the pixel (x,y).
			IMPORTANT: when a is given, all samples within a are assumed to come from the same thread!
			use a=0 for contributions outside the area associated with current thread!
		*/
		void addSample(const colorA_t &c, int x, int y, float dx, float dy, const renderArea_t *a = 0);
		/*!	Add depth (z-buffer) sample; dx and dy describe the position in the pixel (x,y)
		*/
		void addDepthSample(int chan, float val, int x, int y, float dx, float dy);
		/*!	Add light density sample; dx and dy describe the position in the pixel (x,y).
			IMPORTANT: when a is given, all samples within a are assumed to come from the same thread!
			use a=0 for contributions outside the area associated with current thread!
		*/
		void addDensitySample(const color_t &c, int x, int y, float dx, float dy, const renderArea_t *a = 0);
		//! Enables/Disables a light density estimation image
		void setDensityEstimation(bool enable);
		//! set number of samples for correct density estimation (if enabled)
		void setNumSamples(int n){ numSamples = n; }
		/*! Enables/disables color clamping */
		void setClamp(bool c){ clamp = c; }
		/*! Enables/disables gamma correction of output; when gammaVal is <= 0 the current value is kept */
		void setGamma(float gammaVal, bool enable);
		/*! Sets the adaptative AA sampling threshold */
		void setAAThreshold(CFLOAT thresh){ AA_thesh=thresh; }
		/*! Enables interactive color buffer output for preview during render */
		void setInteractive(bool ia){ interactive = ia; }
		/*! Sets a custom progress bar in the image film */
		void setProgressBar(progressBar_t *pb);
		/*! The following methods set the strings used for the parameters badge rendering */
		void setAAParams(const std::string &aa_params);
		void setIntegParams(const std::string &integ_params);
		void setCustomString(const std::string &custom);
		void setUseParamsBadge(bool on = true) { drawParams = on; }
		bool getUseParamsBadge() { return drawParams; }

		/*! Methods for rendering the parameters badge; Note that FreeType lib is needed to render text */
		void drawRenderSettings();

#if HAVE_FREETYPE
		void drawFontBitmap( FT_Bitmap_* bitmap, int x, int y);
#endif

	protected:
		rgba2DImage_t *image; //!< rgba color buffer
		gray2DImage_t *depthMap; //!< storage for z-buffer channel
		rgb2DImage_nw_t *densityImage; //!< storage for z-buffer channel
		rgba2DImage_nw_t *dpimage; //!< render parameters badge image
		tiledBitArray2D_t<3> *flags; //!< flags for adaptive AA sampling;
		int dpHeight; //!< height of the rendering parameters badge;
		int w, h, cx0, cx1, cy0, cy1;
		int area_cnt, completed_cnt;
		volatile int next_area;
		float gamma;
		CFLOAT AA_thesh;
		float filterw, tableScale;
		float *filterTable;
		colorOutput_t *output;
		// Thread mutes for shared access
		yafthreads::mutex_t imageMutex, splitterMutex, outMutex, depthMapMutex, densityImageMutex;
		bool clamp, split, interactive, abort, correctGamma;
		bool estimateDensity;
		int numSamples;
		imageSpliter_t *splitter;
		progressBar_t *pbar;
		renderEnvironment_t *env;
		int nPass;
		bool showMask;
		int tileSize;
		imageSpliter_t::tilesOrderType tilesOrder;
		bool premultAlpha;
		int nPasses;
		bool drawParams;
		std::string aaSettings;
		std::string integratorSettings;
		std::string customString;
};

__END_YAFRAY

#endif // Y_IMAGEFILM_H
