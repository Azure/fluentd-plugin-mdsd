GEM=/opt/microsoft/omsagent/ruby/bin/fluent-gem
MDMGEM=fluent-plugin-mdm


all: $(MDMGEM)

$(MDMGEM): fluent-plugin-mdm.gemspec lib/fluent/plugin/out_mdm.rb
	cp -f ../builddir/release/lib/Libifxext.so lib/fluent/plugin/Libifxext.so
	$(GEM) build fluent-plugin-mdm.gemspec

clean:
	rm -rf *.gem
