DOCS = readme.html install.html
html-local: $(DOCS)
CLEANFILES = $(DOCS)
EXTRA_DIST = install.html.erb readme.html.erb

install.html: $(top_srcdir)/INSTALL
	kramdown --template $(srcdir)/install.html.erb $< > $@

readme.html: $(top_srcdir)/README
	kramdown --template $(srcdir)/readme.html.erb $< > $@
