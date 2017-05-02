#include <boost/test/unit_test.hpp>
#include <future>
#include "ConcurrentMap.h"
#include "SocketClient.h"
#include "DataResender.h"
#include "DjsonLogItem.h"
#include "testutil.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testresender)


size_t
StartDataResender(
    std::promise<void>& threadReady,
    const std::shared_ptr<DataResender>& resender,
    bool& stopRunLoop
    )
{
    threadReady.set_value();

    auto nTimerTriggered = resender->Run();
    // When resender Run() is done, the stopRunLoop must be set to be true.
    BOOST_CHECK(stopRunLoop);
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

        // To sync between main thread and StartDataResender thread
        std::promise<void> threadReady;
        bool stopRunLoop = false;

        auto sockClient = std::make_shared<SocketClient>("/tmp/nosuchfile", 1);
        auto resender = CreateDataResender(sockClient, std::make_shared<ConcurrentMap<LogItemPtr>>(), 0, retryMS);
        auto task = std::async(std::launch::async, StartDataResender, std::ref(threadReady), resender, std::ref(stopRunLoop));

        // Wait until StartDataResender is ready before starting timer
        threadReady.get_future().wait();
        usleep(minRunTimeMS*1000);

        sockClient->Stop();
        stopRunLoop = true;
        resender->Stop();

        // Validate that once DataResender::Stop() is called, it shouldn't take
        // more than N milliseconds for DataResender::Run() thread to break.
        // There is no exact value for N. The test uses some reasonable small number.
        BOOST_CHECK(TestUtil::WaitForTask(task, 5));

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

        std::promise<void> threadReady;
        bool stopRunLoop = false;

        auto sockClient = std::make_shared<SocketClient>("/tmp/nosuchfile", 1);
        auto dataCache = std::make_shared<ConcurrentMap<LogItemPtr>>();
        auto resender = CreateDataResender(sockClient, dataCache, 10, retryMS);
        auto task = std::async(std::launch::async, StartDataResender, std::ref(threadReady), resender, std::ref(stopRunLoop));

        // Wait until StartDataResender is ready before starting timer
        threadReady.get_future().wait();
        usleep(minRunTimeMS*1000);

        sockClient->Stop();
        // resender->Stop() should stop the Run() loop of resender.
        stopRunLoop = true;
        resender->Stop();

        // Validate that once DataResender::Stop() is called, it shouldn't take
        // more than N milliseconds for DataResender::Run() thread to break.
        // There is no exact value for N. The test uses some reasonable small number.
        BOOST_CHECK(TestUtil::WaitForTask(task, 5));

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
