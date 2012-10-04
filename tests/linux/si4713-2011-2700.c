// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/dc6b845044ccb7e9e6f3b7e71bd179b3cf0223b6

#include "linux.h"

#define V4L2_CTRL_CLASS_FM_TX		0x009b0000
#define V4L2_CID_FM_TX_CLASS_BASE	(V4L2_CTRL_CLASS_FM_TX | 0x900)
#define V4L2_CID_RDS_TX_PS_NAME		(V4L2_CID_FM_TX_CLASS_BASE + 5)
#define V4L2_CID_RDS_TX_RADIO_TEXT	(V4L2_CID_FM_TX_CLASS_BASE + 6)

#define MAX_RDS_PS_NAME			96
#define MAX_RDS_RADIO_TEXT		384

struct si4713_device;

struct v4l2_ext_control {
	u32 id;
	u32 size;
	u32 reserved2[1];
	union {
		s32 value;
		s64 value64;
		char *string;
	};
} __attribute__ ((packed));

int si4713_set_rds_ps_name(struct si4713_device *sdev, char *ps_name);

int si4713_set_rds_radio_text(struct si4713_device *sdev, char *rt);

int si4713_write_econtrol_string(struct si4713_device *sdev,
				 struct v4l2_ext_control *control)
{
	int len;
	s32 rval = 0;

	switch (control->id) {
	case V4L2_CID_RDS_TX_PS_NAME: {
		char ps_name[MAX_RDS_PS_NAME + 1];

		len = control->size - 1; // exp: {{usub}}
#ifndef __PATCH__
		if (len > MAX_RDS_PS_NAME) {
#else
		if (len < 0 || len > MAX_RDS_PS_NAME) {
#endif
			rval = -ERANGE;
			goto exit;
		}
		rval = copy_from_user(ps_name, control->string, len); // exp: {{size}}
		if (rval) {
			rval = -EFAULT;
			goto exit;
		}
		ps_name[len] = '\0'; // exp: {{array}}

		rval = si4713_set_rds_ps_name(sdev, ps_name);
	}
		break;

	case V4L2_CID_RDS_TX_RADIO_TEXT: {
		char radio_text[MAX_RDS_RADIO_TEXT + 1];

		len = control->size - 1; // exp:: {{usub}}
#ifndef __PATCH__
		if (len > MAX_RDS_RADIO_TEXT) {
#else
		if (len < 0 || len > MAX_RDS_RADIO_TEXT) {
#endif
			rval = -ERANGE;
			goto exit;
		}
		rval = copy_from_user(radio_text, control->string, len); // exp: {{size}}
		if (rval) {
			rval = -EFAULT;
			goto exit;
		}
		radio_text[len] = '\0'; // exp:: {{array}}

		rval = si4713_set_rds_radio_text(sdev, radio_text);
	}
		break;

	default:
		rval = -EINVAL;
		break;
	}

exit:
	return rval;
}
