include(googletest.cmake)

project(scheduling_tests)
add_executable(${PROJECT_NAME} thread_pool_test.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  gtest_main
  scheduling
)

add_test(${PROJECT_NAME} ${PROJECT_NAME})
