/****************************************************************************
 * 			xmlparser.cc: a libXML based parser for YafRay scenes
 *      This is part of the yafray package
 *      Copyright (C) 2006  Mathias Wein
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
 */

#include <yafraycore/xmlparser.h>
#include <core_api/environment.h>
#include <core_api/scene.h>
#if HAVE_XML
#include <libxml/parser.h>
#endif
#include <cstring>


__BEGIN_YAFRAY

#if HAVE_XML
void startDocument(void *user_data)
{
	//Empty
}

void endDocument(void *user_data)
{
	//Empty
}

void startElement(void * user_data, const xmlChar * name, const xmlChar **attrs)
{
	xmlParser_t &parser = *((xmlParser_t *)user_data);
	parser.startElement((const char *)name, (const char **)attrs);
}

void endElement(void *user_data, const xmlChar *name)
{
	xmlParser_t &parser = *((xmlParser_t *)user_data);
	parser.endElement((const char *)name);
}

static void my_warning(void *user_data, const char *msg, ...) 
{
    va_list args;

    va_start(args, msg);
    printf(msg, args);
    va_end(args);
}

static void my_error(void *user_data, const char *msg, ...) 
{
    va_list args;

    va_start(args, msg);
    printf(msg, args);
    va_end(args);
}

static void my_fatalError(void *user_data, const char *msg, ...) 
{
    va_list args;

    va_start(args, msg);
    printf(msg, args);
    va_end(args);
}

static xmlSAXHandler my_handler =
{
	NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  startDocument, //  startDocumentSAXFunc startDocument;
  endDocument, //  endDocumentSAXFunc endDocument;
  startElement, //  startElementSAXFunc startElement;
  endElement, //  endElementSAXFunc endElement;
  NULL,
  NULL, //  charactersSAXFunc characters;
  NULL,
  NULL,
  NULL,
  my_warning,
  my_error,
  my_fatalError
};
#endif // HAVE_XML

bool parse_xml_file(const char *filename, scene_t *scene, renderEnvironment_t *env, paraMap_t &render)
{
#if HAVE_XML
	xmlParser_t parser(env, scene, render);
	if (xmlSAXUserParseFile(&my_handler, &parser, filename) < 0)
	{
		Y_ERROR << "XMLParser: Parsing the file " << filename << yendl;
		return false;
	}
	return true;
#else
	Y_WARNING << "XMLParser: yafray was compiled without XML support, cannot parse file." << yendl;
	return false;
#endif
}

#if HAVE_XML
/*=============================================================
/ parser functions
=============================================================*/

xmlParser_t::xmlParser_t(renderEnvironment_t *renv, scene_t *sc, paraMap_t &r):
	env(renv), scene(sc), render(r), current(0), level(0)
{
	cparams = &params;
	pushState(startEl_document, endEl_document);
}

void xmlParser_t::pushState(startElement_cb start, endElement_cb end, void *userdata)
{
	parserState_t state;
	state.start = start;
	state.end = end;
	state.userdata = userdata;
	state.level = level;
	state_stack.push_back(state);
	current = &state_stack.back();
}

void xmlParser_t::popState()
{
	state_stack.pop_back();
	if(!state_stack.empty()) current = &state_stack.back();
	else current = 0;
}

/*=============================================================
/ utility functions...
=============================================================*/

inline bool str2bool(const char *s){ return strcmp(s, "true") ? false : true; }

static bool parsePoint(const char **attrs, point3d_t &p, point3d_t &op)
{
	for( ;attrs && attrs[0]; attrs += 2)
	{
		if(attrs[0][0] == 'o')
		{
			if(attrs[0][1] == 0 || attrs[0][2] != 0)
			{
				Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in orco point (1)" << yendl;
				continue; //it is not a single character
			}
			switch(attrs[0][1])
			{
				case 'x' : op.x = atof(attrs[1]); break;
				case 'y' : op.y = atof(attrs[1]); break;
				case 'z' : op.z = atof(attrs[1]); break;
				default: Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in orco point (2)" << yendl;
			}
			continue;
		}
		else if(attrs[0][1] != 0)
		{
			Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in point" << yendl;
			continue; //it is not a single character
		}
		switch(attrs[0][0])
		{
			case 'x' : p.x = atof(attrs[1]); break;
			case 'y' : p.y = atof(attrs[1]); break;
			case 'z' : p.z = atof(attrs[1]); break;
			default: Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in point" << yendl;
		}
	}

	return true;
}

static bool parseNormal(const char **attrs, normal_t &n)
{
	int compoRead = 0;
	for( ;attrs && attrs[0]; attrs += 2)
	{
		if(attrs[0][1] != 0)
		{
			Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in normal" << yendl;
			continue; //it is not a single character
		}
		switch(attrs[0][0])
		{
			case 'x' : n.x = atof(attrs[1]); compoRead++; break;
			case 'y' : n.y = atof(attrs[1]); compoRead++; break;
			case 'z' : n.z = atof(attrs[1]); compoRead++; break;
			default: Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in normal." << yendl;
		}
	}

	return (compoRead == 3);
}

void parseParam(const char **attrs, parameter_t &param)
{
	if(!attrs[0]) return;
	if(!attrs[2]) // only one attribute => bool, integer or float value
	{
		std::string name(attrs[0]);
		if(name == "ival"){ int i = atoi(attrs[1]); param = parameter_t(i); return; }
		else if(name == "fval"){ double f = atof(attrs[1]); param = parameter_t(f); return; }
		else if(name == "bval"){ bool b = str2bool(attrs[1]); param = parameter_t(b); return; }
		else if(name == "sval"){ param = parameter_t(std::string(attrs[1])); return; }
	}
	colorA_t c(0.f); point3d_t p(0,0,0);
	int type=TYPE_NONE;
	for(int n=0; attrs[n]; ++n)
	{
		if(attrs[n][1] != '\0') continue;
		switch(attrs[n][0])
		{
			case 'x': p.x = atof(attrs[n+1]); type = TYPE_POINT; break;
			case 'y': p.y = atof(attrs[n+1]); type = TYPE_POINT; break;
			case 'z': p.z = atof(attrs[n+1]); type = TYPE_POINT; break;
			
			case 'r': c.R = (CFLOAT)atof(attrs[n+1]); type = TYPE_COLOR; break;
			case 'g': c.G = (CFLOAT)atof(attrs[n+1]); type = TYPE_COLOR; break;
			case 'b': c.B = (CFLOAT)atof(attrs[n+1]); type = TYPE_COLOR; break;
			case 'a': c.A = (CFLOAT)atof(attrs[n+1]); type = TYPE_COLOR; break;
		}
	}
	
	switch(type)
	{
		case TYPE_POINT: param = parameter_t(p); break;
		case TYPE_COLOR: param = parameter_t(c); break;
	}
}

/*=============================================================
/ start- and endElement callbacks for the different states
=============================================================*/

void endEl_dummy(xmlParser_t &parser, const char *element)
{	parser.popState();	}

void startEl_dummy(xmlParser_t &parser, const char *element, const char **attrs)
{	parser.pushState(startEl_dummy, endEl_dummy);	}

void startEl_document(xmlParser_t &parser, const char *element, const char **attrs)
{
	if( strcmp(element, "scene") ) Y_WARNING << "XMLParser: skipping <" << element << ">" << yendl; /* parser.error("Expected scene definition"); */
	else
	{
		for( ;attrs && attrs[0]; attrs += 2)
		{
			if(!strcmp(attrs[0], "type") )
			{
				std::string val(attrs[1]);
				if		(val == "triangle")  parser.scene->setMode(0);
				else if	(val == "universal") parser.scene->setMode(1);
			}
		}
		parser.pushState(startEl_scene, endEl_scene);
	}
}

void endEl_document(xmlParser_t &parser, const char *element)
{
	Y_INFO << "XMLParser: Finished document" << yendl;
}

struct mesh_dat_t
{
	mesh_dat_t(): has_orco(false), has_uv(false), smooth(false), smooth_angle(0), ID(0), mat(0) {};
	bool has_orco, has_uv;
	bool smooth;
	float smooth_angle;
	objID_t ID;
	const material_t *mat;
};

// scene-state, i.e. expect only primary elements
// such as light, material, texture, object, integrator, render...

void startEl_scene(xmlParser_t &parser, const char *element, const char **attrs)
{
	std::string el(element), *name=0;
	if( el == "material" || el == "integrator" || el == "light" || el == "texture" ||
		el == "camera" || el == "background" || el == "object" || el == "volumeregion")
	{
		if(!attrs[0])
		{
			Y_ERROR << "XMLParser: No attributes for scene element given!" << yendl;
			return;
		}
		else if(!strcmp(attrs[0], "name")) name = new std::string(attrs[1]);
		else
		{
			Y_ERROR << "XMLParser: Attribute for scene element does not match 'name'!" << yendl;
			return;
		}
		parser.pushState(startEl_parammap, endEl_parammap, name);
	}
	else if(el == "mesh")
	{
		mesh_dat_t *md = new mesh_dat_t();
		int vertices=0, triangles=0, type=0, id=-1;
		for(int n=0; attrs[n]; ++n)
		{
			std::string name(attrs[n]);
			if(name == "has_orco") md->has_orco = str2bool(attrs[n+1]);
			else if(name == "has_uv") md->has_uv = str2bool(attrs[n+1]);
			else if(name == "vertices") vertices = atoi(attrs[n+1]);
			else if(name == "faces") triangles = atoi(attrs[n+1]);
			else if(name == "type")	type = atoi(attrs[n+1]);
			else if(name == "id" ) id = atoi(attrs[n+1]);
		}
		parser.pushState(startEl_mesh, endEl_mesh, md);
		if(!parser.scene->startGeometry()) Y_ERROR << "XMLParser: Invalid scene state on startGeometry()!" << yendl;

		// Get a new object ID if we did not get one
		if(id == -1) md->ID = parser.scene->getNextFreeID();

		if(!parser.scene->startTriMesh(md->ID, vertices, triangles, md->has_orco, md->has_uv, type))
		{
			Y_ERROR << "XMLParser: Invalid scene state on startTriMesh()!" << yendl;
		}
	}
	else if(el == "smooth")
	{
		unsigned int ID=0;
		float angle=181;
		for(int n=0; attrs[n]; ++n)
		{
			std::string name(attrs[n]);
			if(name == "ID") ID = atoi(attrs[n+1]);
			else if(name == "angle") angle = atof(attrs[n+1]);
		}
		//not optimal to take ID blind...
		parser.scene->startGeometry();
		bool success = parser.scene->smoothMesh(ID, angle);
		if(!success) Y_ERROR << "XMLParser: Couldn't smooth mesh ID = " << ID << ", angle = " << angle << yendl;
		parser.scene->endGeometry();
		parser.pushState(startEl_dummy, endEl_dummy);
	}
	else if(el == "render")
	{
		parser.cparams = &parser.render;
		parser.pushState(startEl_parammap, endEl_render);
	}
    else if(el == "instance")
	{
		int base_object_id = -1;
        int * boi_p = &base_object_id;
		for(int n=0; attrs[n]; n++)
		{
			std::string name(attrs[n]);
			if(name == "base_object_id") base_object_id = atoi(attrs[n+1]);
		}
		parser.pushState(startEl_instance,endEl_instance, boi_p);	
	}
	else Y_WARNING << "XMLParser: Skipping unrecognized scene element" << yendl;
}

void endEl_scene(xmlParser_t &parser, const char *element)
{
	if(strcmp(element, "scene")) Y_WARNING << "XMLParser: : expected </scene> tag!" << yendl;
	else
	{
		parser.popState();
	}
}

// mesh-state, i.e. expect only points (vertices), faces and material settings
// since we're supposed to be inside a mesh block, exit state on "mesh" element
void startEl_mesh(xmlParser_t &parser, const char *element, const char **attrs)
{
	std::string el(element);
	mesh_dat_t *dat = (mesh_dat_t *)parser.stateData();
	if(el == "p")
	{
		point3d_t p, op;
		if(!parsePoint(attrs, p, op)) return;
		if(dat->has_orco)	parser.scene->addVertex(p, op);
		else 				parser.scene->addVertex(p);
	}
	else if(el == "n")
	{
		normal_t n(0.0, 0.0, 0.0);
		if(!parseNormal(attrs, n)) return;
		parser.scene->addNormal(n);
	}
	else if(el == "f")
	{
		int a=0, b=0, c=0, uv_a=0, uv_b=0, uv_c=0;
		for( ;attrs && attrs[0]; attrs += 2)
		{
			if(attrs[0][1]==0) switch(attrs[0][0])
			{
				case 'a' : a = atoi(attrs[1]); break;
				case 'b' : b = atoi(attrs[1]); break;
				case 'c' : c = atoi(attrs[1]); break;
				default: Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in face" << yendl;
			}
			else
			{
				if(!strcmp(attrs[0], "uv_a")) 	   uv_a = atoi(attrs[1]);
				else if(!strcmp(attrs[0], "uv_b")) uv_b = atoi(attrs[1]);
				else if(!strcmp(attrs[0], "uv_c")) uv_c = atoi(attrs[1]);
			}
		}
		if(dat->has_uv) parser.scene->addTriangle(a, b, c, uv_a, uv_b, uv_c, dat->mat);
		else 			parser.scene->addTriangle(a, b, c, dat->mat);
	}
	else if(el == "uv")
	{
		float u=0, v=0;
		for( ;attrs && attrs[0]; attrs += 2)
		{
			switch(attrs[0][0])
			{
				case 'u': u = atof(attrs[1]); break;
				case 'v': v = atof(attrs[1]); break;
				default: Y_WARNING << "XMLParser: Ignored wrong attribute " << attrs[0] << " in uv" << yendl;
			}
		}
		parser.scene->addUV(u, v);
	}
	else if(el == "set_material")
	{
		std::string mat_name(attrs[1]);
		dat->mat = parser.env->getMaterial(mat_name);
		if(!dat->mat) Y_WARNING << "XMLParser: Unknown material!" << yendl;
	}
}

void endEl_mesh(xmlParser_t &parser, const char *element)
{
	if(std::string(element) == "mesh")
	{
		mesh_dat_t *md = (mesh_dat_t *)parser.stateData();
		if(!parser.scene->endTriMesh()) Y_ERROR << "XMLParser: Invalid scene state on endTriMesh()!" << yendl;
		if(!parser.scene->endGeometry()) Y_ERROR << "XMLParser: Invalid scene state on endGeometry()!" << yendl;
		delete md;
		parser.popState();
	}
}

void startEl_instance(xmlParser_t &parser, const char *element, const char **attrs)
{
	std::string el(element);
	int * boi_p = (int *)parser.stateData();
	if(el == "transform")
	{
		float m[4][4];
        for(int n=0; attrs[n]; ++n)
		{
            std::string name(attrs[n]);
            if(name ==  "m00") m[0][0] = atof(attrs[n+1]); 
            else if(name ==  "m01") m[0][1] = atof(attrs[n+1]); 
            else if(name ==  "m02") m[0][2] = atof(attrs[n+1]); 
            else if(name ==  "m03") m[0][3] = atof(attrs[n+1]); 
            else if(name ==  "m10") m[1][0] = atof(attrs[n+1]); 
            else if(name ==  "m11") m[1][1] = atof(attrs[n+1]); 
            else if(name ==  "m12") m[1][2] = atof(attrs[n+1]); 
            else if(name ==  "m13") m[1][3] = atof(attrs[n+1]); 
            else if(name ==  "m20") m[2][0] = atof(attrs[n+1]); 
            else if(name ==  "m21") m[2][1] = atof(attrs[n+1]); 
            else if(name ==  "m22") m[2][2] = atof(attrs[n+1]); 
            else if(name ==  "m23") m[2][3] = atof(attrs[n+1]); 
            else if(name ==  "m30") m[3][0] = atof(attrs[n+1]); 
            else if(name ==  "m31") m[3][1] = atof(attrs[n+1]); 
            else if(name ==  "m32") m[3][2] = atof(attrs[n+1]); 
            else if(name ==  "m33") m[3][3] = atof(attrs[n+1]); 
		}
        matrix4x4_t *m4 = new matrix4x4_t(m);
		objID_t boi = *boi_p;
        parser.scene->addInstance(boi,*m4);
	}
}

void endEl_instance(xmlParser_t &parser, const char *element)
{
	if(std::string(element) == "instance" )
	{
		parser.popState();
	}
}
// read a parameter map; take any tag as parameter name
// again, exit when end-element is on of the elements that caused to enter state
// depending on exit element, create appropriate scene element

void startEl_parammap(xmlParser_t &parser, const char *element, const char **attrs)
{
	// support for lists of paramMaps
	if(std::string(element) == "list_element")
	{
		parser.eparams.push_back(paraMap_t());
		parser.cparams = &parser.eparams.back();
		parser.pushState(startEl_paramlist, endEl_paramlist);
		return;
	}
	parameter_t p;
	parseParam(attrs, p);
	parser.setParam(std::string(element), p);
}

void endEl_parammap(xmlParser_t &p, const char *element)
{
	bool exit_state= (p.currLevel() == p.stateLevel());	
	if(exit_state)
	{
		std::string el(element);
		std::string *name = (std::string *)p.stateData();
		if(!name) Y_ERROR << "XMLParser: No name for scene element available!" << yendl;
		else
		{
			if(el == "material")
			{ p.env->createMaterial(*name, p.params, p.eparams); }
			else if(el == "integrator")
			{ p.env->createIntegrator(*name, p.params); }
			else if(el == "light")
			{
				light_t *light = p.env->createLight(*name, p.params);
				if(light) p.scene->addLight(light);
			}
			else if(el == "texture")
			{ p.env->createTexture(*name, p.params); }
			else if(el == "camera")
			{ p.env->createCamera(*name, p.params); }
			else if(el == "background")
			{ p.env->createBackground(*name, p.params); }
			else if(el == "object")
			{
				objID_t id;
				object3d_t *obj = p.env->createObject(*name, p.params);
				if(obj) p.scene->addObject(obj, id);
			}
			else if(el == "volumeregion")
			{
				VolumeRegion* vr = p.env->createVolumeRegion(*name, p.params);
				if (vr) p.scene->addVolumeRegion(vr);
			}
			else Y_WARNING << "XMLParser: Unexpected end-tag of scene element!" << yendl;
		}
		
		if(name) delete name;
		p.popState(); p.params.clear(); p.eparams.clear();
	}
}

void startEl_paramlist(xmlParser_t &parser, const char *element, const char **attrs)
{
	parameter_t p;
	parseParam(attrs, p);
	parser.setParam(std::string(element), p);
}

void endEl_paramlist(xmlParser_t &p, const char *element)
{
	if(std::string(element) == "list_element")
	{
		p.popState();
		p.cparams = &p.params;
	}
}

void endEl_render(xmlParser_t &p, const char *element)
{
	if(!strcmp(element, "render"))
	{
		p.cparams = &p.params;
		p.popState();
	}
}

#endif // HAVE_XML

__END_YAFRAY
