#include <boost/test/unit_test.hpp>
#include <thread>
#include <future>
#include <ConcurrentQueue.h>
#include "testutil.h"
#include "CounterCV.h"

using namespace EndpointLog;

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

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_MaxSize)
{
    try {
        const int maxSize = 2;
        ConcurrentQueue<int> q(maxSize);

        for (int i = 0; i < maxSize; i++) {
            q.push(i);
            BOOST_CHECK_MESSAGE(i+1, q.size());
        }

        for (int i = 0; i < 10; i++) {
            q.push(i);
            BOOST_CHECK_MESSAGE(maxSize == q.size(), "pushed items=" << (maxSize+i+1));
        }
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}


// This function should wait until element is ready.
// Validate that
//   - it actual waits until some element is pushed to the queue.
void
WaitQueueItem(
    std::promise<void> & masterReady,
    std::promise<void> & threadReady,
    ConcurrentQueue<int>* q,
    int expected,
    bool& pushedToQueue
    )
{
    threadReady.set_value();
    masterReady.get_future().wait();

    int actual = 0;
    q->wait_and_pop(actual);
    BOOST_REQUIRE_EQUAL(expected, actual);
    // Test that when wait_and_pop() is done, pushedToQueue must be set to be true.
    BOOST_CHECK(pushedToQueue);
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_Wait)
{
    try {
        // use a promise and shared_future to sync thread startup
        std::promise<void> masterReady;
        std::promise<void> threadReady;
        bool pushedToQueue = false;

        ConcurrentQueue<int> q;
        const uint32_t minRunTimeMS = 10;
        const int expected = 1234;
        auto task = std::async(std::launch::async, WaitQueueItem,
            std::ref(masterReady), std::ref(threadReady), &q, expected, std::ref(pushedToQueue));

        threadReady.get_future().wait();
        masterReady.set_value();

        std::this_thread::sleep_for(std::chrono::milliseconds(minRunTimeMS));
        pushedToQueue = true;
        q.push(expected);

        // Validate that once an item is pushed to the queue, it shouldn't take
        // more than N milliseconds for wait_and_pop() thread to get the item and
        // break waiting. There is no exact value for N. The test uses some
        // reasonable small number.
        BOOST_CHECK(TestUtil::WaitForTask(task, 5));
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

// this function should wait until element is ready
// validate that
//   - it actual waits
//   - it will stop when stop_once_empty() is called
void
WaitEmptyQueue(
    std::promise<void>& masterReady,
    std::promise<void>& threadReady,
    ConcurrentQueue<int>* q,
    bool& stopQueue)
{
    threadReady.set_value();
    masterReady.get_future().wait();

    const int origVal = -123;
    int actual = origVal;
    q->wait_and_pop(actual);

    // nothing should be popped, so value shouldn't be changed
    BOOST_CHECK_EQUAL(origVal, actual);
    // Test that when wait_and_pop() is done, stopQueue must be set to be true.
    BOOST_CHECK(stopQueue);
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_StopWait)
{
    try {
        // use a promise and shared_future to sync thread startup
        std::promise<void> masterReady;
        std::promise<void> threadReady;
        bool stopQueue = false;

        ConcurrentQueue<int> q;
        const uint32_t minRunTimeMS = 10;

        auto task = std::async(std::launch::async, WaitEmptyQueue,
            std::ref(masterReady), std::ref(threadReady), &q, std::ref(stopQueue));

        threadReady.get_future().wait();
        masterReady.set_value();

        std::this_thread::sleep_for(std::chrono::milliseconds(minRunTimeMS));
        stopQueue = true;
        q.stop_once_empty();

        // Validate that once stop_once_empty() is called, it shouldn't take
        // more than N milliseconds for wait_and_pop() thread to break waiting.
        // The value of N is implementation-dependent. The test uses
        // some reasonable small number.
        BOOST_CHECK(TestUtil::WaitForTask(task, 5));
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

void
PushToQueue(
    const std::shared_future<void> & masterReady,
    std::promise<void>& thReady,
    ConcurrentQueue<int>& q,
    int nitems,
    const std::shared_ptr<CounterCV>& cv
    )
{
    CounterCVWrap cvwrap(cv);

    thReady.set_value();
    masterReady.wait();
    for (int i = 0; i < nitems; i++) {
        q.push(i);
    }
}

void
PopFromQueue(
    const std::shared_future<void> & masterReady,
    std::promise<void>& thReady,
    ConcurrentQueue<int>& q,
    const std::shared_ptr<CounterCV>& cv
    )
{
    CounterCVWrap cvwrap(cv);

    thReady.set_value();
    masterReady.wait();
    while(true) {
        auto v = q.wait_and_pop();
        if (nullptr == v) {
            break;
        }
    }
}

// To make sure that all threads are run concurrently, use std::promise and
// std::shared_future to coordinate the threads.
void
MultiPushPopTest(
    int nPushThreads,
    int nPopThreads,
    int nitems
    )
{
    // use a promise and shared_future to sync all threads to start at the same time
    std::promise<void> masterPromise;
    std::shared_future<void> masterReady(masterPromise.get_future());

    std::vector<std::promise<void>> thReadyList;
    for (int i = 0; i < (nPushThreads+nPopThreads); i++) {
        thReadyList.push_back(std::promise<void>());
    }

    ConcurrentQueue<int> q;

    auto pushCV = std::make_shared<CounterCV>(nPushThreads);
    auto popCV = std::make_shared<CounterCV>(nPopThreads);

    std::vector<std::future<void>> pushTasks;
    for (int i = 0; i < nPushThreads; i++) {
        auto t = std::async(std::launch::async, PushToQueue, masterReady,
            std::ref(thReadyList[i]), std::ref(q), nitems, pushCV);
        pushTasks.push_back(std::move(t));
    }

    std::vector<std::future<void>> popTasks;
    for (int i = 0; i < nPopThreads; i++) {
        auto t = std::async(std::launch::async, PopFromQueue, masterReady,
            std::ref(thReadyList[i+nPushThreads]), std::ref(q), popCV);
        popTasks.push_back(std::move(t));
    }

    // wait for all threads to be ready
    for (auto & t: thReadyList) {
        t.get_future().wait();
    }

    // start all threads
    masterPromise.set_value();

    BOOST_CHECK(pushCV->wait_for(400));
    q.stop_once_empty();
    BOOST_CHECK(popCV->wait_for(400));
}

BOOST_AUTO_TEST_CASE(Test_ConcurrentQueue_MultiThreads)
{
    try {
        MultiPushPopTest(6, 4, 10000);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
