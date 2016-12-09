#include <boost/test/unit_test.hpp>

#include "LogItem.h"
#include "DjsonLogItem.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(logitem)

BOOST_AUTO_TEST_CASE(Test_LogItem_Data)
{
    const std::string source = "testSource";
    const std::string schemaAndData = "testSchemaAndData";

    try {
        DjsonLogItem item(source, schemaAndData);
        auto tag = item.GetTag();
        auto data = item.GetData();
        BOOST_REQUIRE_GT(tag.size(), 0);

        std::ostringstream expected;
        auto len = source.size() + schemaAndData.size() + tag.size() + 6;
        expected << len << "\n[\"" << source << "\"," << tag << "," << schemaAndData << "]";
        BOOST_CHECK_EQUAL(expected.str(), data);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_LogItem_CacheTime)
{
    const std::string source = "testSource";
    const std::string schemaAndData = "testSchemaAndData";

    try {
        DjsonLogItem item(source, schemaAndData);
        item.Touch();
        int nexpectedMS = 20;
        usleep(nexpectedMS*1000);
        auto actualMS = item.GetLastTouchMilliSeconds();
        BOOST_CHECK_LE(abs(actualMS - nexpectedMS), 1);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
