cmake_minimum_required(VERSION 3.24)

project(nen_world VERSION 0.1.0 LANGUAGES CXX)

option(NEN_BUILD_TESTS "Build unit tests" ON)
option(NEN_ENABLE_SANITIZERS "Enable address/UB sanitizers in Debug" ON)
option(NEN_ENABLE_CLANG_TIDY "Run clang-tidy during builds when available" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib REQUIRED)

if(NEN_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy /opt/homebrew/opt/llvm/bin/clang-tidy)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
    else()
        message(WARNING "NEN_ENABLE_CLANG_TIDY=ON but clang-tidy was not found")
    endif()
endif()

function(nen_apply_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    endif()
endfunction()

function(nen_enable_sanitizers target_name)
    if(NEN_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined)
            target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(nen_core
    src/nen/affinity.cpp
    src/nen/combat.cpp
    src/nen/hatsu.cpp
    src/nen/hatsu_spec.cpp
    src/nen/log.cpp
    src/nen/nen_system.cpp
    src/nen/quiz.cpp
    src/nen/techniques.cpp
    src/nen/training.cpp
)

target_include_directories(nen_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

nen_apply_warnings(nen_core)
nen_enable_sanitizers(nen_core)

add_library(nen_game
    src/game/attack_system.cpp
    src/game/game.cpp
    src/game/persistence.cpp
)

target_link_libraries(nen_game PUBLIC nen_core raylib)
target_include_directories(nen_game PUBLIC ${PROJECT_SOURCE_DIR}/include)
nen_apply_warnings(nen_game)
nen_enable_sanitizers(nen_game)

add_executable(nen_world src/main.cpp)
target_link_libraries(nen_world PRIVATE nen_game)
nen_apply_warnings(nen_world)
nen_enable_sanitizers(nen_world)

if(NEN_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(nen_tests tests/test_training.cpp)
    target_link_libraries(nen_tests PRIVATE nen_core)
    nen_apply_warnings(nen_tests)
    nen_enable_sanitizers(nen_tests)

    add_test(NAME nen_tests COMMAND nen_tests)
endif()
