cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version} LANGUAGES C CXX)

include(compilerconfig)
include(defaults)
include(helpers)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Stream Chat OBS currently targets Windows x64 only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core Gui Widgets Network)
include(cmake/windows/chat-network.cmake)

add_library(${CMAKE_PROJECT_NAME} MODULE
  src/plugin-main.cpp
  src/chat_message.hpp
  src/oauth_loopback.hpp
  src/secure_store.hpp
  src/traffic.hpp
  src/usage.hpp
  src/twitch_chat.cpp
  src/twitch_chat.hpp
  src/kick_chat.cpp
  src/kick_chat.hpp
  src/kick_wire.hpp
  src/youtube_chat.cpp
  src/youtube_chat.hpp
  src/youtube_wire.hpp)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebSockets crypt32)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE STREAM_CHAT_VERSION="${PROJECT_VERSION}")
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /utf-8 /permissive-)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

enable_testing()
add_executable(stream-chat-usage-tests tests/test_usage.cpp)
target_compile_definitions(stream-chat-usage-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME measured-chat-cpu-counter COMMAND stream-chat-usage-tests)
add_executable(stream-chat-vault-tests tests/test_secure_store.cpp)
target_link_libraries(stream-chat-vault-tests PRIVATE Qt6::Core crypt32)
target_compile_definitions(stream-chat-vault-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME encrypted-local-account-storage COMMAND stream-chat-vault-tests)
set_tests_properties(encrypted-local-account-storage PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stream-chat-youtube-wire-tests tests/test_youtube_wire.cpp)
target_link_libraries(stream-chat-youtube-wire-tests PRIVATE Qt6::Core)
add_test(NAME youtube-stream-wire-format COMMAND stream-chat-youtube-wire-tests)
set_tests_properties(youtube-stream-wire-format PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stream-chat-oauth-tests tests/test_oauth_loopback.cpp src/oauth_loopback.hpp)
target_link_libraries(stream-chat-oauth-tests PRIVATE Qt6::Core Qt6::Network)
add_test(NAME secure-loopback-oauth COMMAND stream-chat-oauth-tests)
set_tests_properties(secure-loopback-oauth PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stream-chat-kick-wire-tests tests/test_kick_wire.cpp)
target_link_libraries(stream-chat-kick-wire-tests PRIVATE Qt6::Core)
add_test(NAME kick-chat-wire-format COMMAND stream-chat-kick-wire-tests)
set_tests_properties(kick-chat-wire-format PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stream-chat-kick-live-check tests/kick_live_check.cpp src/kick_chat.cpp src/kick_chat.hpp src/oauth_loopback.hpp)
target_compile_definitions(stream-chat-kick-live-check PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(stream-chat-kick-live-check PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets crypt32)

add_executable(stream-chat-kick-lifecycle-tests tests/test_kick_lifecycle.cpp src/kick_chat.cpp src/kick_chat.hpp src/oauth_loopback.hpp)
target_compile_definitions(stream-chat-kick-lifecycle-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(stream-chat-kick-lifecycle-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::WebSockets crypt32)
add_test(NAME kick-disabled-lifecycle COMMAND stream-chat-kick-lifecycle-tests)
set_tests_properties(kick-disabled-lifecycle PROPERTIES ENVIRONMENT_MODIFICATION
  "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::WebSockets>")

add_executable(stream-chat-ui-preview tests/ui_preview.cpp src/twitch_chat.cpp src/youtube_chat.cpp src/kick_chat.cpp src/oauth_loopback.hpp)
get_target_property(_obs_includes OBS::libobs INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_frontend_includes OBS::obs-frontend-api INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(stream-chat-ui-preview PRIVATE ${_obs_includes} ${_frontend_includes})
target_compile_definitions(stream-chat-ui-preview PRIVATE STREAM_CHAT_PREVIEW STREAM_CHAT_VERSION="${PROJECT_VERSION}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(stream-chat-ui-preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets crypt32)
if(MSVC)
  target_compile_options(stream-chat-ui-preview PRIVATE /W4 /utf-8 /permissive-)
endif()

add_executable(stream-chat-personal-app-tests tests/test_personal_apps.cpp src/twitch_chat.cpp src/youtube_chat.cpp src/oauth_loopback.hpp)
target_compile_definitions(stream-chat-personal-app-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(stream-chat-personal-app-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::WebSockets crypt32)
add_test(NAME personal-platform-applications COMMAND stream-chat-personal-app-tests)
set_tests_properties(personal-platform-applications PROPERTIES ENVIRONMENT_MODIFICATION
  "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::WebSockets>")
