# To force using memcpy@GLIBC_2.2.5 (for old Linux distro versions)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--wrap=memcpy")
# To avoid GLIBCXX_3.4.(high) version issue, statically link to libstdc++ & libgcc
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")

include_directories(
    ${CMAKE_SOURCE_DIR}/outmdsd
    ${RUBY_INC_PATH}
    ${RUBY_INC_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
add_custom_command(
PRE_BUILD
OUTPUT ${CMAKE_SOURCE_DIR}/outmdsdrb/outmdsdrb_wrap.cxx
COMMAND swig -c++ -ruby ${CMAKE_SOURCE_DIR}/outmdsdrb/outmdsdrb.i
DEPENDS ${CMAKE_SOURCE_DIR}/outmdsdrb/outmdsdrb.i
)
endif()

set(SOURCES
    outmdsd_log.cxx
)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
list(APPEND SOURCES
    wrap_memcpy.c
    outmdsdrb_wrap.cxx
)
endif()

set(LIB_NAME outmdsdrb)

add_library(${LIB_NAME} ${SOURCES})
SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "Lib")

target_link_libraries(${LIB_NAME}
    outmdsd
    # Required not to use clock_gettime@GLIBC_2.17
    -Wl,--no-as-needed
    rt
)

install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/release/lib
)
