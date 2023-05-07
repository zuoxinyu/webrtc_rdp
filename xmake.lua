set_project('webrtc-rdp')
add_rules('mode.debug', 'mode.release')
set_toolchains('clang', 'yasm')
set_defaultmode("debug")

local webrtc_dir = '../webrtc'
local webrtc_src_dir = path.join(webrtc_dir, 'src')
local webrtc_out_dir = 'out/linux-debug'
if is_os('linux') then
    if is_mode('rlease') then
        webrtc_out_dir = 'out/linux-release'
    elseif is_mode('debug') then
        webrtc_out_dir = 'out/linux-debug'
    end
elseif is_os('windows') then
    if is_mode('rlease') then
        webrtc_out_dir = 'out/windows-release'
    elseif is_mode('debug') then
        webrtc_out_dir = 'out/windows-debug'
    end
end
local webrtc_obj_dir = path.join(webrtc_src_dir, webrtc_out_dir, 'obj')

local gn_args = {
    'is_debug=true',
    'enable_libaom=true',     -- av1
    'use_debug_fission=true', -- -gsplit-dwarf
    'use_custom_libcxx=false',
    'use_rtti=true',          -- typeinfo
    'rtc_use_h264=false',
    -- 'ffmpeg_branding=\"Chrome\"',
    'rtc_include_tests=false',
    'rtc_enable_protobuf=false',
    'rtc_enable_symbol_export=true',
}
local check_cmd = [[git checkout -b m113 refs/remotes/branch-heads/5672]]
local gn_cmd = string.format([[gn gen %s --args="%s"]], webrtc_out_dir, table.concat(gn_args, ' '))
local ninja_cmd = string.format([[ninja -C %s]], webrtc_out_dir)

add_requires('SDL2', 'spdlog')
add_requires('boost', {
    configs = {
        json = true,
        presets = { Boost_USE_STATIC_LIB = true }
    }
})
add_requires('boost_json', 'boost_url')

target('dezk', function()
    set_default(true)
    set_kind('binary')
    set_languages('c17', 'cxx20')

    add_includedirs('src', webrtc_src_dir)
    add_files('src/client/**.cc', 'src/client/**.c')
    remove_files("**_test.cc")

    add_packages('boost_json', 'boost_url', 'boost_beast', 'spdlog', 'SDL2')
    add_syslinks('avcodec', 'avutil', 'avformat')
    add_linkdirs(webrtc_obj_dir)
    add_links('webrtc')

    if is_os('linux') then
        add_defines('WEBRTC_POSIX', 'WEBRTC_LINUX', 'WEBRTC_USE_X11', 'RTC_DISABLE_LOGGING')
        add_cxxflags('-Wno-deprecated-declarations')
        add_syslinks('dbus-1', 'glib-2.0', 'gobject-2.0', 'gmodule-2.0', 'gio-2.0', 'gbm')
        add_syslinks('xdo', 'xcb', 'X11', 'Xext', 'Xfixes', 'Xdamage', 'Xrandr', 'Xrender', 'Xau', 'Xdmcp', 'Xcomposite',
            'Xtst')
        add_syslinks('rt', 'atomic', 'GL', 'GLEW', 'drm', 'SDL2_ttf')
    elseif is_os('windows') then
        add_defines('WEBRTC_WIN', 'NOMINMAX', '_WIN32_WINNT=0x0601')
    end

    after_build(function(target)
        os.exec('rsync %s notebook:dezk', target:targetfile())
    end)

    -- add_deps('webrtc')
end)

target('signal_server', function()
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_files('src/server/*.cc')
    add_includedirs('src')

    add_packages('boost_json', 'boost_url', 'boost_beast', 'spdlog', { configs = { shared = false } })

    after_build(function(target)
        os.exec('rsync %s notebook:signal_server', target:targetfile())
    end)
end)

target('video_player_test', function()
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_files('src/client/sink/*.cc')
    add_includedirs('src', webrtc_src_dir)
    add_packages('spdlog', 'SDL2')
    add_syslinks('GL', 'GLEW', 'SDL2_ttf')
    add_linkdirs(webrtc_obj_dir)
    add_links('webrtc')
end)

-- target('webrtc', function()
--     set_kind('static')
--     set_languages('c17', 'cxx17')
--     add_rules('c++')
--     set_targetdir('libwebrtc/debug')
--     add_files(webrtc_obj_dir .. '/**.o')
--
--     remove_files(webrtc_obj_dir .. '/third_party/yasm/gen*/**.o')
--     remove_files(webrtc_obj_dir .. '/third_party/yasm/re2c/**.o')
--     remove_files(webrtc_obj_dir .. '/third_party/yasm/yasm/**.o')
--     remove_files(webrtc_obj_dir .. '/third_party/protobuf/protoc/*.o')
--     remove_files(webrtc_obj_dir .. '/third_party/protobuf/protobuf_full/*.o')
--     remove_files(webrtc_obj_dir .. '/webrtc/examples/**.o')
--     remove_files(webrtc_obj_dir .. '/webrtc/modules/audio_coding/delay_test/utility.o')
--     remove_files(webrtc_obj_dir .. '/webrtc/modules/modules_tests/utility.o')
--     remove_files(webrtc_obj_dir .. '/webrtc/modules/video_capture/video_capture/video_capture_external.o')
--     remove_files(webrtc_obj_dir .. '/webrtc/modules/video_capture/video_capture/device_info_external.o')
-- end)


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
        cprint('${yellow}webrtc config cmd${clear}: %s', gn_cmd)
        cprint('${yellow}webrtc build cmd${clear}: %s', ninja_cmd)
    end)
    set_menu {
        usage = 'xmake echo-cmd',
        description = 'show webrtc build commands',
    }
end)
