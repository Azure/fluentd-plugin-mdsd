// NOTE: #define BOOST_TEST_MODULE xxx
// must be defined before you include unit_test.hpp.
#define BOOST_TEST_MODULE "outmdsdut"
#include <boost/test/included/unit_test.hpp>
#include "Trace.h"
#include "TraceMacros.h"
#include "testutil.h"

using namespace EndpointLog;


class TestInitializer {
public:
    TestInitializer()
    {
        std::ostringstream strm;
        strm << TestUtil::GetCurrDir() << "/outmdsd.log";
        auto logfile = strm.str();
        Trace::Init(logfile, true);
        Trace::SetTraceLevel(TraceLevel::Trace);
        Log(TraceLevel::Info, "\n\n============= start new outmdsd test ==========");
    }
    ~TestInitializer() = default;
};

BOOST_GLOBAL_FIXTURE(TestInitializer);
