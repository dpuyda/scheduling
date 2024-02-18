cmake_minimum_required(VERSION 3.5)

project(scheduling VERSION 0.1.0.0)

include(CMakeDependentOption)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_BENCHMARKS "Build benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_DOCS "Build documentation" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
cmake_dependent_option(BUILD_TASKFLOW_BENCHMARK "Build taskflow benchmark" ${PROJECT_IS_TOP_LEVEL} "BUILD_BENCHMARKS" OFF)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include DESTINATION .)

if (BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if (BUILD_DOCS)
  add_subdirectory(docs)
endif()

if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
