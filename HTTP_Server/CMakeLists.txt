cmake_minimum_required(VERSION 3.15)
project(HTTP_Server)

include_directories(include)

find_package(Threads REQUIRED)

# Set C++ compiler flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -w")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        /usr/local/include
)

link_directories(/usr/local/lib)

# Library Source Files
set(LIB_SOURCES
        src/server.cc
        src/helper.cc
)

# Define the library
add_library(server_lib SHARED ${LIB_SOURCES})
target_link_libraries(server_lib Threads::Threads)

add_dependencies(server_lib
        protolib
        clientlib
        KVSController
        KVSServer)

set(LIBS ${CMAKE_THREAD_LIBS_INIT})

# Define dependencies for the executable
target_link_libraries(server_lib
        ${CMAKE_THREAD_LIBS_INIT}
)

# Add dependencies for the executable
target_link_libraries(server_lib ${LIBS} clientlib protolib)
target_include_directories(server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../kvs/client/src)
target_include_directories(server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../kvs/controller/src)

# Installation rules for the library and header files
install(TARGETS server_lib
        LIBRARY DESTINATION /usr/local/lib/)

install(DIRECTORY include/ DESTINATION /usr/local/include FILES_MATCHING PATTERN "*.hh")

install(DIRECTORY include/
        DESTINATION /usr/local/include
        FILES_MATCHING PATTERN "*.hh")