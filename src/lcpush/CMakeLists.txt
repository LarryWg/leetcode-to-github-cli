# Core library holds every module except main so tests can link it directly.
add_library(lcpush_core STATIC
  errors.hpp
  version.hpp
  version.cpp
  paths.hpp
  paths.cpp
  config.hpp
  config.cpp
  problems.hpp
  detect.hpp
  detect.cpp
  solution.hpp
  solution.cpp
  plausibility.hpp
  plausibility.cpp
  render.hpp
  render.cpp
  ui.hpp
  ui.cpp
  search.hpp
  search.cpp
  util/strings.hpp
  util/strings.cpp
  util/subprocess.hpp
  util/subprocess.cpp
  keyring.hpp
  keyring_common.cpp
  keyring_macos.cpp
  keyring_null.cpp
  tokens.hpp
  tokens.cpp
  clipboard.hpp
  clipboard.cpp
  editor.hpp
  editor.cpp
  http/transport.hpp
  http/curl_transport.hpp
  http/curl_transport.cpp
  github.hpp
  github.cpp
  problems.cpp
  util/time.hpp
  util/time.cpp
  term/terminal.hpp
  term/terminal.cpp
  term/raw_mode.hpp
  term/raw_mode.cpp
  term/keys.hpp
  term/keys.cpp
  term/render.hpp
  term/render.cpp
  prompts.hpp
  prompts.cpp
  picker.hpp
  picker.cpp
  flow.hpp
  flow.cpp
  onboarding.hpp
  onboarding.cpp
  session.hpp
  session.cpp
  cli.hpp
  cli.cpp
)

target_include_directories(lcpush_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(lcpush_core PUBLIC
  nlohmann_json::nlohmann_json
  tomlplusplus::tomlplusplus
  rapidfuzz::rapidfuzz
  CURL::libcurl
)

target_compile_definitions(lcpush_core PUBLIC LCPUSH_VERSION="${PROJECT_VERSION}")

if(APPLE)
  target_link_libraries(lcpush_core PUBLIC
    "-framework Security"
    "-framework CoreFoundation"
  )
endif()

target_link_libraries(lcpush_core PUBLIC CLI11::CLI11)

add_executable(lcpush main.cpp)
target_link_libraries(lcpush PRIVATE lcpush_core)

install(TARGETS lcpush RUNTIME DESTINATION bin)
