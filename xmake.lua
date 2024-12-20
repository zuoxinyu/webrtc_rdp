set_project("webrtc-rdp")

add_rules("mode.debug", "mode.release")
set_toolchains("clang")
set_defaultmode("debug")
add_rules("plugin.compile_commands.autoupdate")

local vcpkg_dir = path.join(
    "vcpkg_installed",
    is_os("windows") and "x64-windows-static" or is_os("macosx") and "arm64-osx" or is_os("linux") and "x64-linux"
)

local webrtc_dir = "third_party/webrtc"
local webrtc_branch = "m132 refs/remotes/branch-heads/6834"
local webrtc_src_dir = path.join(webrtc_dir, "src")
local webrtc_out_dir = path.join("out", string.format("$(os)-%s", is_mode("release") and "release" or "debug"))
local webrtc_obj_dir = path.join(webrtc_src_dir, webrtc_out_dir, "obj")

local slint_version = "1.9.0"
local slint_os_infix = is_os("linux") and "Linux-x86_64" or is_os("macosx") and "Darwin-arm64" or "unknown"
local slint_dir = is_os("windows") and string.format("C:/Program Files/Slint-cpp %s/", slint_version)
    or string.format("./third_party/Slint-cpp-%s-%s", slint_version, slint_os_infix)

local slint_compiler = is_os("windows") and "third_party/slint-compiler.exe" or "third_party/slint-compiler"

-- add_requireconfs("*", { configs = { shared = false, system = true, debug = true }, shared = false })

local function require_vcpkg(pkg, version, opts)
    if not opts then
        opts = {}
    end
    if not version then
        version = "*"
    end
    local o = { alias = pkg, table.unpack(opts) }
    add_requires("vcpkg::" .. pkg .. " " .. version, o)
end

add_requires("boost", { configs = { header_only = true, all = true } })
add_packages("fmt")
add_requires("abseil")
add_requires("spdlog")
add_requires("nlohmann_json")
add_requires("libyuv")
add_requires("glew")
add_requires("libsdl")
add_requires("libsdl_ttf")
add_requires("libpng")
add_requires("ffmpeg")

if is_os("linux") then
    add_requires("system::xdo", { alias = "xdo" })
end

if is_os("windows") then
    require_vcpkg("opengl")
    require_vcpkg("x264")
end

local function windows_options()
    -- clang windows workarounds
    add_cxxflags("-fms-runtime-lib=static_dbg", "-fms-extensions")
    add_defines("WEBRTC_WIN", "NOMINMAX", "_WIN32_WINNT=0x0601", "_CRT_SECURE_NO_WARNINGS")
    add_defines(
        "SDL_MAIN_HANDLED",
        "BOOST_ASIO_HAS_STD_COROUTINE",
        "BOOST_ASIO_HAS_CO_AWAIT",
        "BOOST_URL_NO_LIB",
        "FMT_HEADER_ONLY"
    )
    add_syslinks(
        "kernel32",
        "user32",
        "gdi32",
        "winspool",
        "comdlg32",
        "advapi32",
        "shell32",
        "ole32",
        "oleaut32",
        "uuid",
        "secur32",
        "strmiids",
        "mfuuid",
        "d3d11",
        "wmcodecdspuuid",
        "dmoguids",
        "odbc32",
        "odbccp32",
        "version",
        "winmm",
        "setupapi",
        "imm32",
        "iphlpapi",
        "msdmo",
        "shcore",
        "dwmapi",
        "dxgi"
    )
end

local function linux_options()
    add_defines("WEBRTC_POSIX", "WEBRTC_LINUX", "WEBRTC_USE_X11")
    add_links("glib-2.0", "gobject-2.0", "gio-2.0", "gbm")
    add_links("X11", "Xext", "Xfixes", "Xdamage", "Xrandr", "Xcomposite", "Xtst")
    add_links("rt", "drm")
end

local function dawrin_options()
    add_defines("WEBRTC_POSIX", "WEBRTC_MAC")
    add_frameworks(
        "CoreFoundation",
        "CoreGraphics",
        "CoreMedia",
        "CoreAudio",
        "AudioToolbox",
        "VideoToolbox",
        "IOSurface",
        "ScreenCaptureKit",
        "CoreServices",
        "CoreVideo",
        "CoreHaptics",
        "Foundation",
        "AppKit",
        "IOKit",
        "Security",
        "SystemConfiguration",
        "InputMethodKit",
        "ForceFeedback",
        "GameController",
        "Metal",
        "Carbon"
    )
end

target("dezk", function()
    set_default(true)
    set_kind("binary")
    set_languages("c17", "cxx20")
    add_cxxflags("-Wno-deprecated-declarations")
    add_defines("SLINT_FEATURE_EXPERIMENTAL")

    add_includedirs("src")
    add_includedirs(webrtc_src_dir, slint_dir .. "/include/slint")
    -- add_includedirs(vcpkg_dir .. "/include")
    -- add_linkdirs(vcpkg_dir .. "/lib")
    add_linkdirs(webrtc_obj_dir, slint_dir .. "/lib")
    add_files("src/**.cc")
    remove_files("src/server/**.cc")
    remove_files("src/**_test.cc")

    add_rpathdirs(slint_dir .. "/lib")
    if is_os("linux") then
        linux_options()
        add_packages("xdo")
    end
    if is_os("windows") then
        windows_options()
        add_packages("x264", "opengl")
    end
    if is_os("macosx") then
        dawrin_options()
    end
    add_links("webrtc", "slint_cpp")
    add_packages(
        "fmt",
        "spdlog",
        "boost",
        "abseil",
        "nlohmann_json",
        "ffmpeg",
        "libyuv",
        "libsdl",
        "libsdl_ttf",
        "libpng"
    )
    before_build(function()
        os.exec("%s src/ui/app.slint -o src/ui/app.slint.h", slint_compiler)
    end)
    if is_os("linux") then
        after_build(function(target)
            os.exec("rsync %s notebook:dezk", target:targetfile())
        end)
    end
end)

target("signal_server", function()
    set_kind("binary")
    set_languages("c17", "cxx20")
    add_files("src/server/*.cc")
    -- add_includedirs(vcpkg_dir .. "/include")
    add_includedirs("src")
    if is_os("windows") then
        windows_options()
    end
    if is_os("linux") then
        add_cxxflags("-static-libstdc++", "-static-libgcc")
        -- add_ldflags('-Wl,--rpath=./lib')
        -- add_ldflags('-Wl,--dynamic-linker=./lib/ld-linux.so.2')
    end
    if is_os("macosx") then
        dawrin_options()
    end

    if is_os("linux") then
        after_build(function(target)
            os.exec("rsync %s notebook:signal_server", target:targetfile())
        end)
    end
    add_packages("spdlog", "fmt", "boost", "abseil", "nlohmann_json")
end)

if get_config("buildtest") == 1 then
    target("video_player_test", function()
        set_kind("binary")
        set_languages("c17", "cxx20")
        add_files("src/sink/*.cc")
        add_includedirs("src", vcpkg_dir .. "/include")
        add_linkdirs(vcpkg_dir .. "/lib")
        add_includedirs(webrtc_src_dir)
        add_packages("spdlog", "fmt", "sdl2", "sdl2-ttf", "glew")
        add_linkdirs(webrtc_obj_dir)
        add_links("webrtc")
        if is_os("linux") then
            linux_options()
        end
        if is_os("windows") then
            windows_options()
            add_packages("opengl")
        end
    end)

    if is_os("windows") then
        target("executor_test", function()
            set_kind("binary")
            set_languages("c17", "cxx20")
            add_includedirs("src")
            add_files("src/executor/*.cc")
            add_packages("libsdl", "spdlog")
            if is_os("linux") then
                add_packages("xdo")
            end
            if is_os("windows") then
                windows_options()
            end
        end)
    end
end

-- TODO: use package() instead
local gn_args = {
    "is_debug=true",
    "enable_libaom=true", -- av1 support
    "rtc_use_h264=false", --  use custom h264 impl
    "rtc_include_tests=false",
    "rtc_enable_protobuf=false",
    -- 'ffmpeg_branding=\"Chrome\"',
    "use_rtti=true",        -- typeinfo
    "use_custom_libcxx=false", -- stdlib
    "use_debug_fission=true", -- -gsplit-dwarf
    -- 'rtc_enable_symbol_export=true',
}

if is_os("windows") then
    gn_args = {
        "is_debug=true",
        "enable_libaom=true", -- av1 support
        "rtc_use_h264=false", -- use custom h264 impl
        -- 'ffmpeg_branding=\"Chrome\"',
        "rtc_include_tests=false",
        "rtc_enable_protobuf=false",
        -- for windows
        "use_lld=false",     -- linker
        "use_rtti=true",     -- typeinfo
        "use_custom_libcxx=false", -- stdlib
        "fatal_linker_warnings=false",
        "treat_warnings_as_errors=false",
        "enable_iterator_debugging=true",
        -- no rtc_enable_symbol_export, which leads build errors
    }
end

local check_cmd = string.format([[git checkout -b %s]], webrtc_branch)
local gn_cmd = string.format([[gn gen %s --args="%s"]], webrtc_out_dir, table.concat(gn_args, " "))
local ninja_cmd = string.format([[ninja -C %s]], webrtc_out_dir)
local mt_cmd = [[mt.exe -manifest .\dezk.manifest -outputresource:'.\build\windows\x64\debug\dezk.exe;#1']]
task("fetch-webrtc", function()
    on_run(function()
        os.cd(webrtc_dir)
        os.exec("fetch webrtc") -- TODO: need install depot_tools
        os.cd(webrtc_src_dir)
        os.exec(check_cmd)
    end)
    set_menu({
        usage = "xmake fetch-webrtc",
        description = "fetch webrtc source from google",
        options = {},
    })
end)

task("build-webrtc", function()
    on_run(function()
        os.cd(webrtc_src_dir)
        os.exec(gn_cmd)
        os.exec(ninja_cmd)
    end)

    set_menu({
        usage = "xmake build-webrtc",
        description = "build webrtc source",
        options = {},
    })
end)

task("fetch-slint", function()
    on_run(function()
        os.cd("third_party")
        if os.host() == "linux" or os.host() == "macosx" then
            os.exec(
                string.format(
                    "wget -c https://github.com/slint-ui/slint/releases/download/v%s/Slint-cpp-%s-%s.tar.gz",
                    slint_version,
                    slint_version,
                    slint_os_infix
                )
            )
            os.exec(
                string.format(
                    "wget -c https://github.com/slint-ui/slint/releases/download/v%s/slint-compiler-%s.tar.gz",
                    slint_version,
                    slint_os_infix
                )
            )
            os.exec(string.format("tar zx Slint-cpp-%s-%s.tar.gz", slint_version, slint_os_infix))
            os.exec(string.format("tar zx slint-compiler-%s.tar.gz", slint_os_infix))
        elseif os.host() == "windows" then
            os.exec(
                string.format(
                    "wget https://github.com/slint-ui/slint/releases/download/v%s/Slint-cpp-%s-win64-MSVC.exe",
                    slint_version,
                    slint_version
                )
            )
            -- TODO: install exe
            os.exec(
                string.format(
                    "wget -c https://github.com/slint-ui/slint/releases/download/v%s/slint-compiler-%s.tar.gz",
                    slint_version,
                    "Windows"
                )
            )
            os.exec(string.format("tar zx slint-compiler-%s.tar.gz", "Windows"))
        end
    end)

    set_menu({
        usage = "xmake fetch-slint",
        description = "fetch slint binary release",
        options = {},
    })
end)

task("echo-cmd", function()
    on_run(function()
        cprint("${yellow}webrtc dir${clear}: %s", path.absolute(webrtc_src_dir))
        cprint("${yellow}webrtc branch${clear}: %s", check_cmd)
        cprint("${yellow}webrtc config cmd${clear}: %s", gn_cmd)
        cprint("${yellow}webrtc build cmd${clear}: %s", ninja_cmd)
        cprint("${yellow}mt cmd${clear}: %s", mt_cmd)
    end)
    set_menu({
        usage = "xmake echo-cmd",
        description = "show webrtc build commands",
        options = {},
    })
end)
