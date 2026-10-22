cmake_minimum_required(VERSION 3.28)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version")

project(HelicoidManager VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(HS_WEBRTC_NATIVE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/core/webrtc-native/native")

option(HELICOID_MANAGER_REGISTRY_TESTS_ONLY
  "Configure only the standard-library Manager registry, preview metrics, and native runtime tests"
  OFF
)

function(add_manager_registry_tests)
  if(BUILD_TESTING)
    add_executable(manager_registry_tests
      src/deployment_registry.cpp
      tests/deployment_registry_tests.cpp
    )
    target_compile_features(manager_registry_tests PRIVATE cxx_std_20)
    target_include_directories(manager_registry_tests PRIVATE src)
    if(MSVC)
      target_compile_options(manager_registry_tests PRIVATE /W4 /WX)
    else()
      target_compile_options(manager_registry_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
    add_test(NAME manager_registry_tests COMMAND manager_registry_tests)
  endif()
endfunction()

function(add_manager_preview_metrics_tests)
  if(BUILD_TESTING)
    add_executable(manager_preview_metrics_tests
      tests/computer_preview_metrics_tests.cpp
    )
    target_compile_features(manager_preview_metrics_tests PRIVATE cxx_std_20)
    target_include_directories(manager_preview_metrics_tests PRIVATE src)
    if(MSVC)
      target_compile_options(manager_preview_metrics_tests PRIVATE /W4 /WX)
    else()
      target_compile_options(manager_preview_metrics_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
    add_test(NAME manager_preview_metrics_tests COMMAND manager_preview_metrics_tests)
  endif()
endfunction()

function(add_manager_webrtc_runtime_tests)
  if(BUILD_TESTING)
    add_library(manager_webrtc_abi_valid SHARED tests/native_webrtc_abi_fake.cpp)
    add_library(manager_webrtc_abi_mismatch SHARED tests/native_webrtc_abi_fake.cpp)
    add_library(manager_webrtc_abi_incomplete SHARED tests/native_webrtc_abi_fake.cpp)
    add_library(manager_webrtc_abi_missing_export SHARED tests/native_webrtc_abi_fake.cpp)
    add_library(manager_webrtc_abi_missing_connection_export SHARED tests/native_webrtc_abi_fake.cpp)
    add_library(manager_webrtc_abi_missing_observation_export SHARED tests/native_webrtc_abi_fake.cpp)
    add_library(manager_webrtc_abi_capability_failure SHARED tests/native_webrtc_abi_fake.cpp)
    foreach(fake_target IN ITEMS
        manager_webrtc_abi_valid
        manager_webrtc_abi_mismatch
        manager_webrtc_abi_incomplete
        manager_webrtc_abi_missing_export
        manager_webrtc_abi_missing_connection_export
        manager_webrtc_abi_missing_observation_export
        manager_webrtc_abi_capability_failure)
      target_include_directories(${fake_target} PRIVATE "${HS_WEBRTC_NATIVE_INCLUDE}")
      target_compile_definitions(${fake_target} PRIVATE HS_WEBRTC_EXPORTS)
      if(MSVC)
        target_compile_options(${fake_target} PRIVATE /W4 /WX)
      else()
        target_compile_options(${fake_target} PRIVATE -Wall -Wextra -Werror -pedantic)
      endif()
    endforeach()
    target_compile_definitions(manager_webrtc_abi_mismatch PRIVATE HS_TEST_FAKE_ABI_VERSION=2)
    target_compile_definitions(manager_webrtc_abi_incomplete PRIVATE HS_TEST_FAKE_CAPABILITIES=11u)
    target_compile_definitions(manager_webrtc_abi_valid PRIVATE
      HS_TEST_ENGINE_INIT_SUCCESS HS_TEST_PEER_CREATE_SUCCESS
      HS_TEST_PEER_DRAIN_TIMEOUT_ONCE HS_TEST_ENGINE_SHUTDOWN_TIMEOUT)
    target_compile_definitions(manager_webrtc_abi_missing_export PRIVATE HS_TEST_OMIT_CAPABILITY_EXPORT)
    target_compile_definitions(manager_webrtc_abi_missing_connection_export PRIVATE HS_TEST_OMIT_PEER_CREATE_OFFER)
    target_compile_definitions(manager_webrtc_abi_missing_observation_export PRIVATE
      HS_TEST_FAKE_CAPABILITIES=127u HS_TEST_OMIT_MEDIA_FRAME_EXPORT)
    target_compile_definitions(manager_webrtc_abi_capability_failure PRIVATE
      HS_TEST_CAPABILITY_STATUS=HS_STATUS_UNAVAILABLE)

    add_executable(manager_webrtc_runtime_tests
      src/native_webrtc_runtime.cpp
      tests/native_webrtc_runtime_tests.cpp
    )
    target_compile_features(manager_webrtc_runtime_tests PRIVATE cxx_std_20)
    target_include_directories(manager_webrtc_runtime_tests PRIVATE src "${HS_WEBRTC_NATIVE_INCLUDE}")
    target_compile_definitions(manager_webrtc_runtime_tests PRIVATE
      HELICOID_MANAGER_NATIVE_WEBRTC_TESTING)
    target_link_libraries(manager_webrtc_runtime_tests PRIVATE ${CMAKE_DL_LIBS})
    if(MSVC)
      target_compile_options(manager_webrtc_runtime_tests PRIVATE /W4 /WX)
    else()
      target_compile_options(manager_webrtc_runtime_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
    add_dependencies(manager_webrtc_runtime_tests
      manager_webrtc_abi_valid
      manager_webrtc_abi_mismatch
      manager_webrtc_abi_incomplete
      manager_webrtc_abi_missing_export
      manager_webrtc_abi_missing_connection_export
      manager_webrtc_abi_missing_observation_export
      manager_webrtc_abi_capability_failure
    )
    add_test(NAME manager_webrtc_runtime_tests
      COMMAND manager_webrtc_runtime_tests
        "$<TARGET_FILE:manager_webrtc_abi_valid>"
        "$<TARGET_FILE:manager_webrtc_abi_mismatch>"
        "$<TARGET_FILE:manager_webrtc_abi_incomplete>"
        "$<TARGET_FILE:manager_webrtc_abi_missing_export>"
        "$<TARGET_FILE:manager_webrtc_abi_missing_connection_export>"
        "$<TARGET_FILE:manager_webrtc_abi_missing_observation_export>"
        "$<TARGET_FILE:manager_webrtc_abi_capability_failure>"
    )
  endif()
endfunction()

if(HELICOID_MANAGER_REGISTRY_TESTS_ONLY)
  add_manager_registry_tests()
  add_manager_preview_metrics_tests()
  add_manager_webrtc_runtime_tests()
  return()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.10...<6.11 REQUIRED COMPONENTS Widgets Network Concurrent)
find_program(CARGO_EXECUTABLE cargo REQUIRED)

include(FetchContent)
FetchContent_Declare(
  saribbon_source
  GIT_REPOSITORY https://github.com/czyt1988/SARibbon.git
  GIT_TAG 806e3e93be4dd7676697d3017282a4359519e053
  GIT_SHALLOW FALSE
)
FetchContent_GetProperties(saribbon_source)
if(NOT saribbon_source_POPULATED)
  FetchContent_Populate(saribbon_source)
endif()
set(MANAGER_SARIBBON_LICENSE "${saribbon_source_SOURCE_DIR}/LICENSE")
if(NOT EXISTS "${MANAGER_SARIBBON_LICENSE}")
  message(FATAL_ERROR "The pinned SARibbon source must provide its LICENSE file")
endif()
install(FILES "${MANAGER_SARIBBON_LICENSE}" DESTINATION licenses/SARibbon)

# SARibbon documents SARibbon.h/SARibbon.cpp as its static integration seam.
# Keeping that pinned seam avoids pulling its optional custom window-frame layer
# into the manager; Qt retains ownership of native window controls.
add_library(saribbon STATIC "${saribbon_source_SOURCE_DIR}/src/SARibbon.cpp")
target_include_directories(saribbon PUBLIC "${saribbon_source_SOURCE_DIR}/src")
target_link_libraries(saribbon PUBLIC Qt6::Widgets)
target_compile_definitions(saribbon PRIVATE SARIBBON_USE_3RDPARTY_FRAMELESSHELPER=0)

set(MANAGER_AUTH_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/native-auth/Cargo.toml")
set(MANAGER_AUTH_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/native-auth-target")
set(MANAGER_AUTH_DEPENDS
  "${MANAGER_AUTH_MANIFEST}"
  "${CMAKE_CURRENT_SOURCE_DIR}/native-auth/Cargo.lock"
  "${CMAKE_CURRENT_SOURCE_DIR}/native-auth/src/lib.rs"
)

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  set(MANAGER_AUTH_ARCHIVES)
  foreach(architecture IN LISTS CMAKE_OSX_ARCHITECTURES)
    if(architecture STREQUAL "arm64")
      set(rust_target "aarch64-apple-darwin")
    elseif(architecture STREQUAL "x86_64")
      set(rust_target "x86_64-apple-darwin")
    else()
      message(FATAL_ERROR "Unsupported macOS Rust target architecture: ${architecture}")
    endif()
    set(archive "${MANAGER_AUTH_TARGET_DIR}/${rust_target}/release/libhs_manager_auth.a")
    add_custom_command(
      OUTPUT "${archive}"
      COMMAND "${CARGO_EXECUTABLE}" build --locked --release
              --manifest-path "${MANAGER_AUTH_MANIFEST}"
              --target "${rust_target}"
              --target-dir "${MANAGER_AUTH_TARGET_DIR}"
      DEPENDS ${MANAGER_AUTH_DEPENDS}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/native-auth"
      VERBATIM
    )
    list(APPEND MANAGER_AUTH_ARCHIVES "${archive}")
  endforeach()
  list(LENGTH CMAKE_OSX_ARCHITECTURES architecture_count)
  if(architecture_count GREATER 1)
    find_program(LIPO_EXECUTABLE lipo REQUIRED)
    set(MANAGER_AUTH_LIBRARY "${MANAGER_AUTH_TARGET_DIR}/universal/libhs_manager_auth.a")
    add_custom_command(
      OUTPUT "${MANAGER_AUTH_LIBRARY}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${MANAGER_AUTH_TARGET_DIR}/universal"
      COMMAND "${LIPO_EXECUTABLE}" -create ${MANAGER_AUTH_ARCHIVES} -output "${MANAGER_AUTH_LIBRARY}"
      DEPENDS ${MANAGER_AUTH_ARCHIVES}
      VERBATIM
    )
  else()
    list(GET MANAGER_AUTH_ARCHIVES 0 MANAGER_AUTH_LIBRARY)
  endif()
elseif(WIN32)
  if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(rust_target "i686-pc-windows-msvc")
    else()
      set(rust_target "x86_64-pc-windows-msvc")
    endif()
  else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(rust_target "i686-pc-windows-gnu")
    else()
      set(rust_target "x86_64-pc-windows-gnu")
    endif()
  endif()
  if(MSVC)
    set(MANAGER_AUTH_LIBRARY "${MANAGER_AUTH_TARGET_DIR}/${rust_target}/release/hs_manager_auth.lib")
  else()
    set(MANAGER_AUTH_LIBRARY "${MANAGER_AUTH_TARGET_DIR}/${rust_target}/release/libhs_manager_auth.a")
  endif()
  add_custom_command(
    OUTPUT "${MANAGER_AUTH_LIBRARY}"
    COMMAND "${CARGO_EXECUTABLE}" build --locked --release
            --manifest-path "${MANAGER_AUTH_MANIFEST}"
            --target "${rust_target}"
            --target-dir "${MANAGER_AUTH_TARGET_DIR}"
    DEPENDS ${MANAGER_AUTH_DEPENDS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/native-auth"
    VERBATIM
  )
else()
  set(MANAGER_AUTH_LIBRARY "${MANAGER_AUTH_TARGET_DIR}/release/libhs_manager_auth.a")
  add_custom_command(
    OUTPUT "${MANAGER_AUTH_LIBRARY}"
    COMMAND "${CARGO_EXECUTABLE}" build --locked --release
            --manifest-path "${MANAGER_AUTH_MANIFEST}"
            --target-dir "${MANAGER_AUTH_TARGET_DIR}"
    DEPENDS ${MANAGER_AUTH_DEPENDS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/native-auth"
    VERBATIM
  )
endif()

add_custom_target(manager_auth_build DEPENDS "${MANAGER_AUTH_LIBRARY}")
add_library(manager_auth STATIC IMPORTED GLOBAL)
set_target_properties(manager_auth PROPERTIES IMPORTED_LOCATION "${MANAGER_AUTH_LIBRARY}")

add_executable(HelicoidManager
  src/main.cpp
  src/main_window.cpp
  src/computer_preview_metrics.h
  src/deployment_registry.cpp
  src/manager_signing_identity.cpp
  src/observation_admission.cpp
  src/native_webrtc_runtime.cpp
  src/ssh_setup_preflight.cpp
  resources/manager.qrc
)
target_include_directories(HelicoidManager PRIVATE src "${HS_WEBRTC_NATIVE_INCLUDE}")
target_link_libraries(HelicoidManager PRIVATE
  saribbon manager_auth Qt6::Widgets Qt6::Network Qt6::Concurrent ${CMAKE_DL_LIBS})
add_dependencies(HelicoidManager manager_auth_build)

add_manager_registry_tests()
add_manager_preview_metrics_tests()
add_manager_webrtc_runtime_tests()

if(APPLE)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  target_link_libraries(HelicoidManager PRIVATE "${SECURITY_FRAMEWORK}" "${COREFOUNDATION_FRAMEWORK}")
elseif(WIN32)
  target_link_libraries(HelicoidManager PRIVATE advapi32 bcrypt)
endif()

if(APPLE)
  set_source_files_properties(resources/brand/macos/Manager.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
  )
  target_sources(HelicoidManager PRIVATE resources/brand/macos/Manager.icns)
  set_target_properties(HelicoidManager PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "Manager.icns"
    MACOSX_BUNDLE_GUI_IDENTIFIER "systems.helicoid.manager.dev"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  )
endif()

if(WIN32)
  enable_language(RC)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/brand/windows/manager.ico"
    MANAGER_ICON_NATIVE_PATH
  )
  string(REPLACE "\\" "\\\\" MANAGER_ICON_NATIVE_PATH "${MANAGER_ICON_NATIVE_PATH}")
  configure_file(resources/brand/windows/manager.rc.in
    "${CMAKE_CURRENT_BINARY_DIR}/manager.rc" @ONLY
  )
  target_sources(HelicoidManager PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/manager.rc")
endif()

install(TARGETS HelicoidManager
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin
)

if(APPLE)
  set(MANAGER_WEBRTC_DYLIB "$ENV{HELICOID_MANAGER_WEBRTC_DYLIB}")
  if(MANAGER_WEBRTC_DYLIB)
    if(NOT IS_ABSOLUTE "${MANAGER_WEBRTC_DYLIB}" OR NOT EXISTS "${MANAGER_WEBRTC_DYLIB}")
      message(FATAL_ERROR
        "HELICOID_MANAGER_WEBRTC_DYLIB must name an existing absolute pinned native library"
      )
    endif()
    get_filename_component(MANAGER_WEBRTC_DYLIB_NAME "${MANAGER_WEBRTC_DYLIB}" NAME)
    if(NOT MANAGER_WEBRTC_DYLIB_NAME STREQUAL "libhs_webrtc_shared.dylib")
      message(FATAL_ERROR
        "HELICOID_MANAGER_WEBRTC_DYLIB must be named libhs_webrtc_shared.dylib"
      )
    endif()
    install(FILES "${MANAGER_WEBRTC_DYLIB}"
      DESTINATION "HelicoidManager.app/Contents/Frameworks"
    )
  elseif("$ENV{HELICOID_MANAGER_REQUIRE_WEBRTC}" STREQUAL "1")
    message(FATAL_ERROR
      "The Manager package requires the pinned libhs_webrtc_shared.dylib"
    )
  endif()
endif()

get_filename_component(MANAGER_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
if(APPLE)
  find_program(MANAGER_XCRUN_EXECUTABLE xcrun REQUIRED)
  find_program(MANAGER_PLUTIL_EXECUTABLE plutil REQUIRED)
  find_program(MANAGER_QT_DEPLOY_TOOL
    NAMES macdeployqt
    HINTS "$ENV{QT6_ROOT}/bin" "${MANAGER_QT_PREFIX}/bin"
    NO_DEFAULT_PATH
    REQUIRED
  )
  set(MANAGER_ICON_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/brand/source/HS Manager App icon.icon"
  )
  set(MANAGER_ICON_NAME "HS Manager App icon")
  set(MANAGER_ICON_FALLBACK
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/brand/macos/Manager.icns"
  )
  set(MANAGER_ICON_PROVENANCE
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/brand/macos/PROVENANCE.json"
  )
  set(MANAGER_DEPLOY_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/deploy-manager.cmake")
  configure_file(cmake/deploy-macos.cmake.in "${MANAGER_DEPLOY_SCRIPT}" @ONLY)
  install(SCRIPT "${MANAGER_DEPLOY_SCRIPT}")

  list(LENGTH CMAKE_OSX_ARCHITECTURES MANAGER_PACKAGE_ARCH_COUNT)
  if(MANAGER_PACKAGE_ARCH_COUNT GREATER 1)
    set(MANAGER_PACKAGE_ARCH "universal")
  elseif(CMAKE_OSX_ARCHITECTURES)
    set(MANAGER_PACKAGE_ARCH "${CMAKE_OSX_ARCHITECTURES}")
  else()
    set(MANAGER_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_PACKAGE_FILE_NAME
    "HelicoidManager-${PROJECT_VERSION}-macos-${MANAGER_PACKAGE_ARCH}")
  set(CPACK_DMG_VOLUME_NAME "Helicoid Manager")
elseif(WIN32)
  find_program(MANAGER_QT_DEPLOY_TOOL
    NAMES windeployqt
    HINTS "$ENV{QT6_ROOT}/bin" "${MANAGER_QT_PREFIX}/bin"
    NO_DEFAULT_PATH
    REQUIRED
  )
  set(MANAGER_DEPLOY_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/deploy-manager.cmake")
  configure_file(cmake/deploy-windows.cmake.in "${MANAGER_DEPLOY_SCRIPT}" @ONLY)
  install(SCRIPT "${MANAGER_DEPLOY_SCRIPT}")

  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The Windows Manager package currently supports x64 only")
  endif()
  set(CPACK_GENERATOR "ZIP")
  set(CPACK_PACKAGE_FILE_NAME "HelicoidManager-${PROJECT_VERSION}-windows-x64")
endif()

if(APPLE OR WIN32)
  set(CPACK_PACKAGE_NAME "HelicoidManager")
  set(CPACK_PACKAGE_VENDOR "Helicoid Systems")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Helicoid multi-server computer management console")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  include(CPack)
endif()
