set_project("call_stream")
set_version("0.1.0")

option("host_project")
    set_default(false)
    set_showmenu(true)
    set_description("Build call_stream as the host project, including tests and local tuning")
option_end()

option("ubsan")
    set_default(false)
    set_showmenu(true)
    set_description("Enable UndefinedBehaviorSanitizer for Clang host builds")
option_end()

set_policy("build.c++.modules", true)
add_rules("mode.debug", "mode.release")

set_encodings("utf-8")
set_policy("build.warning", true)

if has_config("host_project") then
    local use_ubsan = has_config("ubsan")
    local toolchain = get_config("toolchain")

    if use_ubsan then
        if toolchain ~= "clang" and toolchain ~= "clang-cl" then
            raise("ubsan requires --toolchain=clang or --toolchain=clang-cl")
        end
        if is_plat("windows") and not is_mode("release") then
            raise("ubsan on Windows requires -m release because LLVM's UBSan runtime uses the release static CRT")
        end

        add_cxxflags("-fsanitize=undefined", "-fno-sanitize-recover=undefined", {force = true})
        add_ldflags("-fsanitize=undefined", {force = true})
    end

    set_symbols("debug")
    set_strip("debug")
    if is_mode("release") then
        set_optimize("fastest")
    end

    add_vectorexts("avx", "avx2")

    if is_plat("windows") then
        -- LLVM's Windows UBSan runtime is only available here as an MT static library.
        set_runtimes(use_ubsan and "MT" or (is_mode("debug") and "MDd" or "MD"))
    else
        set_runtimes("c++_shared")
    end

    add_requires("gtest", "benchmark")
end

target("call_stream")
set_kind("moduleonly")
set_languages("c++latest")


set_warnings("all", "pedantic")

add_files("src/**.ixx", {public = true})
target_end()

if has_config("host_project") then
    target("call_stream.example")
    set_kind("binary")
    set_extension(".exe")
    set_languages("c++latest")

    add_deps("call_stream")

    set_warnings("all", "pedantic")

    add_files("example.cpp")
    target_end()

    target("call_stream.test")
    set_kind("binary")
    set_extension(".exe")
    set_languages("c++latest")

    add_deps("call_stream")

    set_warnings("all", "pedantic")

    add_files("test/*_test.cpp")
    add_packages("gtest")
target_end()

    target("call_stream.benchmark")
    set_kind("binary")
    set_extension(".exe")
    set_languages("c++latest")

    add_deps("call_stream")

    set_warnings("all", "pedantic")

    add_files("test/benchmark/**.cpp")
    add_packages("benchmark")
    target_end()

    target("call_stream.profile")
    set_kind("binary")
    set_extension(".exe")
    set_languages("c++latest")
    set_symbols("debug")
    set_optimize("fastest")

    add_deps("call_stream")

    set_warnings("all", "pedantic")

    add_files("test/profile/**.cpp")
    target_end()
end
