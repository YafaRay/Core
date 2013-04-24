/*
Sutherland-Hodgman triangle clipping
*/
#include<yafray_config.h>

#include <core_api/bound.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>

__BEGIN_YAFRAY

template <class T>
void _swap(T **a, T **b)
{
	T *x;
	x=*a;
	*a=*b;
	*b=x;
}

class DVector
{
	public:
		DVector() {}
		DVector& operator = (const DVector &v) { x=v.x, y=v.y, z=v.z; return *this; }
		double   operator[] (int i) const { return (&x)[i]; }
		double  &operator[] (int i) { return (&x)[i]; }
		~DVector(){}
		double x,y,z;
};

struct clipDump
{
	int nverts;
	DVector poly[10];
};

#define Y_VCPY(a,b)       ( (a)[0] =(b)[0], (a)[1] =(b)[1], (a)[2] =(b)[2] )

/*! function to clip a triangle against an axis aligned bounding box and return new bound
	\param box the AABB of the clipped triangle
	\return 0: triangle was clipped successfully
			1: triangle didn't overlap the bound at all => disappeared
			2: fatal error occured (didn't ever happen to me :)
			3: resulting polygon degenerated to less than 3 edges (never happened either)
*/

int triBoxClip(const double b_min[3], const double b_max[3], const double triverts[3][3], bound_t &box, void* n_dat)
{
	DVector dump1[11], dump2[11]; double t;
	DVector *poly = dump1, *cpoly = dump2;

	for(int q=0;q<3;q++)
	{
		poly[q][0]=triverts[q][0], poly[q][1]=triverts[q][1], poly[q][2]=triverts[q][2];
		poly[3][q]=triverts[0][q];
	}

	int n=3, nc;
	bool p1_inside;

	//for each axis
	for(int axis=0;axis<3;axis++)
	{
		DVector *p1, *p2;
		int nextAxis = (axis+1)%3, prevAxis = (axis+2)%3;

		// === clip lower ===
		nc=0;
		p1_inside = (poly[0][axis] >= b_min[axis]) ? true : false;
		for(int i=0; i<n; i++) // for each poly edge
		{
			p1=&poly[i], p2=&poly[i+1];
			if( p1_inside ) // p1 inside
			{
				if((*p2)[axis] >= b_min[axis]) //both "inside"
				{
					// copy p2 to new poly
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else
				{
					// clip line, add intersection to new poly
					t = (b_min[axis] - (*p1)[axis]) / ((*p2)[axis]-(*p1)[axis]);
					cpoly[nc][axis] = b_min[axis];
					cpoly[nc][nextAxis] = (*p1)[nextAxis] + t * ((*p2)[nextAxis]-(*p1)[nextAxis]);
					cpoly[nc][prevAxis] = (*p1)[prevAxis] + t * ((*p2)[prevAxis]-(*p1)[prevAxis]);
					nc++;
					p1_inside = false;
				}
			}
			else //p1 < b_min -> outside
			{
				if((*p2)[axis] > b_min[axis]) //p2 inside, add s and p2
				{
					t = (b_min[axis] - (*p2)[axis]) / ((*p1)[axis]-(*p2)[axis]);
					cpoly[nc][axis] = b_min[axis];
					cpoly[nc][nextAxis] = (*p2)[nextAxis] + t * ((*p1)[nextAxis]-(*p2)[nextAxis]);
					cpoly[nc][prevAxis] = (*p2)[prevAxis] + t * ((*p1)[prevAxis]-(*p2)[prevAxis]);
					nc++;
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else if((*p2)[axis] == b_min[axis]) //p2 and s are identical, only add p2
				{
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else p1_inside = false;
			}
			//else: both outse, do nothing.
		} //for all edges

		if(nc>9)
		{
			Y_INFO << "TriangleClip: after min n is now " << nc << ", that's bad!" << yendl;
			return 2;
		}

		cpoly[nc] = cpoly[0];
		n = nc;
		_swap(&cpoly, &poly);


		// === clip upper ===
		nc=0;
		p1_inside = (poly[0][axis] <= b_max[axis]) ? true : false;
		for(int i=0; i<n; i++) // for each poly edge
		{
			p1=&poly[i], p2=&poly[i+1];
			if( p1_inside )
			{
				if((*p2)[axis] <= b_max[axis]) //both "inside"
				{
					// copy p2 to new poly
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else
				{
					// clip line, add intersection to new poly
					t = (b_max[axis] - (*p1)[axis]) / ((*p2)[axis]-(*p1)[axis]);
					cpoly[nc][axis] = b_max[axis];
					cpoly[nc][nextAxis] = (*p1)[nextAxis] + t * ((*p2)[nextAxis]-(*p1)[nextAxis]);
					cpoly[nc][prevAxis] = (*p1)[prevAxis] + t * ((*p2)[prevAxis]-(*p1)[prevAxis]);
					nc++;
					p1_inside = false;
				}
			}
			else //p1 > b_max -> outside
			{
				if((*p2)[axis] < b_max[axis]) //p2 inside, add s and p2
				{
					t = (b_max[axis] - (*p2)[axis]) / ((*p1)[axis]-(*p2)[axis]);
					cpoly[nc][axis] = b_max[axis];
					cpoly[nc][nextAxis] = (*p2)[nextAxis] + t * ((*p1)[nextAxis]-(*p2)[nextAxis]);
					cpoly[nc][prevAxis] = (*p2)[prevAxis] + t * ((*p1)[prevAxis]-(*p2)[prevAxis]);
					nc++;
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else if((*p2)[axis] == b_max[axis]) //p2 and s are identical, only add p2
				{
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else p1_inside = false;
			}
			//else: both outse, do nothing.
		} //for all edges

		if(nc > 9)
		{
			Y_INFO << "TriangleClip: After max n is now " << nc << ", that's bad!" << yendl;
			return 2;
		}

		if(nc == 0) return 1;

		cpoly[nc] = cpoly[0];
		n = nc;
		_swap(&cpoly, &poly);
	} //for all axis

	if(n < 2)
	{
		static bool foobar=false;
		if(foobar) return 3;
		Y_INFO << "TriangleClip: Clip degenerated! n="<<n<<yendl;
		Y_INFO << "TriangleClip: b_min:\t" << b_min[0] << ",\t" << b_min[1] << ",\t" << b_min[2] << yendl;
		Y_INFO << "TriangleClip: b_max:\t" << b_max[0] << ",\t" << b_max[1] << ",\t" << b_max[2] << yendl;
		Y_INFO << "TriangleClip: delta:\t" << b_max[0]-b_min[0] << ",\t" << b_max[1]-b_min[1] << ",\t" << b_max[2]-b_min[2] << yendl;

		for(int j=0;j<3;j++)
		{
			Y_INFO << "TriangleClip: point" << j << ": " << triverts[j][0] << ",\t" << triverts[j][1] << ",\t" << triverts[j][2] << yendl;
		}
		foobar=true;
		return 3;
	}

	double a[3], g[3];
	Y_VCPY(a, poly[0]);
	Y_VCPY(g, poly[0]);

	for(int i=1; i<n; i++)
	{
		a[0] = std::min(a[0], poly[i][0]);
		a[1] = std::min(a[1], poly[i][1]);
		a[2] = std::min(a[2], poly[i][2]);
		g[0] = std::max(g[0], poly[i][0]);
		g[1] = std::max(g[1], poly[i][1]);
		g[2] = std::max(g[2], poly[i][2]);
	}

	box.a[0] = a[0], box.g[0] = g[0];
	box.a[1] = a[1], box.g[1] = g[1];
	box.a[2] = a[2], box.g[2] = g[2];

	clipDump *output = (clipDump*)n_dat;
	output->nverts = n;
	memcpy(output->poly, poly, (n+1)*sizeof(DVector));

	return 0;
}

int triPlaneClip(double pos, int axis, bool lower, bound_t &box, void* o_dat, void* n_dat)
{
	clipDump *input = (clipDump*)o_dat;
	clipDump *output = (clipDump*)n_dat;
	double t;
	DVector *poly = input->poly, *cpoly = output->poly;
	int n=input->nverts, nc;

	bool p1_inside;
	DVector *p1, *p2;
	int nextAxis = (axis+1)%3, prevAxis = (axis+2)%3;

	if(lower)
	{
		// === clip lower ===
		nc=0;
		p1_inside = (poly[0][axis] >= pos) ? true : false;
		for(int i=0; i<n; i++) // for each poly edge
		{
			p1=&poly[i], p2=&poly[i+1];
			if( p1_inside ) // p1 inside
			{
				if((*p2)[axis] >= pos) //both "inside"
				{
					// copy p2 to new poly
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else
				{
					// clip line, add intersection to new poly
					t = (pos - (*p1)[axis]) / ((*p2)[axis]-(*p1)[axis]);
					cpoly[nc][axis] = pos;
					cpoly[nc][nextAxis] = (*p1)[nextAxis] + t * ((*p2)[nextAxis]-(*p1)[nextAxis]);
					cpoly[nc][prevAxis] = (*p1)[prevAxis] + t * ((*p2)[prevAxis]-(*p1)[prevAxis]);
					nc++;
					p1_inside = false;
				}
			}
			else //p1 < b_min -> outside
			{
				if((*p2)[axis] > pos) //p2 inside, add s and p2
				{
					t = (pos - (*p2)[axis]) / ((*p1)[axis]-(*p2)[axis]);
					cpoly[nc][axis] = pos;
					cpoly[nc][nextAxis] = (*p2)[nextAxis] + t * ((*p1)[nextAxis]-(*p2)[nextAxis]);
					cpoly[nc][prevAxis] = (*p2)[prevAxis] + t * ((*p1)[prevAxis]-(*p2)[prevAxis]);
					nc++;
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else if((*p2)[axis] == pos) //p2 and s are identical, only add p2
				{
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else p1_inside = false;
			}
			//else: both outse, do nothing.
		} //for all edges

		if(nc == 0) return 1;

		if(nc > 9)
		{
			Y_INFO << "TriangleClip: After min n is now " << nc << ", that's bad!" << yendl;
			return 2;
		}

		cpoly[nc] = cpoly[0];
		n = nc;
		_swap(&cpoly, &poly);
	}
	else
	{
		// === clip upper ===
		nc=0;
		p1_inside = (poly[0][axis] <= pos) ? true : false;
		for(int i=0; i<n; i++) // for each poly edge
		{
			p1=&poly[i], p2=&poly[i+1];
			if( p1_inside )
			{
				if((*p2)[axis] <= pos) //both "inside"
				{
					// copy p2 to new poly
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else
				{
					// clip line, add intersection to new poly
					t = (pos - (*p1)[axis]) / ((*p2)[axis]-(*p1)[axis]);
					cpoly[nc][axis] = pos;
					cpoly[nc][nextAxis] = (*p1)[nextAxis] + t * ((*p2)[nextAxis]-(*p1)[nextAxis]);
					cpoly[nc][prevAxis] = (*p1)[prevAxis] + t * ((*p2)[prevAxis]-(*p1)[prevAxis]);
					nc++;
					p1_inside = false;
				}
			}
			else //p1 > pos -> outside
			{
				if((*p2)[axis] < pos) //p2 inside, add s and p2
				{
					t = (pos - (*p2)[axis]) / ((*p1)[axis]-(*p2)[axis]);
					cpoly[nc][axis] = pos;
					cpoly[nc][nextAxis] = (*p2)[nextAxis] + t * ((*p1)[nextAxis]-(*p2)[nextAxis]);
					cpoly[nc][prevAxis] = (*p2)[prevAxis] + t * ((*p1)[prevAxis]-(*p2)[prevAxis]);
					nc++;
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else if((*p2)[axis] == pos) //p2 and s are identical, only add p2
				{
					cpoly[nc] = *p2;
					nc++;
					p1_inside = true;
				}
				else p1_inside = false;
			}
			//else: both outse, do nothing.
		} //for all edges

		if(nc == 0) return 1;

		if(nc > 9)
		{
			Y_INFO << "TriangleClip: after max n is now " << nc << ", that's bad!" << yendl;
			return 2;
		}

		cpoly[nc] = cpoly[0];
		n = nc;
		_swap(&cpoly, &poly);
	} //for all axis

	if(n < 2)
	{
		static bool foobar=false;
		if(foobar) return 3;
		Y_INFO << "TriangleClip: Clip degenerated! n=" << n << yendl;
		foobar=true;
		return 3;
	}

	double a[3], g[3];
	Y_VCPY(a, poly[0]);
	Y_VCPY(g, poly[0]);

	for(int i=1; i<n; i++)
	{
		a[0] = std::min(a[0], poly[i][0]);
		a[1] = std::min(a[1], poly[i][1]);
		a[2] = std::min(a[2], poly[i][2]);
		g[0] = std::max(g[0], poly[i][0]);
		g[1] = std::max(g[1], poly[i][1]);
		g[2] = std::max(g[2], poly[i][2]);
	}

	box.a[0] = a[0], box.g[0] = g[0];
	box.a[1] = a[1], box.g[1] = g[1];
	box.a[2] = a[2], box.g[2] = g[2];

	output->nverts = n;

	return 0;
}

__END_YAFRAY

