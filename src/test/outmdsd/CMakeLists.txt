set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

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
    testutil.cc
    utmain.cc
)

target_link_libraries(
    ut_outmdsd
    outmdsd
    boost_unit_test_framework
    /usr/lib/x86_64-linux-gnu/libboost_system.a
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
