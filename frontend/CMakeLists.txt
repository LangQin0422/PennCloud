cmake_minimum_required(VERSION 3.15)
project(Frontend)

include_directories(include)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -w -fPIC")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find necessary packages
find_package(Threads REQUIRED) # Find pthreads

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /usr/local/include
)

# Link directories and libraries
link_directories(/usr/local/lib)

# Define the executable
add_executable(frontend src/frontend.cc)
add_dependencies(frontend clientlib protolib)

set(LIBS server_lib ${CMAKE_THREAD_LIBS_INIT})

# Add dependencies for the executable
target_link_libraries(frontend ${LIBS} clientlib protolib)
target_include_directories(frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../kvs/client/src)

# Set the output directory for the 'console' target
set_target_properties(frontend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../frontend)