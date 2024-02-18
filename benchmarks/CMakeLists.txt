project(benchmarks)

include(benchmark.cmake)
set(BENCHMARKS_SOURCES scheduling.cpp)
set(BENCHMARKS_LINK_LIBRARIES
  benchmark::benchmark
  benchmark::benchmark_main
  scheduling
)

if (BUILD_TASKFLOW_BENCHMARK)
  include(taskflow.cmake)
  set(BENCHMARKS_SOURCES ${BENCHMARKS_SOURCES} taskflow.cpp)
  set(BENCHMARKS_LINK_LIBRARIES ${BENCHMARKS_LINK_LIBRARIES} Taskflow)
endif()

add_executable(${PROJECT_NAME} ${BENCHMARKS_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${BENCHMARKS_LINK_LIBRARIES})
