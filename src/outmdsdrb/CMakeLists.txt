# To force using memcpy@GLIBC_2.2.5 (for old Linux distro versions)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--wrap=memcpy")

include_directories(
    ${CMAKE_SOURCE_DIR}/outmdsd
    ${RUBY_INC_PATH}
    ${RUBY_INC_PATH}/x86_64-linux
)

add_custom_command(
PRE_BUILD
OUTPUT ${CMAKE_SOURCE_DIR}/outmdsdrb/outmdsdrb_wrap.cxx
COMMAND swig3.0 -c++ -ruby ${CMAKE_SOURCE_DIR}/outmdsdrb/outmdsdrb.i
DEPENDS ${CMAKE_SOURCE_DIR}/outmdsdrb/outmdsdrb.i
)

set(SOURCES
    wrap_memcpy.c
    outmdsdrb_wrap.cxx
    outmdsd_log.cxx
)

set(LIB_NAME outmdsdrb)

add_library(${LIB_NAME} ${SOURCES})
SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "Lib")

target_link_libraries(${LIB_NAME}
    outmdsd
)

install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/release/lib
)
