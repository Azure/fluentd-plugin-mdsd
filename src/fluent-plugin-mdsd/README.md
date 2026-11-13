# Azure Linux monitoring agent (mdsd) output plugin for [Fluentd](http://fluentd.org)

## Overview

This is fluentd output plugin for Azure Linux monitoring agent (mdsd).  Mdsd is the Linux logging infrastructure for Azure services. It connects various log outputs to Azure monitoring service (Geneva warm path).

The mdsd output plugin is a [buffered fluentd plugin](http://docs.fluentd.org/articles/buffer-plugin-overview).

### Installation

NOTE: The plugin gem must be installed using **fluent-gem**.

Download the [gemfile](https://github.com/Azure/fluentd-plugin-mdsd/releases) to your computer. Assume it is saved to /path/to/gemfile.

For td-agent, run

    $sudo bash
    $umask 22
    $/opt/td-agent/embedded/bin/fluent-gem install /path/to/gemfile

For oms-agent, run

    $sudo bash
    $umask 22
    $/opt/microsoft/omsagent/ruby/bin/fluent-gem install /path/to/gemfile


### Expected Data Formats

The data records sent to this plugin are expected to be in a flat structure key-value pairs, where each value are primitive types like number, string, boolean, or time. Value types including Array, Hash, and Range will be dumped as strings.

If this behavior doesn't meet your requirements, the recommended way is to create filter plugins for your data records.

### Configuration

[See configuration sample](./src/fluent-plugin-mdsd/out_mdsd_sample.conf)

The mdsd output plugin is a buffered fluentd plugin. Besides supporting all the fluentd buffered plugin parameters, it supports the following required parameters

- **log_level**: this is [standard fluentd per plugin log level](http://docs.fluentd.org/v0.12/articles/logging#per-plugin-log). Valid values are: fatal, error, warn, info, debug, trace.

- **djsonsocket**: this is the full path to mdsd dynamic json socket file.

- **acktimeoutms**: max time in milliseconds to wait for mdsd acknowledge response. Before timeout, mdsd plugin will retry periodically to resend the events to mdsd. After timeout, the events holding in mdsd plugin memory will be dropped. If acktimeoutms is 0, the plugin won't do any failure retry if it cannot receives acknowledge from mdsd.

- **mdsd_tag_regex_patterns**: (Optional) An array of regex patterns for mdsd source name unification purpose. The passed will be matched against each regex, and if there's a match, the matched substring will be used as the resulting mdsd source name. For example, if the tag is `mdsd.ext_syslog.user.info` and the regex is `^mdsd\.ext_syslog\.\w+`, then `mdsd.ext_syslog.user` will be the mdsd source name. If this parameter is not specified, or for tags not matching any regexes in this array parameter, the original fluentd tag will be used as the mdsd source name. Default: `[]`.

- **resend_interval_ms**: the interval in milliseconds that failed messages are resent to mdsd by this plugin. Default: 30,000.

- **conn_retry_timeout_ms**: the timeout in milliseconds to do network connection retry when connecting to mdsd process failed. Default: 60,000.

- **emit_timestamp_name**: the field name for the event emit time stamp. Default: "FluentdIngestTimestamp".

- **use_source_timestamp**: use the timestamp in the record source. If false, sets the time to Time.now Default: true.

- **convert_hash_to_json**: if this is set to true, then plugin will convert hash string to proper json before sending to mdsd. 
  Input record with hash  {key => {"X"=>"Y"}} will be tranformed to {key => {"X":"Y"}}

### Invalid buffered events

The output validates the entire chunk before sending. With
`invalid_event_action backup` (the default), it reports the chunk ID, reason and
byte offset explicitly and leaves secondary/whole-chunk backup to Fluentd; a
valid prefix is not partially sent first.

Selective quarantine requires `invalid_event_action quarantine`, an explicit
output `@id`, and an absolute `quarantine_path` on a private persistent filesystem
supporting file and directory `fsync`. It atomically retains the full original
logical chunk and a SHA-256/byte-range manifest before sending well-framed valid
events. Missing source timestamps are retained, never replaced with the current
time. Legitimate NUL bytes in valid payloads are preserved. Undecodable or
incomplete MessagePack, or the 1024-rejected-range safety limit, quarantines the
entire chunk without delivery or guessed resynchronization.

Persistence failures are retryable before delivery. Transport retries can still
duplicate valid events; quarantine entries themselves are idempotent for an
unchanged chunk ID/content/mode. Archives require operator-managed capacity,
retention and reviewed recovery; no automatic deletion/replay occurs. This does
not prevent original corruption, reconstruct overwritten bytes, or prove cloud
delivery. For the full policy, example and operational requirements, see
[the repository README](../../README.md#invalid-buffered-events), which is copied
into the built gem.

### Usage

1. Install and configure mdsd. mdsd is a separate component. Please refer to related document.

1. Install the mdsd output plugin following Installation section.

1. Configure fluentd using mdsd as the output plugin. Configure fluentd using whatever input plugin you want to use.

1. Start (or restart) fluentd daemon.
