LABEL?=~dev
VERSION=$(shell cat ../Version.txt)-$(LABEL)
DEPENDS=

PACKAGE=liboutmdsd-dev
BINDIR=../builddir/release/lib
FAKEROOT=./data-root
DOCDIR=$(FAKEROOT)/usr/share/doc/$(PACKAGE)
LINTIANOVERRIDES=$(FAKEROOT)/usr/share/lintian/overrides/$(PACKAGE)
INCLUDEDIR=$(FAKEROOT)/usr/include/outmdsd/
LIBDIR=$(FAKEROOT)/usr/lib/x86_64-linux-gnu
DEB=$(PACKAGE)_$(VERSION)_amd64.deb

package: $(DEB)

signed-package: _gpgorigin $(DEB)
	ar r $(DEB) $<

_gpgorigin: $(DEB)
	-rm -f $@
	ar p $(DEB) debian-binary control.tar.gz data.tar.gz | gpg -abs -o _gpgorigin

$(DEB): tarballs debian-binary
	-rm -f $@
	ar rc $@ debian-binary control.tar.gz data.tar.gz
	lintian $@

$(DOCDIR):
	mkdir -p $@

$(DOCDIR)/changelog.Debian.gz: changelog $(DOCDIR)
	cat $< | sed "s/PACKAGE/$(PACKAGE)/g" | gzip -9 > $@
	chmod 644 $(@)

$(DOCDIR)/copyright: copyright $(DOCDIR)
	cp $< $@
	chmod 644 $@

$(LINTIANOVERRIDES): lintian-overrides
	mkdir -p $(@D)
	cat $< | sed "s/PACKAGE/$(PACKAGE)/g" > $@
	chmod 644 $(@)

debian-binary:
	echo 2.0 > debian-binary

tarballs: data.tar.gz control.tar.gz

control.tar.gz: md5sums control
	-rm -rf control-root
	-mkdir -p control-root
	cp control md5sums control-root
	sed -i '/^Package:/c Package: $(PACKAGE)' control-root/control
	sed -i '/^Version:/c Version: $(VERSION)' control-root/control
	sed -i '/^Depends:/c Depends: $(DEPENDS)' control-root/control
	chmod 644 control-root/*
	cd control-root && tar -czf ../$@ --owner=root --group=root .

md5sums: install-deps
	(cd $(FAKEROOT) && md5sum `find -type f`) > $@
	chmod 0644 $@

data.tar.gz: install-deps \
             $(DOCDIR)/changelog.Debian.gz \
             $(DOCDIR)/copyright \
             $(LINTIANOVERRIDES)
	find $(FAKEROOT) -type d | xargs chmod 0755
	cd $(FAKEROOT) && tar -czf ../$@ --owner=root --group=root --mode=go-w *

.PHONY: clean install-clean install-deps

clean: install-clean
	-rm -rf control-root
	-rm -f debian-binary *.tar.gz _gpgorigin md5sums
	-rm -f lib*deb

install-clean:
	-rm -rf $(FAKEROOT)

install-deps: install-clean install-lib install-header

install-header:
	mkdir -p $(INCLUDEDIR)
	cp -f -r -P ../outmdsd/*.h $(INCLUDEDIR)
	find $(INCLUDEDIR) -type f | xargs chmod 644

install-lib:
	mkdir -p $(LIBDIR)
	cp -f -P $(BINDIR)/liboutmdsd.a $(LIBDIR)/
	chmod 0644 $(LIBDIR)/lib*
