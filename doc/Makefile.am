if HAVE_DOXYGEN
dist_DATA = Doxyfile.in

directory = $(top_srcdir)/doc

docstamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > docstamp

CLEANFILES = docstamp

all-local: docstamp

clean-local:
	rm -rf $(top_srcdir)/doc

endif
