# This is Linux MDS/Geneva monitoring agent (mdsd) output plugin.
# The plugin will send data to mdsd agent using Unix socket file.

require "json"
require "json/ext"

module Fluent

    class OutputMdsd < BufferedOutput
        Plugin.register_output('mdsd', self)

        MDSD_MAX_RECORD_SIZE = 128 * 1024-1

        def initialize()
            super
            require_relative 'Liboutmdsdrb'

            @mdsdMsgMaker = nil
            @mdsdLogger = nil
            @mdsdTagPrefix = nil
        end

        desc 'full path to mdsd djson socket file'
        config_param :djsonsocket, :string
        desc 'if no ack is received from mdsd after N milliseconds, drop msg.'
        config_param :acktimeoutms, :integer
        desc 'Fluentd tag regex patterns whose match (if any) will be used as mdsd source name'
        config_param :mdsd_tag_regex_patterns, :array, :default => []
        desc 'message resend interval in milliseconds'
        config_param :resend_interval_ms, :integer, :default => 30000
        desc 'timeout in millisecond when connecting to djsonsocket'
        config_param :conn_retry_timeout_ms, :integer, :default => 60000
        desc 'the field name for the event emit time stamp'
        config_param :emit_timestamp_name, :string, :default => "FluentdIngestTimestamp"
        desc "the timestamp to use for records sent to mdsd"
        config_param :use_source_timestamp, :bool, :default => true
        desc "the maximum record size to emit. Cannot exceed #{MDSD_MAX_RECORD_SIZE}"
        config_param :max_record_size, :integer, :default => MDSD_MAX_RECORD_SIZE
        desc "convert hash type to json string"
        config_param :convert_hash_to_json, :bool, :default => false

        # This method is called before starting.
        def configure(conf)
            super

            Liboutmdsdrb::InitLogger($log.out.path, true)
            Liboutmdsdrb::SetLogLevel($log.level.to_s)

            @mdsdMsgMaker = MdsdMsgMaker.new(@log, convert_hash_to_json)
            @mdsdLogger = Liboutmdsdrb::SocketLogger.new(djsonsocket, acktimeoutms,
                resend_interval_ms, conn_retry_timeout_ms)
            @mdsdTagPatterns = mdsd_tag_regex_patterns
            @configured_max_record_size = [max_record_size, MDSD_MAX_RECORD_SIZE].min
        end

        # This method is called before starting.
        def start()
            super
        end

        # This method is called when shutting down.
        def shutdown()
            super
        end

        # This method is called when an event reaches to Fluentd.
        # time: this is an integer, Unix time_t. Number of seconds since 1/1/1970 UTC.
        # NOTE: a plugin must define this because base class doesn't have
        # default implementation.
        def format(tag, time, record)
            if use_source_timestamp
                [tag, time, record].to_msgpack
            else
                [tag, record].to_msgpack
            end
        end

        # This method is called every flush interval.
        #
        # NOTE! This method is called by internal thread, not Fluentd's main thread.
        # So IO wait doesn't affect other plugins.
        def write(chunk)
            if use_source_timestamp
                chunk.msgpack_each {|(tag, time, record)|
                    # Ruby (version >= 1.9) hash preserves insertion order. So the following item is
                    # the last item when iterating the 'record' hash.
                    record[emit_timestamp_name] = Time.at(time)
                    handle_record(tag, record)
                }
            else
                chunk.msgpack_each {|(tag, record)|
                    record[emit_timestamp_name] = Time.now
                    handle_record(tag, record)
                }
            end
            @log.flush
        end

private
        # Handle a regular record, which is hash of key, value pairs.
        # NOTE: not all types are supported. The supported data types are
        # defined in SchemaManager class.
        def handle_record(tag, record)
            mdsdSource = @mdsdMsgMaker.create_mdsd_source(tag, @mdsdTagPatterns)
            dataStr = @mdsdMsgMaker.get_schema_value_str(record)
            return if record_too_large?(dataStr, mdsdSource)
            if not @mdsdLogger.SendDjson(mdsdSource, dataStr)
                raise "Sending data (tag=#{tag}) to mdsd failed"
            end
            @log.trace "source='#{mdsdSource}', data='#{dataStr}'"
        end

        def record_too_large?(dataStr, mdsdSource)
            if dataStr.length > @configured_max_record_size
                @log.warn "Dropping too large record to mdsd with size=#{dataStr.length}, source='#{mdsdSource}"
                true
            else
                false
            end
        end
    end # class OutputMdsd
end # module Fluent

class SchemaManager

    def initialize(logger)
        @logger = logger

        # schemahash contains all known schemas.
        # key: a string-join of the (json-key, value-type-name) pairs
        # value: a two-element array: [SchemaId, SchemaString]
        # NOTE: SchemaId must be unique in the hash
        @schemaHash = {}

        # key: Ruby object type
        # value: mdsd schema type
        # NOTE: for other data types that are not included here, they'll be treated as string.
        @@rb2mdsdType =
        {
            "TrueClass" => "FT_BOOL",
            "FalseClass" => "FT_BOOL",
            "Integer" => "FT_INT64",
            "Fixnum" => "FT_INT64",
            "Bignum" => "FT_INT64",
            "Float" => "FT_DOUBLE",
            "Time" => "FT_TIME",
            "String" => "FT_STRING"
        }

        @schema_id = 0
        @schema_id_mutex = Mutex.new  # To protect schema_id and make sure it is unique.
    end

    # Return a two-element array [SchemaId, SchemaString]
    def get_schema_info(record)
        hashkey = get_schema_key(record)
        value = @schemaHash[hashkey]
        if value
            return value
        else
            new_schema_id = get_new_schema_id()
            new_schema_str = get_new_schema(record)
            new_schema = [new_schema_id, new_schema_str]

            @schemaHash[hashkey] = new_schema
            return new_schema
        end
    end

    def size()
        return @schemaHash.size()
    end

    private

    def get_schema_key(record)
        key_str = ""
        record.each { |key, value|
            key_str << key << "," << value.class.name << ","
        }
        return key_str
    end

    def get_new_schema(record)
        schema_str = ""

        # the last element is the precise time stamp field
        time_field_index = record.size() - 1
        record.each { |key, value|
            rb_typestr = value.class.name
            mdsd_typestr = @@rb2mdsdType[rb_typestr]
            if !mdsd_typestr
                mdsd_typestr = "FT_STRING"
            end

            if (schema_str == "")
                schema_str << "[" << time_field_index.to_s << ","
            else
                schema_str << ","
            end
            schema_str << '["' << key << '","' << mdsd_typestr << '"]'
        }
        schema_str << "]"

        return schema_str
    end

    def get_new_schema_id()
        @schema_id_mutex.synchronize do
            @schema_id += 1
            return @schema_id
        end
    end
end

class MdsdMsgMaker
    def initialize(logger, hashtojson)
        @hashtojson = hashtojson
        @logger = logger
        @schema_mgr = SchemaManager.new(@logger)
    end

    # Input: ruby tag and ruby record
    # Output: a string for mdsd djson including schema info and actual data.
    # Example output:
    # 3,[1,["message","FT_STRING"],["EmitTimestamp","FT_TIME"]],["This is syslog msg",[1493671442,0]]]
    #
    def get_schema_value_str(record)
        resultStr = ""
        schema_obj = @schema_mgr.get_schema_info(record)
        resultStr << schema_obj[0].to_s << "," << schema_obj[1] << ","
        resultStr << get_record_values(record)

        return resultStr
    end

    # If configured, convert (unify) fluentd tags to a unified tag (regex match) so that mdsd
    # sees only one single tag (unique source name) for all the matched fluentd tags. Basic
    # syslog use case is OK with a prefix match (e.g., mdsd.syslog.** to mdsd.syslog), but
    # extended syslog use case needs a pattern matching (e.g., mdsd.syslog.user.info to
    # mdsd.syslog.user, mdsd.syslog.local1.err to mdsd.syslog.local1 : This can be
    # expressed as regex "^mdsd\.syslog\.\w+" and the match will be returned as the
    # corresponding mdsd source name.
    def create_mdsd_source(tag, regex_list)
        regex_list.each { |regex|
            match = tag.match /#{regex}/
            if match
                return match[0]
            end
        }
        return tag
    end

    def get_record_values(record)
        resultStr = ""

        record.each { |key, value|
            if resultStr == ""
                resultStr << "["
            else
                resultStr << ","
            end
            resultStr << get_value_by_type(value)
        }
        resultStr << "]"

        return resultStr
    end


    # Get formatted value string accepted by mdsd dynamic json protocol.
    def get_value_by_type(value)
        
        if (value.kind_of? String)
            # Use JSON.generate for proper JSON escaping, especially for control characters
            return JSON.generate(value)
        elsif (value.kind_of? Array) || (value.kind_of? Hash) || (value.kind_of? Range)
            # Treat data structure as a string. Use JSON.generate for proper JSON escaping.
            # If type is json and hashtojson is set to true, then first convert it to json and then use JSON.generate for proper escape.
            if (@hashtojson) && (value.kind_of? Hash)
                return JSON.generate(value.to_json.to_s)
            else
                return JSON.generate(value.to_s)
            end
        elsif (value.kind_of? Time)
            return ('[' + value.tv_sec.to_s + "," + value.tv_nsec.to_s + ']')
        elsif value.nil?
            return JSON.generate("null")
        else
            # puts "Unknown type value:'#{value}'"
            return value.to_s
        end
    end
end
