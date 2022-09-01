find_package(Catch2 REQUIRED)

include(CTest)
include(Catch)

add_library(catch_main STATIC catch_main.cpp)
target_link_libraries(catch_main PUBLIC Catch2::Catch2)
target_link_libraries(catch_main PRIVATE project_options)

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE project_warnings project_options catch_main FAMGraph)

catch_discover_tests(
  tests
  TEST_PREFIX
  "unittests."
  REPORTER
  xml
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)
