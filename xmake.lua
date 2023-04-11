set_project('webrtc-rdp')
add_rules('mode.debug', 'mode.release')
set_toolchains('clang', 'yasm')
set_defaultmode("debug")

local webrtc_src_dir = '../libwebrtc_build/src'
local webrtc_out_dir = webrtc_src_dir .. '/out/Linux-x64-debug'

add_requires('SDL2', 'spdlog')
add_requires('boost_json', 'boost_url', {
    configs = {
        presets = { Boost_USE_STATIC_LIB = true }
    }
})

target('dezk', function()
    set_default(true)
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_defines('WEBRTC_POSIX', 'WEBRTC_LINUX', 'WEBRTC_USE_X11')
    add_cxxflags('-Wno-deprecated-declarations')

    add_includedirs('src', webrtc_src_dir)
    add_files('src/client/*.cc', 'src/client/*.c')

    add_packages('boost_json', 'boost_url', 'boost_beast', 'spdlog', 'SDL2')
    add_linkdirs('libwebrtc/debug')
    add_links('webrtc')
    add_syslinks('dbus-1', 'glib-2.0', 'gobject-2.0', 'gmodule-2.0', 'gio-2.0', 'gbm')
    add_syslinks('xcb', 'X11', 'Xext', 'Xfixes', 'Xdamage', 'Xrandr', 'Xrender', 'Xau', 'Xdmcp', 'Xcomposite', 'Xtst')
    add_syslinks('rt', 'atomic', 'GL', 'GLEW')

    add_deps('webrtc')
end)

target('signal_server', function()
    set_kind('binary')
    set_languages('c17', 'cxx20')
    add_files('src/server/*.cc')
    add_includedirs('src')

    add_packages('boost_json', 'boost_url', 'boost_beast', 'spdlog', { configs = { shared = false } })
end)

target('webrtc', function()
    set_kind('static')
    set_languages('c17', 'cxx17')
    add_rules('c++')
    set_targetdir('libwebrtc/debug')
    add_files(webrtc_out_dir .. '/obj/**.o')

    remove_files(webrtc_out_dir .. '/obj/third_party/yasm/gen*/**.o')
    remove_files(webrtc_out_dir .. '/obj/third_party/yasm/re2c/**.o')
    remove_files(webrtc_out_dir .. '/obj/third_party/yasm/yasm/**.o')
    remove_files(webrtc_out_dir .. '/obj/third_party/protobuf/protoc/*.o')
    remove_files(webrtc_out_dir .. '/obj/third_party/protobuf/protobuf_full/*.o')
    remove_files(webrtc_out_dir .. '/obj/webrtc/examples/**.o')
    remove_files(webrtc_out_dir .. '/obj/webrtc/modules/audio_coding/delay_test/utility.o')
    remove_files(webrtc_out_dir .. '/obj/webrtc/modules/modules_tests/utility.o')
    remove_files(webrtc_out_dir .. '/obj/webrtc/modules/video_capture/video_capture/video_capture_external.o')
    remove_files(webrtc_out_dir .. '/obj/webrtc/modules/video_capture/video_capture/device_info_external.o')
end)
