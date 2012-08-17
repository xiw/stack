if LINUX
Z3        = z3-x64-4.1
endif
if DARWIN
Z3        = z3-osx-4.1-x64
endif
BOOLECTOR = boolector-1.4.1-376e6b0-110304
PICOSAT   = picosat-936
PRECOSAT  = precosat-576-7e5e66f-120112

all-local: $(SMT_LIB)

libz3.a:
	$(CURL) 'http://research.microsoft.com/en-us/um/redmond/projects/z3/$(Z3).tar.gz' | tar xz
	cp -f z3/include/*.h z3/lib/$@ .

libboolector.a:
	$(CURL) 'http://fmv.jku.at/picosat/$(PICOSAT).tar.gz' | tar xz
	ln -f -s $(PICOSAT) picosat
	sed -i -e "s/CFLAGS=/CFLAGS=-fPIC /" picosat/makefile.in
	cd picosat && ./configure -O && make
	$(CURL) 'http://fmv.jku.at/precosat/$(PRECOSAT).tar.gz' | tar xz
	ln -f -s $(PRECOSAT) precosat
	sed -i -e "s/CXXFLAGS=/CXXFLAGS=-fPIC /" precosat/makefile.in
	cd precosat && ./configure && make
	$(CURL) 'http://fmv.jku.at/boolector/$(BOOLECTOR).tar.gz' | tar xz
	ln -f -s $(BOOLECTOR) boolector
	sed -i -e "s/CFLAGS=/CFLAGS=-fPIC /" boolector/makefile.in
	cd boolector && ./configure -precosat && make
	ln -f -s boolector/libboolector.a

libz3.so:
	$(CURL) 'http://research.microsoft.com/projects/z3/$(Z3).tar.gz' | tar xz
	cp -f z3/include/*.h z3/lib/$@ .
