if HAVE_DOXYGEN
noinst_DATA = Doxyfile.in

directory = $(top_builddir)/doc

docstamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > docstamp

CLEANFILES = docstamp

all-local: docstamp

clean-local:
	rm -rf $(top_builddir)/doc/{html,latex,docstamp}

endif
