cmake_minimum_required (VERSION 2.8.12)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
project (SLAVECONFIGTOOL)

set (SLAVECONFIGTOOL_VERSION_MAJOR 0)
set (SLAVECONFIGTOOL_VERSION_MINOR 1)
set (SLAVECONFIGTOOL_VERSION_PATCH 0)

# Raise the max http content size for tinyhttp to 1MB
add_definitions(-DMAX_HTTP_CONTENT_SIZE=1048576)

# Include paths
include_directories(
  ${SLAVECONFIGTOOL_SOURCE_DIR}
  ${SLAVECONFIGTOOL_SOURCE_DIR}/tinyxml2
  ${SLAVECONFIGTOOL_SOURCE_DIR}/tinyhttp
  ${SLAVECONFIGTOOL_SOURCE_DIR}/tinyhttp/htcc
  ${SLAVECONFIGTOOL_SOURCE_DIR}/MiniJson/Source/include
  )

set(CMAKE_CXX_FLAGS "-O0 -ggdb")

# Platform flags and sources
include(${CMAKE_SYSTEM_NAME} OPTIONAL)

message (STATUS "Building for ${CMAKE_SYSTEM_NAME}")

add_executable(esctool
  MiniJson/Source/src/json.cpp
  MiniJson/Source/src/jsonValue.cpp
  MiniJson/Source/src/parse.cpp
  tinyhttp/websock.cpp
  tinyhttp/http.cpp
  tinyhttp/http.hpp
  tinyxml2/tinyxml2.cpp
  tinyxml2/tinyxml2.h
  utilfunc.cpp
  esctoolhelpers.cpp
  esixmlparsing.cpp
  soesconfigwriter.cpp
  siidecode.cpp
  siiencode.cpp
  main.cpp
  )
