// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp32
// RUN: %linuxcc -m64 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/5591bf07225523600450edd9e6ad258bb877b779

#include "linux.h"

#define MAX_CONTROL_COUNT	1028

struct snd_kcontrol {
	unsigned int count;
};

struct snd_kcontrol_volatile {
	struct snd_ctl_file *owner;
	unsigned int access;
};

struct snd_kcontrol *snd_ctl_new(struct snd_kcontrol *control,
				 unsigned int access)
{
	struct snd_kcontrol *kctl;

#ifdef __PATCH__
	if (control->count > MAX_CONTROL_COUNT)
		return NULL;
#endif
	kctl = kzalloc(sizeof(*kctl) + sizeof(struct snd_kcontrol_volatile) * control->count, GFP_KERNEL); // exp32: {{umul}}
	return kctl;
}
