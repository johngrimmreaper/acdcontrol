RELEASE_FILES = \
	acdcontrol.cpp \
	acdprobe.cpp \
	acdcontrol.init \
	acdcontrol.sysconfig \
	COPYING \
	COPYRIGHT \
	Makefile \
	README.rst \
	VERSION \
	69-apple-cinema.rules \
	man/acdcontrol.1 \
	man/acdprobe.1 \
	docs/probe-workflow.rst \
	docs/summary-schema.rst \
	docs/model-notes/05ac_9226.rst

VERSION = $(shell cat VERSION)
VERNAME = acdcontrol-$(VERSION)
DIRNAME = /tmp/$(VERNAME)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
UDEVRULESDIR ?= /etc/udev/rules.d
MANDIR ?= $(PREFIX)/share/man
MAN1DIR ?= $(MANDIR)/man1
DESTDIR ?=

CXX ?= g++
CXXFLAGS ?=
MAN ?= man

MANPAGES = man/acdcontrol.1 man/acdprobe.1

.PHONY: all clean release upload install uninstall install-man uninstall-man man man-acdcontrol man-acdprobe

all: acdcontrol acdprobe

acdcontrol: acdcontrol.cpp
	$(CXX) $(CXXFLAGS) acdcontrol.cpp -o acdcontrol

acdprobe: acdprobe.cpp
	$(CXX) $(CXXFLAGS) acdprobe.cpp -o acdprobe

clean:
	rm -f acdcontrol acdprobe

release:
	mkdir -p $(DIRNAME)
	rm -rf $(DIRNAME)/*
	cp $(RELEASE_FILES) $(DIRNAME)
	tar cvfz $(VERNAME).tar.gz -C /tmp $(VERNAME)

upload: release
	curl -T $(VERNAME).tar.gz ftp://anonymous@upload.sourceforge.net/incoming/

install:
	install -d $(DESTDIR)$(UDEVRULESDIR)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0644 69-apple-cinema.rules $(DESTDIR)$(UDEVRULESDIR)/
	install -m 0755 acdcontrol $(DESTDIR)$(BINDIR)/

uninstall:
	rm -f $(DESTDIR)$(UDEVRULESDIR)/69-apple-cinema.rules
	rm -f $(DESTDIR)$(BINDIR)/acdcontrol

install-man:
	install -d $(DESTDIR)$(MAN1DIR)
	install -m 0644 $(MANPAGES) $(DESTDIR)$(MAN1DIR)/

uninstall-man:
	rm -f $(DESTDIR)$(MAN1DIR)/acdcontrol.1
	rm -f $(DESTDIR)$(MAN1DIR)/acdprobe.1

man: man-acdcontrol man-acdprobe

man-acdcontrol:
	$(MAN) ./man/acdcontrol.1

man-acdprobe:
	$(MAN) ./man/acdprobe.1
