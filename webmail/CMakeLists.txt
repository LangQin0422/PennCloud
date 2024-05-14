cmake_minimum_required(VERSION 3.15)
project(Webmail)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -w")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(include /usr/local/include)

# Link directories
link_directories(/usr/local/lib)

add_executable(smtp
    src/smtp.cc
    src/smpt_msg.cc
    src/io_helper.cc
    src/smpt_command.cc)
target_link_libraries(smtp ${CMAKE_THREAD_LIBS_INIT} resolv server_lib)

# POP3 Executable
add_executable(pop3
    src/pop3.cc
    src/pop3_msg.cc
    src/io_helper.cc
    src/pop3_command.cc)
target_include_directories(pop3 PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(pop3 ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} resolv server_lib)

# Echo Server Executable
add_executable(echoserver
    src/echoserver.cc)
target_link_libraries(echoserver ${CMAKE_THREAD_LIBS_INIT} resolv server_lib)

# Forward Server Executable
add_executable(forward_server
    src/forward_server.cc
    src/io_helper.cc
    src/MX_helper.cc)
target_link_libraries(forward_server ${CMAKE_THREAD_LIBS_INIT} resolv server_lib)

# Custom target for creating sample mail directories
add_custom_target(samples
    COMMAND ${CMAKE_COMMAND} -E make_directory mailtest
    COMMAND ${CMAKE_COMMAND} -E touch mailtest/linhphan.mbox
    COMMAND ${CMAKE_COMMAND} -E touch mailtest/bcpierce.mbox
    COMMAND ${CMAKE_COMMAND} -E touch mailtest/zives.mbox
    COMMAND ${CMAKE_COMMAND} -E touch mailtest/zxiao.mbox)

# Add a 'realclean' custom target
add_custom_target(realclean
    COMMAND ${CMAKE_COMMAND} -E remove_directory "mailtest")

# Set the output directory for the 'console' target
set_target_properties(pop3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../webmail)

set_target_properties(smtp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../webmail)

set_target_properties(echoserver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../webmail)

set_target_properties(forward_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../webmail)