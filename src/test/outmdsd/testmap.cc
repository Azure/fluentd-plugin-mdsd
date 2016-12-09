#include <boost/test/unit_test.hpp>
#include "ConcurrentMap.h"
#include "DjsonLogItem.h"


using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testmap)

bool filterFunc(LogItemPtr)
{
    return true;
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentMap_AddGet)
{
    try {
        ConcurrentMap m;
        BOOST_CHECK_EQUAL(0, m.Size());

        LogItemPtr p(new DjsonLogItem("testSource", "testSchemaData"));
        const std::string testKey = "testkey";
        m.Add(testKey, p);

        BOOST_CHECK_EQUAL(1, m.Size());
        BOOST_CHECK_EQUAL(p, m.Get(testKey));

        // add existing key should overwrite
        LogItemPtr p2(new DjsonLogItem("testSource2", "testSchemaData2"));
        m.Add(testKey, p2);
        BOOST_CHECK_EQUAL(1, m.Size());
        BOOST_CHECK_EQUAL(p2, m.Get(testKey));
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentMap_Filter)
{
    try {
        ConcurrentMap m;

        auto listForEmptyMap = m.FilterEach(filterFunc);
        BOOST_CHECK_EQUAL(0, listForEmptyMap.size());

        LogItemPtr p(new DjsonLogItem("testSource", "testSchemaData"));
        const std::string testKey = "testkey";
        m.Add(testKey, p);

        auto listForMap1 = m.FilterEach(filterFunc);
        BOOST_CHECK_EQUAL(1, listForMap1.size());
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}


BOOST_AUTO_TEST_CASE(Test_ConcurrentMap_Erase)
{
    try {
        ConcurrentMap m;
        
        LogItemPtr p(new DjsonLogItem("testSource", "testSchemaData"));
        const char* testkey = "testkey";
        m.Add(testkey, p);

        BOOST_CHECK_EQUAL(1, m.Size());
        BOOST_CHECK_EQUAL(0, m.Erase("badkey"));
        BOOST_CHECK_EQUAL(1, m.Size());

        BOOST_CHECK_EQUAL(1, m.Erase(testkey));
        BOOST_CHECK_EQUAL(0, m.Size());

        std::vector<std::string> keylist = {"key1", "key2", "key3"};
        for (const auto & key : keylist) {
            m.Add(key, p);
        }
        auto nGoodKeys = keylist.size();
        keylist.push_back("notExistKey");

        BOOST_CHECK_EQUAL(nGoodKeys, m.Size());
        BOOST_CHECK_EQUAL(nGoodKeys, m.Erase(keylist));
        BOOST_CHECK_EQUAL(0, m.Size());
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}


BOOST_AUTO_TEST_SUITE_END()
