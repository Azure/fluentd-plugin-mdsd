#include <boost/test/unit_test.hpp>
#include <chrono>
#include <unordered_set>

extern "C" {
#include <syslog.h>
}

#include "Trace.h"
#include "SyslogTracer.h"
#include "TraceMacros.h"
#include "testutil.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testtrace)

// Use a high resolution clock to create a test string.
// It should be unique across tests
static std::string
GetTestStr(int level)
{
    static int counter = 0;
    static auto timens = std::chrono::steady_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);

    counter++;
    std::ostringstream strm;
    strm << "TraceLevel_" << level << " " << timens << "-" << counter;
    return strm.str();
}

// Search a set of strings from /var/log/syslog.
// Because it takes undetermined time for a msg to show up in
// /var/log/syslog, search N times with incremental delay.
// Return true if all items are found, return false if not all are found.
static bool
SearchInSyslog(
    std::streampos startPos,
    std::unordered_set<std::string> & searchSet
    )
{
    int ntimes = 10;

    for (int i = 0; i < ntimes; i++) {
        auto foundAll = TestUtil::SearchStrings("/var/log/syslog", startPos, searchSet);
        if (foundAll) {
            return true;
        }
        if (i != (ntimes-1)) {
            usleep((i+1)*1000); // do incremental delay
        }
    }
    return false;
}

static void
ValidateInSyslog(
    std::streampos startPos,
    std::unordered_set<std::string> & searchSet
    )
{
    auto foundAll = SearchInSyslog(startPos, searchSet);

    BOOST_CHECK(foundAll);
    if (!foundAll) {
        for (const auto & str : searchSet) {
            BOOST_TEST_MESSAGE("Error: item '" << str << "' is not found in syslog.");
        }
    }
}

static void
ValidateNotInSyslog(
    std::streampos startPos,
    std::unordered_set<std::string> & searchSet
    )
{
    auto sizeCopy = searchSet.size();
    auto foundAll = SearchInSyslog(startPos, searchSet);
    BOOST_CHECK(!foundAll);
    BOOST_CHECK_EQUAL(sizeCopy, searchSet.size());
}

static void
RunTraceTest(TraceLevel minLevel)
{
    Trace::SetTracer(new SyslogTracer(LOG_CONS, LOG_SYSLOG));
    Trace::SetTraceLevel(minLevel);

    auto origFileSz = TestUtil::GetFileSize("/var/log/syslog");

    auto start = static_cast<int>(TraceLevel::Trace);
    auto end = static_cast<int>(TraceLevel::Fatal);

    std::unordered_set<std::string> expectedSet;
    std::unordered_set<std::string> unexpectedSet;

    for (int i = start; i <= end; i++) {
        auto msg = GetTestStr(i);
        Log(static_cast<TraceLevel>(i), msg);

        if (i >= static_cast<int>(minLevel)) {
            expectedSet.insert(msg);
        }
        else {
            unexpectedSet.insert(msg);
        }
    }

    if (!expectedSet.empty()) {
        ValidateInSyslog(origFileSz, expectedSet);
    }
    if (!unexpectedSet.empty()) {
        ValidateNotInSyslog(origFileSz, unexpectedSet);
    }
}

// Test basic logging to syslog
// NOTE: make sure that some syslog daemon (like rsyslogd, syslog-ng, etc)
// is running before tests are started. Because no way to tell which syslog
// daemon is going to run, this is not asserted in the test.
BOOST_AUTO_TEST_CASE(Test_Syslog_Basic)
{
    try {
        RunTraceTest(TraceLevel::Trace);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

// Test that only levels >= SetTraceLevel are logged
BOOST_AUTO_TEST_CASE(Test_Syslog_Level)
{
    try {
        RunTraceTest(TraceLevel::Warning);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}


BOOST_AUTO_TEST_SUITE_END()
