// RUN: %kcc -nostdinc -m32 -o - %s | %kint --check-prefix=exp
// RUN: %kcc -nostdinc -m64 -o - %s | %kint --check-prefix=exp
// RUN: %kcc -nostdinc -D__PATCH__ -m32 -o - %s | %kint --check-prefix=exp-patch
// RUN: %kcc -nostdinc -D__PATCH__ -m64 -o - %s | %kint --check-prefix=exp-patch

// http://git.kernel.org/linus/fcc6cb0c13555e78c2d47257b6d1b5e59b0c419a

#include "linux.h"

#ifndef __PATCH__
const u8 *cfg80211_find_ie(u8 eid, const u8 *ies, size_t len)
#else
const u8 *cfg80211_find_ie(u8 eid, const u8 *ies, int len)
#endif
{
	while (len > 2 && ies[0] != eid) {
		len -= ies[1] + 2;	// exp: {{usub}}
		ies += ies[1] + 2;
	}
	if (len < 2)
		return NULL;
	if (len < 2 + ies[1])
		return NULL;
	return ies;
}
