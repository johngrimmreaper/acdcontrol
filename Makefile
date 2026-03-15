RELEASE_FILES=acdcontrol.cpp acdcontrol.init acdcontrol.sysconfig COPYING COPYRIGHT Makefile VERSION 69-apple-cinema.rules
VERSION=$(shell cat VERSION)
VERNAME=acdcontrol-$(VERSION)
DIRNAME=/tmp/$(VERNAME)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
UDEVRULESDIR ?= /etc/udev/rules.d
DESTDIR ?=

CXX ?= g++
CXXFLAGS ?=

.PHONY: clean release install uninstall

all: acdcontrol

acdcontrol: acdcontrol.cpp
	$(CXX) $(CXXFLAGS) acdcontrol.cpp -o acdcontrol

clean:
	rm -f acdcontrol

release:
	mkdir -p $(DIRNAME)
	rm -rf $(DIRNAME)/*
	cp $(RELEASE_FILES) $(DIRNAME)
	tar cvfz $(VERNAME).tar.gz -C /tmp $(VERNAME) 

upload:
	curl -T $(VERNAME).tar.gz ftp://anonymous@upload.sourceforge.net/incoming/

install:
	install -d $(DESTDIR)$(UDEVRULESDIR)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0644 69-apple-cinema.rules $(DESTDIR)$(UDEVRULESDIR)/
	install -m 0755 acdcontrol $(DESTDIR)$(BINDIR)/

uninstall:
	rm -f $(DESTDIR)$(UDEVRULESDIR)/69-apple-cinema.rules
	rm -f $(DESTDIR)$(BINDIR)/acdcontrol
	