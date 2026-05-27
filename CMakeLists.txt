cmake_minimum_required(VERSION 3.16)

project(TwitchFollowerChecker VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Compiler Options
# ==============================================================================
# Force MSVC to interpret source files as UTF-8 (prevents mojibake with Japanese comments)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==============================================================================
# Local Configuration Loading
# ==============================================================================
# Load local configuration (local Qt paths, specific build options, etc.)
# Copy local_config.cmake.example to local_config.cmake to customize.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local_config.cmake")
    message(STATUS "Loading local configuration: local_config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/local_config.cmake")
else()
    message(WARNING "local_config.cmake not found. If Qt6 is not found, copy local_config.cmake.example to local_config.cmake and set the paths.")
endif()

# ==============================================================================
# Twitch & TransCipher Credentials (Git-ignored configuration)
# ==============================================================================
if(NOT DEFINED TWITCH_DEFAULT_CLIENT_ID)
    set(TWITCH_DEFAULT_CLIENT_ID "aotnn8qp70c6s418ztbdauinxnf6sd") # Default fallback Client ID (BLUE000's Twitch Client ID)
endif()

if(NOT DEFINED TWITCH_DEFAULT_CLIENT_SECRET)
    set(TWITCH_DEFAULT_CLIENT_SECRET "")
endif()

if(NOT DEFINED TRANSCIPHER_BUILD_SECRET)
    set(TRANSCIPHER_BUILD_SECRET "BLUE000_BUILD_SECRET_KEY")
endif()

if(NOT DEFINED TRANSCIPHER_TOKEN_ISSUER_URL)
    set(TRANSCIPHER_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php")
endif()

# ==============================================================================
# Qt6 Settings
# ==============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# ==============================================================================
# TransCipher Library Settings
# ==============================================================================
set(TRANSCIPHER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/TransCipher-Dist)
include_directories(${TRANSCIPHER_DIR}/include)

# ==============================================================================
# Source Files
# ==============================================================================
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
    src/logger.h
    src/logger.cpp
    src/configmanager.h
    src/configmanager.cpp
    src/twitchauth.h
    src/twitchauth.cpp
    src/twitchapiclient.h
    src/twitchapiclient.cpp
    src/followermodel.h
    src/followermodel.cpp
)

# ==============================================================================
# Target Definitions: Main Executable
# ==============================================================================
if(WIN32)
    # Use WIN32 option to prevent console window from showing behind GUI on Windows
    add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

# Pass CMake managed version and Twitch/TransCipher credentials to C++ compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION_STRING="${PROJECT_VERSION}"
    TWITCH_DEFAULT_CLIENT_ID="${TWITCH_DEFAULT_CLIENT_ID}"
    TWITCH_DEFAULT_CLIENT_SECRET="${TWITCH_DEFAULT_CLIENT_SECRET}"
    TRANSCIPHER_BUILD_SECRET="${TRANSCIPHER_BUILD_SECRET}"
    TRANSCIPHER_TOKEN_ISSUER_URL="${TRANSCIPHER_TOKEN_ISSUER_URL}"
)

# Link Qt libraries and TransCipher Import Library (.dll.a for MinGW)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    "${TRANSCIPHER_DIR}/lib/libTransCipher.dll.a"
)

# Post-Build DLL Copy for Main App
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TRANSCIPHER_DIR}/lib/libTransCipher.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying libTransCipher.dll to executable directory..."
)

# ==============================================================================
# FetchContent for Google Test (GTest)
# ==============================================================================
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For MinGW/MSVC runtime compatibility
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ==============================================================================
# Target Definitions: Automated Test Executable
# ==============================================================================
set(TEST_SOURCES
    src/test/test_main.cpp
    src/logger.h
    src/logger.cpp
    src/configmanager.h
    src/configmanager.cpp
    src/twitchauth.h
    src/twitchauth.cpp
    src/twitchapiclient.h
    src/twitchapiclient.cpp
    src/followermodel.h
    src/followermodel.cpp
)

add_executable(TwitchFollowerCheckerTest ${TEST_SOURCES})

# Pass CMake managed version and Twitch/TransCipher credentials to Test C++ compile definitions
target_compile_definitions(TwitchFollowerCheckerTest PRIVATE
    APP_VERSION_STRING="${PROJECT_VERSION}"
    TWITCH_DEFAULT_CLIENT_ID="${TWITCH_DEFAULT_CLIENT_ID}"
    TWITCH_DEFAULT_CLIENT_SECRET="${TWITCH_DEFAULT_CLIENT_SECRET}"
    TRANSCIPHER_BUILD_SECRET="${TRANSCIPHER_BUILD_SECRET}"
    TRANSCIPHER_TOKEN_ISSUER_URL="${TRANSCIPHER_TOKEN_ISSUER_URL}"
)

target_include_directories(TwitchFollowerCheckerTest PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link GTest, Qt, and TransCipher
target_link_libraries(TwitchFollowerCheckerTest PRIVATE
    gtest
    gtest_main
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    "${TRANSCIPHER_DIR}/lib/libTransCipher.dll.a"
)

# Post-Build DLL Copy for Test Target
add_custom_command(TARGET TwitchFollowerCheckerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TRANSCIPHER_DIR}/lib/libTransCipher.dll"
    "$<TARGET_FILE_DIR:TwitchFollowerCheckerTest>"
    COMMENT "Copying libTransCipher.dll to test directory..."
)

# Setup executable installation destination (optional, for release builds)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
