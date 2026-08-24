cmake_minimum_required(VERSION 3.14)
project(zaloid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(zaloid
  src/main.cpp
  src/auth.cpp
  src/config.cpp
  src/http_client.cpp
  src/zalo_api.cpp
  src/poller.cpp
  src/ocr_client.cpp
)

target_include_directories(zaloid PRIVATE include third_party)
target_link_libraries(zaloid PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
