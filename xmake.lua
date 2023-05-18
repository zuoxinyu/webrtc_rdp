set_project('webrtc-rdp')

add_rules('mode.debug', 'mode.release')
set_toolchains('clang')
set_defaultmode("debug")

local webrtc_dir = '../webrtc'
local webrtc_branch = 'm113 refs/remotes/branch-heads/5672'
local webrtc_src_dir = path.join(webrtc_dir, 'src')
local webrtc_out_dir = path.join('out', '$(os)' .. '-' .. '$(mode)')
local webrtc_obj_dir = path.join(webrtc_src_dir, webrtc_out_dir, 'obj')
local slint_dir = './third_party/Slint-cpp-1.0.2-Linux-x86_64'

add_requireconfs("*", { configs = { shared = false, system = true, debug = true }, shared = false })

local function require_vcpkg(pkg, opts)
    local o = { alias = pkg }
    for k, v in pairs(opts) do
        o[k] = v
    end
    add_requires('vcpkg::' .. pkg, o)
end

require_vcpkg('boost-asio')
require_vcpkg('boost-beast')
require_vcpkg('boost-url')
require_vcpkg('fmt')
require_vcpkg('spdlog')
require_vcpkg('abseil')
require_vcpkg('nlohmann-json')
require_vcpkg('libyuv')
-- glew
require_vcpkg('glew')
-- end glew
require_vcpkg('sdl2')
-- sdl2-ttf
require_vcpkg('sdl2-ttf')
require_vcpkg('freetype')
require_vcpkg('bzip2')
require_vcpkg('brotli')
require_vcpkg('zlib')
require_vcpkg('libpng')
-- end sdl2-ttf
-- ffmpeg
require_vcpkg('ffmpeg[avcodec]', { alias = 'avcodec' })
require_vcpkg('ffmpeg[avutil]', { alias = 'avutil' })
require_vcpkg('ffmpeg[avformat]', { alias = 'avformat' })
require_vcpkg('liblzma')
-- end ffmpeg

if is_os('linux') then
    add_requires('system::xdo', { alias = 'xdo' })
end

if is_os('windows') then
    require_vcpkg('opengl')
    require_vcpkg('x264')
end

local function windows_options()
    -- clang windows workarounds
    add_cxxflags('-fms-runtime-lib=static_dbg', '-fms-extensions')
    add_defines('WEBRTC_WIN', 'NOMINMAX', '_WIN32_WINNT=0x0601', '_CRT_SECURE_NO_WARNINGS')
    add_defines(
        'SDL_MAIN_HANDLED',
        'BOOST_ASIO_HAS_STD_COROUTINE',
        'BOOST_ASIO_HAS_CO_AWAIT',
        'BOOST_URL_NO_LIB',
        'FMT_HEADER_ONLY')
    add_syslinks('kernel32', 'user32', 'gdi32', 'winspool', 'comdlg32', 'advapi32', 'shell32',
        'ole32', 'oleaut32', 'uuid', 'secur32', 'strmiids', 'mfuuid', 'd3d11', 'wmcodecdspuuid', 'dmoguids',
        'odbc32', 'odbccp32', 'version', 'winmm', 'setupapi', 'imm32', 'iphlpapi', 'msdmo', 'shcore', 'dwmapi', 'dxgi')
end

local function linux_options()
    add_defines('WEBRTC_POSIX', 'WEBRTC_LINUX', 'WEBRTC_USE_X11')
    add_links('glib-2.0', 'gobject-2.0', 'gio-2.0', 'gbm')
    add_links('X11', 'Xext', 'Xfixes', 'Xdamage', 'Xrandr', 'Xcomposite', 'Xtst')
    add_links('rt', 'drm')
end

local function add_vcpkg(...)
    local args = { ... }
    for _, v in ipairs(args) do
        add_packages(v)
    end
end

target('dezk', function()
    set_default(true)
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_cxxflags('-Wno-deprecated-declarations')
    add_defines('SLINT_FEATURE_EXPERIMENTAL')

    add_includedirs('src', webrtc_src_dir, slint_dir .. '/include')
    add_files('src/**.cc', 'src/**.c')
    remove_files('src/server/**.cc')
    remove_files('src/**_test.cc')

    add_linkdirs(webrtc_obj_dir, slint_dir .. '/lib')
    add_links('webrtc', 'slint_cpp')

    if is_os('linux') then
        linux_options()
        add_packages('xdo')
    end
    if is_os('windows') then
        windows_options()
        add_vcpkg('x264', 'opengl')
    end
    add_vcpkg('boost-url', 'boost-asio', 'boost-beast', 'spdlog', 'abseil', 'nlohmann-json')
    add_vcpkg('sdl2', 'sdl2-ttf', 'glew')
    add_vcpkg('avcodec', 'avutil', 'avformat', 'libyuv')
    add_vcpkg('freetype', 'zlib', 'liblzma', 'brotli', 'libpng', 'bzip2')

    if is_os('linux') then
        after_build(function(target)
            os.exec('rsync %s notebook:dezk', target:targetfile())
        end)
    end
end)

target('signal_server', function()
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_files('src/server/*.cc')
    add_includedirs('src')

    add_packages('spdlog', 'fmt')
    if is_os('windows') then
        windows_options()
    end
    if is_os('linux') then
        add_cxxflags('-static-libstdc++', '-static-libgcc')
        -- add_ldflags('-Wl,--rpath=./lib')
        -- add_ldflags('-Wl,--dynamic-linker=./lib/ld-linux.so.2')
    end
    add_packages('boost-asio', 'boost-url', 'boost-beast', 'nlohmann-json')

    if is_os('linux') then
        after_build(function(target)
            os.exec('rsync %s notebook:signal_server', target:targetfile())
        end)
    end
end)

if get_config('buildtest') == 1 then
    target('video_player_test', function()
        set_kind('binary')
        set_languages('c17', 'cxx20')
        add_files('src/sink/*.cc')
        add_includedirs('src', webrtc_src_dir)
        add_packages('spdlog', 'fmt', 'sdl2', 'sdl2-ttf', 'glew')
        add_linkdirs(webrtc_obj_dir)
        add_links('webrtc')
        if is_os('linux') then
            linux_options()
        end
        if is_os('windows') then
            windows_options()
            add_packages('opengl')
        end
    end)

    if is_os('windows') then
        target('executor_test', function()
            set_kind('binary')
            set_languages('c17', 'cxx20')
            add_includedirs('src')
            add_files('src/executor/*.cc')
            add_vcpkg('sdl2', 'spdlog')
            if is_os('linux') then
                add_packages('xdo')
            end
            if is_os('windows') then
                windows_options()
            end
        end)
    end
end

-- TODO: use package() instead
local gn_args = {
    'is_debug=true',
    'enable_libaom=true', -- av1 support
    'rtc_use_h264=false', --  use custom h264 impl
    'rtc_include_tests=false',
    'rtc_enable_protobuf=false',
    -- 'ffmpeg_branding=\"Chrome\"',
    'use_rtti=true',           -- typeinfo
    'use_custom_libcxx=false', -- stdlib
    'use_debug_fission=true',  -- -gsplit-dwarf
    -- 'rtc_enable_symbol_export=true',
}

if is_os('windows') then
    gn_args = {
        'is_debug=true',
        'enable_libaom=true', -- av1 support
        'rtc_use_h264=false', -- use custom h264 impl
        -- 'ffmpeg_branding=\"Chrome\"',
        'rtc_include_tests=false',
        'rtc_enable_protobuf=false',
        -- for windows
        'use_lld=false',           -- linker
        'use_rtti=true',           -- typeinfo
        'use_custom_libcxx=false', -- stdlib
        'fatal_linker_warnings=false',
        'treat_warnings_as_errors=false',
        'enable_iterator_debugging=true',
        -- no rtc_enable_symbol_export, which leads build errors
    }
end

local check_cmd = string.format([[git checkout -b %s]], webrtc_branch)
local gn_cmd = string.format([[gn gen %s --args="%s"]], webrtc_out_dir, table.concat(gn_args, ' '))
local ninja_cmd = string.format([[ninja -C %s]], webrtc_out_dir)
local mt_cmd = [[mt.exe -manifest .\dezk.manifest -outputresource:'.\build\windows\x64\debug\dezk.exe;#1']]
task('fetch-webrtc', function()
    on_run(function()
        os.cd(webrtc_dir)
        os.exec('fetch webrtc') -- TODO: need install depot_tools
        os.cd(webrtc_src_dir)
        os.exec(check_cmd)
    end)
    set_menu {
        usage = 'xmake fetch-webrtc',
        description = 'fetch webrtc source from google',
    }
end)

task('build-webrtc', function()
    on_run(function()
        os.cd(webrtc_src_dir)
        os.exec(gn_cmd)
        os.exec(ninja_cmd)
    end)

    set_menu {
        usage = 'xmake build-webrtc',
        description = 'build webrtc source',
    }
end)

task('fetch-slint', function()
    on_run(function()
        os.cd('third_party')
        os.exec(
            'wget -c https://github.com/slint-ui/slint/releases/download/v1.0.2/Slint-cpp-1.0.2-Linux-x86_64.tar.gz -O - | tar -zx')
    end)

    set_menu {
        usage = 'xmake fetch-slint',
        description = 'fetch slint binary release',
    }
end)

task('echo-cmd', function()
    on_run(function()
        cprint('${yellow}webrtc dir${clear}: %s', path.absolute(webrtc_src_dir))
        cprint('${yellow}webrtc branch${clear}: %s', check_cmd)
        cprint('${yellow}webrtc config cmd${clear}: %s', gn_cmd)
        cprint('${yellow}webrtc build cmd${clear}: %s', ninja_cmd)
        cprint('${yellow}mt cmd${clear}: %s', mt_cmd)
    end)
    set_menu {
        usage = 'xmake echo-cmd',
        description = 'show webrtc build commands',
    }
end)

-- package("slint", function()
--     add_deps("cmake")
--     set_sourcedir(path.join(os.scriptdir(), "third_party", 'Slint-cpp-1.0.2-Linux-x86_64'))
--     on_install(function(package)
--         local configs = {}
--         table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
--         table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
--         import("package.tools.cmake").install(package, configs)
--     end)
-- end)
