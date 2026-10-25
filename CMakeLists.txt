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

add_library(${CMAKE_PROJECT_NAME} MODULE src/plugin-main.cpp src/twitch_chat.cpp src/twitch_chat.hpp)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebSockets crypt32)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE STREAM_CHAT_VERSION="${PROJECT_VERSION}")
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /utf-8 /permissive-)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

enable_testing()
add_executable(stream-chat-vault-tests tests/test_secure_store.cpp)
target_link_libraries(stream-chat-vault-tests PRIVATE Qt6::Core crypt32)
target_compile_definitions(stream-chat-vault-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME encrypted-local-account-storage COMMAND stream-chat-vault-tests)
set_tests_properties(encrypted-local-account-storage PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

