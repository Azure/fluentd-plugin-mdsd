# To force using memcpy@GLIBC_2.2.5 (for old Linux distro versions)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--wrap=memcpy")
# To avoid GLIBCXX_3.4.(high) version issue, statically link to libstdc++ & libgcc
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(LIB_NAME outmdsdrb)

find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

include_directories(
    ${CMAKE_SOURCE_DIR}/outmdsd
    ${CMAKE_SOURCE_DIR}/outmdsdrb
    ${RUBY_INC_PATH}
    ${RUBY_INC_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux
)

set(SOURCES
    outmdsd_log.cxx
)
if(NOT ${FLUENTD_TARGET} system)
    list(APPEND SOURCES
        wrap_memcpy.c
    )
endif()

set_property(SOURCE outmdsdrb.i PROPERTY CPLUSPLUS ON)
swig_add_library(${LIB_NAME}
    TYPE MODULE
    LANGUAGE ruby
    SOURCES outmdsdrb.i ${SOURCES}
)
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
