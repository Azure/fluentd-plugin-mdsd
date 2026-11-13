# Contributing

## Install required software

- CMake version >= 3.0
- Boost unit test version 1.55 (libboost-test)
  NOTE: other version of libboost-test may not work.
- Boost system library version 1.55 (libboost-system)
- GCC version >= 4.8.4
- Ruby 2.x with [OMS Linux Agent](https://github.com/Microsoft/OMS-Agent-for-Linux)
  or [Fluentd Agent](http://www.fluentd.org/download)
- [SWIG](http://www.swig.org/) 3.0 for td-agent3 or SWIG 4.0.2 for td-agent4

## How to build the code

Run:

```bash
cd src
./buildall.sh [options]
```

## How to run full tests

Build the code following "How to build the code" section.

```bash
cd src/builddir/release/tests
./runtests.sh [options]
```

Check logs in *.log if any error.

### Buffered-corruption regression tests

After generating the gem specification and making the native library available
as described above, run the focused Ruby tests from `src/fluent-plugin-mdsd`:

```bash
bundle exec ruby -rfluent/output -Ilib test/plugin/test_corrupt_chunks.rb
```

These tests exercise the production `write`, real Fluentd file-buffer
flush/backup/retry behavior, rejection-range limits, and concurrent quarantine
publication with a captured transport. They do not require the C++ mock server
or a live MDSD destination. Native socket integration remains a separate
validation. The development `test-unit` constraint permits newer 3.x releases
because 3.0.x cannot load on Ruby 3.x. The corruption/flush suite targets
Fluentd 1.x.

### Validated build and test profiles

The clean native build, full native and packaged Ruby tests, and local gem
installation have been exercised with:

| Ruby | Fluentd | Boost |
| --- | --- | --- |
| 2.7.8 | 1.16.11 | 1.55.0 |
| 3.3.5 | 1.19.3 | 1.56.0 |

Both profiles used GCC 10.2.1, CMake 3.18.4 and SWIG 4.0.2. These standalone
validations used the `system` build target and matching Ruby/Fluentd executables
for the test runner. They do not certify every version accepted by the unpinned
Fluentd dependency, OMS/td-agent distribution packages, or the legacy
GLIBC <= 2.9 binary compatibility gate.

## Overview of mdsd plugin design and code structure

mdsd output plugin sends events to mdsd by using UNIX domain socket.

The core code that sends events to mdsd is in directory **outmdsd**. It is built into a static library file called **liboutmdsd.a**.

[SWIG](http://www.swig.org/) is used to build a shim around C++ liboutmdsd library. This is in directory **outmdsdrb**. The shim is built into library called **Liboutmdsdrb.so**, which is called by Ruby code directly.

The ruby code calling Liboutmdsdrb.so is in directory **fluent-plugin-mdsd**.

## How mdsd output plugin works

The plugin will send dynamic schema data in JSON format (called DJSON) to mdsd using UNIX domain socket. Please refer to mdsd document on what dynamic schema protocol is supported.

The data flow path is:

1. Fluentd takes input and/or filter plugin data to mdsd output plugin. The data are in JSON format.

1. Mdsd output plugin processes each of the data record.

   a) Find or create schema.
      It uses a global hash to hold all existing schemas. When a new record arrives, its schema is searched in the global hash. If found, it will be used. If not found, it will be created and stored in the hash.

   b) Add a unique message id to the data record.

   c) Construct the message by following mdsd DJSON protocol.

   d) Send data to mdsd socket.
      This part is complex. See related section on how this works.

## How mdsd plugin sends data to mdsd socket

Mdsd plugin uses multiple threads to send data to mdsd process using UNIX domain socket. The #2/#3 threads are created in liboutmdsd.a.

1. Main thread.

   This is the plugin ruby code. The ruby code will call some send API to send a data string. If the send succeeds, the ruby code will return. If the send fails, a runtime error is raised. Fluentd will handle this error and retry later based on [buffered plugin design](http://docs.fluentd.org/articles/buffer-plugin-overview).

   What happens is when the ruby send API succeeds, the data record will be saved into a global concurrent cache. The cache is used for future retry.

1. Reader thread.

   This thread reads mdsd acknowledge response data. When mdsd receives data from socket client, it always returns some TAG information back to socket client. This TAG information can tell which message is received and what happened to the message.

   If plugin parameter acktimeoutms has a value greater than 0, when a TAG is received from mdsd agent, the corresponding data record will be removed from the global concurrent cache.

1. Resender thread.

   If plugin parameter acktimeoutms has a value greater than 0, the plugin will periodically resend the data in the global concurrent cache to mdsd agent.
