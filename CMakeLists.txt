cmake_minimum_required(VERSION 3.22)
project(
    BainosNighteyeFix 
    VERSION 1.0.0 
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)
find_package(enb-api CONFIG REQUIRED)

# Manual iniparser integration
find_path(INIPARSER_INCLUDE_DIR iniparser/iniparser.h)
find_library(INIPARSER_LIBRARY NAMES iniparser iniparser-static)

# --- Define the plugin target first ---
# This line creates the target named 'BainosNighteyeFix'
add_commonlibsse_plugin(
    ${PROJECT_NAME}
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin.cpp
)
# --------------------------------------

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/patch_includes.h" PATCH_HEADER_PATH_NATIVE)
target_compile_options(${PROJECT_NAME} PRIVATE /FI${PATCH_HEADER_PATH_NATIVE})

# --- Configure the new target (BainosNighteyeFix) ---

# 1. Link libraries
# Use ${PROJECT_NAME} (which is BainosNighteyeFix) instead of the undefined 'main'
target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
    spdlog::spdlog
    enb-api::enb-api
    ${INIPARSER_LIBRARY}
)

# Add iniparser include directory
target_include_directories(${PROJECT_NAME} PRIVATE
    ${INIPARSER_INCLUDE_DIR}
)

# 2. Include directories
# The preferred way to get include dirs from a modern package (using CONFIG)
# is to use target_link_libraries. If CommonLibSSE is non-standard, 
# you might need to use CommonLibSSE_INCLUDE_DIRS.
# It's safer to use target_include_directories only if the link is insufficient.

# Remove the target_include_directories call unless compilation fails
# target_include_directories(${PROJECT_NAME} PRIVATE ${CommonLibSSE_INCLUDE_DIRS})
# NOTE: CommonLibSSE::CommonLibSSE should generally handle includes automatically.


# 3. Set properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/out"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/out"
)