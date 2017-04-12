#include <boost/test/unit_test.hpp>
#include <future>

#include "ConcurrentMap.h"
#include "ConcurrentQueue.h"
#include "DataSender.h"
#include "SocketClient.h"
#include "DjsonLogItem.h"
#include "testutil.h"
#include "MockServer.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testsender)

// Return runtime in milli-seconds on how long DataSender::Run() is run
static uint32_t
RunSender(
    std::promise<void>& threadReady,
    DataSender& sender
    )
{
    threadReady.set_value();
    auto startTime = std::chrono::system_clock::now();
    sender.Run();
    auto endTime = std::chrono::system_clock::now();
    return static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));
}

static void
AddItemsToQueue(
    const std::shared_ptr<ConcurrentQueue<LogItemPtr>>& q,
    size_t nitems,
    size_t nbytesPerItem,
    int delayMicroSeconds // number of micro-seconds to delay between send
    )
{
    auto testdata = std::string(nbytesPerItem, 'A');

    for (size_t i = 0; i < nitems; i++) {
        LogItemPtr p(new DjsonLogItem("testsource", testdata));
        q->push(std::move(p));

        if (delayMicroSeconds > 0) {
            usleep(delayMicroSeconds);
        }
    }
}

static void
AddEndOfTestToQueue(
    const std::shared_ptr<ConcurrentQueue<LogItemPtr>>& q
    )
{
    LogItemPtr p(new DjsonLogItem("testsource", TestUtil::EndOfTest()));
    q->push(std::move(p));
}

// This test validates DataSender APIs only. It doesn't validate the data flow
// from socket client to the socket server.
static void
RunAPITest(
    bool useDataCache
    )
{
    try {
        std::promise<void> threadReady;

        const std::string socketfile = "/tmp/nosuchfile";
        auto sockClient = std::make_shared<SocketClient>(socketfile, 1);
        auto q = std::make_shared<ConcurrentQueue<LogItemPtr>>();

        std::shared_ptr<ConcurrentMap<LogItemPtr>> cache;
        if (useDataCache) {
            cache = std::make_shared<ConcurrentMap<LogItemPtr>>();
        }

        const size_t nitems = 2;
        AddItemsToQueue(q, nitems, 10, 0);

        DataSender sender(sockClient, cache, q);
        auto senderTask = std::async(std::launch::async, RunSender, std::ref(threadReady), std::ref(sender));

        const uint32_t runTimeMS = 10;
        threadReady.get_future().wait();
        usleep(runTimeMS*1000);
        sender.Stop();
        q->stop_once_empty();

        BOOST_CHECK_EQUAL(nitems, sender.GetNumSend());
        BOOST_CHECK_EQUAL(0, q->size());
        BOOST_CHECK_EQUAL(0, sender.GetNumSuccess());

        // validate that Run() continues until it is told to stop
        auto actualRunTime = senderTask.get();
        BOOST_CHECK_GE(actualRunTime, runTimeMS);

        if (cache) {
            BOOST_CHECK_EQUAL(nitems, cache->Size());
        }
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with exception " << ex.what());
    }
}

// Test DataSender with invalid socket
//  - all data sent should fail.
//  - DataSender::Run() should continue until it is told to stop.
//  - no unexpected exception
//
BOOST_AUTO_TEST_CASE(Test_DataSender_InvalidSocket)
{
    RunAPITest(false);
}

// Test that all data are moved to the cache after DataSender::Run()
BOOST_AUTO_TEST_CASE(Test_DataSender_Cache)
{
    RunAPITest(true);
}

void
RunE2ETest(size_t nitems, size_t nbytesPerItem)
{
    const std::string sockfile = TestUtil::GetCurrDir() + "/datasender-bvt";
    auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile, false);
    mockServer->Init();

    auto serverTask = std::async(std::launch::async, [mockServer]() { mockServer->Run(); });

    auto sockClient = std::make_shared<SocketClient>(sockfile, 20);
    auto incomingQueue = std::make_shared<ConcurrentQueue<LogItemPtr>>();
    auto dataCache = std::make_shared<ConcurrentMap<LogItemPtr>>();

    std::promise<void> threadReady;
    DataSender sender(sockClient, dataCache, incomingQueue);
    auto senderTask = std::async(std::launch::async, [&sender]() { sender.Run(); });

    AddItemsToQueue(incomingQueue, nitems, nbytesPerItem, 10);
    AddEndOfTestToQueue(incomingQueue);
    nitems++; // end of test message

    bool mockServerDone = mockServer->WaitForTestsDone(1000);
    BOOST_CHECK(mockServerDone);

    mockServer->Stop();
    sender.Stop();
    incomingQueue->stop_once_empty();
    // Wait until sender task is finished or timed out.
    // Without this, senderTask could be in the middle of sending, such that the sender
    // counters (e.g. GetNumSend(), GetNumSuccess(), etc) can be invalid.
    BOOST_CHECK(TestUtil::WaitForTask(senderTask, 500));

    BOOST_CHECK_EQUAL(nitems, sender.GetNumSend());
    BOOST_CHECK_EQUAL(0, incomingQueue->size());

    BOOST_CHECK_EQUAL(sender.GetNumSend(), sender.GetNumSuccess());
    BOOST_CHECK_EQUAL(sender.GetNumSend(), dataCache->Size());
    BOOST_CHECK_EQUAL(0, mockServer->GetTotalTags()); // no tag parsing, so GetTotalTags() should be 0

    // actual data are more than minSendBytes because extra data added in DJSON
    auto minSendBytes = (nitems-1) * nbytesPerItem + TestUtil::EndOfTest().size();
    BOOST_CHECK_GT(mockServer->GetTotalBytesRead(), minSendBytes);
}

BOOST_AUTO_TEST_CASE(Test_DataSender_BVT)
{
    try {
        RunE2ETest(1, 100);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_DataSender_Stress)
{
    try {
        RunE2ETest(5000, 100);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with exception " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
