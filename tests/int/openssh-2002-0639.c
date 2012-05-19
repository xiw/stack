// RUN: %kcc -m32 -o - %s | %intsat --check-prefix=exp
// RUN: %kcc -m32 -D__PATCH__ -o - %s | %intsat --check-prefix=exp-patch
// CVE-2002-0639: http://www.openssh.com/txt/preauth.adv

#include <sys/types.h>
#include <stddef.h>

void     fatal(const char *, ...) __attribute__((format(printf, 1, 2))) __attribute__((noreturn));
u_int    packet_get_int(void);
void    *packet_get_string(u_int *length_ptr);
void    *xmalloc(size_t);

char **input_userauth_info_response()
{
	int i;
	u_int nresp;
	char **response = NULL;
	nresp = packet_get_int();
#ifdef __PATCH__
	if (nresp > 100)
		fatal("input_userauth_info_response: nresp too big %u", nresp);
#endif
	if (nresp > 0) {
		response = xmalloc(nresp * sizeof(char*)); // exp: {{umul}}
		for (i = 0; i < nresp; i++)
			response[i] = packet_get_string(NULL);
	}
	return response;
}
