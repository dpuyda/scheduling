cmake_minimum_required(VERSION 3.21)

project(scheduling VERSION 0.1.0.0 LANGUAGES CXX)

include(CMakeDependentOption)
include(cmake/sanitizers.cmake)

if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# For GCC 12 and above, disable the warning about std::hardware_destructive_interference_size not being ABI-stable.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-interference-size")
  endif()
endif()

option(SCHEDULING_BUILD_BENCHMARKS "Build benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(SCHEDULING_BUILD_DOCS "Build documentation" ${PROJECT_IS_TOP_LEVEL})
option(SCHEDULING_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(SCHEDULING_ENABLE_ADDRESS_SANITIZER "Defines if address sanitizer should be enabled" OFF)
option(SCHEDULING_ENABLE_LEAK_SANITIZER "Defines if leak sanitizer should be enabled" OFF)
option(SCHEDULING_ENABLE_MEMORY_SANITIZER "Defines if memory sanitizer should be enabled" OFF)
option(SCHEDULING_ENABLE_THREAD_SANITIZER "Defines if thread sanitizer should be enabled" OFF)
option(SCHEDULING_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER "Defines if undefined behavior sanitizer should be enabled" OFF)

cmake_dependent_option(SCHEDULING_BUILD_TASKFLOW_BENCHMARKS
  "Build taskflow benchmark" ${PROJECT_IS_TOP_LEVEL} "SCHEDULING_BUILD_BENCHMARKS" OFF
)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (BUILD_SHARED_LIBS)
  message(WARNING "Scheduling does not support dynamic linking.")
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

scheduling_enable_sanitizers(${PROJECT_NAME}
  ${SCHEDULING_ENABLE_ADDRESS_SANITIZER}
  ${SCHEDULING_ENABLE_LEAK_SANITIZER}
  ${SCHEDULING_ENABLE_MEMORY_SANITIZER}
  ${SCHEDULING_ENABLE_THREAD_SANITIZER}
  ${SCHEDULING_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include DESTINATION .)
