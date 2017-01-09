GEM=/opt/microsoft/omsagent/ruby/bin/fluent-gem
MDSDGEM=fluent-plugin-mdsd


all: $(MDSDGEM)

$(MDSDGEM): $(MDSDGEM).gemspec lib/fluent/plugin/out_mdsd.rb
	cp -f ../builddir/release/lib/Liboutmdsdrb.so lib/fluent/plugin/Liboutmdsdrb.so
	$(GEM) build $(MDSDGEM).gemspec

install:
	$(GEM) install $(MDSDGEM)*.gem

clean:
	rm -rf *.gem
