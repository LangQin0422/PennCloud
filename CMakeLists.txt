cmake_minimum_required(VERSION 3.15)

project(
  T07
  VERSION 1.0
  DESCRIPTION "CIS505 Final Project - T07"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(kvs)
add_subdirectory(HTTP_Server)

# Option for building HTTP_Server
add_custom_target(HTTP_Server ALL
  COMMAND make install
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/HTTP_Server
  COMMENT "Building and installing HTTP_Server")

add_dependencies(HTTP_Server
  protolib
  clientlib
  KVSController
  KVSServer)

add_subdirectory(frontend)
add_subdirectory(webmail)
add_subdirectory(console)
set(VERBOSE_FLAG "")

if(VERBOSE_LOGGING)
  set(VERBOSE_FLAG "-v")
endif()

set(ECHO_FRONTEND_COORDINATOR "echo \"cd \$(pwd)/console; ./console ${VERBOSE_FLAG} -n frontend.coordinator -c \$(pwd)/config.ini\" > frontendCoordinator.sh")
set(ECHO_FRONTEND_WORKERS "echo \"cd \$(pwd)/frontend; ./frontend ${VERBOSE_FLAG} -c\$(pwd)/config.ini -n frontend.worker.\$i\" > frontendWorker\$i.sh\nchmod +x frontendWorker\$i.sh\nDISPLAY=:0 xterm -hold -e \"bash \$(pwd)/frontendWorker\$i.sh\" &")
set(ECHO_POP3 "echo \"cd \$(pwd)/webmail; ./pop3 ${VERBOSE_FLAG} mailboxes/ \" > pop3.sh\nchmod +x pop3.sh\nDISPLAY=:0 xterm -hold -e \"bash \$(pwd)/pop3.sh\" &")
set(ECHO_SMTP "echo \"cd \$(pwd)/webmail; ./smtp ${VERBOSE_FLAG} -e mailboxes/ \" > smtp.sh\nchmod +x smtp.sh\nDISPLAY=:0 xterm -hold -e \"bash \$(pwd)/smtp.sh\" &")

# Configure the script file
configure_file(script_template.sh.in ${CMAKE_CURRENT_SOURCE_DIR}/script.sh @ONLY)