if HAVE_DOXYGEN
noinst_DATA = Doxyfile.in

directory = $(top_srcdir)/doc

docstamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > docstamp

CLEANFILES = docstamp

all-local: docstamp

clean-local:
	rm -rf $(top_srcdir)/doc/{html,latex,docstamp}

endif
