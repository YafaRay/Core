
#ifndef Y_TRIANGLE_H
#define Y_TRIANGLE_H

#include <yafray_config.h>

#include <core_api/primitive.h>
//#include <core_api/object3d.h>

__BEGIN_YAFRAY

#define Y_MIN3(a,b,c) ( ((a)>(b)) ? ( ((b)>(c))?(c):(b)):( ((a)>(c))?(c):(a)) )
#define Y_MAX3(a,b,c) ( ((a)<(b)) ? ( ((b)>(c))?(b):(c)):( ((a)>(c))?(a):(c)) )

// triBoxOverlap() is in src/yafraycore/tribox3_d.cc!
int triBoxOverlap(double boxcenter[3],double boxhalfsize[3],double triverts[3][3]);

class triangleObject_t;
class triangleObjectInstance_t;
class meshObject_t;
class triangleInstance_t;

/*! non-inherited triangle, so no virtual functions to allow inlining
	othwise totally identically to vTriangle_t (when it actually ever
	makes it into release)
*/
class YAFRAYCORE_EXPORT triangle_t
{
	friend class scene_t;
	friend class triangleObject_t;
	friend class triangleInstance_t;
	
	public:
		triangle_t(): pa(-1), pb(-1), pc(-1), na(-1), nb(-1), nc(-1), mesh(NULL) { /* Empty */ }
        triangle_t(int ia, int ib, int ic, triangleObject_t* m): pa(ia), pb(ib), pc(ic), na(-1), nb(-1), nc(-1), mesh(m) { /* Empty */ }
		virtual bool intersect(const ray_t &ray, float *t, intersectData_t &data) const;
		virtual bound_t getBound() const;
		virtual bool intersectsBound(exBound_t &eb) const;
		virtual bool clippingSupport() const{ return true; }
		// return: false:=doesn't overlap bound; true:=valid clip exists
		virtual bool clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const;
		virtual const material_t* getMaterial() const { return material; }	
		virtual void getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const;
		virtual float surfaceArea() const;
		virtual void sample(float s1, float s2, point3d_t &p, vector3d_t &n) const;
		
		virtual vector3d_t getNormal() const{ return vector3d_t(normal); }
		void setVertexIndices(int a, int b, int c){ pa=a, pb=b, pc=c; }
		void setMaterial(const material_t *m) { material = m; }
		void setNormals(int a, int b, int c){ na=a, nb=b, nc=c; }
		virtual void recNormal();

	private:
		int pa, pb, pc; //!< indices in point array, referenced in mesh.
		int na, nb, nc; //!< indices in normal array, if mesh is smoothed.
		const material_t* material;
		normal_t normal; //!< the geometric normal
        const triangleObject_t* mesh;
		size_t selfIndex;
};

class YAFRAYCORE_EXPORT triangleInstance_t: public triangle_t
{
	friend class scene_t;
	friend class triangleObjectInstance_t;
	
	public:
		triangleInstance_t(): mBase(NULL), mesh(NULL) { }
        triangleInstance_t(triangle_t* base, triangleObjectInstance_t* m): mBase(base), mesh(m) { }
		virtual bool intersect(const ray_t &ray, float *t, intersectData_t &data) const;
		virtual bound_t getBound() const;
		virtual bool intersectsBound(exBound_t &eb) const;
		virtual bool clippingSupport() const { return true; }
		// return: false:=doesn't overlap bound; true:=valid clip exists
		virtual bool clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const;
		virtual const material_t* getMaterial() const { return mBase->getMaterial(); }	
		virtual void getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const;
		virtual float surfaceArea() const;
		virtual void sample(float s1, float s2, point3d_t &p, vector3d_t &n) const;
		
		virtual vector3d_t getNormal() const;
		virtual void recNormal() { /* Empty */ };

	private:
        const triangle_t* mBase;
        const triangleObjectInstance_t* mesh;
};

/*! inherited triangle, so has virtual functions; connected to meshObject_t;
	otherwise identical to triangle_t
*/

class YAFRAYCORE_EXPORT vTriangle_t: public primitive_t
{
	friend class scene_t;
	public:
		vTriangle_t(){};
		vTriangle_t(int ia, int ib, int ic, meshObject_t* m): pa(ia), pb(ib), pc(ic),
					na(-1), nb(-1), nc(-1), mesh(m){ /*recNormal();*/ };
		virtual bool intersect(const ray_t &ray, PFLOAT *t, intersectData_t &data) const;
		virtual bound_t getBound() const;
		virtual bool intersectsBound(exBound_t &eb) const;
		virtual bool clippingSupport() const { return true; }
		// return: false:=doesn't overlap bound; true:=valid clip exists
		virtual bool clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const;
		virtual const material_t* getMaterial() const { return material; }	
		virtual void getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const;
		
		// following are methods which are not part of primitive interface:
		void setMaterial(const material_t *m) { material = m; }
		void setNormals(int a, int b, int c){ na=a, nb=b, nc=c; }
		vector3d_t getNormal(){ return vector3d_t(normal); }
		PFLOAT surfaceArea() const;
		void sample(float s1, float s2, point3d_t &p, vector3d_t &n) const;
		void recNormal();

	protected:
		int pa, pb, pc; //!< indices in point array, referenced in mesh.
		int na, nb, nc; //!< indices in normal array, if mesh is smoothed.
		normal_t normal; //!< the geometric normal
		const material_t* material;
		const meshObject_t* mesh;
};

/*! a triangle supporting time based deformation described by a quadratic bezier spline */
class YAFRAYCORE_EXPORT bsTriangle_t: public primitive_t
{
	friend class scene_t;
	public:
		bsTriangle_t(){};
		bsTriangle_t(int ia, int ib, int ic, meshObject_t* m): pa(ia), pb(ib), pc(ic),
					na(-1), nb(-1), nc(-1), mesh(m){ };
		virtual bool intersect(const ray_t &ray, PFLOAT *t, intersectData_t &data) const;
		virtual bound_t getBound() const;
		//virtual bool intersectsBound(exBound_t &eb) const;
		// return: false:=doesn't overlap bound; true:=valid clip exists
		//virtual bool clipToBound(double bound[2][3], int axis, bound_t &clipped, void *d_old, void *d_new) const;
		virtual const material_t* getMaterial() const { return material; }	
		virtual void getSurface(surfacePoint_t &sp, const point3d_t &hit, intersectData_t &data) const;
		
		// following are methods which are not part of primitive interface:
		void setMaterial(const material_t *m) { material = m; }
		void setNormals(int a, int b, int c){ na=a, nb=b, nc=c; }
		//PFLOAT surfaceArea() const;
		//void sample(float s1, float s2, point3d_t &p, vector3d_t &n) const;

	protected:
		int pa, pb, pc; //!< indices in point array, referenced in mesh.
		int na, nb, nc; //!< indices in normal array, if mesh is smoothed.
		//normal_t normal; //!< the geometric normal
		const material_t* material;
		const meshObject_t* mesh;
};

__END_YAFRAY

#endif // Y_TRIANGLE_H
