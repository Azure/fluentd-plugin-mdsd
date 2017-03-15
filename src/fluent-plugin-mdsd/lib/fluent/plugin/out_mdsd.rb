# This is Linux MDS/Geneva monitoring agent (mdsd) output plugin.
# The plugin will send data to mdsd agent using Unix socket file.


module Fluent

    class OutputMdsd < BufferedOutput
        Plugin.register_output('mdsd', self)

        def initialize()
            super
            require_relative 'Liboutmdsdrb'

            @mdsdMsgMaker = nil
            @mdsdLogger = nil
        end

        desc 'full path to mdsd djson socket file'
        config_param :djsonsocket, :string
        desc 'if no ack is received from mdsd after N milli-seconds, drop msg.'
        config_param :acktimeoutms, :integer
        desc 'Fluentd tag prefix that will be used as mdsd source name'
        config_param :mdsd_syslog_tag_prefix, :string, :default => nil

        # This method is called before starting.
        def configure(conf)
            super

            Liboutmdsdrb::InitLogger($log.out.path, true)
            Liboutmdsdrb::SetLogLevel(log_level)

            @mdsdMsgMaker = MdsdMsgMaker.new(@log)
            @mdsdLogger = Liboutmdsdrb::SocketLogger.new(djsonsocket, acktimeoutms, 30000, 60000)
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
        # NOTE: a plugin must define this because base class doesn't have
        # default implementation.
        def format(tag, time, record)
            [tag, record].to_msgpack
        end

        # This method is called every flush interval.
        #
        # NOTE! This method is called by internal thread, not Fluentd's main thread.
        # So IO wait doesn't affect other plugins.
        def write(chunk)
          chunk.msgpack_each {|(tag, record)|
            handle_record(tag, record)
          }
          @log.flush
        end

        def handle_record(tag, record)
            mdsdSource = @mdsdMsgMaker.create_mdsd_source(tag)
            dataStr = @mdsdMsgMaker.get_schema_value_str(record)
            if not @mdsdLogger.SendDjson(mdsdSource, dataStr)
                raise "Sending data (tag=#{tag}) to mdsd failed"
            end
            @log.trace "source='#{mdsdSource}', data='#{dataStr}'"
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
        # NOTE: only supported types are listed here. Other types are not supported.
        @@rb2mdsdType = 
        {
            "TrueClass" => "FT_BOOL",
            "FalseClass" => "FT_BOOL",
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

        record.each { |key, value|
            rb_typestr = value.class.name
            mdsd_typestr = @@rb2mdsdType[rb_typestr]
            if !mdsd_typestr
                @logger.error "Error: unsupported Ruby type #{rb_typestr}.\n"
                return nil
            end

            if (schema_str == "")
                schema_str << "["
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
    def initialize(logger)
        @logger = logger
        @schema_mgr = SchemaManager.new(@logger)
    end

    # Input: ruby tag and ruby record
    # Output: a string for mdsd djson including schema info and actual data.
    # Example output:
    # 3,[["timestamp","FT_TIME"],["message","FT_STRING"]],[[1477777,542323],"This is syslog msg"]]
    #
    def get_schema_value_str(record)
        resultStr = ""

        schema_obj = @schema_mgr.get_schema_info(record)
        resultStr << schema_obj[0].to_s << "," << schema_obj[1] << ","
        resultStr << get_record_values(record)

        return resultStr
    end

    # If configured, convert (unify) fluentd tags to a unified tag (prefix) so that mdsd
    # sees only one single tag (unique source name) for all syslog messages. This is
    # the use case for basic syslog messages collection. Also, it appears
    # that tags can't be changed by a fluentd filter, so they need to be
    # changed here in this output plugin.
    def create_mdsd_source(tag)
        if @mdsd_syslog_tag_prefix and tag.start_with?(@mdsd_syslog_tag_prefix)
            return @mdsd_syslog_tag_prefix
        end
        return tag
    end

    private

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

    def get_value_by_type(value)
        if (value.kind_of? String)
            return ('"' + value + '"')
        elsif (value.kind_of? Time)
            return ('[' + value.tv_sec.to_s + "," + value.tv_nsec.to_s + ']')
        else
            return value.to_s
        end
    end
end
