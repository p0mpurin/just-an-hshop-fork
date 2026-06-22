#ifndef nblib_nb_hh
#define nblib_nb_hh

#ifdef __3DS__
#include <3ds/types.h>
#else
#include <nblib/types.hh>
#endif

#include <cstring>
#include <cstdio>
#include <vector>

namespace nb
{
	typedef char NbMagic[4];
	typedef u32 BlobPtr;
	typedef BlobPtr ArrayPtr;
	typedef BlobPtr RawArrayPtr;

	template <typename T>
	inline void ldr(T& out, const T &in)
	{
		if ((uintptr_t)&out & (alignof(T) - 1) || (uintptr_t)&in & (alignof(T) - 1))
			memcpy(&out, &in, sizeof(T));
		else
			out = in;
	}

	template <typename T>
	inline T ldr(const T &in)
	{
		T out { 0 };

		if ((uintptr_t)&in & (alignof(T) - 1))
			memcpy(&out, &in, sizeof(T));
		else
			out = in;

		return out;
	}

	static inline size_t align(size_t input, size_t align)
	{
		return (input + align - 1) & ~(align - 1);
	}

	enum class StatusCode : int
	{
		SUCCESS              = 0,
		NO_INPUT_DATA        = 1,
		MAGIC_MISMATCH       = 2,
		INPUT_DATA_TOO_SHORT = 3,
		UNALIGNED            = 4,
	};
}

#endif
