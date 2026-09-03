add_executable(lcpush_tests
  test_errors.cpp
  test_paths.cpp
  test_config.cpp
  test_detect.cpp
  test_solution.cpp
  test_plausibility.cpp
  test_render.cpp
  test_ui.cpp
  test_search.cpp
  test_parity.cpp
  test_tokens.cpp
  test_input_sources.cpp
  test_github.cpp
  test_problems.cpp
  test_picker.cpp
  test_onboarding.cpp
  test_session.cpp
  test_cli.cpp
)

target_link_libraries(lcpush_tests PRIVATE lcpush_core Catch2::Catch2WithMain)
target_include_directories(lcpush_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lcpush_tests PRIVATE
  LCPUSH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

include(Catch)
catch_discover_tests(lcpush_tests)

add_test(NAME cli_version COMMAND lcpush --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "^lcpush ${PROJECT_VERSION}")
