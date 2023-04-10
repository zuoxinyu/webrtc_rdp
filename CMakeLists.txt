cmake_minimum_required(VERSION 3.14)

project(dezk
    VERSION 0.1
)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_EXPORT_COMPILE_COMMANDS)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "-Wno-deprecated-declarations -fexperimental-library -gsplit-dwarf")# -fno-stack-protector")

message(STATUS "libwebrtc: " ${LIBWEBRTC})

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
    set(WEBRTC_SRC_DIR ../webrtc/src)
    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        find_library(LIBWEBRTC webrtc "libwebrtc/debug")
    else()
        find_library(LIBWEBRTC webrtc "libwebrtc/release")
    endif()
    add_definitions(-DWEBRTC_WIN)
    find_package(boost 1.79 COMPONENTS json url REQUIRED)
    find_package(SDL2 REQUIRED)
    find_package(sdl2-ttf REQUIRED)
elseif (UNIX)
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")
    set(WEBRTC_SRC_DIR ../libwebrtc_build/src)
    if (${CMAKE_BUILD_TYPE} STREQUAL "debug")
        find_library(LIBWEBRTC webrtc "libwebrtc/debug")
    else()
        find_library(LIBWEBRTC webrtc "libwebrtc/release")
    endif()
    add_definitions(-DWEBRTC_POSIX)
    add_definitions(-DWEBRTC_LINUX)
    add_definitions(-DWEBRTC_USE_X11)
    find_package(Boost 1.81 COMPONENTS json url REQUIRED)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(spdlog REQUIRED)
endif()

message(STATUS "boost libs: " ${Boost_LIBRARIES})

# add_subdirectory(libwebrtc)

aux_source_directory(src/client CLIENT_DIR_SRCS)
aux_source_directory(src/server SERVER_DIR_SRCS)

add_executable(${PROJECT_NAME} ${CLIENT_DIR_SRCS})

add_executable(chat_server ${SERVER_DIR_SRCS})

include_directories(
    src
    ${Boost_INCLUDE_DIRS}
    ${WEBRTC_SRC_DIR})

if(WIN32)
    target_link_libraries(chat_server PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(chat_server PRIVATE spdlog::spdlog_header_only)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBWEBRTC})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog_header_only)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2_ttf)
    target_link_libraries(${PROJECT_NAME} PRIVATE GL)
    target_link_libraries(${PROJECT_NAME} PRIVATE rt)
elseif(UNIX)
    target_link_libraries(chat_server PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(chat_server PRIVATE spdlog::spdlog_header_only)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBWEBRTC})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog_header_only)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 SDL2_ttf GL GLEW)
    target_link_libraries(${PROJECT_NAME} PRIVATE rt)
    target_link_libraries(${PROJECT_NAME} PRIVATE atomic)
    target_link_libraries(${PROJECT_NAME} PRIVATE dbus-1)
    target_link_libraries(${PROJECT_NAME} PRIVATE glib-2.0 gobject-2.0 gmodule-2.0 gio-2.0)
    target_link_libraries(${PROJECT_NAME} PRIVATE xcb X11 Xext Xfixes Xdamage Xrandr Xrender Xau Xdmcp Xcomposite Xtst)
    target_link_libraries(${PROJECT_NAME} PRIVATE gbm)
endif()
