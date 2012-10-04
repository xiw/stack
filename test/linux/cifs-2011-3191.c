// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/9438fabb73eb48055b58b89fc51e0bc4db22fabd

#include "linux.h"

#define SMB_COM_TRANSACTION2	0x32

typedef struct smb_com_transaction2_fnext_req {
	char ResumeFileName[1];
} __attribute__((packed)) TRANSACTION2_FNEXT_REQ;

struct cifs_tcon;

struct cifs_search_info {
	const char *presume_name;
	unsigned int resume_name_len;
};

int smb_init(int smb_command, int wct, struct cifs_tcon *tcon,
	     void **request_buf, void **response_buf);

void cifs_buf_release(void *);

int CIFSFindNext(const int xid, struct cifs_tcon *tcon,
		 u16 searchHandle, struct cifs_search_info *psrch_inf)
{
	TRANSACTION2_FNEXT_REQ *pSMB = NULL;
	int rc;
#ifndef __PATCH__
	int name_len;
#else
	unsigned int name_len;
#endif
	u16 params, byte_count;

	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB, NULL);
	if (rc)
		return rc;

	name_len = psrch_inf->resume_name_len;
	params += name_len;
	if (name_len < PATH_MAX) {
		memcpy(pSMB->ResumeFileName, psrch_inf->presume_name, name_len); // exp: {{size}}
		byte_count += name_len;
		/* 14 byte parm len above enough for 2 byte null terminator */
		pSMB->ResumeFileName[name_len] = 0;
		pSMB->ResumeFileName[name_len+1] = 0;	// exp: {{array}}
	} else {
		rc = -EINVAL;
		goto exit;
	}
exit:
	if (rc != 0)
		cifs_buf_release(pSMB);
	return rc;
}
