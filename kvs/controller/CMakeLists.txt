cmake_minimum_required(VERSION 3.15)
project(ControllerModule)

include_directories(../server/src)

add_executable(KVSController ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc)
target_link_libraries(KVSController protolib)
target_include_directories(protolib PUBLIC ${CMAKE_CURRENT_BINARY_DIR})