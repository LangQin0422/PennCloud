cmake_minimum_required(VERSION 3.15)
project(TestModule)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../server/src)

add_executable(test-client ${CMAKE_CURRENT_SOURCE_DIR}/src/test-client.cc)
add_executable(test-remote ${CMAKE_CURRENT_SOURCE_DIR}/src/test-remote.cc)
add_executable(test-scheduler ${CMAKE_CURRENT_SOURCE_DIR}/src/test-scheduler.cc)
add_executable(test-controller ${CMAKE_CURRENT_SOURCE_DIR}/src/test-controller.cc)

add_dependencies(test-client clientlib)
add_dependencies(test-remote clientlib)
add_dependencies(test-controller clientlib)

target_link_libraries(test-remote clientlib protolib)
target_link_libraries(test-client clientlib protolib)
target_link_libraries(test-controller clientlib protolib)
target_include_directories(test-remote PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../client/src)
target_include_directories(test-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../client/src)
target_include_directories(test-controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../client/src)