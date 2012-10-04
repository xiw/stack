// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/b522f02184b413955f3bc952e3776ce41edc6355

#include "linux.h"

#ifdef __LP64__
#define SIZE_OF_AGP_MEMORY	104
#else
#define SIZE_OF_AGP_MEMORY	60
#endif

struct agp_memory {
	char ph[SIZE_OF_AGP_MEMORY];
};

void agp_alloc_page_array(size_t size, struct agp_memory *mem);

struct agp_memory *agp_create_user_memory(unsigned long num_agp_pages)
{
	struct agp_memory *new;
	unsigned long alloc_size = num_agp_pages*sizeof(struct page *); // exp: {{umul}}

#ifdef __PATCH__
	if (INT_MAX/sizeof(struct page *) < num_agp_pages)
		return NULL;
#endif
	new = kzalloc(sizeof(struct agp_memory), GFP_KERNEL);
	if (new == NULL)
		return NULL;

	agp_alloc_page_array(alloc_size, new);

	return new;
}
