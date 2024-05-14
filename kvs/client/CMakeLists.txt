cmake_minimum_required(VERSION 3.15)
project(ClientModule)

find_package(OpenSSL REQUIRED)

# Add library for the client
add_library(clientlib ${CMAKE_CURRENT_SOURCE_DIR}/src/KVSClient.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/KVSCTRLClient.cc)

target_include_directories(clientlib PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(clientlib protolib ${OPENSSL_LIBRARIES})