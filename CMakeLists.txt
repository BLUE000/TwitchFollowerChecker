cmake_minimum_required(VERSION 3.16)

project(TwitchFollowerChecker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# Qt6 Settings
# ==============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ==============================================================================
# Source Files
# ==============================================================================
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
)

# ==============================================================================
# Target Definitions
# ==============================================================================
if(WIN32)
    # Use WIN32 option to prevent console window from showing behind GUI on Windows
    add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

# Pass CMake managed version to C++ compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Setup executable installation destination (optional, for release builds)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
