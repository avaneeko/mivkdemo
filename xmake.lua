add_rules("mode.debug", "mode.release")

add_requires("vulkansdk")

target("camellia")
    set_kind("binary")
    set_languages("c17", "c++20")

    add_packages("vulkansdk")
    add_syslinks("user32", "gdi32")

    add_defines("_CRT_SECURE_NO_WARNINGS")
    add_includedirs("src/Core")
    add_files("src/**.c", "src/**.cpp")
