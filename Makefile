# Note: BINDIR, DATA_DIR, and DESTDIR also occur in src/Makefile.
# Note: The binaries, manpages, data files and messages will eventually be
#       installed under /usr/bin, /usr/share/man, /usr/share and @prefix@/share,
#       respectively; these locations are all prefixed with $(DESTDIR)
#       which can be used, e.g. to build a binary package.
DATA_DIR = /usr/share
MAN_DIR = /usr/share/man
BINDIR  = $(DESTDIR)/usr/bin
DATADIR = $(DESTDIR)/$(DATA_DIR)
MANDIR  = $(DESTDIR)/$(MAN_DIR)
# If you change the names of any of the following subdirs,
# also change paths.h.
OLDKEYMAPDIR = keytables
KEYMAPDIR = keymaps
UNIMAPDIR = unimaps
FONTDIR   = consolefonts
PARTIALDIR= partialfonts
TRANSDIR  = consoletrans
KEYMAPSUBDIRS = include sun amiga atari i386/azerty i386/dvorak i386/fgGIod i386/qwerty i386/qwertz i386/include mac/include mac/all

# Do not use GZIP - it is interpreted by gzip
MYGZIP = gzip -f -9

SUBDIRS = src openvt po

.EXPORT_ALL_VARIABLES:

all:
	for i in $(SUBDIRS) data; do $(MAKE) -C $$i all || exit 1; done
	@echo
	@echo "Done. You can now do  make install"

install:	install-progs install-data install-man

install-progs:
	for i in $(SUBDIRS); do $(MAKE) -C $$i install || exit 1; done

install-man:
	cd man && $(MAKE) install

install-data:
	cd data && $(MAKE) install

clean:
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean || exit 1; done
	cd man && $(MAKE) clean
	cd data && $(MAKE) clean

reallyclean distclean spotless: clean
	find . -name "*~" -exec rm {} ";"
	for i in $(SUBDIRS); do $(MAKE) -C $$i distclean || exit 1; done
	rm -f Makefile src/Makefile make_include defines.h
	cp Makefile.x Makefile
