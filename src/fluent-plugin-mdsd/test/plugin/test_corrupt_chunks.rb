require "test/unit"
require "fluent/test"
require "fluent/output"
require "fluent/plugin/out_mdsd"
require "fluent/plugin/buffer/memory_chunk"
require "tmpdir"

class MdsdCorruptChunksTest < Test::Unit::TestCase
    class CaptureTransport
        attr_reader :records
        attr_accessor :fail_on

        def initialize
            @records = []
            @attempts = 0
        end

        def SendDjson(source, payload)
            @attempts += 1
            return false if @attempts == @fail_on
            _id, schema, values = JSON.parse("[#{payload}]")
            @records << [source, Hash[schema.drop(1).map(&:first).zip(values)]]
            true
        end
    end

    class FaultyQuarantine < Fluent::MdsdChunkQuarantine
        attr_accessor :fail_write, :fail_file_sync, :fail_publish_sync
        attr_reader :write_attempts, :publish_sync_attempts

        def initialize(*args)
            super
            @write_attempts = @publish_sync_attempts = 0
        end

        private

        def write_synced(path, data)
            @write_attempts += 1
            raise Errno::EIO, "injected archive write failure" if @fail_write
            super
        end

        def sync_directory(path)
            if path == @directory && Dir.glob(File.join(path, "*", "manifest.json")).any?
                @publish_sync_attempts += 1
                raise Errno::EIO, "injected parent sync failure" if @fail_publish_sync
            end
            super
        end

        def sync_file(file)
            raise Errno::EIO, "injected file fsync failure" if @fail_file_sync
            super
        end
    end

    def setup
        Fluent::Test.setup
        @temporary = Dir.mktmpdir("mdsd-corruption-test-")
        @quarantine = File.join(@temporary, "quarantine")
        @outputs = []
        @capture = CaptureTransport.new
        system = Fluent::SystemConfig.new
        system.configure(Fluent::Config::Element.new("system", "", { "root_dir" => @temporary }, []))
        Fluent::Engine.init(system)
    end

    def teardown
        @outputs.each do |output|
            buffer = output.buffer
            next unless buffer
            [:stop, :before_shutdown, :shutdown, :after_shutdown, :close, :terminate].each do |method|
                buffer.public_send(method)
            end
        end
        FileUtils.remove_entry_secure(@temporary)
    end

    def output(options = {})
        action = options.fetch(:action, "quarantine")
        path = options.fetch(:path, @quarantine)
        id = options.fetch(:id, "corruption-test")
        source_time = options.fetch(:source_time, true)
        config = Fluent::Config.parse(<<-CONFIG, "corruption-test", "", true)
            #{id ? "@id #{id}" : ""}
            djsonsocket /tmp/unused-test.socket
            acktimeoutms 5000
            use_source_timestamp #{source_time}
            #{action ? "invalid_event_action #{action}" : ""}
            #{path ? "quarantine_path #{path}" : ""}
            buffer_type file
            buffer_path #{File.join(@temporary, "buffer-#{@outputs.length}")}
            flush_interval 1h
            flush_at_shutdown false
        CONFIG
        instance = Fluent::OutputMdsd.allocate
        Fluent::Compat::BufferedOutput.instance_method(:initialize).bind(instance).call
        Fluent::Compat::BufferedOutput.instance_method(:configure).bind(instance).call(config)
        @outputs << instance
        instance.__send__(:configure_chunk_quarantine, config)
        instance.instance_variable_set(:@mdsdMsgMaker, MdsdMsgMaker.new(instance.log, false))
        instance.instance_variable_set(:@mdsdTagPatterns, [])
        instance.instance_variable_set(:@configured_max_record_size, Fluent::OutputMdsd::MDSD_MAX_RECORD_SIZE)
        instance.instance_variable_set(:@mdsdLogger, @capture)
        instance.instance_variable_set(:@retry, nil)
        instance.instance_variable_set(:@retry_mutex, Mutex.new)
        instance.buffer.start
        instance
    end

    def event(instance, id, time = 1794000000, value = "payload")
        instance.format("k8scontainers.test", time, { "id" => id, "payload" => value })
    end

    def memory_chunk(data, count = 1, unique_id = nil)
        metadata = Fluent::Plugin::Buffer::Metadata.new(nil, nil, nil)
        chunk = Fluent::Plugin::Buffer::MemoryChunk.new(metadata)
        chunk.concat(data, count)
        chunk.commit
        chunk.instance_variable_set(:@unique_id, unique_id) if unique_id
        chunk.extend(Fluent::ChunkMessagePackEventStreamer)
        chunk
    end

    def queue(instance, data)
        metadata = instance.buffer.metadata
        instance.buffer.write({ metadata => [data] })
        instance.buffer.enqueue_all(true)
        instance.buffer.queue.first
    end

    def archives
        Dir.glob(File.join(@quarantine, "**", "manifest.json")).sort
    end

    def manifest
        assert_equal(1, archives.size)
        JSON.parse(File.read(archives.first))
    end

    def delivered_ids
        @capture.records.map { |_source, record| record["id"] }
    end

    def assert_raw_preserved(data)
        value = manifest
        raw = File.join(File.dirname(archives.first), "chunk.msgpack")
        assert_equal(data, File.binread(raw))
        assert_equal(Digest::SHA256.hexdigest(data), value["sha256"])
        assert_equal(data.bytesize, value["bytes"])
        assert_equal(0600, File.stat(raw).mode & 0777)
        assert_equal(0700, File.stat(File.dirname(raw)).mode & 0777)
        value
    end

    def test_valid_source_timestamps_and_payload_nuls
        instance = output
        times = [0, -1, 1794000000, 1794000000.25, Fluent::EventTime.new(1794000000, 123456789)]
        data = times.each_with_index.map { |time, id| event(instance, id, time, "valid\x00payload\x00") }.join
        decoded = []
        Fluent::MessagePackFactory.msgpack_unpacker.feed_each(data) { |item| decoded << item }
        instance.write(memory_chunk(data, times.length))
        assert_equal((0...times.length).to_a, delivered_ids)
        @capture.records.each_with_index do |(_source, record), id|
            expected = Time.at(decoded[id][1])
            assert_equal([expected.to_i, expected.nsec], record["FluentdIngestTimestamp"])
            assert_equal("valid\x00payload\x00", record["payload"])
        end
        assert_empty(archives)
    end

    def test_extension_eventtime_uses_the_fluentd_factory
        instance = output
        packer = Fluent::MessagePackFactory.msgpack_packer
        time = Fluent::EventTime.new(1794000000, 123456789)
        packer.pack(["k8scontainers.test", time, { "id" => 1 }])
        instance.write(memory_chunk(packer.full_pack))
        assert_equal([1794000000, 123456789], @capture.records.first[1]["FluentdIngestTimestamp"])
    end

    def test_default_preflight_backs_up_before_any_partial_delivery
        instance = output(:action => nil, :path => nil)
        data = event(instance, 1) + event(instance, 2, nil) + event(instance, 3)
        chunk = queue(instance, data)
        identity = chunk.unique_id.unpack("H*").first
        instance.try_flush
        assert_empty(@capture.records)
        assert_empty(instance.buffer.queue)
        backups = Dir.glob(File.join(@temporary, "backup", "**", "#{identity}.log"))
        assert_equal(1, backups.size)
        assert_equal(data, File.binread(backups.first))
        assert_empty(archives)
    end

    def test_default_error_is_explicit_and_does_not_expose_payload
        instance = output(:action => "backup", :path => nil)
        data = event(instance, 1) + event(instance, 2, "private-invalid-time")
        error = assert_raise(ArgumentError) { instance.write(memory_chunk(data)) }
        assert_match(/invalid_event_time at byte #{event(instance, 1).bytesize}/, error.message)
        assert_not_match(/private-invalid-time/, error.message)
        assert_match(/no events sent/, error.message)
        assert_empty(@capture.records)
    end

    def test_nil_time_is_retained_and_valid_records_on_both_sides_flush
        instance = output
        prefix = event(instance, 1)
        rejected = event(instance, 2, nil)
        data = prefix + rejected + event(instance, 3)
        queue(instance, data)
        instance.try_flush
        assert_equal([1, 3], delivered_ids)
        assert_empty(instance.buffer.queue)
        value = assert_raw_preserved(data)
        assert_equal("deliver_valid_events", value["plan"]["delivery_policy"])
        assert_equal(2, value["plan"]["deliverable_events"])
        assert_equal([{ "offset" => prefix.bytesize, "length" => rejected.bytesize,
                        "events" => 1, "reason" => "invalid_event_time" }], value["plan"]["rejected_ranges"])
    end

    def test_observed_nul_tails_preserve_valid_prefix_and_complete_flush
        [[75, 462], [12, 1515]].each do |count, padding|
            instance = output(:id => "observed-#{count}")
            prefix = count.times.map { |id| event(instance, id, 1794000000 + id) }.join
            data = prefix + "\x00" * padding
            before = @capture.records.length
            queue(instance, data)
            instance.try_flush
            assert_equal(count, @capture.records.length - before)
            assert_empty(instance.buffer.queue)
            entry = archives.map { |filename| JSON.parse(File.read(filename)) }.find { |item| item["plugin_id"] == "observed-#{count}" }
            assert_equal([{ "offset" => prefix.bytesize, "length" => padding,
                            "events" => padding, "reason" => "nul_tail" }], entry["plan"]["rejected_ranges"])
            assert_equal(count, entry["plan"]["deliverable_events"])
        end
        assert_equal(2, archives.size)
    end

    def test_middle_scalar_zero_does_not_hide_later_valid_records
        instance = output
        data = event(instance, 1) + "\x00" + event(instance, 2)
        instance.write(memory_chunk(data))
        assert_equal([1, 2], delivered_ids)
        assert_equal("invalid_event_shape", assert_raw_preserved(data)["plan"]["rejected_ranges"][0]["reason"])
    end

    def test_invalid_shapes_records_tags_and_times_are_isolated
        instance = output
        invalid = [
            1, nil, {}, ["tag"], ["tag", 1, {}, "extra"], [1, 1, {}],
            ["tag", 1, nil], ["tag", 1, { 3 => "non-string-key" }],
            ["tag", nil, {}], ["tag", "not-a-number", {}],
            ["tag", Float::NAN, {}], ["tag", Float::INFINITY, {}]
        ]
        data = invalid.map { |item| item.to_msgpack }.join + event(instance, 99)
        instance.write(memory_chunk(data))
        assert_equal([99], delivered_ids)
        value = assert_raw_preserved(data)
        assert_equal(invalid.length, value["plan"]["rejected_ranges"].sum { |range| range["events"] })
    end

    def test_undecodable_or_incomplete_framing_quarantines_everything
        ["\xc1".b, "\x93\xa3ab".b].each_with_index do |broken, index|
            instance = output(:id => "opaque-#{index}")
            data = event(instance, index) + broken
            instance.write(memory_chunk(data))
        end
        assert_empty(@capture.records)
        assert_equal(2, archives.size)
        archives.each do |filename|
            value = JSON.parse(File.read(filename))
            assert_equal("quarantine_entire_chunk", value["plan"]["delivery_policy"])
            assert_equal(0, value["plan"]["deliverable_events"])
            assert_equal(1, value["plan"]["validated_events"])
        end
    end

    def test_wallclock_mode_preserves_pairs_and_isolates_bad_records
        instance = output(:source_time => false)
        data = event(instance, 1, nil) + ["tag", nil].to_msgpack + event(instance, 3, nil)
        before = Time.now.to_f
        instance.write(memory_chunk(data))
        after = Time.now.to_f
        assert_equal([1, 3], delivered_ids)
        @capture.records.each do |_source, record|
            seconds, nanoseconds = record["FluentdIngestTimestamp"]
            actual = seconds + nanoseconds / 1000000000.0
            assert_operator(actual, :>=, before)
            assert_operator(actual, :<=, after)
        end
        assert_equal(false, assert_raw_preserved(data)["plan"]["use_source_timestamp"])
    end

    def test_replayed_chunks_do_not_guess_a_changed_timestamp_mode
        instance = output
        incompatible = ["k8scontainers.test", { "id" => "old-wallclock-mode" }].to_msgpack
        data = incompatible + event(instance, "valid")
        instance.write(memory_chunk(data))
        assert_equal(["valid"], delivered_ids)
        assert_equal("invalid_event_shape", assert_raw_preserved(data)["plan"]["rejected_ranges"][0]["reason"])
    end

    def test_quarantine_path_failure_retries_chunk_before_any_send
        File.write(@quarantine, "not-a-directory")
        instance = output
        data = event(instance, 1) + event(instance, 2, nil) + event(instance, 3)
        chunk = queue(instance, data)
        instance.try_flush
        assert_empty(@capture.records)
        assert_equal(1, instance.buffer.queue.length)
        assert_equal(data, chunk.read)
        assert_not_nil(instance.instance_variable_get(:@retry))
        assert_empty(Dir.glob(File.join(@temporary, "backup", "**", "*.log")))
        File.unlink(@quarantine)
        instance.try_flush
        assert_equal([1, 3], delivered_ids)
        assert_empty(instance.buffer.queue)
        assert_nil(instance.instance_variable_get(:@retry))
        assert_raw_preserved(data)
    end

    def test_file_write_failure_is_retryable_before_sending
        instance = output
        store = FaultyQuarantine.new(@quarantine, "corruption-test", 0)
        store.fail_write = true
        instance.instance_variable_set(:@chunk_quarantine, store)
        data = event(instance, 1) + event(instance, 2, nil)
        queue(instance, data)
        instance.try_flush
        assert_empty(@capture.records)
        assert_equal(1, instance.buffer.queue.length)
        assert_empty(archives)
        assert_empty(Dir.glob(File.join(@quarantine, "**", ".*-*")))
        store.fail_write = false
        instance.try_flush
        assert_equal([1], delivered_ids)
        assert_raw_preserved(data)
    end

    def test_file_fsync_failure_prevents_publication_and_delivery
        instance = output
        store = FaultyQuarantine.new(@quarantine, "corruption-test", 0)
        store.fail_file_sync = true
        instance.instance_variable_set(:@chunk_quarantine, store)
        data = event(instance, 1) + event(instance, 2, nil)
        queue(instance, data)
        instance.try_flush
        assert_empty(@capture.records)
        assert_equal(1, instance.buffer.queue.length)
        assert_empty(archives)
        store.fail_file_sync = false
        instance.try_flush
        assert_equal([1], delivered_ids)
        assert_raw_preserved(data)
    end

    def test_retry_resyncs_an_entry_published_before_parent_sync_failed
        instance = output
        store = FaultyQuarantine.new(@quarantine, "corruption-test", 0)
        store.fail_publish_sync = true
        instance.instance_variable_set(:@chunk_quarantine, store)
        data = event(instance, 1) + event(instance, 2, nil)
        queue(instance, data)
        instance.try_flush
        assert_empty(@capture.records)
        assert_equal(1, archives.size)
        assert_equal(1, instance.buffer.queue.length)
        writes = store.write_attempts
        store.fail_publish_sync = false
        instance.try_flush
        assert_equal([1], delivered_ids)
        assert_equal(writes, store.write_attempts)
        assert_operator(store.publish_sync_attempts, :>=, 2)
        assert_raw_preserved(data)
    end

    def test_archive_conflict_never_overwrites_or_sends
        instance = output
        data = event(instance, 1) + event(instance, 2, nil)
        chunk = memory_chunk(data)
        instance.write(chunk)
        @capture.records.clear
        raw = File.join(File.dirname(archives.first), "chunk.msgpack")
        File.write(raw, "damaged")
        error = assert_raise(Fluent::MdsdChunkQuarantine::PersistenceError) { instance.write(chunk) }
        assert_match(/conflicts/, error.message)
        assert_empty(@capture.records)
        assert_equal("damaged", File.read(raw))
    end

    def test_quarantine_is_idempotent_when_transport_retries_valid_records
        instance = output
        @capture.fail_on = 2
        data = event(instance, 1) + event(instance, 2, nil) + event(instance, 3)
        queue(instance, data)
        instance.try_flush
        assert_equal([1], delivered_ids)
        assert_equal(1, instance.buffer.queue.length)
        assert_raw_preserved(data)
        original_manifest = File.binread(archives.first)
        instance.try_flush
        assert_equal([1, 1, 3], delivered_ids)
        assert_empty(instance.buffer.queue)
        assert_equal(1, archives.size)
        assert_equal(original_manifest, File.binread(archives.first))
    end

    def test_too_many_ranges_retains_every_byte_without_guessing
        instance = output
        data = (["bad", nil, {}].to_msgpack + event(instance, 1)) * 1025
        instance.write(memory_chunk(data))
        assert_empty(@capture.records)
        value = assert_raw_preserved(data)
        assert_equal("quarantine_entire_chunk", value["plan"]["delivery_policy"])
        assert_equal("validation_range_limit", value["plan"]["rejected_ranges"].last["reason"])
    end

    def test_rejected_range_limit_applies_at_chunk_end_and_before_nul_tails
        instance = output
        invalid = ["bad", nil, {}].to_msgpack
        valid = event(instance, 1)
        prefix = (invalid + valid) * (Fluent::MdsdChunkPlan::MAX_REJECTED_RANGES - 1)
        [invalid, "\x00", invalid + valid + "\x00"].each do |ending|
            plan = Fluent::MdsdChunkPlan.new(prefix + ending, true)
            assert_equal("quarantine_entire_chunk", plan.manifest["delivery_policy"])
            assert_equal(0, plan.deliverable_events)
            delivered = []
            plan.each_event { |item| delivered << item }
            assert_empty(delivered)
        end
    end

    def test_rejected_ranges_below_limit_still_deliver_valid_events
        instance = output
        count = Fluent::MdsdChunkPlan::MAX_REJECTED_RANGES - 1
        data = (["bad", nil, {}].to_msgpack + event(instance, 1)) * count
        instance.write(memory_chunk(data))
        assert_equal(count, @capture.records.length)
        value = assert_raw_preserved(data)
        assert_equal("deliver_valid_events", value["plan"]["delivery_policy"])
        assert_equal(count, value["plan"]["deliverable_events"])
        assert_equal(count, value["plan"]["rejected_ranges"].length)
    end

    def test_manifest_ranges_partition_the_exact_messagepack_stream
        instance = output
        valid_ids = []
        data = 100.times.map do |index|
            if index % 4 == 0
                ["invalid", nil, {}].to_msgpack
            elsif index % 7 == 0
                0.to_msgpack
            else
                valid_ids << index
                event(instance, index, 1794000000 + index, "inside\x00value")
            end
        end.join
        plan = Fluent::MdsdChunkPlan.new(data, true)
        ranges = (plan.manifest["delivery_ranges"] + plan.rejected_ranges).sort_by { |range| range["offset"] }
        position = 0
        ranges.each do |range|
            assert_equal(position, range["offset"])
            position += range["length"]
        end
        assert_equal(data.bytesize, position)
        delivered = []
        plan.each_event { |item| delivered << item[2]["id"] }
        assert_equal(valid_ids, delivered)
    end

    def test_empty_chunk_has_no_quarantine_or_delivery
        instance = output
        instance.write(memory_chunk(""))
        assert_empty(@capture.records)
        assert_empty(archives)
    end

    def test_large_nul_tail_is_one_range_not_millions_of_rejections
        instance = output
        padding = 10 * 1024 * 1024
        data = event(instance, 1) + "\x00" * padding
        instance.write(memory_chunk(data))
        assert_equal([1], delivered_ids)
        value = assert_raw_preserved(data)
        assert_equal(1, value["plan"]["rejected_ranges"].size)
        assert_equal(padding, value["plan"]["rejected_ranges"][0]["length"])
    end

    def test_all_zero_chunk_is_preserved_without_inventing_an_event
        instance = output
        data = "\x00" * 4096
        instance.write(memory_chunk(data))
        assert_empty(@capture.records)
        value = assert_raw_preserved(data)
        assert_equal(0, value["plan"]["deliverable_events"])
        assert_equal("nul_tail", value["plan"]["rejected_ranges"][0]["reason"])
    end

    def test_concurrent_retries_publish_one_identical_archive
        stores = 2.times.map { Fluent::MdsdChunkQuarantine.new(@quarantine, "parallel-test", 0) }
        instance = output
        data = event(instance, 1) + event(instance, 2, nil)
        plan = Fluent::MdsdChunkPlan.new(data, true)
        identity = memory_chunk(data).unique_id
        threads = 4.times.map { |index| Thread.new { stores[index % stores.length].persist(identity, plan) } }
        paths = threads.map(&:value)
        assert_equal(1, paths.uniq.size)
        assert_raw_preserved(data)
    end

    def test_gzip_chunk_quarantines_exact_uncompressed_event_bytes
        instance = output
        data = event(instance, 1) + event(instance, 2, nil) + event(instance, 3)
        metadata = Fluent::Plugin::Buffer::Metadata.new(nil, nil, nil)
        chunk = Fluent::Plugin::Buffer::MemoryChunk.new(metadata, :compress => :gzip)
        chunk.append([data], :compress => :gzip)
        chunk.commit
        instance.write(chunk)
        assert_equal([1, 3], delivered_ids)
        assert_raw_preserved(data)
    end

    def test_timestamp_mode_changes_cannot_reuse_a_different_manifest
        store = Fluent::MdsdChunkQuarantine.new(@quarantine, "corruption-test", 0)
        data = "\x00"
        identity = memory_chunk(data).unique_id
        store.persist(identity, Fluent::MdsdChunkPlan.new(data, true))
        assert_raise(Fluent::MdsdChunkQuarantine::PersistenceError) do
            store.persist(identity, Fluent::MdsdChunkPlan.new(data, false))
        end
        assert_equal(true, manifest["plan"]["use_source_timestamp"])
    end

    def test_configuration_requires_explicit_id_and_absolute_quarantine_path
        assert_raise(Fluent::ConfigError) { output(:path => nil) }
        assert_raise(Fluent::ConfigError) { output(:path => "relative-path") }
        assert_raise(Fluent::ConfigError) { output(:id => nil) }
        assert_raise(Fluent::ConfigError) { output(:action => "backup") }
    end

    def test_quarantine_parent_symlink_is_not_followed
        actual = File.join(@temporary, "actual")
        Dir.mkdir(actual)
        File.symlink(actual, @quarantine)
        instance = output
        data = event(instance, 1) + event(instance, 2, nil)
        assert_raise(Fluent::MdsdChunkQuarantine::PersistenceError) { instance.write(memory_chunk(data)) }
        assert_empty(@capture.records)
        assert_empty(Dir.children(actual))
    end
end
