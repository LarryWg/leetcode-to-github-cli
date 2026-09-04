# All third-party dependencies, pinned by tag and fetched at configure time.
# libcurl is the one system dependency, resolved in the top-level CMakeLists.

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG 30172438cee64926dc41fdd9c11fb3ba5b2ba9de
)

FetchContent_Declare(
  rapidfuzz
  GIT_REPOSITORY https://github.com/rapidfuzz/rapidfuzz-cpp.git
  GIT_TAG 82662f3623b3ca3645e543f677fc32fb8bd1fb95
)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG 4160d259d961cd393fd8d67590a8c7d210207348
)

FetchContent_MakeAvailable(nlohmann_json tomlplusplus rapidfuzz CLI11)

if(LCPUSH_BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG 25319fd3047c6bdcf3c0170e76fa526c77f99ca9
  )
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
