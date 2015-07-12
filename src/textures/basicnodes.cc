
#include <textures/basicnodes.h>
#include <textures/layernode.h>
#include <core_api/object3d.h>
#include <core_api/camera.h>

__BEGIN_YAFRAY

textureMapper_t::textureMapper_t(const texture_t *texture): tex(texture), bumpStr(0.02), doScalar(true)
{
	map_x=1; map_y=2, map_z=3;
}

void textureMapper_t::setup()
{
	if(tex->discrete())
	{
		int u, v, w;
		tex->resolution(u, v, w);
		dU = 1.f/(float)u;
		dV = 1.f/(float)v;
		if(tex->isThreeD()) dW = 1.f/(float)w;
		else dW = 0.f;
	}
	else
	{
		float step = 0.0002f;
		dU = dV = dW = step;
	}

	pDU = point3d_t(dU, 0, 0);
	pDV = point3d_t(0, dV, 0);
	pDW = point3d_t(0, 0, dW);

	bumpStr /= scale.length();

	if (!tex->isNormalmap())
		bumpStr /= 100.0f;
}

// Map the texture to a cylinder
inline point3d_t tubemap(const point3d_t &p)
{
	point3d_t res;
	res.y = p.z;
	PFLOAT d = p.x*p.x + p.y*p.y;
	if (d>0) {
		res.z = 1.0/fSqrt(d);
		res.x = -atan2(p.x, p.y) * M_1_PI;
	}
	else res.x = res.z = 0;
	return res;
}

// Map the texture to a sphere
inline point3d_t spheremap(const point3d_t &p)
{
	point3d_t res(0.f);
	PFLOAT d = p.x*p.x + p.y*p.y + p.z*p.z;
	if (d>0) {
		res.z = fSqrt(d);
		if ((p.x!=0) && (p.y!=0)) res.x = -atan2(p.x, p.y) * M_1_PI;
		res.y = 1.0f - 2.0f*(fAcos(p.z/res.z) * M_1_PI);
	}
	return res;
}

// Map the texture to a cube
inline point3d_t cubemap(const point3d_t &p, const vector3d_t &n)
{
	const int ma[3][3] = { {1, 2, 0}, {0, 2, 1}, {0, 1, 2} };
	// int axis = std::fabs(n.x) > std::fabs(n.y) ? (std::fabs(n.x) > std::fabs(n.z) ? 0 : 2) : (std::fabs(n.y) > std::fabs(n.z) ? 1 : 2);
	// no functionality changes, just more readable code

	int axis;

	if (std::fabs(n.z) >= std::fabs(n.x) && std::fabs(n.z) >= std::fabs(n.y)) {
		axis= 2;
	}
	else if (std::fabs(n.y) >= std::fabs(n.x) && std::fabs(n.y) >= std::fabs(n.z)) {
		axis= 1;
	}
	else {
		axis= 0;
	}

	return point3d_t(p[ma[axis][0]], p[ma[axis][1]], p[ma[axis][2]]);
}

// Map the texture to a plane but it should not be used by now as it does nothing, it's just for completeness sake
inline point3d_t flatmap(const point3d_t &p)
{
	return p;
}

point3d_t textureMapper_t::doMapping(const point3d_t &p, const vector3d_t &N)const
{
	point3d_t texpt(p);
	// Texture coordinates standardized, if needed, to -1..1
	switch(tex_coords)
	{
		case TXC_UV:	texpt = point3d_t(2.0f*texpt.x-1.0f, 2.0f*texpt.y-1.0f, texpt.z); break;
		default: break;
	}
	// Texture axis mapping
	PFLOAT texmap[4] = {0, texpt.x, texpt.y, texpt.z};
	texpt.x=texmap[map_x];
	texpt.y=texmap[map_y];
	texpt.z=texmap[map_z];
	// Texture coordinates mapping
	switch(tex_maptype)
	{
		case TXP_TUBE:	texpt = tubemap(texpt); break;
		case TXP_SPHERE:texpt = spheremap(texpt); break;
		case TXP_CUBE:	texpt = cubemap(texpt, N); break;
		case TXP_PLAIN:	// texpt = flatmap(texpt); break;
		default: break;
	}
	// Texture scale and offset
	texpt = mult(texpt,scale) + offset;

	return texpt;
}


point3d_t eval_uv(const surfacePoint_t &sp)
{
	return point3d_t(sp.U, sp.V, 0.f);
}

void textureMapper_t::getCoords(point3d_t &texpt, vector3d_t &Ng, const surfacePoint_t &sp, const renderState_t &state) const
{
	switch(tex_coords)
	{
		case TXC_UV:	texpt = eval_uv(sp); Ng = sp.Ng; break;
		case TXC_ORCO:	texpt = sp.orcoP; Ng = sp.orcoNg; break;
		case TXC_TRAN:	texpt = mtx * sp.P; Ng = mtx * sp.Ng; break;  // apply 4x4 matrix of object for mapping also to true surface normals
		case TXC_WIN:	texpt = state.cam->screenproject(sp.P); Ng = sp.Ng; break;
		case TXC_STICK:	// Not implemented yet use GLOB
		case TXC_STRESS:// Not implemented yet use GLOB
		case TXC_TAN:	// Not implemented yet use GLOB
		case TXC_NOR:	// Not implemented yet use GLOB
		case TXC_REFL:	// Not implemented yet use GLOB
		case TXC_GLOB:	// GLOB mapped as default
		default:		texpt = sp.P; Ng = sp.Ng; break;
	}
}

void textureMapper_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	point3d_t texpt(0.f);
	vector3d_t Ng(0.f);

	getCoords(texpt, Ng, sp, state);

	texpt = doMapping(texpt, Ng);

	stack[this->ID] = nodeResult_t(tex->getColor(texpt), (doScalar) ? tex->getFloat(texpt) : 0.f );
}

// Basically you shouldn't call this anyway, but for the sake of consistency, redirect:
void textureMapper_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi)const
{
	eval(stack, state, sp);
}

// Normal perturbation

void textureMapper_t::evalDerivative(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	point3d_t texpt(0.f);
	vector3d_t Ng(0.f);
	float du = 0.0f, dv = 0.0f;

	getCoords(texpt, Ng, sp, state);

	//if (tex->discrete()) //FIXME!! Dirty workaround/hack to avoid artifacts (slightly shadowed triangles with slightly different normals?? coincidently with changes in dSdU, dSdV, etc) when using image textures for bump mapping. This change effectively disables this entire calculation intended to be done for image textures, which is not really a good solution. We sould review why is this problem happening in the first place. Looks like some strange issue when using dSdU/dSdV for calculations???
	if(false) //Due to the above explanation, all this "branch" is disabled for now...
	{
		texpt = doMapping(texpt, Ng);
		colorA_t color(0.f);
		vector3d_t norm(0.f);

		if (tex->isNormalmap())
		{
			// Get color from normal map texture
			color = tex->getNoGammaColor(texpt);

			// Assign normal map RGB colors to vector norm
			norm.x = color.getR();
			norm.y = color.getG();
			norm.z = color.getB();
			norm = (2.f * norm) - 1.f;

			// Convert norm into shading space
			du = norm * sp.dSdU;
			dv = norm * sp.dSdV;
		}
		else
		{
			point3d_t i0 = (texpt - pDU);
			point3d_t i1 = (texpt + pDU);
			point3d_t j0 = (texpt - pDV);
			point3d_t j1 = (texpt + pDV);
			float dfdu = (tex->getFloat(i0) - tex->getFloat(i1)) / dU;
			float dfdv = (tex->getFloat(j0) - tex->getFloat(j1)) / dV;

			// now we got the derivative in UV-space, but need it in shading space:
			vector3d_t vecU = sp.dSdU;
			vector3d_t vecV = sp.dSdV;
			vecU.z = dfdu;
			vecV.z = dfdv;
			// now we have two vectors NU/NV/df; Solve plane equation to get 1/0/df and 0/1/df (i.e. dNUdf and dNVdf)
			norm = vecU ^ vecV;
		}

		norm.normalize();

		if(std::fabs(norm.z) > 1e-30f)
		{
			float NF = 1.0/norm.z * bumpStr; // normalizes z to 1, why?
			du = norm.x * NF;
			dv = norm.y * NF;
		}
		else du = dv = 0.f;
	}
	else
	{
		// no uv coords -> procedurals usually, this mapping only depends on NU/NV which is fairly arbitrary
		// weird things may happen when objects are rotated, i.e. incorrect bump change
		point3d_t i0 = doMapping(texpt - dU * sp.NU, Ng);
		point3d_t i1 = doMapping(texpt + dU * sp.NU, Ng);
		point3d_t j0 = doMapping(texpt - dV * sp.NV, Ng);
		point3d_t j1 = doMapping(texpt + dV * sp.NV, Ng);

		du = (tex->getFloat(i0) - tex->getFloat(i1)) / dU;
		dv = (tex->getFloat(j0) - tex->getFloat(j1)) / dV;
		du *= bumpStr;
		dv *= bumpStr;
	}

	stack[this->ID] = nodeResult_t(colorA_t(du, dv, 0.f, 0.f), 0.f );
}

shaderNode_t* textureMapper_t::factory(const paraMap_t &params,renderEnvironment_t &render)
{
	const texture_t *tex=0;
	const std::string *texname=0, *option=0;
	TEX_COORDS tc = TXC_GLOB;
	TEX_PROJ maptype = TXP_PLAIN;
	float bumpStr = 1.f;
	bool scalar=true;
	int map[3] = { 1, 2, 3 };
	point3d_t offset(0.f), scale(1.f);
	matrix4x4_t mtx(1);
	if( !params.getParam("texture", texname) )
	{
		Y_ERROR << "TextureMapper: No texture given for texture mapper!" << yendl;
		return 0;
	}
	tex = render.getTexture(*texname);
	if(!tex)
	{
		Y_ERROR << "TextureMapper: texture '" << texname << "' does not exist!" << yendl;
		return 0;
	}
	textureMapper_t *tm = new textureMapper_t(tex);
	if(params.getParam("texco", option) )
	{
		if(*option == "uv") tc = TXC_UV;
		else if(*option == "global") tc = TXC_GLOB;
		else if(*option == "orco") tc = TXC_ORCO;
		else if(*option == "transformed") tc = TXC_TRAN;
		else if(*option == "window") tc = TXC_WIN;
		else if(*option == "normal") tc = TXC_NOR;
		else if(*option == "reflect") tc = TXC_REFL;
		else if(*option == "stick") tc = TXC_STICK;
		else if(*option == "stress") tc = TXC_STRESS;
		else if(*option == "tangent") tc = TXC_TAN;
	}
	if(params.getParam("mapping", option) && tex->discrete())
	{
		if(*option == "plain") maptype = TXP_PLAIN;
		else if(*option == "cube") maptype = TXP_CUBE;
		else if(*option == "tube") maptype = TXP_TUBE;
		else if(*option == "sphere") maptype = TXP_SPHERE;
	}
	params.getMatrix("transform", mtx);
	params.getParam("scale", scale);
	params.getParam("offset", offset);
	params.getParam("do_scalar", scalar);
	params.getParam("bump_strength", bumpStr);
	params.getParam("proj_x", map[0]);
	params.getParam("proj_y", map[1]);
	params.getParam("proj_z", map[2]);
	for(int i=0; i<3; ++i) map[i] = std::min(3, std::max(0, map[i]));
	tm->tex_coords = tc;
	tm->tex_maptype = maptype;
	tm->map_x = map[0];
	tm->map_y = map[1];
	tm->map_z = map[2];
	tm->scale = vector3d_t(scale);
	tm->offset = vector3d_t(2*offset);	// Offset need to be doubled due to -1..1 texture standardized wich results in a 2 wide width/height
	tm->doScalar = scalar;
	tm->bumpStr = bumpStr;
	tm->mtx = mtx;
	tm->setup();
	return tm;
}

/* ==========================================
/  The most simple node you could imagine...
/  ========================================== */

void valueNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	stack[this->ID] = nodeResult_t(color, value);
}

void valueNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi)const
{
	stack[this->ID] = nodeResult_t(color, value);
}

shaderNode_t* valueNode_t::factory(const paraMap_t &params,renderEnvironment_t &render)
{
	color_t col(1.f);
	float alpha=1.f;
	float val=1.f;
	params.getParam("color", col);
	params.getParam("alpha", alpha);
	params.getParam("scalar", val);
	return new valueNode_t(colorA_t(col, alpha), val);
}

/* ==========================================
/  A simple mix node, could be used to derive other math nodes
/ ========================================== */

mixNode_t::mixNode_t(): cfactor(0.f), input1(0), input2(0), factor(0)
{}

mixNode_t::mixNode_t(float val): cfactor(val), input1(0), input2(0), factor(0)
{}

void mixNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	float f2 = (factor) ? factor->getScalar(stack) : cfactor;
	float f1 = 1.f - f2, fin1, fin2;
	colorA_t cin1, cin2;
	if(input1)
	{
		cin1 = input1->getColor(stack);
		fin1 = input1->getScalar(stack);
	}
	else
	{
		cin1 = col1;
		fin1 = val1;
	}
	if(input2)
	{
		cin2 = input2->getColor(stack);
		fin2 = input2->getScalar(stack);
	}
	else
	{
		cin2 = col2;
		fin2 = val2;
	}

	colorA_t color = f1 * cin1 + f2 * cin2;
	float   scalar = f1 * fin1 + f2 * fin2;
	stack[this->ID] = nodeResult_t(color, scalar);
}

void mixNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi)const
{
	eval(stack, state, sp);
}

bool mixNode_t::configInputs(const paraMap_t &params, const nodeFinder_t &find)
{
	const std::string *name=0;
	if( params.getParam("input1", name) )
	{
		input1 = find(*name);
		if(!input1)
		{
			Y_ERROR << "MixNode: Couldn't get input1 " << *name << yendl;
			return false;
		}
	}
	else if(!params.getParam("color1", col1))
	{
		Y_ERROR << "MixNode: Color1 not set" << yendl;
		return false;
	}

	if( params.getParam("input2", name) )
	{
		input2 = find(*name);
		if(!input2)
		{
			Y_ERROR << "MixNode: Couldn't get input2 " << *name << yendl;
			return false;
		}
	}
	else if(!params.getParam("color2", col2))
	{
		Y_ERROR << "MixNode: Color2 not set" << yendl;
		return false;
	}

	if( params.getParam("factor", name) )
	{
		factor = find(*name);
		if(!factor)
		{
			Y_ERROR << "MixNode: Couldn't get factor " << *name << yendl;
			return false;
		}
	}
	else if(!params.getParam("value", cfactor))
	{
		Y_ERROR << "MixNode: Value not set" << yendl;
		return false;
	}

	return true;
}

bool mixNode_t::getDependencies(std::vector<const shaderNode_t*> &dep) const
{
	if(input1) dep.push_back(input1);
	if(input2) dep.push_back(input2);
	if(factor) dep.push_back(factor);
	return !dep.empty();
}

class addNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);

			cin1 += f2 * cin2;
			fin1 += f2 * fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class multNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;

			cin1 *= colorA_t(f1) + f2 * cin2;
			fin2 *= f1 + f2 * fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class subNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);

			cin1 -= f2 * cin2;
			fin1 -= f2 * fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class screenNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;

			colorA_t color = colorA_t(1.f) - (colorA_t(f1) + f2 * (1.f - cin2)) * (1.f - cin1);
			CFLOAT scalar   = 1.0 - (f1 + f2*(1.f - fin2)) * (1.f -  fin1);
			stack[this->ID] = nodeResult_t(color, scalar);
		}
};

class diffNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;

			cin1.R = f1*cin1.R + f2*std::fabs(cin1.R - cin2.R);
			cin1.G = f1*cin1.G + f2*std::fabs(cin1.G - cin2.G);
			cin1.B = f1*cin1.B + f2*std::fabs(cin1.B - cin2.B);
			cin1.A = f1*cin1.A + f2*std::fabs(cin1.A - cin2.A);
			fin1   = f1*fin1   + f2*std::fabs(fin1 - fin2);
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class darkNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);

			cin2 *= f2;
			if(cin2.R < cin1.R) cin1.R = cin2.R;
			if(cin2.G < cin1.G) cin1.G = cin2.G;
			if(cin2.B < cin1.B) cin1.B = cin2.B;
			if(cin2.A < cin1.A) cin1.A = cin2.A;
			fin2 *= f2;
			if(fin2 < fin1) fin1 = fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class lightNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);

			cin2 *= f2;
			if(cin2.R > cin1.R) cin1.R = cin2.R;
			if(cin2.G > cin1.G) cin1.G = cin2.G;
			if(cin2.B > cin1.B) cin1.B = cin2.B;
			if(cin2.A > cin1.A) cin1.A = cin2.A;
			fin2 *= f2;
			if(fin2 > fin1) fin1 = fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class overlayNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;

			colorA_t color;
			color.R = (cin1.R < 0.5f) ? cin1.R * (f1 + 2.0f*f2*cin2.R) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.R)) * (1.0 - cin1.R);
			color.G = (cin1.G < 0.5f) ? cin1.G * (f1 + 2.0f*f2*cin2.G) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.G)) * (1.0 - cin1.G);
			color.B = (cin1.B < 0.5f) ? cin1.B * (f1 + 2.0f*f2*cin2.B) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.B)) * (1.0 - cin1.B);
			color.A = (cin1.A < 0.5f) ? cin1.A * (f1 + 2.0f*f2*cin2.A) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.A)) * (1.0 - cin1.A);
			CFLOAT scalar = (fin1 < 0.5f) ? fin1 * (f1 + 2.0f*f2*fin2) : 1.0 - (f1 + 2.0f*f2*(1.0 - fin2)) * (1.0 - fin1);
			stack[this->ID] = nodeResult_t(color, scalar);
		}
};


shaderNode_t* mixNode_t::factory(const paraMap_t &params,renderEnvironment_t &render)
{
	float val=0.5f;
	int mode=0;
	params.getParam("cfactor", val);
	params.getParam("mode", mode);

	switch(mode)
	{
		case MN_MIX: 		return new mixNode_t(val);
		case MN_ADD: 		return new addNode_t();
		case MN_MULT: 		return new multNode_t();
		case MN_SUB: 		return new subNode_t();
		case MN_SCREEN:		return new screenNode_t();
		case MN_DIFF:		return new diffNode_t();
		case MN_DARK:		return new darkNode_t();
		case MN_LIGHT:		return new lightNode_t();
		case MN_OVERLAY:	return new overlayNode_t();
	}
	return new mixNode_t(val);
}

// ==================

extern "C"
{
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("texture_mapper", textureMapper_t::factory);
		render.registerFactory("value", valueNode_t::factory);
		render.registerFactory("mix", mixNode_t::factory);
		render.registerFactory("layer", layerNode_t::factory);
	}
}

__END_YAFRAY
