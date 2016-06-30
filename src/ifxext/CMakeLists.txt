set(PAL_INC_PATH "/usr/include/azurepal")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNICODE")

include_directories(
    ${PAL_INC_PATH}
    ${PAL_INC_PATH}/unix/winapi
    ${PAL_INC_PATH}/unix/winapi/crt
    ${PAL_INC_PATH}/etw
    ${PAL_INC_PATH}/IfxMetrics
    ${RUBY_INC_PATH}
    ${RUBY_INC_PATH}/x86_64-linux
)

set(SOURCES
    ifxext.cxx
    ifxext_wrap.cxx
)

set(IFXEXT_LIB ifxext)

add_library(${IFXEXT_LIB} ${SOURCES})

target_link_libraries(${IFXEXT_LIB}
    rtcpal
    IfxMetrics
)

install(TARGETS ${IFXEXT_LIB}
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/release/lib
)
