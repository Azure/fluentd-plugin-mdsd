file(COPY
    runtests.sh
    testpluginrb.sh
    DESTINATION ${CMAKE_BINARY_DIR}/release/tests)

add_subdirectory(outmdsd)
add_subdirectory(outmdsdrb)
