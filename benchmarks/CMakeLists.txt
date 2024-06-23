include(benchmark.cmake)

project(scheduling_benchmarks LANGUAGES CXX)

set(SCHEDULING_BENCHMARKS_SOURCES scheduling.cpp)

set(SCHEDULING_BENCHMARKS_LINK_LIBRARIES
  benchmark::benchmark
  benchmark::benchmark_main
  scheduling
)

if(SCHEDULING_BUILD_TASKFLOW_BENCHMARK)
  include(taskflow.cmake)
  set(SCHEDULING_BENCHMARKS_SOURCES
    ${BENCHMARKS_SOURCES}
    taskflow.cpp
  )
  set(SCHEDULING_BENCHMARKS_LINK_LIBRARIES
    ${SCHEDULING_BENCHMARKS_LINK_LIBRARIES}
    Taskflow
  )
endif()

add_executable(${PROJECT_NAME}
  ${SCHEDULING_BENCHMARKS_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  ${SCHEDULING_BENCHMARKS_LINK_LIBRARIES}
)
