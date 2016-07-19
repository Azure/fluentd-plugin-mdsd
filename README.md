#Azure Linux fluentd plugins

##fluent-plugin-mdm

This is fluentd output plugin for Azure Linux MDM (Multi-Metrics-Monitoring), the Geneva hot path.

###Installation

    1) build the Ifx ruby extension and Gem file.
        $ ./buildall.sh [options]

    2) Install the gem
        $ fluent-gem install <gemfile>
        e.g. $ fluent-gem install fluent-plugin-mdm-0.1.0.gem

###Configuration

<a href="src/fluent-plugin-mdm/out_mdm_sample.conf">See configuration sample</a>
