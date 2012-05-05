if LINUX
Z3        = z3-x64-3.2
endif
if DARWIN
Z3        = z3-osx-3.2.1
endif
BOOLECTOR = boolector-1.4.1-376e6b0-110304
PICOSAT   = picosat-936
PRECOSAT  = precosat-570-239dbbe-100801

all-local: $(SMT_LIB)

libz3.a:
	$(CURL) 'http://research.microsoft.com/projects/z3/$(Z3).tar.gz' | tar xz
	cp -f z3/include/*.h z3/lib/$@ .

libboolector.a:
	$(CURL) 'http://fmv.jku.at/picosat/$(PICOSAT).tar.gz' | tar xz
	ln -f -s $(PICOSAT) picosat
	sed -i -e "s/CFLAGS=/CFLAGS=-fPIC /" picosat/makefile.in
	cd picosat && ./configure -O && make
	$(CURL) 'http://fmv.jku.at/precosat/$(PRECOSAT).tar.gz' | tar xz
	ln -f -s $(PRECOSAT) precosat
	sed -i -e "s/;m32=yes//g" precosat/configure
	sed -i -e "s/malloc.h/stdlib.h/g" precosat/precosat.cc
	echo "#include <stddef.h>" > precosat/precosat.hh~
	cat precosat/precosat.hh >> precosat/precosat.hh~
	mv -f precosat/precosat.hh~ precosat/precosat.hh
	sed -i -e "s/CXXFLAGS=/CXXFLAGS=-fPIC /" precosat/makefile.in
	cd precosat && ./configure -O && make precosat.o precobnr.o
	$(CURL) 'http://fmv.jku.at/boolector/$(BOOLECTOR).tar.gz' | tar xz
	ln -f -s $(BOOLECTOR) boolector
	sed -i -e "s/CFLAGS=/CFLAGS=-fPIC /" boolector/makefile.in
	cd boolector && ./configure -precosat && make
	ln -f -s boolector/libboolector.a

libz3.so:
	$(CURL) 'http://research.microsoft.com/projects/z3/$(Z3).tar.gz' | tar xz
	cp -f z3/include/*.h z3/lib/$@ .
