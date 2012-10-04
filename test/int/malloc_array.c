// RUN: %cc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %cc -m64 %s | intck | diagdiff %s --prefix=exp

#include <stdlib.h>
#include <stdint.h>

void *malloc_array_nc(size_t n, size_t size)
{
	return malloc(n * size); // exp: {{umul}}
}

void *malloc_array_0(size_t n, size_t size)
{
	// need -overflow to recognize the idiom.
	if (size && n > SIZE_MAX / size)
		return NULL;
	return malloc(n * size);
}

void *malloc_array_1(size_t n, size_t size)
{
	// swap n and size; need -overflow-simplify to merge the two
	// overflow builtins (OOB and path predicates).
	if (n && size > SIZE_MAX / n)
		return NULL;
	return malloc(n * size);
}

void *malloc_array_2(size_t n, size_t size)
{
	// need -overflow to recognize the idiom.
	// need to sink the multiplication after the check.
	size_t bytes = n * size;
	if (size && n != bytes / size)
		return NULL;
	return malloc(bytes);
}
