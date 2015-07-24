#ifndef Y_MASKMAT_H
#define Y_MASKMAT_H

#include <yafray_config.h>
#include <yafraycore/nodematerial.h>

__BEGIN_YAFRAY

class texture_t;
class renderEnvironment_t;

class maskMat_t: public nodeMaterial_t
{
	public:
		maskMat_t(const material_t *m1, const material_t *m2, CFLOAT thresh, bool bCastShadows=true);
        virtual void initBSDF(const renderState_t &state, surfacePoint_t &sp, BSDF_t &bsdfTypes)const;
		virtual color_t eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const;
		virtual color_t sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const;
		virtual float pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const;
		virtual bool isTransparent() const;
		virtual bool castShadows() const { return mCastShadows; }
		virtual color_t getTransparency(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const;
		virtual void getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo,
								 bool &reflect, bool &refract, vector3d_t *const dir, color_t *const col)const;
		virtual color_t emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const;
		virtual CFLOAT getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const;
		static material_t* factory(paraMap_t &, std::list< paraMap_t > &, renderEnvironment_t &);
		
	protected:
		const material_t *mat1;
		const material_t *mat2;
		shaderNode_t* mask;
		CFLOAT threshold;
		bool mCastShadows; //!< enables/disables casting shadows. It should always be enabled except in specific cases.
		//const texture_t *mask;
};

__END_YAFRAY

#endif // Y_MASKMAT_H
