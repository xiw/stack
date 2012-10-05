if LINUX
Z3        = z3-x64-4.1
endif
if DARWIN
Z3        = z3-osx-4.1-x64
endif
BOOLECTOR = boolector-1.5.116-eeaf10b-121004
LINGELING = lingeling-al6-080d45d-120922

all-local: $(SMT_LIB)

libz3.a:
	$(CURL) 'http://research.microsoft.com/en-us/um/redmond/projects/z3/$(Z3).tar.gz' | tar xz
	cp -f z3/include/*.h z3/lib/$@ .

libboolector.a: liblgl.a
	$(CURL) 'http://fmv.jku.at/boolector/$(BOOLECTOR).tar.gz' | tar xz
	ln -f -s $(BOOLECTOR) boolector
	sed -i -e "s/CFLAGS=/CFLAGS=-fPIC /" boolector/makefile.in
	cd boolector && ./configure && make
	ln -f -s boolector/libboolector.a

liblgl.a:
	$(CURL) 'http://fmv.jku.at/lingeling/$(LINGELING).tar.gz' | tar xz
	ln -f -s $(LINGELING) lingeling
	sed -i -e "s/CFLAGS=/CFLAGS=-fPIC /" lingeling/makefile.in
	cd lingeling && ./configure && make
	ln -f -s lingeling/liblgl.a
