%module yafrayinterface

%include "cpointer.i"
%pointer_functions(float, floatp);
%pointer_functions(int, intp);
%pointer_functions(unsigned int, uintp);

%include "carrays.i"
%include "std_string.i"
%include "std_vector.i"

%array_functions(float, floatArray);

namespace std
{
	%template(StrVector) vector<string>;
}

%{
#include <interface/yafrayinterface.h>
#include <interface/xmlinterface.h>
#include <yafraycore/imageOutput.h>
#include <yafraycore/memoryIO.h>
using namespace yafaray;
%}

%feature("director") colorOutput_t::putPixel;
%feature("director") colorOutput_t::flush;
%feature("director") colorOutput_t::flushArea;

namespace yafaray {

class colorOutput_t
{
	public:
		virtual ~colorOutput_t() {};
		virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f)=0;
		virtual void flush()=0;
		virtual void flushArea(int x0, int y0, int x1, int y1)=0;
};

class imageHandler_t
{
public:
	virtual void initForOutput(int width, int height, bool withAlpha = false, bool withDepth = true) = 0;
	virtual ~imageHandler_t() {};
	virtual bool loadFromFile(const std::string &name) = 0;
	virtual bool loadFromMemory(const yByte *data, size_t size) {return false; };
	virtual bool saveToFile(const std::string &name) = 0;
	virtual void putPixel(int x, int y, const colorA_t &rgba, float depth = 0.f) = 0;
	virtual colorA_t getPixel(int x, int y) = 0;
	
protected:
	std::string handlerName;
	int m_width;
	int m_height;
	bool m_hasAlpha;
	bool m_hasDepth;
	rgba2DImage_nw_t *m_rgba;
	gray2DImage_nw_t *m_depth;
};

class imageOutput_t : public colorOutput_t
{
	public:
		imageOutput_t(imageHandler_t *handle, const std::string &name, int bx, int by);
		imageOutput_t(); //!< Dummy initializer
		virtual ~imageOutput_t();
		virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f);
		virtual void flush();
		virtual void flushArea(int x0, int y0, int x1, int y1) {}; // not used by images... yet
	private:
		imageHandler_t *image;
		std::string fname;
};

class yafrayInterface_t
{
	public:
		yafrayInterface_t();
		virtual ~yafrayInterface_t();
		// directly related to scene_t:
		virtual void loadPlugins(const char *path); //!< load plugins from path, if NULL load from default path, if available.
		virtual bool startGeometry(); //!< call before creating geometry; only meshes and vmaps can be created in this state
		virtual bool endGeometry(); //!< call after creating geometry;
		/*! start a triangle mesh
			in this state only vertices, UVs and triangles can be created
			\param id returns the ID of the created mesh
		*/
		virtual unsigned int getNextFreeID();
		virtual bool startTriMesh(unsigned int id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool startCurveMesh(unsigned int id, int vertices);
		virtual bool startTriMeshPtr(unsigned int *id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool endTriMesh(); //!< end current mesh and return to geometry state
		virtual bool endCurveMesh(const material_t *mat, float strandStart, float strandEnd, float strandShape); //!< end current mesh and return to geometry state
		virtual int  addVertex(double x, double y, double z); //!< add vertex to mesh; returns index to be used for addTriangle
		virtual int  addVertex(double x, double y, double z, double ox, double oy, double oz); //!< add vertex with Orco to mesh; returns index to be used for addTriangle
		virtual bool addTriangle(int a, int b, int c, const material_t *mat); //!< add a triangle given vertex indices and material pointer
		virtual bool addTriangle(int a, int b, int c, int uv_a, int uv_b, int uv_c, const material_t *mat); //!< add a triangle given vertex and uv indices and material pointer
		virtual int  addUV(float u, float v); //!< add a UV coordinate pair; returns index to be used for addTriangle
		virtual bool smoothMesh(unsigned int id, double angle); //!< smooth vertex normals of mesh with given ID and angle (in degrees)
		// functions to build paramMaps instead of passing them from Blender
		// (decouling implementation details of STL containers, paraMap_t etc. as much as possible)
		virtual void paramsSetPoint(const char* name, double x, double y, double z);
		virtual void paramsSetString(const char* name, const char* s);
		virtual void paramsSetBool(const char* name, bool b);
		virtual void paramsSetInt(const char* name, int i);
		virtual void paramsSetFloat(const char* name, double f);
		virtual void paramsSetColor(const char* name, float r, float g, float b, float a=1.f);
		virtual void paramsSetColor(const char* name, float *rgb, bool with_alpha=false);
		virtual void paramsSetMatrix(const char* name, float m[4][4], bool transpose=false);
		virtual void paramsSetMatrix(const char* name, double m[4][4], bool transpose=false);
		virtual void paramsSetMemMatrix(const char* name, float* matrix, bool transpose=false);
		virtual void paramsSetMemMatrix(const char* name, double* matrix, bool transpose=false);
		virtual void paramsClearAll(); 	//!< clear the paramMap and paramList
		virtual void paramsStartList(); //!< start writing parameters to the extended paramList (used by materials)
		virtual void paramsPushList(); 	//!< push new list item in paramList (e.g. new shader node description)
		virtual void paramsEndList(); 	//!< revert to writing to normal paramMap
		// functions directly related to renderEnvironment_t
		virtual light_t* 		createLight			(const char* name);
		virtual texture_t* 		createTexture		(const char* name);
		virtual material_t* 	createMaterial		(const char* name);
		virtual camera_t* 		createCamera		(const char* name);
		virtual background_t* 	createBackground	(const char* name);
		virtual integrator_t* 	createIntegrator	(const char* name);
		virtual VolumeRegion* 	createVolumeRegion	(const char* name);
		virtual imageHandler_t*	createImageHandler	(const char* name);
		virtual unsigned int 	createObject		(const char* name);
		virtual void clearAll(); //!< clear the whole environment + scene, i.e. free (hopefully) all memory.
		virtual void render(colorOutput_t &output, progressBar_t *pb = 0); //!< render the scene...
		virtual bool startScene(int type=0); //!< start a new scene; Must be called before any of the scene_t related callbacks!
		virtual void setInputGamma(float gammaVal, bool enable);
		virtual void abort();
		virtual paraMap_t* getRenderParameters() { return params; }
		virtual bool getRenderedImage(colorOutput_t &output); //!< put the rendered image to output
		virtual std::vector<std::string> listImageHandlers();
		virtual std::vector<std::string> listImageHandlersFullName();
		virtual std::string getImageFormatFromFullName(const std::string &fullname);
		virtual std::string getImageFullNameFromFormat(const std::string &format);

		virtual void setDrawParams(bool on = true);
		virtual bool getDrawParams();

		//Versioning stuff
		virtual char* getVersion() const;
		
		//Console Printing wrappers to report in color with yafaray's own coloring mechanic
		void printInfo(const std::string &msg);
		void printWarning(const std::string &msg);
		void printError(const std::string &msg);
	
	protected:
		paraMap_t *params;
		std::list<paraMap_t> *eparams; //! for materials that need to define a whole shader tree etc.
		paraMap_t *cparams; //! just a pointer to the current paramMap, either params or a eparams element
		renderEnvironment_t *env;
		scene_t *scene;
		imageFilm_t *film;
		float inputGamma;
		bool gcInput;
};


class xmlInterface_t: public yafrayInterface_t
{
	public:
		xmlInterface_t();
		// directly related to scene_t:
		virtual void loadPlugins(const char *path);
		virtual bool startGeometry();
		virtual bool endGeometry();
		virtual unsigned int getNextFreeID();
		virtual bool startTriMesh(unsigned int id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool startCurveMesh(unsigned int id, int vertices);
		virtual bool startTriMeshPtr(unsigned int *id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool endTriMesh();
		virtual bool endCurveMesh(const material_t *mat, float strandStart, float strandEnd, float strandShape); //!< end current mesh and return to geometry state
		virtual int  addVertex(double x, double y, double z);
		virtual int  addVertex(double x, double y, double z, double ox, double oy, double oz);
		virtual bool addTriangle(int a, int b, int c, const material_t *mat);
		virtual bool addTriangle(int a, int b, int c, int uv_a, int uv_b, int uv_c, const material_t *mat);
		virtual int  addUV(float u, float v);
		virtual bool startVmap(int id, int type, int dimensions);
		virtual bool endVmap();
		virtual bool addVmapValues(float *val);
		virtual bool smoothMesh(unsigned int id, double angle);
		
		// functions directly related to renderEnvironment_t
		virtual light_t* 		createLight		(const char* name);
		virtual texture_t* 		createTexture	(const char* name);
		virtual material_t* 	createMaterial	(const char* name);
		virtual camera_t* 		createCamera	(const char* name);
		virtual background_t* 	createBackground(const char* name);
		virtual integrator_t* 	createIntegrator(const char* name);
		virtual unsigned int 	createObject	(const char* name);
		virtual void clearAll(); //!< clear the whole environment + scene, i.e. free (hopefully) all memory.
		virtual void render(colorOutput_t &output); //!< render the scene...
		virtual bool startScene(int type=0); //!< start a new scene; Must be called before any of the scene_t related callbacks!
		
		virtual void setOutfile(const char *fname);
	protected:
		void writeParamMap(const paramMap_t &pmap, int indent=1);
		void writeParamList(int indent);
		
		std::map<const material_t *, std::string> materials;
		std::ofstream xmlFile;
		std::string xmlName;
		const material_t *last_mat;
		size_t nmat;
		int n_uvs;
		unsigned int nextObj;
};

class memoryIO_t : public colorOutput_t
{
	public:
		memoryIO_t(int resx, int resy, float* iMem);
		virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f);
		void flush();
		virtual void flushArea(int x0, int y0, int x1, int y1) {}; // no tiled file format used...yet
		virtual ~memoryIO_t();
	protected:
		int sizex, sizey;
		float* imageMem;
};

}
