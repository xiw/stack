// RUN: %kcc -o - %s | %cmpsat --check-prefix=exp
// http://git.kernel.org/linus/44b0052c5cb4e75389ed3eb9e98c29295a7dadfb

void bar0(void);
void bar1(void);

void foo(int errc)
{
	if (((errc & 0x7f) >> 8) > 127) // exp: {{comparison always false}}
		bar0();
	if ((errc & 0xff) > 127)
		bar1();
}
