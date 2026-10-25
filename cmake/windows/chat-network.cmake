# Build the exact OBS Qt WebSockets module; never replace OBS Core/Gui/Network.
find_package(Qt6WebSockets QUIET)
if(NOT Qt6WebSockets_FOUND)
  include(FetchContent)
  FetchContent_Declare(qtwebsockets
    GIT_REPOSITORY https://github.com/qt/qtwebsockets.git
    GIT_TAG 451920600d7f0b8a4b458bba56a2dd303e587026
    SOURCE_SUBDIR unused)
  FetchContent_MakeAvailable(qtwebsockets)
  set(_ws_prefix "${CMAKE_BINARY_DIR}/qtwebsockets-install")
  execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${qtwebsockets_SOURCE_DIR}" -B "${CMAKE_BINARY_DIR}/qtwebsockets-build"
    -G "${CMAKE_GENERATOR}" -A "${CMAKE_GENERATOR_PLATFORM}"
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" "-DCMAKE_INSTALL_PREFIX=${_ws_prefix}"
    -DBUILD_WITH_PCH=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
    COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/qtwebsockets-build"
    --config Release --parallel COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/qtwebsockets-build"
    --config Release COMMAND_ERROR_IS_FATAL ANY)
  list(PREPEND CMAKE_PREFIX_PATH "${_ws_prefix}")
  set(Qt6WebSockets_DIR "${_ws_prefix}/lib/cmake/Qt6WebSockets" CACHE PATH "" FORCE)
endif()
find_package(Qt6WebSockets 6.11.1 EXACT REQUIRED)
get_filename_component(_qt_runtime_root "${Qt6_DIR}/../../.." ABSOLUTE)
find_file(_schannel_backend qschannelbackend.dll PATHS "${_qt_runtime_root}/plugins/tls" NO_DEFAULT_PATH REQUIRED)
install(FILES "$<TARGET_FILE:Qt6::WebSockets>" DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit")
install(FILES "${_schannel_backend}" DESTINATION "${CMAKE_PROJECT_NAME}/data/qt-plugins/tls")

