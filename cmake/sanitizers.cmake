# Source:
# https://github.com/cpp-best-practices/cmake_template/blob/main/cmake/Sanitizers.cmake

function(
  scheduling_enable_sanitizers
  project_name
  ENABLE_ADDRESS_SANITIZER
  ENABLE_LEAK_SANITIZER
  ENABLE_MEMORY_SANITIZER
  ENABLE_THREAD_SANITIZER
  ENABLE_UNDEFINED_BEHAVIOR_SANITIZER)

  message(STATUS ENABLE_ADDRESS_SANITIZER=${ENABLE_ADDRESS_SANITIZER})
  message(STATUS ENABLE_LEAK_SANITIZER=${ENABLE_LEAK_SANITIZER})
  message(STATUS ENABLE_MEMORY_SANITIZER=${ENABLE_MEMORY_SANITIZER})
  message(STATUS ENABLE_THREAD_SANITIZER=${ENABLE_THREAD_SANITIZER})
  message(STATUS ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=${ENABLE_UNDEFINED_BEHAVIOR_SANITIZER})

  set(SANITIZERS "")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    if(${ENABLE_ADDRESS_SANITIZER})
      list(APPEND SANITIZERS "address")
    endif()

    if(${ENABLE_LEAK_SANITIZER})
      list(APPEND SANITIZERS "leak")
    endif()

    if(${ENABLE_UNDEFINED_BEHAVIOR_SANITIZER})
      list(APPEND SANITIZERS "undefined")
    endif()

    if(${ENABLE_THREAD_SANITIZER})
      if("address" IN_LIST SANITIZERS OR "leak" IN_LIST SANITIZERS)
        message(FATAL_ERROR "Thread sanitizer does not work with Address and Leak sanitizer enabled")
      else()
        list(APPEND SANITIZERS "thread")
      endif()
    endif()

    if(${ENABLE_MEMORY_SANITIZER} AND CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
      message(
        WARNING
          "Memory sanitizer requires all the code (including libc++) to be MSan-instrumented otherwise it reports false positives"
      )
      if("address" IN_LIST SANITIZERS
         OR "thread" IN_LIST SANITIZERS
         OR "leak" IN_LIST SANITIZERS)
        message(FATAL_ERROR "Memory sanitizer does not work with Address, Thread or Leak sanitizer enabled")
      else()
        list(APPEND SANITIZERS "memory")
      endif()
    endif()
  elseif(MSVC)
    if(${ENABLE_ADDRESS_SANITIZER})
      list(APPEND SANITIZERS "address")
    endif()
    if(${ENABLE_LEAK_SANITIZER}
       OR ${ENABLE_MEMORY_SANITIZER}
       OR ${ENABLE_THREAD_SANITIZER}
       OR ${ENABLE_UNDEFINED_BEHAVIOR_SANITIZER})
      message(FATAL_ERROR "MSVC only supports address sanitizer")
    endif()
  endif()

  list(
    JOIN
    SANITIZERS
    ","
    LIST_OF_SANITIZERS)

  if(LIST_OF_SANITIZERS)
    if(NOT
       "${LIST_OF_SANITIZERS}"
       STREQUAL
       "")
      message(STATUS SANITIZERS=${LIST_OF_SANITIZERS})
      if(NOT MSVC)
        target_compile_options(${project_name} INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
        target_link_options(${project_name} INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
      else()
        string(FIND "$ENV{PATH}" "$ENV{VSINSTALLDIR}" index_of_vs_install_dir)
        if("${index_of_vs_install_dir}" STREQUAL "-1")
          message(
            SEND_ERROR
              "Using MSVC sanitizers requires setting the MSVC environment before building the project. Please manually open the MSVC command prompt and rebuild the project."
          )
        endif()
        target_compile_options(${project_name} INTERFACE /fsanitize=${LIST_OF_SANITIZERS} /Zi /INCREMENTAL:NO)
        target_compile_definitions(${project_name} INTERFACE _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        target_link_options(${project_name} INTERFACE /INCREMENTAL:NO)
      endif()
    endif()
  endif()
endfunction()
