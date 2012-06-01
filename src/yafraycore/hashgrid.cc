#include <yafraycore/hashgrid.h>

__BEGIN_YAFRAY

hashGrid_t::hashGrid_t(double _cellSize, unsigned int _gridSize, yafaray::bound_t _bBox)
:cellSize(_cellSize), gridSize(_gridSize), bBox(_bBox)
{
	invcellSize = 1. / cellSize;
}

void hashGrid_t::setParm(double _cellSize, unsigned int _gridSize, bound_t _bBox)
{
	cellSize = _cellSize;
	invcellSize = 1. / cellSize;
	gridSize = _gridSize;
	bBox = _bBox;
}

void hashGrid_t::clear()
{
	photons.clear();
}

void hashGrid_t::pushPhoton(photon_t &p)
{
	photons.push_back(p);
}


void hashGrid_t::updateGrid()
{

	if (!hashGrid)
	{
		hashGrid = new std::list<photon_t *>*[gridSize];

		for (unsigned int i = 0; i < gridSize; ++i)
			hashGrid[i] = NULL;
	} 
	else 
	{
		for (unsigned int i = 0; i < gridSize; ++i) 
		{	
			if( hashGrid[i])
			{
				//delete hashGrid[i];
				hashGrid[i]->clear(); // fix me! too many time consumed here
				//hashGrid[i] = NULL;
			}
		}
	}

	//travel the vector to build the Grid
	std::vector<photon_t>::iterator itr;
	for(itr = photons.begin(); itr != photons.end(); ++itr)
	{
		point3d_t hashindex  =  ( (*itr).pos - bBox.a) * invcellSize;

		int ix = abs(int(hashindex.x));
		int iy = abs(int(hashindex.y));
		int iz = abs(int(hashindex.z));

		unsigned int index = Hash(ix,iy,iz);

		if(hashGrid[index] == NULL)
			hashGrid[index] = new std::list<photon_t*>();

		hashGrid[index]->push_front(&(*itr));
	}
	unsigned int notused = 0;
	for (unsigned int i = 0; i < gridSize; ++i)
	{
		if(!hashGrid[i] || hashGrid[i]->size() == 0) notused++;
	}
	Y_INFO<<"HashGrid: there are " << notused << " enties not used!"<<std::endl;
}

unsigned int hashGrid_t::gather(const point3d_t &P, foundPhoton_t *found, unsigned int K, PFLOAT sqRadius)
{
	unsigned int count = 0;
	PFLOAT radius = sqrt(sqRadius);

	point3d_t rad(radius, radius, radius);
	point3d_t bMin = ((P - rad) - bBox.a) * invcellSize;
	point3d_t bMax = ((P + rad) - bBox.a) * invcellSize;

	for (int iz = abs(int(bMin.z)); iz <= abs(int(bMax.z)); iz++) {
		for (int iy = abs(int(bMin.y)); iy <= abs(int(bMax.y)); iy++) {
			for (int ix = abs(int(bMin.x)); ix <= abs(int(bMax.x)); ix++) {
				int hv = Hash(ix, iy, iz);

				if(hashGrid[hv] == NULL) continue;

				std::list<photon_t*>::iterator itr;
				for(itr = hashGrid[hv]->begin(); itr != hashGrid[hv]->end(); ++itr)
				{
					if( ( (*itr)->pos- P).lengthSqr() < sqRadius)
					{
						found[count++] = foundPhoton_t((*itr),sqRadius);
					}
				}
			}
		}
	}
	return count;
}

__END_YAFRAY