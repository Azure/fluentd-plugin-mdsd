#include <boost/test/unit_test.hpp>
#include "ConcurrentMap.h"
#include "DjsonLogItem.h"
#include "LogItemPtr.h"
#include "testutil.h"
#include "CounterCV.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testmap)

bool filterFunc(LogItemPtr)
{
    return true;
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentMap_AddGet)
{
    try {
        ConcurrentMap<int> m;
        BOOST_CHECK_EQUAL(0, m.Size());

        const int testVal = 123;
        const std::string testKey = "testkey";
        m.Add(testKey, testVal);

        BOOST_CHECK_EQUAL(1, m.Size());
        BOOST_CHECK_EQUAL(testVal, m.Get(testKey));

        // add existing key should overwrite
        const int testVal2 = testVal + 1;
        m.Add(testKey, testVal2);
        BOOST_CHECK_EQUAL(1, m.Size());
        BOOST_CHECK_EQUAL(testVal2, m.Get(testKey));

        // not exist key should throw
        BOOST_CHECK_THROW(m.Get("nosuchkey"), std::out_of_range);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentMap_Filter)
{
    try {
        ConcurrentMap<LogItemPtr> m;

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
        ConcurrentMap<std::string> m;

        const std::string p = "testVal";
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

void
AddToMap(
    const std::shared_future<void> & masterReady,
    std::promise<void>& thReady,
    const std::string & keyprefix,
    ConcurrentMap<int>& m,
    int nitems,
    const std::shared_ptr<CounterCV>& cv
    )
{
    CounterCVWrap cvwrap(cv);
    thReady.set_value();
    masterReady.wait(); // wait for all threads to be ready

    for(int i = 0; i < nitems; i++) {
        m.Add(keyprefix + std::to_string(i), i);
    }
}

void
EraseFromMap(
    const std::shared_future<void> & masterReady,
    std::promise<void>& thReady,
    const std::string & keyprefix,
    ConcurrentMap<int>& m,
    int nitems
    )
{
    thReady.set_value();
    masterReady.wait(); // wait for all threads to be ready

    for(int i = 0; i < nitems; i++) {
        m.Erase(keyprefix + std::to_string(i));
    }
}

// Run multiple adder and eraser threads. This is to make sure that ConcurrentMap
// won't have unexpected hang or exception. It doesn't validate the exact values
// are added/erased.
// To make sure that all threads are run concurrently, use std::promise and
// std::shared_future to coordinate the threads.
void
MultiAddEraseTest(
    int nAddThreads,
    int nEraseThreads,
    int nitems
    )
{
    // use a promise and shared_future to sync all threads to start at the same time
    std::promise<void> masterPromise;
    std::shared_future<void> masterReady(masterPromise.get_future());

    std::vector<std::promise<void>> thReadyList;
    for (int i = 0; i < nAddThreads+nEraseThreads; i++) {
        thReadyList.push_back(std::promise<void>());
    }

    ConcurrentMap<int> m;
    std::vector<std::future<void>> taskList;

    auto taskCV = std::make_shared<CounterCV>(nAddThreads);

    const std::string keybase = "key";

    for (int i = 0; i < nAddThreads; i++) {
        auto keyprefix = keybase + std::to_string(i);
        auto t = std::async(std::launch::async, AddToMap, masterReady,
            std::ref(thReadyList[i]), keyprefix, std::ref(m), nitems, taskCV);
        taskList.push_back(std::move(t));
    }

    // if nEraseThread != nAddThread, not all keys are matched between Erase an Add. This is
    // OK because this can test code to handle not-exist keys.
    for (int i = 0; i < nEraseThreads; i++) {
        auto keyprefix = keybase + std::to_string(i);
        auto t = std::async(std::launch::async, EraseFromMap, masterReady,
            std::ref(thReadyList[i+nAddThreads]), keyprefix, std::ref(m), nitems);
        taskList.push_back(std::move(t));
    }

    // wait for all threads to be ready
    for (auto & t : thReadyList) {
        t.get_future().wait();
    }

    // start all threads
    masterPromise.set_value();

    BOOST_CHECK(taskCV->wait_for(400));
}


BOOST_AUTO_TEST_CASE(Test_ConcurrentMap_MultiThreads)
{
    try {
        MultiAddEraseTest(6, 4, 10000);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
