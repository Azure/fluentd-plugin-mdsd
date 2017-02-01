#!/usr/bin/ruby

# The following are dummy definitions so that out_mdsd.rb can be loaded
# successfully by require. They are not tested in this file.
module Fluent
    class Plugin
        def self.register_output(x,y)
        end
    end

    class BufferedOutput
        def self.desc(x)
        end
        def self.config_param(x,y)
        end
    end
end

require_relative "../../../fluent-plugin-mdsd/lib/fluent/plugin/out_mdsd"
require "test/unit"

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
            "timekey" => Time.now,
            "strkey" => "teststring"
        }

        schema_obj = @schema_mgr.get_schema_info(record)
        assert_equal(1, schema_obj[0], "first schema id")

        expected_str = '[["truekey","FT_BOOL"],["falsekey","FT_BOOL"],["intkey","FT_INT64"],["floatkey","FT_DOUBLE"],["timekey","FT_TIME"],["strkey","FT_STRING"]]'
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
        assert_equal('[["strkey","FT_STRING"],["intkey","FT_INT64"]]', schema_obj1[1], "schema string")

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

            expected_str = '[["' + tmpstr + '","FT_INT64"]]'
            assert_equal(expected_str, schema_obj[1], "schema str #{n}")
        }

        assert_equal(nrecords, @schema_mgr.size(), "schema mgr size")
    end
end


class MdsdMsgMakerTest < Test::Unit::TestCase

    def setup
        @msg_maker = MdsdMsgMaker.new(nil)
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
        expectedStr = '1,[["truekey","FT_BOOL"],["falsekey","FT_BOOL"],["intkey","FT_INT64"],["floatkey","FT_DOUBLE"],["timekey","FT_TIME"],["strkey","FT_STRING"]],[true,false,1234,3.14,[1478626843,878947000],"teststring"]'
        assert_equal(expectedStr, schemaVal, "get_schema_value_str")
    end
end
