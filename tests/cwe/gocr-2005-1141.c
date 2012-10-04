// RUN: %cc %s | intck | diagdiff %s --prefix=exp
//
// http://www.overflow.pl/adv/gocr.txt
// http://jocr.cvs.sourceforge.net/viewvc/jocr/jocr/src/pnm.c?r1=1.22&r2=1.23
//
// The unusual fix uses floating point to detect integer overflow.
// It also invokes undefined behavior (signed integer overflow).

#include <stdio.h>
#include <stdlib.h>

#define EE()		fprintf(stderr,"\nERROR "__FILE__" L%d: ",__LINE__)
#define F0(x0)		{EE();fprintf(stderr,x0 "\n");      exit(1);}
#define F1(x0,x1)	{EE();fprintf(stderr,x0 "\n",x1);   exit(1);}

typedef struct pixmap {
	unsigned char *p;	/* pointer of image buffer (pixmap) */
	int x;			/* xsize */
	int y;			/* ysize */
	int bpp;		/* bytes per pixel:  1=gray 3=rgb */
} pix;

struct pam {
	int height;
	int width;
};

void readpgm(pix *p, struct pam *inpam)
{
	p->x = inpam->width;
	p->y = inpam->height;
#ifdef __PATCH__
	if ( (1.*(p->x*p->y))!=((1.*p->x)*p->y) )
		F0("Error integer overflow");
#endif
	if ( !(p->p = (unsigned char *)malloc(p->x*p->y)) ) // exp: {{smul}}
		F1("Error at malloc: p->p: %d bytes", p->x*p->y);
}
