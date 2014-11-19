if HAVE_DOXYGEN
directory = $(top_srcdir)/doc

docstamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > docstamp

CLEANFILES = docstamp

all-local: docstamp

clean-local:
	rm -rf $(top_srcdir)/doc

endif
