require "msgpack"

module Fluent
    class MdsdChunkPlan
        MAX_REJECTED_RANGES = 1024

        attr_reader :data, :rejected_ranges, :valid_events

        def initialize(data, use_source_timestamp)
            @data = data.dup.force_encoding(Encoding::BINARY).freeze
            @use_source_timestamp = use_source_timestamp
            @rejected_ranges = []
            @delivery_ranges = []
            @valid_events = 0
            @opaque = false
            scan
        end

        def invalid?
            !@rejected_ranges.empty?
        end

        def deliverable_events
            @opaque ? 0 : @valid_events
        end

        def manifest
            {
                "use_source_timestamp" => @use_source_timestamp,
                "delivery_policy" => @opaque ? "quarantine_entire_chunk" : "deliver_valid_events",
                "validated_events" => @valid_events,
                "deliverable_events" => deliverable_events,
                "delivery_ranges" => @opaque ? [] : @delivery_ranges,
                "rejected_ranges" => @rejected_ranges
            }
        end

        def each_event
            return if @opaque
            @delivery_ranges.each do |range|
                decoder = unpacker
                decoder.feed(@data.byteslice(range["offset"], range["length"]))
                range["events"].times { yield decoder.read }
            end
        end

        private

        def unpacker
            if defined?(Fluent::MessagePackFactory)
                Fluent::MessagePackFactory.msgpack_unpacker
            else
                MessagePack::Unpacker.new
            end
        end

        def scan
            decoder = unpacker
            decoder.feed(@data)
            offset = 0
            suffix = /\x00+\z/n.match(@data)
            nul_start = suffix && suffix.begin(0)
            while offset < @data.bytesize
                if @rejected_ranges.length >= MAX_REJECTED_RANGES
                    @opaque = true
                    reject_range(offset, @data.bytesize - offset, "validation_range_limit", nil)
                    break
                end
                if nul_start && offset >= nul_start
                    reject_range(offset, @data.bytesize - offset, "nul_tail", @data.bytesize - offset)
                    break
                end
                begin
                    event = decoder.read
                rescue EOFError
                    @opaque = true
                    reject_range(offset, @data.bytesize - offset, "incomplete_messagepack", nil)
                    break
                rescue MessagePack::UnpackError
                    @opaque = true
                    reject_range(offset, @data.bytesize - offset, "undecodable_messagepack", nil)
                    break
                end
                ending = @data.bytesize - decoder.buffer.size
                raise "MDSD chunk decoder made no progress" unless ending > offset
                reason = invalid_event_reason(event)
                if reason
                    reject_range(offset, ending - offset, reason, 1)
                else
                    append_range(@delivery_ranges, offset, ending - offset, 1)
                    @valid_events += 1
                end
                offset = ending
            end
            @opaque = true if @rejected_ranges.length >= MAX_REJECTED_RANGES
        end

        def invalid_event_reason(event)
            expected_size = @use_source_timestamp ? 3 : 2
            return "invalid_event_shape" unless event.is_a?(Array) && event.length == expected_size
            return "invalid_tag" unless event[0].is_a?(String)
            record = event[-1]
            return "invalid_record" unless record.is_a?(Hash) && record.keys.all? { |key| key.is_a?(String) }
            return nil unless @use_source_timestamp
            time = event[1]
            supported = time.is_a?(Integer) || time.is_a?(Float) || time.is_a?(Time) ||
                (defined?(Fluent::EventTime) && time.is_a?(Fluent::EventTime))
            return "invalid_event_time" unless supported
            begin
                Time.at(time)
            rescue TypeError, ArgumentError, RangeError
                return "invalid_event_time"
            end
            nil
        end

        def append_range(ranges, offset, length, events, reason = nil)
            previous = ranges.last
            if previous && previous["offset"] + previous["length"] == offset &&
                    previous["reason"] == reason && !events.nil? && !previous["events"].nil?
                previous["length"] += length
                previous["events"] += events
            else
                range = { "offset" => offset, "length" => length, "events" => events }
                range["reason"] = reason if reason
                ranges << range
            end
        end

        def reject_range(offset, length, reason, events)
            append_range(@rejected_ranges, offset, length, events, reason)
        end
    end
end
