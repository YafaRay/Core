
#ifndef Y_PHOTONMAP_H
#define Y_PHOTONMAP_H

#include <yafray_config.h>

#include "pkdtree.h"
#include <core_api/color.h>

__BEGIN_YAFRAY
#define c255Ratio 81.16902097686662123083
#define c256Ratio 40.74366543152520595687


#define cInv255Ratio 0.01231997119054820878
#define cInv256Ratio 0.02454369260617025968


class dirConverter_t
{
	public:
		dirConverter_t();

		vector3d_t convert(unsigned char theta,unsigned char phi)
		{
			return vector3d_t(sintheta[theta]*cosphi[phi],
								sintheta[theta]*sinphi[phi],
								costheta[theta]);
		}
		std::pair<unsigned char,unsigned char> convert(const vector3d_t &dir)
		{
			int t=(int)(fAcos(dir.z)*c255Ratio);
			int p=(int)(atan2(dir.y,dir.x)*c256Ratio);
			if(t>254) t=254;
			else if(t<0) t=0;
			if(p>255) p=255;
			else if(p<0) p+=256;
			return std::pair<unsigned char,unsigned char>(t,p);
		}
		
	protected:
		PFLOAT cosphi[256];
		PFLOAT sinphi[256];
		PFLOAT costheta[255];
		PFLOAT sintheta[255];
};

extern YAFRAYCORE_EXPORT dirConverter_t dirconverter;

class photon_t
{
	public:
		photon_t() {/*theta=255;*/};
		photon_t(const vector3d_t &d,const point3d_t &p, const color_t &col)
		{
#ifdef _SMALL_PHOTONS
			direction(d);
#else
			dir=d;
#endif
			pos=p;
			c=col;
		};
//		photon_t(const runningPhoton_t &p)
//		{
//			pos=p.pos;
//			c=p.c;
//			vector3d_t dir=p.lastpos-p.pos;
//			dir.normalize();
//			direction(dir);
//		};
		const point3d_t & position()const {return pos;};
		const color_t color()const {return c;};
		void color(const color_t &col) {c=col;};
		vector3d_t direction()const 
		{
#ifdef _SMALL_PHOTONS
			if(theta==255) return vector3d_t(0,0,0);
			else return dirconverter.convert(theta,phi);
#else
			return (vector3d_t)dir;
#endif
		};
		void direction(const vector3d_t &d)
		{
#ifdef _SMALL_PHOTONS
			if(dir.null()) theta=255;
			else
			{
				std::pair<unsigned char,unsigned char> cd=dirconverter.convert(d);
				theta=cd.first;
				phi=cd.second;
			}
#else
			dir=d;
#endif
		}
	
		point3d_t pos;
#ifdef _SMALL_PHOTONS
		rgbe_t c;
		unsigned char theta,phi;
#else
		color_t c;
		normal_t dir;
#endif
};

struct radData_t
{
	radData_t(point3d_t &p, vector3d_t n): pos(p), normal(n), use(true) {}
	point3d_t pos;
	vector3d_t normal;
	color_t refl;
	color_t transm;
	mutable bool use;
};

struct foundPhoton_t
{
	foundPhoton_t(){};
	foundPhoton_t(const photon_t *p, PFLOAT d): photon(p), distSquare(d){}
	bool operator<(const foundPhoton_t &p2) const { return distSquare < p2.distSquare; }
	const photon_t *photon;
	PFLOAT distSquare;
	//temp!!
	PFLOAT dis;
};

class YAFRAYCORE_EXPORT photonMap_t
{
	public:
		photonMap_t(): paths(0), updated(false), searchRadius(1.), tree(0){ }
		~photonMap_t(){ if(tree) delete tree; }
		void setNumPaths(int n){ paths=n; }
		int nPaths() const{ return paths; }
		int nPhotons() const{ return photons.size(); }
		void pushPhoton(photon_t &p) { photons.push_back(p); updated=false; }
		void swapVector(std::vector<photon_t> &vec) { photons.swap(vec); updated=false; }
		void updateTree();
		void clear(){ photons.clear(); delete tree; tree=0; updated=false; }
		bool ready() const { return updated; }
	//	void gather(const point3d_t &P, std::vector< foundPhoton_t > &found, unsigned int K, PFLOAT &sqRadius) const;
		int gather(const point3d_t &P, foundPhoton_t *found, unsigned int K, PFLOAT &sqRadius) const;
		const photon_t* findNearest(const point3d_t &P, const vector3d_t &n, PFLOAT dist) const;
	protected:
		std::vector<photon_t> photons;
		int paths; //!< amount of photon paths that have been traced for generating the map
		bool updated;
		PFLOAT searchRadius;
		kdtree::pointKdTree<photon_t> *tree;
};

// photon "processes" for lookup

struct photonGather_t
{
	photonGather_t(u_int32 mp, const point3d_t &p);
	void operator()(const photon_t *photon, PFLOAT dist2, PFLOAT &maxDistSquared) const;
	const point3d_t &p;
	foundPhoton_t *photons;
	u_int32 nLookup;
	mutable u_int32 foundPhotons;
};

struct nearestPhoton_t
{
	nearestPhoton_t(const point3d_t &pos, const vector3d_t &norm): p(pos), n(norm), nearest(0) {}
	void operator()(const photon_t *photon, PFLOAT dist2, PFLOAT &maxDistSquared) const
	{
		if ( photon->direction() * n > 0.f) { nearest = photon; maxDistSquared = dist2; }
	}
	const point3d_t p; //wth do i need this for actually??
	const vector3d_t n;
	mutable const photon_t *nearest;
};

/*! "eliminates" photons within lookup radius (sets use=false) */
struct eliminatePhoton_t
{
	eliminatePhoton_t(const vector3d_t &norm): n(norm) {}
	void operator()(const radData_t *rpoint, PFLOAT dist2, PFLOAT &maxDistSquared) const
	{
		if ( rpoint->normal * n > 0.f) { rpoint->use = false; }
	}
	const vector3d_t n;
};


__END_YAFRAY

#endif // Y_PHOTONMAP_H
