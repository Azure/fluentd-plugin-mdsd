GEM=/opt/microsoft/omsagent/ruby/bin/fluent-gem
MDMGEM=fluent-plugin-mdm


all: $(MDMGEM)

$(MDMGEM): $(MDMGEM).gemspec lib/fluent/plugin/out_mdm.rb
	cp -f ../builddir/release/lib/Libifxext.so lib/fluent/plugin/Libifxext.so
	$(GEM) build $(MDMGEM).gemspec

install:
	$(GEM) install $(MDMGEM)*.gem

clean:
	rm -rf *.gem
