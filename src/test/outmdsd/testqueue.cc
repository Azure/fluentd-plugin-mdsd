#include <boost/test/unit_test.hpp>
#include <future>
#include <ConcurrentQueue.h>

BOOST_AUTO_TEST_SUITE(testqueue)

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_BVT)
{
    try {
        ConcurrentQueue<int> q;
        const int expected = 1234;
        q.push(expected);
        int actual = 0;
        q.wait_and_pop(actual);

        BOOST_CHECK_EQUAL(expected, actual);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

// this function should wait until element is ready
// validate that
//   - it actual waits
//   - it will stop waiting once it gets item
void
WaitQueueItem(ConcurrentQueue<int>* q, int expected, uint32_t minRunTimeMS)
{
    auto startTime = std::chrono::system_clock::now();
    int actual = 0;
    q->wait_and_pop(actual);
    BOOST_REQUIRE_EQUAL(expected, actual);

    auto endTime = std::chrono::system_clock::now();
    auto runTimeMS = static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));
    BOOST_CHECK_GE(runTimeMS, minRunTimeMS);
    BOOST_TEST_MESSAGE("queue wait_and_pop item=" << actual << "; runtime: " << runTimeMS);
}

// wait for a task for a max amount of time. then check its status
void
ValidateTaskStatus(std::future<void>& task, uint32_t timeoutMS)
{
    try {
        auto status = task.wait_for(std::chrono::milliseconds(timeoutMS));
        bool taskOK = (std::future_status::ready == status);
        BOOST_CHECK_EQUAL(true, taskOK);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("ValidateTaskStatus failed: " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_Wait)
{
    try {
        ConcurrentQueue<int> q;
        const uint32_t minRunTimeMS = 100;
        const int expected = 1234;
        auto task = std::async(std::launch::async, WaitQueueItem, &q, expected, minRunTimeMS);
        std::this_thread::sleep_for(std::chrono::milliseconds(minRunTimeMS));
        q.push(expected);

        ValidateTaskStatus(task, minRunTimeMS*5);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

// this function should wait until element is ready
// validate that
//   - it actual waits
//   - it will stop when stop_wait() is called
void
WaitEmptyQueue(ConcurrentQueue<int>* q, uint32_t minRunTimeMS)
{
    auto startTime = std::chrono::system_clock::now();
    const int origVal = -123;
    int actual = origVal;
    q->wait_and_pop(actual);

    // nothing should be popped, so value shouldn't be changed
    BOOST_CHECK_EQUAL(origVal, actual);

    auto endTime = std::chrono::system_clock::now();
    auto runTimeMS = static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));
    BOOST_CHECK_GE(runTimeMS, minRunTimeMS);
    BOOST_TEST_MESSAGE("queue wait_and_pop item=" << actual << "; runtime: " << runTimeMS);
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_StopWait)
{
    try {
        ConcurrentQueue<int> q;
        const uint32_t minRunTimeMS = 100;
        auto task = std::async(std::launch::async, WaitEmptyQueue, &q, minRunTimeMS);
        std::this_thread::sleep_for(std::chrono::milliseconds(minRunTimeMS));
        q.stop_wait();

        ValidateTaskStatus(task, minRunTimeMS*5);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
