#include <boost/test/unit_test.hpp>
#include <future>
#include "ConcurrentMap.h"
#include "SocketClient.h"
#include "DataResender.h"
#include "DjsonLogItem.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testresender)


size_t
StartDataResender(
    const std::shared_ptr<DataResender>& resender,
    uint32_t minRunTimeMS
    )
{
    auto startTime = std::chrono::system_clock::now();
    auto nTimerTriggered = resender->Run();
    auto endTime = std::chrono::system_clock::now();
    auto runTimeMS = static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));
    BOOST_CHECK_GE(runTimeMS, minRunTimeMS);
    BOOST_TEST_MESSAGE("Number of timer triggered = " << nTimerTriggered);
    return nTimerTriggered;
}

std::shared_ptr<DataResender>
CreateDataResender(
    const std::shared_ptr<SocketClient>& sockClient,
    std::shared_ptr<ConcurrentMap<LogItemPtr>> dataCache,
    size_t cacheSize,
    uint32_t retryMS
    )
{
    for (size_t i = 0; i < cacheSize; i++) {
        auto indexStr = std::to_string(i+1);
        auto key = "testkey-" + indexStr;
        LogItemPtr value(new DjsonLogItem("testsource", "testvalue-" + indexStr));
        dataCache->Add(key, value);
    }

    return std::make_shared<DataResender>(sockClient, dataCache, 1, retryMS);
}

BOOST_AUTO_TEST_CASE(Test_DataResender_EmptyCache)
{
    try {
        const uint32_t retryMS = 200;
        const int nExpectedTimers = 2;
        const uint32_t minRunTimeMS = retryMS * nExpectedTimers + retryMS/2;

        auto sockClient = std::make_shared<SocketClient>("/tmp/nosuchfile", 1);
        auto resender = CreateDataResender(sockClient, std::make_shared<ConcurrentMap<LogItemPtr>>(), 0, retryMS);
        auto task = std::async(std::launch::async, StartDataResender, resender, minRunTimeMS);
        usleep(minRunTimeMS*1000);

        sockClient->Stop();
        resender->Stop();
        auto nTimerTriggered = task.get();
        BOOST_CHECK_EQUAL(nExpectedTimers, nTimerTriggered);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_DataResender_OneItem)
{
    try {
        auto retryMS = 50;
        auto minRunTimeMS = retryMS+retryMS/10;
        auto sockClient = std::make_shared<SocketClient>("/tmp/nosuchfile", 1);
        auto dataCache = std::make_shared<ConcurrentMap<LogItemPtr>>();
        auto resender = CreateDataResender(sockClient, dataCache, 10, retryMS);
        auto task = std::async(std::launch::async, StartDataResender, resender, minRunTimeMS);
        std::this_thread::sleep_for(std::chrono::milliseconds(minRunTimeMS));

        sockClient->Stop();
        resender->Stop();
        auto nTimerTriggered = task.get();
        BOOST_CHECK_EQUAL(1, nTimerTriggered);

        // validate data items are erased
        BOOST_CHECK_EQUAL(0, dataCache->Size());
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}


BOOST_AUTO_TEST_SUITE_END()
