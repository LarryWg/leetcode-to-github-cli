# Core library holds every module except main so tests can link it directly.
add_library(lcpush_core STATIC
  # Errors and version info shared by every layer.
  core/errors.hpp
  core/version.hpp
  core/version.cpp

  # Generic helpers with no product knowledge.
  util/strings.hpp
  util/strings.cpp
  util/subprocess.hpp
  util/subprocess.cpp
  util/time.hpp
  util/time.cpp

  # HTTP transport seam and its libcurl implementation.
  http/transport.hpp
  http/curl_transport.hpp
  http/curl_transport.cpp

  # Low-level terminal: raw mode, key decoding, frame painting.
  term/terminal.hpp
  term/terminal.cpp
  term/raw_mode.hpp
  term/raw_mode.cpp
  term/keys.hpp
  term/keys.cpp
  term/render.hpp
  term/render.cpp

  # OS integration: storage locations, clipboard, editor, keyring.
  platform/paths.hpp
  platform/paths.cpp
  platform/clipboard.hpp
  platform/clipboard.cpp
  platform/editor.hpp
  platform/editor.cpp
  platform/keyring.hpp
  platform/keyring_common.cpp
  platform/keyring_macos.cpp
  platform/keyring_null.cpp

  # Config model and TOML persistence.
  config/config.hpp
  config/config.cpp

  # Pure domain logic over solution content.
  domain/detect.hpp
  domain/detect.cpp
  domain/solution.hpp
  domain/solution.cpp
  domain/plausibility.hpp
  domain/plausibility.cpp
  domain/render.hpp
  domain/render.cpp

  # LeetCode problem set: fetching, caching, fuzzy search.
  leetcode/problems.hpp
  leetcode/problems.cpp
  leetcode/search.hpp
  leetcode/search.cpp

  # GitHub API client and token resolution.
  github/client.hpp
  github/client.cpp
  github/tokens.hpp
  github/tokens.cpp

  # Interactive widgets and console output.
  ui/console.hpp
  ui/console.cpp
  ui/prompts.hpp
  ui/prompts.cpp
  ui/picker.hpp
  ui/picker.cpp

  # Application flows wiring everything together.
  app/flow.hpp
  app/flow.cpp
  app/onboarding.hpp
  app/onboarding.cpp
  app/session.hpp
  app/session.cpp
  app/cli.hpp
  app/cli.cpp
)

target_include_directories(lcpush_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(lcpush_core PUBLIC
  nlohmann_json::nlohmann_json
  tomlplusplus::tomlplusplus
  rapidfuzz::rapidfuzz
  CURL::libcurl
  CLI11::CLI11
)

target_compile_definitions(lcpush_core PUBLIC LCPUSH_VERSION="${PROJECT_VERSION}")

if(APPLE)
  target_link_libraries(lcpush_core PUBLIC
    "-framework Security"
    "-framework CoreFoundation"
  )
endif()

add_executable(lcpush main.cpp)
target_link_libraries(lcpush PRIVATE lcpush_core)

install(TARGETS lcpush RUNTIME DESTINATION bin)
