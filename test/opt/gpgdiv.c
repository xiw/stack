// RUN: %cc %s | optck | diagdiff --prefix=exp %s

int bar(int);

int foo1(int msize)
{
	if (!msize)
		msize = 1 / msize; // exp: {{anti-dce}}
	return bar(msize);
}

int foo2(int dsize)
{
	switch (dsize) {
	case 0:
		return 1 / dsize; // exp: {{anti-dce}}
	case 1:
		return bar(1);
	default:
		return bar(dsize);
	}
}

