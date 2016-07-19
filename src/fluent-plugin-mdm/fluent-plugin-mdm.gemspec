Gem::Specification.new do |s|
    s.name = "fluent-plugin-mdm"
    s.version = "0.1.0"
    s.date = "2016-07-18"
    s.summary = "Fluentd output plugin for Linux MDM"
    s.description = "Fluentd output plugin for IfxMetrics API, which sends data to Azure Multi-Metrics-Monitoring (MDM) agent and sevice."
    s.authors = ["Liwei Peng"]
    s.email = "liwei.peng@microsoft.com"
    s.files = ["lib/fluent/plugin/out_mdm.rb",
               "lib/fluent/plugin/Libifxext.so",
               "fluent-plugin-mdm.gemspec",
               "out_mdm_sample.conf"
              ]
    s.homepage = "https://msazure.visualstudio.com/One/Linux/_git/Compute-Runtime-Tux-fluentd"
    s.license = "Microsoft Internal Only"
end
