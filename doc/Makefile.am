SUBDIRS = samples mini-howto man
dist_noinst_DATA = COMMAND_LINE Doxyfile portable_cp
dist_pkgdata_DATA = README Features.html Limitations.html Notes.html Tutorial.html Good_Backup_Practice.html FAQ.html api_tutorial.html dar_doc.jpg dar_s_doc.jpg index.html dar-catalog.dtd Known_Bugs.html authentification.html dar_key.txt old_dar_key.txt from_sources.html downloading.html presentation.html usage_notes.html python/libdar_test.py

INSTALL_HTML_MAN = $(INSTALL) -d $(DESTDIR)$(pkgdatadir)/man ; $(INSTALL) -m 0644 man/*.html $(DESTDIR)$(pkgdatadir)/man
UNINSTALL_HTML_MAN = rm -rf $(DESTDIR)$(pkgdatadir)/man

if USE_DOXYGEN
DOXYGEN = @DOXYGEN_PROG@

all-local: Doxyfile.tmp

Doxyfile.tmp:
	sed -e "s%##VERSION##%@PACKAGE_VERSION@%g" -e "s%##HAS_DOT##%@HAS_DOT@%g" Doxyfile > Doxyfile.tmp
	cd .. ; $(DOXYGEN) doc/Doxyfile.tmp
	if [ -d html/search ]; then chmod u+x html/search ; fi

clean-local:
	rm -rf html Doxyfile.tmp doxygen_sqlite3.db

install-data-hook:
	./portable_cp html $(DESTDIR)$(pkgdatadir)
	$(INSTALL_HTML_MAN)
	$(INSTALL) -d $(DESTDIR)$(pkgdatadir)/python
	$(INSTALL) -m 0644 python/libdar_test.py $(DESTDIR)$(pkgdatadir)/python

uninstall-hook:
	rm -rf $(DESTDIR)$(pkgdatadir)/html
	$(UNINSTALL_HTML_MAN)
	rmdir $(DESTDIR)$(pkgdatadir) || true

else

all-local:

clean-local:

install-data-hook:
	$(INSTALL_HTML_MAN)

uninstall-hook:
	$(UNINSTALL_HTML_MAN)

endif




