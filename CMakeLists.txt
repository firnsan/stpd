cmake_minimum_required(VERSION 2.6)

project(stpd CXX)

set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

set(clientproxy_SRCS
  utils.cc
  http2stpd.cc
  clientproxy.cc
  )

add_executable(clientproxy ${clientproxy_SRCS})
target_link_libraries(clientproxy pthread)

set(serverproxy_SRCS
  utils.cc
  serverproxy.cc
  )
add_executable(serverproxy ${serverproxy_SRCS})
target_link_libraries(serverproxy pthread)


