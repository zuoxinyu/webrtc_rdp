set_project('webrtc-rdp')
add_rules('mode.debug', 'mode.release')
set_toolchains('clang')
set_defaultmode("debug")

local webrtc_dir = '../webrtc'
local webrtc_branch = 'm113 refs/remotes/branch-heads/5672'
local webrtc_src_dir = path.join(webrtc_dir, 'src')
local webrtc_third_party_dirs = {
    -- path.join(webrtc_src_dir, 'third_party', 'abseil-cpp'),
    -- path.join(webrtc_src_dir, 'third_party', 'libyuv'),
}
local webrtc_out_dir = 'out/linux-debug'
if is_os('linux') then
    if is_mode('release') then
        webrtc_out_dir = 'out/linux-release'
    elseif is_mode('debug') then
        webrtc_out_dir = 'out/linux-debug'
    end
elseif is_os('windows') then
    if is_mode('release') then
        webrtc_out_dir = 'out/windows-release'
    elseif is_mode('debug') then
        webrtc_out_dir = 'out/windows-debug'
    end
end
local webrtc_obj_dir = path.join(webrtc_src_dir, webrtc_out_dir, 'obj')

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

local function windows_options()
    -- clang windows workarounds
    add_cxxflags('-fms-runtime-lib=static_dbg', '-fms-extensions')
    add_defines(
        'SDL_MAIN_HANDLED',
        'BOOST_ASIO_HAS_STD_COROUTINE',
        'BOOST_ASIO_HAS_CO_AWAIT',
        'BOOST_JSON_NO_LIB',
        'BOOST_URL_NO_LIB',
        'FMT_HEADER_ONLY')
    add_syslinks('kernel32', 'user32', 'gdi32', 'winspool', 'comdlg32', 'advapi32', 'shell32',
        'ole32', 'oleaut32', 'uuid', 'secur32', 'strmiids', 'mfuuid', 'd3d11', 'wmcodecdspuuid', 'dmoguids',
        'odbc32', 'odbccp32', 'version', 'winmm', 'setupapi', 'imm32', 'iphlpapi', 'msdmo', 'shcore', 'dwmapi', 'dxgi')
end

local function linux_options()
    add_links('glib-2.0', 'gobject-2.0', 'gio-2.0', 'gbm')
    add_links('X11', 'Xext', 'Xfixes', 'Xdamage', 'Xrandr', 'Xcomposite', 'Xtst')
    add_links('rt', 'drm')
end

if is_os('linux') then
    add_requires('xdo', { system = true })
end

add_requires('boost-asio', { system = true, debug = true })
add_requires('boost-beast', { system = true, debug = true })
add_requires('boost-json', { system = true, debug = true })
add_requires('boost-url', { system = true, debug = true })
add_requires('abseil', { system = true })
-- glew
add_requires('glew', { system = true })
add_requires('opengl', { system = true })
-- end glew
add_requires('sdl2', { system = true })
-- sdl2-ttf
add_requires('sdl2-ttf', { system = true })
add_requires('freetype', { system = true })
add_requires('bzip2', { system = true })
add_requires('brotli', { system = true })
add_requires('zlib', { system = true })
add_requires('libpng', { system = true })
-- end sdl2-ttf
add_requires('spdlog', { system = true, })
add_requires('fmt', { system = true, debug = true })
add_requires('nlohmann-json', { system = true })
add_requires('libyuv', { system = true })
-- ffmpeg
add_requires('ffmpeg[avcodec]', { alias = 'avcodec', system = true })
add_requires('ffmpeg[avutil]', { alias = 'avutil', system = true })
add_requires('ffmpeg[avformat]', { alias = 'avformat', system = true })
add_requires('x264', { system = true })
add_requires('liblzma', { system = true })
-- end ffmpeg

target('dezk', function()
    set_default(true)
    set_kind('binary')
    set_languages('c17', 'cxx20')

    add_includedirs('src', webrtc_src_dir)
    add_files('src/client/**.cc', 'src/client/**.c')
    remove_files("**_test.cc")

    add_linkdirs(webrtc_obj_dir)
    add_links('webrtc')

    add_cxxflags('-Wno-deprecated-declarations')

    if is_os('linux') then
        add_defines('WEBRTC_POSIX', 'WEBRTC_LINUX', 'WEBRTC_USE_X11')
        add_packages('xdo')
        linux_options()
    elseif is_os('windows') then
        add_defines('WEBRTC_WIN', 'NOMINMAX', '_WIN32_WINNT=0x0601', '_CRT_SECURE_NO_WARNINGS', 'SDL_MAIN_HANDLED')
        windows_options()
    end
    add_packages('boost-json', 'boost-url', 'spdlog', 'abseil', 'nlohmann-json')
    add_packages('sdl2', 'sdl2-ttf', 'glew')
    add_packages('avcodec', 'avutil', 'avformat', 'libyuv')
    add_packages('opengl', 'freetype', 'zlib', 'liblzma', 'brotli', 'libpng', 'bzip2', 'x264')

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
        add_defines('NOMINMAX', '_WIN32_WINNT=0x0601', '_CRT_SECURE_NO_WARNINGS')
    end
    add_packages('boost-asio', 'boost-json', 'boost-url', 'boost-beast')

    if is_os('linux') then
        after_build(function(target)
            os.exec('rsync %s notebook:signal_server', target:targetfile())
        end)
    end
end)

target('video_player_test', function()
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_files('src/client/sink/*.cc')
    add_includedirs('src', webrtc_src_dir)
    add_includedirs(webrtc_third_party_dirs)
    add_packages('spdlog', 'fmt', 'sdl2', 'sdl2-ttf', 'glew', 'opengl')
    add_linkdirs(webrtc_obj_dir)
    add_links('webrtc')
    if is_os('windows') then
        windows_options()
    end
end)

task('fetch-webrtc', function()
    on_run(function()
        os.cd(webrtc_dir)
        os.exec('fetch webrtc')
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
