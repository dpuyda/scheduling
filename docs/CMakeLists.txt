find_package(Doxygen)

if (DOXYGEN_FOUND)
  set(DOXYGEN_EXCLUDE_SYMBOLS scheduling::internal)
  set(DOXYGEN_HIDE_FRIEND_COMPOUNDS YES)
  set(DOXYGEN_SHOW_USED_FILES NO)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/main_page.md")
  doxygen_add_docs(docs
    ALL
    ${CMAKE_SOURCE_DIR}/include/scheduling
    ${DOXYGEN_USE_MDFILE_AS_MAINPAGE}
    COMMENT "Generate HTML documentation")
else()
  message(WARNING "Doxygen not found, documentation will not be generated")
endif()
