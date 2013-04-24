#ifndef Y_SCRHALTON_H
#define Y_SCRHALTON_H

#include <yafray_config.h>
#include <core_api/vector3d.h>

extern YAFRAYCORE_EXPORT const int* faure[];

__BEGIN_YAFRAY

const int prims[50] = {   1,   2,   3,   5,   7,  11,  13,  17,  19,  23,
					     29,  31,  37,  41,  43,  47,  53,  59,  61,  67,
					     71,  73,  79,  83,  89,  97, 101, 103, 107, 109,
					    113, 127, 131, 137, 139, 149, 151, 157, 163, 167,
					    173, 179, 181, 191, 193, 197, 199, 211, 223, 227  };

const double invPrims[50] = { 1.000000000,0.500000000,0.333333333,0.200000000,0.142857143,
							  0.090909091,0.076923077,0.058823529,0.052631579,0.043478261,
							  0.034482759,0.032258065,0.027027027,0.024390244,0.023255814,
							  0.021276596,0.018867925,0.016949153,0.016393443,0.014925373,
							  0.014084507,0.013698630,0.012658228,0.012048193,0.011235955,
							  0.010309278,0.009900990,0.009708738,0.009345794,0.009174312,
							  0.008849558,0.007874016,0.007633588,0.007299270,0.007194245,
							  0.006711409,0.006622517,0.006369427,0.006134969,0.005988024,
							  0.005780347,0.005586592,0.005524862,0.005235602,0.005181347,
							  0.005076142,0.005025126,0.004739336,0.004484305,0.004405286 };

/** Low Discrepancy Halton sampling */
// dim MUST NOT be larger than 50! Above that, random numbers may be
// the better choice anyway, not even scrambling is realiable at high dimensions.
inline double scrHalton(int dim, unsigned int n)
{
	double value = 0.0;
	if(dim < 50)
	{
		const int *sigma = faure[dim];
		unsigned int base = prims[dim];
		double f, factor, dn = (double)n;

		f = factor = invPrims[dim];
		while (n>0) {
			value += double(sigma[n % base]) * factor;
			dn *= f;
			n = (unsigned int)dn;
			factor *= f;
		}
	}
	else
	{
		value = (double)ourRandom();
	}
	return std::max(0.0, std::min(1.0, value));
}

__END_YAFRAY

#endif // Y_SCRHALTON_H
