set_project("webrtc-rdp")

add_rules("mode.debug", "mode.release")
-- set_toolchains("clang")
set_defaultmode("debug")

local webrtc_dir = "third_party/webrtc"
local vcpkg_dir = "vcpkg_installed"
local webrtc_branch = "m132 refs/remotes/branch-heads/6834"
local webrtc_src_dir = path.join(webrtc_dir, "src")
local webrtc_out_dir = path.join("out", "$(os)" .. "-" .. "$(mode)")
local webrtc_obj_dir = path.join(webrtc_src_dir, webrtc_out_dir, "obj")
local slint_version = "1.8.0"
local slint_dir = "./third_party/Slint-cpp-" .. slint_version .. "-Darwin-arm64"
local slint_compiler = is_os("windows") and "slint_compiler.exe" or "third_party/slint-compiler"

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

-- require_vcpkg("boost-asio", ">=1.86.0")
-- require_vcpkg("boost-beast", ">=1.86.0")
-- require_vcpkg("boost-url", ">=1.86.0")
-- require_vcpkg("boost-thread", ">=1.86.0")
-- require_vcpkg("fmt", ">=11.0.0")
-- require_vcpkg("spdlog", ">=1.15.0")
-- require_vcpkg("abseil", ">=20240722.0")
-- require_vcpkg("nlohmann-json", ">=3.11.2")
-- require_vcpkg("libyuv", ">=1896")
-- -- glew
-- -- require_vcpkg("glew")
-- -- end glew
-- require_vcpkg("sdl2", ">=2.30.0")
-- require_vcpkg("sdl2-ttf", ">=2.22.0")
-- -- require_vcpkg("freetype")
-- require_vcpkg("bzip2", ">=1.0.8")
-- require_vcpkg("brotli", ">=1.0.9")
-- require_vcpkg("zlib", ">=1.2.12")
-- require_vcpkg("libpng", ">=1.6.38")
-- -- end sdl2-ttf
-- -- ffmpeg
-- -- require_vcpkg("ffmpeg[avcodec]", { alias = "avcodec" })
-- -- require_vcpkg("ffmpeg[avutil]", { alias = "avutil" })
-- -- require_vcpkg("ffmpeg[avformat]", { alias = "avformat" })
-- -- require_vcpkg("liblzma")
-- add_requires("vcpkg::ffmpeg >=5.1.2", { configs = { features = { "avcodec", "avutil", "avformat", "avdevice" } } })
-- end ffmpeg

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

local function add_vcpkg(...)
    local args = { ... }
    for _, v in ipairs(args) do
        add_packages(v)
    end
end

target("dezk", function()
    set_default(true)
    set_kind("binary")
    set_languages("c17", "cxx20")
    add_cxxflags("-Wno-deprecated-declarations")
    add_defines("SLINT_FEATURE_EXPERIMENTAL")

    add_includedirs("src", vcpkg_dir .. "/arm64-osx/include")
    add_includedirs(webrtc_src_dir, slint_dir .. "/include/slint")
    add_linkdirs(vcpkg_dir .. "/arm64-osx/lib")
    add_linkdirs(webrtc_obj_dir, slint_dir .. "/lib")
    add_files("src/**.cc")
    remove_files("src/server/**.cc")
    remove_files("src/**_test.cc")


    if is_os("linux") then
        linux_options()
        add_packages("xdo")
    end
    if is_os("windows") then
        windows_options()
        add_vcpkg("x264", "opengl")
    end
    if is_os("macosx") then
        dawrin_options()
    end
    -- add_vcpkg("boost-url", "boost-asio", "boost-beast", "spdlog", "abseil", "nlohmann-json")
    -- add_vcpkg("sdl2", "sdl2-ttf", "glew")
    -- add_vcpkg("avcodec", "avutil", "avformat", "libyuv")
    -- add_vcpkg("freetype", "zlib", "liblzma", "brotli", "libpng", "bzip2")
    --
    add_links("webrtc", "slint_cpp")
    add_links("avcodec", "avutil", "avformat", "avdevice", "avfilter", "swresample", "swscale")
    add_links("fmt", "bz2", "png", "spdlog", "yuv", "z", "brotlicommon", "brotlidec", "brotlienc", "lzma")
    add_links(
        "boost_atomic",
        "boost_container",
        "boost_chrono",
        "boost_context",
        "boost_coroutine",
        "boost_thread",
        "boost_url"
    )
    add_links("SDL2", "SDL2_ttf", "SDL2main", "freetype")
    add_links(
        "absl_bad_any_cast_impl",
        "absl_bad_optional_access",
        "absl_bad_variant_access",
        "absl_base",
        "absl_city",
        "absl_civil_time",
        "absl_cord",
        "absl_cord_internal",
        "absl_cordz_functions",
        "absl_cordz_handle",
        "absl_cordz_info",
        "absl_cordz_sample_token",
        "absl_crc32c",
        "absl_crc_cord_state",
        "absl_crc_cpu_detect",
        "absl_crc_internal",
        "absl_debugging_internal",
        "absl_decode_rust_punycode",
        "absl_demangle_internal",
        "absl_demangle_rust",
        "absl_die_if_null",
        "absl_examine_stack",
        "absl_exponential_biased",
        "absl_failure_signal_handler",
        "absl_flags_commandlineflag",
        "absl_flags_commandlineflag_internal",
        "absl_flags_config",
        "absl_flags_internal",
        "absl_flags_marshalling",
        "absl_flags_parse",
        "absl_flags_private_handle_accessor",
        "absl_flags_program_name",
        "absl_flags_reflection",
        "absl_flags_usage",
        "absl_flags_usage_internal",
        "absl_graphcycles_internal",
        "absl_hash",
        "absl_hashtablez_sampler",
        "absl_int128",
        "absl_kernel_timeout_internal",
        "absl_leak_check",
        "absl_log_entry",
        "absl_log_flags",
        "absl_log_globals",
        "absl_log_initialize",
        "absl_log_internal_check_op",
        "absl_log_internal_conditions",
        "absl_log_internal_fnmatch",
        "absl_log_internal_format",
        "absl_log_internal_globals",
        "absl_log_internal_log_sink_set",
        "absl_log_internal_message",
        "absl_log_internal_nullguard",
        "absl_log_internal_proto",
        "absl_log_severity",
        "absl_log_sink",
        "absl_low_level_hash",
        "absl_malloc_internal",
        "absl_periodic_sampler",
        "absl_poison",
        "absl_random_distributions",
        "absl_random_internal_distribution_test_util",
        "absl_random_internal_platform",
        "absl_random_internal_pool_urbg",
        "absl_random_internal_randen",
        "absl_random_internal_randen_hwaes",
        "absl_random_internal_randen_hwaes_impl",
        "absl_random_internal_randen_slow",
        "absl_random_internal_seed_material",
        "absl_random_seed_gen_exception",
        "absl_random_seed_sequences",
        "absl_raw_hash_set",
        "absl_raw_logging_internal",
        "absl_scoped_set_env",
        "absl_spinlock_wait",
        "absl_stacktrace",
        "absl_status",
        "absl_statusor",
        "absl_str_format_internal",
        "absl_strerror",
        "absl_string_view",
        "absl_strings",
        "absl_strings_internal",
        "absl_symbolize",
        "absl_synchronization",
        "absl_throw_delegate",
        "absl_time",
        "absl_time_zone",
        "absl_utf8_for_code_point",
        "absl_vlog_config_internal"
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
    add_includedirs("src", vcpkg_dir .. "/arm64-osx/include")
    add_linkdirs(vcpkg_dir .. "/arm64-osx/lib")
    add_defines("BOOST_ASIO_HAS_STD_COROUTINE", "BOOST_ASIO_HAS_CO_AWAIT", "BOOST_URL_NO_LIB", "FMT_HEADER_ONLY")
    add_links("fmt", "boost_url", "spdlog")
    -- add_packages("spdlog", "fmt")
    -- add_packages("boost-asio", "boost-url", "boost-beast", "nlohmann-json")
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
end)

if get_config("buildtest") == 1 then
    target("video_player_test", function()
        set_kind("binary")
        set_languages("c17", "cxx20")
        add_files("src/sink/*.cc")
        add_includedirs("src", vcpkg_dir .. "/arm64-osx/include")
        add_linkdirs(vcpkg_dir .. "/arm64-osx/lib")
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
            add_vcpkg("sdl2", "spdlog")
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
    })
end)

task("fetch-slint", function()
    on_run(function()
        os.cd("third_party")
        if os.host() == "linux" then
            os.exec(
                "wget -c https://github.com/slint-ui/slint/releases/download/v"
                .. slint_version
                .. "/Slint-cpp-"
                .. slint_version
                .. "-Linux-x86_64.tar.gz -O - | tar -zx"
            )
        end
        if os.host() == "macosx" then
            os.exec(
                "wget -c https://github.com/slint-ui/slint/releases/download/v"
                .. slint_version
                .. "/Slint-cpp-"
                .. slint_version
                .. "-Darwin-arm64.tar.gz -O - | tar -zx"
            )
        end
    end)

    set_menu({
        usage = "xmake fetch-slint",
        description = "fetch slint binary release",
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
    })
end)
