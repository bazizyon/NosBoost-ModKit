cmake_minimum_required(VERSION 4.1)
project(NosBoostModKit)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
        nosboost_sdk
        GIT_REPOSITORY https://github.com/halilefe4228/NosSDK_-NosBoost-.git
        GIT_TAG main
)
FetchContent_MakeAvailable(nosboost_sdk)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
)
FetchContent_MakeAvailable(imgui)

add_library(ExampleMod SHARED
        ExampleMod/ExampleMod.cpp
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

target_include_directories(ExampleMod PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${nosboost_sdk_SOURCE_DIR}/classes
        ${nosboost_sdk_SOURCE_DIR}/classes/enums
        ${nosboost_sdk_SOURCE_DIR}/classes/structs
        ${nosboost_sdk_SOURCE_DIR}/packets
        ${nosboost_sdk_SOURCE_DIR}/packets/sub
        ${imgui_SOURCE_DIR}
)

target_compile_options(ExampleMod PRIVATE /EHsc /MP)

if(NOT DEFINED ENV{CI})
    set_target_properties(ExampleMod PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Nostale/mods"
    )
endif()
