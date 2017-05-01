# Azure Linux monitoring agent (mdsd) output plugin for [Fluentd](http://fluentd.org)

## Overview

This is fluentd output plugin for Azure Linux monitoring agent (mdsd).  Mdsd is the Linux logging infrastructure for Azure services. It connects various log outputs to Azure monitoring service (Geneva warm path).

The mdsd output plugin is a [buffered fluentd plugin](http://docs.fluentd.org/articles/buffer-plugin-overview).

### Installation

The plugin should be installed using gem:

    Download the gemfile to your computer.
    $ sudo /PATH/TO/fluent-gem install <gemfile>

### Expected Data Formats

The data records sent to this plugin are expected to be in a flat structure key-value pairs, where each value are primitive types like number, string, boolean, or time. Value types including Array, Hash, and Range will be dumped as strings.

If this behavior doesn't meet your requirements, the recommended way is to create filter plugins for your data records.

### Configuration

[See configuration sample](./src/fluent-plugin-mdsd/out_mdsd_sample.conf)

The mdsd output plugin is a buffered fluentd plugin. Besides supporting all the fluentd buffered plugin parameters, it supports the following required parameters

- **log_level**: this is [standard fluentd per plugin log level](http://docs.fluentd.org/v0.12/articles/logging#per-plugin-log). Valid values are: fatal, error, warn, info, debug, trace.

- **djsonsocket**: this is the full path to mdsd dynamic json socket file.

- **acktimeoutms**: max time in milli-seconds to wait for mdsd acknowledge response. Before timeout, mdsd plugin will retry periodically to resend the events to mdsd. After timeout, the events holding in mdsd plugin memory will be dropped. If acktimeoutms is 0, the plugin won't do any failure retry if it cannot receives acknowledge from mdsd.

- **mdsd_tag_regex_patterns**: (Optional) An array of regex patterns for mdsd source name unification purpose. The passed will be matched against each regex, and if there's a match, the matched substring will be used as the resulting mdsd source name. For example, if the tag is `mdsd.ext_syslog.user.info` and the regex is `^mdsd\.ext_syslog\.\w+`, then `mdsd.ext_syslog.user` will be the mdsd source name. If this parameter is not specified, or for tags not matching any regexes in this array parameter, the original fluentd tag will be used as the mdsd source name. Default: `[]`.

- **resend_interval_ms**: the interval in milli-seconds that failed messages are resent to mdsd by this plugin. Default: 30,000.

- **conn_retry_timeout_ms**: the timeout in milli-seconds to do network connection retry when connecting to mdsd process failed. Default: 60,000.

### Usage

1. Install and configure mdsd. mdsd is a separate component. Please refer to related document.

1. Install the mdsd output plugin following Installation section.

1. Configure fluentd using mdsd as the output plugin. Configure fluentd using whatever input plugin you want to use.

1. Start (or restart) fluentd daemon.
