cmake_minimum_required(VERSION 4.2.2)

# CMake 4.2.2 still gates std module support behind this exact feature value.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

if(DEFINED host_project AND NOT DEFINED CALL_STREAM_HOST_PROJECT)
    set(CALL_STREAM_HOST_PROJECT "${host_project}" CACHE BOOL
        "Build call_stream as the host project, including tests and local tuning")
endif()

option(CALL_STREAM_HOST_PROJECT
    "Build call_stream as the host project, including tests and local tuning" OFF)

set(CALL_STREAM_CXX_STANDARD 23 CACHE STRING "C++ standard level used for call_stream targets")
set_property(CACHE CALL_STREAM_CXX_STANDARD PROPERTY STRINGS 23 26)

option(CALL_STREAM_FETCH_DEPENDENCIES
    "Fetch host-project test and benchmark dependencies when find_package cannot locate them" ON)

set(CALL_STREAM_GTEST_TAG "v1.17.0" CACHE STRING "googletest tag used by FetchContent")
set(CALL_STREAM_BENCHMARK_TAG "v1.9.5" CACHE STRING "google/benchmark tag used by FetchContent")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(CALL_STREAM_HOST_PROJECT)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
endif()

project(call_stream VERSION 0.1.0 LANGUAGES CXX)

if(NOT CALL_STREAM_CXX_STANDARD MATCHES "^(23|26)$")
    message(FATAL_ERROR "CALL_STREAM_CXX_STANDARD must be 23 or 26.")
endif()

if(NOT CALL_STREAM_CXX_STANDARD IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    set(_call_stream_import_std_message
        "${CMAKE_CXX${CALL_STREAM_CXX_STANDARD}_COMPILER_IMPORT_STD_NOT_FOUND_MESSAGE}")
    if(_call_stream_import_std_message)
        string(PREPEND _call_stream_import_std_message "\nReason reported by CMake: ")
    endif()
    message(FATAL_ERROR
        "call_stream uses 'import std;', but CMake did not detect std module support "
        "for C++${CALL_STREAM_CXX_STANDARD}. Use a supported compiler and a generator "
        "with imported C++ module support, such as Ninja with a recent MSVC, Clang, or GCC."
        "${_call_stream_import_std_message}")
endif()

set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if(TARGET "__cmake_cxx${CALL_STREAM_CXX_STANDARD}")
    target_compile_options("__cmake_cxx${CALL_STREAM_CXX_STANDARD}" PRIVATE
        "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/utf-8>")
endif()

function(call_stream_configure_cxx_target target scope)
    target_compile_features("${target}" "${scope}" "cxx_std_${CALL_STREAM_CXX_STANDARD}")

    set_target_properties("${target}" PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON)

    target_compile_options("${target}" PRIVATE
        "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/utf-8>"
        "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/W3>"
        "$<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-Wall>"
        "$<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-Wextra>"
        "$<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-Wpedantic>")
endfunction()

function(call_stream_enable_host_tuning target)
    target_compile_options("${target}" PRIVATE
        "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/arch:AVX2>"
        "$<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-mavx2>"
        "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:-g>")
endfunction()

add_library(call_stream OBJECT)
add_library(call_stream::call_stream ALIAS call_stream)

target_sources(call_stream
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/call_stream.ixx")

call_stream_configure_cxx_target(call_stream PUBLIC)

if(CALL_STREAM_HOST_PROJECT)
    call_stream_enable_host_tuning(call_stream)
endif()

function(call_stream_provide_gtest)
    find_package(GTest CONFIG QUIET)

    if(NOT TARGET GTest::gtest)
        if(CALL_STREAM_FETCH_DEPENDENCIES)
            include(FetchContent)
            set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
            set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

            FetchContent_Declare(googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG "${CALL_STREAM_GTEST_TAG}"
                GIT_SHALLOW TRUE
                FIND_PACKAGE_ARGS CONFIG)
            FetchContent_MakeAvailable(googletest)
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()

    if(NOT TARGET GTest::gtest AND TARGET gtest)
        add_library(GTest::gtest ALIAS gtest)
    endif()

    if(NOT TARGET GTest::gtest)
        message(FATAL_ERROR "GTest::gtest target was not provided.")
    endif()
endfunction()

function(call_stream_provide_benchmark)
    find_package(benchmark CONFIG QUIET)

    if(NOT TARGET benchmark::benchmark)
        if(CALL_STREAM_FETCH_DEPENDENCIES)
            include(FetchContent)
            set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
            set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
            set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

            FetchContent_Declare(benchmark
                GIT_REPOSITORY https://github.com/google/benchmark.git
                GIT_TAG "${CALL_STREAM_BENCHMARK_TAG}"
                GIT_SHALLOW TRUE
                FIND_PACKAGE_ARGS CONFIG)
            FetchContent_MakeAvailable(benchmark)
        else()
            find_package(benchmark CONFIG REQUIRED)
        endif()
    endif()

    if(NOT TARGET benchmark::benchmark AND TARGET benchmark)
        add_library(benchmark::benchmark ALIAS benchmark)
    endif()

    if(NOT TARGET benchmark::benchmark)
        message(FATAL_ERROR "benchmark::benchmark target was not provided.")
    endif()
endfunction()

if(CALL_STREAM_HOST_PROJECT)
    enable_testing()

    add_executable(call_stream.example example.cpp)
    call_stream_configure_cxx_target(call_stream.example PRIVATE)
    call_stream_enable_host_tuning(call_stream.example)
    set_target_properties(call_stream.example PROPERTIES SUFFIX ".exe")
    target_link_libraries(call_stream.example PRIVATE call_stream::call_stream)

    call_stream_provide_gtest()
    add_executable(call_stream.test test/call_stream_correctness_test.cpp)
    call_stream_configure_cxx_target(call_stream.test PRIVATE)
    call_stream_enable_host_tuning(call_stream.test)
    set_target_properties(call_stream.test PROPERTIES SUFFIX ".exe")
    target_link_libraries(call_stream.test PRIVATE call_stream::call_stream GTest::gtest)

    include(GoogleTest)
    gtest_discover_tests(call_stream.test)

    call_stream_provide_benchmark()
    add_executable(call_stream.benchmark test/benchmark/call_stream_benchmark.cpp)
    call_stream_configure_cxx_target(call_stream.benchmark PRIVATE)
    call_stream_enable_host_tuning(call_stream.benchmark)
    set_target_properties(call_stream.benchmark PROPERTIES SUFFIX ".exe")
    target_link_libraries(call_stream.benchmark PRIVATE call_stream::call_stream benchmark::benchmark)

    add_executable(call_stream.profile test/profile/call_stream_profile.cpp)
    call_stream_configure_cxx_target(call_stream.profile PRIVATE)
    call_stream_enable_host_tuning(call_stream.profile)
    set_target_properties(call_stream.profile PROPERTIES SUFFIX ".exe")
    target_link_libraries(call_stream.profile PRIVATE call_stream::call_stream)
endif()
