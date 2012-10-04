// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp32
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp64
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/6a54435560efdab1a08f429a954df4d6c740bddf

#include "linux.h"

#define KVM_MAX_CPUID_ENTRIES	80

struct kvm_cpuid2 {
	u32 nent;
};

struct kvm_cpuid_entry2 {
	u32 data[10];
};

int kvm_dev_ioctl_get_supported_cpuid(struct kvm_cpuid2 *cpuid,
                                      struct kvm_cpuid_entry2 *entries)
{
	int r;
	struct kvm_cpuid_entry2 *cpuid_entries;

	if (cpuid->nent < 1)
		goto out;
#ifdef __PATCH__
	if (cpuid->nent > KVM_MAX_CPUID_ENTRIES)
		cpuid->nent = KVM_MAX_CPUID_ENTRIES;
#endif
	r = -ENOMEM;
	cpuid_entries = vmalloc(sizeof(struct kvm_cpuid_entry2) * cpuid->nent); // exp32: {{umul}}
	if (!cpuid_entries)
		goto out;
	r = 0;
	vfree(cpuid_entries);
out:
	return r;
}
