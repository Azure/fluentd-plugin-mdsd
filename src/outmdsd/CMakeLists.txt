include_directories(
)

set(SOURCES
    BufferedLogger.cc
    DataReader.cc
    DataResender.cc
    DataSender.cc
    DjsonLogItem.cc
    FileLogger.cc
    IdMgr.cc
    LogItem.cc
    SockAddr.cc
    SocketClient.cc
    SocketLogger.cc
    Trace.cc
)

# static lib only
add_library(outmdsd STATIC ${SOURCES})

install(TARGETS
        outmdsd
        ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}/release/lib)
