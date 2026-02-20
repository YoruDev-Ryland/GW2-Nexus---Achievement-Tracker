cmake_minimum_required(VERSION 3.20)
project(AchievementTracker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Nexus API header ──────────────────────────────────────────────────────────
FetchContent_Declare(
    nexus_api
    GIT_REPOSITORY https://github.com/RaidcoreGG/RCGG-lib-nexus-api.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nexus_api)

# ── Dear ImGui — MUST be pinned to v1.80 (see section 5) ─────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.80
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

# ── nlohmann/json (optional but very useful) ──────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# ── Embed icon.png as Win32 resource (optional — for quick-access icon) ───────
configure_file(src/resources.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resources.rc @ONLY)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/entry.cpp
    src/Shared.cpp
    src/Settings.cpp
    src/GW2Api.cpp
    src/UI.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/resources.rc

    # ImGui — compiled INTO the DLL, not linked as a library
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

add_library(AchievementTracker SHARED ${SOURCES})

target_include_directories(AchievementTracker PRIVATE
    src
    ${nexus_api_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)

target_link_libraries(AchievementTracker PRIVATE
    nlohmann_json::nlohmann_json
    winhttp
)

if(MSVC)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(AchievementTracker PRIVATE
        /W3 /WX- /MP /EHsc /permissive-
        /wd4189   # local var initialized but not referenced
    )
    # CRITICAL: NOMINMAX must be defined — windows.h min/max macros break std::
    target_compile_definitions(AchievementTracker PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_options(AchievementTracker PRIVATE /SUBSYSTEM:WINDOWS)
endif()

set_target_properties(AchievementTracker PROPERTIES
    OUTPUT_NAME "AchievementTracker"
    PREFIX      ""          # no "lib" prefix on MinGW
    SUFFIX      ".dll"
)
