cmake_minimum_required (VERSION 2.8.12)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
project (SLAVECONFIGTOOL)

set (SLAVECONFIGTOOL_VERSION_MAJOR 0)
set (SLAVECONFIGTOOL_VERSION_MINOR 1)
set (SLAVECONFIGTOOL_VERSION_PATCH 0)

# Include paths
include_directories(
  ${SLAVECONFIGTOOL_SOURCE_DIR}
  ${SLAVECONFIGTOOL_SOURCE_DIR}/tinyxml2
  )

set(CMAKE_CXX_FLAGS "-O0 -ggdb")

# Platform flags and sources
include(${CMAKE_SYSTEM_NAME} OPTIONAL)

message (STATUS "Building for ${CMAKE_SYSTEM_NAME}")

add_executable(esctool
  tinyxml2/tinyxml2.cpp
  tinyxml2/tinyxml2.h
  esctoolhelpers.cpp
  siiencode.cpp
  main.cpp
  )
