// RUN: %cc %s | intck | diagdiff %s --prefix=exp
// RUN: %cc -D__PATCH__ %s | intck | diagdiff %s
//
// http://www.kb.cert.org/vuls/id/192995

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

bool_t
xdr_array(XDR *xdrs,
	caddr_t *addrp,		/* array pointer */
	u_int *sizep,		/* number of elements */
	u_int maxsize,		/* max numberof elements */
	u_int elsize,		/* size in bytes of each element */
	xdrproc_t elproc)	/* xdr routine to handle each element */
{
	u_int i;
	caddr_t target = *addrp;
	u_int c;  /* the actual element count */
	bool_t stat = TRUE;
	u_int nodesize;

	assert(elsize);
	/* like strings, arrays are really counted arrays */
	if (!xdr_u_int(xdrs, sizep)) {
		return (FALSE);
	}
	c = *sizep;
#ifdef __PATCH__
 	if ((c > maxsize || c > UINT_MAX / elsize)
 	    && (xdrs->x_op != XDR_FREE)) {
#else
	if ((c > maxsize) && (xdrs->x_op != XDR_FREE)) {
#endif
		return (FALSE);
	}

	nodesize = c * elsize; // exp: {{umul}}

	/*
	 * if we are deserializing, we may need to allocate an array.
	 * We also save time by checking for a null array if we are freeing.
	 */
	if (target == NULL)
		switch (xdrs->x_op) {
		case XDR_DECODE:
			if (c == 0)
				return (TRUE);
			*addrp = target = mem_alloc(nodesize);
			if (target == NULL) {
				(void) fprintf(stderr, 
					"xdr_array: out of memory\n");
				return (FALSE);
			}
			bzero(target, nodesize);
			break;

		case XDR_FREE:
			return (TRUE);

		default: break;
	}
	
	/*
	 * now we xdr each element of array
	 */
	for (i = 0; (i < c) && stat; i++) {
		stat = (*elproc)(xdrs, target);
		target += elsize;
	}

	/*
	 * the array may need freeing
	 */
	if (xdrs->x_op == XDR_FREE) {
		mem_free(*addrp, nodesize);
		*addrp = NULL;
	}
	return (stat);
}
