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

    <match oms.omi>
        type out_mdm
        log_level debug
        num_threads 1
        buffer_chunk_limit 1m
        buffer_type file
        buffer_path /var/opt/microsoft/omsagent/state/out_mdm*.buffer
        buffer_queue_limit 10
        flush_interval 20s
        retry_limit 10
        retry_wait 30s
        mdm_account "AzLinux"
    </match>

