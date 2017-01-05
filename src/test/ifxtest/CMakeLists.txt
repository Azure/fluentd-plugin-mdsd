file(GLOB RUBYFILES "*.rb")

file(COPY
    ${RUBYFILES}
    DESTINATION
    ${CMAKE_BINARY_DIR}/release/tests)
