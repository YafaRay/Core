#ifndef Y_ALLOC_H
#define Y_ALLOC_H

#include <yafray_config.h>

#include <stdlib.h>
#include <vector>
#include <algorithm>
#if !defined(WIN32) && !defined(__APPLE__)
	#include <alloca.h>
#elif defined (__MINGW32__) //Added by DarkTide to enable mingw32 compliation
	#include <malloc.h>
	// povmaniaco: add fix for use MinGW Gcc 4.7.1
	#define _aligned_malloc __mingw_aligned_malloc
	#define _aligned_free __mingw_aligned_free
	// end
#endif

__BEGIN_YAFRAY

typedef unsigned int u_int32;

#if defined(_WIN32) && !defined(__MINGW32__) //Added by DarkTide to enable mingw32 compliation
#define alloca _alloca
#endif

inline void * y_memalign(size_t bound, size_t size)
{
#if (defined (_WIN32) && (defined (_MSC_VER)) || defined (__MINGW32__)) //Added by DarkTide to enable mingw32 compliation
	return _aligned_malloc(size, bound);
#elif defined(__APPLE__)
	// there's no good aligned allocation on OS X it seems :/
	// however, malloc is supposed to return at least SSE2 compatible alignment, which has to be enough.
	// alternative would be valloc, but i have no good info on its effects.
	return malloc(size);
//#elif defined(__FreeBSD__)
#else
	// someone check if this maybe is available on OS X meanwhile...thx :)
	void *ret;
	if (posix_memalign(&ret, bound, size) != 0)
		return (NULL);
	else
		return (ret);
//#else
//	return memalign(bound, size);
#endif
}

inline void y_free(void *ptr) {
#if (defined (_WIN32) && (defined (_MSC_VER)) || defined (__MINGW32__)) //Added by DarkTide to enable mingw32 compliation
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}


template <class T> class ObjectArena {
public:
	// ObjectArena Public Methods
	ObjectArena() {
		nAvailable = 0;
	}
	T *Alloc() {
		if (nAvailable == 0) {
			int nAlloc = std::max((unsigned long)16,
				(unsigned long)(65536/sizeof(T)));
			mem = (T *)y_memalign(64, nAlloc * sizeof(T));
			nAvailable = nAlloc;
			toDelete.push_back(mem);
		}
		--nAvailable;
		return mem++;
	}
	operator T *() {
		return Alloc();
	}
	~ObjectArena() { FreeAll(); }
	void FreeAll() {
		for (unsigned int i = 0; i < toDelete.size(); ++i)
			y_free(toDelete[i]);
		toDelete.erase(toDelete.begin(), toDelete.end());
		nAvailable = 0;
	}
private:
	// ObjectArena Private Data
	T *mem;
	int nAvailable;
	std::vector<T *> toDelete;
};

class MemoryArena {
public:
	// MemoryArena Public Methods
	MemoryArena(u_int32 bs = 32768) {
		blockSize = bs;
		curBlockPos = 0;
		currentBlock = (char *)y_memalign(64, blockSize);
	}
	~MemoryArena() {
		y_free(currentBlock);
		for (u_int32 i = 0; i < usedBlocks.size(); ++i)
			y_free(usedBlocks[i]);
		for (u_int32 i = 0; i < availableBlocks.size(); ++i)
			y_free(availableBlocks[i]);
	}
	void *Alloc(u_int32 sz) {
		// Round up _sz_ to minimum machine alignment
		sz = ((sz + 7) & (~7));
		if (curBlockPos + sz > blockSize) {
			// Get new block of memory for _MemoryArena_
			usedBlocks.push_back(currentBlock);
			if (availableBlocks.size() && sz <= blockSize) {
				currentBlock = availableBlocks.back();
				availableBlocks.pop_back();
			}
			else
				currentBlock = (char *)y_memalign(64, (std::max(sz, blockSize)) );
			curBlockPos = 0;
		}
		void *ret = currentBlock + curBlockPos;
		curBlockPos += sz;
		return ret;
	}
	void FreeAll() {
		curBlockPos = 0;
		while (usedBlocks.size()) {
			availableBlocks.push_back(usedBlocks.back());
			usedBlocks.pop_back();
		}
	}
private:
	// MemoryArena Private Data
	u_int32 curBlockPos, blockSize;
	char *currentBlock;
	std::vector<char *> usedBlocks, availableBlocks;
};

__END_YAFRAY
#endif // Y_ALLOC_H
