// RUN: %kcc -m32 -o - %s | %intsat --check-prefix=exp
// RUN: %kcc -m64 -o - %s | %intsat --check-prefix=exp

#include <stdlib.h>
#include <stdint.h>

void *malloc_array_nc(size_t n, size_t size)
{
	return malloc(n * size); // exp: {{umul}}
}

void *malloc_array_0(size_t n, size_t size)
{
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
