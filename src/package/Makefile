CC?=clang
LABEL?=~dev
VERSION=0.1.0-$(LABEL)
LIBSUFFIX=
DEPENDS=libc6 (>= 2.15)

ifeq ($(CC), clang)
	LIBSUFFIX=-clang
	DEPENDS += , libc++1
	DEPENDS += , libc++-helpers
endif

PACKAGE=fluentd-ifxext$(LIBSUFFIX)
RELDIR=../builddir/release
FAKEROOT=./data-root
DOCDIR=$(FAKEROOT)/usr/share/doc/$(PACKAGE)
LINTIANOVERRIDES=$(FAKEROOT)/usr/share/lintian/overrides/$(PACKAGE)
PLUGINDIR=$(FAKEROOT)/opt/microsoft/omsagent/plugin/
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
	#lintian -X shared-libs $@

$(DOCDIR):
	mkdir -p $@

$(DOCDIR)/changelog.gz: changelog $(DOCDIR)
	cat $< | sed "s/PACKAGE/$(PACKAGE)/g" | gzip -9 > $@
	chmod 644 $@

$(DOCDIR)/copyright: copyright $(DOCDIR)
	cp $< $@
	chmod 644 $@

$(LINTIANOVERRIDES): lintian-overrides
	mkdir -p $(@D)
	cat $< | sed "s/PACKAGE/$(PACKAGE)/g" > $@
	chmod 644 $@

debian-binary:
	echo 2.0 > debian-binary

tarballs: data.tar.gz control.tar.gz

control.tar.gz: md5sums control
	-rm -rf control-root
	-mkdir -p control-root
	cp control md5sums postinst control-root
	chmod 644 control-root/*
	chmod 755 control-root/postinst
	sed -i '/^Version:/c Version: $(VERSION)' control-root/control
	sed -i '/^Package:/c Package: $(PACKAGE)' control-root/control
	sed -i '/^Depends:/c Depends: $(DEPENDS)' control-root/control
	cd control-root && tar -czf ../$@ --owner=root --group=root .

md5sums: install-deps
	(cd $(FAKEROOT) && md5sum `find -type f`) > $@
	chmod 0644 $@

data.tar.gz: install-deps \
             $(DOCDIR)/changelog.gz \
             $(DOCDIR)/copyright \
             $(LINTIANOVERRIDES)
	find $(FAKEROOT) -type d | xargs chmod 755
	cd $(FAKEROOT) && tar -czf ../$@ --owner=root --group=root --mode=go-w *

.PHONY: clean install-clean install-deps

clean: install-clean
	-rm -rf control-root
	-rm -f debian-binary *.tar.gz _gpgorigin md5sums
	-rm -f *deb

install-clean:
	-rm -rf $(FAKEROOT)

install-deps: install-clean	
	mkdir -p $(PLUGINDIR)
	cp -f -P $(RELDIR)/lib/*.so $(PLUGINDIR)
	cp -f -P $(RELDIR)/plugin/*.rb $(PLUGINDIR)
	chmod 0744 $(PLUGINDIR)/*.so
	chmod 0744 $(PLUGINDIR)/*.rb
