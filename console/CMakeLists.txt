cmake_minimum_required(VERSION 3.15)
project(Console)

include_directories(include)

# Set C++ compiler flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -w")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /usr/local/include
)

# Find necessary packages
find_package(Threads REQUIRED)

# Link directories
link_directories(/usr/local/lib)

# Define the executable
add_executable(console
    src/console.cc
    src/handler.cc)

add_dependencies(console clientlib protolib)

set(LIBS server_lib ${CMAKE_THREAD_LIBS_INIT})

# Define dependencies for the executable
target_link_libraries(console
    server_lib
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(console ${LIBS} clientlib protolib)

# Custom target for cleaning extra files
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove -f $<TARGET_FILE:console>
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaned all generated files.")

# Add dependencies for the executable
target_link_libraries(console ${LIBS} clientlib protolib)
target_include_directories(console PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../kvs/client/src)
target_include_directories(console PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../kvs/controller/src)

# Set the output directory for the 'console' target
set_target_properties(console PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../console)
