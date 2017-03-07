LABEL?=dev
Target?=td

ifeq ($(Target),oms)
  GEM=/opt/microsoft/omsagent/ruby/bin/fluent-gem
else
  GEM=/opt/td-agent/embedded/bin/fluent-gem
endif

MDSDGEM=fluent-plugin-mdsd
Version=$(shell cat ../Version.txt)-$(LABEL)

all: $(MDSDGEM)

$(MDSDGEM): $(MDSDGEM).gemspec lib/fluent/plugin/out_mdsd.rb
	cp -f ../builddir/release/lib/Liboutmdsdrb.so lib/fluent/plugin/Liboutmdsdrb.so
	$(GEM) build $(MDSDGEM).gemspec

$(MDSDGEM).gemspec: outmdsd.gemspec ../Version.txt
	cat $< | sed "s/GEMVERSION/${Version}/g" > $@

install:
	$(GEM) install $(MDSDGEM)*.gem

clean:
	rm -rf *.gem $(MDSDGEM).gemspec
