// RUN: %cc %s | intck | diagdiff %s --prefix=exp
// RUN: %cc -D__PATCH__ %s | intck | diagdiff %s
// http://code.google.com/p/chromium/issues/detail?id=117656

#include <stddef.h>

typedef unsigned int u32;
typedef unsigned int T;

static u32 ComputeMaxResults(size_t size_of_buffer) {
#ifndef __PATCH__
	return (size_of_buffer - sizeof(u32)) / sizeof(T); // exp: {{usub}}
#else
	return (size_of_buffer >= sizeof(u32)) ?
		((size_of_buffer - sizeof(u32)) / sizeof(T)) : 0;
#endif
}

size_t ComputeSize(size_t num_results);

void *GetAddressAndCheckSize(u32);

void *HandleGetAttachedShaders(u32 result_size)
{
	u32 max_count = ComputeMaxResults(result_size);
	return GetAddressAndCheckSize(ComputeSize(max_count));
}
