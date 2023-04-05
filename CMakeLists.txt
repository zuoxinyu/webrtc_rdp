cmake_minimum_required(VERSION 3.0)

project(chatz
    VERSION 0.1
)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "-Wno-deprecated-declarations -fexperimental-library -fno-stack-protector")

find_package(Boost 1.81 COMPONENTS json url REQUIRED)
find_package(SDL2 REQUIRED)

message(STATUS "boost libs: " ${Boost_LIBRARIES})

if (${CMAKE_BUILD_TYPE} STREQUAL "debug")
    find_library(LIBWEBRTC webrtc "libwebrtc/debug")
else()
    find_library(LIBWEBRTC webrtc "libwebrtc/release")
endif()

message(STATUS "libwebrtc: " ${LIBWEBRTC})

#add_definitions(-DBOOST_ASIO_HAS_CO_AWAIT)
if (WIN32)
    add_definitions(-DWEBRTC_WIN)
    find_package(sdl2-ttf REQUIRED)
elseif (UNIX)
    add_definitions(-DWEBRTC_POSIX)
    add_definitions(-DWEBRTC_LINUX)
    add_definitions(-DWEBRTC_USE_X11)
    find_package(SDL2_ttf REQUIRED)
endif()

# find_package(FFMPEG REQUIRED)

# add_subdirectory(libwebrtc)


aux_source_directory(src/client DIR_SRCS)
aux_source_directory(src/server SERVER_DIR_SRCS)

add_executable(${PROJECT_NAME}
    microui/src/microui.c
    ${DIR_SRCS})

add_executable(chat_server ${SERVER_DIR_SRCS})

include_directories(
    src
    microui/src
    ${Boost_INCLUDE_DIRS}
    ../libwebrtc_build/src)

if(WIN32)
    target_link_libraries(chat_server PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBWEBRTC})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2_ttf)
    target_link_libraries(${PROJECT_NAME} PRIVATE GL)
    target_link_libraries(${PROJECT_NAME} PRIVATE rt)
    #target_link_libraries(${PROJECT_NAME} PRIVATE websockets_shared)
    #target_link_libraries(${PROJECT_NAME} PRIVATE cjson)
    #target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_libavcodec_LIBRARY})
    #target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_libavutil_LIBRARY})
endif()

if(UNIX)
    target_link_libraries(chat_server PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBWEBRTC})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 SDL2_ttf GL GLEW)
    target_link_libraries(${PROJECT_NAME} PRIVATE rt)
    target_link_libraries(${PROJECT_NAME} PRIVATE atomic)
    target_link_libraries(${PROJECT_NAME} PRIVATE dbus-1)
    target_link_libraries(${PROJECT_NAME} PRIVATE glib-2.0 gobject-2.0 gmodule-2.0 gio-2.0)
    target_link_libraries(${PROJECT_NAME} PRIVATE xcb X11 Xext Xfixes Xdamage Xrandr Xrender Xau Xdmcp Xcomposite Xtst)
    target_link_libraries(${PROJECT_NAME} PRIVATE gbm)
    #target_link_libraries(${PROJECT_NAME} PRIVATE websockets)
    #target_link_libraries(${PROJECT_NAME} PRIVATE avcodec avutil)
    #target_link_libraries(${PROJECT_NAME} PRIVATE cjson)
endif()


####### CPACK SETTINGS ########
# set(CPACK_PACKAGE_NAME $(PROJECT_NAME))
# set(CPACK_SET_DESTDIR ON)
# set(CPACK_INSTALL_PREFIX "/usr/local")
# set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
# set(CPACK_RPM_PACKAGE_GROUP "zuoxinyu")
# set(CPACK_PACKAGE_VENDOR "zuoxinyu")
# set(CPACK_RPM_PACKAGE_LICENSE "Apache 2.0 + Common Clause 1.0")
# 
# install(TARGETS ${PROJECT_NAME})
# include(CPack)
