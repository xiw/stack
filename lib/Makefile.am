BOOLECTOR = boolector-1.5.116-eeaf10b-121004
LINGELING = lingeling-al6-080d45d-120922

EXTRA_DIST = $(BOOLECTOR).tar.gz $(LINGELING).tar.gz

all-local: libboolector.a

libboolector.a: $(BOOLECTOR).tar.gz liblgl.a
	tar xzf $<
	$(LN_S) -f $(BOOLECTOR) boolector
	$(SED) -i -e "s/CFLAGS=/CFLAGS=-fPIC /" boolector/makefile.in
	cd boolector && ./configure && $(MAKE)
	$(LN_S) -f boolector/libboolector.a

liblgl.a: $(LINGELING).tar.gz
	tar xzf $<
	$(LN_S) -f $(LINGELING) lingeling
	$(SED) -i -e "s/CFLAGS=/CFLAGS=-fPIC /" lingeling/makefile.in
	cd lingeling && ./configure && $(MAKE) liblgl.a
	$(LN_S) -f lingeling/liblgl.a
