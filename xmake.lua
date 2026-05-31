set_project("call_stream")
set_version("0.1.0")

option("host_project")
    set_default(false)
    set_showmenu(true)
    set_description("Build call_stream as the host project, including tests and local tuning")
option_end()

set_policy("build.c++.modules", true)
add_rules("mode.debug", "mode.release")

set_encodings("utf-8")
set_policy("build.warning", true)

if has_config("host_project") then
    set_symbols("debug")
    set_strip("debug")
    if is_mode("release") then
        set_optimize("fastest")
    end

    add_vectorexts("avx", "avx2")
    add_requires("gtest", "benchmark")

    if is_plat("windows") then
        set_runtimes(is_mode("debug") and "MDd" or "MD")
    else
        set_runtimes("c++_shared")
    end
end

target("call_stream")
set_kind("moduleonly")
set_languages("c++latest")


set_warnings("all", "pedantic")

add_files("src/**.ixx", {public = true})
target_end()

if has_config("host_project") then
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
