cmake_minimum_required(VERSION 3.21)

project(scheduling VERSION 0.1.0.0 LANGUAGES CXX)

include(CMakeDependentOption)

if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

option(SCHEDULING_BUILD_BENCHMARKS "Build benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(SCHEDULING_BUILD_DOCS "Build documentation" ${PROJECT_IS_TOP_LEVEL})
option(SCHEDULING_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

cmake_dependent_option(SCHEDULING_BUILD_TASKFLOW_BENCHMARK
  "Build taskflow benchmark" ${PROJECT_IS_TOP_LEVEL} "SCHEDULING_BUILD_BENCHMARKS" OFF
)

add_library(${PROJECT_NAME}
  include/scheduling/scheduling.hpp
  src/scheduling.cpp
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
  include
)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
  SCHEDULING_LIBRARY
)

if (BUILD_SHARED_LIBS)
  target_compile_definitions(${PROJECT_NAME}
    PUBLIC
    SCHEDULING_SHARED_LIBRARY
  )
endif()

if (SCHEDULING_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if (SCHEDULING_BUILD_DOCS)
  add_subdirectory(docs)
endif()

if (SCHEDULING_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include DESTINATION .)
