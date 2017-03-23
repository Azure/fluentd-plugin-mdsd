# Azure Linux monitoring agent (mdsd) output plugin for [Fluentd](http://fluentd.org)

## Overview

This is fluentd output plugin for Azure Linux monitoring agent (mdsd).  Mdsd is the Linux logging infrastructure for Azure services. It connects various log outputs to Azure monitoring service (Geneva warm path).

The mdsd output plugin is a [buffered fluentd plugin](http://docs.fluentd.org/articles/buffer-plugin-overview).

### Installation

The plugin should be installed using gem:

    $ sudo /PATH/TO/fluent-gem install <gemfile>
    e.g. $ sudo /PATH/TO/fluent-gem install fluent-plugin-mdsd-0.1.0.gem

### Configuration

<a href="src/fluent-plugin-mdsd/out_mdsd_sample.conf">See configuration sample</a>

The mdsd output plugin is a buffered fluentd plugin. Besides supporting all the fluentd buffered plugin parameters, it supports the following required parameters

  1) **djsonsocket**: this is the full path to mdsd dynamic json socket file.

  2) **acktimeoutms**: max time in milli-seconds to wait for mdsd acknowledge response. Before timeout, mdsd plugin will retry periodically to resend the events to mdsd. After timeout, the events holding in mdsd plugin memory will be dropped. If acktimeoutms is 0, the plugin won't do any failure retry if it cannot receives acknowledge from mdsd.

  3) **mdsd_tag_regex_patterns**: (Optional) An array of regex patterns for mdsd source name
unification purpose. The passed will be matched against each regex, and if there's a match,
the matched substring will be used as the resulting mdsd source name. For example, if the tag
is `mdsd.ext_syslog.user.info` and the regex is `^mdsd\.ext_syslog\.\w+`, then `mdsd.ext_syslog.user`
will be the mdsd source name.
If this parameter is not specified, or for tags not matching any regexes in this array parameter,
the original fluentd tag will be used as the mdsd source name. Default: `[]`.

### Usage

1) Install and configure mdsd. mdsd is a separate component. Please refer to related document.

2) Install the mdsd output plugin following Installation section.

3) Configure fluentd using mdsd as the output plugin. Configure fluentd using whatever input plugin you want to use.

4) Start (or restart) fluentd daemon.

