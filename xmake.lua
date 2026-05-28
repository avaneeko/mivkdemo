add_rules("mode.debug", "mode.release")

add_requires("vulkansdk")
add_requires("fastgltf")
add_requires("cglm")

target("camellia")
    set_kind("binary")
    set_languages("c17", "c++20")

    add_packages("vulkansdk")
    add_packages("fastgltf")
    add_packages("cglm")
    add_syslinks("user32", "gdi32")

	if is_mode("debug") then
		set_symbols("debug")
		set_optimize("none")
	elseif is_mode("release") then
		set_symbols("hidden")
		set_strip("all")
		set_optimize("fastest")
	end

    set_exceptions("no-cxx")
    -- Required on MSVC-like, as it is not set by set_exceptions("no-cxx") 
    add_defines("_HAS_EXCEPTIONS=0", {tools = {"cl", "clang_cl"}})
    add_defines("NOMINMAX=1", {tools = {"cl", "clang_cl"}})
    add_defines("_CRT_SECURE_NO_WARNINGS")
    add_defines("CGLM_FORCE_LEFT_HANDED")
    add_includedirs("src/Core")
    add_files("src/**.c", "src/**.cpp")
