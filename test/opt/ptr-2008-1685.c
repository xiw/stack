// RUN: %cc %s | optck | diagdiff --prefix=exp %s
//
// https://groups.google.com/d/msg/comp.os.plan9/NYdK1L7rf8Q/yfAiZoOlwNUJ 

void bar(void);

void foo(char *buf)
{
	unsigned int len = 1<<30;
	if (buf + len < buf)
		bar();		// exp: {{anti-dce}}
}
