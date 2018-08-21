Gem::Specification.new do |s|
    s.name = "fluent-plugin-mdsd"
    s.version = "0.1.8-build.dev"
    s.summary = "Fluentd output plugin for Linux monitoring agent (mdsd)"
    s.description = "Fluentd output plugin for Linux monitoring agent(mdsd), which sends data to Azure Geneva service."
    s.authors = ["Liwei Peng"]
    s.email = ["Liwei.Peng@microsoft.com"]
    s.extra_rdoc_files = [
      "LICENSE.txt",
      "README.md"
    ]
    s.files = [
      "Gemfile",
      "LICENSE.txt",
      "README.md",
      "Rakefile",
      "lib/fluent/plugin/out_mdsd.rb",
      "lib/fluent/plugin/Liboutmdsdrb.so",
      "fluent-plugin-mdsd.gemspec",
      "out_mdsd_sample.conf",
      "test/plugin/test_out_mdsd.rb"
    ]
    s.homepage = "https://github.com/Azure/fluentd-plugin-mdsd/"
    s.license = "MIT"

    s.add_dependency "fluentd", "~> 0.12.0"
    s.add_development_dependency "rake"
    s.add_development_dependency "test-unit", "~> 3.0.9"
end
