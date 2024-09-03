workspace "main"
    configurations { "Debug", "Release" }
    architecture "x86_64"
    location "."

project "main"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}"

    files { "src/**.cpp", "src/**.h" }
    includedirs { "include", "/usr/include" }
    libdirs { "/usr/lib" }

    -- Link libraries
    links {
        "glfw",
        "GLEW",
        "GL"
    }

    -- Compiler and linker settings
    filter "system:linux"
        buildoptions { "-std=c++17" }
        links { "GL", "glfw", "GLEW" }
    
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

        postbuildcommands {
            "%{cfg.targetdir}/main 2> /dev/null"
        }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

