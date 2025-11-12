cmake_minimum_required(VERSION 3.22)
project(
    PersonalNotes
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)

# Define the plugin target
add_commonlibsse_plugin(
    ${PROJECT_NAME}
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin.cpp
)

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/patch_includes.h" PATCH_HEADER_PATH_NATIVE)
target_compile_options(${PROJECT_NAME} PRIVATE /FI${PATCH_HEADER_PATH_NATIVE})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
    spdlog::spdlog
)

# Set properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/out"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/out"
)
