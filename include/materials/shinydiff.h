#ifndef Y_SHINYDIFFMAT_H
#define Y_SHINYDIFFMAT_H

#include <yafray_config.h>
#include <yafraycore/nodematerial.h>
#include <core_api/shader.h>
#include <core_api/environment.h>

__BEGIN_YAFRAY

/*! A general purpose material for basic diffuse and specular reflecting
    surfaces with transparency and translucency support.
    Parameter definitions are as follows:
    Of the incoming Light, the specular reflected part is substracted.
        l' = l*(1.0 - specular_refl)
    Of the remaining light (l') the specular transmitted light is substracted.
        l" = l'*(1.0 - specular_transmit)
    Of the remaining light (l") the diffuse transmitted light (translucency)
    is substracted.
        l"' =  l"*(1.0 - translucency)
    The remaining (l"') light is either reflected diffuse or absorbed.
	
*/

class shinyDiffuseMat_t: public nodeMaterial_t
{
    public:
        shinyDiffuseMat_t(const color_t &col, const color_t &srcol, float diffuse, float transp=0.0, float transl=0.0, float sp_refl=0.0, float emit=0.0);
        virtual ~shinyDiffuseMat_t();
        virtual void initBSDF(const renderState_t &state, const surfacePoint_t &sp, BSDF_t &bsdfTypes)const;
        virtual color_t eval(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wl, BSDF_t bsdfs)const;
        virtual color_t sample(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, vector3d_t &wi, sample_t &s, float &W)const;
        virtual float pdf(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi, BSDF_t bsdfs)const;
        virtual bool isTransparent() const { return mIsTransparent; }
        virtual color_t getTransparency(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const;
        virtual color_t emit(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const; // { return emitCol; }
        virtual void getSpecular(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, bool &reflect, bool &refract, vector3d_t *const dir, color_t *const col)const;
        virtual CFLOAT getAlpha(const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo)const;
		
        static material_t* factory(paraMap_t &params, std::list<paraMap_t> &eparams, renderEnvironment_t &render);

        struct SDDat_t
        {
            float component[4];
            void *nodeStack;
        };

    protected:
        void config(shaderNode_t *diff, shaderNode_t *refl, shaderNode_t *transp, shaderNode_t *transl, shaderNode_t *bump);
        int getComponents(const bool *useNode, nodeStack_t &stack, float *component) const;
        void getFresnel(const vector3d_t &wo, const vector3d_t &N, float &Kr) const;

        void initOrenNayar(double sigma);
        CFLOAT OrenNayar(const vector3d_t &wi, const vector3d_t &wo, const vector3d_t &N) const;

        bool mIsTransparent;                //!< Boolean value which is true if you have transparent component
        bool mIsTranslucent;                //!< Boolean value which is true if you have translucent component
        bool mIsMirror;                     //!< Boolean value which is true if you have specular reflection component
        bool mIsDiffuse;                    //!< Boolean value which is true if you have diffuse component

        bool mHasFresnelEffect;             //!< Boolean value which is true if you have Fresnel specular effect
        float mIOR_Squared;                 //!< Squared IOR

        bool viNodes[4], vdNodes[4];        //!< describes if the nodes are viewdependant or not (if available)
        shaderNode_t *mDiffuseShader;       //!< Shader node for diffuse color
        shaderNode_t *mBumpShader;          //!< Shader node for bump
        shaderNode_t *mTransparencyShader;  //!< Shader node for transparency strength (float)
        shaderNode_t *mTranslucencyShader;  //!< Shader node for translucency strength (float)
        shaderNode_t *mMirrorShader;        //!< Shader node for specular reflection strength (float)
        shaderNode_t *mMirrorColorShader;   //!< Shader node for specular reflection color

        color_t mDiffuseColor;              //!< BSDF Diffuse component color
        color_t mEmitColor;                 //!< Emit color
        color_t mMirrorColor;               //!< BSDF Mirror component color
        float mMirrorStrength;              //!< BSDF Specular reflection component strength when not textured
        float mTransparencyStrength;        //!< BSDF Transparency component strength when not textured
        float mTranslucencyStrength;        //!< BSDF Translucency component strength when not textured
        float mDiffuseStrength;             //!< BSDF Diffuse component strength when not textured
        float mEmitStrength;                //!< Emit strength
        float mTransmitFilterStrength;      //!< determines how strong light passing through material gets tinted

        bool mUseOrenNayar;                 //!< Use Oren Nayar reflectance (default Lambertian)
        float mOrenNayar_A;                 //!< Oren Nayar A coefficient
        float mOrenNayar_B;                 //!< Oren Nayar B coefficient

        int nBSDF;
        BSDF_t cFlags[4];                   //!< list the BSDF components that are present
        int cIndex[4];                      //!< list the index of the BSDF components (0=specular reflection, 1=specular transparency, 2=translucency, 3=diffuse reflection)
};


__END_YAFRAY

#endif // Y_SHINYDIFFMAT_H
