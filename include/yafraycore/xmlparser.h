#ifndef Y_XMLPARSER_H
#define Y_XMLPARSER_H

#include "core_api/params.h"
#include <list>
#include <vector>

__BEGIN_YAFRAY

class scene_t;
class renderEnvironment_t;
class xmlParser_t;

YAFRAYCORE_EXPORT bool parse_xml_file(const char *filename, scene_t *scene, renderEnvironment_t *env, paraMap_t &render);

typedef void (*startElement_cb)(xmlParser_t &p, const char *element, const char **attrs);
typedef void (*endElement_cb)(xmlParser_t &p, const char *element);

#if HAVE_XML
struct parserState_t
{
	startElement_cb start;
	endElement_cb end;
	void *userdata;
	int level;
};

class xmlParser_t
{
	public:
		xmlParser_t(renderEnvironment_t *renv, scene_t *sc, paraMap_t &r);
		void pushState(startElement_cb start, endElement_cb end, void *userdata=0);
		void popState();
		void startElement(const char *element, const char **attrs){ ++level; if(current) current->start(*this, element, attrs); }
		void endElement(const char *element)	{ if(current) current->end(*this, element); --level; }
		void* stateData(){ return current->userdata; }
		void setParam(const std::string &name, parameter_t &param){ (*cparams)[name] = param; }
		int currLevel() const{ return level; }
		int stateLevel() const { return current ? current->level : -1; }
		
		renderEnvironment_t *env;
		scene_t *scene;
		paraMap_t params, &render;
		std::list<paraMap_t> eparams; //! for materials that need to define a whole shader tree etc.
		paraMap_t *cparams; //! just a pointer to the current paramMap, either params or a eparams element
	protected:
		std::vector<parserState_t> state_stack;
		parserState_t *current;
		int level;
};

// state callbacks:
void startEl_document(xmlParser_t &p, const char *element, const char **attrs);
void endEl_document(xmlParser_t &p, const char *element);
void startEl_scene(xmlParser_t &p, const char *element, const char **attrs);
void endEl_scene(xmlParser_t &p, const char *element);
void startEl_mesh(xmlParser_t &p, const char *element, const char **attrs);
void endEl_mesh(xmlParser_t &p, const char *element);
void startEl_instance(xmlParser_t &p, const char *element, const char **attrs);
void endEl_instance(xmlParser_t &p, const char *element);
void startEl_parammap(xmlParser_t &p, const char *element, const char **attrs);
void endEl_parammap(xmlParser_t &p, const char *element);
void startEl_paramlist(xmlParser_t &p, const char *element, const char **attrs);
void endEl_paramlist(xmlParser_t &p, const char *element);
void endEl_render(xmlParser_t &p, const char *element);

#endif // HAVE_XML

__END_YAFRAY

#endif // Y_XMLPARSER_H
