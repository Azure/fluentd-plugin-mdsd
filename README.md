# Azure Linux monitoring agent (mdsd) output plugin for [Fluentd](http://fluentd.org)

## Overview

This is fluentd output plugin for Azure Linux monitoring agent (mdsd).  Mdsd is the Linux logging infrastructure for Azure services. It connects various log outputs to Azure monitoring service (Geneva warm path).

The mdsd output plugin is a [buffered fluentd plugin](https://docs.fluentd.org/buffer#overview).

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

### Invalid buffered events

The output validates a complete buffered chunk before sending any of its events.
Source-timestamp mode expects MessagePack arrays `[tag, time, record]`. The tag
and record keys must be strings, the record must be a hash, and the time must be
an integer, float, or a supported Fluentd/time extension accepted by `Time.at`.
The existing formatter and timestamp precision are unchanged. In wall-clock mode
(`use_source_timestamp false`), the format remains `[tag, record]` and the output
explicitly uses the current flush time. Changing modes does not convert an old
buffer's format; incompatible entries are invalid.

- **invalid_event_action**: `backup` (default) or `quarantine`. With `backup`,
  invalid data raises an explicit `ArgumentError` identifying the chunk, reason
  and byte offset before anything is sent. Current Fluentd treats this as
  unrecoverable and applies its normal secondary/whole-chunk backup policy. The
  plugin no longer partially sends a valid prefix before discovering the invalid
  event.
- **quarantine_path**: required absolute directory when `invalid_event_action` is
  `quarantine`; also requires an explicit output `@id`. It must be on a private,
  writable filesystem that supports file and directory `fsync`. Symlinked path
  components are rejected. This option is invalid with `backup`.

Selective quarantine is opt-in:

```text
<match application.**>
  @type mdsd
  @id application_mdsd
  djsonsocket /var/run/mdsd/default_djson.socket
  acktimeoutms 5000
  invalid_event_action quarantine
  quarantine_path /var/lib/fluentd/mdsd-quarantine
</match>
```

In this mode, well-framed invalid events (including missing source timestamps) are
retained and valid events on either side can be sent with their original
timestamp semantics. A trailing sequence of MessagePack integer-zero objects is
identified as a NUL tail only at an already decoded object boundary. NUL bytes
inside a string, hash or other valid event are not trimmed.

Before any event is sent, the plugin persists an atomic entry containing
`chunk.msgpack` (the complete, unmodified logical chunk) and `manifest.json`.
Entries are partitioned by worker and a hash of the explicit plugin ID, then
named by the chunk unique ID. The manifest contains the original byte count and
SHA-256, timestamp mode, valid/deliverable counts, and delivery/rejection byte
ranges. Offsets refer to the uncompressed `chunk.read` MessagePack stream, not to
compressed buffer-file offsets. Directories are created with mode `0700` and
files with mode `0600`.

The plugin syncs the files and directories before delivery and checks an existing
entry's contents on retry. Persistence failures, conflicting entries and
unsupported sync operations raise a retryable error **without sending events**.
The same unchanged chunk does not create another archive on transport retry.
Corruption is logged at error level with chunk ID, archive path and counts, not
record contents. An interrupted persistence attempt may leave an unpublished
dot-prefixed temporary directory; inspect these only when the writer is stopped.

If MessagePack cannot be decoded, is incomplete, or reaches the safety limit of
1024 rejected ranges, the **entire chunk is quarantined and no events are sent**.
The plugin does not search arbitrary bytes for plausible records. Its full raw
archive remains available for manual diagnosis/recovery.

After persistence and successful output handling, Fluentd may remove the active
chunk. Quarantine entries are never automatically replayed or deleted. Operators
must provision persistent storage, monitor its capacity and error logs, and
define retention and a reviewed recovery procedure before enabling this mode.
A path on pod-scoped temporary storage does not survive pod replacement.

This feature does not prevent or reconstruct the original corrupted bytes,
establish downstream cloud delivery, or provide exactly-once semantics. A
transport failure after partial delivery can still cause duplicates on retry.
Do not blindly replay a whole archive: its manifest distinguishes deliverable
events from rejected bytes, and valid events may already have been delivered.
Validation adds a preflight decode and retains one logical chunk in memory;
performance and memory must be qualified for the configured chunk sizes.

### Usage

1. Install and configure mdsd. mdsd is a separate component. Please refer to related document.

1. Install the mdsd output plugin following Installation section.

1. Configure fluentd using mdsd as the output plugin. Configure fluentd using whatever input plugin you want to use.

1. Start (or restart) fluentd daemon.
