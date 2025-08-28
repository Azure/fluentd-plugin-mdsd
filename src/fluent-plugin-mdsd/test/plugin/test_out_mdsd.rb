require "test/unit"
require "fluent/test"
require "fluent/plugin/out_mdsd"

module Fluent
    module Test
        class DummyLogDevice
            def path()
                return Dir.getwd() + "/test_out_mdsd.log"
            end
        end
    end
end


class OutputMdsdTest < Test::Unit::TestCase
    def setup
        Fluent::Test.setup
    end

    def teardown
    end

    CONFIG1 = %[
        log_level trace
        djsonsocket /tmp/mytestsocket
        acktimeoutms 1
        mdsd_tag_regex_patterns [ "^mdsd\\.syslog" ]
        emit_timestamp_name testtimestamp
    ]

    CONFIG2 = %[
        log_level trace
        djsonsocket /tmp/mytestsocket
        acktimeoutms 1
        mdsd_tag_regex_patterns [ "^mdsd\\.syslog" ]
        resend_interval_ms 30
        conn_retry_timeout_ms 60
    ]

    def create_driver(conf = CONFIG1)
        Fluent::Test::BufferedOutputTestDriver.new(Fluent::OutputMdsd).configure(conf)
    end

    def test_configure()
        d = create_driver
        
        assert_equal("/tmp/mytestsocket", d.instance.djsonsocket, "djsonsocket")
        assert_equal(1, d.instance.acktimeoutms, "acktimeoutms")
        assert_equal([ "^mdsd.syslog" ], d.instance.mdsd_tag_regex_patterns, "mdsd_tag_regex_patterns")
        assert_equal("testtimestamp", d.instance.emit_timestamp_name, "emit_timestamp_name")
    end

    def test_write_with_good_socket()
        d = create_driver
        time = Time.parse("2011-01-02 13:14:15 UTC").to_i

        record_list = [
            {"n" => 1},
            {"s" => "str"}
        ]

        sock_server_cmdline = Dir.getwd() + "/../mockserver -u " + d.instance.djsonsocket
        sock_server_pid = spawn (sock_server_cmdline)

        record_list.each { |x|
            d.emit(x, time)
        }

        actual_data = d.run

        Process.kill('KILL', sock_server_pid)

        expected_data = true
        assert_equal(expected_data, actual_data, "OutputMdsd.write() with good socket")

    end

    def test_write_with_bad_socket()
        d = create_driver(CONFIG2)
        time = Time.parse("2011-01-02 13:14:15 UTC").to_i

        record_list = [
            {"s" => "bad socket"}
        ]

        record_list.each { |x|
            d.emit(x, time)
        }

        actualException = false
        begin
            actual_data = d.run
        rescue RuntimeError => ex
            puts "\nReceived RuntimeError: " + ex.message
            actualException = true
        end

        assert_equal(true, actualException, "OutputMdsd.write() with bad socket")
    end
end


class SchemaMgrTest < Test::Unit::TestCase
    # setup test will run before each test
    # set up test must named as 'setup'
    def setup
        @schema_mgr = SchemaManager.new(nil)
    end

    # unit test function must starts with 'test_'
    def test_basic()
        record = 
        {
            "truekey" => true,
            "falsekey" => false,
            "intkey" => 1234,
            "floatkey" => 3.14,
            "strkey" => "teststring",
            "arraykey" => [{'a'=>1}],
            "hashkey" => {1=>2},
            "rangekey" => (1...3),
            "timekey" => Time.now,
        }

        schema_obj = @schema_mgr.get_schema_info(record)
        assert_equal(1, schema_obj[0], "first schema id")

        expected_str = '[8,["truekey","FT_BOOL"],["falsekey","FT_BOOL"],["intkey","FT_INT64"],["floatkey","FT_DOUBLE"],["strkey","FT_STRING"],["arraykey","FT_STRING"],["hashkey","FT_STRING"],["rangekey","FT_STRING"],["timekey","FT_TIME"]]'
        assert_equal(expected_str, schema_obj[1], "schema string")
        puts "schema_str: '#{schema_obj[1]}'"
    end

    def test_dup_records()
        record =
        {
            "strkey" => "teststring",
            "intkey" => 1234
        }

        schema_obj1 = @schema_mgr.get_schema_info(record)
        schema_obj2 = @schema_mgr.get_schema_info(record)

        assert_equal(1, schema_obj1[0], "second schema id")
        assert_equal('[1,["strkey","FT_STRING"],["intkey","FT_INT64"]]', schema_obj1[1], "schema string")

        assert_equal(schema_obj1, schema_obj2, "dup record schema")
        assert_equal(1, @schema_mgr.size(), "schema mgr size")
    end

    def test_multi_records()
        nrecords = 100
        1.upto(nrecords) { |n|
            tmpstr = "intkey-" + n.to_s()
            record = { tmpstr => n}
            schema_obj = @schema_mgr.get_schema_info(record)
            assert_equal(n, schema_obj[0], "schema id #{n}")

            expected_str = '[0,["' + tmpstr + '","FT_INT64"]]'
            assert_equal(expected_str, schema_obj[1], "schema str #{n}")
        }

        assert_equal(nrecords, @schema_mgr.size(), "schema mgr size")
    end
end


class MdsdMsgMakerTest < Test::Unit::TestCase

    def setup
        @msg_maker = MdsdMsgMaker.new(nil, false)
    end

    def test_record()
        record1 = 
        {
            "truekey" => true,
            "falsekey" => false,
            "intkey" => 1234,
            "floatkey" => 3.14,
            "timekey" => Time.at(1478626843,878947),
            "strkey" => "teststring"
        }

        schemaVal = @msg_maker.get_schema_value_str(record1)
        expectedStr = '1,[5,["truekey","FT_BOOL"],["falsekey","FT_BOOL"],["intkey","FT_INT64"],["floatkey","FT_DOUBLE"],["timekey","FT_TIME"],["strkey","FT_STRING"]],[true,false,1234,3.14,[1478626843,878947000],"teststring"]'
        assert_equal(expectedStr, schemaVal, "get_schema_value_str")
    end

    def test_quote_escape()
        record =
        {
            "dquotekey" => "A\"B",
            "squotekey" => 'C"D',
            "arraykey" => ["a"]
        }

        schemaVal = @msg_maker.get_schema_value_str(record)
        expectedStr = '1,[2,["dquotekey","FT_STRING"],["squotekey","FT_STRING"],["arraykey","FT_STRING"]],["A\"B","C\"D","[\"a\"]"]'
        assert_equal(expectedStr, schemaVal, "get_schema_value_str")
    end

    def test_datastructures()
        record =
        {
            "arraykey" => [1],
            "hashkey" => {1=>2},
            "rangekey" => (1...4),
            "emittime" => Time.at(123)
        }

        schemaVal = @msg_maker.get_schema_value_str(record)
        expectedStr = '1,[3,["arraykey","FT_STRING"],["hashkey","FT_STRING"],["rangekey","FT_STRING"],["emittime","FT_TIME"]],["[1]","{1=>2}","1...4",[123,0]]'
        assert_equal(expectedStr, schemaVal, "get_schema_value_str")
    end

    def test_HashType()
        @new_msgmkr = MdsdMsgMaker.new(nil, true)
        record = 
        {
            "properties" => {"hello"=>"there"}
        }

        returnVal = @new_msgmkr.get_schema_value_str(record)
        puts "returned value '#{returnVal}'"  
        expectedStr = '1,[0,["properties","FT_STRING"]],["{\"hello\":\"there\"}"]'
        assert_equal(expectedStr, returnVal, "get_schema_value_str")  

        # send it to msgmkr object with hash_to_json set to false
        returnVal = @msg_maker.get_schema_value_str(record)
        expectedStr = '1,[0,["properties","FT_STRING"]],["{\"hello\"=>\"there\"}"]'
        assert_equal(expectedStr, returnVal, "get_schema_value_str")  
    end
        

    def test_source_name_creator()
        regex_list = [ "^mdsd\.syslog", "^mdsd\.ext_syslog\.\\w+" ]
        test_data = {
            "mdsd.syslog.user.info" => "mdsd.syslog",
            "mdsd.ext_syslog.local1.err" => "mdsd.ext_syslog.local1",
            "mdsd.ext_syslog.syslog.crit" => "mdsd.ext_syslog.syslog",
            "xmdsd.syslog.user.info" => "xmdsd.syslog.user.info"
        }
        test_data.each { |tag, expected_source_name|
            actual_source_name = @msg_maker.create_mdsd_source(tag, regex_list)
            assert_equal(expected_source_name, actual_source_name, "Invalid source name returned")
        }
        test_data2 = [ "any.tag.1", "any.tag.2" ]
        test_data2.each { |tag|
            actual_source_name = @msg_maker.create_mdsd_source(tag, [])
            assert_equal(tag, actual_source_name, "Invalid source name returned")
        }
    end

    def test_get_value_by_type()
        test_data = {
            "stringdata" => "\"stringdata\"",
            [1, 2] => "\"[1, 2]\"",
            Time.at(100) => "[100,0]",
            nil => "\"null\"",
            123 => "123"
        }

        test_data.each { |input, expected_output|
            actual_output = @msg_maker.get_value_by_type(input)
            assert_equal(expected_output, actual_output, "get_value_by_type")
        }
    end

    def test_control_characters_escaping()
        # Test that control characters are properly escaped for round-trip JSON parsing
        # This test demonstrates the bug where Ruby's dump method doesn't properly escape
        # control characters for JSON parsing, causing data corruption downstream
        
        test_cases = [
            {
                name: "null_byte",
                input: "hello\x00world",
                description: "null byte should be escaped as \\u0000"
            },
            {
                name: "control_chars",
                input: "data\x01\x02\x03end",
                description: "control characters should use unicode escapes"
            },
            {
                name: "vertical_tab",
                input: "hello\x0Bworld",
                description: "vertical tab should be escaped as \\u000b"
            },
            {
                name: "escape_char",
                input: "hello\x1Bworld",
                description: "escape character should be escaped as \\u001b"
            }
        ]

        test_cases.each do |test_case|
            record = { test_case[:name] => test_case[:input] }
            schema_val = @msg_maker.get_schema_value_str(record)
            
            # Extract the values part: schema_id,schema,[values]
            # The format is: 1,[0,["key","FT_STRING"]],["value"]
            # We want just the last part: ["value"]
            last_comma_bracket = schema_val.rindex(',[')
            assert_not_nil(last_comma_bracket, "Should find values array for #{test_case[:name]}")
            
            values_part = schema_val[last_comma_bracket + 1..-1]  # Skip the comma, keep the bracket
            
            # Parse the JSON to verify round-trip works correctly
            begin
                parsed = JSON.parse(values_part)
                parsed_value = parsed[0]
                
                # This is the critical test: the parsed value must exactly match the original
                assert_equal(test_case[:input], parsed_value,
                    "Round-trip failed for #{test_case[:name]}: #{test_case[:description]}. " +
                    "Original: #{test_case[:input].inspect}, Parsed: #{parsed_value.inspect}")
                    
            rescue JSON::ParserError => e
                flunk("JSON parsing failed for #{test_case[:name]}: #{e.message}. " +
                      "Values part: #{values_part}")
            end
        end
    end

end
