project(examples)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
  ${CMAKE_SOURCE_DIR}/include/scheduling/scheduler.h
  examples.cpp
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
)
