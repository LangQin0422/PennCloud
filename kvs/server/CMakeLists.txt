cmake_minimum_required(VERSION 3.15)
project(ServerModule)

add_executable(KVSServer ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc)

target_link_libraries(KVSServer protolib)
target_include_directories(protolib PUBLIC ${CMAKE_CURRENT_BINARY_DIR})