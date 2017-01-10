cmake_minimum_required(VERSION 2.8)
project(fluentdplugins)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Platform (not compiler) specific settings
if(UNIX) # This includes Linux
    message("Build for Unix/Linux OS")
else()
  	message("-- Unsupported Build Platform.")
endif()

# Compiler (not platform) specific settings
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    message("-- Clang compiler is not supported")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    message("-- Setting gcc options")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-literal-suffix")
else()
  	message("-- Unknown compiler, success is doubtful.")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(COMM_FLAGS "${COMM_FLAGS} -fstack-protector-all")
set(COMM_FLAGS "${COMM_FLAGS} -fPIC")
set(COMM_FLAGS "${COMM_FLAGS} -D_FORTIFY_SOURCE=2")
set(COMM_FLAGS "${COMM_FLAGS} -ffunction-sections")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMM_FLAGS}")

set(WARNINGS "${WARNINGS} -Wall")
set(WARNINGS "${WARNINGS} -Wextra")
set(WARNINGS "${WARNINGS} -Wno-missing-field-initializers")
set(WARNINGS "${WARNINGS} -Wno-unused-parameter")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNINGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

set(LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")

set(RUBY_INC_PATH /opt/microsoft/omsagent/ruby/include/ruby-2.2.0)

option(BUILD_SHARED_LIBS "Build shared Libraries." ON)

add_subdirectory(ifxext)
add_subdirectory(outmdsd)
add_subdirectory(test)
add_subdirectory(outmdsdrb)
