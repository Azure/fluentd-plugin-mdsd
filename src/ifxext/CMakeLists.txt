set(PAL_INC_PATH "/usr/include/azurepal")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNICODE")

# To force using memcpy@GLIBC_2.2.5 (for old Linux distro versions)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--wrap=memcpy")

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
    wrap_memcpy.c
)

set(IFXEXT_LIB ifxext)

add_library(${IFXEXT_LIB} ${SOURCES})
SET_TARGET_PROPERTIES(${IFXEXT_LIB} PROPERTIES PREFIX "Lib")

target_link_libraries(${IFXEXT_LIB}
    /usr/lib/x86_64-linux-gnu/libIfxMetrics.a
    /usr/lib/x86_64-linux-gnu/librtcpal.a
)

install(TARGETS ${IFXEXT_LIB}
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/release/lib
)
