set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
# Find Boost 1.56 or higher
find_package(Boost 1.56 REQUIRED COMPONENTS system unit_test_framework)

include_directories(
    ${CMAKE_SOURCE_DIR}/outmdsd
)

add_executable(
    ut_outmdsd
    MockServer.cc
    testbuflog.cc
    testlogger.cc
    testlogitem.cc
    testmap.cc
    testqueue.cc
    testreader.cc
    testresender.cc
    testsender.cc
    testsocket.cc
    testtrace.cc
    testutil.cc
    utmain.cc
)

target_link_libraries(
    ut_outmdsd
    outmdsd
    Boost::unit_test_framework
    Boost::system
)

add_executable(
    mockserver
    MockServer.cc
    mockserver_main.cc
    testutil.cc
)

install(TARGETS
    ut_outmdsd
    mockserver
    RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/release/tests
)
