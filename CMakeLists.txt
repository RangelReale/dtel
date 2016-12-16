cmake_minimum_required(VERSION 2.8)
project(dtel)

#enable_testing()

set(CMAKE_CXX_STANDARD 11)
add_definitions(-DDUK_OPT_CPP_EXCEPTIONS)

include_directories(include)
include_directories(duktape)
file(GLOB_RECURSE headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/*.h)

add_executable(example_runner ${CMAKE_CURRENT_SOURCE_DIR}/example/Example.cpp ${headers}
    ${CMAKE_CURRENT_SOURCE_DIR}/duktape/duktape.c
    ${CMAKE_CURRENT_SOURCE_DIR}/duktape/duktape.h
    ${CMAKE_CURRENT_SOURCE_DIR}/duktape/duk_config.h
)

SET_SOURCE_FILES_PROPERTIES( ${CMAKE_CURRENT_SOURCE_DIR}/duktape/duktape.c PROPERTIES LANGUAGE CXX )
