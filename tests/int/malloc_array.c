// RUN: %kcc -m32 -o - %s | %intsat --check-prefix=exp
// RUN: %kcc -m64 -o - %s | %intsat --check-prefix=exp

#include <stdlib.h>
#include <stdint.h>

void *malloc_array_nc(size_t n, size_t size)
{
	return malloc(n * size); // exp: {{umul}}
}

void *malloc_array(size_t n, size_t size)
{
	if (size && n > SIZE_MAX / size)
		return NULL;
	return malloc(n * size);
}
